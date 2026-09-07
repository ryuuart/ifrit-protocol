// The variation drive: draw-time variable-font axes, and the advance probe
// that decides whether an axis may be driven at all. An axis that moves
// advances would re-place every glyph after the one it touched, so the
// drive refuses it rather than shaping again per frame.

#include "support/TextTestSupport.h"

namespace {

/** The instrument face and the design range of its advance-invariant GRAD
 *  axis, read off the face rather than written here. The axis holds every
 *  advance and moves ink alone, which is the property the substitution gate
 *  is about, and it is the same property on every machine. */
sk_sp<SkTypeface> gradeInstrument(float& lo, float& hi) {
  lo = hi = 0;
  sk_sp<SkTypeface> face = sigil::test::instrument::variable();
  if (!face || !fonts().axisIsAdvanceInvariant(face, "GRAD")) return nullptr;
  const int count = face->getVariationDesignParameters({});
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
      lo = axis.min;
      hi = axis.max;
    }
  return hi > lo ? face : nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// VariationDrive — draw-time variable-font axes, gated by the advance probe

TEST(ComposeVariationDrive, GradDrivesPaintOnlyWhenAdvanceInvariant) {
  // The gate proves advances HOLD; it cannot prove the clone RESPONDS, and
  // a face that accepts the axis and renders identically would let this
  // case pass while seeing nothing. The instrument answers both: one glyph
  // rasterized at each end of the range must differ.
  float gradeMin = 0, gradeMax = 0;
  const sk_sp<SkTypeface> ui = gradeInstrument(gradeMin, gradeMax);
  ASSERT_TRUE(ui) << "the instrument's GRAD axis must hold advances "
                     "over a range";
  {
    const sigil::weave::FontVariation vLo("GRAD", gradeMin),
        vHi("GRAD", gradeMax);
    SkFont fLo(fonts().variedTypeface(ui, {&vLo, 1}), 48);
    SkFont fHi(fonts().variedTypeface(ui, {&vHi, 1}), 48);
    SkGlyphID glyph = fLo.unicharToGlyph('W');
    auto rasterize = [&](const SkFont& f) {
      sk_sp<SkSurface> s =
          SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
      s->getCanvas()->clear(SK_ColorBLACK);
      SkPaint paint;
      paint.setColor(SK_ColorWHITE);
      paint.setAntiAlias(true);
      const SkPoint at{10, 60};
      s->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0}, f,
                                 paint);
      SkBitmap bm;
      bm.allocPixels(s->imageInfo());
      s->readPixels(bm.pixmap(), 0, 0);
      return bm;
    };
    SkBitmap rLo = rasterize(fLo), rHi = rasterize(fHi);
    int rasterDelta = 0;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 100; ++x)
        if (rLo.getColor(x, y) != rHi.getColor(x, y)) ++rasterDelta;
    ASSERT_GT(rasterDelta, 0) << "the instrument's GRAD clone renders inert";
  }

  choreograph::Output<float> grade{gradeMin};
  Host host;
  auto describe = [&] {
    sigil::weave::TextStyle style = styleAt(48);
    style.shaping.typeface = ui;
    style.paint.foreground.setColor(SK_ColorWHITE);  // black-on-black otherwise
    return box().child(text(u8"WEIGHT", style)
                           .key("t")
                           .variationDrive("GRAD", &grade)
                           .absolute()
                           .inset(20, 60, 20, 60));
  };
  host.composer.render(describe());
  host.frame();
  const SkRect before = require(host.composer.bounds("t"));
  SkBitmap lo;
  lo.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(lo.pixmap(), 0, 0);

  grade = gradeMax;  // heavy grade — glyphs thicken, advances hold
  host.frame();
  const SkRect after = require(host.composer.bounds("t"));
  SkBitmap hi;
  hi.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(hi.pixmap(), 0, 0);

  EXPECT_EQ(before, after);  // no relayout — paint-only volatility
  int changed = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      if (lo.getColor(x, y) != hi.getColor(x, y)) ++changed;
  EXPECT_GT(changed, 20) << "GRAD range " << gradeMin << ".."
                         << gradeMax;  // visible thickening
}

