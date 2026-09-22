// What a shaped run exposes and how its glyphs are dressed: type set
// along a path — every contour, the word breaks between them, the
// per-run flip decision and the radial and level orientations a dial and
// a calendar ring need — the glyph paints ink and textStroke put on
// the letters rather than the box, and the metrics a placement solves
// from: the cap slack, the pen positions across a run, the cap height
// off the face, what fitting or condensing a run to a width moves, and
// the edges an aliased run draws.

#include "support/TextTestSupport.h"

TEST(ComposeText, OnPathReDescribeDoesNotKeepTheOldBaseline) {
  // A text run's BASELINE has to reach textEqual(). Leave it out and
  // re-describing with a new path or a new `at` prunes, so the run keeps
  // riding the old baseline forever. The compiler is no help here: a
  // TextPath holding a std::function has its defaulted operator== implicitly
  // deleted, so the omission produces no error anywhere.
  Host host(240, 240);
  auto ring = [](float at) {
    return box().children(
        {text(u8"HHHHHHHHHH", whiteStyle(22))
             .key("ring")
             .width(240)
             .height(240)
             .absolute()
             .left(0)
             .top(0)
             .textOnPath({.path = geometry::shapes::arc(180.0f, 359.9f),
                          .at = at,
                          .align = TextPath::Align::Center})});
  };
  auto lit = [&](int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 240; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  host.composer.render(ring(0.25f));
  host.frame();
  ASSERT_GT(lit(0, 110), 200);

  host.composer.render(ring(0.75f));  // same key, same text, new baseline
  host.frame();
  EXPECT_GT(lit(140, 240), 200);  // it moved…
  EXPECT_LT(lit(0, 110), 40);     // …and did not stay put
}

TEST(ComposeText, OnPathFillsEveryContourNotJustTheFirst) {
  // A path clipped to a frame commonly comes back as SEVERAL contours, so a
  // baseline that takes only the first one drops the rest of the run with no
  // diagnostic. Every contour is one INTERVAL of the run's one line: the
  // words fill them in order, and a word that does not fit the contour it
  // reached starts the next one rather than bending across the gap between
  // two disconnected curves.
  auto twoSegments = [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(10, 40).lineTo(190, 40);    // contour 1: across the top
    b.moveTo(10, 160).lineTo(190, 160);  // contour 2: across the bottom
    return b.detach();
  };
  auto lit = [](Host& host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 200; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };

  // A run long enough to overflow contour 1 must continue onto contour 2.
  Host host(200, 200);
  host.composer.render(
      box().children({text(u8"HHHH HHHH HHHH HHHH HHHH HHHH", whiteStyle(20))
                          .width(200)
                          .height(200)
                          .absolute()
                          .left(0)
                          .top(0)
                          .textOnPath({.path = twoSegments, .at = 0.0f})}));
  host.frame();
  EXPECT_GT(lit(host, 20, 60), 200);    // ink on the first contour…
  EXPECT_GT(lit(host, 140, 180), 200);  // …and on the second, which a
                                        // first-contour-only walk would
                                        // leave silently unreachable
}

TEST(ComposeText, OnPathBreaksAtWordsBetweenContours) {
  // The counterpart contract, pinned so it cannot drift back: a WORD is
  // never split across two contours. The two segments here are far apart,
  // and a word bent across the gap would land letters in the empty band
  // between them.
  auto twoSegments = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(10, 40).lineTo(120, 40);
    b.moveTo(10, 160).lineTo(190, 160);
    return b.detach();
  };
  auto lit = [](Host& host, int y0, int y1) {
    int count = 0;
    for (int y = y0; y < y1; ++y)
      for (int x = 0; x < 200; ++x) count += host.pixel(x, y) != SK_ColorBLACK;
    return count;
  };
  Host host(200, 200);
  host.composer.render(
      box().children({text(u8"HHHH HHHHHHHHHH", whiteStyle(20))
                          .width(200)
                          .height(200)
                          .absolute()
                          .left(0)
                          .top(0)
                          .textOnPath({.path = twoSegments, .at = 0.0f})}));
  host.frame();
  EXPECT_GT(lit(host, 20, 60), 100);    // the short word on contour 1…
  EXPECT_GT(lit(host, 140, 180), 200);  // …the long one whole on contour 2
  EXPECT_EQ(lit(host, 70, 130), 0) << "a word bent across the gap";
}

