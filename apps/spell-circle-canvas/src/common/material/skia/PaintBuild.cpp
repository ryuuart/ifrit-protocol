/** @file
 * THE SHADER A PAINT BUILDS: the sksl recipe assembled into a runtime
 * shader (constants, bound outputs, the auto-injected frame inputs and
 * the child slots), the material instance resolved through the core's
 * program cache, the pass specialization the text runtime fills, and the
 * two image constructions a pan and a fit ask for. Each memoises on the
 * bytes it was built from, so a settled paint hands back one shader.
 */

#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Program.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

namespace {

/** Every resolved byte of @p m and its descendants at @p frame, appended
 *  to @p key — the memo's whole input. */
void digest(const sigil::material::Material& m,
            const sigil::material::FrameData& frame,
            std::vector<std::byte>& key) {
  const sigil::material::Material::Resolved r =
      m.resolve(sigil::material::Target::SkSL, frame);
  key.insert(key.end(), r.bytes.begin(), r.bytes.end());
  for (const auto& [slot, child] : m.children())
    if (child.material) digest(*child.material, frame, key);
}

}  // namespace

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected uTime/uResolution/uContentScale — the
// last three only when the effect actually declares them (assigning a uniform
// the effect lacks aborts in debug builds). `paintFrame` is null for the static
// build.
sk_sp<SkShader> Paint::build(const Live& live, const PaintFrame* paintFrame,
                             bool worldSpace) {
  const sk_sp<SkRuntimeEffect>& effect = live.effect;
  if (!effect) return nullptr;
  // A user-provided uniform (constant or bound) OWNS its slot, and the
  // auto-injects below must never overwrite it. Binding uTime to a
  // caller-driven Output is a supported way to control a material's clock,
  // and an injection that ran afterwards would overwrite that value with
  // the context's own elapsed time without saying anything.
  auto userProvided = [&](std::string_view name) {
    for (const auto& [n, v] : live.constants)
      if (n == name) return true;
    for (const auto& [n, v] : live.constants2)
      if (n == name) return true;
    for (const auto& [n, v] : live.constants4)
      if (n == name) return true;
    for (const auto& [n, v] : live.constantArrays)
      if (n == name) return true;
    for (const auto& [n, o] : live.binds)
      if (n == name) return true;
    for (const auto& [n, b] : live.blocks)
      if (n == name) return true;
    return false;
  };
  const bool injectTime = paintFrame &&
                          validUniform(effect, "uTime", sizeof(float)) &&
                          !userProvided("uTime");
  const bool injectScale =
      paintFrame && validUniform(effect, "uContentScale", sizeof(float)) &&
      !userProvided("uContentScale");
  const bool injectResolution =
      paintFrame && validUniform(effect, "uResolution", 2 * sizeof(float)) &&
      !userProvided("uResolution");

  // A world-space material's uResolution is the ROOT canvas size, not the
  // node's box: the shader is already sampling in root coordinates, so
  // dividing by the node's size would rescale the field per node and the
  // two siblings that were meant to share one continuous field would each
  // get their own.
  const SkSize resolutionSize =
      worldSpace && paintFrame && !paintFrame->rootSize.isEmpty()
          ? paintFrame->rootSize
          : (paintFrame ? paintFrame->size : SkSize::MakeEmpty());

  // The varying-input digest (constants are fixed per recipe; injected
  // values participate only when actually injected).
  std::vector<float> inputs;
  if (paintFrame) {
    inputs.reserve(live.binds.size() + 2 * live.blocks.size() + 10);
    for (const auto& [name, out] : live.binds)
      inputs.push_back(motion::resolveFloatAt(nullptr, out));
    // A block's REVISION stands in for its values: commit() moves it, an
    // uncommitted frame leaves it, and two floats carry it without the
    // digest walking the whole table. Split because a float holds 24 bits
    // exactly and a long session's commit count does not fit in them.
    for (const auto& [name, block] : live.blocks) {
      const uint64_t revision = block ? block->revision() : 0;
      inputs.push_back((float)(revision & 0xffffffull));
      inputs.push_back((float)(revision >> 24u));
    }
    // When anchored, W is a varying input like any other: a node that MOVED
    // resolves a different shader, and the memo has to see that or the next
    // frame after the move replays the shader anchored to the old position.
    if (worldSpace) digestToRoot(inputs, paintFrame->toRoot);
    if (injectTime) {
      // Quantized through motion::quantizeTime rather than inline, so that
      // the value digested here and the value assigned to the uniform below
      // are produced by one function at one precision. Two spellings would
      // let the memo compare a time the shader was not built with.
      const double t = sigil::motion::quantizeTime(paintFrame->seconds,
                                                   (double)live.timeQuantizeHz);
      inputs.push_back((float)t);
    }
    if (injectScale) inputs.push_back(paintFrame->contentScale);
    if (injectResolution) {
      inputs.push_back(resolutionSize.width());
      inputs.push_back(resolutionSize.height());
    }
    // THE MEMO'S BLIND SPOT, closed at the door rather than papered over: a
    // child's varying inputs are the CHILD's (its own binds, its own uTime,
    // its own uResolution) and this digest cannot see them, so a material
    // with a context-needing child skips the memo entirely and rebuilds.
    // Returning the memoised shader there would freeze the child at the
    // frame it was first resolved. Static children (an image, a ramp) are
    // recipe and never vary, so they leave the memo intact.
    bool childNeedsCtx = false;
    for (const auto& [name, child] : live.children)
      childNeedsCtx |= child.isAnimated() || child.geometryDependent();
    if (!childNeedsCtx)
      if (sk_sp<SkShader> memoised = live.memo.hit(inputs)) return memoised;
  }
  // An sdf style reserves its glow, shadow and border padding INSIDE the
  // node's box, so a generous glow on a modest box leaves almost no
  // interior and the shape all but disappears — with no error anywhere.
  // The two numbers that decide it only meet here (uPad comes from the
  // style, uResolution from layout), so this is the only place the warning
  // can be issued. Once per process, recognised by the sdf prelude's
  // uniform signature, when the reserve is at least half the shorter side.
  if (injectResolution && paintFrame->size.width() > 0 &&
      paintFrame->size.height() > 0) {
    static bool warnedPad = false;
    if (!warnedPad && validUniform(effect, "uPad", sizeof(float)) &&
        validUniform(effect, "uGlowR", sizeof(float))) {
      const float halfMin =
          0.5f * std::min(paintFrame->size.width(), paintFrame->size.height());
      for (const auto& [name, value] : live.constants)
        if (name == "uPad" && value >= halfMin) {
          warnedPad = true;
          SkDebugf(
              "[material] sdf material: pad %.1f px >= half of the "
              "%.0fx%.0f box — the style's reserve (glow/shadow/border) "
              "eats the whole interior and the visible shape is ~%.1f px "
              "across. Size the node with material::sdf::minBoxFor(style, "
              "contentPx) = content + 2*material::sdf::pad(style). (warned "
              "once)\n",
              value, paintFrame->size.width(), paintFrame->size.height(),
              std::max(1.0f, std::min(paintFrame->size.width(),
                                      paintFrame->size.height()) -
                                 2 * value));
          break;
        }
    }
  }
  SkRuntimeShaderBuilder b(effect);
  for (const auto& [name, value] : live.constants)
    b.uniform(name) = value;  // entries pre-validated at store time
  for (const auto& [name, value] : live.constants2) b.uniform(name) = value;
  for (const auto& [name, value] : live.constants4) b.uniform(name) = value;
  for (const auto& [name, values] : live.constantArrays)
    b.uniform(name).set(values.data(), (int)values.size());
  for (const auto& [name, out] : live.binds)
    b.uniform(name) = motion::resolveFloatAt(nullptr, out);
  for (const auto& [name, block] : live.blocks)
    if (block)  // size pre-validated at store: a full array write, exactly
      b.uniform(name).set(block->values().data(), (int)block->size());
  // Children resolve with the SAME PaintFrame the parent got (so a live
  // child ticks and a geometry child reads the parent node's box), and with
  // the null context on the static snapshot path.
  for (const auto& [name, child] : live.children)
    b.child(name) =
        detail::childShader(child, paintFrame);  // pre-validated at store
  if (paintFrame) {
    // Auto-injects are size-checked too: a user declaring `uniform float
    // uResolution` must not receive a float2 write (SkDEBUGFAIL).
    if (injectTime) {
      b.uniform("uTime") = (float)sigil::motion::quantizeTime(
          paintFrame->seconds, (double)live.timeQuantizeHz);
    }
    if (injectScale) b.uniform("uContentScale") = paintFrame->contentScale;
    if (injectResolution)
      b.uniform("uResolution") =
          std::array<float, 2>{resolutionSize.width(), resolutionSize.height()};
  }
  sk_sp<SkShader> built = b.makeShader();
  // Wrap BEFORE the memo stores. A held world-space field then keeps one
  // stable shader pointer across frames, and shader-pointer stability is
  // exactly what the painter's live-material memo compares to decide a
  // recording can replay. Wrapping after the store would mint a fresh
  // wrapper per resolve and the material would read as never holding still.
  if (worldSpace && paintFrame)
    built = anchorToRoot(std::move(built), *paintFrame);
  if (paintFrame) live.memo.store(std::move(inputs), built);
  return built;
}

