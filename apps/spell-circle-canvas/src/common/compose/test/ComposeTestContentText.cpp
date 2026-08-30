// The text binary's share of ComposeTestContent.cpp: the suites whose subjects
// are text-tier values, cut from that file so each test binary links only the
// target it exercises.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/Feed.h>

#include <numeric>

#include "support/TextTestSupport.h"

namespace {

/** Plain rows, white on the Host's black ground so ink reads as brightness. */
feed::TextOptions feedOptions(size_t visible, float size = 12.0f) {
  feed::TextOptions options;
  options.styles.base(whiteStyle(size));
  options.window.visible = visible;
  options.window.gap = 2.0f;
  return options;
}

}  // namespace

TEST(ComposeFeed, ATypedOnRowPaintsLiveThenCachesWhenItsTrackSettles) {
  // A glyph entrance on a feed row is affordable because it ENDS: while the
  // track's progress moves the row paints live, and once it settles the row
  // is a static leaf again, cached like every row above it. A track that
  // never settled would pin the whole window volatile.
  feed::TextRing ring;
  ring.append({toU8("daemon bound port 6042")});
  const feed::TextOptions options = feedOptions(8, 16.0f);
  auto typed = [&](const feed::TextRow& row) {
    return feed::textRow(row, options.styles)
        .fx({.effect = fx::typeOn(),
             .stagger = {.eachMs = 12, .durationMs = 40},
             .progress = animate(from(0.0f).to(1.0f),
                                 {300ms, &choreograph::easeNone})});
  };
  Host host(240, 120);
  host.composer.render(
      box().padding(4).child(feed::feed(ring, options.window, typed)));
  host.frame(0.05);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "a running typeOn track must paint live";

  for (int i = 0; i < 40; ++i) host.frame(0.033);  // the entrance finishes
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    host.frame(0.016);
    paints += host.composer.stats().nodesPainted;
    records += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "a settled row kept painting live";
  EXPECT_EQ(records, 0u) << "a settled row kept re-recording";
}

namespace {

/** The shape a designed console row takes: several fields, not one line. */
struct StructuredRow {
  std::string ts, tag, body;
  bool operator==(const StructuredRow&) const = default;
};

}  // namespace

TEST(ComposeFeed, AStructuredRowAppendsAtItsOwnConstantCost) {
  // A richer row — a severity stripe beside ONE rich() leaf whose runs
  // speak named styles, entered by a track that settles — is a small
  // subtree rather than a single text node. That changes the CONSTANT in
  // the append price, never its shape: an append patches exactly the new
  // row's own nodes, however many rows the window holds, and the price
  // repeats append after append.
  sigil::weave::StyleSet styles(whiteStyle(12));
  styles.set("ts", whiteStyle(10));
  styles.set("tag", whiteStyle(11));
  feed::Ring<StructuredRow> ring;
  for (int i = 0; i < 30; ++i)
    ring.append({"0412.50", "AUTH", "row " + std::to_string(i)});
  auto rowEl = [&](const StructuredRow& r) {
    auto line = rich(styles.base())
                    .styles(styles)
                    .add(toU8(r.ts + "  "), "ts")
                    .add(toU8(r.tag + "  "), "tag")
                    .add(toU8(r.body));
    return box()
        .row()
        .gap(6)
        .child(box().width(3).height(10).fill(Fill::color(SkColors::kRed)))
        .child(text(std::move(line))
                   .fx({.effect = fx::typeOn(),
                        .stagger = {.eachMs = 5, .durationMs = 30},
                        .progress = animate(from(0.0f).to(1.0f),
                                            {200ms, &choreograph::easeNone})}));
  };
  constexpr size_t kRowNodes = 3;  // the row box, the stripe, the text leaf

  const auto perAppendCost = [&](size_t visible) {
    feed::Options options;
    options.visible = visible;
    options.gap = 2.0f;
    Host host(260, 500);
    auto describe = [&] {
      return box().padding(6).child(feed::feed(ring, options, rowEl));
    };
    host.composer.render(describe());
    host.frame(0.4);  // every mounted entrance has settled

    ring.append({"0413.00", "WARD", "breach"});
    host.composer.render(describe());
    const size_t patched = host.composer.stats().patchedNodes;
    host.frame(0.016);
    const size_t live = host.composer.stats().instances;

    // The same price again, and the retained tree does not grow: one
    // subtree in at the tail, one out at the head.
    ring.append({"0413.50", "LATT", "sweep"});
    host.composer.render(describe());
    EXPECT_EQ(host.composer.stats().patchedNodes, patched);
    host.frame(0.016);
    EXPECT_EQ(host.composer.stats().instances, live)
        << "the window is bounded at " << visible;
    return patched;
  };

  EXPECT_EQ(perAppendCost(8), kRowNodes);
  // Twice the window, the same append price: the cost is the row's own
  // shape, not the window's size.
  EXPECT_EQ(perAppendCost(16), kRowNodes);
}

TEST(TextLayout, FullyConstrainedAbsoluteTextPaints) {
  // Yoga skips the measure callback when absolute insets determine both
  // dimensions; the kernel must lay the paragraph out at paint time.
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      stack().child(text(u8"WWWW", style).absolute().inset(10, 10, 10, 120)));
  host.frame();
  int lit = 0;
  for (int x = 10; x < 190; x += 4)
    for (int y = 10; y < 70; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 20);  // glyph coverage, not empty
}

TEST(TextLayout, AlignItemsCentersTextLeaf) {
  Host host;
  sigil::weave::TextStyle style = styleAt(40);
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(box()
                           .width(200)
                           .height(60)
                           .alignItems(Align::Center)
                           .child(text(u8"W", style)));
  host.frame();

  int litLeft = 0, litMiddle = 0;
  for (int x = 0; x < 50; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litLeft++;
  for (int x = 75; x < 125; x += 2)
    for (int y = 0; y < 60; y += 2)
      if (host.pixel(x, y) != SK_ColorBLACK) litMiddle++;
  EXPECT_EQ(litLeft, 0);    // nothing hugging the start edge
  EXPECT_GT(litMiddle, 5);  // the glyph sits in the middle
}

TEST(TextLayout, ParagraphOverloadPaintsMixedSpans) {
  Host host(400, 200);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  sigil::weave::TextStyle big = styleAt(40);
  big.paint.foreground.setColor(SK_ColorWHITE);
  sigil::weave::TextStyle small = styleAt(16);
  small.paint.foreground.setColor(SK_ColorWHITE);
  para->appendText(std::u8string(u8"BIG"), big);
  para->appendText(std::u8string(u8" and small"), small);

  host.composer.render(box().padding(10).child(text(para).key("spans")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("spans");
  ASSERT_NE(layout, nullptr);
  int lit = 0;
  for (int x = 10; x < 390; x += 3)
    for (int y = 10; y < 70; y += 3)
      if (host.pixel(x, y) != SK_ColorBLACK) lit++;
  EXPECT_GT(lit, 15);  // both spans shaped and painted
}

TEST(Shape, BlobIsDeterministicOrganicAndBounded) {
  auto probe = [](uint32_t seed) {
    Host host;
    host.composer.render(box().child(box()
                                         .width(120)
                                         .height(120)
                                         .shape(shapes::blob(seed, 0.3f, 9))
                                         .fill(red())));
    host.frame();
    std::vector<SkColor> px;
    for (int y = 0; y < 130; y += 4)
      for (int x = 0; x < 130; x += 4) px.push_back(host.pixel(x, y));
    return px;
  };
  std::vector<SkColor> a1 = probe(7), a2 = probe(7), b = probe(8);
  EXPECT_EQ(a1, a2);  // same seed → identical pixels (cacheable chaos)
  EXPECT_NE(a1, b);   // different seed → different blob

  Host host;
  host.composer.render(box().child(box()
                                       .width(120)
                                       .height(120)
                                       .shape(shapes::blob(7, 0.3f, 9))
                                       .fill(red())));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorRED);  // center always covered
  int outside = 0;
  for (int x = 121; x < 200; x += 4)
    for (int y = 0; y < 200; y += 4)
      if (host.pixel(x, y) != SK_ColorBLACK) outside++;
  EXPECT_EQ(outside, 0);  // never escapes its layout box
}

TEST(ComposeKinetic, StaggeredRiseRevealsInOrder) {
  // The stagger law: at mid-progress the early glyphs are fully revealed
  // while the late ones haven't started — the canonical staggered reveal,
  // rendered through batched RSXform draws.
  Host host;
  auto tree = [](Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"IIIIIIIIIIII", whiteStyle(32))
            .key("k")
            .fx({.effect = fx::rise(24),
                 .stagger = {.eachMs = 40, .durationMs = 200},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.0f));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect leftEdge = SkIRect::MakeLTRB(
      (int)b->left(), (int)b->top(), (int)b->left() + 24, (int)b->bottom());
  const SkIRect rightEdge = SkIRect::MakeLTRB(
      (int)b->right() - 24, (int)b->top(), (int)b->right(), (int)b->bottom());
  EXPECT_FALSE(anyWhiteIn(host, leftEdge));  // progress 0: nothing revealed
  host.composer.render(tree(0.45f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, leftEdge));    // head fully in
  EXPECT_FALSE(anyWhiteIn(host, rightEdge));  // tail not started
  host.composer.render(tree(1.0f));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, rightEdge));  // everything landed
}

TEST(ComposeKinetic, ATrackKeepsABlurredUnderlayBeneathTheStroke) {
  // A dressed style through the track's batched draw: a dark blurred
  // stroke underlay must stay BENEATH the light stroked foreground — the
  // halo hugs the letterform, the stroke keeps its colour — including
  // mid-cascade, when each glyph's own fade splits the style across
  // several paint buckets and a later letter's halo reaches a landed
  // letter's stroke. The reference is the same cascade with the style cut
  // into two tracked nodes, halo under stroke by stacking order: the one
  // dressed node must composite exactly as that split does.
  SkPaint stroke;
  stroke.setAntiAlias(true);
  stroke.setStyle(SkPaint::kStroke_Style);
  stroke.setStrokeWidth(4.0f);
  stroke.setColor(SK_ColorWHITE);
  SkPaint halo;
  halo.setAntiAlias(true);
  halo.setStyle(SkPaint::kStroke_Style);
  halo.setStrokeWidth(8.0f);
  halo.setColor(0xFF000000);
  const sigil::weave::PaintLayer blurredHalo =
      sigil::weave::PaintLayer::blurred(halo, 4);

  sigil::weave::TextStyle dressed;
  dressed.shaping.fontSize = 64.0f;
  dressed.paint.foreground = stroke;
  dressed.paint.underlays.push_back(blurredHalo);
  sigil::weave::TextStyle haloOnly = dressed;
  haloOnly.paint.foreground = blurredHalo.paint;
  haloOnly.paint.underlays.clear();
  sigil::weave::TextStyle strokeOnly = dressed;
  strokeOnly.paint.underlays.clear();

  constexpr float kMidCascade = 0.45f;
  const auto tracked = [&](const sigil::weave::TextStyle& style,
                           const char* key) {
    return text(u8"OOOOO", style)
        .key(key)
        .absolute()
        .inset(20, 20, 20, 20)
        .fx({.effect = fx::pop(),
             .stagger = {.eachMs = 30, .durationMs = 480},
             .progress = kMidCascade});
  };

  Host actual(420, 140);
  actual.composer.render(box().child(tracked(dressed, "word")));
  actual.frame();
  Host expected(420, 140);
  expected.composer.render(box()
                               .child(tracked(haloOnly, "halos"))
                               .child(tracked(strokeOnly, "strokes")));
  expected.frame();

  int worst = 0, worstX = -1, worstY = -1;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 420; ++x) {
      const SkColor a = actual.pixel(x, y);
      const SkColor b = expected.pixel(x, y);
      const int diff =
          std::max({std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
      if (diff > worst) {
        worst = diff;
        worstX = x;
        worstY = y;
      }
    }
  EXPECT_LE(worst, 4) << "the dressed node's underlay left its place under "
                         "the strokes at ("
                      << worstX << ", " << worstY << ")";
}

TEST(ComposeKinetic, TransitionedProgressPaintsLive) {
  // The master progress takes the full Animatable treatment: a with()
  // transition animates the reveal and the node paints live while moving.
  Host host;
  auto tree = [](Animatable<float> progress) {
    return box().padding(10).child(
        text(u8"POP", whiteStyle(40))
            .key("k")
            .fx({.effect = fx::pop(),
                 .stagger = {.eachMs = 20, .durationMs = 150},
                 .progress = std::move(progress)}));
  };
  host.composer.render(tree(0.001f));
  host.frame();
  host.composer.render(
      tree(animate(to(1.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);                                    // mid-ramp
  EXPECT_GT(host.composer.stats().nodesPainted, 0u);  // live while animating
  host.frame(0.3);                                    // settle
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())));
}

TEST(ComposeKinetic, ABoundProgressRevealsWithoutARedescribe) {
  // Glyph progress must be classified as CONTENT volatility, not paint-only:
  // it rebuilds glyph geometry, so the node's own recording is invalid the
  // moment it ticks. Classified paint-only, computeVolatile leaves
  // `ownContent` and `subtreeVolatile` false, the picture is never reset,
  // and the reveal FREEZES at whatever progress the last describe recorded.
  //
  // Driving the reveal from a BOUND Output is the only way to see that. Any
  // case that moves the reveal by RE-DESCRIBING marks the node paint-dirty
  // and hides the question entirely, and a case that only checks for ink
  // after settling is satisfied by a frozen half-revealed recording. Hence
  // both halves here: the tail must be dark before, and lit after, with no
  // describe in between.
  Host host;
  choreograph::Output<float> progress{0.0f};
  host.composer.render(box().padding(10).child(
      text(u8"IIIIIIIIIIII", whiteStyle(32))
          .key("k")
          .fx({.effect = fx::rise(24),
               .stagger = {.eachMs = 40, .durationMs = 200},
               .progress = &progress})));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const SkIRect tail = SkIRect::MakeLTRB((int)b->right() - 24, (int)b->top(),
                                         (int)b->right(), (int)b->bottom());
  ASSERT_FALSE(anyWhiteIn(host, tail))
      << "the tail was already revealed at progress 0, so the check below "
         "cannot tell a live reveal from a frozen one";

  progress = 1.0f;  // ONE describe, and the value moves underneath it
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, tail))
      << "the tail never appeared: the node replayed the recording it made at "
         "progress 0, so glyph progress is not invalidating its own picture";
}

namespace {

/** What one glyph's effect saw on one call. */
struct FxSample {
  GlyphInfo info;
  float t = 0;
  float random = 0;
};

/** An effect that draws nothing and RECORDS what it was asked. Every fx
 *  question below — which glyphs a selector addressed, which beat a
 *  cascade put them in, what local time they saw — is answerable from the
 *  samples one frame collects, without reading a single pixel. */
TextEffect probe(std::string key, std::vector<FxSample>* into) {
  return fx::effect(std::move(key),
                    [into](const GlyphInfo& g, float t, Rng& rng) {
                      into->push_back({g, t, rng.unit()});
                      return GlyphMod{};
                    });
}

/** The walk-order indices the samples cover. */
std::vector<size_t> addressed(const std::vector<FxSample>& samples) {
  std::vector<size_t> out;
  out.reserve(samples.size());
  for (const FxSample& s : samples) out.push_back(s.info.index);
  return out;
}

/** A track whose effect records, over the whole text unless told otherwise. */
Track probeTrack(std::vector<FxSample>* into, Selector where = {},
                 Stagger cascade = {.eachMs = 0, .durationMs = 100},
                 float progress = 1.0f) {
  return Track{.where = std::move(where),
               .effect = probe("probe", into),
               .stagger = std::move(cascade),
               .progress = progress};
}

}  // namespace

TEST(ComposeTextFx, AWordCascadeBeatsOncePerWordAndInOrder) {
  // The stagger runs over the track's UNITS, so a word-unit cascade gives
  // every glyph of a word one shared local time and delays the next word by
  // a whole beat. Per-glyph spacing inside a word would be a different
  // effect entirely, and is what `unit::Glyph` is for.
  Host host(300, 120);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB CCC", whiteStyle(20))
          .key("k")
          .fx(probeTrack(
              &samples, {},
              stagger(unit::Word, {.eachMs = 100, .durationMs = 100}), 0.5f))));
  host.frame();
  ASSERT_EQ(samples.size(), 9u) << "the probe did not see every glyph";

  // Three words, three beats: 0..2 / 3..5 / 6..8.
  for (size_t i = 0; i < 9; ++i)
    EXPECT_EQ(samples[i].info.unitIndex, (uint32_t)(i / 3))
        << "glyph " << i << " landed in the wrong beat";
  EXPECT_EQ(samples[0].info.unitCount, 3u);
  // Within a word every glyph shares its word's time…
  EXPECT_FLOAT_EQ(samples[0].t, samples[2].t);
  EXPECT_FLOAT_EQ(samples[3].t, samples[5].t);
  // …and the cascade runs head-first.
  EXPECT_GT(samples[0].t, samples[3].t);
  EXPECT_GT(samples[3].t, samples[6].t);
}

