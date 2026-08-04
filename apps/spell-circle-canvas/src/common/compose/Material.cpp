// Material — construction of the polymorphic paint value. Static materials
// (solid/gradient/image/blend/const-uniform sksl) resolve eagerly to a color
// or shader and collapse to a Fill (toFill()), riding the kernel's existing
// fill path. A LIVE material (sksl with a ch::Output-bound uniform) keeps its
// runtime-effect recipe and rebuilds its shader every frame from the current
// uniform values (resolve()); Element::fill routes it to the live path and the
// painter re-resolves it while its node stays volatile. See Material.h.

#include "sigilcompose/Material.h"

#include "ComposeInternal.h" // ElementNode, for Element::fill(Material)

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h> // SkDebugf
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>
#include <cmath>

namespace sigil::compose {

/** The sksl recipe behind a Material (opaque in the header). */
struct Material::Live {
  sk_sp<SkRuntimeEffect> effect;
  std::vector<std::pair<std::string, float>> constants;
  std::vector<std::pair<std::string, std::array<float, 2>>> constants2;
  std::vector<std::pair<std::string, std::array<float, 4>>> constants4;
  std::vector<std::pair<std::string, const choreograph::Output<float> *>> binds;
  // child(): `uniform shader NAME` slots, filled with whole Materials (§10f).
  // They are recipe (equality) AND volatility: the parent inherits each
  // child's tier, and resolves every child with the same PaintContext.
  std::vector<std::pair<std::string, Material>> children;
  // Context tiers (the REVIEW rule, refined by the leg review):
  //  - usesTime / usesScale: uTime changes every frame and uContentScale
  //    changes with the HOST's canvas scale (zoom) — neither is a function
  //    of the node, so both make the material LIVE (per-frame resolve).
  //  - usesGeometry: uResolution is the node's layout size — stable between
  //    layouts, so the material resolves when the node RECORDS, caches, and
  //    re-records on size change (syncLayoutRects).
  bool usesTime = false;
  bool usesScale = false;
  bool usesGeometry = false;
  // quantizeTime(): step the injected uTime at this rate (0 = continuous) —
  // the P3R sea rule ("we imagine the interpolation ourselves").
  float timeQuantizeHz = 0;
  // Resolve memo: when every varying input (bound Outputs + injected
  // time/scale/resolution) is byte-identical to the previous build, the
  // previous shader is returned — quantized time makes consecutive frames
  // identical, and the paint layer turns that stability into replayed
  // pictures instead of re-rasterized shaders.
  mutable std::vector<float> lastInputs;
  mutable sk_sp<SkShader> lastShader;
};

/** The comparable build recipe behind gradient/image/blend materials — the
 *  structural signature that lets two independently built materials compare
 *  equal (the §8.1 prune). Solid/raw-shader/sksl compare via Material's own
 *  state instead. */
struct Material::Recipe {
  enum class Kind : uint8_t { Linear, Radial, Conical, Sweep, Image, Blend,
                              Buffer };
  Kind kind = Kind::Linear;
  // Gradients: endpoints/center + radius or sweep degrees + ramp.
  SkPoint p0 = {0, 0}, p1 = {0, 0}; // Conical: (focus, center)
  float f0 = 0.0f, f1 = 0.0f; // radius / (startDeg, endDeg) /
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
  // Buffer (§4): the Instances pruning rule verbatim — identity + revision.
  std::shared_ptr<PixelBuffer> source;
  uint64_t revision = 0;

  bool operator==(const Recipe &o) const {
    if (kind != o.kind)
      return false;
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
  std::vector<float> positions; // empty → evenly spaced
};

RampArrays split(const std::vector<Stop> &stops) {
  RampArrays r;
  r.colors.reserve(stops.size());
  r.positions.reserve(stops.size());
  for (const Stop &s : stops) {
    r.colors.push_back(s.color);
    r.positions.push_back(s.pos);
  }
  return r;
}

// A named uniform is usable iff the effect declares it at the expected size —
// assigning an undeclared or mis-sized uniform SkDEBUGFAILs (aborts debug
// builds), which would let one typo kill the ComposeSketch hot-reload host.
// Unknown names warn-and-ignore instead (validated at sksl()/uniform() time,
// so build() never touches an invalid entry).
bool validUniform(const sk_sp<SkRuntimeEffect> &effect, std::string_view name,
                  size_t bytes) {
  if (!effect)
    return false;
  const SkRuntimeEffect::Uniform *u = effect->findUniform(name);
  return u && u->sizeInBytes() == bytes;
}

void warnUnknownUniform(const char *what, const std::string &name) {
  SkDebugf("Material::%s: uniform \"%s\" is not declared by the effect at "
           "float size — ignored\n",
           what, name.c_str());
}

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays &r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()}, tile},
                    {});
}

} // namespace

