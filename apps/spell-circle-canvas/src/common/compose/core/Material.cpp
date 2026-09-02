/** @file
 * Material — construction of the polymorphic paint value. Static materials
 * (solid/gradient/image/blend/const-uniform sksl) resolve eagerly to a color
 * or shader and collapse to a Fill (toFill()), riding the kernel's existing
 * fill path. A LIVE material (sksl with a ch::Output-bound uniform) keeps its
 * runtime-effect recipe and rebuilds its shader every frame from the current
 * uniform values (resolve()); Element::fill routes it to the live path and the
 * painter re-resolves it while its node stays volatile. A RECIPE-BACKED
 * material holds a SigilMaterial instance and resolves it through that
 * library's program cache with the frame built from the PaintContext,
 * memoised on the tree's resolved bytes. See Material.h.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Material.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilmaterial/texture/ShaderLeaf.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <mutex>

#include "ComposeInternal.h"  // ElementNode, for Element::fill(Material)

namespace sigil::compose {

/** The sksl recipe behind a Material (opaque in the header). */
struct Material::Live {
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
  // child(): `uniform shader NAME` slots, filled with whole Materials.
  // They are recipe (they participate in equality) AND volatility: the
  // parent inherits each child's tier, and resolves every child with the
  // same PaintContext it got itself.
  std::vector<std::pair<std::string, Material>> children;
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
  mutable std::vector<float> lastInputs;
  mutable sk_sp<SkShader> lastShader;
};

/** The SigilMaterial instance behind a recipe() material, with the memo
 *  that keeps a settled tree's shader pointer stable: when every resolved
 *  byte of the tree (and W, when anchored) is the previous resolve's, the
 *  previous shader comes back, and the paint layer turns that into a
 *  replayed recording. */
struct Material::Backed {
  sigil::material::Material material;
  mutable std::vector<std::byte> lastKey;
  mutable sk_sp<SkShader> lastShader;
};

namespace {

/** A compose Material filling a slot of a recipe-backed one: the static
 *  snapshot of a native source (a gradient, an image), compared by
 *  compose's own equality so two equal descriptions prune. A child that
 *  needs a paint context to resolve is sampled once here, as its own
 *  documentation says a recipe slot does. */
class MaterialLeaf final : public sigil::material::ShaderLeaf {
 public:
  explicit MaterialLeaf(Material source) : m_source(std::move(source)) {}
  sk_sp<SkShader> shader() const override { return m_source.asShader(); }
  bool animated() const override { return m_source.isAnimated(); }

 protected:
  bool equals(const sigil::material::Leaf& other) const override {
    return m_source == static_cast<const MaterialLeaf&>(other).m_source;
  }

 private:
  Material m_source;
};

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

/** The comparable build recipe behind gradient/image/blend materials: the
 *  structural signature that lets two independently built materials compare
 *  equal. Without it every re-describe would mint a fresh SkShader whose
 *  pointer differs, nothing would ever prune, and a static gradient would
 *  re-record forever. Solid, raw-shader and sksl materials compare through
 *  Material's own state instead. */
struct Material::Recipe {
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
  std::vector<Stop> stops;
  SkTileMode tile = SkTileMode::kClamp;
  // Image: pointer identity + mapping.
  sk_sp<SkImage> image;
  SkTileMode tx = SkTileMode::kClamp, ty = SkTileMode::kClamp;
  SkMatrix local = SkMatrix::I();
  SkSamplingOptions sampling;
  // Blend: layer materials (recursive Material equality) + modes.
  std::vector<std::pair<Material, SkBlendMode>> layers;
  // Buffer: a caller-owned mutable bitmap, so identity alone cannot decide
  // equality — the revision counter is what makes a committed edit visible
  // to the prune.
  std::shared_ptr<PixelBuffer> source;
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
               local == o.local && sampling == o.sampling;
      case Kind::Blend:
        return layers == o.layers;
      case Kind::Buffer:
        return source == o.source && revision == o.revision && tx == o.tx &&
               ty == o.ty && local == o.local && sampling == o.sampling;
    }
    return false;
  }
};

namespace {

struct RampArrays {
  std::vector<SkColor4f> colors;
  std::vector<float> positions;  // empty → evenly spaced
};

RampArrays split(const std::vector<Stop>& stops) {
  RampArrays r;
  r.colors.reserve(stops.size());
  r.positions.reserve(stops.size());
  for (const Stop& s : stops) {
    r.colors.push_back(s.color);
    r.positions.push_back(s.pos);
  }
  return r;
}

// A named uniform is usable iff the effect declares it at the expected size —
// assigning an undeclared or mis-sized uniform SkDEBUGFAILs (aborts debug
// builds), which would let one typo kill the hot-reload host.
// Unknown names warn-and-ignore instead (validated at sksl()/uniform() time,
// so build() never touches an invalid entry). The rule itself lives in
// detail:: because Effect's uniform doors answer to it too.
bool validUniform(const sk_sp<SkRuntimeEffect>& effect, std::string_view name,
                  size_t bytes) {
  return detail::declaresUniform(effect, name, bytes);
}

void warnUnknownUniform(const char* what, const std::string& name) {
  SkDebugf(
      "Material::%s: uniform \"%s\" is not declared by the effect at "
      "this value's size — ignored (an array must supply the declared "
      "total float count exactly)\n",
      what, name.c_str());
}

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays& r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()},
                     tile},
                    {});
}

}  // namespace

