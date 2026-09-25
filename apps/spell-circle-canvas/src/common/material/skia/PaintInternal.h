#pragma once

/** @file
 * WHAT A PAINT'S OWN TRANSLATION UNITS SHARE: the three states a paint
 * can hold — the sksl recipe, the comparable build recipe, the material
 * instance — the memo each of them resolves through, the guard every
 * uniform door asks before it stores a name, and the root anchoring every
 * resolve path performs.
 *
 * Private to the paint: a consumer reaches these states through the paint
 * class, which is why they are opaque in its header.
 */

#include <include/core/SkImage.h>  // the recipe holds one
#include <include/core/SkMatrix.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmaterial/core/FrameData.h>
#include <sigilmotion/values/Animated.h>
#include <sigilmotion/values/Time.h>
#include <sigilmaterial/skia/Paint.h>

#include <array>
#include <functional>
#include <span>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PaintDetail.h"

namespace sigil::material::skia {

/** THE RESOLVE MEMO, which two threads may reach at once. Copies of one
 *  Paint share the state a memo hangs off, and a host may paint two
 *  composers on two threads, so the key and the shader are read and
 *  written under a lock. A copy starts empty: the memo is derived from
 *  the values it was built against and rebuilds itself on the first
 *  resolve, and copying a lock would be copying a claim on nothing. */
template <class Key>
struct ResolveMemo {
  mutable std::mutex mutex;
  Key key;
  sk_sp<SkShader> shader;

  ResolveMemo() = default;
  ResolveMemo(const ResolveMemo&) {}
  ResolveMemo& operator=(const ResolveMemo&) { return *this; }

  /** The memoised shader when @p against is the key it was built with. */
  sk_sp<SkShader> hit(const Key& against) const {
    const std::lock_guard lock(mutex);
    return shader && key == against ? shader : nullptr;
  }
  void store(Key taken, sk_sp<SkShader> built) {
    const std::lock_guard lock(mutex);
    key = std::move(taken);
    shader = std::move(built);
  }
};

/** ONE ENTRY PER NAME, in every uniform lane: setting a uniform twice
 *  replaces the first value rather than stacking two the builder would
 *  both assign, which grew the lane without bound and made a paint set
 *  twice compare unequal to the same paint set once. `slot()` follows
 *  the same rule for the same reason. */
template <class Value>
void putByName(std::vector<std::pair<std::string, Value>>& lane,
               std::string name, Value value) {
  for (auto& entry : lane)
    if (entry.first == name) {
      entry.second = std::move(value);
      return;
    }
  lane.emplace_back(std::move(name), std::move(value));
}

}  // namespace sigil::material::skia

