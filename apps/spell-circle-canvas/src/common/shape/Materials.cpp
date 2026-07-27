#include "sigilshape/Materials.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkM44.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace sigil::shape::materials {

namespace {

// ---------------------------------------------------------------------------
// SkSL

/** Shared helpers: normal decode (child carries the device alignment in
 *  its local matrix) and equirect environment lookup. Normals are
 *  DEVICE-space, +y down, +z toward the viewer. */
constexpr char kPrelude[] = R"(
uniform shader uNormals;
uniform shader uEnv;
uniform float2 uEnvSize;

float3 readNormal(float2 xy) {
  half4 t = uNormals.eval(xy);
  float3 n = float3(t.rgb) * 2.0 - 1.0;
  float len = length(n);
  return len < 0.001 ? float3(0.0, 0.0, 1.0) : n / len;
}

float2 envUv(float3 dir) {
  float u = atan(dir.x, dir.z) / 6.2831853 + 0.5;
  float v = acos(clamp(-dir.y, -1.0, 1.0)) / 3.1415927;
  return float2(u, v);
}

float3 envAt(float2 uv) {
  return float3(uEnv.eval(uv * uEnvSize).rgb);
}

float3 envSample(float3 dir) {
  return envAt(envUv(dir));
}

float hash21(float2 p) {
  return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float vnoise(float2 p) {
  float2 i = floor(p);
  float2 f = fract(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hash21(i);
  float b = hash21(i + float2(1.0, 0.0));
  float c = hash21(i + float2(0.0, 1.0));
  float d = hash21(i + float2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(float2 p) {
  return vnoise(p) * 0.55 + vnoise(p * 2.13) * 0.3 +
         vnoise(p * 4.41) * 0.15;
}
)";

constexpr char kGold[] = R"(
uniform float4 uTint;
uniform float  uCrinkle;
uniform float  uCrinkleScale;
uniform float  uSparkle;
uniform float  uAmbient;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // Foil crinkle: fbm gradient folded into the normal field.
  float2 np = xy * uCrinkleScale;
  float e = 0.45;
  float gx = fbm(np + float2(e, 0.0)) - fbm(np - float2(e, 0.0));
  float gy = fbm(np + float2(0.0, e)) - fbm(np - float2(0.0, e));
  n = normalize(float3(n.x + gx * uCrinkle * 2.2,
                       n.y + gy * uCrinkle * 2.2, n.z));

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 env = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);
  float3 f0 = uTint.rgb;
  // Metals tint their reflection; fresnel whitens the grazing rim.
  float3 spec = env * mix(f0, float3(1.0), fres);
  float3 col = f0 * uAmbient + spec;

  // Glints: sparse hash cells, gated to lit wrinkles.
  float cell = hash21(floor(xy * 0.7));
  float lum = dot(env, float3(0.299, 0.587, 0.114));
  float glint = step(0.992, cell) * uSparkle * smoothstep(0.35, 1.2, lum);
  col += float3(glint);
  return half4(half3(col), 1.0);
}
)";

constexpr char kChrome[] = R"(
uniform float4 uTint;
uniform float  uContrast;
uniform float  uBrushed;
uniform float  uFresnel;
uniform float  uExposure;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float2 uv = envUv(R);

  // Brushed steel: smear the lookup along the azimuth.
  float3 env = float3(0.0);
  if (uBrushed > 0.001) {
    float total = 0.0;
    for (int k = -3; k <= 3; ++k) {
      float w = 1.0 - abs(float(k)) / 4.0;
      float du = float(k) * uBrushed * 0.02;
      // Jitter breaks the 7-tap banding into grain — the brushed tell.
      du += (hash21(xy + float(k)) - 0.5) * uBrushed * 0.012;
      env += envAt(float2(fract(uv.x + du), uv.y)) * w;
      total += w;
    }
    env /= total;
  } else {
    env = envAt(uv);
  }

  // The chrome move: crush the environment's midtones.
  env = env * uExposure;
  env = (env - 0.5) * uContrast + 0.5;
  env = max(env, float3(0.0));

  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float face = mix(1.0 - uFresnel, 1.0, pow(1.0 - ndv, 3.0));
  float3 col = env * uTint.rgb * (0.55 + 0.45 * face) ;
  col += float3(0.9) * pow(1.0 - ndv, 6.0); // hot silhouette rim
  return half4(half3(col), 1.0);
}
)";