TEST(ComposeVariationDrive, TheAxisDrivesOnAPathRunToo) {
  // The old baseline-on-a-path draw was its own path through the engine and
  // took none of the per-glyph dressing with it, so a driven axis on a ring
  // simply did nothing. One draw now places both, so it does.
  float gradeMin = 0, gradeMax = 0;
  const sk_sp<SkTypeface> ui = gradeInstrument(gradeMin, gradeMax);
  ASSERT_TRUE(ui) << "the instrument's GRAD axis must hold advances "
                     "over a range";

  choreograph::Output<float> grade{gradeMin};
  Host host(240, 240);
  sigil::weave::TextStyle style = styleAt(26);
  style.shaping.typeface = ui;
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      box().child(text(u8"GRADED RING", style)
                      .key("ring")
                      .width(200)
                      .height(200)
                      .absolute()
                      .left(20)
                      .top(20)
                      .onPath({.path = geometry::shapes::circle(),
                               .align = TextPath::Align::Center})
                      .variationDrive("GRAD", &grade)));
  host.frame();
  SkBitmap lo;
  lo.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
  host.surface->readPixels(lo.pixmap(), 0, 0);

  grade = gradeMax;
  host.frame();  // paint-only: no re-describe, no relayout
  SkBitmap hi;
  hi.allocPixels(SkImageInfo::MakeN32Premul(240, 240));
  host.surface->readPixels(hi.pixmap(), 0, 0);

  int changed = 0;
  for (int y = 0; y < 240; y += 2)
    for (int x = 0; x < 240; x += 2)
      if (lo.getColor(x, y) != hi.getColor(x, y)) ++changed;
  EXPECT_GT(changed, 20) << "the drive did not reach the glyphs on the ring";
}

namespace {

/** Does @p face DECLARE @p tag at all? `axisIsAdvanceInvariant` answers
 *  FALSE both for "the axis moves advances" and for "there is no such
 *  axis", and the test below has to tell those apart: on a face without a
 *  wght axis the refusal fires for the wrong reason, and the pixels hold no
 *  matter what the drive does — so the test would pass while checking
 *  nothing. */
bool faceDeclaresAxis(const sk_sp<SkTypeface>& face, SkFourByteTag tag) {
  if (!face) return false;
  const int count = face->getVariationDesignParameters({});
  if (count <= 0) return false;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == tag) return true;
  return false;
}

}  // namespace

TEST(ComposeVariationDrive, AdvanceVariantAxisIsRefused) {
  // This needs a face whose wght axis genuinely CHANGES advances, so that
  // the drive has something to refuse. The instrument's does — it is the
  // same face the GRAD cases above read, on its other axis — and both
  // preconditions are asserted below, so the case cannot pass by running
  // against a face that has no axis at all.
  const SkFourByteTag wght = SkSetFourByteTag('w', 'g', 'h', 't');
  const sk_sp<SkTypeface> ui = sigil::test::instrument::variable();
  ASSERT_TRUE(ui);
  ASSERT_TRUE(faceDeclaresAxis(ui, wght));
  ASSERT_FALSE(fonts().axisIsAdvanceInvariant(ui, "wght"))
      << "the instrument face's wght must move advances";

  choreograph::Output<float> weight{400.0f};
  Host host;
  sigil::weave::TextStyle style = styleAt(48);
  style.shaping.typeface = ui;
  style.paint.foreground.setColor(SK_ColorWHITE);  // black-on-black otherwise
  host.composer.render(box().child(text(u8"WEIGHT", style)
                                       .key("t")
                                       .variationDrive("wght", &weight)
                                       .absolute()
                                       .inset(20, 60, 20, 60)));
  host.frame();
  SkBitmap base;
  base.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(base.pixmap(), 0, 0);

  // Liveness guard: the baseline really has ink, so "the pixels hold" below
  // is a claim about glyphs rather than about two identical blank grids.
  int inked = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      if (base.getColor(x, y) != SK_ColorBLACK) ++inked;
  ASSERT_GT(inked, 20) << "baseline text never drew";

  weight = 900.0f;
  host.frame();  // refused: draws at shaped coordinates, pixels hold
  SkBitmap after;
  after.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  host.surface->readPixels(after.pixmap(), 0, 0);
  for (int y = 0; y < 200; y += 4)
    for (int x = 0; x < 200; x += 4)
      ASSERT_EQ(base.getColor(x, y), after.getColor(x, y));
}

