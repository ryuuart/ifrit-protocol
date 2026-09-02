// The brush binary's share of ComposeKitTest.cpp: the suites whose subjects are
// brush-tier values, cut from that file so each test binary links only
// the target it exercises.

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFont.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/kit/Kit.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <cmath>
#include <utility>
#include <vector>
using namespace sigil::compose;
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/kit/Strokes.h>
#include <sigilcompose/typography/Typography.h>

namespace kit = sigil::compose::kit;

namespace {

sigil::weave::FontContext& fonts() {
  static auto* context =
      new sigil::weave::FontContext(sigil::weave::ports::systemFontManager());
  return *context;
}

}  // namespace

TEST(ComposeKitStrokes, ShapersSatisfyThePublicSeam) {
  static_assert(ShaperScheme<kit::brush::shapers::Wave>);
  static_assert(ShaperScheme<kit::brush::shapers::Jitter>);
  static_assert(ShaperScheme<kit::brush::shapers::Offset>);
  // …and the wave doubles as a PROFILE, which is what makes a braid strand
  // and an undulating band one vocabulary.
  static_assert(ProfileScheme<kit::brush::shapers::Wave>);

  SkPathBuilder b;
  b.moveTo(0, 50);
  b.lineTo(200, 50);
  const SkPath straight = b.detach();

  const SkPath waved = kit::brush::shapers::wave(6, 30).shape(straight);
  EXPECT_GT(waved.getBounds().height(), 6.0f) << "the wave did not deviate";
  const SkPath jittered = kit::brush::shapers::jitter(8, 3, 5).shape(straight);
  EXPECT_GT(jittered.getBounds().height(), 1.0f);
  const SkPath railed = kit::brush::shapers::offset(-12).shape(straight);
  EXPECT_NEAR(railed.getBounds().centerY(), 62.0f, 1.5f)
      << "positive offset is LEFT of travel — the one convention (R3's "
         "sign port), so travelling +x with y down a NEGATIVE offset goes "
         "down the screen";

  // Comparable, so a brush holding one prunes.
  EXPECT_TRUE(kit::brush::shapers::wave(6, 30) ==
              kit::brush::shapers::wave(6, 30));
  EXPECT_FALSE(kit::brush::shapers::wave(6, 30) ==
               kit::brush::shapers::wave(6, 31));
}

TEST(ComposeKitStrokes, BraidCrossesByConstruction) {
  // n waves at phase k/n MUST trade sides, which is why the braid primitive
  // is the wave and not the offset. Constant offsets are rails: they stay a
  // fixed distance apart and never cross, so no braid can be built from
  // them — the control at the end of this case is that claim.
  SkPathBuilder b;
  b.moveTo(0, 100);
  b.lineTo(400, 100);
  const SkPath spine = b.detach();

  for (int n : {2, 3, 4}) {
    const std::vector<brush::Strand> braid = kit::strands::braid(
        n, 10, 60, brush::solid(2, Fill::color({1, 0, 0, 1})));
    ASSERT_EQ(braid.size(), (size_t)n);
    std::vector<SkPath> paths;
    paths.reserve(braid.size());
    for (const brush::Strand& s : braid)
      paths.push_back(profileOffset(spine, s.path.profile()));
    const std::vector<Crossing> crossings = discoverCrossings(paths);
    EXPECT_GT(crossings.size(), 0u)
        << n << " braided waves produced no crossing";
    // Numbering is positional and contiguous, so a pin means the same knot
    // for as long as the geometry holds.
    for (size_t i = 0; i < crossings.size(); ++i)
      EXPECT_EQ(crossings[i].index, i);
  }

  // The control: the same strand count as PARALLELS never crosses.
  std::vector<SkPath> rails;
  rails.reserve(3);
  for (int k = 0; k < 3; ++k)
    rails.push_back(profileOffset(spine, strand::offset((float)k * 6.0f)));
  EXPECT_TRUE(discoverCrossings(rails).empty())
      << "parallels are rails — they must not braid";
}

