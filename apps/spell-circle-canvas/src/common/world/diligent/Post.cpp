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
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "Gpu.h"

namespace sigil::world::diligent {

namespace {

/** One triangle, so a post stage binds no vertex buffer at all. */
constexpr uint32_t kFullscreenVertices = 3;

/** Draws @p program over the whole of @p into, reading @p textures in the
 *  order the program declares its slots. */
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
  // Binding the target also sets the viewport to the whole of it, which
  // is what lets a stage draw into a level smaller than the frame.
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

/** Which working target each stage that reads and writes at once takes,
 *  at full size. They are separate numbers because a masked blur uses
 *  all three at once. */
constexpr size_t kAcrossFullTarget = 0;
constexpr size_t kMaskedTarget = 1;
constexpr size_t kAsideTarget = 2;
/** At a reduced level: the picture halved down to it, and the same
 *  spread across. */
constexpr size_t kLevelTarget = 0;
constexpr size_t kAcrossTarget = 1;
/** A bloom's: what passed the threshold at its first level, its near
 *  spread, and its far spread with the far level's working target. */
constexpr size_t kBrightTarget = 2;
constexpr size_t kNearTarget = 3;
constexpr size_t kFarTarget = 4;

/** THE SMALLEST LEVEL A SPREAD IS TAKEN AT, and the narrowest spread a
 *  level may be asked for, in its own texels. Below two texels a level's
 *  Gaussian is too coarse to read back smoothly, so a spread is taken at
 *  the smallest level where it is still at least that wide. */
constexpr int kDeepestLevel = 4;
constexpr float kNarrowestSpread = 2.0f;

/** WHAT HALVING AND READING BACK SPREAD ON THEIR OWN, in squared pixels
 *  of the full picture. Each halving averages four by four texels of the
 *  level above, a variance of 1.25 of that level's texels squared; the
 *  read back is a cubic B-spline over the level's texels, a variance of
 *  a third of one squared. A spread taken at a level is narrowed by
 *  both, so the three together are the spread asked for. */
float spreadOfHalving(int from, int to) {
  float variance = 0.0f;
  for (int level = from + 1; level <= to; ++level) {
    const float texel = (float)(1 << (level - 1));
    variance += 1.25f * texel * texel;
  }
  return variance;
}
float spreadOfReadingBack(int level) {
  if (level <= 0) return 0.0f;
  const float texel = (float)(1 << level);
  return texel * texel / 3.0f;
}

/** The level a spread of @p sigma pixels is taken at, starting from what
 *  the picture at @p from already spread, @p spent squared pixels: the
 *  smallest whose own Gaussian is still `kNarrowestSpread` texels wide. */
int levelFor(float sigma, int from, float spent) {
  int chosen = from;
  for (int level = from + 1; level <= kDeepestLevel; ++level) {
    const float left = sigma * sigma + spent - spreadOfHalving(from, level) -
                       spreadOfReadingBack(level);
    const float texel = (float)(1 << level);
    if (left < kNarrowestSpread * kNarrowestSpread * texel * texel) break;
    chosen = level;
  }
  return chosen;
}

/** The sigma, in the texels of @p level, that leaves a spread of
 *  @p sigma pixels once the halving from @p from and the read back are
 *  counted, with @p spent squared pixels already spread before. */
float sigmaAt(float sigma, int from, int level, float spent) {
  const float left = sigma * sigma + spent - spreadOfHalving(from, level) -
                     spreadOfReadingBack(level);
  const float texel = (float)(1 << level);
  return std::sqrt(std::max(left, 0.0f)) / texel;
}

/** Draws @p program over the whole of @p into, with each texture bound
 *  at the slot its name says. */
void drawStage(
    Gpu& gpu, const material::slang::Compiled& program, dg::ITexture* into,
    SkBlendMode blend, const material::slang::Uniforms& uniforms,
    std::initializer_list<std::pair<std::string_view, dg::ITexture*>> named,
    bool clear) {
  std::vector<dg::ITexture*> textures(program.textures.size(), nullptr);
  for (size_t i = 0; i < program.textures.size(); ++i)
    for (const auto& [slot, texture] : named)
      if (program.textures[i] == slot) textures[i] = texture;
  drawStage(gpu, program, into, blend, uniforms, textures, clear);
}

/** One texel of @p texture, as `uTexel` carries it. */
std::pair<float, float> texelOf(dg::ITexture* texture) {
  const dg::TextureDesc& desc = texture->GetDesc();
  return {1.0f / (float)desc.Width, 1.0f / (float)desc.Height};
}

/** @p source halved down to @p level, from @p from, through the working
 *  targets of every level between; the first halving holds each texel
 *  against a bloom's threshold first when @p threshold. Answers the
 *  smallest level's target. */
dg::ITexture* halveDown(Gpu& gpu, dg::ITexture* source, int from, int level,
                        size_t index, dg::TEXTURE_FORMAT format,
                        const Bloom* threshold = nullptr) {
  const PostPrograms& programs = postPrograms();
  dg::ITexture* at = source;
  for (int next = from + 1; next <= level; ++next) {
    dg::ITexture* into = gpu.workingAt(next, index, format);
    if (!into) return nullptr;
    const bool first = threshold && next == from + 1;
    const material::slang::Compiled& program =
        first ? programs.downThreshold : programs.down;
    material::slang::Uniforms halve = baseUniforms(gpu, program, 0, 1);
    const auto [x, y] = texelOf(at);
    halve.set("uTexel", x, y, 0.0f, 0.0f);
    if (first)
      halve.set("uBloom", threshold->threshold, threshold->intensity, 0.0f,
                0.0f);
    drawStage(gpu, program, into, SkBlendMode::kSrc, halve, {{"uSource", at}},
              true);
    at = into;
  }
  return at;
}

/** A separable Gaussian of @p sigma texels of @p level over @p picture,
 *  in place, through that level's working target @p across. */
void spreadAt(Gpu& gpu, dg::ITexture* picture, dg::ITexture* across,
              float sigma) {
  const PostPrograms& programs = postPrograms();
  const auto [x, y] = texelOf(picture);
  material::slang::Uniforms first = baseUniforms(gpu, programs.blur, sigma, 1);
  first.set("uTexel", x, y, sigma, 0.0f);
  first.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
  drawStage(gpu, programs.blur, across, SkBlendMode::kSrc, first,
            {{"uSource", picture}}, true);
  material::slang::Uniforms second = baseUniforms(gpu, programs.blur, sigma, 1);
  second.set("uTexel", x, y, sigma, 0.0f);
  second.set("uDirection", 0.0f, 1.0f, 0.0f, 0.0f);
  drawStage(gpu, programs.blur, picture, SkBlendMode::kSrc, second,
            {{"uSource", across}}, true);
}

/** A BLUR OF @p sigma PIXELS over @p source into @p into. A narrow one is
 *  taken at full size; a wider one is taken where its footprint is still
 *  `kNarrowestSpread` texels — the picture halved down to that level, the
 *  Gaussian taken there, narrowed by what the halving and the read back
 *  spread on their own, and read back at full size through a cubic
 *  B-spline — so the work no longer grows with the square of the radius,
 *  and the blur is the same footprint as one taken at full size, within
 *  what the smaller level can resolve. */
void blurInto(Gpu& gpu, dg::ITexture* source, dg::ITexture* into, float sigma,
              SkBlendMode blend, bool clear) {
  const PostPrograms& programs = postPrograms();
  const int level = levelFor(sigma, 0, 0.0f);
  if (level == 0) {
    dg::ITexture* across = gpu.working(kAcrossFullTarget);
    if (!across) return;
    material::slang::Uniforms first =
        baseUniforms(gpu, programs.blur, sigma, 1);
    first.set("uDirection", 1.0f, 0.0f, 0.0f, 0.0f);
    drawStage(gpu, programs.blur, across, SkBlendMode::kSrc, first,
              {{"uSource", source}}, true);
    material::slang::Uniforms second =
        baseUniforms(gpu, programs.blur, sigma, 1);
    second.set("uDirection", 0.0f, 1.0f, 0.0f, 0.0f);
    drawStage(gpu, programs.blur, into, blend, second, {{"uSource", across}},
              clear);
    return;
  }
  dg::ITexture* small =
      halveDown(gpu, source, 0, level, kLevelTarget, kRadianceFormat);
  dg::ITexture* across = gpu.workingAt(level, kAcrossTarget);
  if (!small || !across) return;
  spreadAt(gpu, small, across, sigmaAt(sigma, 0, level, 0.0f));
  material::slang::Uniforms up = baseUniforms(gpu, programs.up, 0, 1);
  const auto [x, y] = texelOf(small);
  up.set("uTexel", x, y, 0.0f, 0.0f);
  drawStage(gpu, programs.up, into, blend, up, {{"uSource", small}}, clear);
}

/** THE BLOOM: @p source into @p into, with what passes the threshold
 *  spread over it twice — once to about a ninth of the radius, and that
 *  spread again by about a third of it — and added back at the
 *  intensity, so a bright point keeps a hot core inside a long soft
 *  tail. What passes the threshold is taken while the picture is halved,
 *  both spreads at the level their width allows, and the picture and the
 *  two spreads are laid together in one draw at full size. */
void bloomInto(Gpu& gpu, const Bloom& glow, dg::ITexture* source,
               dg::ITexture* into, bool clear) {
  const PostPrograms& programs = postPrograms();
  material::slang::Uniforms copy = baseUniforms(gpu, programs.copy, 0, 1);
  if (glow.intensity <= 0.0f) {
    drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, copy,
              {{"uSource", source}}, clear);
    return;
  }
  const dg::TEXTURE_FORMAT format = gpu.spreadFormat();
  const float radius = std::max(glow.radius, 0.0f);
  const float nearSigma = std::max(radius / 9.0f, 0.5f);
  const float farSigma = std::max(radius / 3.5f, 0.5f);
  // The threshold is always taken while halving once, so the near spread
  // starts at the first level whatever its width; the first halving's own
  // spread is counted against it.
  const int nearLevel = std::max(levelFor(nearSigma, 0, 0.0f), 1);
  dg::ITexture* bright =
      halveDown(gpu, source, 0, 1, kBrightTarget, format, &glow);
  dg::ITexture* near =
      bright ? halveDown(gpu, bright, 1, nearLevel, kNearTarget, format)
             : nullptr;
  dg::ITexture* nearAcross = gpu.workingAt(nearLevel, kAcrossTarget, format);
  if (!near || !nearAcross) {
    drawStage(gpu, programs.copy, into, SkBlendMode::kSrc, copy,
              {{"uSource", source}}, clear);
    return;
  }
  spreadAt(gpu, near, nearAcross, sigmaAt(nearSigma, 0, nearLevel, 0.0f));
  // THE FAR SPREAD IS THE NEAR ONE SPREAD AGAIN: what the near one
  // already spread at its level, before its read back, is counted as
  // spent.
  const float spent = spreadOfReadingBack(nearLevel);
  const int farLevel = levelFor(farSigma, nearLevel, spent);
  dg::ITexture* far =
      halveDown(gpu, near, nearLevel, farLevel, kFarTarget, format);
  if (far == near) {
    // Both spreads are read at the end, so a far one taken at the near
    // one's level is spread from a copy of it.
    far = gpu.workingAt(farLevel, kFarTarget, format);
    material::slang::Uniforms plain = baseUniforms(gpu, programs.copy, 0, 1);
    drawStage(gpu, programs.copy, far, SkBlendMode::kSrc, plain,
              {{"uSource", near}}, true);
  }
  dg::ITexture* farAcross = gpu.workingAt(farLevel, kFarTarget + 1, format);
  if (!far || !farAcross) return;
  spreadAt(gpu, far, farAcross, sigmaAt(farSigma, nearLevel, farLevel, spent));