TEST(ComposeVariationDrive, TheVerbIsATrackAndComposesWithOtherTracks) {
  // variationDrive() is sugar over fx(): the same axis coordinate reached by
  // hand as a track must draw the same pixels. The equivalence is the point
  // — if the verb kept a text path of its own, a track drawn over it would
  // hide the drive entirely.
  float gradeMin = 0, gradeMax = 0;
  const sk_sp<SkTypeface> ui = gradeInstrument(gradeMin, gradeMax);
  ASSERT_TRUE(ui) << "the instrument's GRAD axis must hold advances "
                     "over a range";

  sigil::weave::TextStyle style = styleAt(48);
  style.shaping.typeface = ui;
  style.paint.foreground.setColor(SK_ColorWHITE);
  choreograph::Output<float> grade{gradeMax};

  Host verb;
  verb.composer.render(box().child(text(u8"GRADE", style)
                                       .key("t")
                                       .variationDrive("GRAD", &grade)
                                       .absolute()
                                       .inset(20, 60, 20, 60)));
  verb.frame();

  Host byHand;
  byHand.composer.render(box().child(
      text(u8"GRADE", style)
          .key("t")
          .fx({.effect = TextEffect::variableAxis("GRAD", gradeMax)})
          .absolute()
          .inset(20, 60, 20, 60)));
  byHand.frame();

  SkBitmap fromVerb, fromTrack;
  fromVerb.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  fromTrack.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  verb.surface->readPixels(fromVerb.pixmap(), 0, 0);
  byHand.surface->readPixels(fromTrack.pixmap(), 0, 0);
  constexpr size_t kRowBytes = 200 * sizeof(uint32_t);
  for (int y = 0; y < 200; ++y)
    ASSERT_EQ(std::memcmp(fromVerb.getAddr32(0, y), fromTrack.getAddr32(0, y),
                          kRowBytes),
              0)
        << "the verb and the hand-built axis track disagreed on row " << y;

  // …and the drive stays visible under a second track: a second
  // track that moves the glyphs leaves the grade in place.
  Host stacked;
  stacked.composer.render(box().child(text(u8"GRADE", style)
                                          .key("t")
                                          .variationDrive("GRAD", &grade)
                                          .fx({.effect = fx::rise(0)})
                                          .absolute()
                                          .inset(20, 60, 20, 60)));
  stacked.frame();
  SkBitmap composed;
  composed.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  stacked.surface->readPixels(composed.pixmap(), 0, 0);
  for (int y = 0; y < 200; ++y)
    ASSERT_EQ(std::memcmp(fromVerb.getAddr32(0, y), composed.getAddr32(0, y),
                          kRowBytes),
              0)
        << "a stacked track dropped the driven axis, at row " << y;
}