namespace detail {

// A named child is usable iff the effect declares it as a SHADER child —
// assigning a missing child SkDEBUGFAILs exactly like a missing uniform.
// Shared with Effect::child: one validation behind both ways of filling a
// child slot, so the two cannot disagree about what is legal.
bool declaresShaderChild(const sk_sp<SkRuntimeEffect>& effect,
                         std::string_view name) {
  if (!effect) return false;
  const SkRuntimeEffect::Child* c = effect->findChild(name);
  return c && c->type == SkRuntimeEffect::ChildType::kShader;
}

// The uniform half of the same guardrail, shared with Effect::shader and
// Effect::uniform so the two doors that take an author's uniform name cannot
// disagree about what the effect will accept.
bool declaresUniform(const sk_sp<SkRuntimeEffect>& effect,
                     std::string_view name, size_t bytes) {
  if (!effect) return false;
  const SkRuntimeEffect::Uniform* u = effect->findUniform(name);
  return u && u->sizeInBytes() == bytes;
}

// THE ONE SPECIALIZATION behind every pass: the author's recipe with the
// runtime's declarations prepended to its SkSL body at the requested unit
// count, held per (recipe identity, count) for the process. The
// specialization carries the author's params layout, so the instance's
// values, bindings and children ride it unchanged; it is a second
// definition, so its program compiles and caches apart, which is what
// stops a count from meaning a compile per frame.
std::shared_ptr<const sigil::material::Recipe> passRecipeFor(
    const std::shared_ptr<const sigil::material::Recipe>& authored,
    uint32_t units) {
  if (!authored || !authored->has(sigil::material::Target::SkSL))
    return nullptr;
  // The authored recipe is held, not pointed at: a definition freed and a
  // second allocated at its address would otherwise inherit the first's
  // specializations. A recipe is defined once and held anyway.
  struct Cached {
    std::shared_ptr<const sigil::material::Recipe> authored;
    uint32_t units;
    std::shared_ptr<const sigil::material::Recipe> recipe;
  };
  static std::vector<Cached> cache;
  // Held under a lock because a host may paint two composers on two
  // threads, and both would reach here for the same definition.
  static std::mutex mutex;
  const std::lock_guard lock(mutex);
  const uint32_t n = std::max(units, 1u);
  for (const Cached& c : cache)
    if (c.units == n && c.authored == authored) return c.recipe;
  const std::string count = std::to_string(n);
  // The arrays and the loop bound sit at the head of the BODY rather than
  // among the declarations, because a params field is a value the author
  // sets and these three are the runtime's alone: nothing may write them
  // through the instance, and the count is a compile-time constant no
  // upload could carry.
  sigil::material::Recipe spec = *authored;
  spec.child("uContent");
  spec.body(sigil::material::Target::SkSL,
            "uniform float4 uUnitRect[" + count +
                "];\n"
                "uniform float2 uUnitPhase[" +
                count +
                "];\n"
                "const int kUnitCount = " +
                count + ";\n" + *authored->body(sigil::material::Target::SkSL));
  auto held = std::make_shared<const sigil::material::Recipe>(std::move(spec));
  cache.push_back({authored, n, held});
  return held;
}

// The Material→SkShader conversion every child slot performs (declared in
// Material.h): the per-frame resolve when there is a context, the
// context-free snapshot when there is not, a solid as a colour shader.
sk_sp<SkShader> childShader(const Material& source, const PaintContext* ctx) {
  if (!ctx)
    return source.asShader();  // already turns a solid into SkShaders::Color
  const Fill f = source.resolve(*ctx);
  if (f.kind == Fill::Kind::Shader) return f.shaderValue;
  if (f.kind == Fill::Kind::Color)
    return SkShaders::Color(f.colorValue, nullptr);
  return nullptr;
}

}  // namespace detail

