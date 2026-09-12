// THE SCHEDULE READ BACK: where beatsOf says the glyphs went and when, what
// cascadeSpanMs the master progress maps onto, a cue table's own times, and
// a looping cascade reopening each unit on its own cycle.
//
// The text binary's share of the content suites, one file per subject.

#include "DressedTypeProbes.h"

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
    const sigil::motion::Spread spec{.eachMs = 100, .durationMs = 100};
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC DD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.where =
                     sigil::weave::selectors::each(sigil::weave::Unit::Word)
                         .take(1) &
                     sigil::weave::selectors::words(1, 4),
                 .effect = fx::rise(6),
                 .stagger = spec,
                 .unit = sigil::weave::Unit::Word,
                 .beatsOver = numbering})
            .fx({.where =
                     sigil::weave::selectors::each(sigil::weave::Unit::Word)
                         .drop(1),
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
  // weave::Unit k starts at table[k], exactly, and nothing about `eachMs`
  // survives beside it.
  const std::vector<float> table{0.0f, 340.0f, 720.0f, 1180.0f};
  Host host(400, 120);
  host.composer.render(box().padding(6).child(
      text(u8"AA BB CC DD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger =
                   sigil::motion::Spread{.eachMs = 999, .durationMs = 180}.cues(
                       table),
               .unit = sigil::weave::Unit::Word})));
  host.frame();
  const std::vector<Beat> beats = host.composer.beatsOf("p", 0);
  ASSERT_EQ(beats.size(), table.size());
  for (size_t k = 0; k < table.size(); ++k) {
    EXPECT_EQ(beats[k].unitIndex, (uint32_t)k);
    EXPECT_FLOAT_EQ(beats[k].startMs, table[k])
        << "unit " << k << " did not start at its own cue";
  }
  // cues() answers a spread, so it compares like one — and a different table is
  // a different cascade, or a re-described track would prune onto the old
  // schedule and keep singing the previous line's timing.
  EXPECT_TRUE(sigil::motion::Spread{}.cues(table) ==
              sigil::motion::Spread{}.cues(table));
  EXPECT_FALSE(sigil::motion::Spread{}.cues(table) ==
               sigil::motion::Spread{}.cues({0.0f, 340.0f, 720.0f}));
  EXPECT_FALSE(sigil::motion::Spread{}.cues(table) == sigil::motion::Spread{});
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
               .stagger = sigil::motion::Spread{.durationMs = 100}.cues(
                   {0.0f, 200.0f}),
               .unit = sigil::weave::Unit::Word})));
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
  sigil::weave::RichText copy = sigil::weave::rich(whiteStyle(15));
  copy.add(u8"alpha bravo ").add(u8"charlie", whiteStyle(24)).add(u8" delta");
  host.composer.render(
      box().padding(10).child(text(copy).key("p").width(120).fx(
          {.effect = fx::rise(8),
           .stagger = {.eachMs = 100, .durationMs = 200},
           .unit = sigil::weave::Unit::Word,
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
          .fx({.effect = fx::rise(4), .unit = sigil::weave::Unit::Cluster})));
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
  // A nested beat lasts exactly as long as its inner ladder needs, so no
  // author can restate the start
  // times. Word w's letter i opens at w·outerEach + i·innerEach, and the
  // read-back is the only place that is true without being retyped.
  Host host(400, 120);
  sigil::motion::Spread cascade{.eachMs = 300};
  cascade.then({.eachMs = 40, .durationMs = 100});
  host.composer.render(box().padding(6).child(
      text(u8"AB CD", whiteStyle(16))
          .key("p")
          .width(360)
          .fx({.effect = fx::rise(6),
               .stagger = cascade,
               .unit = sigil::weave::Unit::Word,
               .innerUnit = sigil::weave::Unit::Cluster})));
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
          .fx({.effect = fx::rise(6), .unit = sigil::weave::Unit::Word})));
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
  const sigil::motion::Spread spec{.eachMs = 100, .durationMs = 200};
  Host host(400, 120);
  host.composer.render(
      box().padding(6).child(text(u8"AA BB CC DD", whiteStyle(16))
                                 .key("p")
                                 .width(360)
                                 .fx({.effect = fx::rise(6),
                                      .stagger = spec,
                                      .unit = sigil::weave::Unit::Word})));
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
  const sigil::motion::Spread amount{.amountMs = 700, .durationMs = 420};
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
  sigil::motion::Spread nested{.eachMs = 300, .durationMs = 999};
  nested.then({.eachMs = 40, .durationMs = 100});
  {
    Host host(400, 120);
    host.composer.render(box().padding(6).child(
        text(u8"AB CD", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6),
                 .stagger = nested,
                 .unit = sigil::weave::Unit::Word,
                 .innerUnit = sigil::weave::Unit::Cluster})));
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
  const sigil::motion::Spread cued =
      sigil::motion::Spread{.durationMs = 180}.cues(table);
  {
    Host host(400, 120);
    host.composer.render(
        box().padding(6).child(text(u8"AA BB CC DD", whiteStyle(16))
                                   .key("p")
                                   .width(360)
                                   .fx({.effect = fx::rise(6),
                                        .stagger = cued,
                                        .unit = sigil::weave::Unit::Word})));
    host.frame();
    const float span = host.composer.cascadeSpanMs("p", 0);
    EXPECT_FLOAT_EQ(span, 1360.0f) << "the table's last time plus one beat";
    EXPECT_FLOAT_EQ(cued.spanMs(4), span);
  }
  EXPECT_FLOAT_EQ(sigil::motion::Spread{.durationMs = 100}
                      .cues({0.0f, 900.0f, 300.0f})
                      .spanMs(3),
                  1000.0f)
      << "an out-of-order table spans to its LATEST time, not its last entry";
}