sk_sp<SkShader> Paint::buildBacked(const PaintFrame* paintFrame) const {
  const Backed& backed = *m_backed;
  // A PASS BODY HAS NO STANDALONE SHADER. It is written against the
  // declarations the fx() runtime prepends once it knows the track's unit
  // count, so compiling it here would report one error per mention of a
  // name that does not exist yet — a page of diagnostics about a compile
  // nobody asked for, on a material that then renders correctly through
  // resolvePass(). Nothing is lost: a pass material used as an ordinary
  // fill has no picture to give, and now it says so by drawing nothing
  // rather than by failing loudly at load.
  if (detail::isPassBody(backed.material.recipe())) return nullptr;
  sigil::material::FrameData frame;
  std::vector<std::byte> key;
  if (paintFrame) {
    frame.seconds = paintFrame->seconds;
    frame.contentScale = paintFrame->contentScale;
    // A world-space material's uResolution is the ROOT canvas size, as on
    // the sksl path: the shader samples in root coordinates.
    const SkSize resolutionSize =
        m_worldSpace && !paintFrame->rootSize.isEmpty() ? paintFrame->rootSize
                                                        : paintFrame->size;
    frame.resolution = {resolutionSize.width(), resolutionSize.height()};
    // An sdf style reserves its glow, shadow and border padding INSIDE the
    // box, so a generous glow on a modest box leaves almost no interior and
    // the shape all but disappears — with no error anywhere. The two
    // numbers that decide it only meet here, once per process.
    const sigil::material::Recipe& r = backed.material.recipe();
    if (r.reads(sigil::material::FrameInput::Resolution) &&
        r.name().rfind("sdf.", 0) == 0 && paintFrame->size.width() > 0 &&
        paintFrame->size.height() > 0) {
      static bool warnedPad = false;
      const float pad = backed.material.get<float>("uPad");
      const float halfMin =
          0.5f * std::min(paintFrame->size.width(), paintFrame->size.height());
      if (!warnedPad && pad >= halfMin) {
        warnedPad = true;
        SkDebugf(
            "[material] sdf material: pad %.1f px >= half of the "
            "%.0fx%.0f box — the style's reserve (glow/shadow/border) "
            "eats the whole interior and the visible shape is ~%.1f px "
            "across. Size the node with material::sdf::minBoxFor(style, "
            "contentPx) = content + 2*material::sdf::pad(style). (warned "
            "once)\n",
            pad, paintFrame->size.width(), paintFrame->size.height(),
            std::max(1.0f, std::min(paintFrame->size.width(),
                                    paintFrame->size.height()) -
                               2 * pad));
      }
    }
    digest(backed.material, frame, key);
    if (m_worldSpace) {
      std::vector<float> w;
      digestToRoot(w, paintFrame->toRoot);
      const auto* bytes = reinterpret_cast<const std::byte*>(w.data());
      key.insert(key.end(), bytes, bytes + w.size() * sizeof(float));
    }
    if (sk_sp<SkShader> memoised = backed.memo.hit(key)) return memoised;
  }
  sk_sp<SkShader> built = sigil::material::skia::shader(backed.material, frame);
  if (m_worldSpace && paintFrame)
    built = anchorToRoot(std::move(built), *paintFrame);
  if (paintFrame) backed.memo.store(std::move(key), built);
  return built;
}

