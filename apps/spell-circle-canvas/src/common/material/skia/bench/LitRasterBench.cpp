/** @file
 * Resolve a moving lighting pass and rasterize its rectangle. Immutable
 * inputs stay prepared across draws. The mapped-normal arms use the
 * analytic environment; EnvironmentTexture changes only its source from
 * the matching ProceduralEnvironment arm.
 */

#include <benchmark/benchmark.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/vec2.hpp>
#include <optional>
#include <utility>
#include <vector>

using namespace sigil::material;

namespace {

enum class Shading {
  Flat,
  TwoPoints,
  CurvedNormal,
  ProceduralEnvironment,
  EnvironmentTexture,
  PaintedHeightNormal,
  BlendedNormals,
  Clearcoat,
};

struct Extent {
  glm::vec2 uExtent;
};

constexpr const char* kCurvedNormal = R"(
half4 main(float2 p) {
  float2 q = p / uExtent - 0.5;
  float3 n = normalize(float3(q.x * 1.3, q.y * 0.4, 1.0));
  return half4(n * 0.5 + 0.5, 1.0);
}
)";

constexpr const char* kRoom = R"(
half4 main(float2 p) {
  float2 q = p / uExtent;
  float strip = abs(fract(q.x + 0.18) - 0.5);
  float side = abs(fract(q.x - 0.26) - 0.5);
  float height = q.y - 0.43;
  float warm = exp(-strip * strip * 1500.0 - height * height * 12.0);
  float cool = exp(-side * side * 220.0 - height * height * 5.0);
  return half4(float3(0.035, 0.045, 0.06)
                   + warm * float3(1.6, 1.3, 0.8)
                   + cool * float3(0.2, 0.45, 0.9), 1.0);
}
)";

sk_sp<SkImage> paintedHeight(int width, int height) {
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(width, height)))
    return nullptr;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const float u = (float(x) + 0.5f) / float(width);
      const float v = (float(y) + 0.5f) / float(height);
      const float stroke = u - (0.2f + v * 0.5f);
      const float dabX = u - 0.68f, dabY = v - 0.4f;
      const float heightValue =
          0.08f + 0.6f * std::exp(-stroke * stroke * 160.0f) +
          0.25f * std::exp(-(dabX * dabX + dabY * dabY) * 50.0f);
      const uint8_t shade = uint8_t(std::clamp(heightValue, 0.0f, 1.0f) * 255);
      *bitmap.getAddr32(x, y) = SkColorSetRGB(shade, shade, shade);
    }
  bitmap.setImmutable();
  return bitmap.asImage();
}

sk_sp<SkImage> rasterPanorama(const Material& room) {
  constexpr int kWidth = 512, kHeight = 256;
  const sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::Make(
      kWidth, kHeight, kRGBA_F32_SkColorType, kPremul_SkAlphaType));
  if (!surface) return nullptr;
  const FrameData frame{.resolution = {float(kWidth), float(kHeight)}};
  const sk_sp<SkShader> shader = skia::shader(skia::paint(room), frame);
  if (!shader) return nullptr;
  SkPaint paint;
  paint.setShader(shader);
  surface->getCanvas()->drawRect(SkRect::MakeWH(kWidth, kHeight), paint);
  return surface->makeImageSnapshot();
}

void LitRaster(benchmark::State& state, Shading shading) {
  const int width = int(state.range(0)), height = int(state.range(1));
  const glm::vec2 extent{float(width), float(height)};
  const sk_sp<SkSurface> target =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  if (!target) {
    state.SkipWithError("Cannot allocate the raster target");
    return;
  }

  const Material curved = shader(kCurvedNormal, Extent{extent});
  const Material room = shader(kRoom, Extent{{512.0f, 256.0f}});
  std::optional<Material> normal;
  if (shading == Shading::CurvedNormal ||
      shading == Shading::ProceduralEnvironment ||
      shading == Shading::EnvironmentTexture)
    normal = curved;
  if (shading == Shading::PaintedHeightNormal ||
      shading == Shading::BlendedNormals || shading == Shading::Clearcoat) {
    const sk_sp<SkImage> heightImage = paintedHeight(width, height);
    if (!heightImage) {
      state.SkipWithError("Cannot allocate the fixed height image");
      return;
    }
    Material painted = surface::normalFromHeight(
        image(heightImage), {.depth = float(width) * 0.025f, .step = 1});
    normal = shading == Shading::PaintedHeightNormal
                 ? std::move(painted)
                 : surface::blendNormals(curved, std::move(painted));
  }

  Material material = from(Color{0.56f, 0.52f, 0.46f, 1});
  if (shading != Shading::Flat)
    material.surface(
        {.metallic = 0.9f,
         .roughness = 0.18f,
         .normal = normal,
         .clearcoat = shading == Shading::Clearcoat ? 0.8f : 0.0f});
  const skia::LitSurface prepared(material);

  Lighting lighting(std::vector<Light>{
      Light{.color = Color{1, 0.86f, 0.65f, 1},
            .intensity = 0.5f,
            .ambient = 0.04f,
            .kind = LightKind::Point,
            .position = {extent.x * 0.22f, extent.y * 0.12f, extent.x * 0.6f},
            .range = extent.x * 2},
      Light{.color = Color{0.4f, 0.7f, 1, 1},
            .intensity = 0.35f,
            .ambient = 0.04f,
            .kind = LightKind::Point,
            .position = {extent.x * 0.8f, extent.y * 0.8f, extent.x * 0.5f},
            .range = extent.x * 2}});
  lighting.frame = LightingFrame::Scene;
  if (shading == Shading::ProceduralEnvironment ||
      shading == Shading::PaintedHeightNormal ||
      shading == Shading::BlendedNormals || shading == Shading::Clearcoat)
    lighting.environment =
        environment(room, {.intensity = 0.45f, .size = {512, 256}});
  if (shading == Shading::EnvironmentTexture) {
    const sk_sp<SkImage> panorama = rasterPanorama(room);
    if (!panorama) {
      state.SkipWithError("Cannot rasterize the environment shader");
      return;
    }
    lighting.environment =
        environment(image(Texture(panorama).tile(Repeat::Repeat, Repeat::Pad)),
                    {.intensity = 0.45f, .size = {512, 256}});
  }

  const FrameData frame{.resolution = extent};
  const SkRect rectangle = SkRect::MakeWH(extent.x, extent.y);
  const auto draw = [&] {
    const Paint pass = prepared.under(lighting);
    if (shading != Shading::Flat && pass.isSolid()) return false;
    const sk_sp<SkShader> resolved =
        pass.isSolid() ? skia::shader(pass) : skia::shader(pass, frame);
    if (!resolved) return false;
    SkPaint paint;
    paint.setShader(resolved);
    target->getCanvas()->drawRect(rectangle, paint);
    SkPixmap pixels;
    if (!target->peekPixels(&pixels)) return false;
    auto center = *pixels.addr32(width / 2, height / 2);
    benchmark::DoNotOptimize(center);
    benchmark::ClobberMemory();
    return true;
  };
  // Compile, resolve and execute each arm before the timed draws.
  for (int warm = 0; warm < 3; ++warm)
    if (!draw()) {
      state.SkipWithError("Cannot resolve or read the raster shading pass");
      return;
    }
  uint64_t iteration = 0;
  for ([[maybe_unused]] auto sample : state) {
    lighting.lights.front().position.x =
        extent.x * (0.16f + 0.12f * float(iteration++ % 256) / 255.0f);
    if (!draw()) {
      state.SkipWithError("Cannot resolve or read the raster shading pass");
      break;
    }
  }
  state.SetItemsProcessed(state.iterations() * int64_t(width) * height);
}

