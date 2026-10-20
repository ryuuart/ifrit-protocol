/** @file
 * The frame's bodies drawn in their three phases — laid over, seen
 * through, blended — each back to front, with the scene behind copied for
 * the bodies that read it: where a body stands in the picture, whether one
 * stands behind another, and how many of the copy's levels are read.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Parameters.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "GeometryPass.h"

namespace sigil::world::diligent {

namespace {

/** WHERE A BODY STANDS IN THE PICTURE: the rectangle its box covers, in
 *  the picture's own normalised coordinates, and how far in front of the
 *  eye its box reaches — nearest, middle and farthest. A box reaching
 *  behind the eye covers the whole picture. */
struct Region {
  glm::vec4 rect{-1.0f, -1.0f, 1.0f, 1.0f};
  float nearest = 0.0f;
  float middle = 0.0f;
  float farthest = 0.0f;
};
Region regionOf(const Draw& body, const glm::mat4& viewProj,
                const glm::mat4& view) {
  Region out;
  const Box box = boxOf(body);
  if (box.low.x > box.high.x) return out;
  glm::vec2 low(std::numeric_limits<float>::max());
  glm::vec2 high(std::numeric_limits<float>::lowest());
  float nearest = std::numeric_limits<float>::max();
  float farthest = std::numeric_limits<float>::lowest();
  bool behindEye = false;
  for (int i = 0; i < 8; ++i) {
    const glm::vec4 corner(box.corner(i), 1.0f);
    const float depth = -(view * corner).z;
    nearest = std::min(nearest, depth);
    farthest = std::max(farthest, depth);
    const glm::vec4 clip = viewProj * corner;
    if (clip.w <= 1e-4f) {
      behindEye = true;
      continue;
    }
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    low = glm::min(low, ndc);
    high = glm::max(high, ndc);
  }
  if (!behindEye) out.rect = glm::vec4(low, high);
  out.nearest = nearest;
  out.farthest = farthest;
  out.middle = (nearest + farthest) * 0.5f;
  return out;
}

/** Whether @p behind, drawn earlier, stands behind @p front where they
 *  overlap in the picture: the middle of its box farther than the whole of
 *  the other's. Bodies standing at one depth — drops on one pane — are
 *  beside each other, not behind. */
bool standsBehind(const Region& behind, const Region& front) {
  const bool overlaps =
      behind.rect.x < front.rect.z && front.rect.x < behind.rect.z &&
      behind.rect.y < front.rect.w && front.rect.y < behind.rect.w;
  return overlaps && behind.middle > front.farthest;
}

/** HOW MANY OF THE SCENE COPY'S SMALLER LEVELS @p body READS, out of the
 *  copy's @p levels: none for a smooth surface, which reads the copy as it
 *  is, and up to the level its roughness picks — scattered by how strongly
 *  it bends — and the one past it a read between two levels takes. A
 *  roughness map multiplies the number, so the number bounds it; a body
 *  whose numbers this cannot read is taken to read every level. */
int levelsRead(const Draw& body, int levels) {
  const material::Material& worn = *body.material;
  if (!worn.hasProgram()) return levels - 1;
  const material::Schema& fields = worn.recipe().parameters();
  const material::Field* roughness = fields.find("roughness");
  const material::Field* ior = fields.find("ior");
  const material::Lobes carried = material::surface::lobes(worn);
  if (!carried.transmission) return carried.distortion ? 0 : levels - 1;
  if (!roughness || !ior || roughness->floats != 1 || ior->floats != 1)
    return levels - 1;
  const float rough = std::clamp(worn.get<float>("roughness"), 0.0f, 1.0f);
  const float bend =
      std::clamp(worn.get<float>("ior") * 2.0f - 2.0f, 0.0f, 1.0f);
  const float level = rough * bend * (float)(levels - 1);
  return std::min((int)std::ceil(level) + (level > 0.0f ? 1 : 0), levels - 1);
}

