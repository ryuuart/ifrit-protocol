/** @file
 * A geometry pass on the device: the bodies the pass's realisation
 * leaves it, rasterised depth-tested into a device texture from a
 * pipeline built out of each material's own Slang body; the stamps of
 * the point sets it reads; and the coverage a masked pass downstream
 * reads its selection from.
 *
 * EVERY BODY IS ONE INSTANCED DRAW: its mesh once, and a second buffer
 * stepping once per instance — instanced geometry's own instances, or one
 * neutral instance for a body drawn once. The copies of one body are
 * drawn in the order its instances stand in; they are not sorted among
 * themselves.
 *
 * THE ORDER: LAID OVER FIRST, THEN SEEN THROUGH, THEN BLENDED. Bodies
 * arrive sorted back to front by where each one's own origin stands from
 * the camera. Every body laid over normally is drawn first, in that order
 * — an opaque one testing and writing depth, a translucent one testing it
 * only. Then, in a frame that holds a body that SEES THROUGH ITSELF — one
 * whose surface transmits or distorts — the lit colour and the depth are
 * copied, the colour with its chain of blurrier levels, and every such
 * body is drawn back to front reading that one copy: so a transmissive
 * body shows the opaque scene behind it, and never another transmissive
 * body, which is one layer of refraction. Last, every BLENDED one — a
 * surface that adds, multiplies or screens, one that says it writes no
 * depth, one that fades into the scene — back to front over all of them,
 * testing depth and writing it only where its material says; so a glow is
 * not seen through glass standing in front of it. A frame with nothing
 * that sees through itself copies nothing and draws exactly the two
 * phases it would without the middle one. So a blended body is hidden by
 * whatever opaque body stands in front of it, including a card whose texture is
 * transparent in places but which writes depth everywhere it covers: such a
 * card states that it writes no depth. The sort is per body, not per triangle:
 * two blended bodies that pass through each other, or one body whose own
 * triangles overlap, are drawn in whatever order their origins and their
 * triangles give, which an additive surface does not mind and a translucent one
 * laid over may.
 *
 * THE SHADOW STAGE COMES BEFORE ALL OF IT. In a pass under a light that
 * casts, every such light's map is drawn into one depth atlas before the
 * target is opened: each body that casts, seen from the light, writing
 * depth and nothing else. A pass under no light that casts draws no map,
 * binds no atlas and is the pass it would be without this stage.
 *
 * Every body writes LIGHT into a target holding light; the camera's
 * reading is the final stage's.
 */

#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/diligent/Runtime.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

/** The half of the artefact numbering a STAMPED point set takes. The
 *  scene's store counts up from one, so nothing it hands out ever
 *  reaches here, and the rest of the number is the stamping's own
 *  content key — which is what makes a set that has not moved answer
 *  with the artefact it was already uploaded under. */
constexpr uint64_t kStampArtefact = 1ull << 63u;

/** THE PAGES THE FRAME'S BODIES WEAR, made ready before any target is
 *  opened: every map a body's material holds, at any depth of its tree,
 *  that names a page standing on the device is wrapped, waited for and
 *  has its levels filled here — so the draws inside a pass find them
 *  filled and none of them breaks its pass to fill one. A material two
 *  bodies share is walked once. */
void preparePages(Gpu& gpu, const View& view) {
  const std::chrono::duration<double> time(gpu.seconds);
  std::vector<const material::Material*> pending;
  std::vector<const material::Material*> walked;
  for (const Draw& body : view.draws) {
    if (body.texture) gpu.maps.prepare(*body.texture, time);
    if (body.material) pending.push_back(body.material);
    for (const material::Material& slot : body.slots) pending.push_back(&slot);
  }
  while (!pending.empty()) {
    const material::Material* worn = pending.back();
    pending.pop_back();
    if (std::find(walked.begin(), walked.end(), worn) != walked.end()) continue;
    walked.push_back(worn);
    for (const auto& [name, slot] : worn->slots()) {
      if (slot.material) {
        pending.push_back(slot.material.get());
      } else if (const auto* map =
                     dynamic_cast<const material::Texture*>(slot.leaf.get())) {
        gpu.maps.prepare(*map, time);
      }
    }
  }
}

}  // namespace

