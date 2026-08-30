/** @file
 * The shader factories and the one-call draws: each SkSL body compiled
 * once into a runtime effect, bound to a normal map and an environment
 * as its children, and handed back as a shader; the draw functions run
 * the whole pipeline for one path.
 */

#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>

#include <string>

#include "Sksl.h"
#include "sigilgeometry/material/Materials.h"

namespace sigil::geometry::materials {

using detail::kChrome;
using detail::kGlass;
using detail::kGold;
using detail::kPrelude;

namespace {

sk_sp<SkRuntimeEffect> makeEffect(const char* body) {
  const std::string src = std::string(kPrelude) + body;
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!fx) SkDebugf("sigilgeometry material shader: %s\n", err.c_str());
  return fx;
}

const sk_sp<SkRuntimeEffect>& goldEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kGold);
  return fx;
}
const sk_sp<SkRuntimeEffect>& chromeEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kChrome);
  return fx;
}
const sk_sp<SkRuntimeEffect>& glassEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kGlass);
  return fx;
}

const SkSamplingOptions kLinear{SkFilterMode::kLinear};

sk_sp<SkShader> normalsChild(const sk_sp<SkImage>& normals, SkPoint origin) {
  if (!normals) return nullptr;
  const SkMatrix local = SkMatrix::Translate(origin.fX, origin.fY);
  return normals->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, kLinear,
                             local);
}

sk_sp<SkShader> envChild(const Environment& env, float roughness,
                         SkV2* sizeOut) {
  sk_sp<SkImage> img = env.image(roughness);
  if (!img) return nullptr;
  *sizeOut = {(float)img->width(), (float)img->height()};
  // Repeat in u so azimuth wraps seamlessly.
  return img->makeShader(SkTileMode::kRepeat, SkTileMode::kClamp, kLinear);
}

}  // namespace

sk_sp<SkShader> gold(sk_sp<SkImage> normals, const Environment& env,
                     SkPoint origin, const GoldParams& params) {
  const sk_sp<SkRuntimeEffect>& fx = goldEffect();
  if (!fx || !env.valid()) return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") =
      SkV4{params.tint.fR, params.tint.fG, params.tint.fB, params.tint.fA};
  b.uniform("uCrinkle") = params.crinkle;
  b.uniform("uCrinkleScale") = std::max(params.crinkleScale, 1e-4f) * 40.0f;
  b.uniform("uSparkle") = params.sparkle;
  b.uniform("uAmbient") = params.ambient;
  return b.makeShader();
}

sk_sp<SkShader> chrome(sk_sp<SkImage> normals, const Environment& env,
                       SkPoint origin, const ChromeParams& params) {
  const sk_sp<SkRuntimeEffect>& fx = chromeEffect();
  if (!fx || !env.valid()) return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") =
      SkV4{params.tint.fR, params.tint.fG, params.tint.fB, params.tint.fA};
  b.uniform("uContrast") = params.contrast;
  b.uniform("uBrushed") = params.brushed;
  b.uniform("uFresnel") = params.fresnel;
  b.uniform("uExposure") = params.exposure;
  return b.makeShader();
}

sk_sp<SkShader> glass(sk_sp<SkImage> normals, const Environment& env,
                      sk_sp<SkImage> backdrop, SkPoint origin,
                      const GlassParams& params) {
  const sk_sp<SkRuntimeEffect>& fx = glassEffect();
  if (!fx || !env.valid() || !backdrop) return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.child("uBackdrop") =
      backdrop->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, kLinear);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") =
      SkV4{params.tint.fR, params.tint.fG, params.tint.fB, params.tint.fA};
  b.uniform("uRefract") = params.refractPx;
  b.uniform("uReflect") = params.reflect;
  b.uniform("uEdgeGlow") = params.edgeGlow;
  b.uniform("uOpacity") = params.opacity;
  return b.makeShader();
}

namespace {

void drawMaterial(SkCanvas& canvas, const SkPath& path,
                  const sk_sp<SkShader>& shader) {
  if (!shader) return;
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setShader(shader);
  canvas.save();
  canvas.clipPath(path, true);
  canvas.drawPaint(paint);
  canvas.restore();
}

SkIRect materialBounds(const SkPath& path, float bevelPx) {
  SkRect b = path.computeTightBounds();
  b.outset(bevelPx + 2, bevelPx + 2);
  return b.roundOut();
}

}  // namespace

void drawGold(SkCanvas& canvas, const SkPath& path, const Environment& env,
              float bevelPx, const GoldParams& params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               gold(std::move(normals), env,
                    {(float)bounds.left(), (float)bounds.top()}, params));
}

void drawChrome(SkCanvas& canvas, const SkPath& path, const Environment& env,
                float bevelPx, const ChromeParams& params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               chrome(std::move(normals), env,
                      {(float)bounds.left(), (float)bounds.top()}, params));
}

void drawGlass(SkCanvas& canvas, const SkPath& path, const Environment& env,
               sk_sp<SkImage> backdrop, float bevelPx,
               const GlassParams& params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               glass(std::move(normals), env, std::move(backdrop),
                     {(float)bounds.left(), (float)bounds.top()}, params));
}

}  // namespace sigil::geometry::materials