namespace sigil::material {

/** The sksl recipe behind a Paint (opaque in the header). */
struct Paint::Live {
  sk_sp<SkRuntimeEffect> effect;
  std::vector<std::pair<std::string, float>> constants;
  std::vector<std::pair<std::string, std::array<float, 2>>> constants2;
  std::vector<std::pair<std::string, std::array<float, 4>>> constants4;
  // Constant arrays, stored flat; validated at store by total float count.
  std::vector<std::pair<std::string, std::vector<float>>> constantArrays;
  std::vector<std::pair<std::string, motion::Animatable<float>>> binds;
  // Live arrays: caller-owned blocks, read per resolve. Any entry makes
  // the material LIVE, like a bind; the resolve memo digests revisions.
  std::vector<
      std::pair<std::string, std::shared_ptr<const material::UniformBlock>>>
      blocks;
  // slot(): `uniform shader NAME` slots, filled with whole Materials.
  // They are recipe (they participate in equality) AND volatility: the
  // parent inherits each child's tier, and resolves every child with the
  // same PaintFrame it got itself.
  std::vector<std::pair<std::string, Paint>> slots;
  // Which context inputs this effect declares, and therefore which tier the
  // material sits in:
  //  - usesTime / usesScale: uTime changes every frame and uContentScale
  //    changes with the HOST's canvas scale (zoom). Neither is a function of
  //    the node, so either makes the material LIVE — resolved per frame.
  //  - usesGeometry: uResolution is the node's laid-out size, stable between
  //    layouts. Such a material resolves when the node RECORDS, caches with
  //    that recording, and re-records when layout changes the node's size.
  bool usesTime = false;
  bool usesScale = false;
  bool usesGeometry = false;
  // quantizeTime(): step the injected uTime at this rate (0 = continuous),
  // so that a material meant to read as a slow sequence of held states
  // actually holds — and, since consecutive frames then produce identical
  // inputs, the memo below returns the same shader and the node's recording
  // survives.
  float timeQuantizeHz = 0;
  // Resolve memo: when every varying input (bound Outputs + injected
  // time/scale/resolution) is byte-identical to the previous build, the
  // previous shader is returned — quantized time makes consecutive frames
  // identical, and the paint layer turns that stability into replayed
  // pictures instead of re-rasterized shaders.
  mutable skia::ResolveMemo<std::vector<float>> memo;
};

/** The SigilMaterial instance behind a recipe() material, with the memo
 *  that keeps a settled tree's shader pointer stable: when every resolved
 *  byte of the tree (and W, when anchored) is the previous resolve's, the
 *  previous shader comes back, and the paint layer turns that into a
 *  replayed recording. */
struct Paint::Backed {
  Material material;
  mutable skia::ResolveMemo<std::vector<std::byte>> memo;
};

/** The comparable build recipe behind gradient/image/blend materials: the
 *  structural signature that lets two independently built materials compare
 *  equal. Without it every re-describe would mint a fresh SkShader whose
 *  pointer differs, nothing would ever prune, and a static gradient would
 *  re-record forever. Solid, raw-shader and sksl materials compare through
 *  Paint's own state instead. */
struct Paint::Recipe {
  enum class Kind : uint8_t {
    Linear,
    Radial,
    Conical,
    Sweep,
    Image,
    Blend,
    Buffer
  };
  Kind kind = Kind::Linear;
  // Gradients: endpoints/center + radius or sweep degrees + ramp.
  SkPoint p0 = {0, 0}, p1 = {0, 0};  // Conical: (focus, center)
  float f0 = 0.0f, f1 = 0.0f;        // radius / (startDeg, endDeg) /
                                     // Conical: (focusRadius, radius)
  material::ColorStops stops;
  SkTileMode tile = SkTileMode::kClamp;
  // Image: pointer identity + mapping.
  sk_sp<SkImage> image;
  SkTileMode tx = SkTileMode::kClamp, ty = SkTileMode::kClamp;
  SkMatrix local = SkMatrix::I();
  SkSamplingOptions sampling;
  Fit fit = Fit::Native;
  // Blend: layer materials (recursive Paint equality) + modes.
  std::vector<std::pair<Paint, BlendMode>> layers;
  // Buffer: a caller-owned mutable bitmap, so identity alone cannot decide
  // equality — the revision counter is what makes a committed edit visible
  // to the prune.
  std::shared_ptr<skia::PixelBuffer> source;
  uint64_t revision = 0;

  bool operator==(const Recipe& o) const {
    if (kind != o.kind) return false;
    switch (kind) {
      case Kind::Linear:
      case Kind::Radial:
      case Kind::Conical:
      case Kind::Sweep:
        return p0 == o.p0 && p1 == o.p1 && f0 == o.f0 && f1 == o.f1 &&
               stops == o.stops && tile == o.tile;
      case Kind::Image:
        return image == o.image && tx == o.tx && ty == o.ty &&
               local == o.local && sampling == o.sampling && fit == o.fit;
      case Kind::Blend:
        return layers == o.layers;
      case Kind::Buffer:
        return source == o.source && revision == o.revision && tx == o.tx &&
               ty == o.ty && local == o.local && sampling == o.sampling &&
               fit == o.fit;
    }
    return false;
  }
};

/** The static resolution a paint holds (opaque in the header). */
struct Paint::Snapshot {
  sk_sp<SkShader> shader;
};

}  // namespace sigil::material

namespace sigil::material::skia {


/** THE EXECUTOR'S VIEW OF A PAINT: the resolve paths that build Skia
 *  objects from the paint's private state. A friend of the paint, so the
 *  value's header names no renderer type. */
struct PaintAccess {
  /** The snapshot @p paint holds, or null. */
  static const sk_sp<SkShader>& snapshot(const Paint& paint) {
    static const sk_sp<SkShader> none;
    return paint.m_shader ? paint.m_shader->shader : none;
  }
  /** @p shader held as a paint's snapshot. */
  static std::shared_ptr<const Paint::Snapshot> hold(sk_sp<SkShader> shader) {
    if (!shader) return nullptr;
    return std::make_shared<const Paint::Snapshot>(
        Paint::Snapshot{std::move(shader)});
  }
  static Paint wrap(sk_sp<SkShader> shader) {
    Paint m;
    m.m_shader = hold(std::move(shader));
    return m;
  }
  static Paint image(sk_sp<SkImage> image, SkTileMode horizontal,
                     SkTileMode vertical, const SkMatrix& local,
                     SkSamplingOptions sampling);
  static Paint buffer(std::shared_ptr<PixelBuffer> source, SkTileMode horizontal,
                      SkTileMode vertical, const SkMatrix& local,
                      SkSamplingOptions sampling);
  static Paint sksl(sk_sp<SkRuntimeEffect> effect,
                    std::vector<std::pair<std::string, float>> constants);
  static void refresh(Paint& paint) {
    paint.m_shader = hold(build(*paint.m_live, nullptr));
  }

