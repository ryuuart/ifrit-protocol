/** @file
 * A geometry pass on the device: the bodies the pass's realisation
 * leaves it, rasterised depth-tested into a device texture from a
 * pipeline built out of each material's own Slang body; the stamps of
 * the point sets it reads; and the coverage a masked pass downstream
 * reads its selection from.
 *
 * THE ORDER IS THE VIEW'S. Bodies arrive sorted back to front, and they
 * are drawn in that order with the depth buffer read and — for an opaque
 * body — written. The two agree wherever the sort is right, and where a
 * centroid sort ranks two bodies the wrong way round the depth buffer is
 * the one telling the truth.
 */

#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Params.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/light/Light.h>

#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** The shading terms the CPU tier holds in its style, so that the two
 *  tiers answer the same question of the same scene. */
constexpr float kAmbient[4] = {0.12f, 0.12f, 0.15f, 1.0f};
constexpr float kSpecular = 0.5f;
constexpr float kShininess = 48.0f;
constexpr float kRim = 0.25f;
/** How many emitters one draw carries — the budget the light value
 *  declares and the host tier honours, so both tiers light a set with
 *  the same emitters rather than the device dropping the last four. */
constexpr size_t kLights = 8;

/** The four panorama slots the scaffold declares, and the sampler they
 *  are read with. Named here because the binding has to tell them from
 *  a material's own slots: an equirect map wants a wrap of its own on
 *  each axis and reads linearly across its levels, and the one sampler
 *  every other slot in a draw shares can be neither. */
constexpr std::string_view kEnvironmentSlots[4] = {
    "uEnvironment", "uEnvironmentNext", "uIrradiance", "uIrradianceNext"};

bool isEnvironmentSlot(std::string_view slot) {
  for (std::string_view name : kEnvironmentSlots)
    if (slot == name) return true;
  return false;
}

/** The scaffold's sampled slot and the placement it is read at — the
 *  two names the map a body is dressed with arrives under. */
constexpr std::string_view kMapSlot = "uBaseColorMap";
constexpr std::string_view kMapUv = "uBaseColorMapUv";

/** The half of the artefact numbering a frame's OWN cooks take. The
 *  scene's store counts up from one, so nothing it hands out ever
 *  reaches here. */
constexpr uint64_t kStampArtefact = 1ull << 63u;

/** The colour a clear is given, premultiplied — which is what a target
 *  holding premultiplied pixels must receive. */
void premultiplied(SkColor4f colour, float* into) {
  into[0] = colour.fR * colour.fA;
  into[1] = colour.fG * colour.fA;
  into[2] = colour.fB * colour.fA;
  into[3] = colour.fA;
}

/** What one body is drawn with: its program, the bytes its material
 *  resolved to, and whether the emitters reach it. */
struct Surface {
  const material::slang::Compiled* program = nullptr;
  std::span<const std::byte> bytes;
  const material::Recipe* recipe = nullptr;
  bool lit = true;
};

Surface surfaceOf(const material::Material* material, bool lit) {
  Surface out;
  out.lit = lit;
  if (!material) {
    out.program = &scaffold(lit);
    return out;
  }
  const material::Variant variant{lit ? kVariantLit : 0u};
  const material::Material::Resolved resolved = material->resolve(
      material::Target::Slang, material::FrameData{}, variant);
  const auto* program =
      resolved.program ? resolved.program->as<material::slang::SlangProgram>() : nullptr;
  if (!program) {
    // The cache has already reported the recipe and the target; the body
    // it would have painted is drawn in the colour the frame extracted,
    // which is what the tier with no compiler answers with too.
    out.program = &scaffold(lit);
    return out;
  }
  out.program = &program->compiled();
  out.bytes = resolved.bytes;
  out.recipe = &material->recipe();
  return out;
}

/** The material's resolved bytes, written at the offsets its program
 *  reported for the same names.
 *
 *  A 3x3 is the one field whose bytes are not already in the order the
 *  shader reads them in: the params hold it column by column, the way
 *  glm does, and the program reads it row by row. */