TEST(ComposeTextFx, ASentenceCascadeBeatsOncePerSentence) {
  Host host(400, 160);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"One two. Three four. Five.", whiteStyle(16))
          .key("k")
          .width(pct(100))
          .fx(probeTrack(
              &samples, {},
              stagger(unit::Sentence, {.eachMs = 100, .durationMs = 100}),
              0.5f))));
  host.frame();
  ASSERT_FALSE(samples.empty());
  EXPECT_EQ(samples.front().info.unitCount, 3u)
      << "ICU sentence segmentation did not produce three beats";
  // Every glyph of a sentence shares one time, and the sentences cascade.
  float previous = 2.0f;
  uint32_t previousUnit = ~0u;
  for (const FxSample& s : samples) {
    if (s.info.unitIndex != previousUnit) {
      EXPECT_LT(s.t, previous) << "sentence " << s.info.unitIndex
                               << " did not start after its predecessor";
      previous = s.t;
      previousUnit = s.info.unitIndex;
    } else {
      EXPECT_FLOAT_EQ(s.t, previous) << "one sentence split into two beats";
    }
  }
}

TEST(ComposeTextFx, AClusterIsOneBeatSoAMarkNeverLeavesItsLetter) {
  // The default unit is the CLUSTER, and that is the whole reason it is
  // the default: a base letter and its combining mark are one thing on the
  // page. Staggered per glyph the accent would fly on its own.
  Host host(300, 120);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"x́ýz", whiteStyle(28))
          .key("k")
          .fx(probeTrack(&samples, {}, {.eachMs = 100, .durationMs = 100},
                         0.5f))));
  host.frame();
  ASSERT_FALSE(samples.empty());
  const uint32_t beats = samples.front().info.unitCount;
  if (beats == samples.size())
    GTEST_SKIP() << "this font composed the marks into single glyphs, so "
                    "there is no multi-glyph cluster to keep together";
  // Glyphs at one text offset are one cluster and must share one time.
  for (size_t i = 1; i < samples.size(); ++i)
    if (samples[i].info.textIndex == samples[i - 1].info.textIndex) {
      EXPECT_EQ(samples[i].info.unitIndex, samples[i - 1].info.unitIndex);
      EXPECT_FLOAT_EQ(samples[i].t, samples[i - 1].t)
          << "a combining mark was staggered away from its base letter";
    }

  // THE CONTROL: unit::Glyph is the raw shaping unit and DOES separate
  // them. Without this the check above is satisfied by a cascade that
  // never beat at all.
  std::vector<FxSample> raw;
  host.composer.render(box().padding(10).child(
      text(u8"x́ýz", whiteStyle(28))
          .key("k")
          .fx(probeTrack(
              &raw, {},
              stagger(unit::Glyph, {.eachMs = 100, .durationMs = 400}),
              0.5f))));
  host.frame();
  ASSERT_EQ(raw.size(), samples.size());
  EXPECT_EQ(raw.front().info.unitCount, (uint32_t)raw.size());
  bool anySplit = false;
  for (size_t i = 1; i < raw.size(); ++i)
    if (raw[i].info.textIndex == raw[i - 1].info.textIndex &&
        raw[i].t != raw[i - 1].t)
      anySplit = true;
  EXPECT_TRUE(anySplit)
      << "unit::Glyph did not split the cluster, so the cluster case above "
         "proves nothing";
}

TEST(ComposeTextFx, TextFillAndTextStrokeTravelWithAMovingGlyph) {
  // A letter in flight is painted with the same glyph paint a resting one
  // is: textFill's material is its foreground and textStroke's outline is
  // an underlay, both carried through the batched draw. Shadowed instead,
  // a chrome wordmark loses its chrome the moment it starts moving.
  Host host(220, 140);
  const auto tree = [](bool moving) {
    Element t = text(u8"II", whiteStyle(48))
                    .key("k")
                    .textFill(Material::solid({0, 1, 0, 1}));
    if (moving)
      t.fx({.effect = fx::effect("still", [](const GlyphInfo&, float, Rng&) {
              return GlyphMod{};
            })});
    return box().padding(10).child(std::move(t));
  };
  const auto greenPixels = [&] {
    auto b = host.composer.bounds("k");
    EXPECT_TRUE(b.has_value());
    int count = 0;
    for (int y = (int)b->top(); y < (int)b->bottom(); ++y)
      for (int x = (int)b->left(); x < (int)b->right(); ++x) {
        const SkColor c = host.pixel(x, y);
        if (SkColorGetG(c) > 200 && SkColorGetR(c) < 80) ++count;
      }
    return count;
  };
  host.composer.render(tree(false));
  host.frame();
  const int resting = greenPixels();
  ASSERT_GT(resting, 20) << "textFill did not paint the resting glyphs green";
  host.composer.render(tree(true));
  host.frame();
  EXPECT_GT(greenPixels(), resting / 2)
      << "the fx path dropped textFill's material and painted the style's "
         "own foreground instead";
}

TEST(ComposeTextFx, SelectorsAddressWhatTheyName) {
  // Absolute forms, the regular expression and the substring, all resolved
  // against the paragraph and all comparable values.
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(run(sel::word(1)), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::words(1, 3)), (std::vector<size_t>{3, 4, 5, 6, 7, 8}));
  EXPECT_EQ(run(sel::text(u8"BBB")), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::regex(u8"B+")), (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sel::line(0)).size(), 9u);  // one line, everything
  EXPECT_EQ(run(Selector()).size(), 9u);    // default: everything
  EXPECT_TRUE(run(sel::regex(u8"([")).empty())
      << "a pattern that does not compile must select NOTHING rather than "
         "everything — silently addressing the whole paragraph is the "
         "failure this rule exists to prevent";
}

TEST(ComposeTextFx, SelectorAlgebraUnionsIntersectsAndComplements) {
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(run(sel::word(0) | sel::word(2)),
            (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_EQ(run(sel::words(0, 2) & sel::word(1)),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(!sel::word(1)), (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_TRUE(run(sel::word(0) & sel::word(1)).empty());
}

TEST(ComposeTextFx, EachTakeAndDropPartitionEveryUnitExactly) {
  // take(n) and drop(n) answer opposite sides of one cut inside every unit,
  // so together they cover the text exactly once. A gap or an overlap here
  // means a two-track composition double-counts letters it should split.
  Host host(300, 120);
  const auto run = [&](Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  const std::vector<size_t> firsts = run(sel::each(unit::Word).take(1));
  const std::vector<size_t> rest = run(sel::each(unit::Word).drop(1));
  EXPECT_EQ(firsts, (std::vector<size_t>{0, 3, 6}));
  EXPECT_EQ(rest, (std::vector<size_t>{1, 2, 4, 5, 7, 8}));
  std::vector<size_t> both = firsts;
  both.insert(both.end(), rest.begin(), rest.end());
  std::sort(both.begin(), both.end());
  std::vector<size_t> all(9);
  std::iota(all.begin(), all.end(), 0u);
  EXPECT_EQ(both, all) << "take/drop is not a partition of the units";
}

TEST(ComposeTextFx, RandomOriginIsAStableScatterAcrossFrames) {
  // A seeded cascade must give the SAME order every frame: a scatter that
  // reshuffles can never settle, so it can never cache, and it flickers.
  std::vector<FxSample> first, second;
  const auto tree = [&](std::vector<FxSample>* into) {
    return box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(into, {},
                           {.eachMs = 100,
                            .durationMs = 100,
                            .from = Stagger::From::Random},
                           0.5f)));
  };
  // TWO HOSTS, one paint each: re-describing the same tracks into one host
  // prunes and replays the recording, which is the right behaviour and
  // would leave this test measuring nothing.
  Host hostA(300, 120), hostB(300, 120);
  hostA.composer.render(tree(&first));
  hostA.frame();
  hostB.composer.render(tree(&second));
  hostB.frame();
  ASSERT_EQ(first.size(), second.size());
  ASSERT_FALSE(first.empty());
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_FLOAT_EQ(first[i].t, second[i].t)
        << "the seeded cascade reshuffled between frames";
  // …and it really is a scatter, not the Start ladder wearing a new name.
  bool anyOutOfOrder = false;
  for (size_t i = 1; i < first.size(); ++i)
    if (first[i].t > first[i - 1].t) anyOutOfOrder = true;
  EXPECT_TRUE(anyOutOfOrder) << "Random produced the Start ordering";
  // The Rng an effect draws from is seeded per glyph and is stable too.
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_FLOAT_EQ(first[i].random, second[i].random);
}

TEST(ComposeTextFx, RandomSeedDealsItsOwnScatterAndZeroKeepsTheDefault) {
  // From::Random ranks units by a hash keyed on the count and Stagger::seed.
  // Three claims, each a behaviour an author leans on: seed 0 IS the
  // count-keyed deal (pinned against the exact ranks that key hashes to, so
  // every settled scene keeps its scatter bit for bit), a nonzero seed deals
  // a different permutation, and two nonzero seeds deal independently.
  const auto ranksOf = [](uint32_t seed) {
    Host host(300, 120);
    Stagger scatter{
        .eachMs = 100, .durationMs = 100, .from = Stagger::From::Random};
    scatter.seed = seed;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx({.effect = fx::rise(6), .stagger = scatter})));
    host.frame();
    const std::vector<Beat> beats = host.composer.beatsOf("k", 0);
    std::vector<int> ranks;
    ranks.reserve(beats.size());
    for (const Beat& b : beats) ranks.push_back((int)(b.startMs / 100.0f));
    return ranks;
  };
  // The count-9 permutation the count-alone key hashes to. Recomputing it
  // here would restate the implementation; these NUMBERS are the pin.
  EXPECT_EQ(ranksOf(0), (std::vector<int>{8, 7, 0, 2, 5, 1, 6, 4, 3}));
  const std::vector<int> dealt42 = ranksOf(42), dealt7 = ranksOf(7);
  EXPECT_NE(dealt42, ranksOf(0));
  EXPECT_NE(dealt42, dealt7);
  // Still the scrambled even ladder: every rank 0..N-1 exactly once, so no
  // two units ever open together however the seed shuffles them.
  std::vector<int> sorted = dealt42;
  std::sort(sorted.begin(), sorted.end());
  std::vector<int> ladder(sorted.size());
  std::iota(ladder.begin(), ladder.end(), 0);
  EXPECT_EQ(sorted, ladder) << "a seeded scatter dropped or doubled a rank";
  // A different seed is a different cascade to the reconciler, or a
  // re-described field would prune onto the old scatter and keep it.
  Stagger a{.from = Stagger::From::Random}, b{.from = Stagger::From::Random};
  b.seed = 42;
  EXPECT_FALSE(a == b);
  a.seed = 42;
  EXPECT_TRUE(a == b);
}

TEST(ComposeTextFx, NestedStaggerDelaysGlyphsInsideTheirWordsBeat) {
  // then() compounds: each word gets its beat, and inside that beat each
  // glyph gets its own start. Two ladders, one master progress.
  Host host(300, 120);
  std::vector<FxSample> samples;
  Stagger cascade = stagger(unit::Word, {.eachMs = 200, .durationMs = 100});
  cascade.then(unit::Glyph, {.eachMs = 50, .durationMs = 100});
  // A beat is 100 + 50·2 = 200 ms and the whole cascade spans 400; 0.3 of
  // that lands inside the first word's beat, where the two ladders are
  // both readable.
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB", whiteStyle(20))
          .key("k")
          .fx(probeTrack(&samples, {}, cascade, 0.3f))));
  host.frame();
  ASSERT_EQ(samples.size(), 6u);
  // Inside a word the glyphs no longer share a time — the inner ladder ran.
  EXPECT_GT(samples[0].t, samples[1].t);
  EXPECT_GT(samples[1].t, samples[2].t);
  // …and the second word still starts a whole beat behind the first.
  EXPECT_GT(samples[0].t, samples[3].t);
}

TEST(ComposeTextFx, TwoTracksComposeByAddingOffsets) {
  // The algebra: offsets add. Two tracks each pushing 30 px right put the
  // glyph 60 px right of rest, which is the only reading under which
  // stacking tracks is composition rather than replacement.
  Host host(220, 120);
  const auto shove = [](float dx) {
    return fx::effect(
        "shove" + std::to_string((int)dx),
        [dx](const GlyphInfo&, float, Rng&) {
          GlyphMod m;
          m.dx = dx;
          return m;
        },
        /*reach=*/80.0f);
  };
  host.composer.render(box().padding(10).child(text(u8"I", whiteStyle(40))
                                                   .key("k")
                                                   .fx({.effect = shove(30)})
                                                   .fx({.effect = shove(30)})));
  host.frame();
  auto b = host.composer.bounds("k");
  ASSERT_TRUE(b.has_value());
  const int top = (int)b->top(), bottom = (int)b->bottom();
  EXPECT_FALSE(anyWhiteIn(
      host, SkIRect::MakeLTRB((int)b->left(), top, (int)b->right(), bottom)))
      << "the glyph never left its rest position";
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left() + 55, top,
                                                 (int)b->left() + 75, bottom)))
      << "two 30 px tracks did not add to 60 px";
}

TEST(ComposeTextFx, ATrackReachKeepsAWideThrowInsideTheCull) {
  // ownPaintBounds has no idea what an effect will do, so a track that
  // throws a glyph out of the element's box must SAY how far. Under-report
  // and the cached picture truncates it with no diagnostic — which is
  // exactly the control below.
  Host host(220, 200);
  const auto drop = [](float reach) {
    Track t{.effect = fx::effect(
                "drop",
                [](const GlyphInfo&, float, Rng&) {
                  GlyphMod m;
                  m.dy = 60;
                  return m;
                },
                /*reach=*/0.0f)};
    t.reach = reach;
    return t;
  };
  const auto inkBelow = [&](float reach) {
    host.composer.render(box().padding(10).child(text(u8"I", whiteStyle(40))
                                                     .key("k")
                                                     .cache(Cache::Texture)
                                                     .fx(drop(reach))));
    host.frame();
    auto b = host.composer.bounds("k");
    EXPECT_TRUE(b.has_value());
    return anyWhiteIn(
        host, SkIRect::MakeLTRB((int)b->left(), (int)b->bottom() + 20,
                                (int)b->right(), (int)b->bottom() + 70));
  };
  EXPECT_TRUE(inkBelow(80.0f)) << "a declared reach did not reach the cull";
  EXPECT_FALSE(inkBelow(0.0f))
      << "the control did not truncate, so the check above proves nothing "
         "about the cull";
}

TEST(ComposeTextFx, EqualTrackListsPruneAndAKeyedLambdaComparesByKey) {
  // Tracks are values, so a re-describe that says the same thing must not
  // mark the node dirty. Without it every text node carrying fx re-records
  // on every frame, whatever its progress is doing.
  Host host(220, 120);
  const auto tree = [](TextEffect effect) {
    return box().padding(10).child(text(u8"AAA", whiteStyle(20))
                                       .key("k")
                                       .fx({.effect = std::move(effect)}));
  };
  host.composer.render(tree(fx::rise(20)));
  host.frame();
  host.composer.render(tree(fx::rise(20)));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged track list did not prune";
  host.composer.render(tree(fx::rise(24)));
  host.frame();
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a changed preset parameter pruned anyway";

  // A keyed lambda compares by its KEY: same key prunes, different key does
  // not. That is the whole contract fx::effect() asks of its caller.
  const auto body = [](const GlyphInfo&, float, Rng&) { return GlyphMod{}; };
  host.composer.render(tree(fx::effect("mine", body)));
  host.frame();
  host.composer.render(tree(fx::effect("mine", body)));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "two effects under one key did not compare equal";
  host.composer.render(tree(fx::effect("other", body)));
  host.frame();
  EXPECT_GT(host.composer.stats().patchedNodes, 0u)
      << "a re-keyed effect pruned onto the old one";
}

