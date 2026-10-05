/** @file
 * A post pass on the device: one triangle covering the target and one
 * fragment stage per layer, so a blur, a grade, a composite and a bloom
 * are shader passes rather than a crossing back to the host — and the
 * final stage a resource leaves the frame through.
 *
 * Every target holds linear radiance, so a blur, a composite and a bloom
 * spread and add light as it stands; a grade reads it through the
 * camera's curve. A blur here is a separable Gaussian, three sigma wide
 * and normalised over the taps it takes.
 */

#include <sigilworld/advanced/Skia.h>
#include <sigilworld/diligent/Runtime.h>

#include <algorithm>
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
/** A bloom's two: what passed the threshold, and the same blurred. */
constexpr size_t kBrightTarget = 3;
constexpr size_t kSpreadTarget = 4;

/** Draws @p program over the whole of @p into, reading @p source and
 *  @p coverage. */
void drawStage(Gpu& gpu, const material::slang::Compiled& program,
               dg::ITexture* into, SkBlendMode blend,
               const material::slang::Uniforms& uniforms,
               const std::vector<dg::ITexture*>& textures, bool clear) {
  if (program.empty() || !into) return;
  PipelineKey key{&program, blend, false, false, true};
  key.target = into->GetDesc().Format;
  const Pipeline* pipeline = gpu.pipelines.pipeline(key);
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
  device::bindDraw(gpu.shared, *pipeline, program, uniforms, textures);
  dg::DrawAttribs draw;
  draw.NumVertices = kFullscreenVertices;
  draw.Flags = dg::DRAW_FLAG_VERIFY_ALL;
  context->Draw(draw);
}

/** The texel size, the sigma and the camera's reading every stage
 *  carries. */
material::slang::Uniforms baseUniforms(const Gpu& gpu,
                                       const material::slang::Compiled& program,
                                       float sigma, float opacity) {
  const SkISize extent = gpu.extent;
  material::slang::Uniforms uniforms(program);
  uniforms.set("uTexel", extent.width() ? 1.0f / (float)extent.width() : 0.0f,
               extent.height() ? 1.0f / (float)extent.height() : 0.0f, sigma,
               0.0f);
  uniforms.set("uGrade", 1.0f, 0.0f, opacity, 0.0f);
  uniforms.set("uTint", 1.0f, 1.0f, 1.0f, 1.0f);
  uniforms.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
  uniforms.set("uTone", gpu.exposure, curveNumber(gpu.toneCurve), 0.0f, 0.0f);
  return uniforms;
}

/** A separable Gaussian of @p sigma over @p source into @p into, through
 *  the working target @p half. */
void blurInto(Gpu& gpu, dg::ITexture* source, dg::ITexture* into,
              dg::ITexture* half, float sigma, SkBlendMode blend, bool clear) {
  const PostPrograms& programs = postPrograms();
  material::slang::Uniforms across = baseUniforms(gpu, programs.blur, sigma, 1);
  across.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
  drawStage(gpu, programs.blur, half, SkBlendMode::kSrc, across, {source},
            true);
  material::slang::Uniforms down = baseUniforms(gpu, programs.blur, sigma, 1);
  down.set("uDirection", 0.0f, 1.0f, 0.0f, 0.0f);
  drawStage(gpu, programs.blur, into, blend, down, {half}, clear);
}

/** THE BLOOM: @p source into @p into, with what passes the threshold
 *  spread over it twice — once to about a third of the radius, once to
 *  the whole of it — and added back at the intensity, so a bright point
 *  keeps a hot core inside a long soft tail. */
void bloomInto(Gpu& gpu, const Bloom& glow, dg::ITexture* source,
               dg::ITexture* into, bool clear) {
  const PostPrograms& programs = postPrograms();
  dg::ITexture* bright = gpu.working(kBrightTarget);
  dg::ITexture* spread = gpu.working(kSpreadTarget);
  dg::ITexture* half = gpu.working(kBlurTarget);
  material::slang::Uniforms copy = baseUniforms(gpu, programs.copy, 0, 1);
  if (!bright || !spread || !half || glow.intensity <= 0.0f) {
    drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, copy, {source},
              clear);
    return;
  }
  material::slang::Uniforms threshold =
      baseUniforms(gpu, programs.threshold, 0, 1);
  threshold.set("uBloom", glow.threshold, glow.intensity, 0.0f, 0.0f);
  drawStage(gpu, programs.threshold, bright, SkBlendMode::kSrc, threshold,
            {source}, true);
  drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, copy, {source}, clear);
  const float radius = std::max(glow.radius, 0.0f);
  // The near spread lands in its own target and is added; the far one is
  // blurred from it, so the two together cost what one wide blur does.
  blurInto(gpu, bright, spread, half, std::max(radius / 9.0f, 0.5f),
           SkBlendMode::kSrc, true);
  material::slang::Uniforms added =
      baseUniforms(gpu, programs.copy, 0, glow.intensity * 0.5f);
  drawStage(gpu, programs.copy, into, SkBlendMode::kPlus, added, {spread},
            false);
  blurInto(gpu, spread, bright, half, std::max(radius / 3.5f, 0.5f),
           SkBlendMode::kSrc, true);
  drawStage(gpu, programs.copy, into, SkBlendMode::kPlus, added, {bright},
            false);
}

