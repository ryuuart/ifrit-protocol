// RUNS INSIDE ONE PASSAGE: rich text's own styles and how they prune, the
// span restyles that recolor or reshape only what they cover, and the style
// selector that addresses runs by name.
//
// The text binary's share of the content suites, one file per subject.

#include <memory>

#include "DressedTypeProbes.h"

TEST(TextRich, MixedRunsPaintTheirOwnStyles) {
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(36, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(36, SK_ColorRED);
  host.composer.render(box().padding(10).child(
      text(sigil::weave::rich(base).add(u8"AAA ").add(u8"BBB", accent))
          .key("t")));
  host.frame();
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);
  EXPECT_GT(countColor(host, band, SK_ColorWHITE), 20) << "the base run";
  EXPECT_GT(countColor(host, band, SK_ColorRED), 20) << "the accented run";
}

TEST(TextRich, AnIdenticalValuePrunesWhereAFreshPointerCannot) {
  // The whole reason weave::rich() is a value: a component that rebuilds its
  // spans every describe must prune like a static leaf. The shared_ptr overload
  // cannot answer the question — a fresh make_shared is a fresh identity —
  // which is exactly the difference this pins.
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(20, SK_ColorRED);
  auto describe = [&](std::u8string_view tail) {
    return box().child(
        text(sigil::weave::rich(base).add(u8"Signal ").add(tail, accent))
            .key("t"));
  };
  host.composer.render(describe(u8"woven"));
  host.frame();
  host.composer.render(describe(u8"woven"));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical rich text did not prune";
  host.composer.render(describe(u8"noise"));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "a changed run pruned — equality is lying";

  auto byPointer = [&] {
    auto para = std::make_shared<sigil::weave::Paragraph>();
    para->appendText(u8"Signal woven", base);
    return box().child(text(para).key("p"));
  };
  host.composer.render(byPointer());
  host.frame();
  host.composer.render(byPointer());
  EXPECT_GE(host.composer.stats().patchedNodes, 1u)
      << "the pointer overload claimed a prune it cannot prove";
}

TEST(TextRich, AChangedRunStylePatchesToo) {
  Host host(400, 120);
  auto describe = [&](SkColor accentColor) {
    return box().child(text(sigil::weave::rich(coloredStyle(20, SK_ColorWHITE))
                                .add(u8"Signal ")
                                .add(u8"woven", coloredStyle(20, accentColor)))
                           .key("t"));
  };
  host.composer.render(describe(SK_ColorRED));
  host.frame();
  host.composer.render(describe(SK_ColorRED));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.composer.render(describe(SK_ColorGREEN));
  EXPECT_GE(host.composer.stats().patchedNodes, 1u);
}

TEST(TextRich, NamedRunsResolveThroughTheAmbientStyleSet) {
  // How a name resolves is weave's (see its own test). What is this
  // library's is the AMBIENT set: `env::Provide<weave::StyleSet>` reaches a
  // text leaf described in its scope, and a set the value names beats it.
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  sigil::weave::StyleSet reds;
  reds.set("accent", coloredStyle(20, SK_ColorRED));
  sigil::weave::StyleSet greens;
  greens.set("accent", coloredStyle(20, SK_ColorGREEN));

  Host host(200, 120);
  const auto accentColor = [&](sigil::weave::RichText content) {
    host.composer.render(
        box().padding(6).child(text(std::move(content)).key("t")));
    host.frame();
    const std::vector<TextUnit> units = host.composer.units(
        "t", sel::style("accent"), sigil::weave::Unit::Cluster);
    return units.empty() ? SK_ColorTRANSPARENT
                         : units[0].style.paint.foreground.getColor();
  };

  {
    core::env::Provide<sigil::weave::StyleSet> ambient(reds);
    EXPECT_EQ(accentColor(sigil::weave::rich(base).add(u8"x", "accent")),
              SK_ColorRED)
        << "the env set never reached the leaf";
    EXPECT_EQ(accentColor(
                  sigil::weave::rich(base).add(u8"x", "accent").styles(greens)),
              SK_ColorGREEN)
        << "an explicit style set must beat the ambient one";
  }
  // Out of scope again: nothing is offered, so rich()'s own base answers.
  EXPECT_EQ(accentColor(sigil::weave::rich(base).add(u8"x", "accent")),
            SK_ColorWHITE);
}