TEST(ComposeKitStrokes, BraidSharesOneBrushAcrossItsStrands) {
  const Decoration ink = brush::solid(2, Fill::color({0, 1, 0, 1}));
  const std::vector<brush::Strand> braid = kit::strands::braid(3, 8, 40, ink);
  for (const brush::Strand& s : braid) {
    EXPECT_TRUE(s.brush == ink)
        << "braid() is sugar for n offsets of ONE brush";
    EXPECT_EQ(s.path.source(), StrandPath::Source::Relative);
    EXPECT_NEAR(s.path.reach(), 8.0f, 1e-4f) << "reach is the amplitude";
  }
  // Distinct phases — otherwise they would be coincident and never cross.
  EXPECT_FALSE(braid[0].path == braid[1].path);
}

TEST(ComposeKitStrokes, SpansAndShapesAreCompositionsNotNewKinds) {
  // kit::spans::brackets is a COMPOSITION of core terms, which is what a
  // kit span can be and why Spans stays a closed value.
  EXPECT_TRUE(kit::spans::brackets(18) == spans::corners(18));
  EXPECT_FALSE(kit::spans::brackets(18) == spans::corners(19));

  // kit::shapes::ring is a plainer name for core's annulus, not a second
  // shape — same path, so a figure can use either spelling.
  const SkPath ring = kit::shapes::ring(0.6f)({100, 100});
  EXPECT_FALSE(ring.isEmpty());
  EXPECT_EQ(ring, sigil::geometry::shapes::annulus(0.6f)({100, 100}));
}

TEST(ComposeKitStrokes, TheWaveProfileIsAKitValueOverACoreSeam) {
  // Core ships strand::self()/offset() only; everything that oscillates
  // lives in the kit — but it plugs the SAME Profile seam, so core code
  // never learns that a kit profile exists.
  const Profile undulating = kit::profile::wave(9, 50);
  EXPECT_NEAR(undulating.max(), 9.0f, 1e-4f) << "max() is required by the seam";
  EXPECT_TRUE(undulating == kit::profile::wave(9, 50));
  EXPECT_FALSE(undulating == kit::profile::wave(9, 51));
  EXPECT_FALSE(undulating == strand::offset(9));

  // A band takes it, because a band's taper and a strand's path are one value.
  Element undulatingBand =
      band(sigil::geometry::shapes::circle(), across(undulating));
  EXPECT_TRUE(undulatingBand.node() != nullptr);
}

namespace {

/** A composer over a raster surface — the kit suite's own harness. Kept
 *  here rather than shared with compose_test because the two binaries are
 *  deliberately separate (a kit failure must not read as a kernel one). */
struct StrokeHost {
  sigil::motion::Ticker ticker;
  Composer composer{ticker, fonts()};
  sk_sp<SkSurface> surface;

  explicit StrokeHost(int w = 200, int h = 200) {
    composer.setSize({(float)w, (float)h});
    surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  }
  SkColor pixel(int x, int y) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
    surface->readPixels(bm.pixmap(), x, y);
    return bm.getColor(0, 0);
  }
  void frame() {
    surface->getCanvas()->clear(SK_ColorBLACK);
    composer.draw(*surface->getCanvas());
  }
};

Fill strokeRed() { return Fill::color({1, 0, 0, 1}); }

Fill strokeGreen() { return Fill::color({0, 1, 0, 1}); }

}  // namespace

// ---------------------------------------------------------------------------
// The shaper seam, exercised with KIT shaper values.
//
// These live here rather than in compose_test because the kernel suite must
// not include a kit header: if it did, a kit compile failure would be
// reported as a kernel failure.