TEST(ComposeTextFx, SettledMultiTrackTextStopsPaintingLive) {
  // Two bound progresses that have held still for long enough release their
  // volatility, exactly as one does — the settle machinery covers EVERY
  // track's progress or a second track pins the node live forever.
  Host host(220, 120);
  choreograph::Output<float> a{0.0f}, b{0.0f};
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB", whiteStyle(20))
          .key("k")
          .fx({.effect = fx::rise(12), .progress = &a})
          .fx({.effect = fx::slide(-8), .progress = &b})));
  host.frame();
  a = 1.0f;
  b = 1.0f;
  for (int i = 0; i < 12; ++i) host.frame(0.016);  // the release warms up
  unsigned paints = 0, records = 0;
  for (int i = 0; i < 5; ++i) {
    host.frame(0.016);
    paints += host.composer.stats().nodesPainted;
    records += host.composer.stats().picturesRecorded;
  }
  EXPECT_EQ(paints, 0u) << "settled multi-track text kept painting live";
  EXPECT_EQ(records, 0u) << "settled multi-track text kept re-recording";

  // …and the frame either progress moves again, the node must re-declare
  // before a stale recording replays.
  b = 0.2f;
  host.frame(0.016);
  EXPECT_GT(host.composer.stats().nodesPainted, 0u)
      << "the second track moved and nothing re-declared";
}

namespace {

/** An effect that reports a constant dy, so a composition's arithmetic is
 *  readable straight off the returned GlyphMod. */
TextEffect constantDy(float dy) {
  return fx::effect("constantDy" + std::to_string((int)dy),
                    [dy](const GlyphInfo&, float, Rng&) {
                      GlyphMod m;
                      m.dy = dy;
                      return m;
                    });
}

/** An effect that reports the local time it was handed. */
TextEffect reportT() {
  return fx::effect("reportT", [](const GlyphInfo&, float t, Rng&) {
    GlyphMod m;
    m.dy = t;
    return m;
  });
}

GlyphMod evaluate(const TextEffect& effect, float t) {
  GlyphInfo g;
  Rng rng(1);
  return effect(g, t, rng);
}

}  // namespace

TEST(ComposeTextFx, SeqRenormalizesEachPhaseOverItsOwnWindow) {
  // Each phase sees a full 0→1 across its slice of local time — that is
  // what makes a sequence a sequence rather than three effects sharing one
  // clock and each playing a third of its curve.
  const TextEffect sequence = fx::seq(reportT().until(0.5f), reportT());
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.0f).dy, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.25f).dy, 0.5f);  // half of phase one
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.75f).dy, 0.5f);  // half of phase two
  EXPECT_FLOAT_EQ(evaluate(sequence, 1.0f).dy, 1.0f);
  // The last phase runs to the end whatever it was declared with.
  const TextEffect short_ =
      fx::seq(reportT().until(0.5f), reportT().until(0.6f));
  EXPECT_FLOAT_EQ(evaluate(short_, 1.0f).dy, 1.0f);
}

TEST(ComposeTextFx, SeqCutsHardByDefaultAndLerpsAcrossAnXfade) {
  const TextEffect cut = fx::seq(constantDy(10).until(0.5f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(cut, 0.49f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(cut, 0.51f).dy, 0.0f);

  const TextEffect faded =
      fx::seq(constantDy(10).until(0.5f).xfade(0.2f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(faded, 0.29f).dy, 10.0f);    // before the window
  EXPECT_NEAR(evaluate(faded, 0.40f).dy, 5.0f, 1e-4f);  // halfway across
  EXPECT_NEAR(evaluate(faded, 0.50f).dy, 0.0f, 1e-4f);  // at the joint
  EXPECT_FLOAT_EQ(evaluate(faded, 0.60f).dy, 0.0f);     // past it
}

TEST(ComposeTextFx, MixEvaluatesBothAndComposesByTheTrackAlgebra) {
  const TextEffect both = fx::mix(constantDy(10), constantDy(4));
  EXPECT_FLOAT_EQ(evaluate(both, 0.5f).dy, 14.0f);  // offsets add
  const auto half = [](float scale) {
    return fx::effect("half" + std::to_string((int)(scale * 10)),
                      [scale](const GlyphInfo&, float, Rng&) {
                        GlyphMod m;
                        m.scale = scale;
                        m.alpha = scale;
                        return m;
                      });
  };
  const TextEffect scaled = fx::mix(half(0.5f), half(0.5f));
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).scale, 0.25f);  // scale multiplies
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).alpha, 0.25f);  // …and so does alpha
}

TEST(ComposeTextFx, CombinatorsAreComparableWhenTheirOperandsAre) {
  EXPECT_TRUE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
              fx::seq(fx::rise(20).until(0.5f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(20).until(0.6f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(22).until(0.5f), fx::pop()));
  EXPECT_TRUE(fx::mix(fx::rise(20), fx::slide()) ==
              fx::mix(fx::rise(20), fx::slide()));
  EXPECT_FALSE(fx::mix(fx::rise(20), fx::slide()) ==
               fx::mix(fx::slide(), fx::rise(20)));
}

// THE PLACEMENT FACT IS INFERRED wherever the data allows it, because it is
// the data: a preset's deviation is its own body, a table's mods are its
// entries, and a combinator can only do what its operands do. Only an
// ad-hoc lambda is opaque, and that door assumes motion.
TEST(ComposeTextFx, EveryEffectAnswersWhetherItMovesItsGlyphs) {
  // The presets that move geometry.
  EXPECT_TRUE(fx::rise().displaces());
  EXPECT_TRUE(fx::slide().displaces());
  EXPECT_TRUE(fx::pop().displaces());
  EXPECT_TRUE(fx::spinIn().displaces());
  EXPECT_TRUE(fx::scatter().displaces());
  EXPECT_TRUE(fx::waveLoop().displaces());
  // …and the ones that touch coverage, colour or the outline only, leaving
  // every pen position exactly where the layout put it.
  EXPECT_FALSE(fx::typeOn().displaces());
  EXPECT_FALSE(TextEffect::variableAxis("GRAD", 80).displaces());
  EXPECT_FALSE(fx::variableAxisSweep("GRAD", 0, 80).displaces());
  EXPECT_FALSE(fx::tint(SkColors::kGray, SkColors::kWhite).displaces());
  EXPECT_FALSE(fx::scramble().displaces());

  // A TABLE ANSWERS FROM ITS OWN ENTRIES. Colour and coverage are not
  // placement…
  EXPECT_FALSE(fx::keys({{0.0f, {.alpha = 0.0f}}, {1.0f, {}}}).displaces());
  EXPECT_FALSE(
      fx::keys({{0.0f, {.colorMul = {0.2f, 0.2f, 0.2f, 1}}}, {1.0f, {}}})
          .displaces());
  // …and every lane that is.
  EXPECT_TRUE(fx::keys({{0.0f, {.dx = 12.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.dy = 12.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.scale = 1.4f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.rotateDeg = 8.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.scaleX = 1.2f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.skewXDeg = 6.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.skewYDeg = 6.0f}}, {1.0f, {}}}).displaces());

  // A COMBINATOR DERIVES: any operand it may evaluate moving is enough, and
  // none of them moving is enough the other way. `fx::hold` vetoes with
  // alpha, which places nothing, so it is its operand's answer.
  EXPECT_FALSE(fx::mix(fx::typeOn(), fx::scramble()).displaces());
  EXPECT_TRUE(fx::mix(fx::typeOn(), fx::rise()).displaces());
  EXPECT_FALSE(fx::seq(fx::typeOn().until(0.5f), fx::scramble()).displaces());
  EXPECT_TRUE(fx::seq(fx::typeOn().until(0.5f), fx::rise()).displaces());
  EXPECT_FALSE(fx::hold(fx::scramble()).displaces());
  EXPECT_TRUE(fx::hold(fx::rise()).displaces());
  // Nesting keeps the derivation exact rather than sticky.
  EXPECT_FALSE(fx::mix(fx::seq(fx::tint(SkColors::kGray, SkColors::kWhite)),
                       fx::hold(fx::typeOn()))
                   .displaces());

  // A PASS IS NOT A PLACEMENT: its shader runs over pixels already
  // rasterized at the resting origins.
  EXPECT_FALSE(fx::pass(Material::sksl("half4 main(float2 xy) { return "
                                       "uContent.eval(xy); }"))
                   .displaces());

  // THE OPAQUE DOOR assumes motion, and takes the author's word otherwise.
  const GlyphModFn still = [](const GlyphInfo&, float, Rng&) {
    GlyphMod m;
    m.alpha = 0.5f;
    return m;
  };
  EXPECT_TRUE(fx::effect("opaque", still).displaces());
  EXPECT_FALSE(fx::effect("opaque", still).displacing(false).displaces());
  // …and the declaration rides the params, so two bodies under one key that
  // disagree about placement do not prune onto each other.
  EXPECT_FALSE(fx::effect("opaque", still) ==
               fx::effect("opaque", still).displacing(false));
  EXPECT_TRUE(fx::effect("opaque", still).displacing(false) ==
              fx::effect("opaque", still).displacing(false));
}

TEST(ComposeTextFx, KeysReproducesEveryEntryAtItsOwnPosition) {
  // A published table is a promise about the moments it names. Whatever the
  // curve between them, the deviation AT an entry is the entry.
  const std::vector<fx::Key> table = {
      {0.00f, {}},
      {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
      {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
      {1.00f, {}}};
  const TextEffect rubber = fx::keys(table, &choreograph::easeInOutCubic);
  for (const fx::Key& key : table) {
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleX, key.mod.scaleX)
        << "scaleX at " << key.at;
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleY, key.mod.scaleY)
        << "scaleY at " << key.at;
  }
  // Outside the table's own span it HOLDS at the ends rather than
  // extrapolating numbers nobody published.
  EXPECT_FLOAT_EQ(evaluate(rubber, -0.5f).scaleX, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(rubber, 2.0f).scaleX, 1.0f);
}

TEST(ComposeTextFx, KeysEasesEachSegmentOnItsOwn) {
  // THE WHOLE CURVE, EVERY SEGMENT — which is what a keyframe list means
  // and what one curve stretched across the table would not be. Three
  // entries are the fewest that can tell the two apart.
  const std::vector<fx::Key> ramp = {
      {0.0f, {}}, {0.5f, {.dy = 10.0f}}, {1.0f, {}}};
  const TextEffect linear = fx::keys(ramp);
  EXPECT_FLOAT_EQ(evaluate(linear, 0.125f).dy, 2.5f);

  const TextEffect eased = fx::keys(ramp, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(eased, 0.5f).dy, 10.0f);  // the entry is still exact
  EXPECT_LT(evaluate(eased, 0.125f).dy, 1.5f)  // …the middle is not linear
      << "a quarter of the way into the first segment the reading is the "
         "linear one, so the curve was not applied to the segment";
  // The second segment runs the same curve over its own span: a quarter in
  // and a quarter from the end of the two segments are mirror readings.
  EXPECT_NEAR(evaluate(eased, 0.375f).dy, evaluate(eased, 0.625f).dy, 1e-4f);

  // A per-entry curve governs the segment that OPENS at that entry, and no
  // other.
  std::vector<fx::Key> mixed = ramp;
  mixed[0].ease = &choreograph::easeNone;
  const TextEffect part = fx::keys(mixed, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(part, 0.125f).dy, 2.5f);
  EXPECT_NEAR(evaluate(part, 0.625f).dy, evaluate(eased, 0.625f).dy, 1e-4f);
}

TEST(ComposeTextFx, KeysCutsASubstitutionAndLerpsAMatchingAxis) {
  // The seq crossfade's rules, because it is the same arithmetic: there is
  // no half-way glyph between two outlines, and an axis is the one
  // substitution with a continuum — and only between two entries naming the
  // SAME axis.
  const TextEffect letters =
      fx::keys({{0.0f, {.codepoint = U'A'}}, {1.0f, {.codepoint = U'B'}}});
  EXPECT_EQ(evaluate(letters, 0.40f).codepoint, U'A');
  EXPECT_EQ(evaluate(letters, 0.60f).codepoint, U'B');

  const sigil::weave::FontVariation light("GRAD", 400.0f);
  const sigil::weave::FontVariation heavy("GRAD", 800.0f);
  const TextEffect swept =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = heavy}}});
  const GlyphMod midway = evaluate(swept, 0.5f);
  ASSERT_TRUE(midway.axis.has_value());
  EXPECT_FLOAT_EQ(midway.axis.value_or(sigil::weave::FontVariation()).value,
                  600.0f);

  const sigil::weave::FontVariation slant("slnt", -10.0f);
  const TextEffect crossed =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = slant}}});
  const auto tagOf = [](const GlyphMod& mod) {
    return mod.axis ? std::string(mod.axis->tag, 4) : std::string("(unset)");
  };
  EXPECT_EQ(tagOf(evaluate(crossed, 0.4f)), "GRAD");
  EXPECT_EQ(tagOf(evaluate(crossed, 0.6f)), "slnt")
      << "two different axes were averaged, which names a coordinate on "
         "neither of them";
}

TEST(ComposeTextFx, AKeyTableIsComparableByItsNumbersAndItsCurves) {
  const auto table = [](float peak) {
    return std::vector<fx::Key>{
        {0.0f, {}}, {0.5f, {.scaleY = peak}}, {1.0f, {}}};
  };
  EXPECT_TRUE(fx::keys(table(1.25f)) == fx::keys(table(1.25f)));
  EXPECT_FALSE(fx::keys(table(1.25f)) == fx::keys(table(1.30f)));
  // The curve is part of the identity. A table re-eased is a different
  // motion, and an effect comparing equal to the one it replaced would go
  // on drawing the old one with no diagnostic.
  EXPECT_FALSE(fx::keys(table(1.25f)) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_FALSE(fx::keys(table(1.25f), &choreograph::easeOutQuad) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_TRUE(fx::keys(table(1.25f), &choreograph::easeInOutCubic) ==
              fx::keys(table(1.25f), &choreograph::easeInOutCubic));
}

TEST(ComposeTextFx, AKeyedTrackPrunesWhenItsTableIsUnchanged) {
  Host host;
  const auto tree = [] {
    return box().padding(10).child(
        text(u8"KEYS", whiteStyle(28))
            .key("k")
            .fx({.effect =
                     fx::keys({{0.0f, {}}, {0.5f, {.dy = -8.0f}}, {1.0f, {}}},
                              &choreograph::easeInOutCubic)}));
  };
  host.composer.render(tree());
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  host.composer.render(tree());  // fresh Elements, an identical table
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame(0.016);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // The control: a table with one number moved is a different value, and
  // the node it describes has to be patched.
  host.composer.render(box().padding(10).child(
      text(u8"KEYS", whiteStyle(28))
          .key("k")
          .fx({.effect =
                   fx::keys({{0.0f, {}}, {0.5f, {.dy = -9.0f}}, {1.0f, {}}},
                            &choreograph::easeInOutCubic)})));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeTextFx, HoldWithholdsTheEffectUntilTheBeatOpens) {
  const TextEffect held = fx::hold(constantDy(10));
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).dy, 0.0f)
      << "the wrapped effect ran on a beat that had not opened";
  // …and the moment it opens, EXACTLY the wrapped effect.
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 1.0f).dy, 10.0f);

  EXPECT_TRUE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(20)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(22)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::rise(20));
  EXPECT_FLOAT_EQ(fx::hold(fx::rise(20)).reach(), fx::rise(20).reach());
}

namespace {

/** An effect under `key` returning one fixed deviation — the readable way
 *  to drive a single GlyphMod field from a test. */
TextEffect fixed(std::string key, GlyphMod mod) {
  return fx::effect(
      std::move(key), [mod](const GlyphInfo&, float, Rng&) { return mod; },
      /*reach=*/120.0f);
}

/** The bounding box of everything that is not the cleared background. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  SkIRect bounds = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        const SkIRect pixel = SkIRect::MakeXYWH(x, y, 1, 1);
        if (bounds.isEmpty())
          bounds = pixel;
        else
          bounds.join(pixel);
      }
  return bounds;
}

/** The mean y of the inked pixels on columns [left, right). */
float inkCentroidY(Host& host, int h, int left, int right) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(right - left, h));
  host.surface->readPixels(bitmap.pixmap(), left, 0);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < right - left; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += y;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

/** The mean x of the inked pixels on rows [top, bottom). */
float inkCentroidX(Host& host, int w, int top, int bottom) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, bottom - top));
  host.surface->readPixels(bitmap.pixmap(), 0, top);
  double sum = 0;
  int count = 0;
  for (int y = 0; y < bottom - top; ++y)
    for (int x = 0; x < w; ++x)
      if (bitmap.getColor(x, y) != SK_ColorBLACK) {
        sum += x;
        ++count;
      }
  return count ? (float)(sum / count) : 0.0f;
}

}  // namespace