TEST(ComposeText, AutoFlipIsOnePerRunDecisionSampledAcrossTheRun) {
  // autoFlip is a PER-RUN decision, which is easy to mistake for a no-op. A
  // run that stays on the bottom flips; one that stays on the top does not;
  // and one that WRAPS PAST the crossover cannot be fixed by a single flip,
  // so it is not pretended otherwise. The answer for that case is two runs,
  // top and bottom set separately.
  //
  // The decision samples ACROSS the run rather than reading one midpoint
  // tangent, so a midpoint that happens to land on a locally odd tangent
  // cannot decide for every glyph in the run.
  auto ring = [](float at, bool flip) {
    return box().children({text(u8"HHHHHHHH", whiteStyle(20))
                               .width(200)
                               .height(200)
                               .absolute()
                               .left(0)
                               .top(0)
                               .textOnPath({.path = geometry::shapes::circle(),
                                            .at = at,
                                            .align = TextPath::Align::Center,
                                            .offset = 4.0f,
                                            .autoFlip = flip})});
  };
  auto snap = [](Host& host) {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(200, 200));
    host.surface->readPixels(bm.pixmap(), 0, 0);
    return bm;
  };
  auto differs = [](const SkBitmap& a, const SkBitmap& b) {
    int n = 0;
    for (int y = 0; y < 200; ++y)
      for (int x = 0; x < 200; ++x) n += a.getColor(x, y) != b.getColor(x, y);
    return n;
  };

  // A short caption sitting squarely on the BOTTOM of the ring: every
  // sample says upside down, so the flip must fire.
  Host plain(200, 200), flipped(200, 200);
  plain.composer.render(ring(0.5f, false));
  plain.frame();
  flipped.composer.render(ring(0.5f, true));
  flipped.frame();
  EXPECT_GT(differs(snap(plain), snap(flipped)), 200);

  // …and on the TOP, where every sample says upright, it must not.
  Host topPlain(200, 200), topFlipped(200, 200);
  topPlain.composer.render(ring(0.0f, false));
  topPlain.frame();
  topFlipped.composer.render(ring(0.0f, true));
  topFlipped.frame();
  EXPECT_EQ(differs(snap(topPlain), snap(topFlipped)), 0);
}

TEST(ComposeText, OnPathCanOrientGlyphsRadiallyForADial) {
  // textOnPath rotates glyphs to the TANGENT, which is running lettering — a
  // motto, a ring inscription. An astrolabe limb, a compass rose and a
  // radial axis want the other one: type RADIATING like a spoke, read by
  // turning the instrument. Without it each numeral costs one rotated
  // Element, which is precisely the per-glyph cost textOnPath exists to
  // abolish.
  //
  // (Tangent already gives "up points outward" on a circle — that is why
  // a clock face's 6 comes out upside down — so radiating is genuinely
  // the only orientation that was missing, not a restatement.)
  // Built with parametric() rather than circle() so the test knows
  // exactly where fraction 0.25 is: t runs from 3 o'clock, and with y
  // down a quarter turn lands at the BOTTOM.
  auto ring = [](TextPath::Orient orient) {
    auto circle = geometry::shapes::parametric(
        [](float t) { return SkPoint{std::cos(t), std::sin(t)}; }, 0.0f,
        2.0f * SK_FloatPI, 360, true);
    // ONE tall glyph: a run spread along the arc keeps a wide footprint
    // whichever way its glyphs face, so a multi-glyph run cannot see the
    // per-glyph rotation at all.
    return box().children(
        {text(u8"I", whiteStyle(64))
             .width(240)
             .height(240)
             .absolute()
             .left(0)
             .top(0)
             .textOnPath({.path = circle,
                          .at = 0.25f,  // the bottom of the ring
                          .align = TextPath::Align::Center,
                          .offset = -50.0f,
                          .orient = orient})});
  };
  auto footprint = [](Host& host) {
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          minX = std::min(minX, x);
          maxX = std::max(maxX, x);
          minY = std::min(minY, y);
          maxY = std::max(maxY, y);
        }
    return SkISize{maxX - minX, maxY - minY};
  };

  Host tangent(240, 240), radial(240, 240);
  tangent.composer.render(ring(TextPath::Orient::Tangent));
  tangent.frame();
  radial.composer.render(ring(TextPath::Orient::Radial));
  radial.frame();

  const SkISize t = footprint(tangent), r = footprint(radial);
  ASSERT_GT(t.width(), 0);
  ASSERT_GT(r.width(), 0);
  // At the bottom of the ring the tangent is horizontal, so the glyph
  // stands (upside down, but standing): tall. Radial turns its baseline
  // down the radius, laying it on its side: wide.
  EXPECT_GT(t.height(), t.width());
  EXPECT_GT(r.width(), r.height());
}

