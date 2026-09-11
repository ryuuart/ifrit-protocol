#pragma once

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace magi {

// The arrival field is computed once. The material samples the same data
// that its inverse reads, so CPU and GPU hashes cannot disagree about which
// cells have been reached. Skia's raw image shader preserves numeric channels.
inline constexpr float kRailScale = 0.085f;
inline constexpr float kArrivalScale = 16384.0f;

inline const char* kInfectionSrc = R"SKSL(
uniform float2 uResolution;
uniform float uFront;
uniform float2 uCells;
uniform float4 uPour;
uniform float4 uKey;
uniform shader uArrival;

float arrival(float2 cell) {
  return uArrival.eval(cell + 1.5).r;
}

half4 main(float2 xy) {
  float2 uv = xy / max(uResolution, float2(1.0));
  float2 cell = floor(uv * uCells);
  float2 sub = fract(uv * uCells);
  float on = step(arrival(cell), uFront);

  // The dark keyline occupies selected uninfected boundary cells.
  float key = 0.0;
  if (on < 0.5 && uArrival.eval(cell + 1.5).g > 0.5) {
    if (min(sub.x, 1.0 - sub.x) < 0.21) {
      float side = sub.x < 0.5 ? -1.0 : 1.0;
      key = max(key, step(arrival(cell + float2(side, 0.0)), uFront));
    }
    if (min(sub.y, 1.0 - sub.y) < 0.21) {
      float side = sub.y < 0.5 ? -1.0 : 1.0;
      key = max(key, step(arrival(cell + float2(0.0, side)), uFront));
    }
  }
  float alpha = max(on, key);
  return half4(half3(mix(uKey.rgb, uPour.rgb, on) * alpha), half(alpha));
}
)SKSL";

// Effect identity is part of a paint's identity. Hold one per sketch instance.
inline sk_sp<SkRuntimeEffect> infectionEffect() {
  auto [effect, error] =
      SkRuntimeEffect::MakeForShader(SkString(kInfectionSrc));
  if (!effect) SkDebugf("magi infection shader: %s\n", error.c_str());
  return effect;
}

inline float fract1(float v) { return v - std::floor(v); }
inline float h21(float x, float y) {
  float qx = fract1(x * 0.1031f);
  float qy = fract1(y * 0.1031f);
  float qz = qx;
  const float dot =
      qx * (qy + 33.33f) + qy * (qz + 33.33f) + qz * (qx + 33.33f);
  qx += dot;
  qy += dot;
  qz += dot;
  return fract1((qx + qy) * qz);
}

// Manhattan distance over two coarse anisotropic speed fields makes long
// orthogonal fingers. A per-cell mask leaves islands inside the pour.
inline float arrival(float x, float y, SkPoint seed) {
  const float distance = std::fabs(x - seed.fX) + std::fabs(y - seed.fY);
  const float horizontal = h21(std::floor(x * kRailScale), y);
  const float vertical = h21(x + 41.0f, std::floor(y * kRailScale) + 41.0f);
  const float grain = fract1(x * 0.7548777f + y * 0.5698403f + 0.137f);
  const float speed = std::max(horizontal, vertical);
  return (distance / (0.38f + 1.70f * speed * speed) + 1.7f * grain +
          (grain <= 0.11f ? 1e4f : 0.0f)) /
         kArrivalScale;
}

struct Arrivals {
  std::vector<std::pair<float, float>> cells;  // arrival, visible area weight
  sk_sp<SkShader> field;
  int columns = 0;
  int rows = 0;
  float total = 0;
};

inline Arrivals arrivalTable(SkRect box, float cell, SkPoint seed,
                             const SkPath& outline) {
  Arrivals result;
  result.columns = std::max(1, (int)std::ceil(box.width() / cell));
  result.rows = std::max(1, (int)std::ceil(box.height() / cell));
  const float width = box.width() / (float)result.columns;
  const float height = box.height() / (float)result.rows;
  SkBitmap values;
  values.allocPixels(SkImageInfo::Make(result.columns + 2, result.rows + 2,
                                       kRGBA_F32_SkColorType,
                                       kOpaque_SkAlphaType));
  // One border cell supports neighbour queries at the panel's outer edge.
  for (int y = -1; y <= result.rows; ++y) {
    for (int x = -1; x <= result.columns; ++x) {
      const float reach = arrival((float)x, (float)y, seed);
      auto* pixel = static_cast<float*>(values.getAddr(x + 1, y + 1));
      pixel[0] = reach;
      pixel[1] = h21((float)x + 5.5f, (float)y + 5.5f) > 0.42f ? 1.0f : 0.0f;
      pixel[2] = 0;
      pixel[3] = 1;
      if (x < 0 || y < 0 || x >= result.columns || y >= result.rows) continue;
      int inside = 0;
      for (int sy = 0; sy < 4; ++sy)
        for (int sx = 0; sx < 4; ++sx)
          if (outline.contains(
                  ((float)x + ((float)sx + 0.5f) * 0.25f) * width,
                  ((float)y + ((float)sy + 0.5f) * 0.25f) * height))
            ++inside;
      if (!inside) continue;
      const float weight = (float)inside / 16.0f;
      result.cells.emplace_back(reach, weight);
      result.total += weight;
    }
  }
  values.setImmutable();
  result.field =
      SkImages::RasterFromBitmap(values)->makeRawShader(SkSamplingOptions{});
  std::sort(result.cells.begin(), result.cells.end());
  return result;
}

inline float frontFor(const Arrivals& table, float fraction) {
  if (table.cells.empty() || table.total <= 0 || fraction <= 0) return -1;
  const float wanted = fraction * table.total;
  float covered = 0;
  for (size_t i = 0; i < table.cells.size(); ++i) {
    covered += table.cells[i].second;
    if (covered >= wanted) {
      if (i + 1 == table.cells.size()) return 1;
      return (table.cells[i].first + table.cells[i + 1].first) * 0.5f;
    }
  }
  return 1;
}

// At one pixel per cell the raster samples cell centres, excluding keylines.
// The square voting modules give each cell equal visible area.
inline float renderedCoverage(const sk_sp<SkRuntimeEffect>& effect,
                              const Arrivals& table, float front) {
  if (!effect || !table.field) return -1;
  auto surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(table.columns, table.rows));
  if (!surface) return -1;
  SkRuntimeShaderBuilder program(effect);
  program.uniform("uResolution") =
      SkV2{(float)table.columns, (float)table.rows};
  program.uniform("uCells") = SkV2{(float)table.columns, (float)table.rows};
  program.uniform("uFront") = front;
  program.uniform("uPour") = SkV4{1, 0, 0, 1};
  program.uniform("uKey") = SkV4{0, 0, 0, 1};
  program.child("uArrival") = table.field;
  SkPaint paint;
  paint.setShader(program.makeShader());
  surface->getCanvas()->drawPaint(paint);
  SkPixmap pixels;
  if (!surface->peekPixels(&pixels)) return -1;
  int infected = 0;
  for (int y = 0; y < table.rows; ++y)
    for (int x = 0; x < table.columns; ++x)
      if (SkColorGetR(pixels.getColor(x, y)) > 127) ++infected;
  return (float)infected / (float)(table.columns * table.rows);
}

}  // namespace magi