TEST(ComposeTextFx, ColorMulTintsEveryPassOfADressedGlyph) {
  // The multiplier is a statement about the GLYPH, not about its fill: it
  // has to reach the underlays and overlays a span was styled with too.
  // Reaching only the foreground would leave a tinted letter wearing its
  // old drop shadow, which is the bug this test exists to name.
  sigil::weave::TextStyle shadowed = whiteStyle(52);
  shadowed.paint.addUnderlay(sigil::weave::PaintLayer(SK_ColorRED, {16, 0}));

  const auto render = [&](Host& host, std::string key, SkColor4f tint) {
    GlyphMod mod;
    mod.colorMul = tint;
    host.composer.render(box().padding(10).child(
        text(u8"I", shadowed)
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  const auto count = [](Host& host, auto&& predicate) {
    int hits = 0;
    for (int y = 0; y < 140; ++y)
      for (int x = 0; x < 140; ++x)
        if (predicate(host.pixel(x, y))) ++hits;
    return hits;
  };
  const auto reddish = [](SkColor c) {
    return SkColorGetR(c) > 150 && SkColorGetG(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto greenish = [](SkColor c) {
    return SkColorGetG(c) > 150 && SkColorGetR(c) < 80 && SkColorGetB(c) < 80;
  };
  const auto whitish = [](SkColor c) {
    return SkColorGetR(c) > 200 && SkColorGetG(c) > 200 && SkColorGetB(c) > 200;
  };

  Host plain(140, 140);
  render(plain, "plain", {1, 1, 1, 1});
  ASSERT_GT(count(plain, reddish), 10) << "the underlay pass never painted";
  ASSERT_GT(count(plain, whitish), 10) << "the foreground pass never painted";

  Host tinted(140, 140);
  render(tinted, "greenOnly", {0, 1, 0, 1});
  EXPECT_EQ(count(tinted, reddish), 0)
      << "the underlay kept its red under a tint that multiplies red by zero";
  EXPECT_EQ(count(tinted, whitish), 0) << "the foreground kept its white";
  EXPECT_GT(count(tinted, greenish), 10)
      << "the tint painted nothing at all — the glyph vanished instead";
}

namespace {

/** Every pixel of the host's surface, for a byte-for-byte compare. */
std::vector<uint8_t> surfaceBytes(Host& host, int w, int h) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  const auto* data = (const uint8_t*)bitmap.getPixels();
  return {data, data + bitmap.computeByteSize()};
}

/** A style whose ink is one flat grey — headroom in every channel, so an
 *  added or screened term has somewhere to go. */
sigil::weave::TextStyle greyStyle(float size, float level) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor4f({level, level, level, 1.0f}, nullptr);
  return s;
}

/** One glyph under one or two colour-term tracks, drawn and read back. */
std::vector<uint8_t> renderColorTracks(
    std::vector<std::pair<std::string, GlyphMod>> tracks,
    float greyLevel = 0.25f, bool continuous = false) {
  Host host(140, 140);
  Element leaf = text(u8"I", greyStyle(52, greyLevel)).key("k");
  for (auto& [key, mod] : tracks)
    leaf.fx({.effect = fixed(key, mod), .continuous = continuous});
  host.composer.render(box().padding(10).child(std::move(leaf)));
  host.frame();
  return surfaceBytes(host, 140, 140);
}

}  // namespace

TEST(ComposeTextFx, ColorAddAddsAcrossTracksAndClampsAtTheDraw) {
  // One flash brightens: an added half-red over a quarter-grey glyph reads
  // red-forward where the untouched glyph reads flat grey.
  GlyphMod addHalfRed;
  addHalfRed.colorAdd = {0.5f, 0, 0, 0};
  const std::vector<uint8_t> flashed =
      renderColorTracks({{"addHalfRed", addHalfRed}});
  const std::vector<uint8_t> plain = renderColorTracks({});
  EXPECT_NE(flashed, plain) << "an added term changed nothing";

  // TWO tracks ADD — 0.5 + 0.75 — and the sum clamps ONCE at the draw, so
  // the pair is byte-identical to a single track adding full red. Clamping
  // per track instead would land at 1.25-before-snap only by accident; the
  // law is sum-then-clamp, and byte identity against the saturated single
  // track is that law observed.
  GlyphMod addMoreRed;
  addMoreRed.colorAdd = {0.75f, 0, 0, 0};
  GlyphMod addFullRed;
  addFullRed.colorAdd = {1.0f, 0, 0, 0};
  EXPECT_EQ(renderColorTracks(
                {{"addHalfRed", addHalfRed}, {"addMoreRed", addMoreRed}}),
            renderColorTracks({{"addFullRed", addFullRed}}))
      << "0.5 + 0.75 across two tracks must draw as a clamped 1.0";
}

TEST(ComposeTextFx, ColorScreenScreensCommutativelyAcrossTracks) {
  GlyphMod screenRed;
  screenRed.colorScreen = {0.5f, 0, 0, 0};
  GlyphMod screenGreen;
  screenGreen.colorScreen = {0, 0.5f, 0, 0};
  // Order-free: 1 − (1−a)(1−b) reads the same both ways, so two glow
  // tracks land identically whichever is declared first.
  EXPECT_EQ(renderColorTracks({{"sR", screenRed}, {"sG", screenGreen}}),
            renderColorTracks({{"sG", screenGreen}, {"sR", screenRed}}))
      << "screen composition depended on track order";
  // …and the arithmetic is the screen blend itself: half screened twice is
  // 1 − 0.5·0.5 = 0.75 screened once. Both sides land on the snap ladder
  // (16/32 and 24/32), so the compare is exact.
  GlyphMod screenThreeQuarters;
  screenThreeQuarters.colorScreen = {0.75f, 0, 0, 0};
  EXPECT_EQ(renderColorTracks({{"sR", screenRed}, {"sR2", screenRed}}),
            renderColorTracks({{"s34", screenThreeQuarters}}))
      << "two half screens must compose to one three-quarter screen";
}

TEST(ComposeTextFx, TheColourTermsLerpComponentwiseInAKeysTable) {
  // A keys segment interpolates the two terms channel by channel, so the
  // table's midpoint draws exactly as a constant half-strength term does.
  // Local t is pinned at 0.5 by a zero-spread cascade and progress 0.5;
  // 0.5 sits on the snap ladder, so the compare is byte-exact.
  const auto renderKeysAt = [](TextEffect effect) {
    Host host(140, 140);
    host.composer.render(
        box().padding(10).child(text(u8"I", greyStyle(52, 0.25f))
                                    .key("k")
                                    .fx({.effect = std::move(effect),
                                         .stagger = {.eachMs = 0},
                                         .progress = 0.5f})));
    host.frame();
    return surfaceBytes(host, 140, 140);
  };
  GlyphMod fullAdd;
  fullAdd.colorAdd = {1.0f, 0, 0, 0};
  GlyphMod halfAdd;
  halfAdd.colorAdd = {0.5f, 0, 0, 0};
  EXPECT_EQ(renderKeysAt(fx::keys({{0.0f, {}}, {1.0f, fullAdd}})),
            renderKeysAt(fixed("halfAddK", halfAdd)))
      << "colorAdd did not lerp componentwise across a keys segment";
  GlyphMod fullScreen;
  fullScreen.colorScreen = {0, 1.0f, 0, 0};
  GlyphMod halfScreen;
  halfScreen.colorScreen = {0, 0.5f, 0, 0};
  EXPECT_EQ(renderKeysAt(fx::keys({{0.0f, {}}, {1.0f, fullScreen}})),
            renderKeysAt(fixed("halfScreenK", halfScreen)))
      << "colorScreen did not lerp componentwise across a keys segment";
  // The terms are part of a table's identity: two tables differing only in
  // a colour term are two different effects, and must not prune onto each
  // other.
  EXPECT_FALSE(fx::keys({{0.0f, {}}, {1.0f, fullAdd}}) ==
               fx::keys({{0.0f, {}}, {1.0f, halfAdd}}));
  EXPECT_FALSE(fx::keys({{0.0f, {}}, {1.0f, fullScreen}}) ==
               fx::keys({{0.0f, {}}, {1.0f, halfScreen}}));
}

TEST(ComposeTextFx, NeutralColourTermsKeepTheFastPathByteIdentical) {
  // A deviation that spells the neutral terms outright must draw the very
  // bytes one that never mentions them draws: neutral means the untouched
  // source paint, not an identity-shaped filter over it.
  GlyphMod spelled;
  spelled.dy = -4.0f;
  spelled.colorAdd = {0, 0, 0, 0};
  spelled.colorScreen = {0, 0, 0, 0};
  GlyphMod silent;
  silent.dy = -4.0f;
  EXPECT_EQ(renderColorTracks({{"spelledNeutral", spelled}}),
            renderColorTracks({{"silentNeutral", silent}}));
}

TEST(ComposeTextFx, TheSnapLadderBoundsTheColourTermsAndContinuousLiftsIt) {
  // Below half a ladder step the snapped term rounds to neutral — that
  // rounding is what bounds the memoized filter population — and
  // Track::continuous lifts it, letting the raw value through at the cost
  // the opt-out names.
  GlyphMod faint;
  faint.colorAdd = {0.01f, 0.01f, 0.01f, 0};
  EXPECT_EQ(renderColorTracks({{"faintAdd", faint}}, 0.5f),
            renderColorTracks({}, 0.5f))
      << "a term below half a snap step must round to neutral";
  EXPECT_NE(renderColorTracks({{"faintAdd", faint}}, 0.5f,
                              /*continuous=*/true),
            renderColorTracks({}, 0.5f, /*continuous=*/true))
      << "continuous must lift the snap and let the faint term through";
}

TEST(ComposeTextFx, TheFilterPathAgreesWithTheFlatColourPath) {
  // A pass whose colour is decided by a shader takes the colour terms as a
  // memoized matrix filter; a flat pass takes them in its colour. Same
  // arithmetic by contract — multiply, add, clamp, then screen — so the
  // two paths must land within rounding of each other.
  GlyphMod mod;
  mod.colorMul = {0.5f, 1.0f, 1.0f, 1.0f};
  mod.colorAdd = {0.25f, 0.25f, 0, 0};
  mod.colorScreen = {0, 0.5f, 0.5f, 0};
  const auto render = [&](Host& host, bool shaderFill) {
    sigil::weave::TextStyle style = greyStyle(52, 0.5f);
    if (shaderFill)
      style.paint.foreground.setShader(
          SkShaders::Color({0.5f, 0.5f, 0.5f, 1.0f}, nullptr));
    host.composer.render(box().padding(10).child(
        text(u8"I", style).key("k").fx({.effect = fixed("both", mod)})));
    host.frame();
  };
  Host flat(140, 140);
  render(flat, false);
  Host filtered(140, 140);
  render(filtered, true);
  int worst = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 140; ++x) {
      const SkColor a = flat.pixel(x, y);
      const SkColor b = filtered.pixel(x, y);
      worst =
          std::max({worst, std::abs((int)SkColorGetR(a) - (int)SkColorGetR(b)),
                    std::abs((int)SkColorGetG(a) - (int)SkColorGetG(b)),
                    std::abs((int)SkColorGetB(a) - (int)SkColorGetB(b))});
    }
  EXPECT_LE(worst, 2)
      << "the matrix-filter path and the flat-colour path disagree about "
         "the colour-term arithmetic";
}

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
    Rng rng(90210);  // one glyph's stream, replayed
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

TEST(ComposeTextFx, OnlyAnEqualAdvanceSubstitutionIsDrawn) {
  // A substitution draws at the ORIGINAL glyph's pen position, so it is
  // sound exactly when the advances agree. Refusing is not cosmetic: a
  // wider replacement would overlap the letter after it while every letter
  // after that stayed where shaping put it.
  sk_sp<SkTypeface> face = fonts().defaultTypeface();
  if (!face) GTEST_SKIP() << "no default face on this system";
  SkFont probe(face, 100.0f);
  probe.setLinearMetrics(true);
  probe.setHinting(SkFontHinting::kNone);
  const SkGlyphID base = probe.unicharToGlyph('X');
  if (!base) GTEST_SKIP() << "the default face cannot draw X";
  const float baseWidth = probe.getWidth(base);
  char32_t twin = 0, proportional = 0;
  for (char32_t point = U'!'; point <= U'~'; ++point) {
    const SkGlyphID glyph = probe.unicharToGlyph((SkUnichar)point);
    if (!glyph || glyph == base) continue;
    const float width = probe.getWidth(glyph);
    if (!twin && std::abs(width - baseWidth) <= 0.05f) twin = point;
    if (!proportional && std::abs(width - baseWidth) > 8.0f)
      proportional = point;
  }

  const auto render = [&](Host& host, std::string key, char32_t point) {
    GlyphMod mod;
    mod.codepoint = point;
    host.composer.render(box().padding(10).child(
        text(u8"XXX", whiteStyle(44))
            .key("k")
            .fx({.effect = fixed(std::move(key), mod)})));
    host.frame();
  };
  Host rest(200, 120);
  render(rest, "none", 0);

  if (proportional) {
    Host refused(200, 120);
    render(refused, "wide", proportional);
    EXPECT_TRUE(identicalPixels(rest, refused, 200, 120))
        << "a proportional substitution was drawn instead of refused";
  }
  if (twin) {
    Host swapped(200, 120);
    render(swapped, "twin", twin);
    EXPECT_FALSE(identicalPixels(rest, swapped, 200, 120))
        << "an equal-advance substitution was refused, so the gate refuses "
           "everything and the case above proves nothing";
  }
  if (!twin && !proportional)
    GTEST_SKIP() << "the default face offered neither an equal-advance nor a "
                    "proportional partner for X";
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
      t.fx({.where = sel::line(1), .effect = fixed("lean", lean)});
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

namespace {

/** The startMs of the beat covering unit @p unitIndex, or -1 where the
 *  track runs no beat there. */
float startOfUnit(const std::vector<Beat>& beats, uint32_t unitIndex) {
  for (const Beat& beat : beats)
    if (beat.unitIndex == unitIndex) return beat.startMs;
  return -1.0f;
}

/** WHERE THE LAYOUT ACTUALLY PUT EACH WORD, straight off the positioned
 *  runs — the ground truth a beat's rect is checked against, so the check
 *  cannot pass by both sides re-measuring the text the same wrong way. */
std::vector<SkRect> wordExtents(const sigil::weave::ParagraphLayout& layout,
                                size_t words, SkPoint origin) {
  std::vector<SkRect> out(words, SkRect::MakeEmpty());
  for (const sigil::weave::PositionedRun& run : layout.runs) {
    if (!run.shaped || run.wordIndex >= words) continue;
    SkRect extent = SkRect::MakeXYWH(run.origin.x() + origin.x(),
                                     run.origin.y() + origin.y() - 0.5f,
                                     run.shaped->advance, 1.0f);
    if (out[run.wordIndex].isEmpty())
      out[run.wordIndex] = extent;
    else
      out[run.wordIndex].join(extent);
  }
  return out;
}

/** A baseline that leaves the node's box entirely: a ring centred well to
 *  the right of it, as a comparable scheme so the node still prunes. */
struct BeatRing {
  SkPath path(SkSize) const {
    SkPathBuilder builder;
    builder.addCircle(200, 100, 60);
    return builder.detach();
  }
  bool operator==(const BeatRing&) const = default;
};

}  // namespace

TEST(ComposeTextFx, PartitioningTracksShareOneClockOnlyUnderBeatsText) {
  // THE FLAGSHIP DEFECT. Two tracks split one paragraph — a track on the
  // words' initials and a track on their bodies — and the initials track
  // additionally spares the first word. Under the default numbering each
  // track counts only the units ITS OWN selector resolved, so the two lists
  // are three long and four long and every word after the first has its
  // initial on one beat and its body on another. Under beats::Text both
  // count the paragraph's words, so word three is beat three in both.
  const auto beatsUnder = [](Beats numbering) {
    Host host(400, 120);
    const Stagger spec{.eachMs = 100,
                       .durationMs = 100,
                       .over = unit::Word,
                       .beatsOver = numbering};
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC DD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.where = sel::each(unit::Word).take(1) & sel::words(1, 4),
                 .effect = fx::rise(6),
                 .stagger = spec})
            .fx({.where = sel::each(unit::Word).drop(1),
                 .effect = fx::rise(6),
                 .stagger = spec})));
    host.frame();
    return std::pair(host.composer.beatsOf("p", 0),
                     host.composer.beatsOf("p", 1));
  };

  const auto [initialsText, bodiesText] = beatsUnder(beats::Text);
  ASSERT_EQ(initialsText.size(), 3u) << "three words carry a spared initial";
  ASSERT_EQ(bodiesText.size(), 4u) << "four words carry a body";
  // The paragraph numbers the beats, so the initial of word k and the body
  // of word k open together — for every word the two tracks share.
  for (uint32_t word = 1; word <= 3; ++word)
    EXPECT_FLOAT_EQ(startOfUnit(initialsText, word),
                    startOfUnit(bodiesText, word))
        << "word " << word << " arrives in two pieces under beats::Text";
  EXPECT_FLOAT_EQ(startOfUnit(bodiesText, 3), 300.0f);

  // …and the default really is the other thing, or the setting is inert.
  const auto [initialsSel, bodiesSel] = beatsUnder(beats::Selection);
  ASSERT_EQ(initialsSel.size(), 3u);
  ASSERT_EQ(bodiesSel.size(), 4u);
  // The initials track renumbered its three words from zero: its LAST beat
  // is word three's and opens at 200 ms, while word three's body opens at
  // 300 ms. That hundred milliseconds is the defect, stated.
  EXPECT_FLOAT_EQ(initialsSel.back().startMs, 200.0f);
  EXPECT_FLOAT_EQ(bodiesSel.back().startMs, 300.0f);
  EXPECT_NE(initialsSel.back().startMs, bodiesSel.back().startMs)
      << "the two numberings produced the same schedule, so beats::Text is "
         "not doing anything";
}

