#pragma once

/** @file
 * ONE NOISE FIELD, READ AT A POINT.
 *
 * `Noise.h` answers a number for an INDEX: neighbouring indices are
 * unrelated, which is what a per-stamp jitter wants. A field answers a
 * number for a POSITION, and points near each other read near values,
 * which is what a displacement, a drift, a grain and a flow want. It is
 * built on the same `noise::lattice` mixer, so the two agree about what
 * a seed means.
 *
 * IT IS ONE VALUE WITH PROPS, not a header per kind. Perlin, simplex and
 * cellular noise are the `kind`; fBm is `octaves` with `gain` and
 * `lacunarity`; ridged and billowed noise is the `fold`; a tileable
 * field is a `period`; a warped one is `warp`. Every member is a plain
 * number or a small enumeration, so the value compares exactly and a
 * memo keyed on one can be skipped, and a look chosen once for a whole
 * sheet is one of these carried as a token rather than seven arguments
 * repeated at every call site.
 *
 * WHAT AGREES WITH WHAT. `FieldKind::Value` at one octave IS the
 * trilinear value noise `geometry::path::valueNoise` answers, to the
 * bit: the same lattice word, squeezed the same way, eased with the
 * same smoothstep, in the same [-1, 1]. Two other value noises in this
 * tree deliberately do NOT agree with it and must not be re-spelled as
 * it — `draw::NoiseField` has p5's shape (a cosine blend of the low 24
 * bits, in [0, 1)), and the kit's SkSL grain has a sine-fract hash,
 * which is not a good hash and is the right one there because it is the
 * same arithmetic on every device that can run the shader. Each of the
 * three seeds pictures stored as bytes; re-spelling any of them as
 * another re-rolls those pictures.
 */

#include <sigilcore/compute/Noise.h>

#include <cmath>
#include <cstdint>