constexpr char kGlass[] = R"(
uniform shader uBackdrop;
uniform float4 uTint;
uniform float  uRefract;
uniform float  uReflect;
uniform float  uEdgeGlow;
uniform float  uOpacity;

half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // The bevel bends the view ray; the flat interior passes straight
  // through, so refraction reads at the rim — the lens look.
  float2 offset = n.xy * uRefract;
  float3 bg = float3(uBackdrop.eval(xy + offset).rgb);

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 env = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);

  float3 trans = bg * uTint.rgb;
  float mixv = clamp(uReflect * (0.06 + 0.94 * fres), 0.0, 1.0);
  float3 col = mix(trans, env, mixv);
  col += uEdgeGlow * pow(1.0 - ndv, 2.5) * float3(1.0);
  return half4(half3(col), 1.0) * uOpacity;
}
)";

sk_sp<SkRuntimeEffect> makeEffect(const char *body) {
  const std::string src = std::string(kPrelude) + body;
  auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(src.c_str()));
  if (!fx)
    SkDebugf("sigilshape material shader: %s\n", err.c_str());
  return fx;
}

const sk_sp<SkRuntimeEffect> &goldEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kGold);
  return fx;
}
const sk_sp<SkRuntimeEffect> &chromeEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kChrome);
  return fx;
}
const sk_sp<SkRuntimeEffect> &glassEffect() {
  static const sk_sp<SkRuntimeEffect> fx = makeEffect(kGlass);
  return fx;
}

// ---------------------------------------------------------------------------
// Environment bakes

sk_sp<SkImage> bakeEquirect(int width,
                            const std::function<SkV3(float u, float v)> &fn) {
  const int height = std::max(width / 2, 8);
  std::vector<float> pixels((size_t)width * height * 4);
  for (int y = 0; y < height; ++y) {
    const float v = ((float)y + 0.5f) / (float)height;
    for (int x = 0; x < width; ++x) {
      const float u = ((float)x + 0.5f) / (float)width;
      const SkV3 c = fn(u, v);
      float *px = &pixels[((size_t)y * width + x) * 4];
      px[0] = c.x;
      px[1] = c.y;
      px[2] = c.z;
      px[3] = 1;
    }
  }
  const SkImageInfo info = SkImageInfo::Make(
      width, height, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  return SkImages::RasterFromPixmapCopy(
      {info, pixels.data(), (size_t)width * 4 * sizeof(float)});
}

float gauss(float x, float sigma) {
  return std::exp(-(x * x) / (2 * sigma * sigma));
}

/** Angular distance on the equirect u axis (wraps). */
float du(float a, float b) {
  const float d = std::abs(a - b);
  return std::min(d, 1.0f - d);
}

} // namespace

