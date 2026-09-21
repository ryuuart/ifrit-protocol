// Live text: what a moving passage costs frame by frame, what its last
// layout reports, and which frames a composing passage is kept out of the
// cache for.

#include <vector>

#include "support/ParagraphTestSupport.h"

namespace {

/** A floor the breaker meets on the passage above with room to spare, in
 *  break candidates: a block of it weighs a couple of thousand at these
 *  measures. */
constexpr int kMetFloor = 20000;

/** One frame of a live passage at `measure`, reporting what it cost. */
TextSettling settlingAt(Host& host, float measure, int candidates) {
  host.composer.render(box().children(
      {text(longPassage(), whiteStyle(13))
           .key("t")
           .width(measure)
           .block({.lineBreak = sigil::weave::LineBreakStrategy::kKnuthPlass})
           .textWillChange(true, candidates)}));
  host.frame();
  return host.composer.settling("t");
}

/** A floor the swell's block meets at every measure in its range, with
 *  room to spare, in break candidates: a block of this passage weighs a
 *  few hundred anywhere in that range. */
constexpr int kSweptFloor = 4000;

/** A passage whose measure swells from 150 px to 230 px and back, drawn at
 *  every whole pixel, then set once more at @p endAt — and what
 *  `Composer::settling` reports about that last frame. */
TextSettling sweptSettling(bool live, int candidates, float endAt) {
  Host host(280, 320);
  const auto step = [&](float measure) {
    // Said twice: the floor is tested once per break position, so a block
    // has to carry enough of them for the search to be still running at
    // the second one.
    Element leaf =
        text(
            u8"A measure that animates is one input of a run of layouts "
            u8"rather than a question somebody asked once, and the block "
            u8"that knows so keeps the break decisions it has already "
            u8"made. A measure that animates is one input of a run of "
            u8"layouts rather than a question somebody asked once, and "
            u8"the block that knows so keeps the break decisions it has "
            u8"already made.",
            whiteStyle(11.5f))
            .key("para")
            .width(measure)
            .block({.lineBreak = sigil::weave::LineBreakStrategy::kKnuthPlass});
    if (live) leaf.textWillChange(true, candidates);
    host.composer.render(box().padding(10).children({std::move(leaf)}));
    host.frame();
  };
  for (float measure = 150; measure <= 230; measure += 1) step(measure);
  for (float measure = 230; measure >= 150; measure -= 1) step(measure);
  step(endAt);
  return host.composer.settling("para");
}

}  // namespace

TEST(ComposeLiveText, ABoundMeasureRelaysEveryFrameWithoutGrowingTheTree) {
  Host host(600, 500);
  // The width sweeps, so every frame is a layout the frame before it did
  // not answer — which is what a bound measure is.
  const std::vector<float> sweep = {320, 330, 340, 350, 360, 370, 380};
  std::vector<int> lineCounts;
  for (const float measure : sweep) {
    settlingAt(host, measure, 0);
    const sigil::weave::ParagraphLayout* layout =
        host.composer.paragraphLayout("t");
    ASSERT_NE(layout, nullptr);
    lineCounts.push_back(layout->lineCount);
  }
  // It re-laid: a wider measure took fewer lines than the narrowest.
  EXPECT_GT(lineCounts.front(), lineCounts.back());

  // …AND THE TREE DOES NOT GROW WHILE IT DOES. A frame of a moving passage
  // re-decides breaks and re-fills; what it must not do is leave anything
  // behind. Compared between two runs of the same sweep rather than
  // against the first frame, because the first run of a sweep is also the
  // one that warms the caches a settled measure is then answered from.
  for (int pass = 0; pass < 3; ++pass)
    for (const float measure : sweep) settlingAt(host, measure, 0);
  const Composer::Stats warm = host.composer.stats();
  for (int pass = 0; pass < 3; ++pass)
    for (const float measure : sweep) settlingAt(host, measure, 0);
  const Composer::Stats after = host.composer.stats();
  EXPECT_EQ(after.instances, warm.instances);
  EXPECT_EQ(after.picturesLive, warm.picturesLive);
  EXPECT_EQ(after.texturesLive, warm.texturesLive);

  // …and a measure it has already crossed costs no break decision at all:
  // the block is answered from decisions this thread already has.
  const TextSettling seen = settlingAt(host, sweep.front(), 0);
  EXPECT_TRUE(seen.live);
  EXPECT_GT(seen.reused, 0);
  EXPECT_EQ(seen.degraded, 0);
}