TEST(ComposeText, OnPathCanLeaveEveryGlyphLevelForACalendarRing) {
  // The third orientation, and the one neither of the others can reach: a
  // calendar ring's dates and a modern gauge's numerals stand LEVEL at
  // every division, however the baseline is running under them. Read where
  // the tangent is VERTICAL — a quarter turn from the bottom — because
  // that is where a tangent-oriented glyph lies on its side and an upright
  // one still stands.
  auto ring = [](TextPath::Orient orient) {
    auto circle = geometry::shapes::parametric(
        [](float t) { return SkPoint{std::cos(t), std::sin(t)}; }, 0.0f,
        2.0f * SK_FloatPI, 360, true);
    return box().children(
        {text(u8"I", whiteStyle(64))
             .width(240)
             .height(240)
             .absolute()
             .left(0)
             .top(0)
             .textOnPath({.path = circle,
                          .at = 0.5f,  // 9 o'clock: tangent upward
                          .align = TextPath::Align::Center,
                          .offset = -50.0f,
                          .orient = orient})});
  };
  auto footprint = [](Host& host) {
    int minX = 9999, maxX = -1, minY = 9999, maxY = -1;
    for (int y = 0; y < 240; ++y)
      for (int x = 0; x < 240; ++x)
        if (host.pixel(x, y) != SK_ColorBLACK) {
          minX = std::min(minX, x);
          maxX = std::max(maxX, x);
          minY = std::min(minY, y);
          maxY = std::max(maxY, y);
        }
    return SkISize{maxX - minX, maxY - minY};
  };

  Host tangent(240, 240), upright(240, 240);
  tangent.composer.render(ring(TextPath::Orient::Tangent));
  tangent.frame();
  upright.composer.render(ring(TextPath::Orient::Upright));
  upright.frame();

  const SkISize t = footprint(tangent), u = footprint(upright);
  ASSERT_GT(t.width(), 0);
  ASSERT_GT(u.width(), 0);
  EXPECT_GT(t.width(), t.height())
      << "a tangent-oriented glyph where the tangent runs up the page "
         "should lie on its side";
  EXPECT_GT(u.height(), u.width())
      << "an upright glyph stands wherever it sits on the baseline";
}

TEST(ComposeText, MetricsExposeTheCapSlackThatPlacementNeeds) {
  // A text node's top is the LINE BOX top, while type is usually positioned
  // by its CAP TOP, so aligning a layout against a reference needs the SLACK
  // between the two. intrinsicSize() returns only an SkSize, which leaves a
  // caller guessing a fraction of the line height — a constant that changes
  // with every face.
  const auto m = metrics(whiteStyle(40), fonts());
  EXPECT_GT(m.ascent, 0.0f);   // reported as a positive distance, not
  EXPECT_GT(m.descent, 0.0f);  // Skia's signed convention
  EXPECT_GT(m.capHeight, 0.0f);
  EXPECT_GT(m.xHeight, 0.0f);
  // Sanity, and the reason both have fallbacks: x-height sits under cap
  // height, which sits under the ascent.
  EXPECT_LT(m.xHeight, m.capHeight);
  EXPECT_LE(m.capHeight, m.ascent + 0.01f);
  EXPECT_GT(m.capSlack(), 0.0f);
  EXPECT_NEAR(m.lineHeight, m.ascent + m.descent + m.leading, 1e-4f);

  // It scales with the size, which is what makes it usable as a constant
  // per style rather than per run.
  const auto twice = metrics(whiteStyle(80), fonts());
  EXPECT_NEAR(twice.capHeight, m.capHeight * 2.0f, 0.5f);
}

TEST(ComposeText, TextFillWorksWithTheUnitRamps) {
  // ink and the weave::Unit ramps must compose, and they very nearly do
  // not: the metric band already maps the shader's [0,1]² onto the text, so a
  // weave::Unit ramp dividing by the NODE's size a second time collapses the
  // whole gradient to a sliver near zero. Every glyph then paints the first
  // stop, flat — a wrong picture that looks like a deliberate solid fill.
  Host host(320, 160);
  host.composer.render(box().padding(20).children(
      {text(u8"HH", whiteStyle(96))
           .ink(material::skia::Paint::linearUnit(
               {0, 0}, {0, 1},
               {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))}));
  host.frame();

  // Walk the glyph band and collect the reddest and bluest inked pixels.
  int bestRedY = -1, bestBlueY = -1;
  int bestRed = 0, bestBlue = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      if (c == SK_ColorBLACK) continue;
      if ((int)SkColorGetR(c) > bestRed) {
        bestRed = SkColorGetR(c);
        bestRedY = y;
      }
      if ((int)SkColorGetB(c) > bestBlue) {
        bestBlue = SkColorGetB(c);
        bestBlueY = y;
      }
    }
  ASSERT_GE(bestRedY, 0);
  ASSERT_GE(bestBlueY, 0);
  EXPECT_GT(bestRed, 180);
  EXPECT_GT(bestBlue, 180);
  // The ramp runs top to bottom across the CAP BAND, so red is above blue.
  EXPECT_LT(bestRedY, bestBlueY - 20);
}