namespace sigil::core::noise {

/** WHAT THE FIELD IS MADE OF between its lattice points.
 *
 *  The four differ in character, not in quality: value noise is blobby
 *  and cheap, gradient noise has the even, directionless texture Perlin
 *  wrote it for, simplex has the same character without the axis-aligned
 *  ridges a cubic lattice leaves at high frequency, and cellular noise
 *  is not smooth at all — it is the distance to the nearest of a field
 *  of scattered points, which is what reads as cracks, scales and
 *  stones. */
enum class FieldKind : uint8_t { Value, Gradient, Simplex, Worley };

/** WHAT EACH OCTAVE IS BENT BY BEFORE IT IS ADDED.
 *
 *  A fold is applied per octave and not to the sum, which is the whole
 *  point of it: folding first is what puts a crease in the field at
 *  every scale and makes the octaves above a crease follow it. */
enum class Fold : uint8_t {
  /** The octave as it is, in [-1, 1]. */
  None,
  /** Its magnitude — `|n|`, in [0, 1]. Creases upward at every zero
   *  crossing: billows, smoke, cloud. */
  Turbulence,
  /** The magnitude inverted — `1 - 2|n|`, in [-1, 1]. The same creases
   *  the other way up, which reads as ridges and eroded ranges. */
  Ridged,
};

/** ONE LATTICE CORNER, [0, 1]: `noise::lattice`'s word over the whole
 *  32-bit range. Not the low-24-bit squeeze `draw::NoiseField::corner`
 *  takes — this is the squeeze the signed value field is defined by, and
 *  the two fields differ because of it. */
[[nodiscard]] inline float latticeUnit(uint32_t seed, int x, int y, int z) {
  return (float)lattice(seed, x, y, z) / (float)0xFFFFFFFFu;
}

/** A lattice coordinate folded into [0, period), or left alone when the
 *  field does not tile. Negative coordinates fold the same way positive
 *  ones do, which is what makes the tiling seamless across the origin
 *  rather than mirrored at it. */
[[nodiscard]] inline int wrapLattice(int value, int period) {
  if (period <= 0) return value;
  const int folded = value % period;
  return folded < 0 ? folded + period : folded;
}

namespace detail {

/** The smoothstep every lattice-based kind eases a cell with: zero slope
 *  at both ends, so cells meet without a crease. */
[[nodiscard]] inline float ease(float t) { return t * t * (3.0f - 2.0f * t); }

/** Perlin's quintic fade: zero slope AND zero curvature at both ends,
 *  which a gradient field needs and a value field does not — a cell
 *  boundary in a gradient field shows as a visible line under the
 *  cubic. */
[[nodiscard]] inline float fade(float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

[[nodiscard]] inline float mix(float a, float b, float t) {
  return a + (b - a) * t;
}

/** The unit gradient a lattice cell pulls, in two dimensions: one of
 *  eight directions, spelled as exact floats so the field is the same
 *  number on every machine — a table of cosines would be the platform's
 *  libm rather than this repository's arithmetic. */
inline void gradient2(uint32_t seed, int x, int y, int period, float& gx,
                      float& gy) {
  // Half the square root of two, to a float's full precision: the
  // component of a unit vector on a diagonal.
  constexpr float d = 0.70710678f;
  static constexpr float kx[8] = {1.0f, d, 0.0f, -d, -1.0f, -d, 0.0f, d};
  static constexpr float ky[8] = {0.0f, d, 1.0f, d, 0.0f, -d, -1.0f, -d};
  const uint32_t h =
      lattice(seed, wrapLattice(x, period), wrapLattice(y, period), 0) & 7u;
  gx = kx[h];
  gy = ky[h];
}

/** The gradient a lattice cell pulls, in three dimensions: the twelve
 *  cube-edge directions of Perlin's improved noise, each of length the
 *  square root of two — the set every other implementation of this
 *  field uses, so a picture made here and one made elsewhere have the
 *  same character and the same amplitude. */
inline void gradient3(uint32_t seed, int x, int y, int z, int period, float& gx,
                      float& gy, float& gz) {
  static constexpr float kx[12] = {1, -1, 1, -1, 1, -1, 1, -1, 0, 0, 0, 0};
  static constexpr float ky[12] = {1, 1, -1, -1, 0, 0, 0, 0, 1, -1, 1, -1};
  static constexpr float kz[12] = {0, 0, 0, 0, 1, 1, -1, -1, 1, 1, -1, -1};
  const uint32_t h = lattice(seed, wrapLattice(x, period),
                             wrapLattice(y, period), wrapLattice(z, period)) %
                     12u;
  gx = kx[h];
  gy = ky[h];
  gz = kz[h];
}

/** Where the one feature point of a cell sits inside it, in [0, 1) on
 *  each axis. */
inline void featurePoint(uint32_t seed, int x, int y, int z, int period,
                         float& fx, float& fy, float& fz) {
  const int wx = wrapLattice(x, period), wy = wrapLattice(y, period),
            wz = wrapLattice(z, period);
  fx = pcgUnit(lattice(seed, wx, wy, wz));
  fy = pcgUnit(lattice(seed ^ 0x68bc21ebu, wx, wy, wz));
  fz = pcgUnit(lattice(seed ^ 0x02e5be93u, wx, wy, wz));
}

}  // namespace detail

/** TRILINEAR VALUE NOISE over the integer lattice, in [-1, 1].
 *
 *  Eight corner words, eased with a smoothstep on each axis. This is the
 *  body `geometry::path::valueNoise` answers, to the bit, and a
 *  two-dimensional reading of it is exactly this one with `z` at zero —
 *  the corners at `dz = 1` are weighted by nothing. */
[[nodiscard]] inline float valueNoise(uint32_t seed, float x, float y, float z,
                                      int period = 0) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y),
            zi = (int)std::floor(z);
  const float u = detail::ease(x - (float)xi), v = detail::ease(y - (float)yi),
              w = detail::ease(z - (float)zi);
  float accum = 0;
  for (int dz = 0; dz <= 1; ++dz)
    for (int dy = 0; dy <= 1; ++dy)
      for (int dx = 0; dx <= 1; ++dx) {
        const float weight =
            (dx ? u : 1 - u) * (dy ? v : 1 - v) * (dz ? w : 1 - w);
        accum += latticeUnit(seed, wrapLattice(xi + dx, period),
                             wrapLattice(yi + dy, period),
                             wrapLattice(zi + dz, period)) *
                 weight;
      }
  return accum * 2.0f - 1.0f;
}

