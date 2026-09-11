/** @file
 * THE LEAVES A PAINT IS BUILT FROM: the solid, the raw shader wrap, the
 * material instance, the four gradients and the unit-square ramp they
 * share, the image and the caller-owned pixel buffer behind it, and the
 * SkSL runtime effect. Each factory mints the shader it can resolve
 * eagerly and the comparable recipe two independently built paints are
 * compared by.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <include/effects/SkGradient.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/core/Program.h>  // reportOnce
#include <sigilshaders/MaterialSkia.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "PaintInternal.h"

namespace sigil::material::skia {

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

// SkGradient's color/position spans are non-owning — keep `r` alive across the
// SkShaders::*Gradient call (the shader copies the stops during construction).
SkGradient makeGradient(const RampArrays& r, SkTileMode tile) {
  return SkGradient({{r.colors.data(), r.colors.size()},
                     {r.positions.data(), r.positions.size()},
                     tile},
                    {});
}

}  // namespace

Paint Paint::solid(SkColor4f color) {
  Paint m;
  m.m_isSolid = true;
  m.m_solid = color;
  return m;
}

Paint Paint::shader(sk_sp<SkShader> shader) {
  Paint m;
  m.m_shader = std::move(shader);
  return m;
}

Paint Paint::recipe(sigil::material::Material material) {
  Paint m;
  m.m_backed = std::make_shared<Backed>(Backed{std::move(material), {}});
  m.m_shader = m.buildBacked(nullptr);  // static snapshot
  return m;
}

Paint Paint::linear(SkPoint a, SkPoint b, std::vector<Stop> stops,
                    SkTileMode tile) {
  RampArrays r = split(stops);
  const SkPoint pts[2] = {a, b};
  Paint m = shader(SkShaders::LinearGradient(pts, makeGradient(r, tile)));
  auto rec = std::make_shared<Recipe>();
  rec->kind = Recipe::Kind::Linear;
  rec->p0 = a;
  rec->p1 = b;
  rec->stops = std::move(stops);
  rec->tile = tile;
  m.m_recipe = std::move(rec);
  return m;
}

Paint Paint::radial(SkPoint center, float radius, std::vector<Stop> stops,
                    SkTileMode tile) {
  RampArrays r = split(stops);
  Paint m =
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

Paint Paint::conical(SkPoint focus, float focusRadius, SkPoint center,
                     float radius, std::vector<Stop> stops, SkTileMode tile) {
  RampArrays r = split(stops);
  Paint m = shader(SkShaders::TwoPointConicalGradient(
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

Paint Paint::sweep(SkPoint center, std::vector<Stop> stops, float startDeg,
                   float endDeg) {
  // Skia's sweep CLAMPS outside [startDeg, endDeg] — it never wraps.
  // A window reaching past the circle (`sweep(c, stops, 90, 450)`, the
  // obvious hue-wheel-starting-at-red) paints the run before startDeg in
  // the first stop's flat colour, silently. The numbers only meet here, so
  // this is where the diagnostic lives — once per process.
  if (startDeg < 0.0f || endDeg > 360.0f) {
    static bool warnedSweepWindow = false;
    if (!warnedSweepWindow) {
      warnedSweepWindow = true;
      SkDebugf(
          "[material] skia::Paint::sweep(start %.1f, end %.1f): angles "
          "outside [0, 360] CLAMP, they do not wrap — no canvas angle "
          "ever reaches the part of the window past the circle, so that "
          "run paints in the nearest stop's flat colour. Rotate the "
          "stops into [0, 360] instead. (warned once)\n",
          startDeg, endDeg);
    }
  }
  RampArrays r = split(stops);
  Paint m = shader(SkShaders::SweepGradient(
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

Paint Paint::image(sk_sp<SkImage> image, SkTileMode tx, SkTileMode ty,
                   const SkMatrix& local, SkSamplingOptions sampling) {
  if (!image) return {};
  Paint m = shader(SkShaders::Image(image, tx, ty, sampling, &local));
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

Paint Paint::buffer(std::shared_ptr<PixelBuffer> source, SkTileMode tx,
                    SkTileMode ty, const SkMatrix& local,
                    SkSamplingOptions sampling) {
  if (!source) return {};
  sk_sp<SkImage> snapshot = source->image();
  if (!snapshot) return {};
  Paint m = shader(SkShaders::Image(snapshot, tx, ty, sampling, &local));
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

namespace detail {

Paint unitRamp(SkPoint a, SkPoint b, std::vector<Stop> stops, bool radial) {
  // The stop count is baked into the source and one effect is cached per
  // count. Generating per count leaves no arbitrary ceiling below the
  // uniform budget and avoids a uniform-guarded fixed-maximum loop.
  if (stops.empty()) return Paint::solid(SkColor4f{0, 0, 0, 0});
  if (stops.size() == 1) return Paint::solid(stops.front().color);
  constexpr size_t kMaxStops = 256;
  if (stops.size() > kMaxStops) {
    material::reportOnce(
        "ramp:stops",
        "a unit-space ramp takes at most " + std::to_string(kMaxStops) +
            " stops and was given " + std::to_string(stops.size()) +
            "; the rest are dropped, so the ramp ends at stop " +
            std::to_string(kMaxStops) + "'s colour rather than the last one");
    stops.resize(kMaxStops);
  }
  const size_t n = stops.size();

  struct Cached {
    size_t count;
    sk_sp<SkRuntimeEffect> effect;
  };
  static std::mutex mutex;
  static std::vector<Cached> cache;
  sk_sp<SkRuntimeEffect> fx;
  {
    const std::lock_guard lock(mutex);
    for (const Cached& cached : cache)
      if (cached.count == n) {
        fx = cached.effect;
        break;
      }
    if (!fx) {
      std::string declarations;
      std::string mixes;
      for (size_t i = 1; i < n; ++i) {
        declarations += "uniform float4 uC" + std::to_string(i) + ";\n";
        declarations += "uniform float uS" + std::to_string(i) + ";\n";
        const std::string previous = std::to_string(i - 1);
        const std::string current = std::to_string(i);
        mixes += "  col = mix(col, uC" + current + ", clamp((t - uS" +
                 previous + ") / max(uS" + current + " - uS" + previous +
                 ", 1e-6), 0.0, 1.0));\n";
      }
      std::string source(shaderSource("UnitRamp.sksl"));
      const auto insert = [&](std::string_view marker,
                              std::string_view replacement) {
        const size_t at = source.find(marker);
        if (at != std::string::npos)
          source.replace(at, marker.size(), replacement);
      };
      insert("// SIGIL_STOP_DECLARATIONS", declarations);
      insert("// SIGIL_STOP_MIXES", mixes);
      auto [effect, error] =
          SkRuntimeEffect::MakeForShader(SkString(source.c_str()));
      if (!effect) {
        SkDebugf("sigilmaterial unitRamp shader (%zu stops): %s\n", n,
                 error.c_str());
        return Paint::solid(stops.front().color);
      }
      fx = std::move(effect);
      cache.push_back({n, fx});
    }
  }

  Paint material = Paint::sksl(fx, {{"uRadial", radial ? 1.0f : 0.0f}});
  material.uniform("uA", std::array<float, 2>{a.x(), a.y()});
  material.uniform("uB", std::array<float, 2>{b.x(), b.y()});
  for (size_t i = 0; i < n; ++i) {
    material.uniform("uC" + std::to_string(i), stops[i].color);
    material.uniform("uS" + std::to_string(i), stops[i].pos);
  }
  return material;
}

}  // namespace detail

Paint Paint::linearUnit(SkPoint from01, SkPoint to01, std::vector<Stop> stops) {
  return detail::unitRamp(from01, to01, std::move(stops), false);
}

Paint Paint::radialUnit(SkPoint center01, float radius01,
                        std::vector<Stop> stops) {
  return detail::unitRamp(center01, {radius01, radius01}, std::move(stops),
                          true);
}

Paint Paint::glowUnit(SkPoint center01, float radius01,
                      std::vector<Stop> stops) {
  // Convert the radius from half-sides to half-diagonals of the unit square.
  return detail::unitRamp(center01,
                          {radius01 * 0.70710678f, radius01 * 0.70710678f},
                          std::move(stops), true);
}

Paint Paint::sksl(sk_sp<SkRuntimeEffect> effect,
                  std::vector<std::pair<std::string, float>> constants) {
  Paint m;
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
          "[material] skia::Paint::sksl(null effect): the material is NONE "
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
    putByName(m.m_live->constants, std::move(name), value);
  }
  m.m_live->usesTime = validUniform(m.m_live->effect, "uTime", sizeof(float));
  m.m_live->usesScale =
      validUniform(m.m_live->effect, "uContentScale", sizeof(float));
  m.m_live->usesGeometry =
      validUniform(m.m_live->effect, "uResolution", 2 * sizeof(float));
  m.m_shader = build(*m.m_live, nullptr);  // static snapshot (constants only)
  return m;
}

}  // namespace sigil::material::skia
