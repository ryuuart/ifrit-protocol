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
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Params.h>
#include <sigilworld/diligent/Runtime.h>
#include <sigilworld/light/Light.h>

#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <span>
#include <string>
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
/** How many emitters one draw carries. */
constexpr size_t kLights = 4;

/** The half of the artefact numbering a frame's OWN cooks take. The
 *  scene's store counts up from one, so nothing it hands out ever
 *  reaches here. */
constexpr uint64_t kStampArtefact = 1ull << 63u;

/** THE CAMERA AS THE DEVICE WANTS IT.
 *
 *  The camera's `viewProjection` lands in PIXELS, because that is what a
 *  canvas concat needs; a device wants clip space, so the projection and
 *  the view are composed without the viewport step. What is left to
 *  correct is depth: the projection runs z from one at the near plane to
 *  minus one at the far one, and the device wants zero to one the other
 *  way about. The x and y of that clip space already agree — both count
 *  y upward — so nothing turns them over. */
glm::mat4 clipFor(const Camera& camera, SkISize extent) {
  const float aspect = extent.height() > 0
                           ? (float)extent.width() / (float)extent.height()
                           : 1.0f;
  glm::mat4 depth(1.0f);
  depth[2][2] = -0.5f;
  depth[3][2] = 0.5f;
  return depth * camera.projection(aspect) * camera.view();
}

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
  const Compiled* program = nullptr;
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
      resolved.program ? resolved.program->as<SlangProgram>() : nullptr;
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
void writeMaterial(Uniforms& uniforms, const Surface& surface) {
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
void writeScaffold(Uniforms& uniforms, const Compiled& program,
                   const glm::mat4& viewProj, const glm::mat4& view,
                   const glm::mat4& model, glm::vec4 baseColor,
                   std::span<const Light> lights, bool lit) {
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
  uniforms.set("uAmbient", kAmbient, 4);
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

/** One body, drawn. */
void drawBody(Gpu& gpu, const glm::mat4& viewProj, const glm::mat4& view,
              uint64_t artefact, const Mesh& mesh, const glm::mat4& model,
              glm::vec4 baseColor, const material::Material* material,
              std::span<const Light> lights, bool lit, bool depthWrite) {
  const MeshBuffers* buffers = gpu.upload(artefact, mesh);
  if (!buffers) return;
  const Surface surface = surfaceOf(material, lit);
  if (!surface.program || surface.program->empty()) return;

  const PipelineKey key{surface.program, SkBlendMode::kSrcOver, true,
                        depthWrite, false};
  const Pipeline* pipeline = gpu.pipeline(key);
  if (!pipeline) return;

  Uniforms uniforms(*surface.program);
  writeScaffold(uniforms, *surface.program, viewProj, view, model, baseColor,
                lights, lit);
  writeMaterial(uniforms, surface);

  dg::IDeviceContext* context = gpu.device->context();
  context->SetPipelineState(pipeline->state);
  bindAndCommit(gpu, *pipeline, *surface.program, uniforms, {});
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
    drawBody(gpu, viewProj, viewMatrix, body.geometry, *body.mesh, body.world,
             colour, flat ? nullptr : body.material, view.lights, lit,
             /*depthWrite=*/colour.a >= 1.0f);
  }
}

/** Binds @p colour as the pass's target, with the depth buffer when one
 *  is wanted, and clears both. */
void openTarget(Gpu& gpu, dg::ITexture* colour, const float* clear,
                bool withDepth) {
  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* rtv =
      colour->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  dg::ITextureView* dsv =
      withDepth && gpu.depth
          ? gpu.depth->GetDefaultView(dg::TEXTURE_VIEW_DEPTH_STENCIL)
          : nullptr;
  context->SetRenderTargets(1, &rtv, dsv,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(rtv, clear,
                             dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (dsv)
    context->ClearDepthStencil(dsv, dg::CLEAR_DEPTH_FLAG, 1.0f, 0,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
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
               glm::mat4(1.0f), {0.9f, 0.9f, 0.95f, 1.0f}, nullptr, view.lights,
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