  material::slang::Uniforms laid =
      baseUniforms(gpu, programs.bloom, 0, glow.intensity * 0.5f);
  const auto [nx, ny] = texelOf(near);
  const auto [fx, fy] = texelOf(far);
  laid.set("uTexels", nx, ny, fx, fy);
  drawStage(gpu, programs.bloom, into, SkBlendMode::kSrc, laid,
            {{"uSource", source}, {"uSecond", near}, {"uThird", far}}, clear);
}

/** The operation, applied to @p source into @p into. */
void applyOperation(Gpu& gpu, const PostOperation& operation,
                    dg::ITexture* source, dg::ITexture* into, SkBlendMode blend,
                    bool clear) {
  const PostPrograms& programs = postPrograms();
  if (const Blur* blur = std::get_if<Blur>(&operation)) {
    blurInto(gpu, source, into, blur->sigma, blend, clear);
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

  const Levels* grade = std::get_if<Levels>(&pass.operation());
  if (coverage && grade) {
    // A MASKED GRADE is one draw: the picture, and its grade laid over it
    // where the coverage stands — what the general path below draws in
    // three, with no full-size target in between.
    material::slang::Uniforms uniforms =
        baseUniforms(gpu, programs.maskedLevels, 0.0f, 1.0f);
    uniforms.set("uGrade", grade->gain, grade->lift, 1.0f, 0.0f);
    uniforms.set("uTint", grade->tint.r, grade->tint.g, grade->tint.b, 1.0f);
    drawStage(gpu, programs.maskedLevels, into, SkBlendMode::kSrc, uniforms,
              {{"uSource", layers.front()}, {"uCoverage", coverage}}, true);
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

  const SkBlendMode blend = compositeBlend(pass.operation());
  const float opacity = compositeOpacity(pass.operation());
  if (std::holds_alternative<Composite>(pass.operation()) &&
      layers.size() == 2) {
    // TWO LAYERS ARE LAID IN ONE DRAW, reading both, rather than the
    // first copied in and the second blended over it.
    material::slang::Uniforms uniforms =
        baseUniforms(gpu, programs.composite, 0.0f, 1.0f);
    uniforms.set("uLayer", opacity,
                 blend == SkBlendMode::kSrcOver ? 1.0f : 0.0f, 0.0f, 0.0f);
    drawStage(gpu, programs.composite, into, SkBlendMode::kSrc, uniforms,
              {{"uSource", layers[0]}, {"uSecond", layers[1]}}, true);
    return;
  }
  applyOperation(gpu, pass.operation(), layers.front(), into, SkBlendMode::kSrc,
                 /*clear=*/true);
  if (!std::holds_alternative<Composite>(pass.operation())) return;
  for (size_t i = 1; i < layers.size(); ++i) {
    material::slang::Uniforms uniforms =
        baseUniforms(gpu, programs.copy, 0.0f, opacity);
    drawStage(gpu, programs.copy, into, blend, uniforms, {layers[i]}, false);
  }
}

}  // namespace sigil::world::diligent