TEST(ComposeKitStrokes, ShapedAgreesWithTheRestyleWrapper) {
  // `.shaped(value)` is the ONE geometry-deviation seam. `brush::restyle`
  // does the same job around a `GeometryOp`, and it stays because a raw
  // lambda can never be a Shaper — a Shaper is comparable by design, and a
  // lambda is not. So the claim here is that the two spellings agree, not
  // that one replaces the other.
  //
  // What is asserted is INK-COUNT SIMILARITY within 5%, not identical
  // output. The two paths build their own PaintContext and wrap the op
  // differently, so byte equality is not the property on offer; a shaper
  // that silently drew nothing, or drew something else, still fails.
  auto draw = [](bool legacySpelling) {
    StrokeHost host(200, 200);
    Element e = box().rect(SkRect::MakeXYWH(30, 30, 140, 140));
    if (legacySpelling)
      e.stroke(brush::restyle(kit::brush::shapers::Wave{5, 24},
                              brush::solid(3, strokeRed()), 8));
    else
      e.stroke(Brush{}
                   .shaped(kit::brush::shapers::wave(5, 24))
                   .layer(brush::solid(3, strokeRed())));
    host.composer.render(stack().child(std::move(e)));
    host.frame();
    int inked = 0;
    for (int x = 0; x < 200; ++x)
      for (int y = 0; y < 200; ++y)
        if (host.pixel(x, y) == SK_ColorRED) ++inked;
    return inked;
  };
  const int shaped = draw(false), wrapper = draw(true);
  EXPECT_GT(shaped, 100) << "the shaper drew nothing";
  EXPECT_NEAR((double)shaped, (double)wrapper, (double)wrapper * 0.05);

  // Two brushes built from equal shaper values compare equal, which is what
  // lets a node carrying a shaped brush prune instead of re-patching.
  EXPECT_TRUE(Brush{}.shaped(kit::brush::shapers::wave(5, 24)) ==
              Brush{}.shaped(kit::brush::shapers::wave(5, 24)));
  EXPECT_FALSE(Brush{}.shaped(kit::brush::shapers::wave(5, 24)) ==
               Brush{}.shaped(kit::brush::shapers::wave(5, 25)));
}

TEST(ComposeKitStrokes, ShapersAreComparableValuesAndPrune) {
  static_assert(ShaperScheme<kit::brush::shapers::Wave>);
  EXPECT_TRUE(Shaper(kit::brush::shapers::wave(4, 20)) ==
              Shaper(kit::brush::shapers::wave(4, 20)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::wave(4, 20)) ==
               Shaper(kit::brush::shapers::wave(5, 20)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::wave(4, 20)) ==
               Shaper(kit::brush::shapers::jitter()));
  EXPECT_TRUE(Shaper() == Shaper()) << "reflexive when empty";
}