TEST(ComposeText, TextStrokeDressesTheGlyphsNotTheBox) {
  // Element::stroke() dresses the node's BOX outline, which is a different
  // mark entirely. Without a text-level stroke, thickening a face means
  // dropping to PaintStyle::addUnderlay with a hand-built paint — or, worse,
  // spelling an outline as a ring of offset re-draws of the whole run.
  auto count = [](Host& host, bool wantGreen) {
    int n = 0;
    for (int y = 0; y < 160; ++y)
      for (int x = 0; x < 320; ++x) {
        const SkColor c = host.pixel(x, y);
        n += wantGreen ? (SkColorGetG(c) > 180 && SkColorGetR(c) < 90)
                       : (SkColorGetR(c) > 180 && SkColorGetG(c) < 90);
      }
    return n;
  };

  Host plain(320, 160), outlined(320, 160);
  auto style = whiteStyle(96);
  style.paint.foreground.setColor4f({1, 0, 0, 1}, nullptr);
  plain.composer.render(box().padding(20).children({text(u8"HH", style)}));
  plain.frame();
  outlined.composer.render(box().padding(20).children(
      {text(u8"HH", style).textStroke(8.0f, Fill::color({0, 1, 0, 1}))}));
  outlined.frame();

  // The letterform bodies still paint in the fill colour…
  EXPECT_GT(count(outlined, /*green=*/false), 100);
  // …with a green ring around them that was not there before.
  EXPECT_EQ(count(plain, /*green=*/true), 0);
  EXPECT_GT(count(outlined, /*green=*/true), 200);
  // The node's own box is untouched — this is glyph-level, not stroke().
  EXPECT_EQ(outlined.pixel(2, 2), SK_ColorBLACK);
}

TEST(ComposeText, TextStrokeComposesWithTextFill) {
  // The stroke is a pass BENEATH whatever fills the letterforms, so the
  // two spell "engraved chrome type" together rather than fighting.
  Host host(320, 160);
  host.composer.render(box().padding(20).children(
      {text(u8"HH", whiteStyle(96))
           .textStroke(9.0f, Fill::color({0, 1, 0, 1}))
           .ink(material::skia::Paint::linearUnit(
               {0, 0}, {0, 1},
               {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}}))}));
  host.frame();
  int green = 0, ramp = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      green +=
          SkColorGetG(c) > 180 && SkColorGetR(c) < 90 && SkColorGetB(c) < 90;
      ramp +=
          (SkColorGetR(c) > 150 || SkColorGetB(c) > 150) && SkColorGetG(c) < 90;
    }
  EXPECT_GT(green, 200);  // the outline survives the fill override…
  EXPECT_GT(ramp, 100);   // …and the ramp still fills the bodies
}

TEST(ComposeText, AGlyphOutlineTakesTheInkWhenAPaintHasNoOneColour) {
  // The outline is ONE comparable Fill on the node, measured with no
  // frame in hand, so a unit-square ramp has no single colour to give
  // it. What it must not do is fall through to the opaque black an
  // empty fill leaves behind: the letters are outlined in the ink in
  // force instead, which is the colour the node was already set in.
  Host host(320, 160);
  host.composer.render(
      box()
          .padding(20)
          .ink({0, 1, 0, 1})
          .children({text(u8"HH", whiteStyle(96))
                         .textStroke(8.0f, material::skia::Paint::linearUnit(
                                               {0, 0}, {0, 1},
                                               {{0.0f, {1, 0, 0, 1}},
                                                {1.0f, {0, 0, 1, 1}}}))}));
  host.frame();
  int green = 0;
  for (int y = 0; y < 160; ++y)
    for (int x = 0; x < 320; ++x) {
      const SkColor c = host.pixel(x, y);
      green += SkColorGetG(c) > 180 && SkColorGetR(c) < 90;
    }
  EXPECT_GT(green, 200) << "the outline painted something other than the ink";
}