namespace detail {

// A named child is usable iff the effect declares it as a SHADER child —
// assigning a missing child SkDEBUGFAILs exactly like a missing uniform.
// Shared with Effect::child (§19): one validation, two child doors.
bool declaresShaderChild(const sk_sp<SkRuntimeEffect> &effect,
                         std::string_view name) {
  if (!effect)
    return false;
  const SkRuntimeEffect::Child *c = effect->findChild(name);
  return c && c->type == SkRuntimeEffect::ChildType::kShader;
}

// The Material→SkShader conversion every child slot performs (declared in
// Material.h): the per-frame resolve when there is a context, the
// context-free snapshot when there is not, a solid as a colour shader.
sk_sp<SkShader> childShader(const Material &source, const PaintContext *ctx) {
  if (!ctx)
    return source.asShader(); // already turns a solid into SkShaders::Color
  const Fill f = source.resolve(*ctx);
  if (f.kind == Fill::Kind::Shader)
    return f.shaderValue;
  if (f.kind == Fill::Kind::Color)
    return SkShaders::Color(f.colorValue, nullptr);
  return nullptr;
}

} // namespace detail

namespace {
/** §19's wrap, the ONE construction both resolve paths share: the built
 *  shader sampled through W⁻¹, so a local drawing coordinate p evaluates
 *  the field at W·p — the root-frame point the node actually occupies.
 *  Precedent: textFill's metric-band mapping (Paint.cpp), the same
 *  makeWithLocalMatrix composition. Identity W (outside a composer, or a
 *  root-level node with no transform) wraps nothing: node-local and
 *  root-local are the same frame there, which is the documented
 *  degradation. */
sk_sp<SkShader> anchorToRoot(sk_sp<SkShader> s, const PaintContext &ctx) {
  if (!s || ctx.toRoot.isIdentity())
    return s;
  SkMatrix inv;
  if (!ctx.toRoot.invert(&inv))
    return s; // degenerate transform: nothing sensible to anchor through
  return s->makeWithLocalMatrix(inv);
}
/** W's affine six, for the varying-input digest (§10f: a digest cannot
 *  see an input it was never fed — without these, frame 2 after a move
 *  serves frame 1's shader). */
void digestToRoot(std::vector<float> &inputs, const SkMatrix &w) {
  inputs.push_back(w.getScaleX());
  inputs.push_back(w.getSkewX());
  inputs.push_back(w.getTranslateX());
  inputs.push_back(w.getSkewY());
  inputs.push_back(w.getScaleY());
  inputs.push_back(w.getTranslateY());
}
} // namespace

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected uTime/uResolution/uContentScale — the
// last three only when the effect actually declares them (assigning a uniform
// the effect lacks aborts in debug builds). `ctx` is null for the static build.
sk_sp<SkShader> Material::build(const Live &live, const PaintContext *ctx,
                                bool worldSpace) {
  if (!live.effect)
    return nullptr;
  // A user-provided uniform (constant or bound) OWNS its slot: the
  // auto-injects must never overwrite it — binding uTime to a quantized
  // Output is the documented way to step a material, and injection ran
  // AFTER binds, silently defeating it (the persona-sea finding).
  auto userProvided = [&](std::string_view name) {
    for (const auto &[n, v] : live.constants)
      if (n == name)
        return true;
    for (const auto &[n, v] : live.constants2)
      if (n == name)
        return true;
    for (const auto &[n, v] : live.constants4)
      if (n == name)
        return true;
    for (const auto &[n, o] : live.binds)
      if (n == name)
        return true;
    return false;
  };
  const bool injectTime = ctx &&
                          validUniform(live.effect, "uTime", sizeof(float)) &&
                          !userProvided("uTime");
  const bool injectScale =
      ctx && validUniform(live.effect, "uContentScale", sizeof(float)) &&
      !userProvided("uContentScale");
  const bool injectRes =
      ctx && validUniform(live.effect, "uResolution", 2 * sizeof(float)) &&
      !userProvided("uResolution");

  // §19: a world-space material's uResolution is the ROOT canvas size —
  // linearUnit's division lands in canvas-unit space, so a unit ramp spans
  // the canvas and two flagged siblings sample one continuous field.
  const SkSize resSize = worldSpace && ctx && !ctx->rootSize.isEmpty()
                             ? ctx->rootSize
                             : (ctx ? ctx->size : SkSize::MakeEmpty());

  // The varying-input digest (constants are fixed per recipe; injected
  // values participate only when actually injected).
  std::vector<float> inputs;
  if (ctx) {
    inputs.reserve(live.binds.size() + 10);
    for (const auto &[name, out] : live.binds)
      inputs.push_back(out ? out->value() : 0.0f);
    // §19 × §10f: when anchored, W is a varying input like any other — a
    // node that MOVED resolves a different shader, and the memo must see
    // that or the second frame after a move serves the stale one.
    if (worldSpace)
      digestToRoot(inputs, ctx->toRoot);
    // §19 × §10f: when anchored, W is a varying input like any other — a
    // node that MOVED resolves a different shader, and the memo must see
    // that or the second frame after a move serves the stale one.
    if (injectTime) {
      // Routed through the canonical quantizer (motion::quantizeTime) at
      // the SAME precision — double in, hz promoted exactly as the old
      // inline floor did — so this is a spelling change, not a pixel one.
      const double t =
          motion::quantizeTime(ctx->elapsedSeconds, (double)live.timeQuantizeHz);
      inputs.push_back((float)t);
    }
    if (injectScale)
      inputs.push_back(ctx->contentScale);
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
    for (const auto &[name, child] : live.children)
      childNeedsCtx |= child.isAnimated() || child.geometryDependent();
    if (!childNeedsCtx && live.lastShader && inputs == live.lastInputs)
      return live.lastShader;
  }
  // §14: the sdf:: glow eats the shape silently. sdf::pad() is reserved
  // INSIDE the node's box, so a 300×300 box with glowRadius 54 renders a
  // ~1px disc and said nothing — sdf::minBoxFor() is the answer and
  // nothing pointed at it from the call site. The numbers only meet HERE
  // (uPad is a style constant, uResolution the laid-out size), so this is
  // where the warning lives: once per process, on the sdf prelude's
  // signature (uPad + uGlowR + uResolution), when pad >= half-size.
  if (injectRes && ctx->size.width() > 0 && ctx->size.height() > 0) {
    static bool warnedPad = false;
    if (!warnedPad && validUniform(live.effect, "uPad", sizeof(float)) &&
        validUniform(live.effect, "uGlowR", sizeof(float))) {
      const float halfMin =
          0.5f * std::min(ctx->size.width(), ctx->size.height());
      for (const auto &[name, value] : live.constants)
        if (name == "uPad" && value >= halfMin) {
          warnedPad = true;
          SkDebugf("[compose] sdf material: pad %.1f px >= half of the "
                   "%.0fx%.0f box — the style's reserve (glow/shadow/border) "
                   "eats the whole interior and the visible shape is ~%.1f px "
                   "across. Size the node with sdf::minBoxFor(style, "
                   "contentPx) = content + 2*sdf::pad(style). (warned once)\n",
                   value, ctx->size.width(), ctx->size.height(),
                   std::max(1.0f, std::min(ctx->size.width(),
                                           ctx->size.height()) -
                                      2 * value));
          break;
        }
    }
  }
  SkRuntimeShaderBuilder b(live.effect);
  for (const auto &[name, value] : live.constants)
    b.uniform(name.c_str()) = value; // entries pre-validated at store time
  for (const auto &[name, value] : live.constants2)
    b.uniform(name.c_str()) = value;
  for (const auto &[name, value] : live.constants4)
    b.uniform(name.c_str()) = value;
  for (const auto &[name, out] : live.binds)
    if (out)
      b.uniform(name.c_str()) = out->value();
  // Children resolve with the SAME PaintContext the parent got (so a live
  // child ticks and a geometry child reads the parent node's box), and with
  // the null context on the static snapshot path.
  for (const auto &[name, child] : live.children)
    b.child(name.c_str()) =
        detail::childShader(child, ctx); // pre-validated at store
  if (ctx) {
    // Auto-injects are size-checked too: a user declaring `uniform float
    // uResolution` must not receive a float2 write (SkDEBUGFAIL).
    if (injectTime) {
      b.uniform("uTime") = (float)motion::quantizeTime(
          ctx->elapsedSeconds, (double)live.timeQuantizeHz);
    }
    if (injectScale)
      b.uniform("uContentScale") = ctx->contentScale;
    if (injectRes)
      b.uniform("uResolution") =
          std::array<float, 2>{resSize.width(), resSize.height()};
  }
  sk_sp<SkShader> built = b.makeShader();
  // §19: wrap BEFORE the memo stores — a held world-space field then keeps
  // a stable shader pointer across frames, which is what the liveMatOnly
  // stability probe compares (a per-resolve wrapper would read as a
  // material that never holds still).
  if (worldSpace && ctx)
    built = anchorToRoot(std::move(built), *ctx);
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

Material Material::sweep(SkPoint center, std::vector<Stop> stops, float startDeg,
                         float endDeg) {
  // §10j: Skia's sweep CLAMPS outside [startDeg, endDeg] — it never wraps.
  // A window reaching past the circle (`sweep(c, stops, 90, 450)`, the
  // obvious hue-wheel-starting-at-red) paints the run before startDeg in
  // the first stop's flat colour, silently. The numbers only meet here, so
  // this is where the diagnostic lives — once per process, like the sdf
  // pad warning above.
  if (startDeg < 0.0f || endDeg > 360.0f) {
    static bool warnedSweepWindow = false;
    if (!warnedSweepWindow) {
      warnedSweepWindow = true;
      SkDebugf("[compose] Material::sweep(start %.1f, end %.1f): angles "
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
                         const SkMatrix &local, SkSamplingOptions sampling) {
  if (!image)
    return {};
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

Material Material::buffer(std::shared_ptr<PixelBuffer> source,
                          SkTileMode tx, SkTileMode ty, const SkMatrix &local,
                          SkSamplingOptions sampling) {
  if (!source)
    return {};
  sk_sp<SkImage> snapshot = source->image();
  if (!snapshot)
    return {};
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

// ---- PixelBuffer (§4) ------------------------------------------------------

struct PixelBuffer::State {
  SkBitmap bitmap;
  std::unique_ptr<SkCanvas> canvas;
};

PixelBuffer::PixelBuffer(int width, int height)
    : m_state(std::make_unique<State>()) {
  m_state->bitmap.allocPixels(SkImageInfo::MakeN32Premul(
      std::max(1, width), std::max(1, height)));
  m_state->bitmap.eraseColor(SK_ColorTRANSPARENT);
  m_state->canvas = std::make_unique<SkCanvas>(m_state->bitmap);
}
PixelBuffer::~PixelBuffer() = default;
SkBitmap &PixelBuffer::bitmap() { return m_state->bitmap; }
SkCanvas &PixelBuffer::canvas() { return *m_state->canvas; }
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
    // Host & tooling: "a material that fails to build should be loud."
    // The classic route here is `MakeForShader` returning null and the
    // caller passing it straight in — the material then resolves to NONE
    // and the node paints nothing, with the compile error long scrolled
    // away. Say so at BUILD, once, where the mistake is.
    static bool warnedNullEffect = false;
    if (!warnedNullEffect) {
      warnedNullEffect = true;
      SkDebugf("[compose] Material::sksl(null effect): the material is NONE "
               "and its node will paint nothing. Check the error string "
               "MakeForShader returned next to the effect. (warned once)\n");
    }
    return m;
  }
  m.m_live = std::make_shared<Live>();
  m.m_live->effect = std::move(effect);
  for (auto &[name, value] : constants) {
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
  m.m_shader = build(*m.m_live, nullptr); // static snapshot (constants only)
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
  if (!fx)
    return b;
  SkRuntimeShaderBuilder builder(fx);
  builder.child("a") = std::move(a);
  builder.child("b") = std::move(b);
  builder.uniform("uAmt") = t;
  return builder.makeShader();
}
} // namespace

Material Material::blend(std::vector<std::pair<Material, SkBlendMode>> layers) {
  if (layers.empty())
    return {};
  sk_sp<SkShader> acc = layers.front().first.asShader();
  for (size_t i = 1; i < layers.size(); ++i) {
    sk_sp<SkShader> src = layers[i].first.asShader();
    if (!acc) {
      acc = std::move(src);
      continue;
    }
    if (!src)
      continue;
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

bool Material::operator==(const Material &o) const {
  if (m_amount != o.m_amount)
    return false; // a layer strength is recipe, like a stop or a mode
  if (m_bleed != o.m_bleed)
    return false; // a cull reserve is recipe too — a change must re-record
  if (m_worldSpace != o.m_worldSpace)
    return false; // §19: the FLAG is recipe (which frame the author meant);
                  // W itself is layout-derived and never compares — it
                  // rides invalidation exactly like uResolution does
  if (m_isSolid != o.m_isSolid)
    return false;
  if (m_isSolid)
    return m_solid == o.m_solid;
  // sksl-backed: static recipes compare structurally (effect pointer +
  // constant values); live ones by identity — conservative, they never prune.
  if ((m_live != nullptr) != (o.m_live != nullptr))
    return false;
  if (m_live) {
    if (isAnimated() || o.isAnimated())
      return m_live == o.m_live;
    // Children are recipe, recursively: two materials over one effect that
    // sample DIFFERENT palettes are different materials, and a node that
    // pruned across that swap would sample the old one forever.
    return m_live->effect == o.m_live->effect &&
           m_live->constants == o.m_live->constants &&
           m_live->constants2 == o.m_live->constants2 &&
           m_live->constants4 == o.m_live->constants4 &&
           m_live->children == o.m_live->children;
  }
  if ((m_recipe != nullptr) != (o.m_recipe != nullptr))
    return false;
  if (m_recipe)
    return *m_recipe == *o.m_recipe;
  return m_shader == o.m_shader; // raw shader wrap / none
}

// uniform() mutations copy-on-write the recipe: Material is a VALUE — copies
// of a base material must never alias one another's uniforms (two HUD bars
// sharing one sksl base bind different Outputs; last-write-wins would corrupt
// both silently — the Fable audit's aliasing defect).
void Material::detachLive() {
  if (m_live && m_live.use_count() > 1)
    m_live = std::make_shared<Live>(*m_live);
}

Material &Material::uniform(std::string name, float value) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", const): ignored — this material has no "
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
  m_shader = build(*m_live, nullptr); // refresh the static snapshot
  return *this;
}

Material &Material::child(std::string name, Material source) {
  if (!m_live) {
    SkDebugf("Material::child(\"%s\"): ignored — this material has no shader "
             "children (only sksl() does)\n",
             name.c_str());
    return *this;
  }
  if (!detail::declaresShaderChild(m_live->effect, name)) {
    SkDebugf("Material::child: \"%s\" is not declared by the effect as "
             "`uniform shader` — ignored\n",
             name.c_str());
    return *this;
  }
  detachLive();
  // Last write wins on a name, so re-filling a slot replaces rather than
  // stacking (two entries would both be assigned and the second silently
  // shadow the first in the builder).
  for (auto &slot : m_live->children)
    if (slot.first == name) {
      slot.second = std::move(source);
      m_shader = build(*m_live, nullptr);
      return *this;
    }
  m_live->children.emplace_back(std::move(name), std::move(source));
  m_shader = build(*m_live, nullptr); // refresh the static snapshot
  return *this;
}

Material &Material::uniform(std::string name,
                            const choreograph::Output<float> *output) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", &output): ignored — this material has "
             "no named uniforms (only sksl() does)\n",
             name.c_str());
    return *this;
  }
  if (!validUniform(m_live->effect, name, sizeof(float))) {
    warnUnknownUniform("uniform", name);
    return *this;
  }
  detachLive();
  m_live->binds.emplace_back(std::move(name), output);
  return *this; // now LIVE; painting resolves per frame (resolve())
}

Material &Material::amount(float a01) {
  m_amount = std::clamp(a01, 0.0f, 1.0f);
  return *this;
}

Material &Material::worldSpace(bool on) {
  m_worldSpace = on; // recipe, like amount()/bleed(): joins operator==
  return *this;
}

bool Material::usesWorldSpace() const {
  if (m_worldSpace)
    return true;
  // The FLAG is layer-local (never inherited), but the reconcile walk
  // needs to see a flagged layer anywhere below: a blend whose second
  // layer anchors still needs its node W-invalidated.
  if (m_live)
    for (const auto &[name, child] : m_live->children)
      if (child.usesWorldSpace())
        return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto &layer : m_recipe->layers)
      if (layer.first.usesWorldSpace())
        return true;
  return false;
}

Material &Material::bleed(float px) {
  m_bleed = px; // read by the recording cull (max-accumulated, so only a
  return *this; // positive reserve ever grows anything)
}

Material &Material::quantizeTime(float hz) {
  if (!m_live) {
    SkDebugf("Material::quantizeTime: ignored — only sksl() materials carry "
             "uTime\n");
    return *this;
  }
  if (!validUniform(m_live->effect, "uTime", sizeof(float))) {
    SkDebugf("Material::quantizeTime: ignored — the effect does not declare "
             "`uniform float uTime`\n");
    return *this;
  }
  detachLive();
  m_live->timeQuantizeHz = hz > 0 ? hz : 0;
  return *this;
}

bool Material::isAnimated() const {
  if (m_live &&
      (!m_live->binds.empty() || m_live->usesTime || m_live->usesScale))
    return true;
  // A child slot's volatility is the parent's: the parent samples it, so a
  // live child that did not lift the parent to the live path would be
  // resolved once and frozen into the parent's cache.
  if (m_live)
    for (const auto &[name, child] : m_live->children)
      if (child.isAnimated())
        return true;
  // A blend inherits liveness from its layers (deferred fold in resolve()).
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto &layer : m_recipe->layers)
      if (layer.first.isAnimated())
        return true;
  return false;
}