Environment Environment::studio(int width) {
  Environment env;
  env.m_base = bakeEquirect(width, [](float u, float v) -> SkV3 {
    // Graded neutral shell: bright zenith, dim floor.
    const float sky = std::pow(std::clamp(1.0f - v, 0.0f, 1.0f), 1.4f);
    SkV3 c = {0.10f + 0.55f * sky, 0.11f + 0.56f * sky,
              0.13f + 0.60f * sky};
    // Floor bounce card below the horizon.
    if (v > 0.62f) {
      const float f = gauss(v - 0.78f, 0.10f) * 0.5f;
      c += {f * 0.9f, f * 0.85f, f * 0.75f};
    }
    // Three softboxes: key, fill, rim strip.
    const float key =
        gauss(du(u, 0.30f), 0.055f) * gauss(v - 0.30f, 0.09f) * 3.2f;
    const float fill =
        gauss(du(u, 0.72f), 0.075f) * gauss(v - 0.38f, 0.12f) * 1.1f;
    const float rim =
        gauss(du(u, 0.02f), 0.02f) * gauss(v - 0.22f, 0.20f) * 2.2f;
    c += {key + fill + rim, key + fill + rim, key + fill + rim};
    return c;
  });
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

Environment Environment::sunset(int width) {
  Environment env;
  env.m_base = bakeEquirect(width, [](float u, float v) -> SkV3 {
    const float horizon = 0.52f;
    if (v < horizon) {
      // Banded sky falling toward a hot horizon stripe.
      const float t = v / horizon; // 0 zenith -> 1 horizon
      SkV3 top = {0.05f, 0.10f, 0.30f};
      SkV3 low = {1.05f, 0.45f, 0.15f};
      SkV3 c = top + (low - top) * std::pow(t, 1.6f);
      // The classic chrome bands.
      const float band =
          0.5f + 0.5f * std::sin(t * 40.0f + std::cos(t * 13.0f));
      c *= 0.82f + 0.18f * band * std::pow(t, 2.0f);
      // Sun blob.
      const float sun =
          gauss(du(u, 0.5f), 0.035f) * gauss(v - horizon + 0.06f, 0.045f);
      c += SkV3{2.6f, 1.6f, 0.7f} * sun;
      // Horizon flare line.
      c += SkV3{1.6f, 0.9f, 0.45f} * gauss(v - horizon, 0.008f);
      return c;
    }
    // Ground: near-black violet falling away.
    const float t = (v - horizon) / (1.0f - horizon);
    SkV3 c = {0.16f, 0.05f, 0.16f};
    c = c * (1.0f - t * 0.8f);
    c += SkV3{0.5f, 0.2f, 0.4f} * gauss(v - horizon, 0.02f);
    return c;
  });
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

Environment Environment::fromEquirect(sk_sp<SkImage> image) {
  Environment env;
  env.m_base = std::move(image);
  env.m_blurs = std::make_shared<std::map<int, sk_sp<SkImage>>>();
  return env;
}

namespace {

/** Three-pass box blur ~= gaussian, run on F32 pixels with horizontal
 *  WRAP (an equirect's u axis is periodic — Skia's blur filter can't
 *  know that) and vertical clamp. */
void boxBlurF32(std::vector<float> &pixels, int w, int h, int radius) {
  if (radius < 1)
    return;
  std::vector<float> tmp(pixels.size());
  const int window = radius * 2 + 1;
  for (int pass = 0; pass < 3; ++pass) {
    // Horizontal, wrapped.
    for (int y = 0; y < h; ++y) {
      float *row = &pixels[(size_t)y * w * 4];
      float acc[4] = {0, 0, 0, 0};
      for (int k = -radius; k <= radius; ++k) {
        const int x = ((k % w) + w) % w;
        for (int c = 0; c < 4; ++c)
          acc[c] += row[x * 4 + c];
      }
      float *out = &tmp[(size_t)y * w * 4];
      for (int x = 0; x < w; ++x) {
        for (int c = 0; c < 4; ++c)
          out[x * 4 + c] = acc[c] / (float)window;
        const int drop = (((x - radius) % w) + w) % w;
        const int add = (x + radius + 1) % w;
        for (int c = 0; c < 4; ++c)
          acc[c] += row[add * 4 + c] - row[drop * 4 + c];
      }
    }
    // Vertical, clamped.
    for (int x = 0; x < w; ++x) {
      float acc[4] = {0, 0, 0, 0};
      for (int k = -radius; k <= radius; ++k) {
        const int y = std::clamp(k, 0, h - 1);
        for (int c = 0; c < 4; ++c)
          acc[c] += tmp[((size_t)y * w + x) * 4 + c];
      }
      for (int y = 0; y < h; ++y) {
        for (int c = 0; c < 4; ++c)
          pixels[((size_t)y * w + x) * 4 + c] = acc[c] / (float)window;
        const int drop = std::clamp(y - radius, 0, h - 1);
        const int add = std::clamp(y + radius + 1, 0, h - 1);
        for (int c = 0; c < 4; ++c)
          acc[c] += tmp[((size_t)add * w + x) * 4 + c] -
                    tmp[((size_t)drop * w + x) * 4 + c];
      }
    }
  }
}

} // namespace

sk_sp<SkImage> Environment::image(float roughness) const {
  if (!m_base)
    return nullptr;
  roughness = std::clamp(roughness, 0.0f, 1.0f);
  const int bucket = (int)std::lround(roughness * 8.0f);
  if (bucket == 0)
    return m_base;
  if (m_blurs) {
    if (auto it = m_blurs->find(bucket); it != m_blurs->end())
      return it->second;
  }
  const int w = m_base->width(), h = m_base->height();
  const SkImageInfo info = SkImageInfo::Make(
      w, h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  std::vector<float> pixels((size_t)w * h * 4);
  const SkPixmap pixmap(info, pixels.data(),
                        (size_t)w * 4 * sizeof(float));
  if (!m_base->readPixels(nullptr, pixmap, 0, 0))
    return m_base;
  // Box radius from the bucket: three passes triple the effective
  // spread, so keep the per-pass radius modest.
  const int radius = std::max(
      1, (int)std::lround(std::pow((float)bucket / 8.0f, 1.5f) *
                          (float)w * 0.045f));
  boxBlurF32(pixels, w, h, radius);
  sk_sp<SkImage> blurred = SkImages::RasterFromPixmapCopy(pixmap);
  if (!blurred)
    return m_base;
  if (m_blurs)
    (*m_blurs)[bucket] = blurred;
  return blurred;
}

sk_sp<SkImage> bevelNormals(const SkPath &path, SkIRect bounds,
                            float bevelPx, float heightScale) {
  const int w = std::max(bounds.width(), 1);
  const int h = std::max(bounds.height(), 1);

  // Coverage, blurred into a bevel ramp.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface)
    return nullptr;
  SkCanvas *canvas = surface->getCanvas();
  canvas->clear(SK_ColorBLACK);
  SkPaint white;
  white.setAntiAlias(true);
  white.setColor(SK_ColorWHITE);
  if (bevelPx > 0.01f)
    white.setImageFilter(
        SkImageFilters::Blur(bevelPx * 0.5f, bevelPx * 0.5f, nullptr));
  canvas->save();
  canvas->translate(-(float)bounds.left(), -(float)bounds.top());
  canvas->drawPath(path, white);
  canvas->restore();

  SkBitmap ramp;
  ramp.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  if (!surface->readPixels(ramp.pixmap(), 0, 0))
    return nullptr;

  // Height with a smoothstep shoulder, then Sobel -> normals.
  std::vector<float> height((size_t)w * h);
  for (int y = 0; y < h; ++y) {
    const uint32_t *row = ramp.getAddr32(0, y);
    for (int x = 0; x < w; ++x) {
      const float c = (float)((row[x] >> SK_R32_SHIFT) & 0xff) / 255.0f;
      height[(size_t)y * w + x] = c * c * (3.0f - 2.0f * c);
    }
  }

  SkBitmap out;
  out.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
  const float steep = std::max(bevelPx, 1.0f) * heightScale;
  auto at = [&](int x, int y) {
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    return height[(size_t)y * w + x];
  };
  for (int y = 0; y < h; ++y) {
    uint32_t *row = (uint32_t *)out.getAddr32(0, y);
    for (int x = 0; x < w; ++x) {
      const float dx = (at(x + 1, y) - at(x - 1, y)) * 0.5f * steep;
      const float dy = (at(x, y + 1) - at(x, y - 1)) * 0.5f * steep;
      float nx = -dx, ny = -dy, nz = 1.0f;
      const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      nx /= len;
      ny /= len;
      nz /= len;
      const auto enc = [](float f) {
        return (uint32_t)std::clamp((int)std::lround((f * 0.5f + 0.5f) * 255.0f),
                                    0, 255);
      };
      row[x] = (0xffu << SK_A32_SHIFT) | (enc(nx) << SK_R32_SHIFT) |
               (enc(ny) << SK_G32_SHIFT) | (enc(nz) << SK_B32_SHIFT);
    }
  }
  out.setImmutable();
  return out.asImage();
}

namespace {

const SkSamplingOptions kLinear{SkFilterMode::kLinear};

sk_sp<SkShader> normalsChild(const sk_sp<SkImage> &normals, SkPoint origin) {
  if (!normals)
    return nullptr;
  const SkMatrix local = SkMatrix::Translate(origin.fX, origin.fY);
  return normals->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                             kLinear, local);
}

sk_sp<SkShader> envChild(const Environment &env, float roughness,
                         SkV2 *sizeOut) {
  sk_sp<SkImage> img = env.image(roughness);
  if (!img)
    return nullptr;
  *sizeOut = {(float)img->width(), (float)img->height()};
  // Repeat in u so azimuth wraps seamlessly.
  return img->makeShader(SkTileMode::kRepeat, SkTileMode::kClamp, kLinear);
}

} // namespace

sk_sp<SkShader> gold(sk_sp<SkImage> normals, const Environment &env,
                     SkPoint origin, const GoldParams &params) {
  const sk_sp<SkRuntimeEffect> &fx = goldEffect();
  if (!fx || !env.valid())
    return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") = SkV4{params.tint.fR, params.tint.fG, params.tint.fB,
                            params.tint.fA};
  b.uniform("uCrinkle") = params.crinkle;
  b.uniform("uCrinkleScale") =
      std::max(params.crinkleScale, 1e-4f) * 40.0f;
  b.uniform("uSparkle") = params.sparkle;
  b.uniform("uAmbient") = params.ambient;
  return b.makeShader();
}

sk_sp<SkShader> chrome(sk_sp<SkImage> normals, const Environment &env,
                       SkPoint origin, const ChromeParams &params) {
  const sk_sp<SkRuntimeEffect> &fx = chromeEffect();
  if (!fx || !env.valid())
    return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") = SkV4{params.tint.fR, params.tint.fG, params.tint.fB,
                            params.tint.fA};
  b.uniform("uContrast") = params.contrast;
  b.uniform("uBrushed") = params.brushed;
  b.uniform("uFresnel") = params.fresnel;
  b.uniform("uExposure") = params.exposure;
  return b.makeShader();
}