enum class Assembly { Directional, BoundDirectional, MappedMixed };

// Measure pass assembly separately from raster shading. The effect variant
// and all immutable material inputs are prepared before timing starts.
void LitAssembly(benchmark::State& state, Assembly assembly) {
  const int count = int(state.range(0));
  const bool mapped = assembly == Assembly::MappedMixed;
  const Material normal = shader(kCurvedNormal, Extent{{128, 64}});
  const Material room = shader(kRoom, Extent{{512, 256}});
  Material material = from(Color{0.56f, 0.52f, 0.46f, 1});
  material.surface(
      {.metallic = 0.9f,
       .roughness = 0.18f,
       .normal = mapped ? std::optional<Material>(normal) : std::nullopt});
  const skia::LitSurface prepared(material);
  auto strength = sigil::motion::animatable(0.4f);
  Lighting lighting;
  for (int i = 0; i < count; ++i) {
    Light light{.direction = 30.0f * float(i),
                .color = Color{1, 0.86f, 0.65f, 1},
                .intensity = 0.4f,
                .ambient = 0.04f};
    if (assembly == Assembly::BoundDirectional) light.intensity = strength;
    if (mapped) {
      light.kind = i % 3 == 0   ? LightKind::Directional
                   : i % 3 == 1 ? LightKind::Point
                                : LightKind::Spot;
      light.position = {64, 32, 80};
      light.range = 256;
    }
    lighting.lights.push_back(std::move(light));
  }
  if (mapped) {
    lighting.frame = LightingFrame::Scene;
    lighting.environment =
        environment(room, {.intensity = 0.45f, .size = {512, 256}});
  }
  for (int warm = 0; warm < 3; ++warm)
    if (!skia::staticShader(prepared.under(lighting))) {
      state.SkipWithError("Cannot assemble the lighting pass");
      return;
    }
  uint64_t iteration = 0;
  for ([[maybe_unused]] auto sample : state) {
    const float value = 0.3f + float(iteration++ % 256) / 1280.0f;
    if (assembly == Assembly::BoundDirectional)
      strength = value;
    else
      lighting.lights.front().intensity = value;
    Paint pass = prepared.under(lighting);
    benchmark::DoNotOptimize(pass);
    benchmark::DoNotOptimize(skia::staticShader(pass).get());
  }
  state.counters["lights"] = count;
  state.SetItemsProcessed(state.iterations());
}

}  // namespace

BENCHMARK_CAPTURE(LitRaster, Flat, Shading::Flat)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, TwoPoints, Shading::TwoPoints)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, CurvedNormal, Shading::CurvedNormal)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, ProceduralEnvironment,
                  Shading::ProceduralEnvironment)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, EnvironmentTexture, Shading::EnvironmentTexture)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, PaintedHeightNormal, Shading::PaintedHeightNormal)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, BlendedNormals, Shading::BlendedNormals)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);
BENCHMARK_CAPTURE(LitRaster, Clearcoat, Shading::Clearcoat)
    ->Args({266, 80})
    ->Args({1064, 320})
    ->Unit(benchmark::kMillisecond);

BENCHMARK_CAPTURE(LitAssembly, Directional, Assembly::Directional)
    ->Arg(1)
    ->Arg(4)
    ->Arg(16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(LitAssembly, BoundDirectional, Assembly::BoundDirectional)
    ->Arg(1)
    ->Arg(4)
    ->Arg(16)
    ->Unit(benchmark::kMicrosecond);
BENCHMARK_CAPTURE(LitAssembly, MappedMixed, Assembly::MappedMixed)
    ->Arg(1)
    ->Arg(4)
    ->Arg(16)
    ->Unit(benchmark::kMicrosecond);