TEST(ComposeText, AGlyphPaintTheSlotCannotStoreLeavesTheOneItHas) {
  // A reference is not a paint: it reads the ink in force where it
  // lands, and a glyph paint is stored as one paint resolved without the
  // tree. Writing one over a ramp must therefore leave the ramp alone —
  // blanking it would repaint the letters in a colour nobody named,
  // while an EMPTY paint is how the override is meant to be cleared.
  const auto ramp = material::skia::Paint::linearUnit(
      {0, 0}, {0, 1}, {{0.0f, {1, 0, 0, 1}}, {1.0f, {0, 0, 1, 1}}});
  auto rampedPixels = [&](SurfacePaint after) {
    Host host(320, 160);
    host.composer.render(box().padding(20).children(
        {text(u8"HH", whiteStyle(96)).ink(ramp).ink(after)}));
    host.frame();
    int inked = 0;
    for (int y = 0; y < 160; ++y)
      for (int x = 0; x < 320; ++x) {
        const SkColor c = host.pixel(x, y);
        inked += (SkColorGetR(c) > 150 || SkColorGetB(c) > 150) &&
                 SkColorGetG(c) < 90;
      }
    return inked;
  };
  EXPECT_GT(rampedPixels(Fill::currentInk()), 100);
  EXPECT_GT(rampedPixels(Fill::var("accent")), 100);
  // …and the glyphs go back to the style's own white when it is cleared.
  EXPECT_EQ(rampedPixels(Fill::none()), 0);
}

TEST(ComposeText, MeasureRunShapesOnceAndMatchesTheLaidOutElement) {
  // intrinsicSize() is per-Element, so hand-placing N glyphs costs N layouts.
  // measureRun() is ONE layout through the same shaping path a text() leaf
  // takes — which is only useful if it agrees with that leaf, so the
  // assertion is that its advances reproduce what the Element machinery
  // measures for the same run.
  const sigil::weave::TextStyle style = whiteStyle(24);
  const std::vector<float> advances =
      measureRun(u8"HAMBURGEFONTSIV", style, fonts());
  ASSERT_FALSE(advances.empty());
  float sum = 0;
  for (float a : advances) {
    EXPECT_GT(a, 0.0f);
    sum += a;
  }
  // The independent arm: the full Element path (reconcile + Yoga + text
  // measure) sizes the same run. intrinsicSize() ceils the shaped width, so
  // agreement is to the ceil.
  const SkSize laidOut =
      intrinsicSize(text(u8"HAMBURGEFONTSIV", style), fonts());
  EXPECT_NEAR(std::ceil(sum), laidOut.width(), 1.01f)
      << "measureRun's advances disagree with the laid-out element";
  // Controls: a doubled face doubles the run (shaping is live, not a
  // cached constant)…
  float sumBig = 0;
  for (float a : measureRun(u8"HAMBURGEFONTSIV", whiteStyle(48), fonts()))
    sumBig += a;
  EXPECT_NEAR(sumBig, sum * 2.0f, sum * 0.1f);
  // …an empty run shapes to nothing, and the count is the GLYPH count —
  // read off the layout's own walk rather than off the character count,
  // which a ligature in the machine's face would part company with.
  EXPECT_TRUE(measureRun(u8"", style, fonts()).empty());
  size_t placedGlyphs = 0;
  {
    sigil::weave::Paragraph paragraph;
    paragraph.appendText(u8"HAMBURGEFONTSIV", style);
    sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
    sigil::weave::forEachPlacedGlyph(
        sigil::weave::layoutParagraph(fonts(), paragraph, flow), paragraph,
        [&](const sigil::weave::PlacedGlyph&) { ++placedGlyphs; });
  }
  EXPECT_EQ(advances.size(), placedGlyphs);
}

TEST(ComposeText, MeasureRunPrefixSumsAreThePenPositionsAcrossWords) {
  // The header's contract is that the prefix sums ARE the pen positions. An
  // inter-word space is a gap the flow leaves rather than a glyph, so it
  // visits nothing in the glyph walk; left out of the advances, every glyph
  // after a space is placed short and the error grows with each word. The
  // ground truth is the same layout's own placements, so one word, two words
  // and a leading space are all checked against it — a fix that satisfies
  // only the single-word case fails here.
  const sigil::weave::TextStyle style = whiteStyle(40);
  const auto lastPenEnd = [&](std::u8string_view utf8) {
    sigil::weave::Paragraph paragraph;
    paragraph.appendText(utf8, style);
    sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
    const sigil::weave::ParagraphLayout layout =
        sigil::weave::layoutParagraph(fonts(), paragraph, flow);
    // The right edge of the last glyph, and the pen the first one starts at:
    // measureRun's sums are relative to that first pen.
    float first = 0, end = 0;
    bool seen = false;
    sigil::weave::forEachPlacedGlyph(
        layout, paragraph, [&](const sigil::weave::PlacedGlyph& placed) {
          if (!seen) first = placed.rest.x();
          seen = true;
          end = placed.rest.x() + placed.advance;
        });
    return end - first;
  };
  for (std::u8string_view run :
       {std::u8string_view(u8"ONE"), std::u8string_view(u8"A B"),
        std::u8string_view(u8" A B"),
        std::u8string_view(u8"ONE PASS PER WORD PHASE")}) {
    const std::vector<float> advances = measureRun(run, style, fonts());
    ASSERT_FALSE(advances.empty());
    float sum = 0;
    for (float a : advances) sum += a;
    EXPECT_NEAR(sum, lastPenEnd(run), 0.01f)
        << "prefix sums mis-place the last glyph of \""
        << std::string(reinterpret_cast<const char*>(run.data()), run.size())
        << "\"";
  }
  // The glyph count is untouched: a space still contributes no entry, it
  // only lends its advance to the glyph before it. Stated against the same
  // two letters with no space between them, so the claim does not turn on
  // how many glyphs the machine's face makes of them.
  const std::vector<float> spaced = measureRun(u8"A B", style, fonts());
  ASSERT_EQ(spaced.size(), measureRun(u8"AB", style, fonts()).size());
  ASSERT_EQ(spaced.size(), 2u);
  EXPECT_GT(spaced[0], measureRun(u8"A", style, fonts())[0])
      << "the gap must ride the advance of the glyph it follows";
  EXPECT_FLOAT_EQ(spaced[1], measureRun(u8"B", style, fonts())[0])
      << "the last glyph carries no trailing gap";
}

