// THE CASCADE AND THE SELECTION a track runs on: which units get a beat and
// in what order, what a selector addresses, how two tracks compose, what a
// track reserves, and when a tracked run prunes, bakes and settles.
//
// The text binary's share of the content suites, one file per subject.

#include <numeric>

#include "DressedTypeProbes.h"

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
                    [into](const GlyphInfo& g, float t,
                           sigil::core::noise::Mix64Stream& rng) {
                      into->push_back({g, t, rng.unit()});
                      return GlyphModifier{};
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
Track probeTrack(std::vector<FxSample>* into, sigil::weave::Selector where = {},
                 sigil::motion::Spread cascade = {.eachMs = 0,
                                                  .durationMs = 100},
                 float progress = 1.0f,
                 sigil::weave::Unit over = sigil::weave::Unit::Cluster) {
  return Track{.where = std::move(where),
               .effect = probe("probe", into),
               .stagger = std::move(cascade),
               .unit = over,
               .progress = progress};
}

}  // namespace

TEST(ComposeTextFx, AWordCascadeBeatsOncePerWordAndInOrder) {
  // The stagger runs over the track's UNITS, so a word-unit cascade gives
  // every glyph of a word one shared local time and delays the next word by
  // a whole beat. Per-glyph spacing inside a word would be a different
  // effect entirely, and is what `weave::Unit::Glyph` is for.
  Host host(300, 120);
  std::vector<FxSample> samples;
  host.composer.render(box().padding(10).child(
      text(u8"AAA BBB CCC", whiteStyle(20))
          .key("k")
          .fx(probeTrack(&samples, {}, {.eachMs = 100, .durationMs = 100}, 0.5f,
                         sigil::weave::Unit::Word))));
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
          .fx(probeTrack(&samples, {}, {.eachMs = 100, .durationMs = 100}, 0.5f,
                         sigil::weave::Unit::Sentence))));
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
      text(u8"x́ýz", markStyle(28))
          .key("k")
          .fx(probeTrack(&samples, {}, {.eachMs = 100, .durationMs = 100},
                         0.5f))));
  host.frame();
  ASSERT_FALSE(samples.empty());
  const uint32_t beats = samples.front().info.unitCount;
  ASSERT_LT((size_t)beats, samples.size())
      << "the instrument composed the marks into single glyphs, so there is "
         "no multi-glyph cluster left to keep together";
  // Glyphs at one text offset are one cluster and must share one time.
  for (size_t i = 1; i < samples.size(); ++i)
    if (samples[i].info.textIndex == samples[i - 1].info.textIndex) {
      EXPECT_EQ(samples[i].info.unitIndex, samples[i - 1].info.unitIndex);
      EXPECT_FLOAT_EQ(samples[i].t, samples[i - 1].t)
          << "a combining mark was staggered away from its base letter";
    }

  // THE CONTROL: the same text over weave::Unit::Glyph, which is the raw
  // shaping unit and DOES separate a mark from its base. Without it the check
  // above is satisfied by a cascade that never beat at all.
  std::vector<FxSample> raw;
  host.composer.render(box().padding(10).child(
      text(u8"x́ýz", markStyle(28))
          .key("k")
          .fx(probeTrack(&raw, {}, {.eachMs = 100, .durationMs = 400}, 0.5f,
                         sigil::weave::Unit::Glyph))));
  host.frame();
  ASSERT_EQ(raw.size(), samples.size());
  EXPECT_EQ(raw.front().info.unitCount, (uint32_t)raw.size());
  bool anySplit = false;
  for (size_t i = 1; i < raw.size(); ++i)
    if (raw[i].info.textIndex == raw[i - 1].info.textIndex &&
        raw[i].t != raw[i - 1].t)
      anySplit = true;
  EXPECT_TRUE(anySplit) << "weave::Unit::Glyph did not split the cluster, so "
                           "the cluster case above "
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
                    .textFill(material::skia::Paint::solid({0, 1, 0, 1}));
    if (moving)
      t.fx({.effect = fx::effect("still", [](const GlyphInfo&, float,
                                             sigil::core::noise::Mix64Stream&) {
              return GlyphModifier{};
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
  const auto run = [&](sigil::weave::Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(run(sigil::weave::selectors::word(1)),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sigil::weave::selectors::words(1, 3)),
            (std::vector<size_t>{3, 4, 5, 6, 7, 8}));
  EXPECT_EQ(run(sigil::weave::selectors::text(u8"BBB")),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sigil::weave::selectors::regex(u8"B+")),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(sigil::weave::selectors::line(0)).size(),
            9u);                                        // one line, everything
  EXPECT_EQ(run(sigil::weave::Selector()).size(), 9u);  // default: everything
  EXPECT_TRUE(run(sigil::weave::selectors::regex(u8"([")).empty())
      << "a pattern that does not compile must select NOTHING rather than "
         "everything — silently addressing the whole paragraph is the "
         "failure this rule exists to prevent";
}

TEST(ComposeTextFx, SelectorAlgebraUnionsIntersectsAndComplements) {
  Host host(300, 120);
  const auto run = [&](sigil::weave::Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  EXPECT_EQ(
      run(sigil::weave::selectors::word(0) | sigil::weave::selectors::word(2)),
      (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_EQ(run(sigil::weave::selectors::words(0, 2) &
                sigil::weave::selectors::word(1)),
            (std::vector<size_t>{3, 4, 5}));
  EXPECT_EQ(run(!sigil::weave::selectors::word(1)),
            (std::vector<size_t>{0, 1, 2, 6, 7, 8}));
  EXPECT_TRUE(
      run(sigil::weave::selectors::word(0) & sigil::weave::selectors::word(1))
          .empty());
}

TEST(ComposeTextFx, EachTakeAndDropPartitionEveryUnitExactly) {
  // take(n) and drop(n) answer opposite sides of one cut inside every unit,
  // so together they cover the text exactly once. A gap or an overlap here
  // means a two-track composition double-counts letters it should split.
  Host host(300, 120);
  const auto run = [&](sigil::weave::Selector where) {
    std::vector<FxSample> samples;
    host.composer.render(box().padding(10).child(
        text(u8"AAA BBB CCC", whiteStyle(20))
            .key("k")
            .fx(probeTrack(&samples, std::move(where)))));
    host.frame();
    return addressed(samples);
  };
  const std::vector<size_t> firsts =
      run(sigil::weave::selectors::each(sigil::weave::Unit::Word).take(1));
  const std::vector<size_t> rest =
      run(sigil::weave::selectors::each(sigil::weave::Unit::Word).drop(1));
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
                            .from = sigil::motion::Spread::From::Random},
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
  // The stream an effect draws from is seeded per glyph and is stable too.
  for (size_t i = 0; i < first.size(); ++i)
    EXPECT_FLOAT_EQ(first[i].random, second[i].random);
}

TEST(ComposeTextFx, RandomSeedDealsItsOwnScatterAndZeroKeepsTheDefault) {
  // From::Random ranks units by a hash keyed on the count and the seed.
  // Three claims, each a behaviour an author leans on: seed 0 IS the
  // count-keyed deal (pinned against the exact ranks that key hashes to, so
  // every settled scene keeps its scatter bit for bit), a nonzero seed deals
  // a different permutation, and two nonzero seeds deal independently.
  const auto ranksOf = [](uint32_t seed) {
    Host host(300, 120);
    sigil::motion::Spread scatter{.eachMs = 100,
                                  .durationMs = 100,
                                  .from = sigil::motion::Spread::From::Random};
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
  sigil::motion::Spread a{.from = sigil::motion::Spread::From::Random},
      b{.from = sigil::motion::Spread::From::Random};
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
  sigil::motion::Spread cascade{.eachMs = 200, .durationMs = 100};
  cascade.then({.eachMs = 50, .durationMs = 100});
  // A beat is 100 + 50·2 = 200 ms and the whole cascade spans 400; 0.3 of
  // that lands inside the first word's beat, where the two ladders are
  // both readable.
  host.composer.render(
      box().padding(10).child(text(u8"AAA BBB", whiteStyle(20))
                                  .key("k")
                                  .fx(probeTrack(&samples, {}, cascade, 0.3f,
                                                 sigil::weave::Unit::Word))));
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
        [dx](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
          GlyphModifier m;
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

TEST(ComposeTextFx, ATrackedRunSurvivesTheBakeItIsCachedInto) {
  // A run of turned-and-placed glyphs is handed to the canvas with bounds
  // beside it, and those bounds decide whether the canvas draws the run at
  // all. Get them wrong and the failure is TOTAL and silent: the batch is
  // rejected whole and the node paints nothing, no diagnostic, at some sizes
  // and not others, for some faces and not others — because whether the
  // wrong rectangle happens to overlap the clip depends on the face's own
  // bounding box, on where the glyphs sit and on how big the surface is.
  //
  // A texture bake is the surface that exposes it: it is the size of the
  // node's own paint bounds rather than the canvas's, so the same run that
  // draws live can vanish once the node is cached. All three spellings must
  // paint the same letter in the same place.
  Host host(220, 200);
  const auto tracked = [](bool cached) {
    Element leaf = text(u8"I", whiteStyle(40))
                       .key("k")
                       .fx({.effect = fx::effect(
                                "drop",
                                [](const GlyphInfo&, float,
                                   sigil::core::noise::Mix64Stream&) {
                                  GlyphModifier m;
                                  m.dy = 60;
                                  return m;
                                },
                                /*reach=*/80.0f)});
    if (cached) leaf.cache(Cache::Texture);
    return box().padding(10).child(std::move(leaf));
  };
  const auto ink = [&](bool cached) {
    host.composer.render(tracked(cached));
    host.frame();
    auto b = host.composer.bounds("k");
    EXPECT_TRUE(b.has_value());
    return anyWhiteIn(
        host, SkIRect::MakeLTRB((int)b->left(), (int)b->top() + 60,
                                (int)b->right(), (int)b->bottom() + 60));
  };
  EXPECT_TRUE(ink(false)) << "the track alone drew nothing";
  EXPECT_TRUE(ink(true))
      << "the same track drew nothing once the node was baked into a texture";
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
                [](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
                  GlyphModifier m;
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
  const auto body = [](const GlyphInfo&, float,
                       sigil::core::noise::Mix64Stream&) {
    return GlyphModifier{};
  };
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