/** The phases a frame's bodies are drawn in, in order. */
enum class Phase : uint8_t { LaidOver, SeenThrough, Blended };

/** Copies @p from into @p into, both on the device. The caller binds its
 *  target again afterwards: a copy ends the drawing that was under way. */
void copyForReading(Gpu& gpu, dg::ITexture* from, dg::ITexture* into) {
  // The target is let go of first: a copy cannot read what is bound to be
  // drawn into.
  gpu.device->context()->SetRenderTargets(
      0, nullptr, nullptr, dg::RESOURCE_STATE_TRANSITION_MODE_NONE);
  dg::CopyTextureAttribs attribs;
  attribs.pSrcTexture = from;
  attribs.SrcTextureTransitionMode =
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  attribs.pDstTexture = into;
  attribs.DstTextureTransitionMode =
      dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  gpu.device->context()->CopyTexture(attribs);
}

}  // namespace

/** The frame's bodies, or the ones a collection leaves: the ones laid
 *  over normally first, then the ones that see through themselves over
 *  them, then the blended ones, each phase back to front. A body that
 *  sees through itself reads the lit colour and the depth as they stood
 *  once the first phase was down, copied before the first of them is
 *  drawn — and copied again before one that stands in front of another
 *  drawn since the last copy, so glass seen through glass shows both, as
 *  deep as the bodies are layered; a blended body that fades into the
 *  scene reads that same depth, or copies it before the first of them
 *  when nothing copied it yet. */