TEST(ComposeKitStrokes, BraidAlternatesAlongTheWholeRun) {
  // The degeneracy this guards against: as mark-width/sin(crossing angle)
  // approaches the spacing between knots, neighbouring overlap lenses touch,
  // pathops merges them into ONE contour, and the first crossing's patch
  // claims the whole run — so the braid renders as a single strand laid on
  // top of the other. It shows up only at tight amplitude-to-ink ratios,
  // hence the two deliberately tight parameter sets at the bottom.
  //
  // braid() shares ONE brush across its strands by design, so the strands
  // here are rebuilt with the geometry braid() produces (waves at phase k/n,
  // asserted equal below) but two different inks — that is what makes the
  // alternation readable from pixels at all.
  auto wrongKnots = [](float amp, float wavelength, float inkWidth) {
    StrokeHost host(1000, 240);
    SkPathBuilder sp;
    sp.moveTo(0, 120);
    sp.lineTo(1000, 120);
    const SkPath spine = sp.detach();

    const std::vector<brush::Strand> strands = {
        brush::Strand{kit::profile::wave(amp, wavelength, 0.0f),
                      brush::solid(inkWidth, strokeRed())},
        brush::Strand{kit::profile::wave(amp, wavelength, 0.5f),
                      brush::solid(inkWidth, strokeGreen())}};
    // Same phases braid() would hand out for n = 2.
    const std::vector<brush::Strand> viaBraid = kit::strands::braid(
        2, amp, wavelength, brush::solid(inkWidth, strokeRed()));
    EXPECT_EQ(viaBraid[0].path, strands[0].path);
    EXPECT_EQ(viaBraid[1].path, strands[1].path);

    host.composer.render(stack().child(
        box()
            .inset(0)
            // the callable is invoked on every layout, so its capture must
            // survive each return
            // NOLINTNEXTLINE(performance-no-automatic-move)
            .shape([&](SkSize) { return spine; })
            .stroke(brush::weave(strands, crossing::alternate()))));
    host.frame();

    // The knots, in the same order the rule numbers them.
    std::vector<SkPath> paths;
    paths.reserve(strands.size());
    for (const brush::Strand& st : strands)
      paths.push_back(profileOffset(spine, st.path.profile()));
    const std::vector<Crossing> knots = discoverCrossings(paths);
    EXPECT_GT(knots.size(), 20u) << "not enough knots to show the defect";

    // alternate() puts strand 0 (red) over at even ordinals, strand 1
    // (green) over at odd ones. Sample each knot and count disagreements.
    int wrong = 0, sampled = 0;
    for (const Crossing& k : knots) {
      const int px = (int)std::lround(k.at.fX);
      const int py = (int)std::lround(k.at.fY);
      // A knot bisected by the frame has no interior pixel to read — the
      // spine ends ON the last one, at x == width. Skip rather than count
      // the surface's out-of-bounds transparent black as a defect.
      if (px < 1 || px > 998 || py < 1 || py > 238) continue;
      ++sampled;
      const SkColor want = (k.index % 2 == 0) ? SK_ColorRED : SK_ColorGREEN;
      if (host.pixel(px, py) != want) ++wrong;
    }
    EXPECT_GT(sampled, 20) << "too few knots landed inside the frame";
    return std::pair<int, size_t>{wrong, knots.size()};
  };

  const auto a = wrongKnots(3.0f, 40.0f, 6.0f);
  EXPECT_EQ(a.first, 0) << a.first << " of " << a.second
                        << " knots wrong at amp 3 / wl 40 / ink 6";
  const auto b = wrongKnots(4.0f, 30.0f, 5.0f);
  EXPECT_EQ(b.first, 0) << b.first << " of " << b.second
                        << " knots wrong at amp 4 / wl 30 / ink 5";
}

// ---------------------------------------------------------------------------
// Rounded, Square and Zigzag are the corner-rounding and squared-off
// shapers. Each must actually deviate the path it is handed, and Zigzag
// must not collapse into Wave — a sharp zigzag and a smooth wave are
// separate marks, and a shared parameter pair makes them easy to conflate.

TEST(ComposeKitStrokes, TheThreeTwinsThatAbsorbedTheOpsStructs) {
  SkPathBuilder b;
  b.moveTo(10, 10);
  b.lineTo(160, 10);
  b.lineTo(160, 120);
  b.lineTo(10, 120);
  b.close();
  const SkPath src = b.detach();

  // (Named locals rather than braced temporaries inline: a designated
  // aggregate inside EXPECT_* hands the macro its commas.)
  const kit::brush::shapers::Square kitSquare{5, 26};
  const kit::brush::shapers::Zigzag kitZigzag{4, 28};
  const kit::brush::shapers::Wave kitWave{4, 28};

  EXPECT_FALSE(kit::brush::shapers::Rounded{9.0f}.shape(src) == src)
      << "Rounded did not round the corners";
  EXPECT_FALSE(kitSquare.shape(src) == src);
  EXPECT_FALSE(kitZigzag.shape(src) == src);
  // …and Zigzag is NOT Wave, at identical amplitude and wavelength.
  EXPECT_FALSE(kitZigzag.shape(src) == kitWave.shape(src));
}