TEST(ComposeTextFx, ACueTableStartsUnitKAtItsOwnTime) {
  // Real caption timing is a table cut against a recording, not a spacing.
  // Unit k starts at table[k], exactly, and nothing about `eachMs` survives
  // beside it.
  const std::vector<float> table{0.0f, 340.0f, 720.0f, 1180.0f};
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger =
                   stagger(unit::Word,
                           cues(table, {.eachMs = 999, .durationMs = 180}))})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), table.size());
  for (size_t k = 0; k < table.size(); ++k) {
    EXPECT_EQ(beats[k].unitIndex, (uint32_t)k);
    EXPECT_FLOAT_EQ(beats[k].startMs, table[k])
        << "unit " << k << " did not start at its own cue";
  }
  // cues() is a Stagger, so it compares like one — and a different table is
  // a different cascade, or a re-described track would prune onto the old
  // schedule and keep singing the previous line's timing.
  EXPECT_TRUE(cues(table) == cues(table));
  EXPECT_FALSE(cues(table) == cues({0.0f, 340.0f, 720.0f}));
  EXPECT_FALSE(cues(table) == Stagger{});
}

TEST(ComposeTextFx, AShortCueTablePilesItsTailAndWarnsOnce) {
  // A table that does not have one time per unit is a table cut against the
  // wrong text. The tail holds on the last cue — visible as a pile, rather
  // than times its author never wrote — and it says so once.
  ::testing::internal::CaptureStderr();
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger = stagger(
                   unit::Word, cues({0.0f, 200.0f}, {.durationMs = 100}))})));
  host.frame();
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("cue table"), std::string::npos) << log;
  EXPECT_NE(log.find("last time"), std::string::npos) << log;

  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u);
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[1].startMs, 200.0f);
  EXPECT_FLOAT_EQ(beats[2].startMs, 200.0f) << "the tail must hold, not run on";
  EXPECT_FLOAT_EQ(beats[3].startMs, 200.0f);

  // Once per shape: the same mismatch again is silent.
  ::testing::internal::CaptureStderr();
  host.frame();
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "the cue-table warning is not once per shape";
}

TEST(ComposeTextFx, BeatsOfReportsWhereTheGlyphsActuallyWentAndWhen) {
  // The read-back has to agree with the placement, not with a second
  // measurement: cross-check every beat's rect against the glyphs the
  // layout placed, over a paragraph that WRAPS and carries two sizes, where
  // a re-measured line would land in the wrong place twice over.
  Host host(240, 240);
  choreograph::Output<float> progress{0.5f};
  RichText copy = rich(whiteStyle(15));
  copy.add(u8"alpha bravo ").add(u8"charlie", whiteStyle(24)).add(u8" delta");
  host.composer.render(
      box().padding(10).child(text(copy).key("p").width(120).fx(
          {.effect = fx::rise(8),
           .stagger = stagger(unit::Word, {.eachMs = 100, .durationMs = 200}),
           .progress = &progress})));
  host.frame();

  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u) << "one beat per word of the paragraph";
  const sigil::weave::ParagraphLayout* layout =
      host.composer.paragraphLayout("p");
  ASSERT_NE(layout, nullptr);
  const std::optional<SkRect> box_ = host.composer.bounds("p");
  ASSERT_TRUE(box_.has_value());

  const std::vector<SkRect> placed =
      wordExtents(*layout, beats.size(), {box_->left(), box_->top()});
  bool wrapped = false;
  for (size_t i = 1; i < beats.size(); ++i)
    if (beats[i].rect.centerY() > beats[i - 1].rect.centerY() + 4)
      wrapped = true;
  EXPECT_TRUE(wrapped) << "the paragraph never wrapped: nothing is proven";
  for (size_t i = 0; i < beats.size(); ++i) {
    ASSERT_FALSE(placed[i].isEmpty()) << "word " << i << " was never placed";
    EXPECT_NEAR(beats[i].rect.left(), placed[i].left(), 1.0f) << "word " << i;
    EXPECT_GE(beats[i].rect.right(), placed[i].right() - 2.0f)
        << "word " << i << "'s beat stops short of its own last letter";
    EXPECT_LE(beats[i].rect.top(), placed[i].centerY()) << "word " << i;
    EXPECT_GE(beats[i].rect.bottom(), placed[i].centerY()) << "word " << i;
  }
  EXPECT_GT(beats[2].rect.height(), beats[0].rect.height())
      << "the 24 px run's beat is no taller than the 15 px runs', so the "
         "rect is not reading the placement";

  // …and the LOCAL TIME is the number the glyphs are being handed. The
  // cascade spans 200 + 100·3 = 500 ms of virtual time, so at master 0.5
  // the front stands at 250 ms: word 0 is done, word 1 is three quarters
  // through its own 200 ms beat, word 2 has just started and word 3 has not
  // opened at all.
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[3].startMs, 300.0f);
  EXPECT_FLOAT_EQ(beats[0].localT, 1.0f);
  EXPECT_FLOAT_EQ(beats[1].localT, 0.75f);
  EXPECT_FLOAT_EQ(beats[2].localT, 0.25f);
  EXPECT_FLOAT_EQ(beats[3].localT, 0.0f);
  EXPECT_FALSE(beats[0].active) << "a finished beat is not running";
  EXPECT_TRUE(beats[1].active);
  EXPECT_TRUE(beats[2].active);
  EXPECT_FALSE(beats[3].active) << "a beat that has not opened is not running";
}

TEST(ComposeTextFx, BeatsOfFollowsAPathBaseline) {
  // A path run's letters are not on the node's straight baseline at all,
  // and a mark placed from anything but the placement would sit in the
  // node's box while the type rides a circle beside it.
  Host host(300, 200);
  host.composer.render(box().child(
      text(u8"CIRCVMFERENTIA", whiteStyle(18))
          .key("ring")
          .width(100)
          .height(100)
          .onPath({.path = BeatRing{}})
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Cluster)})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("ring", 0);
  ASSERT_GT(beats.size(), 8u);
  // The ring is centred at (200, 100) with radius 60, and the node's box is
  // the 100x100 at the origin: every beat must be OUT there, on the curve.
  SkRect union_ = SkRect::MakeEmpty();
  for (const Beat& beat : beats) {
    const float radius =
        std::hypot(beat.rect.centerX() - 200.0f, beat.rect.centerY() - 100.0f);
    EXPECT_NEAR(radius, 60.0f, 14.0f)
        << "a beat left the baseline it was supposed to be reading";
    union_.join(beat.rect);
  }
  EXPECT_GT(union_.right(), 200.0f)
      << "every beat stayed inside the node's box, so the rects are the "
         "straight baseline's rather than the curve's";
  // …and they go ROUND the ring rather than piling at its entry point: the
  // run sweeps a real arc, which only a rect read off the curve can show.
  const auto angleOf = [](const Beat& beat) {
    return std::atan2(beat.rect.centerY() - 100.0f,
                      beat.rect.centerX() - 200.0f);
  };
  float swept = 0;
  for (size_t i = 1; i < beats.size(); ++i) {
    float step = angleOf(beats[i]) - angleOf(beats[i - 1]);
    while (step > 3.14159265f) step -= 6.2831853f;
    while (step < -3.14159265f) step += 6.2831853f;
    swept += std::abs(step);
  }
  EXPECT_GT(swept, 1.0f) << "the beats never went round the ring";
}

TEST(ComposeTextFx, BeatsOfCompoundsANestedCascade) {
  // The case the karaoke study had to give up: a nested beat lasts exactly
  // as long as its inner ladder needs, so no author can restate the start
  // times. Word w's letter i opens at w·outerEach + i·innerEach, and the
  // read-back is the only place that is true without being retyped.
  Host host(400, 120);
  Stagger cascade = stagger(unit::Word, {.eachMs = 300});
  cascade.then(unit::Cluster, {.eachMs = 40, .durationMs = 100});
  host.composer.render(box().padding(6).child(
      text(u8"AB CD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6), .stagger = cascade})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), 4u) << "one beat per letter, two letters per word";
  // Two beats per outer unit, each carrying its own compounded start.
  EXPECT_EQ(beats[0].unitIndex, 0u);
  EXPECT_EQ(beats[1].unitIndex, 0u);
  EXPECT_EQ(beats[2].unitIndex, 1u);
  EXPECT_EQ(beats[3].unitIndex, 1u);
  EXPECT_FLOAT_EQ(beats[0].startMs, 0.0f);
  EXPECT_FLOAT_EQ(beats[1].startMs, 40.0f);
  EXPECT_FLOAT_EQ(beats[2].startMs, 300.0f);
  EXPECT_FLOAT_EQ(beats[3].startMs, 340.0f);
}

TEST(ComposeTextFx, BeatsOfResolvesEmptyRatherThanGuessing) {
  Host host(200, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB", whiteStyle(16))
          .key("p")
          .fx({.effect = fx::rise(6), .stagger = stagger(unit::Word)})));
  host.frame();
  EXPECT_FALSE(host.composer.beatsOf("p", 0).empty());
  EXPECT_TRUE(host.composer.beatsOf("typo", 0).empty()) << "unknown key";
  EXPECT_TRUE(host.composer.beatsOf("p", 7).empty()) << "no such track";
}

TEST(ComposeTextFx, CascadeSpanMsIsWhatTheMasterProgressMapsOnto) {
  // The one number beatsOf does not carry: a beat says when it OPENS, and
  // nothing else says when the whole schedule is OVER. The span is the
  // last beat's start plus one beat's own duration — checked against the
  // beats themselves, so the two read-backs cannot drift apart — and the
  // declare-time form answers the same number from the counts alone.
  const Stagger spec = stagger(unit::Word, {.eachMs = 100, .durationMs = 200});
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6), .stagger = spec})));
  host.frame();
  const float span = host.composer.cascadeSpanMs("p", 0);
  EXPECT_FLOAT_EQ(span, 500.0f) << "durationMs + eachMs·(N−1) over 4 words";
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_FALSE(beats.empty());
  float latest = 0;
  for (const Beat& beat : beats) latest = std::max(latest, beat.startMs);
  EXPECT_FLOAT_EQ(span, latest + 200.0f)
      << "the span disagrees with the beats about when the last one closes";
  EXPECT_FLOAT_EQ(spec.spanMs(4), span)
      << "the declare-time form and the mounted one answer differently for "
         "the same cascade and count";

  // Amount-mode is count-independent past one unit — the amount IS the
  // spread — and one unit (or none) is a bare beat.
  const Stagger amount{.amountMs = 700, .durationMs = 420};
  EXPECT_FLOAT_EQ(amount.spanMs(2), 1120.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(9), 1120.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(1), 420.0f);
  EXPECT_FLOAT_EQ(amount.spanMs(0), 420.0f);
}

TEST(ComposeTextFx, CascadeSpanMsCompoundsNestingAndReadsTheTable) {
  // Nested: a beat lasts exactly as long as its inner ladder needs, so the
  // span compounds — the latest outer start, plus the latest inner start,
  // plus one INNER duration (the outer durationMs is ignored, as always
  // under then()).
  Stagger nested = stagger(unit::Word, {.eachMs = 300, .durationMs = 999});
  nested.then(unit::Cluster, {.eachMs = 40, .durationMs = 100});
  {
    Host host(400, 120);
    host.composer.render(box().padding(6).child(
        text(u8"AB CD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6), .stagger = nested})));
    host.frame();
    const float span = host.composer.cascadeSpanMs("p", 0);
    EXPECT_FLOAT_EQ(span, 440.0f) << "300·1 + 40·1 + 100";
    const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
    ASSERT_EQ(beats.size(), 4u);
    EXPECT_FLOAT_EQ(span, beats.back().startMs + 100.0f);
    EXPECT_FLOAT_EQ(nested.spanMs(2, 2), span)
        << "the declare-time form must take the inner count per beat";
  }

  // A cue table's extent is the table's own: the LATEST time any unit
  // reads plus the duration — a max, not the final entry, because a table
  // is not required to ascend.
  const std::vector<float> table{0.0f, 340.0f, 720.0f, 1180.0f};
  const Stagger cued = stagger(unit::Word, cues(table, {.durationMs = 180}));
  {
    Host host(400, 120);
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC DD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6), .stagger = cued})));
    host.frame();
    const float span = host.composer.cascadeSpanMs("p", 0);
    EXPECT_FLOAT_EQ(span, 1360.0f) << "the table's last time plus one beat";
    EXPECT_FLOAT_EQ(cued.spanMs(4), span);
  }
  EXPECT_FLOAT_EQ(
      stagger(unit::Word, cues({0.0f, 900.0f, 300.0f}, {.durationMs = 100}))
          .spanMs(3),
      1000.0f)
      << "an out-of-order table spans to its LATEST time, not its last entry";
}

TEST(ComposeTextFx, CascadeSpanMsResolvesZeroRatherThanGuessing) {
  Host host(200, 120);
  host.composer.render(box().padding(6).key("b").child(
      text(u8"AA BB", whiteStyle(16))
          .key("p")
          .fx({.effect = fx::rise(6), .stagger = stagger(unit::Word)})));
  host.frame();
  EXPECT_GT(host.composer.cascadeSpanMs("p", 0), 0.0f);
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("typo", 0), 0.0f)
      << "unknown key";
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("p", 7), 0.0f) << "no such track";
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("b", 0), 0.0f)
      << "a node that is not text";
}

namespace {

/** Three words on a looping word cascade at one master value, with the
 *  schedule read back. eachMs 100, durationMs 200, loopMs 400 — starts 0,
 *  100, 200, each beat re-opening every 400 virtual ms. */
std::vector<Beat> loopBeatsAt(Host& host, float master, float loopMs = 400) {
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger = stagger(
                   unit::Word,
                   {.eachMs = 100, .durationMs = 200, .loopMs = loopMs}),
               .progress = master})));
  host.frame();
  return host.composer.beatsOf("p", 0);
}

}  // namespace