void drawBodies(Gpu& gpu, const View& view, const glm::mat4& viewProj,
                const glm::mat4& viewMatrix, const Collection* only, bool lit,
                const glm::vec4* flat, dg::ITexture* target, bool timed) {
  // THE LIGHTS EACH BODY IS REACHED BY: every light, unless its light
  // link leaves the body out. Asked per body, against the same member a
  // pass's collection reads, so a light link and a pass narrow on one
  // vocabulary.
  std::vector<const PlacedLight*> reaching;
  reaching.reserve(view.lights.size());
  dg::ITexture* sceneDepth = nullptr;
  // A FLAT colour replaces the body's own AND its material: coverage and
  // a variant re-draw are about where a body is, not what it is made of.
  // Whether the flat colour is then SHADED is the pass's call — coverage
  // is not a picture and is drawn unlit; a variant surface is a colour
  // laid over the bodies a collection names and stands under the pass's
  // lights like any other.
  dg::ITexture* sceneColor = nullptr;
  // The bodies that see through themselves drawn since the scene was last
  // copied, by where they stand in the picture.
  std::vector<Region> sinceCopied;
  // How many of the copy's smaller levels the frame's bodies read, found
  // at the first copy.
  std::optional<int> readLevels;
  for (const Phase phase :
       {Phase::LaidOver, Phase::SeenThrough, Phase::Blended}) {
    for (const Draw& body : view.draws) {
      if (!body.mesh) continue;
      const Member member = memberOf(body);
      if (only && !only->contains(member)) continue;
      const glm::vec4 colour = flat ? *flat : body.baseColor;
      const Blending blending = blendingOf(body, colour, flat != nullptr);
      const Phase own = seesThrough(body, flat != nullptr) ? Phase::SeenThrough
                        : blended(body, blending, flat != nullptr)
                            ? Phase::Blended
                            : Phase::LaidOver;
      if (own != phase) continue;
      // THE SCENE BEHIND, copied once every body that does not see
      // through itself is laid over and before the first that does: the
      // lit colour with its blurrier levels derived from it, and the depth
      // beside it, which says where what was copied stands.
      // …AND AGAIN before one standing in front of a body that saw through
      // itself since, which the copy then holds as it was drawn.
      bool copying = false;
      bool drewSeen = false;
      if (phase == Phase::SeenThrough) {
        const Region here = regionOf(body, viewProj, viewMatrix);
        copying =
            !sceneColor || std::any_of(sinceCopied.begin(), sinceCopied.end(),
                                       [&](const Region& drawn) {
                                         return standsBehind(drawn, here);
                                       });
        drewSeen = !sinceCopied.empty();
        if (copying) sinceCopied.clear();
        sinceCopied.push_back(here);
      }
      if (copying) {
        // What the bodies seen through before this copy cost is theirs,
        // not the copy's.
        if (timed && drewSeen) gpu.mark("seen");
        if (dg::ITexture* copy = gpu.sceneColorForReading()) {
          copyForReading(gpu, target, copy);
          if (timed) gpu.mark("colour-copy");
          // ONLY THE LEVELS SOMETHING READS: the deepest any body that sees
          // through itself in this frame reaches, and none at all where
          // every one of them is smooth.
          if (!readLevels) {
            readLevels = 0;
            const int levels = (int)copy->GetDesc().MipLevels;
            for (const Draw& other : view.draws)
              if (other.mesh && (!only || only->contains(memberOf(other))) &&
                  seesThrough(other, flat != nullptr))
                *readLevels = std::max(*readLevels, levelsRead(other, levels));
          }
          if (*readLevels > 0)
            if (dg::ITextureView* chain =
                    gpu.sceneColorLevels(*readLevels + 1)) {
              gpu.device->context()->GenerateMips(chain);
              if (timed) gpu.mark("mips");
            }
          sceneColor = copy;
        }
        if (gpu.depth)
          if (dg::ITexture* depthCopy = gpu.depthForReading()) {
            copyForReading(gpu, gpu.depth, depthCopy);
            if (timed) gpu.mark("depth-copy");
            sceneDepth = depthCopy;
          }
        resumeTarget(gpu, target, true);
      }
      // THE OPAQUE DEPTH, copied once the last opaque body is down and
      // before the first body that fades into it, with the target bound
      // again afterwards: a copy ends the drawing that was under way.
      if (blending.fades && !sceneDepth && gpu.depth) {
        if (dg::ITexture* copy = gpu.depthForReading()) {
          copyForReading(gpu, gpu.depth, copy);
          if (timed) gpu.mark("depth-copy");
          resumeTarget(gpu, target, true);
          sceneDepth = copy;
        }
      }
      reaching.clear();
      for (const PlacedLight& light : view.lights)
        if (light.light.lightLink.everything() ||
            light.light.lightLink.contains(member))
          reaching.push_back(&light);
      // A BODY THAT IS ITS OWN LIGHT is drawn unlit whatever the pass
      // says, because that is what its surface is and not how this pass
      // reads it — a variant laid over it included.
      // A MESH WHOSE FACES WEAR SEVERAL SLOTS is drawn once a slot, each
      // draw the triangles its "Material" lane names with that slot,
      // wearing that slot's surface and map.
      const bool perSlot = !flat && body.slots.size() > 1 && !body.instances;
      const size_t draws = perSlot ? body.slots.size() : 1;
      for (size_t k = 0; k < draws; ++k) {
        const material::Material* worn = flat      ? nullptr
                                         : perSlot ? &body.slots[k]
                                                   : body.material;
        const material::Texture* map =
            flat      ? nullptr
            : perSlot ? material::surface::map(
                            body.slots[k], material::surface::kBaseColorSlot)
                      : body.texture;
        drawBody(gpu, viewProj, viewMatrix,
                 Shape{body.geometry, body.mesh, body.instances, body.facing,
                       perSlot ? (int)k : -1, body.primitiveColor},
                 body.world, colour, worn, map, reaching, view.environment,
                 lit && body.lit, blending,
                 /*cull=*/body.backface == material::Backface::Hidden,
                 blending.fades || phase == Phase::SeenThrough ? sceneDepth
                                                               : nullptr,
                 phase == Phase::SeenThrough ? sceneColor : nullptr, nullptr,
                 body.receiveShadow);
      }
    }
    if (timed)
      gpu.mark(phase == Phase::LaidOver      ? "laid"
               : phase == Phase::SeenThrough ? "seen"
                                             : "blended");
  }
}

}  // namespace sigil::world::diligent