void writeMaterial(material::slang::Uniforms& uniforms, const Surface& surface) {
  if (!surface.recipe) return;
  for (const material::Field& field : surface.recipe->layout().fields) {
    const size_t bytes = field.floats * sizeof(float);
    if (field.offset + bytes > surface.bytes.size()) continue;
    const auto* values =
        reinterpret_cast<const float*>(surface.bytes.data() + field.offset);
    if (field.kind == material::Kind::Mat3) {
      float rows[9];
      for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) rows[r * 3 + c] = values[c * 3 + r];
      uniforms.set(field.name, rows, 9);
      continue;
    }
    uniforms.set(field.name, values, field.floats);
  }
}

/** Everything a draw's scaffold needs: where the body stands, where the
 *  camera is, and what is shining on it. */
void writeScaffold(material::slang::Uniforms& uniforms, const material::slang::Compiled& program,
                   const glm::mat4& viewProj, const glm::mat4& view,
                   const glm::mat4& model, glm::vec4 baseColor,
                   std::span<const Light> lights, const Environment& sky,
                   const glm::mat3& orientation, int levels, bool lit) {
  uniforms.set("uViewProj", viewProj);
  uniforms.set("uModel", model);
  uniforms.set("uBaseColor", baseColor.r, baseColor.g, baseColor.b,
               baseColor.a);
  if (!lit || !program.uniform("uShading")) return;

  const glm::mat4 modelView = view * model;
  uniforms.set("uModelView", modelView);
  uniforms.set("uNormalMatrix",
               glm::mat4(glm::inverseTranspose(glm::mat3(modelView))));
  uniforms.set("uLightMatrix",
               glm::mat4(glm::inverseTranspose(glm::mat3(view))));
  // THE AMBIENT CONSTANT, and what replaces it. A set carrying a
  // panorama reads what actually falls on a surface facing each way; one
  // that carries none keeps the single value both tiers hold, so its
  // picture is what it was.
  uniforms.set("uAmbient", kAmbient, 4);
  const bool hasSky = levels > 0;
  uniforms.set("uEnvTint", sky.tint.x * sky.intensity,
               sky.tint.y * sky.intensity, sky.tint.z * sky.intensity,
               std::clamp(sky.crossfade, 0.0f, 1.0f));
  uniforms.set("uEnvDials", hasSky ? sky.diffuse : 0.0f,
               hasSky ? sky.specular : 0.0f, sky.roughnessBias,
               hasSky ? (float)levels : 0.0f);
  // The shading is written in view space and a panorama is of the world,
  // so a direction goes out through the view's inverse and then into the
  // frame the node that placed the sky put it in.
  uniforms.set("uEnvMatrix",
               glm::mat4(orientation * glm::transpose(
                                           glm::mat3(glm::inverseTranspose(
                                               glm::mat3(view))))));
  const size_t count = std::min(lights.size(), kLights);
  for (size_t i = 0; i < count; ++i) {
    const light::Directional value = light::directional(lights[i]);
    const float direction[4] = {value.direction.x, value.direction.y,
                                value.direction.z, 0.0f};
    const float colour[4] = {value.color.r * value.intensity,
                             value.color.g * value.intensity,
                             value.color.b * value.intensity, 1.0f};
    uniforms.setElement("uLightDir", i, direction, 4);
    uniforms.setElement("uLightColor", i, colour, 4);
  }
  uniforms.set("uShading", kSpecular, kShininess, kRim, (float)count);
}