  /** @p worldSpace routes root anchoring through this ONE build: the
   *  digest of varying inputs gains W's six floats, uResolution becomes
   *  the root canvas size, and the shader is wrapped in W⁻¹ before the
   *  memo stores it. */
  static sk_sp<SkShader> build(const Paint::Live& live, const PaintFrame* frame,
                               bool worldSpace = false);
  /** Fold a Blend recipe's layers into one shader — `frame` null is the
   *  frameless form (the snapshot), non-null the per-draw one. One
   *  function so the two can never disagree. */
  static sk_sp<SkShader> foldBlend(const Paint& self, const PaintFrame* frame);
  /** THE FOLD ITSELF, which blend()'s eager flatten and foldBlend's
   *  deferred one both are: each layer after the first composited over
   *  the accumulation with its mode, then mixed back toward it by its
   *  `amount`. @p shaderOf resolves ONE layer, which is the whole of
   *  what the two callers differ by. */
  static sk_sp<SkShader> foldLayers(
      std::span<const std::pair<Paint, BlendMode>> layers,
      const std::function<sk_sp<SkShader>(const Paint&)>& shaderOf);
  /** The image shader rebuilt with the bound pan's CURRENT values
   *  post-translated onto the recipe matrix — one construction shared by
   *  both resolves, so a bound-offset paint cannot look different
   *  depending on which asked. */
  static sk_sp<SkShader> pannedImageShader(const Paint& self);
  static sk_sp<SkShader> fittedImageShader(const Paint& self,
                                           const PaintFrame& frame);
  /** The recipe-backed resolve: @p frame (null is the static snapshot),
   *  the tree's resolved bytes as the memo key, the world-space wrap
   *  inside the memo. */
  static sk_sp<SkShader> buildBacked(const Paint& self, const PaintFrame* frame);
  static sk_sp<SkShader> asShader(const Paint& self);
  static sk_sp<SkShader> shaderFor(const Paint& self, const PaintFrame& frame);
  static sk_sp<SkShader> resolvePass(const Paint& self, const PassInputs& in,
                                     const PaintFrame& frame);
  static const Paint::Live* live(const Paint& paint) {
    return paint.m_live.get();
  }
};

// A named uniform is usable iff the effect declares it at the expected size —
// assigning an undeclared or mis-sized uniform SkDEBUGFAILs (aborts debug
// builds), which would let one typo kill the hot-reload host.
// Unknown names warn-and-ignore instead (validated at sksl()/uniform() time,
// so build() never touches an invalid entry). The rule itself lives in
// detail:: because Effect's uniform doors answer to it too.
inline bool validUniform(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name, size_t bytes) {
  return detail::declaresUniform(effect, name, bytes);
}

inline void warnUnknownUniform(const char* what, const std::string& name) {
  SkDebugf(
      "skia::Paint::%s: uniform \"%s\" is not declared by the effect at "
      "this value's size — ignored (an array must supply the declared "
      "total float count exactly)\n",
      what, name.c_str());
}

/** World-space anchoring, the ONE construction every resolve path
 *  shares: the built shader is sampled through the inverse of the node's
 *  node-to-root matrix W, so a local drawing coordinate p evaluates the
 *  field at W·p — the root-frame point the node actually occupies. That is
 *  what lets two separate nodes sample one continuous field.
 *
 *  An identity W — outside a composer, or a root-level node with no
 *  transform — wraps nothing, because node-local and root-local are then
 *  the same frame. */
inline sk_sp<SkShader> anchorToRoot(sk_sp<SkShader> s,
                                    const PaintFrame& paintFrame) {
  if (!s || paintFrame.toRoot.isIdentity()) return s;
  SkMatrix inv;
  if (!paintFrame.toRoot.invert(&inv))
    return s;  // degenerate transform: nothing sensible to anchor through
  return s->makeWithLocalMatrix(inv);
}
/** W's affine six, fed into the varying-input digest a build memoises on.
 *  A digest cannot see an input it was never given: leave these out and
 *  the frame after a world-space node moves compares equal to the frame
 *  before it, and the memo hands back a shader anchored to the old
 *  position. */
inline void digestToRoot(std::vector<float>& inputs, const SkMatrix& w) {
  inputs.push_back(w.getScaleX());
  inputs.push_back(w.getSkewX());
  inputs.push_back(w.getTranslateX());
  inputs.push_back(w.getSkewY());
  inputs.push_back(w.getScaleY());
  inputs.push_back(w.getTranslateY());
}

}  // namespace sigil::material::skia