TEST(TextSpans, SpanPaintRecolorsWithoutReshaping) {
  // Paint-only means paint-only: the glyphs are the glyphs the unrestyled
  // text shaped, at the positions it shaped them, drawn in another colour.
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(28, SK_ColorWHITE);
  const std::u8string body = u8"Count 1234 now";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> shapesBefore = runShapes(host, "t");
  const std::vector<SkPoint> originsBefore = runOrigins(host, "t");
  ASSERT_FALSE(shapesBefore.empty());

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sigil::weave::sel::regex(u8"[0-9]+"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  EXPECT_EQ(runShapes(host, "t"), shapesBefore)
      << "a paint-only restyle re-shaped a word";
  EXPECT_EQ(runOrigins(host, "t"), originsBefore)
      << "a paint-only restyle moved a glyph";
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);
  EXPECT_GT(countColor(host, band, SK_ColorRED), 10) << "the digits";
  EXPECT_GT(countColor(host, band, SK_ColorWHITE), 10) << "everything else";
}

TEST(TextSpans, SpanStyleReshapesOnlyTheWordsItCovers) {
  Host host(400, 160);
  const sigil::weave::TextStyle base = coloredStyle(24, SK_ColorWHITE);
  const std::u8string body = u8"alpha beta gamma";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> before = runShapes(host, "t");
  ASSERT_EQ(before.size(), 3u);

  // The LAST word, so the two ahead of it keep their pen positions too.
  host.composer.render(
      box().padding(10).child(text(body, base)
                                  .spanStyle(sigil::weave::sel::text(u8"gamma"),
                                             coloredStyle(40, SK_ColorRED))
                                  .key("t")));
  host.frame();
  const std::vector<const void*> after = runShapes(host, "t");
  ASSERT_EQ(after.size(), 3u);
  EXPECT_EQ(after[0], before[0]) << "an uncovered word re-shaped";
  EXPECT_EQ(after[1], before[1]) << "an uncovered word re-shaped";
  EXPECT_NE(after[2], before[2]) << "the covered word did not re-shape";
}

TEST(TextSpans, ALaterRestyleWinsOnOverlap) {
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(28, SK_ColorWHITE);
  const std::u8string body = u8"alpha beta";
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sigil::weave::sel::text(u8"beta"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .spanPaint(sigil::weave::sel::words(0, 2),
                     sigil::weave::PaintStyle(SK_ColorGREEN))
          .key("t")));
  host.frame();
  EXPECT_EQ(countColor(host, band, SK_ColorRED), 0)
      << "the earlier narrow rule survived a later broad one";
  EXPECT_GT(countColor(host, band, SK_ColorGREEN), 20);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sigil::weave::sel::words(0, 2),
                     sigil::weave::PaintStyle(SK_ColorGREEN))
          .spanPaint(sigil::weave::sel::text(u8"beta"),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  EXPECT_GT(countColor(host, band, SK_ColorRED), 10) << "the narrow exception";
  EXPECT_GT(countColor(host, band, SK_ColorGREEN), 10) << "the broad rule";
}

TEST(TextSpans, ALineSelectorAddressesTheLayout) {
  Host host(240, 200);
  const sigil::weave::TextStyle base = coloredStyle(24, SK_ColorWHITE);
  const std::u8string body = u8"one two three four five six seven eight";
  host.composer.render(
      box().padding(10).child(text(body, base).width(200).key("t")));
  host.frame();
  const auto* plain = host.composer.paragraphLayout("t");
  ASSERT_NE(plain, nullptr);
  ASSERT_GT(plain->lineCount, 1);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .width(200)
          .spanPaint(sigil::weave::sel::line(0),
                     sigil::weave::PaintStyle(SK_ColorRED))
          .key("t")));
  host.frame();
  const SkIRect all = SkIRect::MakeXYWH(0, 0, 240, 200);
  EXPECT_GT(countColor(host, all, SK_ColorRED), 10) << "the first line";
  EXPECT_GT(countColor(host, all, SK_ColorWHITE), 10) << "the rest";
}

