/** @file
 * THE SHADER A PAINT BUILDS: the sksl recipe assembled into a runtime
 * shader (constants, bound outputs, the auto-injected frame inputs and
 * the slots), the material instance resolved through the core's
 * program cache, the pass specialization the text runtime fills, and the
 * two image constructions a pan and a fit ask for. Each memoises on the
 * bytes it was built from, so a settled paint hands back one shader.
 */

#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/advanced/Leaf.h>
#include <sigilmaterial/advanced/Program.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Environment.h"
#include "PaintInternal.h"

namespace sigil::material::skia {

SkSize PaintAccess::environmentDomain(const Paint::Live& live) {
  SkSize size = SkSize::MakeEmpty();
  for (const auto& [name, value] : live.constants2)
    if (name == "uEnvironmentSize") size = {value[0], value[1]};
  for (const auto& [name, value] : live.constantArrays)
    if (name == "uEnvironmentSize" && value.size() == 2)
      size = {value[0], value[1]};
  for (const auto& [name, block] : live.blocks)
    if (name == "uEnvironmentSize" && block) {
      const auto value = block->committedValues();
      size = {value[0], value[1]};
    }
  return size;
}

namespace {

struct CapturedFrame {
  std::shared_ptr<const media::Frame> frame;
  media::Frame frameAt(std::chrono::duration<double>) const { return *frame; }
  bool isRunning() const { return false; }
  glm::ivec2 size() const {
    return frame->image ? frame->image.size()
                        : glm::ivec2{frame->device.width, frame->device.height};
  }
  bool operator==(const CapturedFrame&) const = default;
};

struct CapturedMaterial {
  Material sampled;
  bool usesRecorder = false;
};

CapturedMaterial captureSources(const Material& material,
                                std::vector<CapturedLeaf>& leaves,
                                const std::vector<CapturedLeaf>& previous) {
  // Paint executes the base, layers and masks. Surface and effects are
  // consumed by their own executors rather than this shader fold.
  CapturedMaterial result{material.base(), material.source() != nullptr};
  for (const auto& [name, child] : material.slots()) {
    if (!material.recipe().samples(Target::SkSL, name)) continue;
    if (child.material) {
      auto captured = captureSources(*child.material, leaves, previous);
      result.usesRecorder |= captured.usesRecorder;
      result.sampled.slot(name, std::move(captured.sampled));
    } else if (const auto* texture =
                   dynamic_cast<const Texture*>(child.leaf.get());
               texture && !texture->animated()) {
      const auto find = [&](const auto& entries) {
        return std::find_if(
            entries.begin(), entries.end(),
            [&](const auto& input) { return input.description == child.leaf; });
      };
      auto found = find(leaves);
      if (found == leaves.end()) {
        const auto held = find(previous);
        if (held != previous.end()) {
          leaves.push_back(*held);
        } else {
          media::Frame frame = texture->source().frameAt({});
          const bool device = !frame.image && bool(frame.device);
          Texture captured(media::PixelSource(CapturedFrame{
              std::make_shared<const media::Frame>(std::move(frame))}));
          captured.tile(texture->tileX(), texture->tileY())
              .uv(texture->uv())
              .sampling(texture->sampling());
          if (texture->region()) captured.region(*texture->region());
          leaves.push_back(
              {child.leaf, std::make_shared<const Texture>(std::move(captured)),
               device});
        }
        found = std::prev(leaves.end());
      }
      result.usesRecorder |= found->usesRecorder;
      result.sampled.slot(name, found->sampled);
    } else if (child.leaf) {
      // A backend leaf can choose a shader for the supplied recorder.
      result.usesRecorder = true;
    }
  }
  for (const Layer& layer : material.layers()) {
    auto captured = captureSources(layer.source, leaves, previous);
    result.usesRecorder |= captured.usesRecorder;
    auto options = layer.options;
    if (options.mask) {
      auto mask = captureSources(options.mask->source, leaves, previous);
      result.usesRecorder |= mask.usesRecorder;
      options.mask->source = std::move(mask.sampled);
    }
    result.sampled.layer(std::move(captured.sampled), options);
  }
  return result;
}

/** Append resolved upload bytes; return false when sampled sources can
 *  change independently of those bytes, so the key cannot prove reuse. */
bool digest(const sigil::material::Material& m,
            const sigil::material::FrameData& frame,
            std::vector<std::byte>& uniforms,
            std::vector<std::shared_ptr<const Leaf>>& leaves,
            std::vector<std::shared_ptr<const Material>>& materials,
            bool applyWorldSpace = true) {
  // Composed children are lowered through their paint source, layers and
  // masks. Their varying inputs are outside the recipe's upload bytes.
  if (m.isComposed() && (m.isRunning() || m.geometryDependent())) return false;
  const FrameData resolvedFrame =
      applyWorldSpace && m.worldSpace() ? rootSamplingFrame(frame) : frame;
  const sigil::material::Material::Resolved r =
      m.resolve(sigil::material::Target::SkSL, resolvedFrame);
  uniforms.insert(uniforms.end(), r.bytes.begin(), r.bytes.end());
  for (const auto& [slot, child] : m.slots()) {
    if (!m.recipe().samples(sigil::material::Target::SkSL, slot)) continue;
    if (child.leaf &&
        (child.leaf->animated() || child.leaf->geometryDependent()))
      return false;
    if (child.leaf) leaves.push_back(child.leaf);
    if (child.material) {
      materials.push_back(child.material);
      if (!digest(*child.material, resolvedFrame, uniforms, leaves, materials))
        return false;
    }
  }
  return true;
}

}  // namespace

std::shared_ptr<const Paint::Backed::Sources> PaintAccess::sampledSources(
    const Paint::Backed& backed) {
  const std::lock_guard lock(backed.sourceMutex);
  if (!backed.sources || backed.sources->description != backed.material) {
    std::vector<CapturedLeaf> previous;
    if (backed.sources) previous = backed.sources->leaves;
    std::vector<CapturedLeaf> leaves;
    auto captured = captureSources(backed.material, leaves, previous);
    backed.sources = std::make_shared<const Paint::Backed::Sources>(
        Paint::Backed::Sources{backed.material, std::move(captured.sampled),
                               captured.usesRecorder, std::move(leaves)});
  }
  return backed.sources;
}

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected frame inputs — only when the effect
// actually declares them (assigning a missing uniform aborts in debug builds).
// `paintFrame` is null for the static build.
sk_sp<SkShader> PaintAccess::build(const Paint::Live& live,
                                   const PaintFrame* paintFrame,
                                   bool worldSpace,
                                   std::span<const ChildOverride> children) {
  const sk_sp<SkRuntimeEffect>& effect = live.effect;
  if (!effect) return nullptr;
  const bool anchorHere = worldSpace && !live.childAnchorsToRoot;
  PaintFrame rootFrame;
  if (anchorHere && paintFrame) {
    rootFrame = rootSamplingFrame(*paintFrame);
    paintFrame = &rootFrame;
  }
  // A user-provided uniform (constant or bound) OWNS its slot, and the
  // auto-injects below must never overwrite it. Binding uTime to a
  // caller-driven Output is a supported way to control a material's clock,
  // and an injection that ran afterwards would overwrite that value with
  // the context's own elapsed time without saying anything.
  const bool injectTime = paintFrame &&
                          validUniform(effect, "uTime", UniformType::kFloat) &&
                          !live.ownsUniform("uTime");
  const bool injectScale =
      paintFrame &&
      validUniform(effect, "uContentScale", UniformType::kFloat) &&
      !live.ownsUniform("uContentScale");
  // Said before the memo is consulted: a remembered shader was built from
  // the scale exactly as a fresh one is.
  if (injectScale && paintFrame->contentScaleRead)
    *paintFrame->contentScaleRead = true;
  const bool injectResolution =
      paintFrame && validUniform(effect, "uResolution", UniformType::kFloat2) &&
      !live.ownsUniform("uResolution");
  const bool injectWorld =
      paintFrame && validWorldUniform(effect) && !live.ownsUniform("uWorld");
  const bool injectSampling =
      validUniform(effect, "uLocalToSample", UniformType::kFloat3x3) &&
      !live.ownsUniform("uLocalToSample");

  // A world-space material's uResolution is the ROOT canvas size, not the
  // node's box: the shader is already sampling in root coordinates, so
  // dividing by the node's size would rescale the field per node and the
  // two siblings that were meant to share one continuous field would each
  // get their own.
  const SkSize resolutionSize =
      anchorHere && paintFrame && !paintFrame->rootSize.isEmpty()
          ? paintFrame->rootSize
          : (paintFrame ? paintFrame->size : SkSize::MakeEmpty());

  // Resolve the authored panorama once before the parent memo. Its source
  // and recorder may change without changing the surface's scalar inputs.
  sk_sp<SkShader> environmentSource;
  sk_sp<SkShader> environmentFiltered;
  bool validEnvironmentDomain = true;
  if (live.environment) {
    const SkSize domain = environmentDomain(live);
    validEnvironmentDomain = std::isfinite(domain.width()) &&
                             std::isfinite(domain.height()) &&
                             domain.width() >= 0 && domain.height() >= 0;
    for (const auto& [name, child] : live.slots)
      if (name == "uEnvironment") {
        environmentSource = detail::childShader(child, paintFrame);
        break;
      }
    if (paintFrame)
      environmentFiltered = live.environment->shader(environmentSource, domain,
                                                     paintFrame->recorder);
  }

  // The varying-input digest (constants are fixed per recipe; injected
  // values participate only when actually injected).
  Paint::Live::Inputs key;
  std::vector<float>& inputs = key.uniforms;
  bool recorderSensitive = false;
  const bool memoEnabled = paintFrame && children.empty();
  if (memoEnabled)
    for (const auto& [name, child] : live.slots)
      recorderSensitive |= usesRecorder(child);
  const size_t memoIndex =
      recorderSensitive && paintFrame && paintFrame->recorder ? 1 : 0;
  if (memoEnabled) {
    if (recorderSensitive) {
      key.recorder = paintFrame->recorder;
      key.recorderId = recorderId(key.recorder);
    }
    if (live.environment) {
      const uint64_t identity =
          reinterpret_cast<uintptr_t>(environmentFiltered.get());
      inputs.push_back(float(identity & 0xffffffu));
      inputs.push_back(float((identity >> 24u) & 0xffffffu));
      inputs.push_back(float((identity >> 48u) & 0xffffu));
    }
    inputs.reserve(4 * live.binds.size() + 2 * live.blocks.size() + 14);
    // Copies share this memo but can wrap coordinates differently with
    // identical uniform uploads and placement.
    inputs.push_back(worldSpace ? 1.0f : 0.0f);
    for (const auto& [name, out] : live.binds)
      std::visit(
          [&](const auto& source) {
            const auto value = source.value();
            if constexpr (std::is_same_v<std::remove_cv_t<decltype(value)>,
                                         Color>)
              inputs.insert(inputs.end(), {value.r, value.g, value.b, value.a});
            else
              inputs.push_back(value);
          },
          out);
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
    if (worldSpace || injectWorld) digestToRoot(inputs, paintFrame->toRoot);
    if (injectSampling) digestToRoot(inputs, paintFrame->localToSample);
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
    // frame it was first resolved. Static slots (an image, a ramp) are
    // recipe and never vary, so they leave the memo intact.
    bool childNeedsCtx = false;
    for (const auto& [name, child] : live.slots)
      childNeedsCtx |= child.isRunning() || child.geometryDependent();
    if (!childNeedsCtx)
      if (sk_sp<SkShader> memoised = live.memo[memoIndex].hit(key))
        return memoised;
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
    static std::atomic_bool warnedPad{false};
    if (!warnedPad.load(std::memory_order_relaxed) &&
        validUniform(effect, "uPad", UniformType::kFloat) &&
        validUniform(effect, "uGlowR", UniformType::kFloat)) {
      const float halfMin =
          0.5f * std::min(paintFrame->size.width(), paintFrame->size.height());
      for (const auto& [name, value] : live.constants)
        if (name == "uPad" && value >= halfMin) {
          if (warnedPad.exchange(true, std::memory_order_relaxed)) break;
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
    std::visit(
        [&](const auto& source) {
          const auto value = source.value();
          if constexpr (std::is_same_v<std::remove_cv_t<decltype(value)>,
                                       Color>)
            b.uniform(name) =
                std::array<float, 4>{value.r, value.g, value.b, value.a};
          else
            b.uniform(name) = value;
        },
        out);
  for (const auto& [name, block] : live.blocks)
    if (block)  // floating type and complete count validated at store
      b.uniform(name).set(block->committedValues().data(), (int)block->size());
  // Children resolve with the SAME PaintFrame the parent got (so a live
  // child ticks and a geometry child reads the parent node's box), and with
  // the null context on the static snapshot path.
  for (const auto& [name, child] : live.slots) {
    const auto replacement =
        std::find_if(children.begin(), children.end(),
                     [&](const auto& input) { return input.name == name; });
    b.child(name) = replacement != children.end() ? replacement->shader
                    : live.environment && name == "uEnvironment"
                        ? environmentSource
                        : detail::childShader(child, paintFrame);
  }
  if (live.environment) {
    b.child("uEnvironmentFiltered") = environmentFiltered;
    b.uniform("uHasEnvironmentFiltered") = environmentFiltered ? 1.0f : 0.0f;
    if (!validEnvironmentDomain || (paintFrame && !environmentFiltered))
      b.uniform("uHasEnvironment") = 0.0f;
  }
  if (injectSampling) {
    const glm::mat3 metric =
        paintFrame ? toMatrix(paintFrame->localToSample) : glm::mat3{1};
    b.uniform("uLocalToSample").set(&metric[0][0], 9);
  }
  if (paintFrame) {
    // Frame inputs require their exact floating declarations; equal-sized
    // integers and arrays do not receive scalar or vector frame bytes.
    if (injectTime) {
      b.uniform("uTime") = (float)sigil::motion::quantizeTime(
          paintFrame->seconds, (double)live.timeQuantizeHz);
    }
    if (injectScale) b.uniform("uContentScale") = paintFrame->contentScale;
    if (injectResolution)
      b.uniform("uResolution") =
          std::array<float, 2>{resolutionSize.width(), resolutionSize.height()};
    if (injectWorld) {
      std::array<float, 16> world;
      SkM44(paintFrame->toRoot).getColMajor(world.data());
      b.uniform("uWorld").set(world.data(), (int)world.size());
    }
  }
  sk_sp<SkShader> built = b.makeShader();
  // Wrap BEFORE the memo stores. A held world-space field then keeps one
  // stable shader pointer across frames, and shader-pointer stability is
  // exactly what the painter's live-material memo compares to decide a
  // recording can replay. Wrapping after the store would mint a fresh
  // wrapper per resolve and the material would read as never holding still.
  if (anchorHere && paintFrame)
    built = anchorToRoot(std::move(built), *paintFrame);
  if (memoEnabled) live.memo[memoIndex].store(std::move(key), built);
  return built;
}

sk_sp<SkShader> PaintAccess::buildBacked(const Paint& self,
                                         const PaintFrame* paintFrame) {
  const Paint::Backed& backed = *self.m_backed;
  // A PASS BODY HAS NO STANDALONE SHADER. It is written against the
  // declarations the fx() runtime prepends once it knows the track's unit
  // count, so compiling it here would report one error per mention of a
  // name that does not exist yet — a page of diagnostics about a compile
  // nobody asked for, on a material that then renders correctly through
  // resolvePass(). Nothing is lost: a pass material used as an ordinary
  // fill has no picture to give, and now it says so by drawing nothing
  // rather than by failing loudly at load.
  if (detail::isPassBody(backed.material.recipe())) return nullptr;
  detail::ensureCompiler();
  FrameData frame = paintFrame ? frameDataOf(*paintFrame) : FrameData{};
  Paint::Backed::Inputs key;
  bool memoizable = false;
  const auto sources = sampledSources(backed);
  const bool recorderSensitive = sources->usesRecorder;
  const size_t memoIndex =
      recorderSensitive && paintFrame && paintFrame->recorder ? 1 : 0;
  if (paintFrame) {
    if (recorderSensitive) {
      key.recorder = paintFrame->recorder;
      key.recorderId = recorderId(key.recorder);
    }
    // An sdf style reserves its glow, shadow and border padding INSIDE the
    // box, so a generous glow on a modest box leaves almost no interior and
    // the shape all but disappears — with no error anywhere. The two
    // numbers that decide it only meet here, once per process.
    const sigil::material::Recipe& r = backed.material.recipe();
    if (r.reads(sigil::material::FrameInput::Resolution) &&
        r.name().rfind("sdf.", 0) == 0 && paintFrame->size.width() > 0 &&
        paintFrame->size.height() > 0) {
      static std::atomic_bool warnedPad{false};
      const float pad = backed.material.get<float>("uPad");
      const float halfMin =
          0.5f * std::min(paintFrame->size.width(), paintFrame->size.height());
      if (pad >= halfMin &&
          !warnedPad.exchange(true, std::memory_order_relaxed)) {
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
  }
  if (self.m_worldSpace && paintFrame) frame = rootSamplingFrame(frame);
  // A static device source's eager raster snapshot is also its first
  // framed raster entry. Framed resolution still binds each new recorder.
  if (paintFrame || (!self.isRunning() && !self.geometryDependent())) {
    memoizable = digest(backed.material, frame, key.uniforms, key.leaves,
                        key.materials, false);
    if (memoizable && self.usesWorldSpace() && paintFrame) {
      std::vector<float> w;
      digestToRoot(w, paintFrame->toRoot);
      const auto* bytes = reinterpret_cast<const std::byte*>(w.data());
      key.uniforms.insert(key.uniforms.end(), bytes,
                          bytes + w.size() * sizeof(float));
    }
    if (memoizable)
      if (sk_sp<SkShader> memoised = backed.memo[memoIndex].hit(key))
        return memoised;
  }
  Material sampled = sources->sampled;
  // Paint owns the outer anchoring; nested materials keep their own flags.
  sampled.worldSpace(false);
  sk_sp<SkShader> built = sigil::material::skia::shader(sampled, frame);
  if (self.m_worldSpace && paintFrame)
    built = anchorToRoot(std::move(built), *paintFrame);
  if (memoizable) backed.memo[memoIndex].store(std::move(key), built);
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
sk_sp<SkShader> PaintAccess::pannedImageShader(const Paint& self) {
  if (!self.m_recipe) return nullptr;
  sk_sp<SkImage> img = self.m_recipe->image;
  if (!img) return nullptr;
  SkMatrix local = self.m_recipe->local;
  const glm::vec2 pan = self.boundOffsetValue();
  local.postTranslate(pan.x, pan.y);
  return SkShaders::Image(std::move(img), self.m_recipe->tx, self.m_recipe->ty,
                          self.m_recipe->sampling, &local);
}

/** The FITTED build, the box's answer to the pan's: the source mapped
 *  onto @p frame's box by the stated rule, then post-translated by any
 *  bound pan exactly as `pannedImageShader` translates the static matrix.
 *  Null when there is no fit, no source, or no box to fit to — the caller
 *  falls through to the unfitted form, which is the same degradation a
 *  world-space material makes outside a composer. */
sk_sp<SkShader> PaintAccess::fittedImageShader(const Paint& self,
                                               const PaintFrame& frame) {
  if (!self.hasFit() || frame.size.isEmpty()) return nullptr;
  sk_sp<SkImage> img = self.m_recipe->image;
  if (!img || img->width() <= 0 || img->height() <= 0) return nullptr;
  const float iw = (float)img->width(), ih = (float)img->height();
  const float bw = frame.size.width(), bh = frame.size.height();
  const float sx = bw / iw, sy = bh / ih;
  SkMatrix local;
  switch (self.m_recipe->fit) {
    case Fit::Stretch:
      local = SkMatrix::Scale(sx, sy);
      break;
    case Fit::Cover:
    case Fit::Contain: {
      // One scale for both axes; which one is the whole difference
      // between covering the box and sitting inside it. Centred either
      // way, so the crop takes the same from both edges and the margin
      // leaves the same at both.
      const float k = self.m_recipe->fit == Fit::Cover ? std::max(sx, sy)
                                                       : std::min(sx, sy);
      local = SkMatrix::Scale(k, k);
      local.postTranslate((bw - iw * k) * 0.5f, (bh - ih * k) * 0.5f);
      break;
    }
    case Fit::Native:
      return nullptr;
  }
  const glm::vec2 pan = self.boundOffsetValue();
  local.postTranslate(pan.x, pan.y);
  return SkShaders::Image(std::move(img), self.m_recipe->tx, self.m_recipe->ty,
                          self.m_recipe->sampling, &local);
}

sk_sp<SkShader> PaintAccess::resolvePass(const Paint& self,
                                         const PassInputs& in,
                                         const PaintFrame& paintFrame) {
  if (!self.m_backed) return nullptr;
  const Paint::Backed& backed = *self.m_backed;
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
  const FrameData frame = frameDataOf(
      self.m_worldSpace ? rootSamplingFrame(paintFrame) : paintFrame);
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
  if (self.m_worldSpace) built = anchorToRoot(std::move(built), paintFrame);
  return built;
}

}  // namespace sigil::material::skia