TEST(ComposeText, RunPensAreThePenPositionsWithOnePastTheEnd) {
  // runPens is measureRun already summed, and the whole reason it exists is
  // that everybody who calls measureRun writes that sum by hand. Two claims:
  // the sums are the pen positions the LAYOUT used (checked against its own
  // placements, so an inter-word gap folded into the wrong advance shows),
  // and there is one entry past the end whose value is the run's width.
  const sigil::weave::TextStyle style = whiteStyle(40);
  const auto placed = [&](std::u8string_view utf8) {
    sigil::weave::Paragraph paragraph;
    paragraph.appendText(utf8, style);
    sigil::weave::BlockFlow flow(SkRect::MakeWH(1.0e6f, 1.0e6f));
    const sigil::weave::ParagraphLayout layout =
        sigil::weave::layoutParagraph(fonts(), paragraph, flow);
    // Pen positions relative to the FIRST glyph's pen, which is where the
    // run starts — leading whitespace is no part of it.
    std::vector<float> pens;
    float first = 0;
    bool seen = false;
    sigil::weave::forEachPlacedGlyph(
        layout, paragraph, [&](const sigil::weave::PlacedGlyph& glyph) {
          if (!seen) first = glyph.rest.x();
          seen = true;
          pens.push_back(glyph.rest.x() - first);
        });
    return std::pair{pens, seen};
  };
  for (std::u8string_view run :
       {std::u8string_view(u8"ONE"), std::u8string_view(u8"A B"),
        std::u8string_view(u8" A B"),
        std::u8string_view(u8"ONE PASS PER WORD PHASE")}) {
    const std::vector<float> pens = runPens(run, style, fonts());
    const std::vector<float> advances = measureRun(run, style, fonts());
    ASSERT_EQ(pens.size(), advances.size() + 1)
        << "n glyphs must give n + 1 entries";
    EXPECT_FLOAT_EQ(pens.front(), 0.0f) << "the run starts at its first glyph";
    auto [truth, seen] = placed(run);
    ASSERT_TRUE(seen);
    for (size_t i = 0; i + 1 < pens.size(); ++i)
      EXPECT_NEAR(pens[i], truth[i], 0.01f)
          << "glyph " << i << " of \""
          << std::string(reinterpret_cast<const char*>(run.data()), run.size())
          << "\" is not where the layout put it";
    float width = 0;
    for (float a : advances) width += a;
    EXPECT_FLOAT_EQ(pens.back(), width)
        << "the past-the-end entry is the run's laid-out width";
  }
  // An empty run is 0 wide, and says so with the one entry the contract
  // promises rather than with nothing at all.
  const std::vector<float> nothing = runPens(u8"", style, fonts());
  ASSERT_EQ(nothing.size(), 1u);
  EXPECT_FLOAT_EQ(nothing[0], 0.0f);
}

TEST(ComposeText, EveryCascadeFieldOfATrackParticipatesInEquality) {
  // A cascade over text is two values: the SCHEDULE, which is
  // SigilMotion's and pinned there, and the three fields that say what a
  // unit IS, which are this library's. Miss one and a re-described track
  // keeps the OLD schedule with no diagnostic — a granularity that never
  // takes, or a `beatsOver` flipped to Text on a paragraph that goes on
  // beating over each half's own selection. Both are silent, and both look
  // exactly like the engine ignoring the author.
  //
  // The pin beside `Track::sameShape()` makes a NEW field a build failure;
  // this makes the decision about it mechanical.
  const Track base{.stagger = {.eachMs = 30}};
  Track over = base;
  over.unit = sigil::weave::Unit::Line;
  EXPECT_FALSE(base.sameShape(over)) << "over";
  Track innerUnit = base;
  innerUnit.innerUnit = sigil::weave::Unit::Line;
  EXPECT_FALSE(base.sameShape(innerUnit)) << "innerUnit";
  Track beatsOver = base;
  beatsOver.beatsOver = Beats::Text;
  EXPECT_FALSE(base.sameShape(beatsOver)) << "beatsOver";
  Track schedule = base;
  schedule.stagger.eachMs = 31;
  EXPECT_FALSE(base.sameShape(schedule)) << "the schedule itself";
  EXPECT_TRUE(base.sameShape(base));
}