/** PERLIN GRADIENT NOISE in two dimensions, in [-1, 1].
 *
 *  Zero at every lattice point and shaped by the direction each corner
 *  pulls, which is what removes the blobbiness of value noise. The
 *  interpolated dot product of unit gradients is bounded by half the
 *  square root of two, so the result is scaled by its reciprocal to
 *  reach the ends of the range. */
[[nodiscard]] inline float gradientNoise(uint32_t seed, float x, float y,
                                         int period = 0) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y);
  const float fx = x - (float)xi, fy = y - (float)yi;
  const float u = detail::fade(fx), v = detail::fade(fy);
  auto corner = [&](int dx, int dy) {
    float gx = 0, gy = 0;
    detail::gradient2(seed, xi + dx, yi + dy, period, gx, gy);
    return gx * (fx - (float)dx) + gy * (fy - (float)dy);
  };
  const float x0 = detail::mix(corner(0, 0), corner(1, 0), u);
  const float x1 = detail::mix(corner(0, 1), corner(1, 1), u);
  return detail::mix(x0, x1, v) * 1.41421356f;
}

/** PERLIN GRADIENT NOISE in three dimensions, within [-1, 1]. The same
 *  construction over eight corners, at the amplitude Perlin's improved
 *  noise has and every other implementation of it shares: unscaled, and
 *  approaching the ends of the range rather than reaching them. */
[[nodiscard]] inline float gradientNoise(uint32_t seed, float x, float y,
                                         float z, int period = 0) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y),
            zi = (int)std::floor(z);
  const float fx = x - (float)xi, fy = y - (float)yi, fz = z - (float)zi;
  const float u = detail::fade(fx), v = detail::fade(fy), w = detail::fade(fz);
  auto corner = [&](int dx, int dy, int dz) {
    float gx = 0, gy = 0, gz = 0;
    detail::gradient3(seed, xi + dx, yi + dy, zi + dz, period, gx, gy, gz);
    return gx * (fx - (float)dx) + gy * (fy - (float)dy) +
           gz * (fz - (float)dz);
  };
  const float x00 = detail::mix(corner(0, 0, 0), corner(1, 0, 0), u);
  const float x10 = detail::mix(corner(0, 1, 0), corner(1, 1, 0), u);
  const float x01 = detail::mix(corner(0, 0, 1), corner(1, 0, 1), u);
  const float x11 = detail::mix(corner(0, 1, 1), corner(1, 1, 1), u);
  return detail::mix(detail::mix(x00, x10, v), detail::mix(x01, x11, v), w);
}

/** SIMPLEX NOISE in two dimensions, in about [-1, 1].
 *
 *  The same gradients read on a triangular lattice instead of a square
 *  one: three corners contribute instead of four, and there is no
 *  direction along which the cell boundaries line up, which is the
 *  axis-aligned ridging a square lattice leaves at high frequency.
 *
 *  It does NOT tile. The skew that turns squares into triangles does not
 *  carry a Cartesian period through it, so a `period` is ignored for
 *  this kind rather than silently producing a seam. */