TEST(ComposeLiveText, ASettledTextDecidesItsBreaksOnceAndThenComposesNothing) {
  Host host(600, 500);
  const Element leaf = box().children(
      {text(longPassage(), whiteStyle(13))
           .key("t")
           .width(340.0f)
           .block(
               {.lineBreak = sigil::weave::LineBreakStrategy::kKnuthPlass})});
  host.composer.render(leaf);
  host.frame();
  // A settled passage never asks the break store — it is not one of a run
  // of layouts, so there is nothing for a later frame to reuse.
  const TextSettling first = host.composer.settling("t");
  EXPECT_FALSE(first.live);
  EXPECT_EQ(first.reused, 0);
  EXPECT_EQ(first.degraded, 0);

  // …and the frames after it compose nothing at all: the layout is valid
  // for the measure it was asked at, so no dynamic program runs and the
  // node's recording stands.
  for (int i = 0; i < 3; ++i) {
    host.composer.render(leaf);
    host.frame();
  }
  const Composer::Stats settled = host.composer.stats();
  EXPECT_EQ(settled.picturesRecorded, 0u);
  EXPECT_EQ(host.composer.settling("t").reused, 0);
  EXPECT_EQ(host.composer.settling("t").degraded, 0);
}

TEST(ComposeLiveText, AnInheritingPassageSettlesExactlyAsATotalOneDoes) {
  // The settling report — and the break store behind it — must not depend
  // on whether the passage's style was inherited or written whole: an
  // inheriting leaf is shaped once, in the font it lands in, never first
  // against the root and again after the cascade.
  const auto sweep = [](bool inherits) {
    Host host(600, 500);
    std::vector<TextSettling> answers;
    for (const float measure : {320.0f, 340.0f, 360.0f, 340.0f, 320.0f}) {
      Element leaf =
          inherits ? text(longPassage())
                         .font({.size = 13, .color = SkColor4f{1, 1, 1, 1}})
                   : text(longPassage(),
                          sigil::weave::textStyle(
                              {.size = 13, .color = SkColor4f{1, 1, 1, 1}}));
      host.composer.render(box().children(
          {std::move(leaf)
               .key("t")
               .width(measure)
               .block(
                   {.lineBreak = sigil::weave::LineBreakStrategy::kKnuthPlass})
               .textWillChange(true, 1)}));
      host.frame();
      answers.push_back(host.composer.settling("t"));
    }
    return answers;
  };
  const std::vector<TextSettling> total = sweep(false);
  const std::vector<TextSettling> inheriting = sweep(true);
  ASSERT_EQ(total.size(), inheriting.size());
  for (size_t i = 0; i < total.size(); ++i) {
    EXPECT_TRUE(total[i] == inheriting[i]) << "frame " << i;
    EXPECT_TRUE(total[i].live);
    EXPECT_EQ(total[i].reused, 0) << "a degraded frame stores nothing";
    EXPECT_EQ(total[i].degraded, 1) << "one block, over a floor of one";
  }
}

TEST(ComposeLiveText, ADegradedFrameIsProvisionalAndTheSettingComesBack) {
  Host host(600, 500);
  // A floor no breaker can meet: the block is filled greedily for that
  // frame and says so.
  const TextSettling starved = settlingAt(host, 340.0f, 1);
  EXPECT_TRUE(starved.live);
  EXPECT_GT(starved.degraded, 0);
  // …and a floor it can meet gets the setting back, at the same measure,
  // because a degrade never held the layout as the answer for it.
  const TextSettling fed = settlingAt(host, 340.0f, kMetFloor);
  EXPECT_EQ(fed.degraded, 0);
}

TEST(ComposeSettling, AMeasureAlreadyCrossedCostsNoBreakDecision) {
  // The break decisions of a live block are kept and reused, keyed on the
  // words and on the measure taken to the whole pixel below it. A swell
  // that has run the whole range and comes back to a measure inside it
  // therefore decides nothing — and the report says so, at either end.
  const TextSettling narrow = sweptSettling(true, kSweptFloor, 150.0f);
  EXPECT_TRUE(narrow.live);
  EXPECT_GT(narrow.reused, 0);
  EXPECT_EQ(narrow.degraded, 0);
  const TextSettling wide = sweptSettling(true, kSweptFloor, 230.0f);
  EXPECT_TRUE(wide.live);
  EXPECT_GT(wide.reused, 0);
  EXPECT_EQ(wide.degraded, 0);
}