TEST(ComposeText, ACapHeightIsSolvedFromTheFaceAndNotFromARatio) {
  // A reference states how tall a capital stands, never the font size, and
  // the ratio between them is one face's rather than every face's.
  const sigil::weave::TextStyle fitted =
      atCapHeight(whiteStyle(12), 40.0f, fonts());
  EXPECT_NEAR(metrics(fitted, fonts()).capHeight, 40.0f, 0.5f);
  // It is a property of the face, so it lands on the same size whatever
  // size it started at.
  const sigil::weave::TextStyle other =
      atCapHeight(whiteStyle(96), 40.0f, fonts());
  EXPECT_NEAR(other.shaping.fontSize, fitted.shaping.fontSize, 0.1f);
  // Nothing to solve for leaves the style as it was.
  EXPECT_EQ(atCapHeight(whiteStyle(20), 0.0f, fonts()).shaping.fontSize, 20.0f);
}

TEST(ComposeText, FittingARunToAWidthSolvesPastTheTrackingThatDoesNotScale) {
  const auto widthOf = [](const sigil::weave::TextStyle& style) {
    return runPens(u8"CONDENSED TO FIT", style, fonts()).back();
  };
  const sigil::weave::TextStyle big = whiteStyle(64);
  ASSERT_GT(widthOf(big), 300.0f);

  const sigil::weave::TextStyle fitted =
      fitRun(u8"CONDENSED TO FIT", big, 300.0f, fonts());
  EXPECT_LE(widthOf(fitted), 300.5f);
  EXPECT_GT(widthOf(fitted), 285.0f);  // it fits, it does not merely shrink
  EXPECT_LT(fitted.shaping.fontSize, big.shaping.fontSize);
  EXPECT_FLOAT_EQ(fitted.shaping.scaleX, 1.0f);  // no squeeze was needed

  // A run that already fits is left exactly as it was: the fit is a
  // ceiling, not a resize onto the width.
  const sigil::weave::TextStyle small = whiteStyle(10);
  EXPECT_EQ(
      fitRun(u8"CONDENSED TO FIT", small, 4000.0f, fonts()).shaping.fontSize,
      10.0f);

  // TRACKING IS PX, so it does not shrink with the size and one division
  // overshoots by exactly it. A heavily tracked run is where a fit
  // written as a single ratio is visibly wrong; this one still lands.
  sigil::weave::TextStyle tracked = big;
  tracked.shaping.letterSpacing = 8.0f;
  const sigil::weave::TextStyle fittedTracked =
      fitRun(u8"CONDENSED TO FIT", tracked, 300.0f, fonts());
  EXPECT_LE(widthOf(fittedTracked), 300.5f);
  EXPECT_GT(widthOf(fittedTracked), 270.0f);
}

TEST(ComposeText, TheCondenseClosesOnlyWhatTheSizeFloorLeftOver) {
  const auto widthOf = [](const sigil::weave::TextStyle& style) {
    return runPens(u8"CONDENSED TO FIT", style, fonts()).back();
  };
  // Held at a size floor, the fit has nowhere left to shrink and squeezes
  // instead — and only by what the floor left over.
  const sigil::weave::TextStyle squeezed =
      fitRun(u8"CONDENSED TO FIT", whiteStyle(64), 300.0f, fonts(),
             {.minSize = 48.0f, .minCondense = 0.5f});
  EXPECT_FLOAT_EQ(squeezed.shaping.fontSize, 48.0f);
  EXPECT_LT(squeezed.shaping.scaleX, 1.0f);
  EXPECT_GE(squeezed.shaping.scaleX, 0.5f);
  EXPECT_LE(widthOf(squeezed), 301.0f);

  // A run that fits by shrinking alone is never also squeezed, even when
  // it was allowed to be.
  const sigil::weave::TextStyle shrunk =
      fitRun(u8"CONDENSED TO FIT", whiteStyle(64), 300.0f, fonts(),
             {.minCondense = 0.5f});
  EXPECT_FLOAT_EQ(shrunk.shaping.scaleX, 1.0f);

  // Neither floor is a promise to fit: a run that cannot reach the width
  // comes back at the floors, over-wide, rather than at a size nothing
  // could read.
  const sigil::weave::TextStyle refused = fitRun(
      u8"CONDENSED TO FIT", whiteStyle(64), 10.0f, fonts(), {.minSize = 48.0f});
  EXPECT_FLOAT_EQ(refused.shaping.fontSize, 48.0f);
  EXPECT_GT(widthOf(refused), 10.0f);
}

