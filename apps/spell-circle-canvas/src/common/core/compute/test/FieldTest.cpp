/** @file
 * The noise field: what it answers, and what it agrees with.
 *
 * A field's numbers are pinned as bits for the same reason the mixers'
 * are — a picture stored as bytes was seeded through one of these, and
 * "smooth", "in range" and "the same twice" are all still true of a
 * field that drifted. Beside the pins are the claims a pin cannot make:
 * that a value field IS the one another library already ships, that a
 * period really tiles, that adding octaves does not widen the range, and
 * that near points read near values, which is the whole difference
 * between a field and the per-index hash under it.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Field.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

using namespace sigil::core;
using noise::Field;
using noise::FieldKind;
using noise::Fold;

uint32_t bits(float f) {
  uint32_t b = 0;
  std::memcpy(&b, &f, sizeof b);
  return b;
}

/** `geometry::path::valueNoise`, transcribed. SigilCoreCompute links
 *  nothing of this project's, so the agreement between the two bodies is
 *  asserted against a copy of the other one rather than against it; a
 *  copy is enough, because what would break the agreement is an edit
 *  HERE and the copy is what such an edit would stop matching. */
float geometryValueNoise(float px, float py, float pz, uint32_t seed) {
  auto hash = [seed](int x, int y, int z) {
    return (float)noise::lattice(seed, x, y, z) / (float)0xFFFFFFFFu;
  };
  const int xi = (int)std::floor(px), yi = (int)std::floor(py),
            zi = (int)std::floor(pz);
  const float xf = px - (float)xi, yf = py - (float)yi, zf = pz - (float)zi;
  auto smooth = [](float t) { return t * t * (3 - 2 * t); };
  const float u = smooth(xf), v = smooth(yf), w = smooth(zf);
  float accum = 0;
  for (int dz = 0; dz <= 1; ++dz)
    for (int dy = 0; dy <= 1; ++dy)
      for (int dx = 0; dx <= 1; ++dx) {
        const float weight =
            (dx ? u : 1 - u) * (dy ? v : 1 - v) * (dz ? w : 1 - w);
        accum += hash(xi + dx, yi + dy, zi + dz) * weight;
      }
  return accum * 2.0f - 1.0f;
}

/** The lowest and highest the field reaches over a run that walks all
 *  three axes at once, so no axis is left at a lattice point. */
struct Reach {
  float low = 1e9f;
  float high = -1e9f;
  void add(float v) {
    if (v < low) low = v;
    if (v > high) high = v;
  }
};

Reach reachOf(const Field& field, int samples = 200000) {
  Reach reach;
  for (int i = 0; i < samples; ++i)
    reach.add(field.at((float)i * 0.0173f, (float)(i % 977) * 0.0431f + 0.13f,
                       (float)(i % 613) * 0.0217f));
  return reach;
}

}  // namespace

TEST(Field, ValueNoiseIsTheFieldAnotherLibraryAlreadyShipsToTheBit) {
  const Field field{
      .kind = FieldKind::Value, .seed = 7, .dimension = 3, .octaves = 1};
  for (int i = 0; i < 20000; ++i) {
    const float x = (float)i * 0.0137f - 50.0f;
    const float y = (float)i * 0.0211f - 30.0f;
    const float z = (float)i * 0.0071f - 10.0f;
    ASSERT_EQ(bits(field.at(x, y, z)), bits(geometryValueNoise(x, y, z, 7)))
        << "at " << x << ", " << y << ", " << z;
  }
}

TEST(Field, EachKindAnswersTheSameFloatsInEveryImplementation) {
  EXPECT_EQ(bits(Field{.kind = FieldKind::Value, .seed = 7, .dimension = 3}.at(
                1.5f, 2.25f, 0.75f)),
            0xbd96b348u);
  EXPECT_EQ(bits(Field{.kind = FieldKind::Gradient, .seed = 7}.at(1.5f, 2.25f)),
            0x3f2733f8u);
  EXPECT_EQ(bits(Field{.kind = FieldKind::Simplex, .seed = 7}.at(1.5f, 2.25f)),
            0xbf2658ecu);
  EXPECT_EQ(bits(Field{.kind = FieldKind::Worley, .seed = 7}.at(1.5f, 2.25f)),
            0x3e119c98u);
}