bool Material::geometryDependent() const {
  // §19: W is layout-derived exactly as uResolution is — a world-space
  // material needs PaintContext (its node's toRoot) at resolve, resolves
  // when the node records, and re-records when the layout moves it. This
  // one `true` is what routes EVERY flagged material — gradient factories
  // included, which have no m_live — through the context-carrying paths:
  // Element::fill's live slot, the coverage gate's resolve, childShader's
  // per-frame form, and build()'s memo skip for children.
  if (m_worldSpace)
    return true;
  if (m_live && m_live->usesGeometry)
    return true;
  if (m_live)
    for (const auto &[name, child] : m_live->children)
      if (child.geometryDependent())
        return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto &layer : m_recipe->layers)
      if (layer.first.geometryDependent())
        return true;
  return false;
}

Material &Material::uniform(std::string name, std::array<float, 2> value) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", float2): ignored — this material has "
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
  m_shader = build(*m_live, nullptr); // refresh the static snapshot
  return *this;
}

Material &Material::uniform(std::string name, SkColor4f value) {
  if (!m_live) {
    SkDebugf("Material::uniform(\"%s\", color): ignored — this material has "
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
      std::move(name), std::array<float, 4>{value.fR, value.fG, value.fB,
                                            value.fA});
  m_shader = build(*m_live, nullptr); // refresh the static snapshot
  return *this;
}

/** THE BLEND FOLD, in one place because it has two callers that must agree:
 *  `ctx` non-null is resolve()'s per-frame form, null is asShader()'s
 *  context-free one. Either way the LAYERS are re-read here rather than the
 *  flattened snapshot blend() built, which is the whole point — a live layer
 *  contributes its current value per call. */
sk_sp<SkShader> Material::foldBlend(const PaintContext *ctx) const {
  sk_sp<SkShader> acc;
  bool first = true;
  for (const auto &[mat, mode] : m_recipe->layers) {
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
    if (!src)
      continue;
    const float amt = mat.m_amount; // same rule as the eager flatten
    sk_sp<SkShader> blended = SkShaders::Blend(mode, acc, std::move(src));
    acc = amt >= 1.0f ? std::move(blended)
                      : mixShaders(std::move(acc), std::move(blended), amt);
  }
  return acc;
}

sk_sp<SkShader> Material::asShader() const {
  // A BLEND has no m_live of its own — it inherits liveness through
  // m_recipe->layers — so the live branch below would dereference null for
  // it (§35.1; reachable by nesting a blend in another blend's layer list,
  // because blend() calls asShader() on every layer). Guarding the null and
  // falling through to m_shader would not be right either: m_shader is the
  // eager snapshot blend() built, which is the exact stale answer the live
  // branch exists to prevent. Fold the layers instead, per call.
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend && isAnimated())
    return foldBlend(nullptr);
  // A live material's m_shader snapshot predates its binds — rebuild fresh so
  // bound Outputs contribute their CURRENT values (what blend() flattens; the
  // Fable audit's stale-snapshot defect). Past the blend case, isAnimated()
  // implies m_live: the other two liveness sources both read it.
  if (isAnimated())
    return build(*m_live, nullptr);
  if (m_shader)
    return m_shader;
  if (m_isSolid)
    return SkShaders::Color(m_solid, nullptr);
  return nullptr; // none
}

