// WHAT A DEVIATION DOES TO THE GLYPH ITSELF: the code-point substitution's
// churn, the shear and non-uniform scale that take the matrix lane while
// their neighbours keep the fast one, and what `continuous` lifts.
//
// The text binary's share of the content suites, one file per subject.

#include "DressedTypeProbes.h"

TEST(ComposeTextFx, ScrambleChurnsDeterministicallyAndResolvesAtOne) {
  // The churn is seeded from the glyph's identity, so the same local time
  // gives the same character every time it is asked — which is what lets a
  // settled scramble cache instead of boiling forever. And every glyph is
  // the letter the text actually says by the end: a decode that never
  // decodes is not the effect.
  const TextEffect churn = fx::scramble(U"ABC");
  GlyphInfo glyph;
  glyph.index = 3;
  glyph.textIndex = 3;
  const auto at = [&](float t) {
    sigil::core::noise::Mix64Stream rng(90210);  // one glyph's stream, replayed
    return churn(glyph, t, rng).codepoint;
  };
  EXPECT_EQ(at(0.1f), at(0.1f)) << "the same moment gave two characters";
  EXPECT_EQ(at(1.0f), (char32_t)0) << "the glyph never resolved";
  bool churned = false, inCharset = true;
  for (int step = 0; step < 40; ++step) {
    const char32_t point = at((float)step / 40.0f);
    if (point == 0) continue;
    churned = true;
    if (point != U'A' && point != U'B' && point != U'C') inCharset = false;
  }
  EXPECT_TRUE(churned) << "nothing was substituted at any moment";
  EXPECT_TRUE(inCharset) << "the churn left the charset it was given";

  // A value like any other preset: comparable by its parameters, and the
  // charset is one of them.
  EXPECT_TRUE(fx::scramble(U"ABC") == fx::scramble(U"ABC"));
  EXPECT_FALSE(fx::scramble(U"ABC") == fx::scramble(U"ABD"));
  EXPECT_FALSE(fx::scramble(U"ABC") == fx::scramble(U"ABC", 7));
}