TEST(ComposeKitStrokes, TheNewTwinsAreComparableSeamValuesLikeTheRest) {
  static_assert(ShaperScheme<kit::brush::shapers::Rounded>);
  static_assert(ShaperScheme<kit::brush::shapers::Square>);
  static_assert(ShaperScheme<kit::brush::shapers::Zigzag>);
  EXPECT_TRUE(Shaper(kit::brush::shapers::rounded(6)) ==
              Shaper(kit::brush::shapers::rounded(6)));
  EXPECT_FALSE(Shaper(kit::brush::shapers::rounded(6)) ==
               Shaper(kit::brush::shapers::rounded(7)));
  // Different KINDS never compare equal even at equal numbers — the type
  // is part of the value, which is what keeps a re-described brush honest.
  EXPECT_FALSE(Shaper(kit::brush::shapers::square(4, 28)) ==
               Shaper(kit::brush::shapers::zigzag(4, 28)));
}

TEST(ComposeKitPresets, TheFourPresetsCameOutOfCoreUNCHANGED) {
  // Each preset is pinned against a HAND-BUILT copy of its layer stack.
  // `LayeredBrush` has a defaulted `==`, so this compares every field of
  // every layer — width, colour, blur, dash, phase, blend, the lot.
  // Counting layers and spot-checking one width would keep passing on a
  // preset whose colours had all been halved, which is precisely the kind
  // of drift a shared preset suffers.
  using kit::brush::presets::circuit;
  using kit::brush::presets::filament;
  using kit::brush::presets::pulse;
  using kit::brush::presets::rope;

  const SkColor4f glow{0.435f, 0.847f, 1.0f, 1};
  const SkColor4f core{0.875f, 0.965f, 1.0f, 1};
  SkColor4f g18 = glow, g45 = glow, c90 = core;
  g18.fA = 0.18f;
  g45.fA = 0.45f;
  c90.fA = 0.90f;
  const LayeredBrush wantFilament{{
      {14, g18, 8, {}, 0, SkBlendMode::kPlus},
      {7, g45, 3, {}, 0, SkBlendMode::kPlus},
      {2.5f, c90},
      {1, {1, 1, 1, 0.7f}},
  }};
  EXPECT_TRUE(filament() == wantFilament);

  // circuit: three tiers, three different stacks, and tier 2 is the only
  // one that lays down two layers (an under-glow beneath the trace).
  const SkColor4f teal{0.208f, 0.878f, 0.824f, 1};
  SkColor4f data = teal, main = teal, power = teal, under = teal;
  data.fA = 0.55f;
  main.fA = 0.85f;
  power.fA = 1.0f;
  under.fA = 0.15f;
  // (Named locals, not braced temporaries inline: an aggregate inside
  // EXPECT_* hands the macro its commas.)
  const LayeredBrush wantData{
      {{1, data, 0, {}, 0, SkBlendMode::kSrcOver, false}}};
  const LayeredBrush wantMain{
      {{2, main, 0, {}, 0, SkBlendMode::kSrcOver, false}}};
  const LayeredBrush wantPower{
      {{8, under, 4}, {4, power, 0, {}, 0, SkBlendMode::kSrcOver, false}}};
  EXPECT_TRUE(circuit(teal, 0) == wantData);
  EXPECT_TRUE(circuit(teal, 1) == wantMain);
  EXPECT_TRUE(circuit(teal, 2) == wantPower);
  EXPECT_TRUE(circuit() == circuit(teal, 1)) << "the shipped defaults";

  // rope: the palette ladder, verified against Path of Building, plus the
  // Active state's halo. `scale` multiplies every width, dash and blur.
  const SkColor4f body{0.541f, 0.447f, 0.282f, 1};
  const SkColor4f ridge{0.780f, 0.659f, 0.420f, 1};
  const SkColor4f bodyLit{body.fR * 1.15f, body.fG * 1.15f, body.fB * 1.15f, 1};
  const SkColor4f ridgeLit{ridge.fR * 1.3f, ridge.fG * 1.3f, ridge.fB * 1.3f,
                           0.6f};
  const LayeredBrush wantActive{{
      {18, {1.0f, 0.788f, 0.439f, 0.13f}, 6},
      {11, body, 0, {}, 0, SkBlendMode::kSrcOver, false},
      {7, ridge, 0, {7, 5}, 0},
      {7, bodyLit, 0, {7, 5}, 6},
      {2, ridgeLit, 0, {7, 5}, 3},
  }};
  EXPECT_TRUE(rope(2) == wantActive);
  // The state index CLAMPS rather than reading off the end of the table.
  EXPECT_TRUE(rope(9) == wantActive);
  EXPECT_TRUE(rope(-3) == rope(0));
  EXPECT_FALSE(rope(0) == rope(1)) << "the three states are three palettes";

  const SkColor4f halo{1.0f, 0.79f, 0.44f, 0.35f};
  SkColor4f pulseBody = halo;
  pulseBody.fA = std::min(1.0f, halo.fA * 2.2f);
  const LayeredBrush wantPulse{{
      {12, halo, 5, {}, 0, SkBlendMode::kPlus},
      {5, pulseBody, 2, {}, 0, SkBlendMode::kPlus},
      {2, {1, 1, 1, 0.9f}},
  }};
  EXPECT_TRUE(pulse() == wantPulse);
}

