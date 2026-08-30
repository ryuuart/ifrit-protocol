/** @file
 * A post pass on the device: one triangle covering the target and one
 * fragment stage per layer, so a blur, a grade and a composite are
 * shader passes rather than a crossing back to the host.
 *
 * WHAT IS NOT THE SAME AS THE HOST'S ANSWER. A blur here is a separable
 * Gaussian, three sigma wide and normalised over the taps it takes; a
 * raster blur is a box approximation of one. They are the same effect
 * and not the same kernel, which is why the tier that compares them
 * states a tolerance rather than asking for identical bytes.
 */

#include <sigilworld/diligent/Runtime.h>

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** One triangle, so a post stage binds no vertex buffer at all. */
constexpr uint32_t kFullscreenVertices = 3;

/** Which working target each stage that reads and writes at once takes.
 *  They are separate numbers because a masked blur uses all three at
 *  once. */
constexpr size_t kBlurTarget = 0;
constexpr size_t kMaskedTarget = 1;
constexpr size_t kAsideTarget = 2;

/** Draws @p program over the whole of @p into, reading @p source and
 *  @p coverage. */
void drawStage(Gpu& gpu, const Compiled& program, dg::ITexture* into,
               SkBlendMode blend, const Uniforms& uniforms,
               const std::vector<dg::ITexture*>& textures, bool clear) {
  if (program.empty() || !into) return;
  const PipelineKey key{&program, blend, false, false, true};
  const Pipeline* pipeline = gpu.pipeline(key);
  if (!pipeline) return;

  dg::IDeviceContext* context = gpu.device->context();
  dg::ITextureView* rtv = into->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
  context->SetRenderTargets(1, &rtv, nullptr,
                            dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  if (clear) {
    const float nothing[4] = {0, 0, 0, 0};
    context->ClearRenderTarget(rtv, nothing,
                               dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
  context->SetPipelineState(pipeline->state);
  bindAndCommit(gpu, *pipeline, program, uniforms, textures);
  dg::DrawAttribs draw;
  draw.NumVertices = kFullscreenVertices;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->Draw(draw);
}

/** The texel size and the sigma every stage carries. */
Uniforms baseUniforms(const Compiled& program, SkISize extent, float sigma,
                      float opacity) {
  Uniforms uniforms(program);
  uniforms.set("uTexel", extent.width() ? 1.0f / (float)extent.width() : 0.0f,
               extent.height() ? 1.0f / (float)extent.height() : 0.0f, sigma,
               0.0f);
  uniforms.set("uGrade", 1.0f, 0.0f, opacity, 0.0f);
  uniforms.set("uTint", 1.0f, 1.0f, 1.0f, 1.0f);
  uniforms.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
  return uniforms;
}

/** The op, applied to @p source into @p into. A blur takes two passes
 *  through a working target; everything else takes one. */
void applyOp(Gpu& gpu, const PostOp& op, dg::ITexture* source,
             dg::ITexture* into, SkBlendMode blend, bool clear) {
  const PostPrograms& programs = postPrograms();
  if (const Blur* blur = std::get_if<Blur>(&op)) {
    dg::ITexture* half = gpu.working(kBlurTarget);
    if (!half) return;
    Uniforms across =
        baseUniforms(programs.blur, gpu.extent, blur->sigma, 1.0f);
    across.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
    drawStage(gpu, programs.blur, half, SkBlendMode::kSrc, across, {source},
              true);
    Uniforms down = baseUniforms(programs.blur, gpu.extent, blur->sigma, 1.0f);
    down.set("uDirection", 0.0f, 1.0f, 0.0f, 0.0f);
    drawStage(gpu, programs.blur, into, blend, down, {half}, clear);
    return;
  }
  if (const Levels* levels = std::get_if<Levels>(&op)) {
    Uniforms uniforms = baseUniforms(programs.levels, gpu.extent, 0.0f, 1.0f);
    uniforms.set("uGrade", levels->gain, levels->lift, 1.0f, 0.0f);
    uniforms.set("uTint", levels->tint.fR, levels->tint.fG, levels->tint.fB,
                 1.0f);
    drawStage(gpu, programs.levels, into, blend, uniforms, {source}, clear);
    return;
  }
  Uniforms uniforms = baseUniforms(programs.copy, gpu.extent, 0.0f, 1.0f);
  drawStage(gpu, programs.copy, into, blend, uniforms, {source}, clear);
}

/** Which blend a composite lays a layer under. Anything this backend has
 *  no blend state for arrives as the plain one over another, which is
 *  what a pass that named no mode gets too. */
SkBlendMode compositeBlend(const PostOp& op) {
  const Composite* composite = std::get_if<Composite>(&op);
  if (!composite) return SkBlendMode::kSrcOver;
  return composite->mode == SkBlendMode::kPlus ? SkBlendMode::kPlus
                                               : SkBlendMode::kSrcOver;
}

float compositeOpacity(const PostOp& op) {
  const Composite* composite = std::get_if<Composite>(&op);
  return composite ? composite->opacity : 1.0f;
}

}  // namespace

void applyPost(Gpu& gpu, const PassWork& work) {
  const Pass& pass = *work.pass;
  const std::span<const std::string> writes = pass.writes();
  if (writes.empty()) return;

  // The layers are taken BEFORE the target is opened, because a pass may
  // write what it reads: opening it first would hand the op its own
  // blank target instead of the picture.
  const PostPrograms& programs = postPrograms();
  std::vector<dg::ITexture*> layers;
  for (const std::string& read : pass.reads()) {
    if (read == work.coverageIn) continue;
    if (dg::ITexture* texture = gpu.current(read)) layers.push_back(texture);
  }
  for (const std::string& before : pass.previous())
    if (dg::ITexture* texture = gpu.previous(before)) layers.push_back(texture);
  dg::ITexture* coverage =
      work.coverageIn.empty() ? nullptr : gpu.current(work.coverageIn);

  dg::ITexture* into = gpu.target(writes.front());
  if (!into) return;
  // A pass may WRITE WHAT IT READS. On the host the layers are snapshots
  // taken before the target is cleared; here they are the textures
  // themselves, so the one that is also the target is copied aside
  // first — a device cannot sample an image it is drawing into.
  for (dg::ITexture*& layer : layers) {
    dg::ITexture* aside = gpu.working(kAsideTarget);
    if (layer != into || !aside) continue;
    Uniforms copy = baseUniforms(programs.copy, gpu.extent, 0.0f, 1.0f);
    drawStage(gpu, programs.copy, aside, SkBlendMode::kSrc, copy, {layer},
              true);
    layer = aside;
  }
  if (layers.empty()) {
    const float nothing[4] = {0, 0, 0, 0};
    dg::ITextureView* rtv =
        into->GetDefaultView(dg::TEXTURE_VIEW_RENDER_TARGET);
    gpu.device->context()->SetRenderTargets(
        1, &rtv, nullptr, dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    gpu.device->context()->ClearRenderTarget(
        rtv, nothing, dg::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    return;
  }

  if (coverage) {
    // Masked: the picture stands everywhere, and the op reaches it only
    // where the coverage does. The op lands in a working target first
    // because the coverage has to multiply the OP, not the picture.
    Uniforms plain = baseUniforms(programs.copy, gpu.extent, 0.0f, 1.0f);
    drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, plain,
              {layers.front()}, true);
    dg::ITexture* lifted = gpu.working(kMaskedTarget);
    if (!lifted) return;
    applyOp(gpu, pass.op(), layers.front(), lifted, SkBlendMode::kSrc, true);
    Uniforms masked = baseUniforms(programs.masked, gpu.extent, 0.0f, 1.0f);
    drawStage(gpu, programs.masked, into, SkBlendMode::kSrcOver, masked,
              {lifted, coverage}, false);
    return;
  }

  applyOp(gpu, pass.op(), layers.front(), into, SkBlendMode::kSrc,
          /*clear=*/true);
  const SkBlendMode blend = compositeBlend(pass.op());
  const float opacity = compositeOpacity(pass.op());
  if (!std::holds_alternative<Composite>(pass.op())) return;
  for (size_t i = 1; i < layers.size(); ++i) {
    Uniforms uniforms = baseUniforms(programs.copy, gpu.extent, 0.0f, opacity);
    drawStage(gpu, programs.copy, into, blend, uniforms, {layers[i]}, false);
  }
}

}  // namespace sigil::world::diligent