TEST(ComposeTextFx, ALoopingCascadeReopensEachUnitOnItsOwnCycle) {
  Host host(400, 120);
  // Master 0.25 of a 400ms period is virtual 100: the first word is
  // halfway through its 200ms beat, the second opens this instant, and the
  // third — 100ms short of its start in one-shot terms — is MID-CYCLE at
  // elapsed 300, resting at 1. The fold leaves no unit waiting.
  {
    const std::vector<Beat> beats = loopBeatsAt(host, 0.25f);
    ASSERT_EQ(beats.size(), 3u);
    EXPECT_FLOAT_EQ(beats[0].localT, 0.5f);
    EXPECT_FLOAT_EQ(beats[1].localT, 0.0f);
    EXPECT_FLOAT_EQ(beats[2].localT, 1.0f)
        << "a unit short of its start must rest at 1 mid-cycle, not wait "
           "at 0";
    EXPECT_TRUE(beats[0].active);
    EXPECT_FALSE(beats[2].active) << "resting between beats is not active";
  }
  // Three quarters on: the ladder has rolled through — the tail is now the
  // one mid-beat.
  {
    const std::vector<Beat> beats = loopBeatsAt(host, 0.75f);
    ASSERT_EQ(beats.size(), 3u);
    EXPECT_FLOAT_EQ(beats[0].localT, 1.0f);
    EXPECT_FLOAT_EQ(beats[1].localT, 1.0f);
    EXPECT_FLOAT_EQ(beats[2].localT, 0.5f);
  }
  // THE SEAM: master 0 and master 1 are the same instant of the cycle, so
  // a wrapping bound phase crosses its own wrap with no jump — every beat
  // answers identically at both ends.
  {
    const std::vector<Beat> low = loopBeatsAt(host, 0.0f);
    const std::vector<Beat> high = loopBeatsAt(host, 1.0f);
    ASSERT_EQ(low.size(), high.size());
    for (size_t i = 0; i < low.size(); ++i)
      EXPECT_FLOAT_EQ(low[i].localT, high[i].localT)
          << "beat " << i << " jumps across the master's wrap";
  }
  // The period is what the master maps onto, so the span queries answer it
  // — mounted and at declare time alike.
  EXPECT_FLOAT_EQ(host.composer.cascadeSpanMs("p", 0), 400.0f);
  const Stagger looping =
      stagger(unit::Word, {.eachMs = 100, .durationMs = 200, .loopMs = 400});
  EXPECT_FLOAT_EQ(looping.spanMs(3), 400.0f);

  // A start PAST the period folds mod it — the beat still re-opens once
  // per cycle — while Beat::startMs keeps reporting the authored delay.
  {
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6),
                 .stagger =
                     stagger(unit::Word,
                             {.eachMs = 300, .durationMs = 200, .loopMs = 400}),
                 .progress = 0.75f})));  // virtual 300
    host.frame();
    const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
    ASSERT_EQ(beats.size(), 3u);
    EXPECT_FLOAT_EQ(beats[2].startMs, 600.0f)
        << "the schedule stays authored; only the clock folds";
    // Elapsed mod 400 of (300 − 600) is 100: halfway through a 200ms beat.
    EXPECT_FLOAT_EQ(beats[2].localT, 0.5f);
  }
}

TEST(ComposeTextFx, LoopMsZeroIsTheOneShotCascade) {
  // 0 — the default — is the one-shot path: a spelled-out zero is the same
  // value, the same schedule and the same bytes as never mentioning it,
  // and a unit short of its start WAITS at 0 rather than resting at 1.
  EXPECT_TRUE((Stagger{.eachMs = 100, .durationMs = 200}) ==
              (Stagger{.eachMs = 100, .durationMs = 200, .loopMs = 0}));
  EXPECT_FALSE((Stagger{.eachMs = 100, .durationMs = 200}) ==
               (Stagger{.eachMs = 100, .durationMs = 200, .loopMs = 400}));

  const auto render = [](Stagger cascade) {
    Host host(400, 120);
    host.composer.render(
        box().padding(6).child(text(u8"AA BB CC", whiteStyle(16))
                                   .key("p")
                                   .width(360)
                                   .fx({.effect = fx::rise(6),
                                        .stagger = std::move(cascade),
                                        .progress = 0.25f})));
    host.frame();
    return std::pair(host.composer.beatsOf("p", 0),
                     surfaceBytes(host, 400, 120));
  };
  const auto [defaulted, defaultedBytes] =
      render(stagger(unit::Word, {.eachMs = 100, .durationMs = 200}));
  const auto [spelled, spelledBytes] = render(
      stagger(unit::Word, {.eachMs = 100, .durationMs = 200, .loopMs = 0}));
  EXPECT_EQ(defaultedBytes, spelledBytes);
  ASSERT_EQ(defaulted.size(), 3u);
  ASSERT_EQ(spelled.size(), 3u);
  // One-shot semantics, for contrast with the fold: span 400 covers the
  // ladder, so master 0.25 is virtual 100 here too — but the un-started
  // tail reads 0, where the loop above rests it at 1.
  EXPECT_FLOAT_EQ(defaulted[2].localT, 0.0f);
  for (size_t i = 0; i < defaulted.size(); ++i) {
    EXPECT_FLOAT_EQ(defaulted[i].localT, spelled[i].localT);
    EXPECT_FLOAT_EQ(defaulted[i].startMs, spelled[i].startMs);
  }
}

TEST(ComposeTextFx, AHeldEffectOnALoopingCascadeHasNothingLeftToVeto) {
  // fx::hold blanks a unit whose beat has not opened — and a looping
  // cascade HAS no such unit: the fold keeps every beat somewhere in its
  // cycle, so the word a one-shot hold still withholds paints under a
  // loop, at the rest its cycle has reached. The veto survives only as
  // the single instant of re-opening (local 0), which is exactly where
  // the existing hold law already blanks.
  const auto tree = [](float loopMs) {
    return box().padding(10).child(
        text(u8"AA BB", whiteStyle(28))
            .key("p")
            .fx({.effect = fx::hold(fx::rise(24)),
                 .stagger = stagger(
                     unit::Word,
                     {.eachMs = 300, .durationMs = 100, .loopMs = loopMs}),
                 .progress = 0.25f}));
  };
  const auto rightHalfInk = [](Host& host) {
    auto b = host.composer.bounds("p");
    EXPECT_TRUE(b.has_value());
    // The right word's whole travel: its rest box plus the rise's reach
    // below it, clamped to the host.
    return anyWhiteIn(host,
                      SkIRect::MakeLTRB((int)(b->centerX() + 4), (int)b->top(),
                                        std::min((int)b->right(), 239),
                                        std::min((int)b->bottom() + 24, 119)));
  };
  Host oneShot(240, 120);
  oneShot.composer.render(tree(0));
  oneShot.frame();
  EXPECT_FALSE(rightHalfInk(oneShot))
      << "one-shot control: the held word's beat (start 300, virtual 100) "
         "has not opened, so it must paint nothing";
  Host looping(240, 120);
  looping.composer.render(tree(400));
  looping.frame();
  EXPECT_TRUE(rightHalfInk(looping))
      << "under the loop the same word is mid-cycle (elapsed 200 of 400) "
         "and must paint at its rest";
}

TEST(ComposeTextFx, ALoopingCascadeOnAWrappingPhaseNeverSettles) {
  // The loop's permanent volatility is DECLARED by its driver, the way
  // waveLoop's already is: a wrapping bound phase is live on every frame,
  // so the element paints live forever — across the wrap included — while
  // the same reveal as a one-shot transition settles and goes back to a
  // cached picture.
  Host live(240, 120);
  choreograph::Output<float> phase{0.0f};
  live.composer.render(box().padding(10).child(
      text(u8"LOOP", whiteStyle(28))
          .key("p")
          .fx({.effect = fx::rise(24),
               .stagger =
                   stagger(unit::Cluster,
                           {.eachMs = 100, .durationMs = 200, .loopMs = 400}),
               .progress = &phase})));
  live.frame();
  double clock = 0.0;
  for (int i = 0; i < 24; ++i) {  // three settle windows, several wraps
    clock += 0.07;
    phase = (float)std::fmod(clock, 1.0);
    live.frame(0.016);
    EXPECT_GE(live.composer.stats().nodesPainted, 1u)
        << "frame " << i << ": a driven looping cascade stopped painting "
        << "live";
  }

  Host still(240, 120);
  still.composer.render(box().padding(10).child(
      text(u8"LOOP", whiteStyle(28))
          .key("p")
          .fx({.effect = fx::rise(24),
               .stagger =
                   stagger(unit::Cluster, {.eachMs = 100, .durationMs = 200}),
               .progress = animate(from(0.0f).to(1.0f),
                                   {200ms, &choreograph::easeNone})})));
  for (int i = 0; i < 24; ++i) still.frame(0.016);
  unsigned settledPaints = 0;
  for (int i = 0; i < 4; ++i) {
    still.frame(0.016);
    settledPaints += still.composer.stats().nodesPainted;
  }
  EXPECT_EQ(settledPaints, 0u)
      << "the one-shot control kept painting live after its entrance "
         "settled";
}

TEST(ComposeLayouts, BaselineGridRendersInsideStackedAbsoluteColumn) {
  // Text inside a BaselineGrid, nested in an absolute column, inside a
  // stack(). A custom layout scheme writes back into Yoga out of band, and
  // an absolute ancestor changes how its subtree is sized, so this is the
  // combination most likely to leave the text laid out at zero size and
  // therefore invisible.
  Host host;
  host.composer.render(stack().child(
      box()
          .column()
          .absolute()
          .inset(10, 10, 10, 10)
          .child(layout(layouts::BaselineGrid{.rhythm = 24})
                     .width(pct(100))
                     .child(text(u8"probe", whiteStyle(28)).key("p")))));
  host.frame();
  auto b = host.composer.bounds("p");
  ASSERT_TRUE(b.has_value());
  EXPECT_GT(b->width(), 5.0f) << "placed rect " << b->left() << "," << b->top()
                              << " " << b->width() << "x" << b->height();
  EXPECT_GT(b->height(), 5.0f);
  EXPECT_TRUE(
      anyWhiteIn(host, SkIRect::MakeLTRB((int)b->left(), (int)b->top(),
                                         (int)b->right(), (int)b->bottom())))
      << "placed rect " << b->left() << "," << b->top() << " " << b->width()
      << "x" << b->height();
}

namespace {

/** How many pixels of exactly this colour a region holds. */
int countColor(Host& host, SkIRect region, SkColor color) {
  int hits = 0;
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == color) ++hits;
  return hits;
}

/** The shaped-word identity behind every placed run. The shape cache is
 *  content-addressed, so an unchanged pointer is proof that nothing about
 *  that word's shaping inputs moved. */
std::vector<const void*> runShapes(Host& host, const char* key) {
  std::vector<const void*> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.shaped.get());
  return out;
}

std::vector<SkPoint> runOrigins(Host& host, const char* key) {
  std::vector<SkPoint> out;
  const auto* layout = host.composer.paragraphLayout(key);
  if (!layout) return out;
  for (const sigil::weave::PositionedRun& run : layout->runs)
    out.push_back(run.origin);
  return out;
}

sigil::weave::TextStyle coloredStyle(float size, SkColor color) {
  sigil::weave::TextStyle s = styleAt(size);
  s.paint.foreground.setColor(color);
  return s;
}

/** @p base with one variable-font axis set — the restyle that differs from
 *  the text it covers in that axis alone. */
sigil::weave::TextStyle withAxis(sigil::weave::TextStyle base,
                                 const char (&tag)[5], float value) {
  base.variation(tag, value);
  return base;
}

}  // namespace

TEST(TextRich, MixedRunsPaintTheirOwnStyles) {
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(36, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(36, SK_ColorRED);
  host.composer.render(box().padding(10).child(
      text(rich(base).add(u8"AAA ").add(u8"BBB", accent)).key("t")));
  host.frame();
  const SkIRect band = SkIRect::MakeXYWH(0, 0, 400, 80);
  EXPECT_GT(countColor(host, band, SK_ColorWHITE), 20) << "the base run";
  EXPECT_GT(countColor(host, band, SK_ColorRED), 20) << "the accented run";
}

TEST(TextRich, AnIdenticalValuePrunesWhereAFreshPointerCannot) {
  // The whole reason rich() is a value: a component that rebuilds its spans
  // every describe must prune like a static leaf. The shared_ptr overload
  // cannot answer the question — a fresh make_shared is a fresh identity —
  // which is exactly the difference this pins.
  Host host(400, 120);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const sigil::weave::TextStyle accent = coloredStyle(20, SK_ColorRED);
  auto describe = [&](std::u8string_view tail) {
    return box().child(
        text(rich(base).add(u8"Signal ").add(tail, accent)).key("t"));
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
    para->appendText(std::u8string(u8"Signal woven"), base);
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
    return box().child(text(rich(coloredStyle(20, SK_ColorWHITE))
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

TEST(TextRich, NamedRunsResolveThroughAStyleSet) {
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  sigil::weave::StyleSet reds;
  reds.set("accent", coloredStyle(20, SK_ColorRED));
  sigil::weave::StyleSet greens;
  greens.set("accent", coloredStyle(20, SK_ColorGREEN));

  const auto colorOf = [](const RichText& value, size_t run) {
    return value.runs()[run].style.paint.foreground.getColor();
  };

  const RichText supplied = rich(base).add(u8"x", "accent").styles(reds);
  EXPECT_EQ(colorOf(supplied, 0), SK_ColorRED);

  // styles() before the named run resolves identically: the order the two
  // are written in must not matter.
  const RichText suppliedFirst = rich(base).styles(reds).add(u8"x", "accent");
  EXPECT_EQ(colorOf(suppliedFirst, 0), SK_ColorRED);
  EXPECT_TRUE(supplied == suppliedFirst);

  {
    env::Provide<sigil::weave::StyleSet> ambient(reds);
    const RichText inherited = rich(base).add(u8"x", "accent");
    EXPECT_EQ(colorOf(inherited, 0), SK_ColorRED) << "the env set was ignored";
    const RichText overridden = rich(base).add(u8"x", "accent").styles(greens);
    EXPECT_EQ(colorOf(overridden, 0), SK_ColorGREEN)
        << "an explicit style set must beat the inherited one";
  }
  // Out of scope again: nothing is inherited, so the base answers.
  const RichText unbound = rich(base).add(u8"x", "accent");
  EXPECT_EQ(colorOf(unbound, 0), SK_ColorWHITE);
  // A name the set does not register resolves to rich()'s own base.
  const RichText unknown = rich(base).add(u8"x", "nope").styles(reds);
  EXPECT_EQ(colorOf(unknown, 0), SK_ColorWHITE);
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
          .spanPaint(sel::regex(u8"[0-9]+"),
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
  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanStyle(sel::text(u8"gamma"), coloredStyle(40, SK_ColorRED))
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
          .spanPaint(sel::text(u8"beta"), sigil::weave::PaintStyle(SK_ColorRED))
          .spanPaint(sel::words(0, 2), sigil::weave::PaintStyle(SK_ColorGREEN))
          .key("t")));
  host.frame();
  EXPECT_EQ(countColor(host, band, SK_ColorRED), 0)
      << "the earlier narrow rule survived a later broad one";
  EXPECT_GT(countColor(host, band, SK_ColorGREEN), 20);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanPaint(sel::words(0, 2), sigil::weave::PaintStyle(SK_ColorGREEN))
          .spanPaint(sel::text(u8"beta"), sigil::weave::PaintStyle(SK_ColorRED))
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
          .spanPaint(sel::line(0), sigil::weave::PaintStyle(SK_ColorRED))
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
RichText glossaryCopy(const sigil::weave::StyleSet& set) {
  RichText copy = rich(set.base());
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
  const RichText copy = glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at WORD granularity number the units the track's own selection
  // resolved, so the beat list IS the addressed word list — and each beat's
  // rect says which word it is.
  const auto wordsAddressed = [&](Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").fx(
        {.where = std::move(where),
         .effect = fx::rise(0),
         .stagger = stagger(unit::Word, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0);
  };

  const std::vector<Beat> byName = wordsAddressed(sel::style("term"));
  const std::vector<Beat> byWords = wordsAddressed(sel::text(u8"beta"));
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
  const RichText copy = glossaryCopy(glossarySet(SK_ColorRED, 24));
  // Beats at GLYPH granularity: one per addressed glyph, so the count is the
  // selection's size and the algebra can be checked as arithmetic.
  const auto glyphsAddressed = [&](Selector where) {
    host.composer.render(box().padding(10).child(text(copy).key("t").fx(
        {.where = std::move(where),
         .effect = fx::rise(0),
         .stagger = stagger(unit::Glyph, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };

  const size_t whole = glyphsAddressed(Selector{});
  const size_t named = glyphsAddressed(sel::style("term"));
  ASSERT_EQ(named, 8u) << "two four-letter runs";
  EXPECT_EQ(glyphsAddressed(sel::text(u8"beta")), 12u) << "three of them";

  // The three operators, against the same two runs.
  EXPECT_EQ(glyphsAddressed(sel::style("term") | sel::word(0)), named + 5u)
      << "alpha joined the union";
  EXPECT_EQ(glyphsAddressed(sel::style("term") & sel::word(1)), 4u)
      << "the intersection is the first named run alone";
  EXPECT_EQ(glyphsAddressed(!sel::style("term")), whole - named);
}

TEST(TextStyleSelector, PlainTextCarriesNoNamesAndSaysSoOnce) {
  // Only a named rich() run carries a name. Plain text has none, so the
  // selector addresses nothing — the silent-no-op rule, made audible.
  Host host(400, 120);
  ::testing::internal::CaptureStderr();
  const auto describe = [] {
    return box().padding(10).child(
        text(u8"alpha beta gamma", coloredStyle(24, SK_ColorWHITE))
            .key("t")
            .fx({.where = sel::style("unregistered-register"),
                 .effect = fx::rise(0),
                 .stagger = stagger(unit::Glyph, {.durationMs = 1})}));
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
               .stagger = stagger(unit::Glyph, {.durationMs = 1})})));
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
  const RichText copy = glossaryCopy(glossarySet(SK_ColorWHITE, 24));

  // Where the three betas actually sit, read off the layout rather than
  // guessed, so the assertions below can name one of them.
  host.composer.render(box().padding(10).child(text(copy).key("t").fx(
      {.where = sel::text(u8"beta"),
       .effect = fx::rise(0),
       .stagger = stagger(unit::Word, {.eachMs = 1, .durationMs = 1})})));
  host.frame();
  const std::vector<Beat> betas = host.composer.beatsOf("t", 0);
  ASSERT_EQ(betas.size(), 3u);
  const auto bandOf = [](const Beat& b) {
    return SkIRect::MakeLTRB((int)std::floor(b.rect.left()), 0,
                             (int)std::ceil(b.rect.right()), 140);
  };

  const auto redsIn = [&](Selector where, const Beat& beat) {
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
  EXPECT_GT(redsIn(sel::text(u8"beta"), betas[1]), 5)
      << "…which the literal selector does catch, as it must";

  // And through spanStyle, which re-shapes: exactly the named runs do.
  host.composer.render(box().padding(10).child(text(copy).key("t")));
  host.frame();
  const std::vector<const void*> before = runShapes(host, "t");
  ASSERT_FALSE(before.empty());
  const auto reshapedUnder = [&](Selector where) {
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
  EXPECT_EQ(reshapedUnder(sel::text(u8"beta")), 3u);
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
                 .stagger =
                     stagger(unit::Glyph, {.eachMs = 1, .durationMs = 1})})));
    host.frame();
    return host.composer.beatsOf("t", 0).size();
  };
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorRED, 24)), 8u);
  EXPECT_EQ(namedGlyphs(glossarySet(SK_ColorGREEN, 36)), 8u)
      << "a re-registered name stopped resolving, so the selector is keyed "
         "on the style rather than on the run that wears it";
}

namespace {

/** A face carrying the advance-invariant GRAD axis, and that axis's own
 *  design range — null when this machine has none, or has one whose varied
 *  clone renders identically, because a test that cannot see the axis move
 *  would pass while checking nothing. */
sk_sp<SkTypeface> gradFace(float& lo, float& hi) {
  lo = hi = 0;
  sk_sp<SkFontMgr> manager = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> face;
  for (const char* family :
       {".AppleSystemUIFont", ".SF NS", "SF Pro Text", "SF Pro"}) {
    face = manager->matchFamilyStyle(family, SkFontStyle());
    if (face && fonts().axisIsAdvanceInvariant(face, "GRAD")) break;
    face = nullptr;
  }
  if (!face) return nullptr;
  const int count = face->getVariationDesignParameters({});
  if (count <= 0) return nullptr;
  std::vector<SkFontParameters::Variation::Axis> axes((size_t)count);
  face->getVariationDesignParameters({axes.data(), axes.size()});
  for (const auto& axis : axes)
    if (axis.tag == SkSetFourByteTag('G', 'R', 'A', 'D')) {
      lo = axis.min;
      hi = axis.max;
    }
  if (hi <= lo) return nullptr;

  // The advance probe proves advances HOLD; it cannot prove the clone
  // RESPONDS. Rasterize one glyph at both ends and insist it does.
  const sigil::weave::FontVariation vLo("GRAD", lo), vHi("GRAD", hi);
  const auto ink = [&](const sigil::weave::FontVariation& v) {
    SkFont font(fonts().variedTypeface(face, {&v, 1}), 48);
    const SkGlyphID glyph = font.unicharToGlyph('W');
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100, 80));
    surface->getCanvas()->clear(SK_ColorBLACK);
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    paint.setAntiAlias(true);
    const SkPoint at{10, 60};
    surface->getCanvas()->drawGlyphs(SkSpan(&glyph, 1), SkSpan(&at, 1), {0, 0},
                                     font, paint);
    SkBitmap bitmap;
    bitmap.allocPixels(surface->imageInfo());
    surface->readPixels(bitmap.pixmap(), 0, 0);
    return bitmap;
  };
  const SkBitmap light = ink(vLo), heavy = ink(vHi);
  for (int y = 0; y < 80; ++y)
    for (int x = 0; x < 100; ++x)
      if (light.getColor(x, y) != heavy.getColor(x, y)) return face;
  return nullptr;
}

/** The whole surface, for a byte comparison against another frame. */
SkBitmap grab(Host& host, int w, int h) {
  SkBitmap out;
  out.allocPixels(SkImageInfo::MakeN32Premul(w, h));
  host.surface->readPixels(out.pixmap(), 0, 0);
  return out;
}

int pixelsDiffering(const SkBitmap& a, const SkBitmap& b, int w, int h) {
  int changed = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) changed += a.getColor(x, y) != b.getColor(x, y);
  return changed;
}

}  // namespace