[[nodiscard]] inline float simplexNoise(uint32_t seed, float x, float y) {
  // The skew and unskew of an equilateral lattice: (sqrt(3) - 1) / 2 and
  // (3 - sqrt(3)) / 6.
  constexpr float kSkew = 0.36602540f, kUnskew = 0.21132487f;
  const float s = (x + y) * kSkew;
  const int i = (int)std::floor(x + s), j = (int)std::floor(y + s);
  const float t = (float)(i + j) * kUnskew;
  const float x0 = x - ((float)i - t), y0 = y - ((float)j - t);
  const int i1 = x0 > y0 ? 1 : 0, j1 = x0 > y0 ? 0 : 1;
  const float x1 = x0 - (float)i1 + kUnskew, y1 = y0 - (float)j1 + kUnskew;
  const float x2 = x0 - 1.0f + 2.0f * kUnskew, y2 = y0 - 1.0f + 2.0f * kUnskew;
  auto contribution = [&](int di, int dj, float dx, float dy) {
    float falloff = 0.5f - dx * dx - dy * dy;
    if (falloff <= 0.0f) return 0.0f;
    float gx = 0, gy = 0;
    detail::gradient2(seed, i + di, j + dj, 0, gx, gy);
    falloff *= falloff;
    return falloff * falloff * (gx * dx + gy * dy);
  };
  return 99.2f * (contribution(0, 0, x0, y0) + contribution(i1, j1, x1, y1) +
                  contribution(1, 1, x2, y2));
}

/** SIMPLEX NOISE in three dimensions, in about [-1, 1]: the same
 *  construction over a tetrahedral lattice, four corners contributing.
 *  It does not tile, for the reason the two-dimensional one does not. */
[[nodiscard]] inline float simplexNoise(uint32_t seed, float x, float y,
                                        float z) {
  constexpr float kSkew = 1.0f / 3.0f, kUnskew = 1.0f / 6.0f;
  const float s = (x + y + z) * kSkew;
  const int i = (int)std::floor(x + s), j = (int)std::floor(y + s),
            k = (int)std::floor(z + s);
  const float t = (float)(i + j + k) * kUnskew;
  const float x0 = x - ((float)i - t), y0 = y - ((float)j - t),
              z0 = z - ((float)k - t);

  int i1, j1, k1, i2, j2, k2;  // the two corners between the first and last
  if (x0 >= y0) {
    if (y0 >= z0) {
      i1 = 1;
      j1 = 0;
      k1 = 0;
      i2 = 1;
      j2 = 1;
      k2 = 0;
    } else if (x0 >= z0) {
      i1 = 1;
      j1 = 0;
      k1 = 0;
      i2 = 1;
      j2 = 0;
      k2 = 1;
    } else {
      i1 = 0;
      j1 = 0;
      k1 = 1;
      i2 = 1;
      j2 = 0;
      k2 = 1;
    }
  } else {
    if (y0 < z0) {
      i1 = 0;
      j1 = 0;
      k1 = 1;
      i2 = 0;
      j2 = 1;
      k2 = 1;
    } else if (x0 < z0) {
      i1 = 0;
      j1 = 1;
      k1 = 0;
      i2 = 0;
      j2 = 1;
      k2 = 1;
    } else {
      i1 = 0;
      j1 = 1;
      k1 = 0;
      i2 = 1;
      j2 = 1;
      k2 = 0;
    }
  }
  auto contribution = [&](int di, int dj, int dk, float dx, float dy,
                          float dz) {
    float falloff = 0.6f - dx * dx - dy * dy - dz * dz;
    if (falloff <= 0.0f) return 0.0f;
    float gx = 0, gy = 0, gz = 0;
    detail::gradient3(seed, i + di, j + dj, k + dk, 0, gx, gy, gz);
    falloff *= falloff;
    return falloff * falloff * (gx * dx + gy * dy + gz * dz);
  };
  return 32.0f *
         (contribution(0, 0, 0, x0, y0, z0) +
          contribution(i1, j1, k1, x0 - (float)i1 + kUnskew,
                       y0 - (float)j1 + kUnskew, z0 - (float)k1 + kUnskew) +
          contribution(i2, j2, k2, x0 - (float)i2 + 2.0f * kUnskew,
                       y0 - (float)j2 + 2.0f * kUnskew,
                       z0 - (float)k2 + 2.0f * kUnskew) +
          contribution(1, 1, 1, x0 - 1.0f + 3.0f * kUnskew,
                       y0 - 1.0f + 3.0f * kUnskew, z0 - 1.0f + 3.0f * kUnskew));
}