/** The operation, applied to @p source into @p into. A blur takes two passes
 *  through a working target; everything else takes one. */
void applyOperation(Gpu& gpu, const PostOperation& operation,
                    dg::ITexture* source, dg::ITexture* into, SkBlendMode blend,
                    bool clear) {
  const PostPrograms& programs = postPrograms();
  if (const Blur* blur = std::get_if<Blur>(&operation)) {
    dg::ITexture* half = gpu.working(kBlurTarget);
    if (!half) return;
    blurInto(gpu, source, into, half, blur->sigma, blend, clear);
    return;
  }
  if (const Bloom* glow = std::get_if<Bloom>(&operation)) {
    bloomInto(gpu, *glow, source, into, clear);
    return;
  }
  if (const Levels* levels = std::get_if<Levels>(&operation)) {
    material::slang::Uniforms uniforms =
        baseUniforms(gpu, programs.levels, 0.0f, 1.0f);
    uniforms.set("uGrade", levels->gain, levels->lift, 1.0f, 0.0f);
    uniforms.set("uTint", levels->tint.r, levels->tint.g, levels->tint.b, 1.0f);
    drawStage(gpu, programs.levels, into, blend, uniforms, {source}, clear);
    return;
  }
  material::slang::Uniforms uniforms =
      baseUniforms(gpu, programs.copy, 0.0f, 1.0f);
  drawStage(gpu, programs.copy, into, blend, uniforms, {source}, clear);
}

/** Which blend a composite lays a layer under. Anything this backend has
 *  no blend state for arrives as the plain one over another, which is
 *  what a pass that named no mode gets too. */
SkBlendMode compositeBlend(const PostOperation& operation) {
  const Composite* composite = std::get_if<Composite>(&operation);
  if (!composite) return SkBlendMode::kSrcOver;
  return composite->mode == material::BlendMode::PlusLighter
             ? SkBlendMode::kPlus
             : SkBlendMode::kSrcOver;
}

float compositeOpacity(const PostOperation& operation) {
  const Composite* composite = std::get_if<Composite>(&operation);
  return composite ? composite->opacity : 1.0f;
}

}  // namespace

void finalStage(Gpu& gpu, dg::ITexture* radiance, dg::ITexture* display) {
  const PostPrograms& programs = postPrograms();
  material::slang::Uniforms uniforms =
      baseUniforms(gpu, programs.final, 0.0f, 1.0f);
  drawStage(gpu, programs.final, display, SkBlendMode::kSrc, uniforms,
            {radiance}, true);
}

void fillTarget(Gpu& gpu, dg::ITexture* into, const material::Color& colour) {
  if (colour.a <= 0.0f) return;
  const PostPrograms& programs = postPrograms();
  material::slang::Uniforms uniforms =
      baseUniforms(gpu, programs.fill, 0.0f, 1.0f);
  uniforms.set("uTint", colour.r, colour.g, colour.b, colour.a);
  drawStage(gpu, programs.fill, into, SkBlendMode::kSrc, uniforms, {}, false);
}

void applyPost(Gpu& gpu, const PassWork& work) {
  const Pass& pass = *work.pass;
  const std::span<const std::string> writes = pass.writes();
  if (writes.empty()) return;

  // The layers are taken BEFORE the target is opened, because a pass may
  // write what it reads: opening it first would hand the operation its own
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
    material::slang::Uniforms copy =
        baseUniforms(gpu, programs.copy, 0.0f, 1.0f);
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
    // Masked: the picture stands everywhere, and the operation reaches it only
    // where the coverage does. The operation lands in a working target first
    // because the coverage has to multiply the OP, not the picture.
    material::slang::Uniforms plain =
        baseUniforms(gpu, programs.copy, 0.0f, 1.0f);
    drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, plain,
              {layers.front()}, true);
    dg::ITexture* lifted = gpu.working(kMaskedTarget);
    if (!lifted) return;
    applyOperation(gpu, pass.operation(), layers.front(), lifted,
                   SkBlendMode::kSrc, true);
    material::slang::Uniforms masked =
        baseUniforms(gpu, programs.masked, 0.0f, 1.0f);
    drawStage(gpu, programs.masked, into, SkBlendMode::kSrcOver, masked,
              {lifted, coverage}, false);
    return;
  }

  applyOperation(gpu, pass.operation(), layers.front(), into, SkBlendMode::kSrc,
                 /*clear=*/true);
  const SkBlendMode blend = compositeBlend(pass.operation());
  const float opacity = compositeOpacity(pass.operation());
  if (!std::holds_alternative<Composite>(pass.operation())) return;
  for (size_t i = 1; i < layers.size(); ++i) {
    material::slang::Uniforms uniforms =
        baseUniforms(gpu, programs.copy, 0.0f, opacity);
    drawStage(gpu, programs.copy, into, blend, uniforms, {layers[i]}, false);
  }
}

}  // namespace sigil::world::diligent