/** The panned build — the ONE construction resolve() and asShader() share
 *  for a bound-offset material: the recipe's static matrix post-translated
 *  by the bindings' CURRENT values.
 *
 *  A fresh shader is minted per call, and that is fine here: the paint
 *  layer's scalar memo compares the resolved pan values rather than the
 *  shader pointer, so a node whose pan is holding still keeps its recording
 *  even though this hands back a new pointer each time. */
sk_sp<SkShader> Paint::pannedImageShader() const {
  if (!m_recipe) return nullptr;
  sk_sp<SkImage> img =
      m_recipe->kind == Recipe::Kind::Buffer
          ? (m_recipe->source ? m_recipe->source->image() : nullptr)
          : m_recipe->image;
  if (!img) return nullptr;
  SkMatrix local = m_recipe->local;
  const SkPoint pan = boundOffsetValue();
  local.postTranslate(pan.fX, pan.fY);
  return SkShaders::Image(std::move(img), m_recipe->tx, m_recipe->ty,
                          m_recipe->sampling, &local);
}

/** The FITTED build, the box's answer to the pan's: the source mapped
 *  onto @p frame's box by the stated rule, then post-translated by any
 *  bound pan exactly as `pannedImageShader` translates the static matrix.
 *  Null when there is no fit, no source, or no box to fit to — the caller
 *  falls through to the unfitted form, which is the same degradation a
 *  world-space material makes outside a composer. */