TEST(Field, TheWholeLookIsOneNumberAtAPointAndNotSevenCallsAtOne) {
  // Every prop at once, pinned: an octave sum with a fold, a warp and a
  // frequency, which is what a carried look actually is.
  const Field look{.kind = FieldKind::Simplex,
                   .seed = 7,
                   .dimension = 2,
                   .frequency = 0.5f,
                   .octaves = 4,
                   .gain = 0.5f,
                   .lacunarity = 2.0f,
                   .fold = Fold::Ridged,
                   .warp = 0.35f};
  EXPECT_EQ(bits(look.at(3.0f, -2.0f)), 0xbea1d4b6u);
}

TEST(Field, ReadsNearValuesAtNearPointsWhichIsWhatMakesItAField) {
  // The claim that separates a field from the per-index hash under it:
  // a step of a thousandth of a cell moves the value by about a
  // thousandth of its range, not by half of it.
  for (FieldKind kind : {FieldKind::Value, FieldKind::Gradient,
                         FieldKind::Simplex, FieldKind::Worley}) {
    const Field field{.kind = kind, .seed = 3, .dimension = 2};
    float largest = 0.0f;
    for (int i = 0; i < 20000; ++i) {
      const float x = (float)i * 0.0173f, y = (float)(i % 331) * 0.0431f;
      largest = std::max(largest,
                         std::abs(field.at(x, y) - field.at(x + 0.001f, y)));
    }
    // Cellular noise has creases where two feature points are equally
    // near, so its bound is looser than a smooth field's; both are far
    // below the half-range step an unrelated hash would take.
    EXPECT_LT(largest, kind == FieldKind::Worley ? 0.02f : 0.01f)
        << "kind " << (int)kind;
  }
}

TEST(Field, EachKindStaysInsideTheRangeItsHeaderStates) {
  const Reach value = reachOf({.kind = FieldKind::Value, .seed = 3});
  const Reach gradient = reachOf({.kind = FieldKind::Gradient, .seed = 3});
  const Reach simplex = reachOf({.kind = FieldKind::Simplex, .seed = 3});
  const Reach worley = reachOf({.kind = FieldKind::Worley, .seed = 3});
  for (const Reach& reach : {value, gradient, simplex, worley}) {
    EXPECT_GE(reach.low, -1.0f);
    EXPECT_LE(reach.high, 1.0f);
    // And it uses the range rather than sitting in the middle of it.
    EXPECT_LT(reach.low, -0.85f);
    EXPECT_GT(reach.high, 0.85f);
  }
}

TEST(Field, AddingOctavesChangesTheDetailAndNeverTheRange) {
  for (int octaves : {1, 2, 4, 8}) {
    const Reach reach =
        reachOf({.kind = FieldKind::Gradient, .seed = 3, .octaves = octaves},
                40000);
    EXPECT_GE(reach.low, -1.0f) << "octaves " << octaves;
    EXPECT_LE(reach.high, 1.0f) << "octaves " << octaves;
  }
  // One octave is the bare kind, with no division and no second seed.
  const Field one{.kind = FieldKind::Gradient, .seed = 3, .octaves = 1};
  for (int i = 0; i < 512; ++i) {
    const float x = (float)i * 0.037f, y = (float)i * 0.071f;
    EXPECT_EQ(bits(one.at(x, y)), bits(noise::gradientNoise(3u, x, y)));
  }
}

TEST(Field, AFoldCreasesEveryOctaveAndSaysWhichWayUp) {
  const Reach turbulence = reachOf({.kind = FieldKind::Gradient,
                                    .seed = 1,
                                    .octaves = 4,
                                    .fold = Fold::Turbulence},
                                   40000);
  EXPECT_GE(turbulence.low, 0.0f);
  EXPECT_LE(turbulence.high, 1.0f);

  const Reach ridged = reachOf({.kind = FieldKind::Gradient,
                                .seed = 1,
                                .octaves = 4,
                                .fold = Fold::Ridged},
                               40000);
  EXPECT_GE(ridged.low, -1.0f);
  EXPECT_LE(ridged.high, 1.0f);
  // The two folds are the same crease the other way up: one has its
  // extreme at the top of the range, the other at the bottom of a range
  // that never goes below zero.
  EXPECT_GT(ridged.high, 0.9f);
  EXPECT_LT(turbulence.low, 0.1f);
}