TEST(ComposeTextFx, SkewAndNonUniformScaleTakeTheMatrixPath) {
  // Neither a shear nor an uneven scale is expressible as an RSXform, so
  // these glyphs route through a per-glyph matrix. The assertions are about
  // the SHAPE that appears, because the route is only worth having if it
  // paints what it promises.
  // Room on every side: a doubled glyph and a leaning one both grow past
  // the box, and a clipped measurement would compare two surface edges.
  const auto render = [&](Host& host, std::string key, GlyphMod mod) {
    host.composer.render(box().padding(60).child(
        text(u8"H", whiteStyle(60))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host upright(200, 200);
  render(upright, "upright", GlyphMod{});
  const SkIRect rest = inkBounds(upright, 200, 200);
  ASSERT_FALSE(rest.isEmpty()) << "the resting glyph never painted";

  GlyphMod leaning;
  leaning.skewXDeg = 30;
  Host sheared(200, 200);
  render(sheared, "sheared", leaning);
  const SkIRect leant = inkBounds(sheared, 200, 200);
  EXPECT_GT(leant.width(), rest.width() + 4)
      << "a 30-degree shear did not widen the glyph's footprint";
  // …and it leans the way Element::skewX does: the top toward −x.
  const int band = std::max(leant.height() / 4, 2);
  EXPECT_LT(inkCentroidX(sheared, 200, leant.top(), leant.top() + band),
            inkCentroidX(sheared, 200, leant.bottom() - band, leant.bottom()))
      << "the shear leant the wrong way, or not at all";

  GlyphMod tall;
  tall.scaleY = 2.0f;
  Host stretched(200, 200);
  render(stretched, "tall", tall);
  const SkIRect grown = inkBounds(stretched, 200, 200);
  EXPECT_GT(grown.height(), rest.height() * 3 / 2)
      << "scaleY did not stretch the glyph";
  EXPECT_LE(grown.width(), rest.width() + 2)
      << "scaleY widened the glyph, so the scale was not non-uniform";
}

TEST(ComposeTextFx, AHeldTrackPaintsNothingBeforeItsBeatBesideAnOpenTrack) {
  // The end-to-end half of the hold: not "the effect returns alpha 0" but
  // "no ink reaches the surface" — and it holds against a SECOND track whose
  // own progress is long settled, because alpha multiplies and a glyph that
  // has not arrived has not arrived.
  choreograph::Output<float> progress{0.0f};
  GlyphMod lift;
  lift.dy = -3;
  const auto render = [&](Host& host, TextEffect decode) {
    host.composer.render(box().padding(20).child(
        text(u8"HOLD", whiteStyle(36))
            .key("k")
            .fx({.effect = std::move(decode),
                 .stagger = {.eachMs = 40, .durationMs = 200},
                 .progress = &progress})
            .fx({.effect = fixed("lift", lift)})));
    host.frame();
  };
  // The control first: unheld, the same tree at the same moment paints —
  // wrong letters, but it paints — so an empty surface below is the hold's
  // doing and not a scene that never drew.
  Host unheld(240, 140);
  render(unheld, fx::scramble(U"XYZ", 6));
  ASSERT_FALSE(inkBounds(unheld, 240, 140).isEmpty())
      << "the unheld decode drew nothing, so this test proves nothing";

  Host host(240, 140);
  render(host, fx::hold(fx::scramble(U"XYZ", 6)));
  EXPECT_TRUE(inkBounds(host, 240, 140).isEmpty())
      << "a decode drew before any of its beats had opened — either the "
         "hold let the effect through, or the second track's open beat "
         "overrode it";

  progress = 1.0f;
  host.frame();
  EXPECT_FALSE(inkBounds(host, 240, 140).isEmpty())
      << "the hold never released";
}

TEST(ComposeTextFx, SkewYShearsTheOtherAxisAndTakesTheMatrixPath) {
  // The Y counterpart, probed on the axis skewX leaves alone: an X shear
  // moves the top sideways, a Y shear pushes the right side DOWN. Reading
  // the same asymmetry on the same axis for both would pass for a `skewY`
  // that was quietly wired to `skewXDeg`.
  const auto render = [&](Host& host, std::string key, GlyphMod mod) {
    host.composer.render(box().padding(60).child(
        text(u8"H", whiteStyle(60))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host upright(200, 200);
  render(upright, "upright", GlyphMod{});
  const SkIRect rest = inkBounds(upright, 200, 200);
  ASSERT_FALSE(rest.isEmpty()) << "the resting glyph never painted";

  GlyphMod leaning;
  leaning.skewYDeg = 30;
  Host sheared(200, 200);
  render(sheared, "shearedY", leaning);
  const SkIRect leant = inkBounds(sheared, 200, 200);
  EXPECT_GT(leant.height(), rest.height() + 4)
      << "a 30-degree Y shear did not deepen the glyph's footprint";
  EXPECT_LE(leant.width(), rest.width() + 2)
      << "the Y shear widened the glyph, which is what an X shear does";
  // …and it leans the way Element::skewY does: the right side toward +y.
  const int band = std::max(leant.width() / 4, 2);
  EXPECT_LT(inkCentroidY(sheared, 200, leant.left(), leant.left() + band),
            inkCentroidY(sheared, 200, leant.right() - band, leant.right()))
      << "the shear leant the wrong way, or not at all";
}

namespace {

/** Runs the fast-path/matrix-neighbour check with one line sheared on the
 *  axis @p lean names — the SAME assertion for each shear axis, so a routing
 *  condition that learns about one and not the other fails here. */
void expectFastPathLineUntouched(GlyphMod lean) {
  // The route is decided PER GLYPH. A glyph whose deviation is an RSXform
  // draws exactly as it would if no glyph in the node needed a matrix —
  // otherwise adding a shear to one line would silently re-rasterize every
  // other line through a different code path.
  sigil::weave::TextStyle style = whiteStyle(30);
  const auto tree = [&](bool shearSecondLine) {
    GlyphMod lift;
    lift.dy = -4;
    Element t = text(u8"AAAA BBBB", style)
                    .key("k")
                    .width(70)
                    .fx({.effect = fixed("lift", lift)});
    if (shearSecondLine)
      t.fx(
          {.where = sigil::weave::sel::line(1), .effect = fixed("lean", lean)});
    return box().padding(10).child(std::move(t));
  };
  Host plain(200, 200), mixed(200, 200);
  plain.composer.render(tree(false));
  plain.frame();
  mixed.composer.render(tree(true));
  mixed.frame();

  // The empty rows between the two lines: everything above them belongs to
  // the line no track sheared.
  SkBitmap reference;
  reference.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  plain.surface->readPixels(reference.pixmap(), 0, 0);
  const auto rowInked = [&](int y) {
    for (int x = 0; x < 200; ++x)
      if (reference.getColor(x, y) != SK_ColorBLACK) return true;
    return false;
  };
  int firstInked = -1, split = -1;
  for (int y = 0; y < 200; ++y) {
    if (rowInked(y)) {
      if (firstInked < 0) firstInked = y;
    } else if (firstInked >= 0) {
      split = y;
      break;
    }
  }
  ASSERT_GT(split, 0) << "the text did not wrap into two lines, so there is "
                         "no unsheared line to compare";

  SkBitmap sheared;
  sheared.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
  mixed.surface->readPixels(sheared.pixmap(), 0, 0);
  constexpr size_t kRowBytes = 200 * sizeof(uint32_t);
  for (int y = 0; y < split; ++y)
    ASSERT_EQ(std::memcmp(reference.getAddr32(0, y), sheared.getAddr32(0, y),
                          kRowBytes),
              0)
        << "row " << y
        << " of the fast-path line changed when a NEIGHBOURING "
           "line took the matrix route";
  // The control: the sheared line really did change, so the rows above it
  // holding still is a fact about the routing and not about a track that
  // did nothing.
  bool below = false;
  for (int y = split; y < 200 && !below; ++y)
    below = std::memcmp(reference.getAddr32(0, y), sheared.getAddr32(0, y),
                        kRowBytes) != 0;
  EXPECT_TRUE(below) << "the shear track changed nothing anywhere";
}

}  // namespace

TEST(ComposeTextFx, AGlyphOnTheFastPathIsUntouchedByAMatrixNeighbour) {
  GlyphMod leanX;
  leanX.skewXDeg = 25;
  expectFastPathLineUntouched(leanX);
}

TEST(ComposeTextFx, AGlyphWithNoYShearKeepsTheFastPathBesideOneThatHasIt) {
  // The same assertion on the axis the routing condition learned LAST: a
  // glyph whose `skewYDeg` is 0 must stay on the shared transform array
  // however its neighbours lean.
  GlyphMod leanY;
  leanY.skewYDeg = 25;
  expectFastPathLineUntouched(leanY);
}

TEST(ComposeTextFx, ContinuousLiftsTheSnapAndStillSettles) {
  // Rotations are snapped to a 64-step table, so a two-degree lean rounds
  // to no lean at all. `continuous` is the opt-out, and it must actually
  // change what is drawn or it is a field that does nothing.
  const auto render = [](Host& host, bool continuous) {
    GlyphMod lean;
    lean.rotateDeg = 2.0f;
    Track track{.effect = fixed("lean2", lean)};
    track.continuous = continuous;
    host.composer.render(box().padding(20).child(
        text(u8"HH", whiteStyle(64)).key("k").fx(std::move(track))));
    host.frame();
  };
  Host snapped(200, 200), smooth(200, 200);
  render(snapped, false);
  render(smooth, true);
  EXPECT_FALSE(identicalPixels(snapped, smooth, 200, 200))
      << "continuous drew exactly what the snapped ladder did, so the "
         "opt-out is inert";

  // …and it is an opt-out of SNAPPING, not of caching: a continuous track
  // whose progress has stopped moving settles like any other.
  Host settling(200, 200);
  choreograph::Output<float> progress{0.0f};
  Track track{.effect = fx::rise(14), .progress = &progress};
  track.continuous = true;
  settling.composer.render(box().padding(20).child(
      text(u8"SETTLE", whiteStyle(24)).key("k").fx(std::move(track))));
  settling.frame();
  progress = 1.0f;
  for (int i = 0; i < 12; ++i) settling.frame(0.016);
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    settling.frame(0.016);
    paints += settling.composer.stats().nodesPainted;
    records += settling.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "settled continuous text kept painting live";
  EXPECT_EQ(records, 0u) << "settled continuous text kept re-recording";
}