TEST(TextSpanAxis, AnInvariantAxisRedrawsWithoutReshaping) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(40, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"Count 1234 now";
  host.composer.render(box().padding(10).child(text(body, base).key("t")));
  host.frame();
  const std::vector<const void*> shapesBefore = runShapes(host, "t");
  const std::vector<SkPoint> originsBefore = runOrigins(host, "t");
  ASSERT_FALSE(shapesBefore.empty());
  const SkBitmap plain = grab(host, 400, 120);

  host.composer.render(box().padding(10).child(
      text(body, base)
          .spanStyle(sel::regex(u8"[0-9]+"), withAxis(base, "GRAD", hi))
          .key("t")));
  host.frame();
  EXPECT_EQ(runShapes(host, "t"), shapesBefore)
      << "an axis-only spanStyle re-shaped a word — the axis went into the "
         "shaping style instead of onto the glyphs";
  EXPECT_EQ(runOrigins(host, "t"), originsBefore)
      << "an axis-only spanStyle moved a glyph";
  EXPECT_GT(pixelsDiffering(plain, grab(host, 400, 120), 400, 120), 20)
      << "the graded numerals are drawing exactly as the ungraded ones did";
}

TEST(TextSpanAxis, AnAdvanceVariantAxisReshapesInstead) {
  // The instrument face whose wght genuinely interpolates advances, so the
  // fold has something to decline. Loaded here rather than shared, so this
  // face's verdict is this test's own to observe.
  const sk_sp<SkTypeface> face = fonts().fontManager()->makeFromFile(
      SIGILCOMPOSE_TEST_ASSET_DIR "/AdvanceVariant.ttf");
  ASSERT_TRUE(face) << "test asset AdvanceVariant.ttf failed to load";
  ASSERT_GT(face->getVariationDesignParameters({}), 0);
  ASSERT_FALSE(fonts().axisIsAdvanceInvariant(face, "wght"))
      << "the instrument face's wght must move advances";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(44, SK_ColorWHITE);
  base.shaping.typeface = face;
  const std::u8string body = u8"WEIGHT";
  // An axis the face moves advances on cannot be held on the glyphs, so the
  // restyle takes the other road: it re-shapes the words it covers, which is
  // what the verb promises anyway — and a routing decision the author never
  // asked about is nothing to warn about.
  ::testing::internal::CaptureStderr();
  const auto at = [&](float weight) {
    host.composer.render(box().padding(10).child(
        text(body, base)
            .spanStyle(Selector{}, withAxis(base, "wght", weight))
            .key("t")));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light = at(400.0f);
  const SkBitmap heavy = at(900.0f);
  const std::string log = ::testing::internal::GetCapturedStderr();
  int inked = 0;
  for (int y = 0; y < 120; y += 2)
    for (int x = 0; x < 400; x += 2)
      inked += light.getColor(x, y) != SK_ColorBLACK;
  ASSERT_GT(inked, 20) << "the text never drew";
  EXPECT_GT(pixelsDiffering(light, heavy, 400, 120), 20)
      << "an advance-variant axis did not re-shape — the glyphs kept the "
         "outline and pen positions the lighter weight gave them";
  EXPECT_EQ(log.find("moves advances"), std::string::npos)
      << "a restyle that re-shapes warned as if it had been refused: " << log;
}

TEST(TextSpanAxis, TheCoordinateTakesTheSizeScaledLadder) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  // A FIXED WINDOW of design space, swept at a fixed number of samples, at
  // two rendered sizes. The ladder is cut per rendered size, so the same
  // window holds more rungs on the larger type — and a rung is a retained
  // varied clone, which is countable where a pixel difference of one rung
  // is not. Both sizes sit inside the ladder's proportional band, away from
  // its floor and its ceiling, so this reads the proportion and not a clamp.
  constexpr float kSmallPx = 24.0f, kLargePx = 112.0f;
  constexpr int kSamples = 61;
  const float window = (hi - lo) / 32.0f;
  const auto clonesAcrossTheWindow = [&](float pixelSize) {
    sigil::weave::FontContext local(sigil::weave::ports::systemFontManager());
    sigil::motion::Ticker ticker;
    Composer composer(ticker, local);
    composer.setSize({200, 200});
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(200, 200));
    sigil::weave::TextStyle style = coloredStyle(pixelSize, SK_ColorWHITE);
    style.shaping.typeface = face;
    for (int i = 0; i < kSamples; ++i) {
      const float value =
          lo + (hi - lo) * 0.5f + window * (float)i / (float)(kSamples - 1);
      composer.render(box().padding(4).child(
          // A default-constructed selector addresses every glyph.
          text(u8"888", style)
              .key("t")
              .spanStyle(Selector{}, withAxis(style, "GRAD", value))));
      ticker.tick(1.0 / 60.0);
      surface->getCanvas()->clear(SK_ColorBLACK);
      composer.draw(*surface->getCanvas());
    }
    return local.variedTypefaceCount();
  };

  const size_t coarse = clonesAcrossTheWindow(kSmallPx);
  const size_t fine = clonesAcrossTheWindow(kLargePx);
  EXPECT_GT(coarse, 0u) << "the coordinate never reached a face at all";
  EXPECT_LT(coarse, (size_t)kSamples)
      << "every sample minted its own clone — the value is reaching the memo "
         "unsnapped";
  EXPECT_LT(coarse, fine)
      << "the same window of design space resolved to no more rungs at "
      << kLargePx << " px than at " << kSmallPx
      << " px, so the ladder is not cut by the rendered size";
}

TEST(TextSpanAxis, ALaterDeclarationWinsOnOverlap) {
  float lo = 0, hi = 0;
  const sk_sp<SkTypeface> face = gradFace(lo, hi);
  if (!face) GTEST_SKIP() << "no responsive advance-invariant GRAD face here";

  Host host(400, 120);
  sigil::weave::TextStyle base = coloredStyle(46, SK_ColorWHITE);
  base.shaping.typeface = face;
  const auto drawn = [&](const std::function<Element(Element)>& dress) {
    host.composer.render(
        box().padding(10).child(dress(text(u8"GRADE", base).key("t"))));
    host.frame();
    return grab(host, 400, 120);
  };
  const SkBitmap light = drawn([&](Element t) {
    return t.spanStyle(Selector{}, withAxis(base, "GRAD", lo));
  });
  const SkBitmap heavy = drawn([&](Element t) {
    return t.spanStyle(Selector{}, withAxis(base, "GRAD", hi));
  });
  ASSERT_GT(pixelsDiffering(light, heavy, 400, 120), 20)
      << "the two ends of the axis draw the same, so nothing below is a test";

  const SkBitmap both = drawn([&](Element t) {
    return t.spanStyle(Selector{}, withAxis(base, "GRAD", lo))
        .spanStyle(Selector{}, withAxis(base, "GRAD", hi));
  });
  EXPECT_EQ(pixelsDiffering(both, heavy, 400, 120), 0)
      << "the earlier declaration survived the later one — an axis is a "
         "substitution, and overlapping substitutions are last-one-wins";
}

TEST(TextOptionSetters, MaxLinesAndEllipsisClampTheText) {
  Host host(240, 200);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  const std::u8string body =
      u8"one two three four five six seven eight nine ten eleven twelve";
  host.composer.render(box().padding(10).child(
      text(body, base).width(180).maxLines(2).ellipsis(u8"...").key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2);
  EXPECT_TRUE(layout->overflowed());
  EXPECT_TRUE(layout->ellipsized) << "the marker never landed";
}

TEST(TextOptionSetters, HyphenationRendersTheHyphenAtASoftBreak) {
  Host host(260, 220);
  const sigil::weave::TextStyle base = coloredStyle(20, SK_ColorWHITE);
  // A short word, then a long one carrying a discretionary break, in a
  // measure that cannot hold both whole. The line breaks at the soft hyphen
  // either way; what the option controls is whether the hyphen is DRAWN,
  // which shows up as one extra run on the broken line.
  const std::u8string body = u8"short extraordi\u00adnarily";
  const auto runsOnFirstLineWith = [&](bool enabled) {
    host.composer.render(
        box().padding(4).child(text(body, base)
                                   .width(150)
                                   .hyphenation({.enabled = enabled})
                                   .key("t")));
    host.frame();
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return 0;
    int runs = 0;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex == 0) ++runs;
    return runs;
  };
  const int without = runsOnFirstLineWith(false);
  const int with = runsOnFirstLineWith(true);
  EXPECT_GT(with, without) << "the hyphenation setting never reached layout";
}

TEST(TextOptionSetters, SettersOverrideAPassedOptionsValueFieldByField) {
  // The contract for the full-control overload: a setter names one field and
  // leaves every other field of the passed options standing.
  Host host(300, 220);
  auto para = std::make_shared<sigil::weave::Paragraph>();
  para->appendText(std::u8string(u8"one two three four five six seven eight"),
                   coloredStyle(20, SK_ColorWHITE));
  sigil::weave::ParagraphLayoutOptions passed;
  passed.alignment = sigil::weave::TextAlignment::kCenter;
  passed.overflow.maxLines = 5;
  host.composer.render(
      box().child(text(para, passed).width(200).maxLines(2).key("t")));
  host.frame();
  const auto* layout = host.composer.paragraphLayout("t");
  ASSERT_NE(layout, nullptr);
  EXPECT_EQ(layout->lineCount, 2) << "the setter did not override maxLines";
  ASSERT_FALSE(layout->runs.empty());
  EXPECT_GT(layout->runs.front().origin.fX, 1.0f)
      << "the passed alignment was clobbered rather than left alone";
}

TEST(TextOptionSetters, KnuthPlassBreaksARaggedParagraphDifferently) {
  Host host(320, 260);
  const sigil::weave::TextStyle base = coloredStyle(18, SK_ColorWHITE);
  // Deliberately uneven word lengths: the greedy breaker fills each line as
  // far as it goes, and the optimal one trades an early line short to keep
  // the whole paragraph even.
  const std::u8string body =
      u8"a longer word then tiny bits of text and an extraordinarily "
      u8"lengthy one to finish the measure";
  const auto lineStartsUnder = [&](sigil::weave::LineBreakStrategy strategy) {
    host.composer.render(box().padding(4).child(
        text(body, base).width(240).lineBreak(strategy).key("t")));
    host.frame();
    std::vector<uint32_t> starts;
    const auto* layout = host.composer.paragraphLayout("t");
    if (!layout) return starts;
    int previous = -1;
    for (const sigil::weave::PositionedRun& run : layout->runs)
      if (run.lineIndex != previous) {
        starts.push_back(run.wordIndex);
        previous = run.lineIndex;
      }
    return starts;
  };
  const std::vector<uint32_t> greedy =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kGreedy);
  const std::vector<uint32_t> optimal =
      lineStartsUnder(sigil::weave::LineBreakStrategy::kKnuthPlass);
  ASSERT_GT(greedy.size(), 1u);
  EXPECT_NE(greedy, optimal) << "the break strategy setter changed nothing";
}

namespace {

/** A caption with one reserved slot, and a pill child keyed for it. */
Element pillCaption(std::string childKey, float width, SkSize size = {34, 16}) {
  // The width lives on an inner box: the render root is always resized to
  // the composer's own size, so a width written there is overwritten.
  return box().child(box().padding(8).width(width).child(
      text(rich(coloredStyle(18, SK_ColorWHITE))
               .add(u8"press the archive key ")
               .slot("pill", size, 4)
               .add(u8" to continue the long descent"))
          .key("caption")
          .child(box().key(std::move(childKey)).fill(red()))));
}

}  // namespace