Fill Material::toFill() const {
  if (m_isSolid)
    return Fill::color(m_solid);
  if (m_shader)
    return Fill::shader(m_shader);
  return Fill::none();
}

Fill Material::resolve(const PaintContext &ctx) const {
  // Deferred blend: when any layer needs the PaintContext (live uniforms,
  // SDF uResolution), the flatten happens HERE, per resolve, so every layer
  // contributes its correct current form — the eager snapshot from blend()
  // would have baked those layers with a null context (uResolution = 0,0).
  // §19: a flagged OUTER blend anchors the whole fold; a flagged LAYER
  // anchored itself inside childShader → resolve (layer-local, by design).
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend &&
      (isAnimated() || geometryDependent())) {
    sk_sp<SkShader> folded = foldBlend(&ctx);
    if (m_worldSpace)
      folded = anchorToRoot(std::move(folded), ctx);
    return Fill::shader(folded);
  }
  // The sksl path — build() digests W and wraps inside the memo. Guarded
  // on m_live now: §19 makes gradient-factory materials (no m_live)
  // geometryDependent too, and those take the branch below instead.
  if (m_live && (isAnimated() || geometryDependent()))
    return Fill::shader(build(*m_live, &ctx, m_worldSpace));
  // §19's seam for everything that ISN'T an sksl recipe: the gradient
  // factories, image()/buffer(), raw shader() wraps — chaucer's brass ramp
  // is Material::linear in canvas px, and this is the line that reaches
  // it. (The wrap is minted per resolve; these are geometry-tier, resolved
  // at record, so no per-frame pointer stability is at stake.)
  Fill f = toFill();
  if (m_worldSpace && f.kind == Fill::Kind::Shader && f.shaderValue)
    f.shaderValue = anchorToRoot(f.shaderValue, ctx);
  return f;
}

Element &Element::textFill(Material m) {
  m_node->textData.ensure().metricFill = std::move(m);
  return *this;
}

Element &Element::fill(Material m) {
  detail::MaterialData &slots = m_node->materialData.ensure();
  if (m.isAnimated() || m.geometryDependent()) {
    // Live materials re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with PaintContext.
    slots.live = std::move(m);
    m_node->paint.fill.reset();
    slots.recipe.reset();
  } else {
    m_node->paint.fill = Animatable<Fill>{m.toFill()};
    slots.recipe = std::move(m); // the prune signature
    slots.live.reset();
  }
  return *this;
}

} // namespace sigil::compose