/** One body, drawn. @p map is the texture it is dressed with, or null. */
void drawBody(Gpu& gpu, const glm::mat4& viewProj, const glm::mat4& view,
              uint64_t artefact, const Mesh& mesh, const glm::mat4& model,
              glm::vec4 baseColor, const material::Material* material,
              const material::Texture* map, std::span<const Light> lights,
              const Environment& sky, const glm::mat3& orientation, bool lit,
              bool depthWrite) {
  const MeshBuffers* buffers = gpu.upload(artefact, mesh);
  if (!buffers) return;
  const Surface surface = surfaceOf(material, lit);
  if (!surface.program || surface.program->empty()) return;

  const PipelineKey key{surface.program, SkBlendMode::kSrcOver, true,
                        depthWrite, false};
  const Pipeline* pipeline = gpu.pipeline(key);
  if (!pipeline) return;

  // THE PANORAMA, uploaded once per map and kept. Its level count is
  // what says there is a sky at all, so it is asked for before the
  // uniforms are written.
  dg::ITexture* panorama = lit ? gpu.environment(sky.map) : nullptr;
  dg::ITexture* panoramaNext = lit ? gpu.environment(sky.next) : nullptr;
  dg::ITexture* lobe = lit ? gpu.irradiance(sky.map) : nullptr;
  dg::ITexture* lobeNext = lit ? gpu.irradiance(sky.next) : nullptr;
  if (!panoramaNext) panoramaNext = panorama;
  if (!lobeNext) lobeNext = lobe;
  const int levels =
      panorama ? (int)panorama->GetDesc().MipLevels : 0;

  material::slang::Uniforms uniforms(*surface.program);
  writeScaffold(uniforms, *surface.program, viewProj, view, model, baseColor,
                lights, sky, orientation, levels, lit);
  writeMaterial(uniforms, surface);

  // THE SAMPLED SLOTS, bound by NAME rather than by position, because a
  // material's body declares slots of its own and the compiler is free
  // to report them in whatever order it laid them out. An unbound slot
  // reads one white texel, which is the neutral for every map a scalar
  // multiplies — and the one value a tangent-space normal cannot mean,
  // so a body can tell an undressed slot from a dressed one exactly.
  std::vector<dg::ITexture*> textures(surface.program->textures.size(),
                                      nullptr);
  // A STACK RUNNING ITS OWN BODY OWNS EVERY MAP IN IT. The frame extracts
  // the map of the material at the bottom of a stack, because that is
  // what a tier with no compiler can answer with; a composed body samples
  // both operands' maps itself, through slots of its own, so handing the
  // scaffold the bottom's map as well would land it a second time and
  // over the whole face rather than where the mask says.
  if (material && surface.recipe && material::stackDepth(*material) > 0)
    map = nullptr;
  const Sampling sampling = map ? samplingOf(*map) : Sampling{};
  uniforms.set(kMapUv, mapMatrix(sampling.uv));
  for (size_t i = 0; i < textures.size(); ++i) {
    const std::string& slot = surface.program->textures[i];
    // THE BASE COLOUR MAP IS THE SCAFFOLD'S, and only the scaffold's. It
    // multiplies the SHADED colour rather than the surface before it,
    // because the host tier's rasteriser can only modulate a texture
    // against the colour it already shaded — so the body's own base
    // colour slot is left reading white and the map lands once, on the
    // side of the lighting both tiers put it.
    if (slot == kMapSlot) {
      // Asked of the device and not of the host image: a source whose
      // pixels stand on this device has no host image to check for.
      if (map) textures[i] = gpu.sample(*map);
      continue;
    }
    // …and the four panorama slots are the FRAME'S, not any material's:
    // a sky is a property of the set every body in it reads, so it is
    // bound beside the lights rather than dressed onto a surface.
    if (slot == kEnvironmentSlots[0]) {
      textures[i] = panorama;
      continue;
    }
    if (slot == kEnvironmentSlots[1]) {
      textures[i] = panoramaNext;
      continue;
    }
    if (slot == kEnvironmentSlots[2]) {
      textures[i] = lobe;
      continue;
    }
    if (slot == kEnvironmentSlots[3]) {
      textures[i] = lobeNext;
      continue;
    }
    if (!material) continue;
    // …and every OTHER slot is the material's own. `kit::map` answers
    // null for a slot still holding the neutral dressing a surface is
    // built with, which is what keeps an undressed body reading the one
    // white texel rather than a map that says nothing.
    const material::Texture* worn = material::kit::map(*material, slot);
    if (worn && worn != map) textures[i] = gpu.sample(*worn);
  }

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  // The WRAP is the map's own, not a default: one wrap serves both axes
  // and every slot bound here, and clamping an axis that was asked to
  // repeat drags one edge's texels across the whole face.
  bindAndCommit(gpu, *pipeline, *surface.program, uniforms, textures,
                sampling.filter, sampling.tile, &isEnvironmentSlot);
  dg::IBuffer* vertices = buffers->vertices;
  const dg::Uint64 offset = 0;
  context->SetVertexBuffers(0, 1, &vertices, &offset,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                            dg::SET_VERTEX_BUFFERS_FLAG_RESET);
  context->SetIndexBuffer(buffers->indices, 0,
                          dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  dg::DrawIndexedAttribs draw;
  draw.NumIndices = buffers->indexCount;
  draw.IndexType = dg::VT_UINT32;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->DrawIndexed(draw);
}

/** THE SKY, drawn over the whole target before every body in it. It is a
 *  pass and not a body because a body would need a mesh, a placement and
 *  a depth, and a sky has none of the three: it is what is there when
 *  nothing else is. */
void drawBackdrop(Gpu& gpu, const View& view, const glm::mat4& projection,
                  const glm::mat4& viewMatrix) {
  const Environment& sky = view.environment;
  if (!sky.valid() || sky.backdrop.intensity <= 0) return;
  const material::slang::Compiled& program = backdropProgram();
  if (program.empty()) return;
  dg::ITexture* panorama = gpu.environment(sky.map);
  if (!panorama) return;
  dg::ITexture* panoramaNext = gpu.environment(sky.next);
  if (!panoramaNext) panoramaNext = panorama;
  dg::ITexture* lobe = gpu.irradiance(sky.map);
  dg::ITexture* lobeNext = gpu.irradiance(sky.next);
  if (!lobeNext) lobeNext = lobe;

  // The sky is opaque and stands behind everything, so it writes no
  // depth and takes none: whatever is drawn after it covers it.
  const PipelineKey key{&program, SkBlendMode::kSrc, false, false, true};
  const Pipeline* pipeline = gpu.pipeline(key);
  if (!pipeline) return;

  material::slang::Uniforms uniforms(program);
  uniforms.set("uEnvTint", sky.tint.x * sky.intensity,
               sky.tint.y * sky.intensity, sky.tint.z * sky.intensity,
               std::clamp(sky.crossfade, 0.0f, 1.0f));
  uniforms.set("uEnvDials", sky.diffuse, sky.specular, sky.roughnessBias,
               (float)panorama->GetDesc().MipLevels);
  uniforms.set("uBackdrop", sky.backdrop.intensity,
               std::clamp(sky.backdrop.blur, 0.0f, 1.0f), 0.0f, 0.0f);
  uniforms.set("uEnvMatrix",
               glm::mat4(view.orientation *
                         glm::transpose(glm::mat3(
                             glm::inverseTranspose(glm::mat3(viewMatrix))))));
  uniforms.set("uInvProj", glm::inverse(projection));

  std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
  for (size_t i = 0; i < program.textures.size(); ++i) {
    const std::string& slot = program.textures[i];
    if (slot == kEnvironmentSlots[0]) textures[i] = panorama;
    else if (slot == kEnvironmentSlots[1]) textures[i] = panoramaNext;
    else if (slot == kEnvironmentSlots[2]) textures[i] = lobe;
    else if (slot == kEnvironmentSlots[3]) textures[i] = lobeNext;
  }

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  bindAndCommit(gpu, *pipeline, program, uniforms, textures,
                SkFilterMode::kLinear, false, &isEnvironmentSlot);
  dg::DrawAttribs draw;
  draw.NumVertices = 3;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->Draw(draw);
}

/** The frame's bodies, or the ones a selector leaves. */
void drawBodies(Gpu& gpu, const View& view, const glm::mat4& viewProj,
                const glm::mat4& viewMatrix, const Selector* only, bool lit,
                const glm::vec4* flat) {
  // A FLAT colour replaces the body's own AND its material: coverage and
  // a variant re-draw are about where a body is, not what it is made of.
  for (const Draw& body : view.draws) {
    if (!body.mesh) continue;
    if (only && !only->matches(subjectOf(body))) continue;
    const glm::vec4 colour = flat ? *flat : body.baseColor;
    // A BODY THAT IS ITS OWN LIGHT is drawn unlit whatever the pass
    // says, because that is what its surface is and not how this pass
    // reads it. A flat draw is unlit already.
    drawBody(gpu, viewProj, viewMatrix, body.geometry, *body.mesh, body.world,
             colour, flat ? nullptr : body.material,
             flat ? nullptr : body.texture, view.lights, view.environment,
             view.orientation, lit && body.lit,
             /*depthWrite=*/colour.a >= 1.0f);
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

  const glm::mat4 viewProj = clipFor(view.camera, view.extent);
  const glm::mat4 viewMatrix = view.camera.view();

  float clear[4];
  premultiplied(pass.clear(), clear);
  const bool asCoverage = work.realisation == Selection::Mask;
  if (asCoverage) {
    // Coverage is where the selection is, not what it looks like: flat
    // white, and nothing else reaches it.
    const float clearNothing[4] = {0, 0, 0, 0};
    openTarget(gpu, colour, clearNothing, true);
    const glm::vec4 white{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.selector(),
               /*lit=*/false, &white);
    return;
  }

  openTarget(gpu, colour, clear, true);
  // The PROJECTION alone is what turns a pixel into a ray, and it is the
  // clip transform with the view taken back out of it — the clip
  // transform is where the target's aspect and the depth convention
  // already are, so deriving it is what keeps the sky's rays and the
  // bodies' positions answering to one camera.
  drawBackdrop(gpu, view, viewProj * glm::inverse(viewMatrix), viewMatrix);
  const bool cull = work.realisation == Selection::Cull;
  drawBodies(gpu, view, viewProj, viewMatrix, cull ? &pass.selector() : nullptr,
             /*lit=*/true, nullptr);

  // The stamps of every point set the pass reads. A compute pass writes
  // points; this is what makes them visible, and a pass with no stamp
  // draws none of them.
  if (!pass.stamp().positions.empty()) {
    // The stamps are cooked here rather than resolved from the store, so
    // they carry no artefact number: each is given one this frame alone
    // uses, which is what makes the upload fresh and lets it go.
    uint64_t stamped = kStampArtefact | (gpu.frame << 8u);
    for (const std::string& name : pass.reads()) {
      const Cloud* cloud = targets.points(name);
      if (!cloud || cloud->positions.empty()) continue;
      const Cooked cooked = cook(Stamped{*cloud, pass.stamp()});
      if (cooked.mesh.indices.empty()) continue;
      drawBody(gpu, viewProj, viewMatrix, ++stamped, cooked.mesh,
               glm::mat4(1.0f), {0.9f, 0.9f, 0.95f, 1.0f}, nullptr, nullptr,
               view.lights, view.environment, view.orientation,
               /*lit=*/true, /*depthWrite=*/true);
    }
  }

  // The selection, drawn again in the surface the pass named — which is
  // what makes it visible in a pass that paints everything.
  if (work.realisation == Selection::Variant && pass.variant()) {
    const material::Field* field =
        pass.variant()->recipe().params().find("baseColor");
    const glm::vec4 colour = field && field->floats == 4
                                 ? pass.variant()->get<glm::vec4>("baseColor")
                                 : glm::vec4{1, 1, 1, 1};
    drawBodies(gpu, view, viewProj, viewMatrix, &pass.selector(),
               /*lit=*/false, &colour);
  }

  if (work.coverageOut.empty()) return;
  dg::ITexture* coverage = gpu.target(work.coverageOut);
  if (!coverage) return;
  const float clearNothing[4] = {0, 0, 0, 0};
  openTarget(gpu, coverage, clearNothing, true);
  const glm::vec4 white{1, 1, 1, 1};
  drawBodies(gpu, view, viewProj, viewMatrix, &work.coverageOf, /*lit=*/false,
             &white);
}

}  // namespace sigil::world::diligent