TEST(ComposeKitPresets, TheDefaultArgumentsSurvivedTheMove) {
  // The presets' default arguments are part of their published shape: a
  // caller writing `rope(1)` must get the same brush as `rope(1, 1.0f)`.
  // Nothing else in the suite would notice a changed default, since every
  // other case passes all the arguments explicitly.
  EXPECT_TRUE(kit::brush::presets::rope(1) ==
              kit::brush::presets::rope(1, 1.0f));
  const SkColor4f teal{0.2f, 0.9f, 0.8f, 1};
  EXPECT_TRUE(kit::brush::presets::circuit(teal) ==
              kit::brush::presets::circuit(teal, 1));
  EXPECT_FALSE(kit::brush::presets::circuit(teal, 2) ==
               kit::brush::presets::circuit(teal, 1));
}

TEST(ComposeKitStrokes, ABleedIsADISTANCEAndNeverNegative) {
  // bleed() grows the recording cull, so a NEGATIVE one shrinks it and
  // clips the mark it was supposed to protect. A negative amplitude is a
  // perfectly legal wave — it simply starts the other way — so every
  // oscillating value must report the magnitude, never the raw parameter.
  // The rule is the same across core and kit, so all three are checked.
  // (Named locals: a braced aggregate inside EXPECT_* hands the macro its
  // commas.)
  const kit::brush::shapers::Wave kitWave{-4.0f, 20.0f};
  const kit::brush::shapers::Square kitSquare{-5.0f, 26.0f};
  const kit::brush::shapers::Zigzag kitZigzag{-4.0f, 28.0f};
  EXPECT_FLOAT_EQ(kitWave.bleed(), 4.0f);
  EXPECT_FLOAT_EQ(kitSquare.bleed(), 5.0f);
  EXPECT_FLOAT_EQ(kitZigzag.bleed(), 4.0f);
  // …and the type-erased seams read the same number through.
  EXPECT_FLOAT_EQ(Shaper(kitWave).bleed(), 4.0f);
  EXPECT_FLOAT_EQ(GeometryOp(kitSquare).bleed(), 5.0f);
  // A negative amplitude still DRAWS — it is the same wave, half a cycle
  // over — so this is a cull fix and not a clamp on the value.
  SkPathBuilder b;
  b.moveTo(10, 60);
  b.lineTo(190, 60);
  const SkPath line = b.detach();
  EXPECT_FALSE(kitWave.shape(line).isEmpty());
}