sk_sp<SkShader> Paint::fittedImageShader(const PaintFrame& frame) const {
  if (!hasFit() || frame.size.isEmpty()) return nullptr;
  sk_sp<SkImage> img =
      m_recipe->kind == Recipe::Kind::Buffer
          ? (m_recipe->source ? m_recipe->source->image() : nullptr)
          : m_recipe->image;
  if (!img || img->width() <= 0 || img->height() <= 0) return nullptr;
  const float iw = (float)img->width(), ih = (float)img->height();
  const float bw = frame.size.width(), bh = frame.size.height();
  const float sx = bw / iw, sy = bh / ih;
  SkMatrix local;
  switch (m_recipe->fit) {
    case Fit::Stretch:
      local = SkMatrix::Scale(sx, sy);
      break;
    case Fit::Cover:
    case Fit::Contain: {
      // One scale for both axes; which one is the whole difference
      // between covering the box and sitting inside it. Centred either
      // way, so the crop takes the same from both edges and the margin
      // leaves the same at both.
      const float k =
          m_recipe->fit == Fit::Cover ? std::max(sx, sy) : std::min(sx, sy);
      local = SkMatrix::Scale(k, k);
      local.postTranslate((bw - iw * k) * 0.5f, (bh - ih * k) * 0.5f);
      break;
    }
    case Fit::Native:
      return nullptr;
  }
  const SkPoint pan = boundOffsetValue();
  local.postTranslate(pan.fX, pan.fY);
  return SkShaders::Image(std::move(img), m_recipe->tx, m_recipe->ty,
                          m_recipe->sampling, &local);
}

sk_sp<SkShader> Paint::resolvePass(const PassInputs& in,
                                   const PaintFrame& paintFrame) const {
  if (!m_backed) return nullptr;
  const Backed& backed = *m_backed;
  const uint32_t n = std::max(in.units, 1u);
  std::shared_ptr<const sigil::material::Recipe> spec =
      detail::passRecipeFor(backed.material.recipePointer(), n);
  if (!spec) return nullptr;
  // The instance is respecialized per draw rather than held: it carries
  // the material's CURRENT values, and a held one would go stale the
  // moment a uniform was set on the material it was taken from. The
  // definition it points at is what is cached, and that is where the
  // compile lives.
  const sigil::material::Material specialized =
      backed.material.withRecipe(std::move(spec));
  sigil::material::FrameData frame;
  frame.seconds = paintFrame.seconds;
  frame.contentScale = paintFrame.contentScale;
  const SkSize resolutionSize = m_worldSpace && !paintFrame.rootSize.isEmpty()
                                    ? paintFrame.rootSize
                                    : paintFrame.size;
  frame.resolution = {resolutionSize.width(), resolutionSize.height()};
  // uContent is the runtime's to fill, never the instance's: the layer is
  // rendered per draw and no material value could name it.
  static constexpr std::string_view kContent = "uContent";
  const std::array<std::string_view, 1> leave{kContent};
  std::unique_ptr<SkRuntimeShaderBuilder> b =
      sigil::material::skia::builder(specialized, frame, {}, leave);
  if (!b) return nullptr;  // the cache already said why, once
  // A procedural pass may replace the layer without sampling it, in
  // which case the compiled program declares no content slot.
  if (b->effect()->findChild(kContent)) b->child(kContent) = in.content;
  // The array counts are the specialization's by construction, and the
  // builder demands exactly them.
  if (in.rects) b->uniform("uUnitRect").set(in.rects, (int)n * 4);
  if (in.phases) b->uniform("uUnitPhase").set(in.phases, (int)n * 2);
  sk_sp<SkShader> built = b->makeShader();
  // No memo here: the per-unit phases move every frame the pass is live,
  // and a settled pass replays from its node's recording rather than
  // resolving.
  if (m_worldSpace) built = anchorToRoot(std::move(built), paintFrame);
  return built;
}

}  // namespace sigil::material::skia