// -------------------------------------------------------------------------
// Which way the glyphs face on a ring, and the edges an aliased run
// draws.

TEST(ComposeText, RingWindingDecidesWhichWayTheGlyphsFace) {
  // Direction is not a detail on a text baseline. textOnPath orients to the
  // tangent, so a clockwise ring puts glyph-up radially OUTWARD
  // (Nightingale's 1858 plate) and a counter-clockwise one puts it INWARD
  // (Chevreul's 1864 limb) — both uniform engraver's conventions,
  // opposite in sign, which is why a ring inscription so often ends up
  // hand-rolling an OutlineFunction over a default nobody chose.
  //
  // The two assertions are chosen so as NOT to depend on knowing which
  // quadrant Skia's addOval starts in: the directed overload at kCW is
  // EXACTLY the undirected one — a strict superset, not a near-miss — and
  // kCCW is observably different. Anything that names a specific expected
  // position is asserting an inference about Skia rather than a property of
  // this library.
  auto render = [](std::function<SkPath(SkSize)> path) {
    auto host = std::make_unique<Host>(300, 300);
    host->composer.render(
        box().children({text(u8"RING INSCRIPTION", whiteStyle(30))
                            .width(240)
                            .height(240)
                            .absolute()
                            .left(30)
                            .top(30)
                            .textOnPath({.path = std::move(path),
                                         .at = 0.25f,
                                         .align = TextPath::Align::Center,
                                         .offset = 0.0f})}));
    host->frame();
    return host;
  };
  auto differing = [](Host& a, Host& b) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x) n += a.pixel(x, y) != b.pixel(x, y);
    return n;
  };
  auto inked = [](Host& h) {
    int n = 0;
    for (int y = 0; y < 300; ++y)
      for (int x = 0; x < 300; ++x) n += h.pixel(x, y) != SK_ColorBLACK;
    return n;
  };

  auto cw = render(geometry::shapes::circle(SkPathDirection::kCW));
  auto ccw = render(geometry::shapes::circle(SkPathDirection::kCCW));
  auto plain = render(geometry::shapes::circle());

  ASSERT_GT(inked(*cw), 300);
  ASSERT_GT(inked(*ccw), 300);
  // The winding is observable — the run faces the other way.
  EXPECT_GT(differing(*cw, *ccw), 500);
  // …and the directed overload's default IS the undirected one.
  EXPECT_EQ(differing(*cw, *plain), 0);
}

TEST(ComposeText, AliasedTextHasHardEdges) {
  // Skia takes glyph edging from the FONT, never the paint, so
  // `paint.foreground.setAntiAlias(false)` is silently ignored on text.
  // Without a field on the shaping style there is no way to ask for aliased
  // type at all, and the only recourse is a raw kAlias SkFont drawn inside a
  // decoration on a hand-measured box — which forfeits shaping, bidi,
  // fallback and contentFlowAround. One field buys it back; this is not a
  // bitmap-font path, just an edging switch.
  // 33 px in the instrument face puts every letter's stems at 3.3 and 16.5
  // px into its cell and its top at 23.1 px, so no edge lies on the pixel
  // grid and each stem leaves a partial pixel on every row it spans but
  // the two its own top and bottom edges cut.
  constexpr int kGlyphs = 4;
  constexpr int kPartialPerGlyph = 2 * 21;
  auto greys = [](bool aliased) {
    auto style = whiteStyle(33);
    style.shaping.aliased = aliased;
    Host host(240, 100);
    host.composer.render(box().padding(12).children({text(u8"AVWM", style)}));
    host.frame();
    int partial = 0, full = 0;
    for (int y = 0; y < 100; ++y)
      for (int x = 0; x < 240; ++x) {
        const int r = SkColorGetR(host.pixel(x, y));
        full += r > 250;
        partial += r > 15 && r < 240;  // an antialiased edge pixel
      }
    return std::pair<int, int>{full, partial};
  };

  const auto soft = greys(false);
  const auto hard = greys(true);
  ASSERT_GT(soft.first, 200);  // both actually drew
  ASSERT_GT(hard.first, 200);
  // Antialiased type is fringed with partial coverage; aliased type is
  // not — every pixel is on or off.
  EXPECT_GE(soft.second, kGlyphs * kPartialPerGlyph);
  EXPECT_LT(hard.second, soft.second / 8);
}