namespace {

sigil::weave::StyleSet glossarySet(SkColor termColor, float termSize) {
  sigil::weave::StyleSet set{coloredStyle(24, SK_ColorWHITE)};
  set.set("term", coloredStyle(termSize, termColor));
  return set;
}

/** "alpha beta gamma beta delta beta", where the first and last `beta` are
 *  written under the name and the middle one is not. */
sigil::weave::RichText glossaryCopy(const sigil::weave::StyleSet& set) {
  sigil::weave::RichText copy = sigil::weave::rich(set.base());
  copy.styles(set)
      .add(u8"alpha ")
      .add(u8"beta", "term")
      .add(u8" gamma ")
      .add(u8"beta")
      .add(u8" delta ")
      .add(u8"beta", "term");
  return copy;
}

}  // namespace

TEST(TextStyleSelector, AddressesTheNamedRunsAndNotTheirWords) {
  Host host(760, 140);
  const sigil::weave::RichText copy =
      glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at WORD granularity number the units the track's own selection
  // resolved, so the beat list IS the addressed word list — and each beat's
  // rect says which word it is.
  const auto wordsAddressed = [&](sigil::weave::Selector where) {
    host.composer.render(box().padding(10).child(
        text(copy).key("t").fx({.where = std::move(where),
                                .effect = fx::rise(0),
                                .stagger = {.eachMs = 1, .durationMs = 1},
                                .over = sigil::weave::Unit::Word})));
    host.frame();
    return host.composer.beatsOf("t", 0);
  };

  const std::vector<Beat> byName = wordsAddressed(sel::style("term"));
  const std::vector<Beat> byWords =
      wordsAddressed(sigil::weave::sel::text(u8"beta"));
  ASSERT_EQ(byWords.size(), 3u) << "the three literal betas";
  ASSERT_EQ(byName.size(), 2u)
      << "the name caught a run nobody wrote it on — sel::style is matching "
         "text rather than the runs the content named";
  // …and they are the FIRST and THIRD of them, in place: the middle beta is
  // the one the name skips.
  EXPECT_EQ(byName[0].rect, byWords[0].rect);
  EXPECT_EQ(byName[1].rect, byWords[2].rect);
}

