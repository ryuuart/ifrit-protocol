// Material — construction of the polymorphic paint value. Static materials
// (solid/gradient/image/blend/const-uniform sksl) resolve eagerly to a color
// or shader and collapse to a Fill (toFill()), riding the kernel's existing
// fill path. A LIVE material (sksl with a ch::Output-bound uniform) keeps its
// runtime-effect recipe and rebuilds its shader every frame from the current
// uniform values (resolve()); Element::fill routes it to the live path and the
// painter re-resolves it while its node stays volatile. See Material.h.

#include "sigilcompose/Material.h"

#include "ComposeInternal.h" // ElementNode, for Element::fill(Material)

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
  enum class Kind : uint8_t { Linear, Radial, Sweep, Image, Blend };
  Kind kind = Kind::Linear;
  // Gradients: endpoints/center + radius or sweep degrees + ramp.
  SkPoint p0 = {0, 0}, p1 = {0, 0};
  float f0 = 0.0f, f1 = 0.0f; // radius / (startDeg, endDeg)
  std::vector<Stop> stops;
  SkTileMode tile = SkTileMode::kClamp;
  // Image: pointer identity + mapping.
  sk_sp<SkImage> image;
  SkTileMode tx = SkTileMode::kClamp, ty = SkTileMode::kClamp;
  SkMatrix local = SkMatrix::I();
  SkSamplingOptions sampling;
  // Blend: layer materials (recursive Material equality) + modes.
  std::vector<std::pair<Material, SkBlendMode>> layers;

  bool operator==(const Recipe &o) const {
    if (kind != o.kind)
      return false;
    switch (kind) {
    case Kind::Linear:
    case Kind::Radial:
    case Kind::Sweep:
      return p0 == o.p0 && p1 == o.p1 && f0 == o.f0 && f1 == o.f1 &&
             stops == o.stops && tile == o.tile;
    case Kind::Image:
      return image == o.image && tx == o.tx && ty == o.ty &&
             local == o.local && sampling == o.sampling;
    case Kind::Blend:
      return layers == o.layers;
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

// Build a shader from an sksl recipe: constants, then bound Outputs at their
// current value, then the auto-injected uTime/uResolution/uContentScale — the
// last three only when the effect actually declares them (assigning a uniform
// the effect lacks aborts in debug builds). `ctx` is null for the static build.
sk_sp<SkShader> Material::build(const Live &live, const PaintContext *ctx) {
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

  // The varying-input digest (constants are fixed per recipe; injected
  // values participate only when actually injected).
  std::vector<float> inputs;
  if (ctx) {
    inputs.reserve(live.binds.size() + 4);
    for (const auto &[name, out] : live.binds)
      inputs.push_back(out ? out->value() : 0.0f);
    if (injectTime) {
      double t = ctx->elapsedSeconds;
      if (live.timeQuantizeHz > 0)
        t = std::floor(t * live.timeQuantizeHz) / live.timeQuantizeHz;
      inputs.push_back((float)t);
    }
    if (injectScale)
      inputs.push_back(ctx->contentScale);
    if (injectRes) {
      inputs.push_back(ctx->size.width());
      inputs.push_back(ctx->size.height());
    }
    if (live.lastShader && inputs == live.lastInputs)
      return live.lastShader;
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
  if (ctx) {
    // Auto-injects are size-checked too: a user declaring `uniform float
    // uResolution` must not receive a float2 write (SkDEBUGFAIL).
    if (injectTime) {
      double t = ctx->elapsedSeconds;
      if (live.timeQuantizeHz > 0)
        t = std::floor(t * live.timeQuantizeHz) / live.timeQuantizeHz;
      b.uniform("uTime") = (float)t;
    }
    if (injectScale)
      b.uniform("uContentScale") = ctx->contentScale;
    if (injectRes)
      b.uniform("uResolution") =
          std::array<float, 2>{ctx->size.width(), ctx->size.height()};
  }
  sk_sp<SkShader> built = b.makeShader();
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

Material Material::sweep(SkPoint center, std::vector<Stop> stops, float startDeg,
                         float endDeg) {
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

Material Material::sksl(sk_sp<SkRuntimeEffect> effect,
                        std::vector<std::pair<std::string, float>> constants) {
  Material m;
  if (!effect)
    return m;
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
    acc = SkShaders::Blend(layers[i].second, std::move(acc), std::move(src));
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
  if (m_isSolid != o.m_isSolid)
    return false;
  if (m_isSolid)
    return m_solid == o.m_solid;
  // sksl-backed: static recipes compare structurally (effect pointer +
  // constant values); live ones by identity — conservative, they never prune.
  if ((m_live != nullptr) != (o.m_live != nullptr))
    return false;
  if (m_live) {
    if (isLive() || o.isLive())
      return m_live == o.m_live;
    return m_live->effect == o.m_live->effect &&
           m_live->constants == o.m_live->constants &&
           m_live->constants2 == o.m_live->constants2 &&
           m_live->constants4 == o.m_live->constants4;
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

bool Material::isLive() const {
  if (m_live &&
      (!m_live->binds.empty() || m_live->usesTime || m_live->usesScale))
    return true;
  // A blend inherits liveness from its layers (deferred fold in resolve()).
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto &layer : m_recipe->layers)
      if (layer.first.isLive())
        return true;
  return false;
}

bool Material::geometryDependent() const {
  if (m_live && m_live->usesGeometry)
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

sk_sp<SkShader> Material::asShader() const {
  // A live material's m_shader snapshot predates its binds — rebuild fresh so
  // bound Outputs contribute their CURRENT values (what blend() flattens; the
  // Fable audit's stale-snapshot defect).
  if (isLive())
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
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend &&
      (isLive() || geometryDependent())) {
    sk_sp<SkShader> acc;
    bool first = true;
    for (const auto &[mat, mode] : m_recipe->layers) {
      Fill f = mat.resolve(ctx);
      sk_sp<SkShader> src;
      if (f.kind == Fill::Kind::Shader)
        src = f.shaderValue;
      else if (f.kind == Fill::Kind::Color)
        src = SkShaders::Color(f.colorValue, nullptr);
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
      acc = SkShaders::Blend(mode, std::move(acc), std::move(src));
    }
    return Fill::shader(std::move(acc));
  }
  if (isLive() || geometryDependent())
    return Fill::shader(build(*m_live, &ctx));
  return toFill();
}

Element &Element::textFill(Material m) {
  m_node->textMetricFill = std::move(m);
  return *this;
}

Element &Element::fill(Material m) {
  if (m.isLive() || m.geometryDependent()) {
    // Live materials re-resolve per frame; geometry-dependent ones resolve
    // when the node records (and re-record on size change) — both route
    // through the material slot so the painter resolves with PaintContext.
    m_node->liveMaterial = std::move(m);
    m_node->paint.fill.reset();
    m_node->staticMaterial.reset();
  } else {
    m_node->paint.fill = PropValue<Fill>{m.toFill()};
    m_node->staticMaterial = std::move(m); // the prune signature
    m_node->liveMaterial.reset();
  }
  return *this;
}

} // namespace sigil::compose