TEST(Field, APeriodRepeatsTheFieldExactlyOnEveryAxis) {
  for (FieldKind kind :
       {FieldKind::Value, FieldKind::Gradient, FieldKind::Worley}) {
    const Field field{.kind = kind,
                      .seed = 5,
                      .dimension = 2,
                      .frequency = 1.0f,
                      .octaves = 3,
                      .period = 8};
    for (int i = 0; i < 400; ++i) {
      const float x = (float)i * 0.0417f, y = (float)i * 0.0291f;
      // A tile away on each axis is the same field. The tolerance is a
      // float's own rounding of a coordinate eight larger, not a seam.
      EXPECT_NEAR(field.at(x, y), field.at(x + 8.0f, y), 1e-4f)
          << "kind " << (int)kind;
      EXPECT_NEAR(field.at(x, y), field.at(x, y + 8.0f), 1e-4f)
          << "kind " << (int)kind;
      EXPECT_NEAR(field.at(x, y), field.at(x - 16.0f, y + 8.0f), 1e-4f)
          << "kind " << (int)kind;
    }
  }
}

TEST(Field, TheSkewedLatticeCarriesNoPeriodSoSimplexIgnoresOne) {
  // Stated rather than silently seamed: a period on a simplex field
  // changes nothing, so a caller reading the header knows the tile it
  // asked for did not happen.
  const Field plain{.kind = FieldKind::Simplex, .seed = 5};
  const Field tiled{.kind = FieldKind::Simplex, .seed = 5, .period = 8};
  for (int i = 0; i < 256; ++i) {
    const float x = (float)i * 0.0417f, y = (float)i * 0.0291f;
    EXPECT_EQ(bits(plain.at(x, y)), bits(tiled.at(x, y)));
  }
}

TEST(Field, DimensionSaysHowManyCoordinatesAreReadAndHoldsTheRestAtZero) {
  const Field flat{.kind = FieldKind::Value, .seed = 4, .dimension = 2};
  const Field solid{.kind = FieldKind::Value, .seed = 4, .dimension = 3};
  for (int i = 0; i < 512; ++i) {
    const float x = (float)i * 0.037f, y = (float)i * 0.071f;
    // The z given is ignored at two dimensions, and the value field at
    // two dimensions is the solid one sliced at zero.
    EXPECT_EQ(bits(flat.at(x, y, 3.5f)), bits(flat.at(x, y, 0.0f)));
    EXPECT_EQ(bits(flat.at(x, y)), bits(solid.at(x, y, 0.0f)));
  }
  const Field line{.kind = FieldKind::Value, .seed = 4, .dimension = 1};
  EXPECT_EQ(bits(line.at(2.5f, 9.0f)), bits(line.at(2.5f, 0.0f)));
}

TEST(Field, AWarpDisplacesTheFieldByItselfAndStaysDeterministic) {
  const Field plain{.kind = FieldKind::Gradient, .seed = 6, .frequency = 0.5f};
  Field warped = plain;
  warped.warp = 0.6f;
  // A warp is the field displacing its own input, so it moves the value
  // and keeps it a field: still in range, still smooth.
  const Field rebuilt{
      .kind = FieldKind::Gradient, .seed = 6, .frequency = 0.5f, .warp = 0.6f};
  bool moved = false;
  for (int i = 0; i < 512; ++i) {
    const float x = (float)i * 0.037f, y = (float)i * 0.071f;
    moved |= bits(warped.at(x, y)) != bits(plain.at(x, y));
    EXPECT_EQ(bits(warped.at(x, y)), bits(rebuilt.at(x, y)));
    EXPECT_GE(warped.at(x, y), -1.0f);
    EXPECT_LE(warped.at(x, y), 1.0f);
  }
  EXPECT_TRUE(moved);
}

TEST(Field, IsALookThatComparesExactlySoAMemoOverOneCanBeSkipped) {
  const Field grain{.kind = FieldKind::Simplex,
                    .seed = 9,
                    .frequency = 3.0f,
                    .octaves = 4,
                    .fold = Fold::Turbulence};
  EXPECT_EQ(grain, (Field{.kind = FieldKind::Simplex,
                          .seed = 9,
                          .frequency = 3.0f,
                          .octaves = 4,
                          .fold = Fold::Turbulence}));
  Field louder = grain;
  louder.gain = 0.4f;
  EXPECT_NE(grain, louder);
}