void paintGeometry(Gpu& gpu, const PassWork& work, const View& view,
                   Targets& targets) {
  const Pass& pass = *work.pass;
  const std::span<const std::string> writes = pass.writes();
  if (writes.empty()) return;
  dg::ITexture* colour = gpu.target(writes.front());
  if (!colour) return;

  const glm::mat4 viewProj = clipProjection(view);
  const glm::mat4 viewMatrix = view.camera.view();

  const float clearNothing[4] = {0, 0, 0, 0};
  const bool asCoverage = work.realisation == Selection::Mask;
  gpu.shadows.clear();
  gpu.standIns = view.standIns;
  gpu.programsPending = 0;
  // The programs a light kind entering started on an earlier pass are
  // waited for here, before anything is drawn, so the stand-ins it cost
  // stop at the pass that met it.
  for (const auto& [recipe, variant] : gpu.awaitedPrograms)
    (void)material::ProgramCache::shared().program(
        recipe, material::Target::Slang, material::Variant{variant});
  gpu.awaitedPrograms.clear();
  // Every body this pass drew as a stand-in is told to the scene, however
  // the pass returns.
  struct Report {
    Gpu& gpu;
    Targets& targets;
    ~Report() { targets.addProgramsPending(gpu.programsPending); }
  } report{gpu, targets};
  if (asCoverage) {
    // Coverage is where the selection is, not what it looks like: flat
    // white, and nothing else reaches it.
    openTarget(gpu, colour, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.collection(),
               /*lit=*/false, &white, colour);
    gpu.mark("coverage");
    return;
  }

  // THE PAGES' CHAINS, before any target is opened: the shadow stage
  // below reads a page's opacity and transmission too.
  preparePages(gpu, view);
  // THE PANORAMA'S FILTERED CHAIN, before the target is opened: made on
  // the device the first time a sky is met, which draws into a target of
  // its own.
  if (view.environment.valid()) {
    (void)prefilteredPanorama(gpu, gpu.maps.environment(view.environment.map));
    (void)prefilteredPanorama(gpu, gpu.maps.environment(view.environment.next));
  }
  // What the pages' painting kept the device waiting for, and their levels,
  // are the pages' and not the shadow stage's.
  gpu.mark("pages");
  // THE SHADOW STAGE, before the target is opened: the maps every lit
  // body below reads, then the tint beside them.
  const int64_t mapsDrawn = drawShadows(gpu, view, viewMatrix);
  gpu.mark("shadow");
  targets.addShadowMapsDrawn(mapsDrawn +
                             drawTints(gpu, view, viewMatrix, mapsDrawn));
  gpu.mark("tint");
  // THE BACKDROPS' TEXTURES, before the target is opened, so a page met
  // here is wrapped and waited for outside the pass that lays it.
  std::vector<dg::ITexture*> backdrops;
  for (const material::Texture& picture : view.backdrops)
    if (dg::ITexture* texture =
            gpu.maps.sample(picture, {.mipmaps = false},
                            std::chrono::duration<double>(gpu.seconds)))
      backdrops.push_back(texture);
  // THE CLEAR IS AN AUTHORED COLOUR, so it is filled as the light the
  // final stage turns back into it, and the depth is cleared beside it.
  openTarget(gpu, colour, clearNothing, true);
  if (!gpu.shadows.empty()) {
    // The stage drew into tiles of the atlas; the picture is drawn whole.
    dg::Viewport whole;
    whole.Width = (float)gpu.extent.width();
    whole.Height = (float)gpu.extent.height();
    gpu.device->context()->SetViewports(1, &whole,
                                        (dg::Uint32)gpu.extent.width(),
                                        (dg::Uint32)gpu.extent.height());
  }
  fillTarget(gpu, colour, pass.clear());
  resumeTarget(gpu, colour, true);
  // The PROJECTION alone is what turns a pixel into a ray, and it is the
  // clip transform with the view taken back out of it — the clip
  // transform is where the target's aspect and the depth convention
  // already are, so deriving it is what keeps the sky's rays and the
  // bodies' positions answering to one camera.
  drawBackdrop(gpu, view, viewProj * glm::inverse(viewMatrix), viewMatrix);
  // THE FRAME'S BACKDROPS, over the clear and the sky and under every
  // body: each the whole picture, of which a target that is a part of it
  // shows its own window.
  if (!backdrops.empty()) {
    glm::vec4 window{0.0f, 0.0f, 1.0f, 1.0f};
    if (isPart(view.offset, view.extent))
      window = {(float)view.offset.origin.x / (float)view.offset.whole.x,
                (float)view.offset.origin.y / (float)view.offset.whole.y,
                (float)view.extent.x / (float)view.offset.whole.x,
                (float)view.extent.y / (float)view.offset.whole.y};
    for (dg::ITexture* picture : backdrops)
      layAuthored(gpu, colour, picture, window);
    resumeTarget(gpu, colour, true);
  }
  const bool cull = work.realisation == Selection::Cull;
  drawBodies(gpu, view, viewProj, viewMatrix,
             cull ? &pass.collection() : nullptr, /*lit=*/true, nullptr, colour,
             /*timed=*/true);

  // The stamps of every point set the pass reads, drawn as instanced
  // geometry like any instanced body. A compute pass writes points; this
  // is what makes them visible, and a pass with no stamp draws none of
  // them.
  if (!pass.stamp().positions.empty()) {
    // A pass's stamps belong to no node: they joined no collection and
    // stand under no key, so a light reaches them unless its link
    // includes only what they are not.
    std::vector<const PlacedLight*> everyLight;
    for (const PlacedLight& light : view.lights)
      if (light.light.lightLink.contains(Member{}))
        everyLight.push_back(&light);
    // A STAMPING IS AN ARTEFACT LIKE ANY OTHER: formed once per distinct
    // (cloud, stamp) in the store, and uploaded once under the number
    // that pair folds to. A scene that has not moved between two frames is
    // therefore neither instanced again nor re-uploaded, however many
    // frames draw it.
    for (const std::string& name : pass.reads()) {
      // Read without inserting: a stamped pass reads image names too,
      // and the inserting overload would leave an empty cloud behind
      // for each of them that nothing ever erases.
      const geometry::mesh::Cloud* cloud = std::as_const(targets).points(name);
      if (!cloud || cloud->positions.empty()) continue;
      uint64_t key = 0;
      const geometry::mesh::Instances* stamped =
          targets.stamped(*cloud, pass.stamp(), &key);
      if (!stamped || stamped->stamp.indices.empty()) continue;
      // The top bit is the half of the numbering these take, so the
      // store's key is carried in the rest of it.
      const uint64_t artefact = kStampArtefact | (key >> 1u);
      drawBody(gpu, viewProj, viewMatrix,
               Shape{artefact, &stamped->stamp, stamped}, glm::mat4(1.0f),
               {0.9f, 0.9f, 0.95f, 1.0f}, nullptr, nullptr, everyLight,
               view.environment, /*lit=*/true, Blending{}, /*cull=*/true,
               nullptr);
    }
  }

  // The selection, drawn again in the surface the pass named — which is
  // what makes it visible in a pass that paints everything.
  if (work.realisation == Selection::Variant && pass.variant()) {
    const material::Field* field =
        pass.variant()->recipe().parameters().find("baseColor");
    const glm::vec4 variant = field && field->floats == 4
                                  ? pass.variant()->get<glm::vec4>("baseColor")
                                  : glm::vec4{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.collection(),
               /*lit=*/true, &variant, colour);
  }

  // One coverage per masked pass behind this one, each holding the
  // selection that pass asked about.
  for (const Coverage& painted : work.coverageOut) {
    dg::ITexture* coverage = gpu.target(painted.name);
    if (!coverage) continue;
    openTarget(gpu, coverage, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &painted.of, /*lit=*/false,
               &white, coverage);
  }
  gpu.mark("extras");
}

}  // namespace sigil::world::diligent