TEST(ComposeTextFx, CascadeSpanMsResolvesZeroRatherThanGuessing) {
  Host host(200, 120);
  host.composer.render(box().padding(6).key("b").child(
      text(u8"AA BB", whiteStyle(16))
          .key("p")
          .fx({.effect = fx::rise(6), .unit = sigil::weave::Unit::Word})));
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
               .stagger = {.eachMs = 100, .durationMs = 200, .loopMs = loopMs},
               .unit = sigil::weave::Unit::Word,
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
  const sigil::motion::Spread looping{
      .eachMs = 100, .durationMs = 200, .loopMs = 400};
  EXPECT_FLOAT_EQ(looping.spanMs(3), 400.0f);

  // A start PAST the period folds mod it — the beat still re-opens once
  // per cycle — while Beat::startMs keeps reporting the authored delay.
  {
    host.composer.render(box().padding(6).child(
        text(u8"AA BB CC", whiteStyle(16))
            .key("p")
            .width(360)
            .fx({.effect = fx::rise(6),
                 .stagger = {.eachMs = 300, .durationMs = 200, .loopMs = 400},
                 .unit = sigil::weave::Unit::Word,
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
  EXPECT_TRUE(
      (sigil::motion::Spread{.eachMs = 100, .durationMs = 200}) ==
      (sigil::motion::Spread{.eachMs = 100, .durationMs = 200, .loopMs = 0}));
  EXPECT_FALSE(
      (sigil::motion::Spread{.eachMs = 100, .durationMs = 200}) ==
      (sigil::motion::Spread{.eachMs = 100, .durationMs = 200, .loopMs = 400}));

  const auto render = [](sigil::motion::Spread cascade) {
    Host host(400, 120);
    host.composer.render(
        box().padding(6).child(text(u8"AA BB CC", whiteStyle(16))
                                   .key("p")
                                   .width(360)
                                   .fx({.effect = fx::rise(6),
                                        .stagger = std::move(cascade),
                                        .unit = sigil::weave::Unit::Word,
                                        .progress = 0.25f})));
    host.frame();
    return std::pair(host.composer.beatsOf("p", 0),
                     surfaceBytes(host, 400, 120));
  };
  const auto [defaulted, defaultedBytes] =
      render({.eachMs = 100, .durationMs = 200});
  const auto [spelled, spelledBytes] =
      render({.eachMs = 100, .durationMs = 200, .loopMs = 0});
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
    return box().padding(10).child(text(u8"AA BB", whiteStyle(28))
                                       .key("p")
                                       .fx({.effect = fx::hold(fx::rise(24)),
                                            .stagger = {.eachMs = 300,
                                                        .durationMs = 100,
                                                        .loopMs = loopMs},
                                            .unit = sigil::weave::Unit::Word,
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
               .stagger = {.eachMs = 100, .durationMs = 200, .loopMs = 400},
               .unit = sigil::weave::Unit::Cluster,
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
               .stagger = {.eachMs = 100, .durationMs = 200},
               .unit = sigil::weave::Unit::Cluster,
               .progress = animate(motion::from(0.0f).to(1.0f),
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