TEST(ComposeVariationDrive, ADrivenAxisRetainsABoundedFacePopulation) {
  // A varied clone the font context RETAINS is retained forever, so a
  // driven axis fed a value that never repeats
  // must not put one there per frame. It reads as free under a fixed-dt
  // harness — where an animated value cycles on the scene's own period and
  // the memo saturates — and as a frame time growing linearly with uptime
  // in any host that steps wall-clock dt.
  //
  // So the coordinates here NEVER REPEAT: a golden-ratio rotation across
  // the axis's design range, which is irrational and therefore visits a
  // distinct value on every one of the frames below.
  float gradeMin = 0, gradeMax = 0;
  const sk_sp<SkTypeface> ui = gradeInstrument(gradeMin, gradeMax);
  ASSERT_TRUE(ui) << "the instrument's GRAD axis must hold advances "
                     "over a range";

  constexpr float kSize = 48.0f;
  constexpr int kHalf = 200;  // frames per half; the run is two of them
  // The population is read TWICE — once half way, once at the end — so the
  // assertions can speak about GROWTH and not only about a number. Each run
  // gets its own context, so both readings are absolute populations rather
  // than deltas against whatever the rest of this file left behind.
  struct Retained {
    size_t half = 0, full = 0;
  };
  const auto retainedAfter = [&](bool continuous) {
    sigil::weave::FontContext local(sigil::weave::ports::systemFontManager());
    sigil::motion::Ticker ticker;
    Composer composer(ticker, local);
    composer.setSize({200, 200});
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
    sigil::weave::TextStyle style = styleAt(kSize);
    style.shaping.typeface = ui;
    style.paint.foreground.setColor(SK_ColorWHITE);
    choreograph::Output<float> phase{0.0f};
    // eachMs = 0: every glyph reads the one master phase, so the coordinate
    // is exactly the sequence driven below and nothing else.
    Track track{.effect = fx::variableAxisSweep("GRAD", gradeMin, gradeMax),
                .stagger = {.eachMs = 0, .durationMs = 100},
                .progress = &phase};
    track.continuous = continuous;
    composer.render(box().padding(10).child(
        text(u8"GRADE", style).key("t").fx(std::move(track))));
    Retained out;
    double walk = 0.0;
    for (int f = 0; f < 2 * kHalf; ++f) {
      constexpr double kGolden = 0.6180339887498949;
      walk = std::fmod(walk + kGolden, 1.0);
      phase = (float)walk;
      ticker.tick(1.0 / 60.0);
      surface->getCanvas()->clear(SK_ColorBLACK);
      composer.draw(*surface->getCanvas());
      if (f + 1 == kHalf) out.half = local.variedTypefaceCount();
    }
    out.full = local.variedTypefaceCount();
    return out;
  };

  // A snapped track lands on the ladder, and the ladder is what bounds the
  // population: at this rendered size it offers a few hundred rungs, and no
  // number of frames can reach past them. The count that must never come
  // back is one per frame.
  const Retained snapped = retainedAfter(false);
  EXPECT_GT(snapped.full, 0u)
      << "the drive retained nothing at all, so this measures a track that "
         "never reached the face";
  EXPECT_LT(snapped.full, (size_t)(2 * kHalf))
      << "the snapped ladder retained a clone per frame — the coordinate is "
         "reaching the memo unsnapped";
  // Four rungs per rendered pixel of em, plus the two clones an advance
  // probe adds when this run is the one that pays for it.
  EXPECT_LE(snapped.full, 4u * (size_t)kSize + 2u)
      << "more clones than the ladder at " << kSize << " px has rungs";

  // A continuous track has no ladder, so it has nothing bounded to retain
  // and must retain nothing: the clone is transient and dies with the frame
  // that drew it. Stated as growth as well as as a count, because that is
  // the property — 200 further frames, every one asking for a coordinate no
  // earlier frame asked for, must add exactly zero.
  const Retained smooth = retainedAfter(true);
  EXPECT_EQ(smooth.full, smooth.half)
      << "a continuous coordinate grew the retained population between "
         "frame "
      << kHalf << " and frame " << 2 * kHalf;
  EXPECT_LE(smooth.full, 2u)
      << "a continuous coordinate was retained; only the advance probe's "
         "own two clones can belong in the memo";
}