TEST(ComposeSettling, APassageThatNeverSaidItMovesStoresNothing) {
  const TextSettling settled = sweptSettling(false, 0, 230.0f);
  EXPECT_FALSE(settled.live);
  EXPECT_EQ(settled.reused, 0);
  EXPECT_EQ(settled.degraded, 0);
}

TEST(ComposeSettling, AFloorNothingCanMeetDegradesAndSaysSo) {
  // The floor under a frame the optimizing breaker cannot finish: the
  // block is filled greedily for that frame and counted. The floor is
  // tested DURING the search, at each break position, so a block of one
  // position is never stopped by one — and a floor of one stops every
  // block longer than that.
  const TextSettling starved = sweptSettling(true, 1, 230.0f);
  EXPECT_TRUE(starved.live);
  EXPECT_GT(starved.degraded, 0);
}

TEST(ComposeSettling, TheSameSwellRunTwiceReportsTheSameSettling) {
  // WHAT A SWELL REPORTS IS A FACT ABOUT THE TEXT AND THE MEASURE. How
  // many candidates a block weighs is decided by its words and the
  // measure it is set in, so the floor is met or missed the same way on
  // every run — and a block that meets it puts its break decisions in the
  // store every later step of the swell is answered from, which is why
  // one block deciding differently would change the whole report and the
  // whole setting. Run under the floor nothing can meet, which is where a
  // floor spent against a clock raced.
  const TextSettling first = sweptSettling(true, 1, 230.0f);
  const TextSettling second = sweptSettling(true, 1, 230.0f);
  EXPECT_EQ(first.reused, second.reused);
  EXPECT_EQ(first.degraded, second.degraded);
  EXPECT_EQ(first.live, second.live);
}

// A COMPOSING PASSAGE IS NEVER PROVEN STILL. A live block that had to
// DECIDE a break this frame, or that the floor DEGRADED, may be set
// differently the next frame with no number on this node changing — which
// is precisely what no memo can see — so the layout reports it and the
// volatility pass keeps the node out of the cache. The frame it answers
// every block from decisions it already had and degrades none, it is
// proved still like anything else and the cache takes it. Read as the
// node's own cache state, so the claim is what the painter did rather
// than what a counter said.
TEST(ComposeLiveText,
     AComposingPassageIsKeptOutOfTheCacheForExactlyThoseFrames) {
  Host host(600, 500);
  host.composer.setProfiling(true);
  const auto stateOfT = [&]() {
    for (const Composer::NodeCost& r : host.composer.profile())
      if (r.label.rfind("t (", 0) == 0) return r.cacheState;
    ADD_FAILURE() << "the passage was never painted";
    return Composer::CacheState::Live;
  };
  // Cache::None above it so the leaf is visited every frame; without that
  // a settled node paints once into an ancestor's recording and never
  // appears in the profile at all.
  const auto frameAt = [&](float measure, int candidates) {
    host.composer.render(
        box()
            .cache(Cache::None)
            .children(
                {text(longPassage(), whiteStyle(13))
                     .key("t")
                     .width(measure)
                     .block({.lineBreak =
                                 sigil::weave::LineBreakStrategy::kKnuthPlass})
                     .textWillChange(true, candidates)}));
    host.frame();
    return host.composer.settling("t");
  };

  // A measure this thread has not settled before: every block is decided,
  // nothing is reused, and the node paints live for as long as that holds.
  for (int i = 0; i < 3; ++i) {
    const TextSettling composing = frameAt(340.0f, 0);
    EXPECT_EQ(composing.reused, 0);
    EXPECT_EQ(composing.degraded, 0);
    EXPECT_EQ(stateOfT(), Composer::CacheState::Live);
  }

  // Move off the measure and back, and the block is answered from the
  // decisions the store now holds: reuse with no degrade is the frame the
  // node stops composing, and it is cached from that frame.
  frameAt(360.0f, 0);
  for (int i = 0; i < 3; ++i) {
    const TextSettling settled = frameAt(340.0f, 0);
    EXPECT_GT(settled.reused, 0);
    EXPECT_EQ(settled.degraded, 0);
    EXPECT_EQ(stateOfT(), Composer::CacheState::Picture);
  }

  // A floor no breaker can meet degrades the block, which is
  // provisional by construction — so the node is out of the cache again on
  // exactly the frames that report it.
  for (int i = 0; i < 2; ++i) {
    const TextSettling starved = frameAt(340.0f, 1);
    EXPECT_GT(starved.degraded, 0);
    EXPECT_EQ(stateOfT(), Composer::CacheState::Live);
  }
}