/** CELLULAR NOISE in two dimensions, in [-1, 1]: the distance to the
 *  nearest of one scattered point per cell, read as −1 AT a point and
 *  rising towards +1 away from every one of them. Not smooth — the
 *  creases where two points are equally near are the whole texture, and
 *  they are what reads as cracks, scales and stones. */
[[nodiscard]] inline float worleyNoise(uint32_t seed, float x, float y,
                                       int period = 0) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float nearest = 4.0f;
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      float fx = 0, fy = 0, unused = 0;
      detail::featurePoint(seed, xi + dx, yi + dy, 0, period, fx, fy, unused);
      const float px = (float)(xi + dx) + fx - x;
      const float py = (float)(yi + dy) + fy - y;
      const float squared = px * px + py * py;
      if (squared < nearest) nearest = squared;
    }
  const float distance = std::sqrt(nearest);
  return (distance < 1.0f ? distance : 1.0f) * 2.0f - 1.0f;
}

/** CELLULAR NOISE in three dimensions, in [-1, 1]: the same, over the
 *  twenty-seven cells around the one the point falls in. */
[[nodiscard]] inline float worleyNoise(uint32_t seed, float x, float y, float z,
                                       int period = 0) {
  const int xi = (int)std::floor(x), yi = (int)std::floor(y),
            zi = (int)std::floor(z);
  float nearest = 9.0f;
  for (int dz = -1; dz <= 1; ++dz)
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        float fx = 0, fy = 0, fz = 0;
        detail::featurePoint(seed, xi + dx, yi + dy, zi + dz, period, fx, fy,
                             fz);
        const float px = (float)(xi + dx) + fx - x;
        const float py = (float)(yi + dy) + fy - y;
        const float pz = (float)(zi + dz) + fz - z;
        const float squared = px * px + py * py + pz * pz;
        if (squared < nearest) nearest = squared;
      }
  const float distance = std::sqrt(nearest);
  return (distance < 1.0f ? distance : 1.0f) * 2.0f - 1.0f;
}

/** A NOISE LOOK AS ONE VALUE, read at a point.
 *
 *  The seven numbers below are what a grain, a drift, a flow or an
 *  erosion is set by, and they are chosen once for a drawing far more
 *  often than they are chosen per call — which is what makes this a
 *  value to carry rather than a call to repeat. It compares exactly,
 *  member for member, so a memo keyed on one may be skipped and a theme
 *  may bind one.
 *
 *  The range is [-1, 1] for every kind at `Fold::None` and
 *  `Fold::Ridged`, and [0, 1] at `Fold::Turbulence`; the octave sum is
 *  divided by the amplitudes that went into it, so adding octaves
 *  changes the detail and never the range. `Value` and `Worley` reach
 *  the ends of it exactly; `Gradient` and `Simplex` approach them
 *  without a hard bound, which is the amplitude every implementation of
 *  those two has, and neither is clamped. A fold is not symmetric —
 *  once octaves are summed, `Ridged` sits well above the floor and
 *  `Turbulence` well below the ceiling, which is what a crease at every
 *  scale does to a sum. */