namespace {
/** World-space anchoring, the ONE construction every resolve path here
 *  shares: the built shader is sampled through the inverse of the node's
 *  node-to-root matrix W, so a local drawing coordinate p evaluates the
 *  field at W·p — the root-frame point the node actually occupies. That is
 *  what lets two separate nodes sample one continuous field.
 *
 *  An identity W — outside a composer, or a root-level node with no
 *  transform — wraps nothing, because node-local and root-local are then
 *  the same frame. */
sk_sp<SkShader> anchorToRoot(sk_sp<SkShader> s, const PaintContext& ctx) {
  if (!s || ctx.toRoot.isIdentity()) return s;
  SkMatrix inv;
  if (!ctx.toRoot.invert(&inv))
    return s;  // degenerate transform: nothing sensible to anchor through
  return s->makeWithLocalMatrix(inv);
}
/** W's affine six, fed into the varying-input digest below. A digest cannot
 *  see an input it was never given: leave these out and the frame after a
 *  world-space node moves compares equal to the frame before it, and the
 *  memo hands back a shader anchored to the old position. */
void digestToRoot(std::vector<float>& inputs, const SkMatrix& w) {
  inputs.push_back(w.getScaleX());
  inputs.push_back(w.getSkewX());
  inputs.push_back(w.getTranslateX());
  inputs.push_back(w.getSkewY());
  inputs.push_back(w.getScaleY());
  inputs.push_back(w.getTranslateY());
}
}  // namespace

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected uTime/uResolution/uContentScale — the
// last three only when the effect actually declares them (assigning a uniform
// the effect lacks aborts in debug builds). `ctx` is null for the static build.
sk_sp<SkShader> Material::build(const Live& live, const PaintContext* ctx,
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
  const bool injectTime = ctx && validUniform(effect, "uTime", sizeof(float)) &&
                          !userProvided("uTime");
  const bool injectScale =
      ctx && validUniform(effect, "uContentScale", sizeof(float)) &&
      !userProvided("uContentScale");
  const bool injectRes =
      ctx && validUniform(effect, "uResolution", 2 * sizeof(float)) &&
      !userProvided("uResolution");

  // A world-space material's uResolution is the ROOT canvas size, not the
  // node's box: the shader is already sampling in root coordinates, so
  // dividing by the node's size would rescale the field per node and the
  // two siblings that were meant to share one continuous field would each
  // get their own.
  const SkSize resSize = worldSpace && ctx && !ctx->rootSize.isEmpty()
                             ? ctx->rootSize
                             : (ctx ? ctx->size : SkSize::MakeEmpty());

  // The varying-input digest (constants are fixed per recipe; injected
  // values participate only when actually injected).
  std::vector<float> inputs;
  if (ctx) {
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
    if (worldSpace) digestToRoot(inputs, ctx->toRoot);
    if (injectTime) {
      // Quantized through motion::quantizeTime rather than inline, so that
      // the value digested here and the value assigned to the uniform below
      // are produced by one function at one precision. Two spellings would
      // let the memo compare a time the shader was not built with.
      const double t = motion::quantizeTime(ctx->elapsedSeconds,
                                            (double)live.timeQuantizeHz);
      inputs.push_back((float)t);
    }
    if (injectScale) inputs.push_back(ctx->contentScale);
    if (injectRes) {
      inputs.push_back(resSize.width());
      inputs.push_back(resSize.height());
    }
    // THE MEMO'S BLIND SPOT, closed at the door rather than papered over: a
    // child's varying inputs are the CHILD's (its own binds, its own uTime,
    // its own uResolution) and this digest cannot see them, so a material
    // with a context-needing child skips the memo entirely and rebuilds.
    // Returning `lastShader` there would freeze the child at the frame it
    // was first resolved. Static children (an image, a ramp) are recipe and
    // never vary, so they leave the memo intact.
    bool childNeedsCtx = false;
    for (const auto& [name, child] : live.children)
      childNeedsCtx |= child.isAnimated() || child.geometryDependent();
    if (!childNeedsCtx && live.lastShader && inputs == live.lastInputs)
      return live.lastShader;
  }
  // An sdf style reserves its glow, shadow and border padding INSIDE the
  // node's box, so a generous glow on a modest box leaves almost no
  // interior and the shape all but disappears — with no error anywhere.
  // The two numbers that decide it only meet here (uPad comes from the
  // style, uResolution from layout), so this is the only place the warning
  // can be issued. Once per process, recognised by the sdf prelude's
  // uniform signature, when the reserve is at least half the shorter side.
  if (injectRes && ctx->size.width() > 0 && ctx->size.height() > 0) {
    static bool warnedPad = false;
    if (!warnedPad && validUniform(effect, "uPad", sizeof(float)) &&
        validUniform(effect, "uGlowR", sizeof(float))) {
      const float halfMin =
          0.5f * std::min(ctx->size.width(), ctx->size.height());
      for (const auto& [name, value] : live.constants)
        if (name == "uPad" && value >= halfMin) {
          warnedPad = true;
          SkDebugf(
              "[compose] sdf material: pad %.1f px >= half of the "
              "%.0fx%.0f box — the style's reserve (glow/shadow/border) "
              "eats the whole interior and the visible shape is ~%.1f px "
              "across. Size the node with material::sdf::minBoxFor(style, "
              "contentPx) = content + 2*material::sdf::pad(style). (warned "
              "once)\n",
              value, ctx->size.width(), ctx->size.height(),
              std::max(1.0f, std::min(ctx->size.width(), ctx->size.height()) -
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
  // Children resolve with the SAME PaintContext the parent got (so a live
  // child ticks and a geometry child reads the parent node's box), and with
  // the null context on the static snapshot path.
  for (const auto& [name, child] : live.children)
    b.child(name) = detail::childShader(child, ctx);  // pre-validated at store
  if (ctx) {
    // Auto-injects are size-checked too: a user declaring `uniform float
    // uResolution` must not receive a float2 write (SkDEBUGFAIL).
    if (injectTime) {
      b.uniform("uTime") = (float)motion::quantizeTime(
          ctx->elapsedSeconds, (double)live.timeQuantizeHz);
    }
    if (injectScale) b.uniform("uContentScale") = ctx->contentScale;
    if (injectRes)
      b.uniform("uResolution") =
          std::array<float, 2>{resSize.width(), resSize.height()};
  }
  sk_sp<SkShader> built = b.makeShader();
  // Wrap BEFORE the memo stores. A held world-space field then keeps one
  // stable shader pointer across frames, and shader-pointer stability is
  // exactly what the painter's live-material memo compares to decide a
  // recording can replay. Wrapping after the store would mint a fresh
  // wrapper per resolve and the material would read as never holding still.
  if (worldSpace && ctx) built = anchorToRoot(std::move(built), *ctx);
  if (ctx) {
    live.lastInputs = std::move(inputs);
    live.lastShader = built;
  }
  return built;
}

Material Material::solid(SkColor4f color) {
  Material m;
  m.m_isSolid = true;
  m.m_solid = color;
  return m;
}

Material Material::shader(sk_sp<SkShader> shader) {
  Material m;
  m.m_shader = std::move(shader);
  return m;
}

Material Material::recipe(sigil::material::Material material) {
  sigil::material::skia::install();
  Material m;
  m.m_backed = std::make_shared<Backed>(Backed{std::move(material), {}, {}});
  m.m_shader = m.buildBacked(nullptr);  // static snapshot
  return m;
}

const sigil::material::Material* Material::recipeMaterial() const {
  return m_backed ? &m_backed->material : nullptr;
}

void Material::detachBacked() {
  if (m_backed && m_backed.use_count() > 1)
    m_backed = std::make_shared<Backed>(*m_backed);
}

sk_sp<SkShader> Material::buildBacked(const PaintContext* ctx) const {
  const Backed& backed = *m_backed;
  sigil::material::FrameData frame;
  std::vector<std::byte> key;
  if (ctx) {
    frame.seconds = ctx->elapsedSeconds;
    frame.contentScale = ctx->contentScale;
    // A world-space material's uResolution is the ROOT canvas size, as on
    // the sksl path: the shader samples in root coordinates.
    const SkSize resSize =
        m_worldSpace && !ctx->rootSize.isEmpty() ? ctx->rootSize : ctx->size;
    frame.resolution = {resSize.width(), resSize.height()};
    // An sdf style reserves its glow, shadow and border padding INSIDE the
    // box, so a generous glow on a modest box leaves almost no interior and
    // the shape all but disappears — with no error anywhere. The two
    // numbers that decide it only meet here, once per process.
    const sigil::material::Recipe& r = backed.material.recipe();
    if (r.reads(sigil::material::FrameInput::Resolution) &&
        r.name().rfind("sdf.", 0) == 0 && ctx->size.width() > 0 &&
        ctx->size.height() > 0) {
      static bool warnedPad = false;
      const float pad = backed.material.get<float>("uPad");
      const float halfMin =
          0.5f * std::min(ctx->size.width(), ctx->size.height());
      if (!warnedPad && pad >= halfMin) {
        warnedPad = true;
        SkDebugf(
            "[compose] sdf material: pad %.1f px >= half of the "
            "%.0fx%.0f box — the style's reserve (glow/shadow/border) "
            "eats the whole interior and the visible shape is ~%.1f px "
            "across. Size the node with material::sdf::minBoxFor(style, "
            "contentPx) = content + 2*material::sdf::pad(style). (warned "
            "once)\n",
            pad, ctx->size.width(), ctx->size.height(),
            std::max(1.0f, std::min(ctx->size.width(), ctx->size.height()) -
                               2 * pad));
      }
    }
    digest(backed.material, frame, key);
    if (m_worldSpace) {
      std::vector<float> w;
      digestToRoot(w, ctx->toRoot);
      const auto* bytes = reinterpret_cast<const std::byte*>(w.data());
      key.insert(key.end(), bytes, bytes + w.size() * sizeof(float));
    }
    if (backed.lastShader && key == backed.lastKey) return backed.lastShader;
  }
  sk_sp<SkShader> built = sigil::material::skia::shader(backed.material, frame);
  if (m_worldSpace && ctx) built = anchorToRoot(std::move(built), *ctx);
  if (ctx) {
    backed.lastKey = std::move(key);
    backed.lastShader = built;
  }
  return built;
}

Material Material::linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                          SkTileMode tile) {
  RampArrays r = split(stops);
  const SkPoint pts[2] = {a, b};
  Material m = shader(SkShaders::LinearGradient(pts, makeGradient(r, tile)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Linear;
  rec->p0 = a;
  rec->p1 = b;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Material Material::radial(SkPoint center, float radius, std::vector<Stop> stops,
                          SkTileMode tile) {
  RampArrays r = split(stops);
  Material m =
      shader(SkShaders::RadialGradient(center, radius, makeGradient(r, tile)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Radial;
  rec->p0 = center;
  rec->f0 = radius;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Material Material::conical(SkPoint focus, float focusRadius, SkPoint center,
                           float radius, std::vector<Stop> stops,
                           SkTileMode tile) {
  RampArrays r = split(stops);
  Material m = shader(SkShaders::TwoPointConicalGradient(
      focus, focusRadius, center, radius, makeGradient(r, tile)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Conical;
  rec->p0 = focus;
  rec->p1 = center;
  rec->f0 = focusRadius;
  rec->f1 = radius;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Material Material::sweep(SkPoint center, std::vector<Stop> stops,
                         float startDeg, float endDeg) {
  // Skia's sweep CLAMPS outside [startDeg, endDeg] — it never wraps.
  // A window reaching past the circle (`sweep(c, stops, 90, 450)`, the
  // obvious hue-wheel-starting-at-red) paints the run before startDeg in
  // the first stop's flat colour, silently. The numbers only meet here, so
  // this is where the diagnostic lives — once per process, like the sdf
  // pad warning above.
  if (startDeg < 0.0f || endDeg > 360.0f) {
    static bool warnedSweepWindow = false;
    if (!warnedSweepWindow) {
      warnedSweepWindow = true;
      SkDebugf(
          "[compose] Material::sweep(start %.1f, end %.1f): angles "
          "outside [0, 360] CLAMP, they do not wrap — no canvas angle "
          "ever reaches the part of the window past the circle, so that "
          "run paints in the nearest stop's flat colour. Rotate the "
          "stops into [0, 360] instead. (warned once)\n",
          startDeg, endDeg);
    }
  }
  RampArrays r = split(stops);
  Material m = shader(SkShaders::SweepGradient(
      center, startDeg, endDeg, makeGradient(r, SkTileMode::kClamp)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Sweep;
  rec->p0 = center;
  rec->f0 = startDeg;
  rec->f1 = endDeg;
  rec->stops = std::move(stops);
  m.m_recipe = std::move(rec);
  return m;
}

Material Material::image(sk_sp<SkImage> image, SkTileMode tx, SkTileMode ty,
                         const SkMatrix& local, SkSamplingOptions sampling) {
  if (!image) return {};
  Material m = shader(SkShaders::Image(image, tx, ty, sampling, &local));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Image;
  rec->image = std::move(image);
  rec->tx = tx;
  rec->ty = ty;
  rec->local = local;
  rec->sampling = sampling;
  m.m_recipe = std::move(rec);
  return m;
}

Material Material::buffer(std::shared_ptr<PixelBuffer> source, SkTileMode tx,
                          SkTileMode ty, const SkMatrix& local,
                          SkSamplingOptions sampling) {
  if (!source) return {};
  sk_sp<SkImage> snapshot = source->image();
  if (!snapshot) return {};
  Material m = shader(SkShaders::Image(snapshot, tx, ty, sampling, &local));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Buffer;
  rec->revision = source->revision();
  rec->source = std::move(source);
  rec->tx = tx;
  rec->ty = ty;
  rec->local = local;
  rec->sampling = sampling;
  m.m_recipe = std::move(rec);
  return m;
}

// ---- PixelBuffer -----------------------------------------------------------

struct PixelBuffer::State {
  SkBitmap bitmap;
  std::unique_ptr<SkCanvas> canvas;
};

PixelBuffer::PixelBuffer(int width, int height)
    : m_state(std::make_unique<State>()) {
  m_state->bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(std::max(1, width), std::max(1, height)));
  m_state->bitmap.eraseColor(SK_ColorTRANSPARENT);
  m_state->canvas = std::make_unique<SkCanvas>(m_state->bitmap);
}
PixelBuffer::~PixelBuffer() = default;
SkBitmap& PixelBuffer::bitmap() { return m_state->bitmap; }
SkCanvas& PixelBuffer::canvas() { return *m_state->canvas; }
sk_sp<SkImage> PixelBuffer::image() {
  if (m_snapshotRevision != m_revision) {
    // A COPY, once per commit: the user's bitmap stays mutable while the
    // snapshot the shader holds is immutable — no torn frames, and a
    // pruned describe never reaches this line.
    m_snapshot = SkImages::RasterFromBitmap(m_state->bitmap);
    m_snapshotRevision = m_revision;
  }
  return m_snapshot;
}

Material Material::sksl(sk_sp<SkRuntimeEffect> effect,
                        std::vector<std::pair<std::string, float>> constants) {
  Material m;
  if (!effect) {
    // A material that fails to build must be loud. The usual route here is
    // `MakeForShader` returning null on a shader compile error and the
    // caller passing that null straight in: the material resolves to NONE,
    // the node paints nothing, and the compile error is long gone from the
    // log by the time anyone looks. Say it at BUILD, where the mistake is.
    static bool warnedNullEffect = false;
    if (!warnedNullEffect) {
      warnedNullEffect = true;
      SkDebugf(
          "[compose] Material::sksl(null effect): the material is NONE "
          "and its node will paint nothing. Check the error string "
          "MakeForShader returned next to the effect. (warned once)\n");
    }
    return m;
  }
  m.m_live = std::make_shared<Live>();
  m.m_live->effect = std::move(effect);
  for (auto& [name, value] : constants) {
    if (!validUniform(m.m_live->effect, name, sizeof(float))) {
      warnUnknownUniform("sksl", name);
      continue;
    }
    m.m_live->constants.emplace_back(std::move(name), value);
  }
  m.m_live->usesTime = validUniform(m.m_live->effect, "uTime", sizeof(float));
  m.m_live->usesScale =
      validUniform(m.m_live->effect, "uContentScale", sizeof(float));
  m.m_live->usesGeometry =
      validUniform(m.m_live->effect, "uResolution", 2 * sizeof(float));
  m.m_shader = build(*m.m_live, nullptr);  // static snapshot (constants only)
  return m;
}

namespace {
/** mix(a, b, t) as one nested shader — what a blend layer's amount()
 *  lerps with (SkShaders has Blend but no Lerp). One effect for the
 *  whole process, per the Patterns.h one-effect rule. */
sk_sp<SkShader> mixShaders(sk_sp<SkShader> a, sk_sp<SkShader> b, float t) {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader a;"
                 "uniform shader b;"
                 "uniform half uAmt;"
                 "half4 main(float2 p) { return mix(a.eval(p), b.eval(p),"
                 "                                  uAmt); }"));
    return effect;
  }();
  if (!fx) return b;
  SkRuntimeShaderBuilder builder(fx);
  builder.child("a") = std::move(a);
  builder.child("b") = std::move(b);
  builder.uniform("uAmt") = t;
  return builder.makeShader();
}
}  // namespace

Material Material::blend(std::vector<std::pair<Material, SkBlendMode>> layers) {
  if (layers.empty()) return {};
  sk_sp<SkShader> acc = layers.front().first.asShader();
  for (size_t i = 1; i < layers.size(); ++i) {
    sk_sp<SkShader> src = layers[i].first.asShader();
    if (!acc) {
      acc = std::move(src);
      continue;
    }
    if (!src) continue;
    // amount(): composite the layer in full, then mix the result back
    // toward the accumulation — Photoshop layer opacity, not src-alpha
    // thinning (the two differ on every non-porter-duff mode).
    const float amt = layers[i].first.m_amount;
    sk_sp<SkShader> blended =
        SkShaders::Blend(layers[i].second, acc, std::move(src));
    acc = amt >= 1.0f ? std::move(blended)
                      : mixShaders(std::move(acc), std::move(blended), amt);
  }
  Material m = shader(std::move(acc));
  // Keep the layer materials as the comparable recipe (recursive equality) —
  // a blend containing a live layer compares by that layer's identity, so it
  // stays conservatively un-pruned, as it must (the snapshot sampled Outputs).
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Blend;
  rec->layers = std::move(layers);
  m.m_recipe = std::move(rec);
  return m;
}

bool Material::operator==(const Material& o) const {
  if (m_amount != o.m_amount)
    return false;  // a layer strength is recipe, like a stop or a mode
  if (m_bleed != o.m_bleed)
    return false;  // a cull reserve is recipe too — a change must re-record
  if (m_boundOffset != o.m_boundOffset)
    return false;  // the pan BINDING is recipe, compared by pointer like any
                   // other binding; the values it resolves to are the paint
                   // layer's scalar memo to track, never the prune's
  if (m_worldSpace != o.m_worldSpace)
    return false;  // the FLAG is recipe — it says which frame the author
                   // meant. W itself is layout-derived and never compares
                   // here; it is invalidated the way uResolution is
  if (m_isSolid != o.m_isSolid) return false;
  if (m_isSolid) return m_solid == o.m_solid;
  // Recipe-backed: SigilMaterial's own equality — recipe identity, bytes,
  // bindings by identity, children by value.
  if ((m_backed != nullptr) != (o.m_backed != nullptr)) return false;
  if (m_backed) return m_backed->material == o.m_backed->material;
  // sksl-backed: static recipes compare structurally (effect pointer +
  // constant values); live ones by identity — conservative, they never prune.
  if ((m_live != nullptr) != (o.m_live != nullptr)) return false;
  if (m_live) {
    if (isAnimated() || o.isAnimated()) return m_live == o.m_live;
    // Children are recipe, recursively: two materials over one effect that
    // sample DIFFERENT palettes are different materials, and a node that
    // pruned across that swap would sample the old one forever.
    return m_live->effect == o.m_live->effect &&
           m_live->constants == o.m_live->constants &&
           m_live->constants2 == o.m_live->constants2 &&
           m_live->constants4 == o.m_live->constants4 &&
           m_live->constantArrays == o.m_live->constantArrays &&
           m_live->children == o.m_live->children;
  }
  if ((m_recipe != nullptr) != (o.m_recipe != nullptr)) return false;
  if (m_recipe) return *m_recipe == *o.m_recipe;
  return m_shader == o.m_shader;  // raw shader wrap / none
}

// uniform() and child() mutations copy-on-write the recipe, because Material
// is a VALUE: copies of one base material must never alias each other's
// uniforms. Two elements built from a shared sksl base and then bound to
// different Outputs are the ordinary case, and without this the second bind
// would overwrite the first through the shared block and both would be wrong
// with nothing to indicate it.
void Material::detachLive() {
  if (m_live && m_live.use_count() > 1)
    m_live = std::make_shared<Live>(*m_live);
}

Material& Material::uniform(std::string name, float value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, value);
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", const): ignored — this material has no "
        "named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  // Constants own their slot (injection ownership): a baked-in uTime /
  // uContentScale never ticks again, so it must not keep the material Live
  // — the header promises "the material stays static".
  if (name == "uTime")
    m_live->usesTime = false;
  else if (name == "uContentScale")
    m_live->usesScale = false;
  m_live->constants.emplace_back(std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::child(std::string name, Material source) {
  if (m_backed) {
    detachBacked();
    if (source.m_backed)
      m_backed->material.child(name, source.m_backed->material);
    else
      m_backed->material.child(name, MaterialLeaf(std::move(source)));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::child(\"%s\"): ignored — this material has no shader "
        "children (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!detail::declaresShaderChild(m_live->effect, name)) {
    SkDebugf(
        "Material::child: \"%s\" is not declared by the effect as "
        "`uniform shader` — ignored\n",
        name.c_str());
    return *this;
  }
  detachLive();
  // Last write wins on a name, so re-filling a slot replaces rather than
  // stacking (two entries would both be assigned and the second silently
  // shadow the first in the builder).
  for (auto& slot : m_live->children)
    if (slot.first == name) {
      slot.second = std::move(source);
      m_shader = build(*m_live, nullptr);
      return *this;
    }
  m_live->children.emplace_back(std::move(name), std::move(source));
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::uniform(std::string name,
                            motion::Animatable<float> output) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(output));
    return *this;  // now LIVE
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", &output): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->binds.emplace_back(std::move(name), std::move(output));
  return *this;  // now LIVE; painting resolves per frame (resolve())
}

Material& Material::amount(float a01) {
  m_amount = std::clamp(a01, 0.0f, 1.0f);
  return *this;
}

Material& Material::offset(std::optional<motion::Animatable<float>> x,
                           std::optional<motion::Animatable<float>> y) {
  // Only the image-backed kinds carry a local matrix for the pan to
  // translate. Everything else warns and ignores, following the same rule
  // uniform() uses: never abort (one typo must not kill a live-coding host),
  // and never silently no-op (an ignored pan on a gradient looks like a
  // broken animation).
  const bool pannable = m_recipe && (m_recipe->kind == Recipe::Kind::Image ||
                                     m_recipe->kind == Recipe::Kind::Buffer);
  if (!pannable) {
    SkDebugf(
        "Material::offset(&x, &y): ignored — only image()/buffer() "
        "materials carry a local matrix to pan (Pattern's backend)\n");
    return *this;
  }
  m_boundOffset = {std::move(x), std::move(y)};
  return *this;
}

SkPoint Material::boundOffsetValue() const {
  return {m_boundOffset[0] ? motion::resolveFloatAt(nullptr, *m_boundOffset[0])
                           : 0.0f,
          m_boundOffset[1] ? motion::resolveFloatAt(nullptr, *m_boundOffset[1])
                           : 0.0f};
}

/** The panned build — the ONE construction resolve() and asShader() share
 *  for a bound-offset material: the recipe's static matrix post-translated
 *  by the bindings' CURRENT values.
 *
 *  A fresh shader is minted per call, and that is fine here: the paint
 *  layer's scalar memo compares the resolved pan values rather than the
 *  shader pointer, so a node whose pan is holding still keeps its recording
 *  even though this hands back a new pointer each time. */
sk_sp<SkShader> Material::pannedImageShader() const {
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

Material& Material::worldSpace(bool on) {
  m_worldSpace = on;  // recipe, like amount()/bleed(): joins operator==
  return *this;
}

bool Material::usesWorldSpace() const {
  if (m_worldSpace) return true;
  // The FLAG is layer-local (never inherited), but the reconcile walk
  // needs to see a flagged layer anywhere below: a blend whose second
  // layer anchors still needs its node W-invalidated.
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.usesWorldSpace()) return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.usesWorldSpace()) return true;
  return false;
}

Material& Material::bleed(float px) {
  m_bleed = px;  // read by the recording cull (max-accumulated, so only a
  return *this;  // positive reserve ever grows anything)
}

Material& Material::quantizeTime(float hz) {
  if (m_backed) {
    detachBacked();
    m_backed->material.quantizeTime(hz);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::quantizeTime: ignored — only sksl() materials carry "
        "uTime\n");
    return *this;
  }
  if (!validUniform(m_live->effect, "uTime", sizeof(float))) {
    SkDebugf(
        "Material::quantizeTime: ignored — the effect does not declare "
        "`uniform float uTime`\n");
    return *this;
  }
  detachLive();
  m_live->timeQuantizeHz = hz > 0 ? hz : 0;
  return *this;
}

bool Material::isAnimated() const {
  // A bound pan IS animation: the material re-resolves per frame while the
  // pan moves, so it has to occupy the live slot and every consumer — the
  // deferred blend flatten, child-slot liveness, decoration volatility —
  // must see it as such.
  //
  // HOW its node caches is a separate question, split out below: a pan-only
  // material qualifies for the cheaper scalar-comparison lane rather than
  // the live-material memo. See animatedBeyondBoundOffset().
  if (boundOffsetLive()) return true;
  return animatedBeyondBoundOffset();
}

bool Material::animatedBeyondBoundOffset() const {
  if (m_backed && m_backed->material.isAnimated()) return true;
  // A bound UniformBlock is a bind whose value is a table: the material
  // re-resolves per frame (the resolve memo reads the revision), and its
  // node is declared volatile so a cache cannot freeze the array.
  if (m_live &&
      (!m_live->blocks.empty() || m_live->usesTime || m_live->usesScale))
    return true;
  // A scalar bind counts only while it is LIVE: one holding a plain
  // number is a value written into the uniform, and a node does not
  // repaint forever for a constant.
  if (m_live)
    for (const auto& [name, out] : m_live->binds)
      if (motion::isLive(nullptr, out)) return true;
  // A child slot's volatility is the parent's: the parent samples it, so a
  // live child that did not lift the parent to the live path would be
  // resolved once and frozen into the parent's cache. A NESTED bound
  // offset deliberately counts here (child.isAnimated(), not the
  // subtraction): the node-level scalar lane resolves only the TOP
  // material's own pan, so anything deeper stays conservatively opaque.
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.isAnimated()) return true;
  // A blend inherits liveness from its layers (deferred fold in resolve()).
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.isAnimated()) return true;
  return false;
}

bool Material::geometryDependent() const {
  // W is layout-derived exactly as uResolution is: a world-space material
  // needs the PaintContext (for its node's toRoot) at resolve time, it
  // resolves when the node records, and it re-records when layout moves the
  // node. This one `true` is what routes EVERY flagged material — including
  // the gradient factories, which have no sksl recipe at all — through the
  // context-carrying paths: Element::fill's live slot, the coverage gate's
  // resolve, childShader's per-frame form, and build()'s memo skip.
  if (m_worldSpace) return true;
  if (m_backed && m_backed->material.geometryDependent()) return true;
  if (m_live && m_live->usesGeometry) return true;
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.geometryDependent()) return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.geometryDependent()) return true;
  return false;
}

Material& Material::uniform(std::string name, std::array<float, 2> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, glm::vec2(value[0], value[1]));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", float2): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 2 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->constants2.emplace_back(std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::uniform(std::string name, SkColor4f value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(
        name, sigil::material::Color{value.fR, value.fG, value.fB, value.fA});
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", color): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 4 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->constants4.emplace_back(
      std::move(name),
      std::array<float, 4>{value.fR, value.fG, value.fB, value.fA});
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::uniform(std::string name, std::array<float, 4> value) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name,
                           glm::vec4(value[0], value[1], value[2], value[3]));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", float4): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, 4 * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->constants4.emplace_back(std::move(name), value);
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::uniform(std::string name, std::vector<float> values) {
  if (m_backed) {
    detachBacked();
    m_backed->material.set(name, std::span<const float>(values));
    m_shader = buildBacked(nullptr);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", array): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  // Validated by TOTAL byte size, which is all the builder distinguishes —
  // and the builder refuses a partial array write, so the count must be
  // the declaration's exactly.
  if (!validUniform(m_live->effect, name, values.size() * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->constantArrays.emplace_back(std::move(name), std::move(values));
  m_shader = build(*m_live, nullptr);  // refresh the static snapshot
  return *this;
}

Material& Material::uniform(
    std::string name, std::shared_ptr<const material::UniformBlock> block) {
  if (m_backed) {
    detachBacked();
    m_backed->material.bind(name, std::move(block));
    return *this;  // now LIVE
  }
  if (!m_live) {
    SkDebugf(
        "Material::uniform(\"%s\", block): ignored — this material has "
        "no named uniforms (only sksl() does)\n",
        name.c_str());
    return *this;
  }
  if (!block) {
    SkDebugf(
        "Material::uniform(\"%s\", block): null UniformBlock — there is "
        "nothing to read at paint time; ignored\n",
        name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, block->size() * sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->blocks.emplace_back(std::move(name), std::move(block));
  return *this;  // now LIVE; painting resolves per frame (resolve())
}

sk_sp<SkShader> Material::resolvePass(const detail::TextPassInputs& in,
                                      const PaintContext& ctx) const {
  if (!m_backed) return nullptr;
  const Backed& backed = *m_backed;
  const uint32_t n = std::max(in.units, 1u);
  std::shared_ptr<const sigil::material::Recipe> spec =
      detail::passRecipeFor(backed.material.recipePtr(), n);
  if (!spec) return nullptr;
  // The instance is respecialized per draw rather than held: it carries
  // the material's CURRENT values, and a held one would go stale the
  // moment a uniform was set on the material it was taken from. The
  // definition it points at is what is cached, and that is where the
  // compile lives.
  const sigil::material::Material specialized =
      backed.material.withRecipe(std::move(spec));
  sigil::material::FrameData frame;
  frame.seconds = ctx.elapsedSeconds;
  frame.contentScale = ctx.contentScale;
  const SkSize resSize =
      m_worldSpace && !ctx.rootSize.isEmpty() ? ctx.rootSize : ctx.size;
  frame.resolution = {resSize.width(), resSize.height()};
  // uContent is the runtime's to fill, never the instance's: the layer is
  // rendered per draw and no material value could name it.
  static constexpr std::string_view kContent = "uContent";
  const std::array<std::string_view, 1> leave{kContent};
  std::unique_ptr<SkRuntimeShaderBuilder> b =
      sigil::material::skia::builder(specialized, frame, {}, leave);
  if (!b) return nullptr;  // the cache already said why, once
  b->child(kContent) = in.content;
  // The array counts are the specialization's by construction, and the
  // builder demands exactly them.
  if (in.rects) b->uniform("uUnitRect").set(in.rects, (int)n * 4);
  if (in.phases) b->uniform("uUnitPhase").set(in.phases, (int)n * 2);
  sk_sp<SkShader> built = b->makeShader();
  // No memo here: the per-unit phases move every frame the pass is live,
  // and a settled pass replays from its node's recording rather than
  // resolving.
  if (m_worldSpace) built = anchorToRoot(std::move(built), ctx);
  return built;
}

/** THE BLEND FOLD, in one place because it has two callers that must agree:
 *  `ctx` non-null is resolve()'s per-frame form, null is asShader()'s
 *  context-free one. Either way the LAYERS are re-read here rather than the
 *  flattened snapshot blend() built, which is the whole point — a live layer
 *  contributes its current value per call. */
sk_sp<SkShader> Material::foldBlend(const PaintContext* ctx) const {
  sk_sp<SkShader> acc;
  bool first = true;
  for (const auto& [mat, mode] : m_recipe->layers) {
    sk_sp<SkShader> src = detail::childShader(mat, ctx);
    if (first) {
      acc = std::move(src);
      first = false;
      continue;
    }
    if (!acc) {
      acc = std::move(src);
      continue;
    }
    if (!src) continue;
    const float amt = mat.m_amount;  // same rule as the eager flatten
    sk_sp<SkShader> blended = SkShaders::Blend(mode, acc, std::move(src));
    acc = amt >= 1.0f ? std::move(blended)
                      : mixShaders(std::move(acc), std::move(blended), amt);
  }
  return acc;
}

sk_sp<SkShader> Material::asShader() const {
  // A BLEND has no sksl recipe of its own — it inherits liveness through
  // its layers — so the live branch below would dereference a null m_live
  // for it. This is reachable by nesting a blend inside another blend's
  // layer list, since blend() calls asShader() on every layer. Guarding the
  // null and falling through to m_shader would not be right either:
  // m_shader is the eager snapshot blend() built, which is precisely the
  // stale answer the live branch exists to avoid. Fold the layers instead,
  // per call.
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend && isAnimated())
    return foldBlend(nullptr);
  // A bound-offset image material's m_shader snapshot baked the static
  // matrix — rebuild with the pan's current values, the same
  // stale-snapshot rule as the live branch below.
  if (hasBoundOffset())
    if (sk_sp<SkShader> panned = pannedImageShader()) return panned;
  // A live material's m_shader snapshot predates its binds, so rebuild it
  // fresh and let bound Outputs contribute their CURRENT values — this is
  // what blend() flattens, and returning the snapshot would bake whatever
  // the Outputs happened to hold at construction. The m_live guard is
  // explicit rather than implied by isAnimated(): a bound pan (handled just
  // above) reports animated with no sksl recipe behind it.
  if (m_live && isAnimated()) return build(*m_live, nullptr);
  if (m_backed && isAnimated()) return buildBacked(nullptr);
  if (m_shader) return m_shader;
  if (m_isSolid) return SkShaders::Color(m_solid, nullptr);
  return nullptr;  // none
}

Fill Material::toFill() const {
  if (m_isSolid) return Fill::color(m_solid);
  if (m_shader) return Fill::shader(m_shader);
  return Fill::none();
}

Fill Material::resolve(const PaintContext& ctx) const {
  // Deferred blend: when any layer needs the PaintContext (live uniforms,
  // SDF uResolution), the flatten happens HERE, per resolve, so every layer
  // contributes its correct current form — the eager snapshot from blend()
  // would have baked those layers with a null context (uResolution = 0,0).
  // World-space is layer-local by design: a flagged OUTER blend anchors the
  // whole fold here, while a flagged LAYER already anchored itself on the
  // way through childShader → resolve.
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend &&
      (isAnimated() || geometryDependent())) {
    sk_sp<SkShader> folded = foldBlend(&ctx);
    if (m_worldSpace) folded = anchorToRoot(std::move(folded), ctx);
    return Fill::shader(folded);
  }
  // The sksl path — build() digests W and applies the world-space wrap
  // inside its memo. Guarded on m_live because a world-space flag makes
  // gradient-factory materials geometry-dependent too, and those have no
  // sksl recipe; they take the final branch below instead.
  if (m_live && (isAnimated() || geometryDependent()))
    return Fill::shader(build(*m_live, &ctx, m_worldSpace));
  // The recipe-backed path — the same rule, through SigilMaterial's cache.
  if (m_backed && (isAnimated() || geometryDependent()))
    return Fill::shader(buildBacked(&ctx));
  // The bound pan: the recipe matrix translated by the bindings' current
  // values, per resolve. It has to sit above toFill() because the static
  // snapshot baked the UNpanned matrix. The paint layer's scalar memo,
  // which carries the resolved pan among its content scalars, is what keeps
  // the node's recording alive between moves.
  if (hasBoundOffset()) {
    if (sk_sp<SkShader> panned = pannedImageShader()) {
      Fill f = Fill::shader(std::move(panned));
      if (m_worldSpace) f.shaderValue = anchorToRoot(f.shaderValue, ctx);
      return f;
    }
  }
  // World-space anchoring for everything that ISN'T an sksl recipe: the
  // gradient factories, image()/buffer(), raw shader() wraps. A gradient
  // declared in canvas coordinates reaches root space through this line.
  // The wrapper is minted per resolve, which costs nothing here: these are
  // geometry-tier materials, resolved when the node records, so no
  // per-frame pointer stability is at stake.
  Fill f = toFill();
  if (m_worldSpace && f.kind == Fill::Kind::Shader && f.shaderValue)
    f.shaderValue = anchorToRoot(f.shaderValue, ctx);
  return f;
}

Element& Element::textFill(Material m) {
  m_node->textData.ensure().metricFill = std::move(m);
  return *this;
}

Element& Element::fill(Material m) {
  detail::MaterialData& slots = m_node->materialData.ensure();
  if (m.isAnimated() || m.geometryDependent()) {
    // Live materials re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with PaintContext.
    slots.live = std::move(m);
    m_node->paint.fill.reset();
    slots.recipe.reset();
  } else {
    m_node->paint.fill = motion::Animatable<Fill>{m.toFill()};
    slots.recipe = std::move(m);  // the prune signature
    slots.live.reset();
  }
  return *this;
}

}  // namespace sigil::compose