TEST(TextSlot, AChildPaintsInsideTheReservedRect) {
  Host host(300, 200);
  host.composer.render(pillCaption("pill", 280));
  host.frame();

  const std::optional<SkRect> rect = host.composer.bounds("pill");
  ASSERT_TRUE(rect);
  // The box IS the size the content reserved — nothing about the child's
  // own description decides it.
  EXPECT_FLOAT_EQ(rect->width(), 34.0f);
  EXPECT_FLOAT_EQ(rect->height(), 16.0f);
  // …and the fill lands inside it.
  EXPECT_EQ(host.pixel((int)rect->centerX(), (int)rect->centerY()),
            SK_ColorRED);
  // The reserved run is blank: the caption's own words sit either side of
  // it, never through it.
  EXPECT_GT(countColor(host, SkIRect::MakeXYWH(0, 0, 300, 200), SK_ColorWHITE),
            20);
}

TEST(TextSlot, TheReservedRunIsUnbreakableAndMovesOnRelayout) {
  // The placeholder re-resolves with the paragraph: narrow the box and the
  // pill lands on a different line, at a different place on it.
  Host wide(300, 200), narrow(300, 200);
  wide.composer.render(pillCaption("pill", 280));
  wide.frame();
  narrow.composer.render(pillCaption("pill", 120));
  narrow.frame();

  const std::optional<SkRect> a = wide.composer.bounds("pill");
  const std::optional<SkRect> b = narrow.composer.bounds("pill");
  ASSERT_TRUE(a && b);
  EXPECT_NE(a->top(), b->top());
  // Same reserved size wherever it lands — the box travels, it does not
  // stretch, and no line breaks inside it.
  EXPECT_FLOAT_EQ(a->width(), b->width());
  EXPECT_FLOAT_EQ(a->height(), b->height());
  EXPECT_EQ(narrow.pixel((int)b->centerX(), (int)b->centerY()), SK_ColorRED);
}

TEST(TextSlot, ATallSlotOpensItsLine) {
  // The breakers treat the reserved box as a word with a height, so a slot
  // taller than the type pushes the whole paragraph down.
  Host shortPill(300, 240), tallPill(300, 240);
  shortPill.composer.render(pillCaption("pill", 280, {34, 16}));
  shortPill.frame();
  tallPill.composer.render(pillCaption("pill", 280, {34, 60}));
  tallPill.frame();
  const std::optional<SkRect> a = shortPill.composer.bounds("caption");
  const std::optional<SkRect> b = tallPill.composer.bounds("caption");
  ASSERT_TRUE(a && b);
  EXPECT_GT(b->height(), a->height());
}

TEST(TextSlot, AnUnknownKeyDrawsNothing) {
  // The loud-once member of the silent-no-op family: a child keyed for a
  // slot the content never reserved lays out at zero and paints nothing.
  Host host(300, 200);
  host.composer.render(pillCaption("typo", 280));
  host.frame();
  const std::optional<SkRect> rect = host.composer.bounds("typo");
  ASSERT_TRUE(rect);
  EXPECT_TRUE(rect->isEmpty());
  EXPECT_EQ(countColor(host, SkIRect::MakeXYWH(0, 0, 300, 200), SK_ColorRED),
            0);
}

TEST(TextSlot, TheHitTestReachesThePillChild) {
  Host host(300, 200);
  host.composer.render(pillCaption("pill", 280));
  host.frame();
  const std::optional<SkRect> rect = host.composer.bounds("pill");
  ASSERT_TRUE(rect);
  const std::optional<std::string> hit =
      host.composer.hitTest({rect->centerX(), rect->centerY()});
  ASSERT_TRUE(hit);
  EXPECT_EQ(*hit, "pill");
}

TEST(TextSlot, TheSlotNamespaceIsTheValuesOwnNotTheMountRegistry) {
  // Two captions may both reserve "icon" without colliding, because a text
  // slot is matched against THIS text node's children and nowhere else.
  Host host(320, 240);
  auto caption = [](SkColor ink, const char8_t* words) {
    return text(rich(coloredStyle(16, SK_ColorWHITE))
                    .slot("icon", {20, 12}, 2)
                    .add(words))
        .child(box().key("icon").fill(Fill::color(SkColor4f::FromColor(ink))));
  };
  host.composer.render(
      box()
          .column()
          .padding(6)
          .width(300)
          .child(caption(SK_ColorRED, u8" first line of the pair").key("a"))
          .child(
              caption(SK_ColorGREEN, u8" second line of the pair").key("b")));
  host.frame();
  const SkIRect all = SkIRect::MakeXYWH(0, 0, 320, 240);
  EXPECT_GT(countColor(host, all, SK_ColorRED), 20);
  EXPECT_GT(countColor(host, all, SK_ColorGREEN), 20);
}

// ---------------------------------------------------------------------------
// fx::tint — the colour reveal, and the inversion it hides

TEST(ComposeTextFx, TintRampsColorMulBetweenTheTwoColoursInTimeOrder) {
  // The arguments read in TIME ORDER while the mechanism runs the other
  // way: colorMul MULTIPLIES, so the element is set in the destination and
  // the effect divides down toward the origin. At t = 1 the multiplier must
  // therefore be white — anything else tints a line that has arrived.
  const SkColor4f pale{0.9f, 0.8f, 0.4f, 1};
  const SkColor4f sung{0.3f, 0.6f, 0.8f, 1};
  const TextEffect ramp = fx::tint(pale, sung);
  GlyphInfo glyph;
  Rng rng(1);
  const GlyphMod start = ramp(glyph, 0.0f, rng);
  const GlyphMod end = ramp(glyph, 1.0f, rng);
  const GlyphMod middle = ramp(glyph, 0.5f, rng);

  // Origin: the multiplier that takes the DESTINATION to `pale`.
  EXPECT_NEAR(start.colorMul.fR * sung.fR, pale.fR, 1e-5f);
  EXPECT_NEAR(start.colorMul.fG * sung.fG, pale.fG, 1e-5f);
  EXPECT_NEAR(start.colorMul.fB * sung.fB, pale.fB, 1e-5f);
  // Destination: no tint at all.
  EXPECT_NEAR(end.colorMul.fR, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fG, 1.0f, 1e-5f);
  EXPECT_NEAR(end.colorMul.fB, 1.0f, 1e-5f);
  // And a monotone ramp between them on every channel, in whichever
  // direction that channel happens to run.
  for (auto lane : {&SkColor4f::fR, &SkColor4f::fG, &SkColor4f::fB}) {
    const float a = start.colorMul.*lane, b = middle.colorMul.*lane;
    EXPECT_GT((b - a) * (1.0f - a), 0.0f)
        << "the middle of the ramp is not between its ends";
  }
  // Alpha is left alone: a reveal that also fades is a separate track.
  EXPECT_FLOAT_EQ(start.colorMul.fA, 1.0f);
  // The value is comparable, which is what lets a re-described wipe prune.
  EXPECT_TRUE(fx::tint(pale, sung) == ramp);
  EXPECT_FALSE(fx::tint(sung, pale) == ramp);
  // A destination channel of zero cannot be departed from, and the ramp
  // says so by holding at 1 rather than dividing by nothing.
  const GlyphMod dark = fx::tint({1, 1, 1, 1}, {0, 0, 0, 1})(glyph, 0.0f, rng);
  EXPECT_FLOAT_EQ(dark.colorMul.fR, 1.0f);
}

TEST(ComposeTextFx, TintComposesWithAnotherTrackByMultiplying) {
  // Stacked tracks multiply their colour multipliers, so a tint under a
  // second tint is the product — not the last one to run. The letter is set
  // in white so the product is readable straight off the pixels.
  Host host(160, 120);
  host.composer.render(box().padding(8).child(
      text(u8"I", whiteStyle(64))
          .key("k")
          // Both tracks are AT REST (progress 0), where each contributes
          // its own origin: 0.5 on red and 0.5 on green.
          .fx({.effect = fx::tint({0.5f, 1, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})
          .fx({.effect = fx::tint({1, 0.5f, 1, 1}, {1, 1, 1, 1}),
               .stagger = {.eachMs = 0, .durationMs = 100},
               .progress = 0.0f})));
  host.frame();
  bool sawProduct = false;
  for (int y = 0; y < 120 && !sawProduct; ++y)
    for (int x = 0; x < 160; ++x) {
      const SkColor c = host.pixel(x, y);
      if (SkColorGetB(c) < 200) continue;  // not a glyph pixel
      // Half red AND half green, within the multiplier's quantization.
      if (std::abs((int)SkColorGetR(c) - 128) < 24 &&
          std::abs((int)SkColorGetG(c) - 128) < 24) {
        sawProduct = true;
        break;
      }
      EXPECT_FALSE(SkColorGetR(c) > 200 && SkColorGetG(c) > 200)
          << "one of the two tints never reached the glyph";
    }
  EXPECT_TRUE(sawProduct) << "the two tints did not multiply";
}

namespace {

/** The mark's rect, as the composer reports it. */
SkRect markRect(Host& host, std::string_view key) {
  const std::optional<SkRect> rect = host.composer.bounds(key);
  return rect.value_or(SkRect::MakeEmpty());
}

}  // namespace

TEST(ComposeTextFx, MarkPlacesAChildOnTheRectItsSelectorResolves) {
  // A mark's box IS the unit's rect when it says nothing about its own
  // placement — and it is the SAME rect the schedule read-back reports, so
  // a caret and a beat can never disagree about where a word is.
  Host host(400, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(1), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("line", 0);
  ASSERT_EQ(beats.size(), 3u);
  const SkRect caret = markRect(host, "caret");
  EXPECT_NEAR(caret.left(), beats[1].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[1].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[1].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[1].rect.height(), 0.01f);
  EXPECT_GT(caret.width(), 1.0f) << "the mark collapsed to nothing";

  // Its own placement longhand is read INSIDE that rect, which is the whole
  // difference from a slot: a 2 px caret pinned to the unit's leading edge
  // and hanging below it.
  Host pinned(400, 140);
  pinned.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA GAMMA", whiteStyle(24))
          .key("line")
          .mark(
              sel::word(1),
              box().key("caret").left(0).top(pct(100)).width(2).height(9).fill(
                  green()))));
  pinned.frame();
  const SkRect tick = markRect(pinned, "caret");
  EXPECT_NEAR(tick.left(), caret.left(), 0.01f);
  EXPECT_NEAR(tick.top(), caret.bottom(), 0.01f)
      << "pct(100) of the unit's rect is its bottom edge";
  EXPECT_FLOAT_EQ(tick.width(), 2.0f);
}

TEST(ComposeTextFx, MarkFollowsItsUnitWhenTheTextReflows) {
  // The rect is read off the placement, so a narrower box that pushes the
  // word onto the next line takes the mark with it — the reason to anchor a
  // caret rather than compute one.
  const auto placeAt = [](float width) {
    Host host(400, 200);
    host.composer.render(box().padding(10).child(
        text(u8"ALPHA BETA GAMMA DELTA", whiteStyle(24))
            .key("line")
            .width(width)
            .mark(sel::word(3), box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect wide = placeAt(360);
  const SkRect narrow = placeAt(150);
  ASSERT_FALSE(wide.isEmpty());
  ASSERT_FALSE(narrow.isEmpty());
  EXPECT_GT(narrow.top(), wide.top())
      << "the word wrapped onto another line and the mark stayed behind";
}

TEST(ComposeTextFx, MarkStandsAtRestWhileACascadeDeviatesTheGlyphs) {
  // The rect is where the LAYOUT put the glyphs, not where a track has
  // thrown them: a deviation is per glyph and per track and several
  // compose, so there is no one place a moving unit "is". A mark that must
  // ride the motion reads beatsOf and drives its own transform.
  const auto placeAtProgress = [](float progress) {
    Host host(400, 200);
    host.composer.render(box().padding(10).child(
        text(u8"ALPHA BETA", whiteStyle(24))
            .key("line")
            .fx({.effect = fx::rise(40),
                 .stagger = {.eachMs = 0, .durationMs = 100},
                 .progress = progress})
            .mark(sel::word(1), box().key("caret").fill(green()))));
    host.frame();
    return markRect(host, "caret");
  };
  const SkRect early = placeAtProgress(0.0f);
  const SkRect settled = placeAtProgress(1.0f);
  ASSERT_FALSE(settled.isEmpty());
  EXPECT_NEAR(early.top(), settled.top(), 0.01f);
  EXPECT_NEAR(early.left(), settled.left(), 0.01f);
}

TEST(ComposeTextFx, MarkOnAPathRunStandsOnTheCurve) {
  // A path-laid run's marks stand on the CURVE the letters stand on: the
  // rect is the union of the advance boxes where the baseline placed them,
  // the same placement beatsOf reports — so a caret and a beat cannot
  // disagree on a ring any more than they can on a line. Resolved after
  // layout, because the curve resolves against the node's final box.
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .onPath({.path = shapes::circle()})
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(2), box().key("caret").fill(green()))));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("ring", 0);
  ASSERT_GT(beats.size(), 2u);
  const SkRect caret = markRect(host, "caret");
  ASSERT_FALSE(caret.isEmpty()) << "the mark placed nothing on the curve";
  EXPECT_NEAR(caret.left(), beats[2].rect.left(), 0.01f);
  EXPECT_NEAR(caret.top(), beats[2].rect.top(), 0.01f);
  EXPECT_NEAR(caret.width(), beats[2].rect.width(), 0.01f);
  EXPECT_NEAR(caret.height(), beats[2].rect.height(), 0.01f);
  // And it IS the curved placement, not the straight flow line the run
  // does not use: the same content laid straight puts the word somewhere
  // else entirely.
  Host straight;
  straight.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING IT GOES", whiteStyle(18))
          .key("ring")
          .width(180)
          .height(180)
          .fx({.effect = fx::rise(4), .stagger = stagger(unit::Word)})
          .mark(sel::word(2), box().key("caret").fill(green()))));
  straight.frame();
  const SkRect flow = markRect(straight, "caret");
  EXPECT_TRUE(std::abs(caret.left() - flow.left()) > 1.0f ||
              std::abs(caret.top() - flow.top()) > 1.0f)
      << "the curved rect matched the straight one — the probe proves "
         "nothing at this size";
}

TEST(ComposeTextFx, MarkResolvingNothingPlacesNothing) {
  // The silent-no-op family's terms, with the warning that goes with them:
  // a style name no run carries selects nothing, and a mark on nothing must
  // draw nothing rather than land at the text node's origin.
  Host host(300, 140);
  host.composer.render(box().padding(10).child(
      text(u8"ALPHA BETA", whiteStyle(24))
          .key("line")
          .mark(sel::style("nobody"),
                box().key("caret").width(30).height(30).fill(green()))));
  host.frame();
  EXPECT_TRUE(markRect(host, "caret").isEmpty())
      << "a mark on nothing took a box anyway";
  int greens = 0;
  for (int y = 0; y < 140; ++y)
    for (int x = 0; x < 300; ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) ++greens;
  EXPECT_EQ(greens, 0) << "a mark on nothing drew something";
}

TEST(ComposeTextFx, MarkPrunesAndReResolvesWhenItMoves) {
  // A mark is a comparable selector plus the key of the child it anchors,
  // so a re-described identical mark must prune — and one pointed at a
  // different word must not, or the caret keeps the rect it had.
  Host host(400, 140);
  const auto describe = [](uint32_t word) {
    return box().padding(10).child(
        text(u8"ALPHA BETA GAMMA", whiteStyle(24))
            .key("line")
            .mark(sel::word(word), box().key("caret").fill(green())));
  };
  host.composer.render(describe(0));
  host.frame();
  const SkRect first = markRect(host, "caret");
  host.composer.render(describe(0));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged mark list did not prune";
  EXPECT_NEAR(markRect(host, "caret").left(), first.left(), 0.01f);

  host.composer.render(describe(2));
  host.frame();
  EXPECT_GT(markRect(host, "caret").left(), first.left() + 1.0f)
      << "the mark kept the rect the previous selector resolved";
}

TEST(ComposeTextFx, MarkIsNotASlotAndReservesNoSpaceInTheFlow) {
  // The distinction the header states: a slot's box is woven into the line
  // and the type after it starts further along; a mark is placed on a line
  // laid out as though it were not there.
  const auto widthOf = [](Element leaf) {
    Host host(400, 140);
    host.composer.render(box().padding(10).child(std::move(leaf).key("line")));
    host.frame();
    return host.composer.bounds("line").value_or(SkRect::MakeEmpty()).width();
  };
  const float bare = widthOf(text(u8"ALPHA BETA", whiteStyle(24)));
  const float marked =
      widthOf(text(u8"ALPHA BETA", whiteStyle(24))
                  .mark(sel::word(0), box().key("m").width(40).fill(green())));
  EXPECT_NEAR(marked, bare, 0.01f) << "the mark reserved space in the flow";
}