TEST(TextStyleSelector, ComposesUnderTheSelectorAlgebra) {
  Host host(760, 140);
  const sigil::weave::RichText copy =
      glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at GLYPH granularity: one per addressed glyph, so the count is the
  // selection's size and the algebra can be checked as arithmetic.
  const auto glyphsAddressed = [&](sigil::weave::Selector where) {
    host.composer.render(box().padding(10).child(
        text(copy).key("t").fx({.where = std::move(where),
                                .effect = fx::rise(0),
                                .stagger = {.eachMs = 1, .durationMs = 1},
                                .over = sigil::weave::Unit::Glyph})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };

  const size_t whole = glyphsAddressed(sigil::weave::Selector{});
  const size_t named = glyphsAddressed(sel::style("term"));
  ASSERT_EQ(named, 8u) << "two four-letter runs";
  EXPECT_EQ(glyphsAddressed(sigil::weave::sel::text(u8"beta")), 12u)
      << "three of them";

  // The three operators, against the same two runs.
  EXPECT_EQ(glyphsAddressed(sel::style("term") | sigil::weave::sel::word(0)),
            named + 5u)
      << "alpha joined the union";
  EXPECT_EQ(glyphsAddressed(sel::style("term") & sigil::weave::sel::word(1)),
            4u)
      << "the intersection is the first named run alone";
  EXPECT_EQ(glyphsAddressed(!sel::style("term")), whole - named);
}

TEST(TextStyleSelector, PlainTextCarriesNoNamesAndSaysSoOnce) {
  // Only a named weave::rich() run carries a name. Plain text has none, so the
  // selector addresses nothing — the silent-no-op rule, made audible.
  Host host(400, 120);
  ::testing::internal::CaptureStderr();
  const auto describe = [] {
    return box().padding(10).child(
        text(u8"alpha beta gamma", coloredStyle(24, SK_ColorWHITE))
            .key("t")
            .fx({.where = sel::style("unregistered-register"),
                 .effect = fx::rise(0),
                 .stagger = {.durationMs = 1},
                 .over = sigil::weave::Unit::Glyph}));
  };
  host.composer.render(describe());
  host.frame();
  EXPECT_TRUE(host.composer.beatsOf("t", 0).empty())
      << "a name no run was written with addressed glyphs anyway";

  // A selector is re-resolved on every reflow, so a name that is wrong is
  // wrong every time — and must not report itself every time.
  host.composer.render(box().padding(11).child(
      text(u8"alpha beta gamma", coloredStyle(24, SK_ColorWHITE))
          .key("t")
          .fx({.where = sel::style("unregistered-register"),
               .effect = fx::rise(0),
               .stagger = {.durationMs = 1},
               .over = sigil::weave::Unit::Glyph})));
  host.frame();
  const std::string log = ::testing::internal::GetCapturedStderr();
  size_t seen = 0;
  for (size_t at = log.find("unregistered-register"); at != std::string::npos;
       at = log.find("unregistered-register", at + 1))
    ++seen;
  EXPECT_EQ(seen, 1u) << log;
}

TEST(TextStyleSelector, ReachesTheSpanRestylesToo) {
  // The span verbs resolve their selection as TEXT RANGES through a second
  // resolver. One vocabulary means one answer: the name must address the
  // same two runs there.
  Host host(760, 140);
  const sigil::weave::RichText copy =
      glossaryCopy(glossarySet(SK_ColorWHITE, 24));

  // Where the three betas actually sit, read off the layout rather than
  // guessed, so the assertions below can name one of them.
  host.composer.render(box().padding(10).child(
      text(copy).key("t").fx({.where = sigil::weave::sel::text(u8"beta"),
                              .effect = fx::rise(0),
                              .stagger = {.eachMs = 1, .durationMs = 1},
                              .over = sigil::weave::Unit::Word})));
  host.frame();
  const std::vector<Beat> betas = host.composer.beatsOf("t", 0);
  ASSERT_EQ(betas.size(), 3u);
  const auto bandOf = [](const Beat& b) {
    return SkIRect::MakeLTRB((int)std::floor(b.rect.left()), 0,
                             (int)std::ceil(b.rect.right()), 140);
  };

  const auto redsIn = [&](sigil::weave::Selector where, const Beat& beat) {
    host.composer.render(box().padding(10).child(text(copy).key("t").spanPaint(
        std::move(where), sigil::weave::PaintStyle(SK_ColorRED))));
    host.frame();
    return countColor(host, bandOf(beat), SK_ColorRED);
  };

  EXPECT_GT(redsIn(sel::style("term"), betas[0]), 5) << "the first named run";
  EXPECT_GT(redsIn(sel::style("term"), betas[2]), 5) << "the last named run";
  EXPECT_EQ(redsIn(sel::style("term"), betas[1]), 0)
      << "the unnamed beta was repainted, so the restyle resolver matched "
         "the word rather than the run";
  EXPECT_GT(redsIn(sigil::weave::sel::text(u8"beta"), betas[1]), 5)
      << "…which the literal selector does catch, as it must";

  // And through spanStyle, which re-shapes: exactly the named runs do.
  host.composer.render(box().padding(10).child(text(copy).key("t")));
  host.frame();
  const std::vector<const void*> before = runShapes(host, "t");
  ASSERT_FALSE(before.empty());
  const auto reshapedUnder = [&](sigil::weave::Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").spanStyle(
        std::move(where), coloredStyle(34, SK_ColorGREEN))));
    host.frame();
    const std::vector<const void*> after = runShapes(host, "t");
    // A run list of a different LENGTH is not a re-shape count at all — the
    // restyle re-broke the passage, and the comparison below would be
    // pairing runs that are not each other's.
    if (after.size() != before.size()) return SIZE_MAX;
    size_t moved = 0;
    for (size_t i = 0; i < after.size(); ++i) moved += after[i] != before[i];
    return moved;
  };
  EXPECT_EQ(reshapedUnder(sel::style("term")), 2u);
  EXPECT_EQ(reshapedUnder(sigil::weave::sel::text(u8"beta")), 3u);
}

TEST(TextStyleSelector, ANameOutlivesTheStyleItResolvedTo) {
  // The name is a handle on the RUN, not on the style span it produced — so
  // re-registering it against a different style, at a different size that
  // re-shapes and re-places everything, leaves the same runs addressed.
  Host host(760, 140);
  const auto namedGlyphs = [&](const sigil::weave::StyleSet& set) {
    host.composer.render(box().padding(10).child(
        text(glossaryCopy(set))
            .key("t")
            .fx({.where = sel::style("term"),
                 .effect = fx::rise(0),
                 .stagger = {.eachMs = 1, .durationMs = 1},
                 .over = sigil::weave::Unit::Glyph})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorRED, 24)), 8u);
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorGREEN, 36)), 8u)
      << "a re-registered name stopped resolving, so the selector is keyed "
         "on the style rather than on the run that wears it";
}
