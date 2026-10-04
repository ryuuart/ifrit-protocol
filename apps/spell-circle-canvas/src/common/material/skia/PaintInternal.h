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
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Time.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "PaintDetail.h"

namespace sigil::material::skia {

class EnvironmentPreparation;

struct CapturedLeaf {
  std::shared_ptr<const Leaf> description;
  std::shared_ptr<const Leaf> sampled;
  bool usesRecorder = false;
};

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
  void clear() {
    const std::lock_guard lock(mutex);
    key = {};
    shader.reset();
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
  using Binding =
      std::variant<motion::Animatable<float>, motion::Animatable<Color>>;
  std::vector<std::pair<std::string, Binding>> binds;
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
  // A coordinate-preserving wrapper leaves its sole child's node frame and
  // root anchoring intact instead of establishing another sampling boundary.
  bool childAnchorsToRoot = false;
  // Prepared lighting keeps the authored panorama in its ordinary slot;
  // framed resolution replaces only the private filtered child.
  std::shared_ptr<skia::EnvironmentPreparation> environment;
  /** An authored input owns its uniform instead of a frame injection. */
  bool ownsUniform(std::string_view name) const {
    for (const auto& [uniform, value] : constants)
      if (uniform == name) return true;
    for (const auto& [uniform, value] : constants2)
      if (uniform == name) return true;
    for (const auto& [uniform, value] : constants4)
      if (uniform == name) return true;
    for (const auto& [uniform, value] : constantArrays)
      if (uniform == name) return true;
    for (const auto& [uniform, value] : binds)
      if (uniform == name) return true;
    for (const auto& [uniform, value] : blocks)
      if (uniform == name) return true;
    return false;
  }
  // Which context inputs this effect declares, and therefore which tier the
  // material sits in:
  //  - usesTime: uTime changes every frame, so the material is LIVE —
  //    resolved per frame.
  //  - usesGeometry / usesWorld / usesSampling: size, root placement and
  //    pixel sampling steps are layout-derived. A change re-records the node.
  //  - usesScale: uContentScale is the destination's device scale. It holds
  //    still between frames and re-records the node when it changes.
  bool usesTime = false;
  bool usesScale = false;
  bool usesGeometry = false;
  bool usesWorld = false;
  bool usesSampling = false;
  // quantizeTime(): step the injected uTime at this rate (0 = continuous),
  // so that a material meant to read as a slow sequence of held states
  // actually holds — and, since consecutive frames then produce identical
  // inputs, the memo below returns the same shader and the node's recording
  // survives.
  float timeQuantizeHz = 0;
  // Resolve memo: when the sampling mode and every varying input (bound
  // Outputs + injected time/scale/resolution/placement) matches the previous
  // build, the previous shader is returned — quantized time makes consecutive
  // frames identical, and the paint layer turns that stability into replayed
  // pictures instead of re-rasterized shaders. Raster and the most recent
  // recorder retain separate entries.
  struct Inputs {
    std::vector<float> uniforms;
    skgpu::graphite::Recorder* recorder = nullptr;
    uint32_t recorderId = 0;
    bool operator==(const Inputs&) const = default;
  };
  mutable std::array<skia::ResolveMemo<Inputs>, 2> memo;
};

/** The SigilMaterial instance behind a recipe() material, with the memo
 *  that keeps a settled tree's shader pointer stable: when every resolved
 *  byte of the tree (and W, when anchored) is the previous resolve's, the
 *  previous shader comes back, and the paint layer turns that into a
 *  replayed recording. Sampled source handles and recorder identity keep
 *  replacements and device bindings distinct. */
struct Paint::Backed {
  explicit Backed(Material source) : material(std::move(source)) {}
  Backed(const Backed& other) : material(other.material) {
    const std::lock_guard lock(other.sourceMutex);
    sources = other.sources;
  }
  Material material;
  struct Sources {
    Material description;
    Material sampled;
    bool usesRecorder = false;
    std::vector<skia::CapturedLeaf> leaves;
  };
  mutable std::mutex sourceMutex;
  mutable std::shared_ptr<const Sources> sources;
  struct Inputs {
    std::vector<std::byte> uniforms;
    std::vector<std::shared_ptr<const Leaf>> leaves;
    std::vector<std::shared_ptr<const Material>> materials;
    skgpu::graphite::Recorder* recorder = nullptr;
    uint32_t recorderId = 0;
    bool operator==(const Inputs&) const = default;
  };
  mutable std::array<skia::ResolveMemo<Inputs>, 2> memo;
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
  // Image and Buffer: captured pixels and mapping. A buffer's equality
  // uses its source and captured revision rather than image identity.
  sk_sp<SkImage> image;
  SkTileMode tx = SkTileMode::kClamp, ty = SkTileMode::kClamp;
  SkMatrix local = SkMatrix::I();
  SkSamplingOptions sampling;
  Fit fit = Fit::Native;
  // Blend: layer materials (recursive Paint equality) + modes.
  std::vector<std::pair<Paint, BlendMode>> layers;
  // Solids have no retained shader of their own. Keep their framed child
  // shaders stable without changing the layer descriptors used by equality.
  std::vector<sk_sp<SkShader>> solidLayers;
  struct BlendInputs {
    std::vector<sk_sp<SkShader>> shaders;
    SkMatrix toRoot = SkMatrix::I();
    bool worldSpace = false;