struct Field {
  FieldKind kind = FieldKind::Value;
  uint32_t seed = 0;
  /** How many coordinates are read: 1, 2 or 3. The rest are held at
   *  zero, and each kind takes the cheapest body that answers for the
   *  count — which for `Value` is the same number the three-dimensional
   *  body answers with `z` at zero, and for the others is a genuinely
   *  lower-dimensional field. */
  int dimension = 2;
  /** How many lattice cells one unit of the caller's coordinates spans
   *  at the first octave. */
  float frequency = 1.0f;
  /** How many layers are summed, each at `lacunarity` times the
   *  frequency and `gain` times the amplitude of the one before. One is
   *  the plain field. */
  int octaves = 1;
  /** What each octave weighs relative to the one before it. Below one,
   *  which is what makes the sum converge on a shape rather than on
   *  static. */
  float gain = 0.5f;
  /** What each octave's frequency is multiplied by. Two is the doubling
   *  every fBm is written around; a whole number is what a tiling field
   *  needs. */
  float lacunarity = 2.0f;
  Fold fold = Fold::None;
  /** How far the field displaces its own input before reading it, in
   *  lattice cells. Zero reads the point given. A warped field is what
   *  turns even, isotropic noise into something that looks flowed,
   *  marbled or eroded, and it costs one extra evaluation per axis. */
  float warp = 0.0f;
  /** The field repeats every `period` lattice cells — every
   *  `period / frequency` of the caller's own units — on every axis.
   *  Zero does not tile. The tiling is exact when `lacunarity` is a
   *  whole number, since an octave's period is the base one multiplied
   *  by it; `FieldKind::Simplex` never tiles and ignores this. */
  int period = 0;

  /** THE FIELD AT A POINT. */
  [[nodiscard]] float at(float x, float y = 0.0f, float z = 0.0f) const {
    if (dimension < 3) z = 0.0f;
    if (dimension < 2) y = 0.0f;
    if (warp != 0.0f) {
      const float wx = base(seed ^ 0x5bf03635u, x * frequency, y * frequency,
                            z * frequency, period);
      const float wy = base(seed ^ 0x1b56c4e9u, x * frequency, y * frequency,
                            z * frequency, period);
      const float wz = base(seed ^ 0x27d4eb2fu, x * frequency, y * frequency,
                            z * frequency, period);
      x += warp * wx / (frequency != 0.0f ? frequency : 1.0f);
      y += warp * wy / (frequency != 0.0f ? frequency : 1.0f);
      z += warp * wz / (frequency != 0.0f ? frequency : 1.0f);
    }

    const int layers = octaves > 0 ? octaves : 1;
    float total = 0.0f, amplitude = 1.0f, weight = 0.0f, step = frequency;
    int octavePeriod = period;
    for (int octave = 0; octave < layers; ++octave) {
      // A different seed per octave: at a whole lacunarity the layers
      // would otherwise share lattice points and the sum would show the
      // grid they share.
      const uint32_t layerSeed = seed + (uint32_t)octave * 0x9e3779b9u;
      float value = base(layerSeed, x * step, y * step, z * step, octavePeriod);
      switch (fold) {
        case Fold::None:
          break;
        case Fold::Turbulence:
          value = std::abs(value);
          break;
        case Fold::Ridged:
          value = 1.0f - 2.0f * std::abs(value);
          break;
      }
      total += value * amplitude;
      weight += amplitude;
      amplitude *= gain;
      step *= lacunarity;
      if (octavePeriod > 0) {
        const auto next = (int)std::lround((double)octavePeriod * lacunarity);
        octavePeriod = next > 0 ? next : octavePeriod;
      }
    }
    return weight > 0.0f ? total / weight : 0.0f;
  }

  friend bool operator==(const Field&, const Field&) = default;

 private:
  /** One octave, before the fold: the kind read at the dimension. */
  [[nodiscard]] float base(uint32_t layerSeed, float x, float y, float z,
                           int wrap) const {
    switch (kind) {
      case FieldKind::Value:
        return valueNoise(layerSeed, x, y, z, wrap);
      case FieldKind::Gradient:
        return dimension >= 3 ? gradientNoise(layerSeed, x, y, z, wrap)
                              : gradientNoise(layerSeed, x, y, wrap);
      case FieldKind::Simplex:
        return dimension >= 3 ? simplexNoise(layerSeed, x, y, z)
                              : simplexNoise(layerSeed, x, y);
      case FieldKind::Worley:
        return dimension >= 3 ? worleyNoise(layerSeed, x, y, z, wrap)
                              : worleyNoise(layerSeed, x, y, wrap);
    }
    return 0.0f;
  }
};

}  // namespace sigil::core::noise