sk_sp<SkShader> glass(sk_sp<SkImage> normals, const Environment &env,
                      sk_sp<SkImage> backdrop, SkPoint origin,
                      const GlassParams &params) {
  const sk_sp<SkRuntimeEffect> &fx = glassEffect();
  if (!fx || !env.valid() || !backdrop)
    return nullptr;
  SkV2 envSize{1, 1};
  SkRuntimeShaderBuilder b(fx);
  b.child("uNormals") = normalsChild(normals, origin);
  b.child("uEnv") = envChild(env, params.roughness, &envSize);
  b.child("uBackdrop") =
      backdrop->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, kLinear);
  b.uniform("uEnvSize") = envSize;
  b.uniform("uTint") = SkV4{params.tint.fR, params.tint.fG, params.tint.fB,
                            params.tint.fA};
  b.uniform("uRefract") = params.refractPx;
  b.uniform("uReflect") = params.reflect;
  b.uniform("uEdgeGlow") = params.edgeGlow;
  b.uniform("uOpacity") = params.opacity;
  return b.makeShader();
}

namespace {

void drawMaterial(SkCanvas &canvas, const SkPath &path,
                  const sk_sp<SkShader> &shader) {
  if (!shader)
    return;
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setShader(shader);
  canvas.save();
  canvas.clipPath(path, true);
  canvas.drawPaint(paint);
  canvas.restore();
}

SkIRect materialBounds(const SkPath &path, float bevelPx) {
  SkRect b = path.computeTightBounds();
  b.outset(bevelPx + 2, bevelPx + 2);
  return b.roundOut();
}

} // namespace

void drawGold(SkCanvas &canvas, const SkPath &path, const Environment &env,
              float bevelPx, const GoldParams &params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               gold(std::move(normals), env,
                    {(float)bounds.left(), (float)bounds.top()}, params));
}

void drawChrome(SkCanvas &canvas, const SkPath &path,
                const Environment &env, float bevelPx,
                const ChromeParams &params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               chrome(std::move(normals), env,
                      {(float)bounds.left(), (float)bounds.top()}, params));
}

void drawGlass(SkCanvas &canvas, const SkPath &path, const Environment &env,
               sk_sp<SkImage> backdrop, float bevelPx,
               const GlassParams &params) {
  const SkIRect bounds = materialBounds(path, bevelPx);
  sk_sp<SkImage> normals = bevelNormals(path, bounds, bevelPx);
  drawMaterial(canvas, path,
               glass(std::move(normals), env, std::move(backdrop),
                     {(float)bounds.left(), (float)bounds.top()}, params));
}

} // namespace sigil::shape::materials