    bool operator==(const BlendInputs&) const = default;
  };
  mutable skia::ResolveMemo<BlendInputs> blendMemo;
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
  static Paint buffer(std::shared_ptr<PixelBuffer> source,
                      SkTileMode horizontal, SkTileMode vertical,
                      const SkMatrix& local, SkSamplingOptions sampling);
  static Paint sksl(sk_sp<SkRuntimeEffect> effect,
                    std::vector<std::pair<std::string, float>> constants);
  /** Retained descriptions that resolve sources on their first draw. */
  static Paint prepareMaterial(const Material& material);
  /** Cut colour samples while retaining the paint's outer blend, cull and
   *  anchoring settings. */
  static Paint alphaCutout(Paint source, float cutoff, bool snapshot);
  static Paint unresolvedRecipe(Material material);
  static Paint unresolvedBlend(std::vector<std::pair<Paint, BlendMode>> layers);
  // Assemble an unpublished raw recipe without constructing intermediate
  // snapshots. The caller refreshes once after storing its complete inputs.
  static Paint unresolvedSksl(sk_sp<SkRuntimeEffect> effect);
  static bool storeUniform(Paint& paint, std::string name, float value);
  static bool storeUniform(Paint& paint, std::string name,
                           std::array<float, 2> value);
  static bool storeUniform(Paint& paint, std::string name,
                           std::array<float, 4> value);
  static bool storeUniform(Paint& paint, std::string name, Color value) {
    return storeUniform(
        paint, std::move(name),
        std::array<float, 4>{value.r, value.g, value.b, value.a});
  }
  static bool storeUniform(Paint& paint, std::string name,
                           std::vector<float> values);
  static bool storeBinding(Paint& paint, std::string name,
                           motion::Animatable<float> output);
  static bool storeBinding(Paint& paint, std::string name,
                           motion::Animatable<Color> output);
  static bool storeSlot(Paint& paint, std::string name, Paint source);
  static void refresh(Paint& paint) {
    paint.m_shader = hold(build(*paint.m_live, nullptr));
  }
  static void retainSnapshot(Paint& paint) {
    if (!paint.m_isSolid && !snapshot(paint))
      paint.m_shader = hold(asShader(paint));
  }
  static void retainSolidShader(Paint& paint) {
    if (paint.m_isSolid)
      paint.m_shader = hold(detail::childShader(paint, nullptr));
  }
  static void prepareEnvironment(
      Paint& paint, std::shared_ptr<EnvironmentPreparation> preparation) {
    paint.m_live->environment = std::move(preparation);
    paint.m_live->usesGeometry = true;
  }
  static std::shared_ptr<EnvironmentPreparation> environmentPreparation(
      const Paint& paint) {
    return paint.m_live ? paint.m_live->environment : nullptr;
  }
  static SkSize environmentDomain(const Paint::Live& live);
  /** Sampled sources may bind different images for each recorder. */
  static bool usesRecorder(const Paint& paint);
  static uint32_t recorderId(skgpu::graphite::Recorder* recorder);
  static std::shared_ptr<const Paint::Backed::Sources> sampledSources(
      const Paint::Backed& backed);

  /** @p worldSpace routes root anchoring through this ONE build: the
   *  digest of varying inputs gains W's six floats, uResolution becomes
   *  the root canvas size, and the shader is wrapped in W⁻¹ before the
   *  memo stores it. */
  struct ChildOverride {
    std::string_view name;
    sk_sp<SkShader> shader;
  };
  /** Per-draw child replacements skip the parent memo. Their shaders
   *  already resolved against the input frame and replace retained slots. */
  static sk_sp<SkShader> build(const Paint::Live& live, const PaintFrame* frame,
                               bool worldSpace = false,
                               std::span<const ChildOverride> children = {});
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
  static sk_sp<SkShader> buildBacked(const Paint& self,
                                     const PaintFrame* frame);
  static sk_sp<SkShader> asShader(const Paint& self);
  static sk_sp<SkShader> shaderFor(const Paint& self, const PaintFrame& frame);
  static sk_sp<SkShader> resolvePass(const Paint& self, const PassInputs& in,
                                     const PaintFrame& frame);
  static const Paint::Live* live(const Paint& paint) {
    return paint.m_live.get();
  }
};

/** The local-to-root frame input is one float4x4, not an array of floats. */
inline bool validWorldUniform(const sk_sp<SkRuntimeEffect>& effect) {
  return validUniform(effect, "uWorld", UniformType::kFloat4x4);
}

inline void warnUnknownUniform(const char* what, const std::string& name) {
  SkDebugf(
      "skia::Paint::%s: uniform \"%s\" does not match this value's "
      "floating type and complete float count — ignored\n",
      what, name.c_str());
}

/** The full local-to-root matrix, fed into the varying-input digest.
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
  inputs.push_back(w.getPerspX());
  inputs.push_back(w.getPerspY());
  inputs.push_back(w.get(SkMatrix::kMPersp2));
}

}  // namespace sigil::material::skia
