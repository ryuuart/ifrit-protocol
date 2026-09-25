/** @file
 * What runs on the engine: an animation on a live value — to completion,
 * taking a value over, blending a retarget so the velocity carries, read
 * by every copy and every bound value the frame it moves; a timeline's
 * positions, its calls and its playback; a timer every frame, at an exact
 * step rate counted from total time, and throttled; the playback verbs
 * every one of them answers to; and the order a frame steps them in. Every
 * frame is a stated one, so no wall clock is read.
 */

#include "support/StandsAlone.h"

#include <gtest/gtest.h>
#include <sigilmotion/bind/Binding.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Tween.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace {

/** Moves @p engine one stated frame of @p step past where it stands. */
Duration stepBy(Engine& engine, Duration step) {
  return engine.advance(engine.elapsed() + step);
}

/** A linear tween to @p to over a second — the motion whose value halfway
 *  through a test can name without evaluating a curve. */
Tween<float> linearTo(float to, Duration length = 1s) {
  return {.to = to, .duration = length, .ease = ease::linear};
}

}  // namespace

// ---------------------------------------------------------------------------
// An animation on a live value.

TEST(Engine, DrivesAnAnimationToCompletionAndThenSettles) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  const Animation animation = engine.animate(value, linearTo(10.0f));

  EXPECT_TRUE(engine.isRunning());
  EXPECT_TRUE(animation.isRunning());
  engine.advance(500ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
  EXPECT_TRUE(engine.isRunning());

  engine.advance(1100ms);  // past the end
  EXPECT_FLOAT_EQ(value.value(), 10.0f);
  EXPECT_TRUE(animation.isCompleted());
  EXPECT_FALSE(engine.isRunning());  // a finished motion leaves the engine
}

TEST(Engine, AnimatingAConstantMakesItLiveFromTheValueItHolds) {
  Engine engine;
  Animatable<float> glow = 0.25f;
  engine.animate(glow, linearTo(1.25f));
  EXPECT_EQ(glow.form(), Animatable<float>::Form::Live);
  EXPECT_FLOAT_EQ(glow.value(), 0.25f);
  engine.advance(500ms);
  EXPECT_NEAR(glow.value(), 0.75f, 1e-4f);
}

TEST(Engine, ANamedFromIsWhereTheValueStandsTheMomentTheAnimationStarts) {
  Engine engine;
  Animatable<float> value = animatable(3.0f);
  engine.animate(value, {.from = 5.0f,
                         .to = 10.0f,
                         .duration = 1s,
                         .delay = 500ms,
                         .ease = ease::linear});
  EXPECT_FLOAT_EQ(value.value(), 5.0f);
  engine.advance(500ms);
  EXPECT_FLOAT_EQ(value.value(), 5.0f);  // the delay holds `from`
  engine.advance(1s);
  EXPECT_NEAR(value.value(), 7.5f, 1e-4f);
}

TEST(Engine, AnAnimationReadsTheSameNumberTheTweenReadsWithNoEngine) {
  // One tween, two readers: the engine stepping it frame by frame and
  // `Tween::at` asked at the same time. Keyframes with and without
  // durations, a delay, passes and an alternating pass all agree.
  const Tween<float> path{.from = 0.0f,
                          .keyframes = {{.to = 10.0f, .duration = 200ms},
                                        {.to = 4.0f}},
                          .duration = 1s,
                          .delay = 100ms,
                          .ease = ease::linear,
                          .loop = 1,
                          .alternate = true};
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  engine.animate(value, path);
  for (int frame = 1; frame <= 90; ++frame) {
    const Duration now = Duration(frame / 60.0);
    engine.advance(now);
    ASSERT_NEAR(value.value(), path.at(now), 1e-4f) << "at " << now.count();
  }
  // Past the last pass it rests where that pass ended — back at the start.
  engine.advance(2s);
  EXPECT_FALSE(engine.isRunning());
  EXPECT_FLOAT_EQ(value.value(), 0.0f);
}

TEST(Engine, EveryCopyOfTheValueReadsTheAnimationsNumber) {
  // A live value is shared: a copy made before the animation started, and
  // a copy made after, read the number the engine writes.
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  const Animatable<float> before = value;
  engine.animate(value, linearTo(10.0f));
  const Animatable<float> after = value;
  engine.advance(250ms);
  EXPECT_NEAR(before.value(), 2.5f, 1e-4f);
  EXPECT_NEAR(after.value(), 2.5f, 1e-4f);
}

TEST(Engine, ABoundValueIsNeverAFrameLate) {
  // A derived value is `bind(source, stages)` read when it is read, so it
  // has no stepping order to get wrong: after any frame it reads the
  // number the source holds on that frame, at any depth of derivation.
  Engine engine;
  Animatable<float> ramp = animatable(0.0f);
  const Animatable<float> shadow = bind(ramp, {.to = {0.0f, 2.0f}});
  const Animatable<float> trail =
      bind(bind(ramp, {.from = {0.25f, 1.25f}, .clampFrom = true}),
           {.to = {0.0f, 240.0f}});
  engine.animate(ramp, linearTo(1.0f));
  engine.advance(500ms);
  EXPECT_NEAR(ramp.value(), 0.5f, 1e-4f);
  EXPECT_FLOAT_EQ(shadow.value(), ramp.value() * 2.0f);
  EXPECT_NEAR(trail.value(), 60.0f, 1e-3f);

  // It holds nothing on the engine: when the source stops moving the
  // engine settles, and a host that writes the source by hand is read
  // without a frame at all.
  engine.advance(1100ms);
  EXPECT_FALSE(engine.isRunning());
  ramp = 0.5f;
  EXPECT_FLOAT_EQ(shadow.value(), 1.0f);
}

TEST(Engine, AnAnimationKeepsWritingWhateverStillReadsItsValue) {
  // The engine runs an animation whether or not a handle or the value it
  // was started on is kept: a bound value that follows it keeps moving
  // after the sketch lets go of the value itself.
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  const Animatable<float> pixels = bind(value, {.to = {0.0f, 100.0f}});
  engine.animate(value, linearTo(1.0f));
  value = Animatable<float>{};
  engine.advance(250ms);
  EXPECT_NEAR(pixels.value(), 25.0f, 1e-3f);
  EXPECT_TRUE(engine.isRunning());
  engine.advance(1s);
  EXPECT_NEAR(pixels.value(), 100.0f, 1e-3f);
}

TEST(Engine, ASecondAnimationOnTheSameValueTakesItOverFromWhereItStands) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  const Animation first = engine.animate(value, linearTo(10.0f));
  engine.advance(500ms);
  ASSERT_NEAR(value.value(), 5.0f, 1e-4f);
  const Animation second = engine.animate(value, linearTo(0.0f));
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);  // no jump
  stepBy(engine, 500ms);
  EXPECT_NEAR(value.value(), 2.5f, 1e-4f);
  EXPECT_FALSE(first.isRunning());
  EXPECT_TRUE(second.isRunning());
}

TEST(Engine, ABlendedRetargetCarriesTheVelocityWhereAReplacedOneStartsAtRest) {
  // Both retargets run the same curve — one that starts at rest — from
  // the same moment of the same flight: the value halfway up a linear
  // climb, moving up at 100 a second.
  const auto retargeted = [](Composition composition) {
    Engine engine;
    Animatable<float> value = animatable(0.0f);
    engine.animate(value, linearTo(100.0f));
    engine.advance(500ms);
    engine.animate(value, {.to = 0.0f,
                           .duration = 1s,
                           .ease = ease::inQuad,
                           .composition = composition});
    const float before = value.value();
    stepBy(engine, 10ms);
    const float moved = value.value() - before;
    engine.advance(3s);
    return std::vector<float>{before, moved, value.value()};
  };

  const std::vector<float> blended = retargeted(Composition::Blend);
  const std::vector<float> replaced = retargeted(Composition::Replace);
  EXPECT_NEAR(blended[0], 50.0f, 1e-3f);
  EXPECT_NEAR(replaced[0], 50.0f, 1e-3f);
  // Blended, the value keeps moving the way it was going right after the
  // retarget — about as far as a hundred a second carries it in 10ms.
  EXPECT_GT(blended[1], 0.9f);
  // Replaced, it starts again from rest.
  EXPECT_LT(std::abs(replaced[1]), 0.01f);
  // Both land on the new target.
  EXPECT_NEAR(blended[2], 0.0f, 1e-3f);
  EXPECT_NEAR(replaced[2], 0.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// The playback verbs, on an animation.

TEST(Engine, AnAnimationPausesResumesSeeksReversesAndCompletes) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  int completions = 0;
  animation.onComplete([&completions] { ++completions; });

  engine.advance(250ms);
  EXPECT_NEAR(value.value(), 2.5f, 1e-4f);

  animation.pause();
  EXPECT_TRUE(animation.isPaused());
  EXPECT_FALSE(animation.isRunning());
  EXPECT_FALSE(engine.isRunning());  // a paused playback is not declared to move
  stepBy(engine, 250ms);
  EXPECT_NEAR(value.value(), 2.5f, 1e-4f);

  animation.resume();
  stepBy(engine, 250ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);

  // A seek shows the time at once.
  animation.seek(750ms);
  EXPECT_NEAR(value.value(), 7.5f, 1e-4f);
  EXPECT_NEAR(animation.currentTime().count(), 0.75, 1e-9);
  EXPECT_NEAR(animation.progress(), 0.75f, 1e-6f);

  animation.reverse();
  stepBy(engine, 250ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
  animation.reverse();
  stepBy(engine, 100ms);
  EXPECT_NEAR(value.value(), 6.0f, 1e-4f);

  animation.complete();
  EXPECT_FLOAT_EQ(value.value(), 10.0f);
  EXPECT_TRUE(animation.isCompleted());
  EXPECT_EQ(completions, 1);
  stepBy(engine, 250ms);
  EXPECT_EQ(completions, 1);  // called once
}

TEST(Engine, ACancelledAnimationLeavesTheValueWhereItStands) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  engine.advance(500ms);
  animation.cancel();
  EXPECT_FALSE(animation.isRunning());
  stepBy(engine, 250ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
  EXPECT_FALSE(engine.isRunning());
}

TEST(Engine, ARevertedAnimationPutsTheValueBackWhereItStarted) {
  Engine engine;
  Animatable<float> value = animatable(2.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  engine.advance(500ms);
  EXPECT_NEAR(value.value(), 6.0f, 1e-4f);
  animation.revert();
  EXPECT_FLOAT_EQ(value.value(), 2.0f);
  EXPECT_FALSE(animation.isRunning());
  stepBy(engine, 250ms);
  EXPECT_FLOAT_EQ(value.value(), 2.0f);
}

TEST(Engine, ARestartedAnimationRunsAgainFromItsStart) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  engine.advance(500ms);
  animation.restart();
  EXPECT_FLOAT_EQ(value.value(), 0.0f);
  stepBy(engine, 500ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
}

TEST(Engine, ACompletedAnimationPlayedAgainRunsAgain) {
  // `play()` on a completed playback starts it again, and `restart()` is
  // back to the start, running — after the engine has finished it as
  // much as before.
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  engine.advance(2s);
  ASSERT_TRUE(animation.isCompleted());
  animation.restart();
  EXPECT_TRUE(animation.isRunning());
  EXPECT_TRUE(engine.isRunning());
  stepBy(engine, 500ms);
  EXPECT_NEAR(value.value(), 5.0f, 1e-4f);
}

TEST(Engine, AReversedPlaybackCompletesWhenToldTo) {
  // `complete()` jumps to the end and completes, whichever way the
  // playback is running.
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Animation animation = engine.animate(value, linearTo(10.0f));
  engine.advance(500ms);
  animation.reverse();
  animation.complete();
  EXPECT_TRUE(animation.isCompleted());
}

TEST(Engine, TheAlternateVerbFlipsEveryOtherPass) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Tween<float> twice = linearTo(10.0f);
  twice.loop = 1;
  engine.animate(value, twice).alternate();
  engine.advance(1250ms);
  EXPECT_NEAR(value.value(), 7.5f, 1e-4f);  // the second pass runs back
}

// ---------------------------------------------------------------------------
// A timeline.

TEST(Engine, ATimelinePlacesEachItemWhereItsPositionSays) {
  Engine engine;
  Animatable<float> first = animatable(0.0f), after = animatable(0.0f),
                    beside = animatable(0.0f), labelled = animatable(0.0f),
                    early = animatable(0.0f), last = animatable(0.0f);
  const Timeline timeline =
      engine.timeline()
          .add(first, linearTo(1.0f))                       // afterEnd: 0
          .add(after, linearTo(1.0f), afterPrevious(500ms))  // 1.5s
          .add(beside, linearTo(1.0f), withPrevious(250ms))  // 1.75s
          .label("late", at(4s))
          .add(labelled, linearTo(1.0f), atLabel("late"))  // 4s
          .add(early, linearTo(1.0f), at(250ms))           // 250ms
          .add(last, linearTo(1.0f));                      // after the end: 5s

  engine.advance(1s);
  EXPECT_FLOAT_EQ(first.value(), 1.0f);
  EXPECT_NEAR(early.value(), 0.75f, 1e-4f);
  EXPECT_FLOAT_EQ(after.value(), 0.0f);  // not yet started: untouched

  engine.advance(2s);
  EXPECT_NEAR(after.value(), 0.5f, 1e-4f);
  EXPECT_NEAR(beside.value(), 0.25f, 1e-4f);

  engine.advance(4500ms);
  EXPECT_NEAR(labelled.value(), 0.5f, 1e-4f);
  EXPECT_FLOAT_EQ(last.value(), 0.0f);

  engine.advance(5500ms);
  EXPECT_NEAR(last.value(), 0.5f, 1e-4f);
  EXPECT_TRUE(timeline.isRunning());

  engine.advance(6s);
  EXPECT_FLOAT_EQ(last.value(), 1.0f);
  EXPECT_TRUE(timeline.isCompleted());
  EXPECT_FALSE(engine.isRunning());
}

TEST(Engine, AnItemWithNoFromStartsFromWhereItsTargetStandsWhenItStarts) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  engine.timeline()
      .add(value, linearTo(1.0f))
      .add(value, linearTo(0.0f));
  engine.advance(1500ms);
  EXPECT_NEAR(value.value(), 0.5f, 1e-4f);
  engine.advance(2s);
  EXPECT_FLOAT_EQ(value.value(), 0.0f);
}

TEST(Engine, ATimelineCallFiresOnceGoingForwardsAndNeverGoingBack) {
  Engine engine;
  Animatable<float> filler = animatable(0.0f);
  int calls = 0;
  Timeline timeline = engine.timeline()
                          .add(filler, linearTo(1.0f, 2s))
                          .call([&calls] { ++calls; }, at(500ms));
  engine.advance(250ms);
  EXPECT_EQ(calls, 0);
  engine.advance(750ms);
  EXPECT_EQ(calls, 1);
  engine.advance(1s);
  EXPECT_EQ(calls, 1);

  // Backwards over it fires nothing…
  timeline.reverse();
  stepBy(engine, 750ms);
  EXPECT_EQ(calls, 1);
  // …and forwards over it again fires it again, once.
  timeline.reverse();
  stepBy(engine, 500ms);
  EXPECT_EQ(calls, 2);
  stepBy(engine, 100ms);
  EXPECT_EQ(calls, 2);

  // A seek back before it and a frame forward past it is a pass forwards.
  timeline.seek(250ms);
  EXPECT_EQ(calls, 2);
  stepBy(engine, 500ms);
  EXPECT_EQ(calls, 3);
}

TEST(Engine, ATimelinePausesSeeksCompletesAndReverts) {
  Engine engine;
  Animatable<float> first = animatable(0.2f), second = animatable(0.0f);
  Timeline timeline = engine.timeline()
                          .add(first, linearTo(1.2f))
                          .add(second, linearTo(1.0f));
  int completions = 0;
  timeline.onComplete([&completions] { ++completions; });

  engine.advance(500ms);
  EXPECT_NEAR(first.value(), 0.7f, 1e-4f);
  timeline.pause();
  stepBy(engine, 500ms);
  EXPECT_NEAR(first.value(), 0.7f, 1e-4f);
  timeline.resume();

  timeline.seek(1500ms);
  EXPECT_FLOAT_EQ(first.value(), 1.2f);
  EXPECT_NEAR(second.value(), 0.5f, 1e-4f);
  EXPECT_NEAR(timeline.progress(), 0.75f, 1e-6f);

  // Revert puts every target back where it was before the timeline began.
  Timeline reverted = timeline;
  reverted.revert();
  EXPECT_FLOAT_EQ(first.value(), 0.2f);
  EXPECT_FLOAT_EQ(second.value(), 0.0f);
  EXPECT_FALSE(reverted.isRunning());
  EXPECT_EQ(completions, 0);

  // A fresh one completes on the spot when told to, and says so once.
  Animatable<float> third = animatable(0.0f);
  Timeline finished = engine.timeline().add(third, linearTo(1.0f));
  int finishedCount = 0;
  finished.onComplete([&finishedCount] { ++finishedCount; });
  finished.complete();
  EXPECT_FLOAT_EQ(third.value(), 1.0f);
  EXPECT_TRUE(finished.isCompleted());
  stepBy(engine, 1s);
  EXPECT_EQ(finishedCount, 1);
}

TEST(Engine, ACancelledTimelineStopsEveryItemWhereItStands) {
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  Timeline timeline = engine.timeline().add(value, linearTo(1.0f));
  engine.advance(250ms);
  timeline.cancel();
  stepBy(engine, 500ms);
  EXPECT_NEAR(value.value(), 0.25f, 1e-4f);
  EXPECT_FALSE(engine.isRunning());
}

// ---------------------------------------------------------------------------
// A timer.

TEST(Engine, ATimerNamesOnlyWhatItReadsAndAVoidOneKeepsRunning) {
  // The delta and the time since the timer started are both offered, and
  // every prefix of them is a callback: neither, the delta, or both. A
  // callback that ANSWERS NOTHING says nothing about being finished, so it
  // runs until it is cancelled and holds the engine running for as long.
  Engine engine;
  int bare = 0;
  Duration summed{}, seen{};
  engine.timer([&bare] { ++bare; });
  engine.timer([&summed](Duration delta) { summed += delta; });
  const Timer both =
      engine.timer([&seen](Duration, Duration elapsed) { seen = elapsed; });

  stepBy(engine, 250ms);
  stepBy(engine, 250ms);
  EXPECT_EQ(bare, 2);
  EXPECT_EQ(summed, Duration(0.5));
  EXPECT_EQ(seen, Duration(0.5));
  EXPECT_TRUE(engine.isRunning()) << "a void timer never retires itself";
  EXPECT_TRUE(both.isRunning());
}

TEST(Engine, ATimerRunsUntilItAnswersFalseAndIsThenDropped) {
  Engine engine;
  Duration accumulated{};
  const Timer timer = engine.timer([&accumulated](Duration delta) {
    accumulated += delta;
    return accumulated < 1s;
  });
  stepBy(engine, 400ms);
  stepBy(engine, 400ms);
  EXPECT_TRUE(engine.isRunning());
  stepBy(engine, 400ms);  // crossed a second: retired
  EXPECT_FALSE(engine.isRunning());
  EXPECT_FALSE(timer.isRunning());
}

TEST(Engine, ATimerWaitsOutItsDelayAndStopsAtTheEndOfItsDuration) {
  Engine engine;
  std::vector<Duration> deltas;
  int completions = 0;
  Timer timer = engine.timer(
      [&deltas](Duration delta) { deltas.push_back(delta); },
      {.duration = 1s, .delay = 500ms});
  timer.onComplete([&completions] { ++completions; });
  engine.advance(250ms);
  EXPECT_TRUE(deltas.empty());
  engine.advance(750ms);
  // The first update covers only the stretch past the delay.
  ASSERT_EQ(deltas.size(), 1u);
  EXPECT_NEAR(deltas[0].count(), 0.25, 1e-9);
  engine.advance(1600ms);
  EXPECT_TRUE(timer.isCompleted());
  EXPECT_EQ(completions, 1);
  EXPECT_FALSE(engine.isRunning());
}

TEST(Engine, APausedTimerIsNotCalledUntilItResumes) {
  Engine engine;
  int calls = 0;
  Timer timer = engine.timer([&calls] { ++calls; });
  stepBy(engine, 100ms);
  timer.pause();
  stepBy(engine, 100ms);
  EXPECT_EQ(calls, 1);
  timer.resume();
  stepBy(engine, 100ms);
  EXPECT_EQ(calls, 2);
  timer.cancel();
  stepBy(engine, 100ms);
  EXPECT_EQ(calls, 2);
  EXPECT_FALSE(engine.isRunning());
}

TEST(Engine, MotionsAreSteppedBeforeTimersWhateverOrderTheyStarted) {
  // A timer reading a moving value reads THIS frame's number, even when
  // the timer was registered before the animation that moves the value.
  Engine engine;
  Animatable<float> value = animatable(0.0f);
  const Animatable<float> doubled = bind(value, {.to = {0.0f, 2.0f}});
  float seen = -1.0f, seenDoubled = -1.0f;
  engine.timer([&] {
    seen = value.value();
    seenDoubled = doubled.value();
  });
  engine.animate(value, linearTo(1.0f));
  engine.advance(500ms);
  EXPECT_NEAR(seen, 0.5f, 1e-4f);
  EXPECT_NEAR(seenDoubled, 1.0f, 1e-4f);
}

TEST(Engine, AWiggleMovesOnlyWhenTheEngineAdvancesTheValueItReads) {
  // A camera shake driven by the clock rather than by a phase the caller
  // steps: the seconds value rides the engine, and the rig reads it. The
  // rig sits at REST — its output range is the one point 0 — so what moves
  // the property is only the noise, and the only thing that moves the
  // noise is the engine advancing the seconds. What the shake is BOUNDED
  // by is the binding's own claim and is asked of it directly in the bind
  // tests; what is asked here is that the clock is what drives it.
  Engine engine;
  Animatable<float> seconds = animatable(0.0f);
  engine.animate(seconds, linearTo(2.0f, 2s));  // one to one

  const Animatable<float> shakeAcross = bind(
      seconds,
      {.to = {0.0f, 0.0f}, .wiggle = {.amount = 12.0f, .frequency = 7.0f, .seed = 1}});
  const Animatable<float> shakeDown = bind(
      seconds,
      {.to = {0.0f, 0.0f}, .wiggle = {.amount = 12.0f, .frequency = 7.0f, .seed = 2}});

  const float atRest = shakeAcross.value();
  EXPECT_FLOAT_EQ(shakeAcross.value(), atRest)
      << "the rig read time for itself rather than the value it follows";

  std::vector<float> across, down;
  for (int frame = 0; frame < 120; ++frame) {
    stepBy(engine, 1s / 60.0);
    across.push_back(shakeAcross.value());
    down.push_back(shakeDown.value());
  }
  EXPECT_NE(std::count(across.begin(), across.end(), atRest), (long)across.size())
      << "the rig never moved as the engine ran";
  EXPECT_NE(across, down) << "two seeds on one clock moved together";

  // A frame of no length advances nothing, so the rig holds where it was.
  const float held = across.back();
  stepBy(engine, 0s);
  EXPECT_FLOAT_EQ(shakeAcross.value(), held);
}

// ---------------------------------------------------------------------------
// A step-rate timer: a callback that advances at its own rate whatever the
// host draws at, which is what anything simulation-shaped needs before it
// will look the same on two machines.

namespace {

/** A step-rate timer that only counts, and the count. */
struct StepRun {
  Engine engine;
  int steps = 0;
  Timer timer;

  explicit StepRun(double stepRate, int catchUp = 8) {
    timer = engine.timer(
        [this] {
          ++steps;
          return true;
        },
        {.stepRate = stepRate, .catchUp = catchUp});
  }

  /** Draws @p seconds worth of frames at @p drawRate and answers the
   *  count. */
  int over(double seconds, double drawRate) {
    const int frames = (int)std::lround(seconds * drawRate);
    for (int frame = 1; frame <= frames; ++frame)
      engine.advance(Duration(seconds * frame / (double)frames));
    return steps;
  }
};

}  // namespace

TEST(Engine, AStepRateRunsAtItsOwnRateWhateverRateTheHostDrawsAt) {
  for (double drawRate : {24.0, 60.0, 144.0}) {
    StepRun run(27.0);
    EXPECT_EQ(run.over(1.0, drawRate), 27) << "drawing at " << drawRate;
  }
}

TEST(Engine, AStepRateCountsFromTotalTimeSoOneMomentIsAlwaysTheSameStep) {
  // The count is `floor(total * rate)` and not a running sum, so the same
  // simulated instant lands on the same side of a step boundary at every
  // draw rate. A sum compared against a step size slips by one comparison
  // over a long pre-roll, and only at some rates.
  StepRun reference(60.0, 64);
  const int expected = reference.over(3.1, 60.0);
  for (double drawRate : {10.0, 15.0, 20.0, 30.0, 120.0}) {
    StepRun run(60.0, 64);
    EXPECT_EQ(run.over(3.1, drawRate), expected) << "drawing at " << drawRate;
  }
}

TEST(Engine, AStepRateDropsItsBacklogRatherThanRunningItAndSaysSoWhenItDid) {
  // A hitch longer than one step would otherwise make the next frame run
  // the backlog, which takes longer still. Dropping simulated time is the
  // correct failure, and the flag is the only signal that anything
  // measured on that frame is meaningless.
  StepRun run(60.0, /*catchUp=*/4);
  run.engine.advance(10s);  // ten seconds in one frame: 600 steps of backlog
  EXPECT_EQ(run.steps, 4);
  EXPECT_EQ(run.timer.stepsThisFrame(), 4);
  EXPECT_TRUE(run.timer.droppedTime());

  // The dropped time is gone rather than carried into the next frame.
  run.steps = 0;
  stepBy(run.engine, 1s / 60.0);
  EXPECT_EQ(run.steps, 1);
  EXPECT_FALSE(run.timer.droppedTime());
}

TEST(Engine, AStepRateTimerThatAnswersFalseIsDroppedLikeAnyOther) {
  Engine engine;
  int steps = 0;
  engine.timer([&steps] { return ++steps < 3; }, {.stepRate = 60.0});
  engine.advance(1s);
  EXPECT_EQ(steps, 3);
  stepBy(engine, 1s);
  EXPECT_EQ(steps, 3);
  EXPECT_FALSE(engine.isRunning());
}

TEST(Engine, AStepRatePublishesHowFarThroughAStepTheFrameEnded) {
  // A fixed-rate simulation drawn straight from its own state judders
  // whenever the draw rate is not a multiple of its own; the leftover
  // fraction is what `lerp(previous, current, betweenSteps)` needs.
  StepRun run(10.0);

  run.engine.advance(50ms);  // half a step in: none taken, and it says so
  EXPECT_EQ(run.steps, 0);
  EXPECT_NEAR(run.timer.betweenSteps(), 0.5f, 1e-4f);

  run.engine.advance(120ms);  // across the boundary: one step, the rest over
  EXPECT_EQ(run.steps, 1);
  EXPECT_NEAR(run.timer.betweenSteps(), 0.2f, 1e-4f);

  run.engine.advance(200ms);  // landing on a boundary leaves nothing over
  EXPECT_EQ(run.steps, 2);
  EXPECT_NEAR(run.timer.betweenSteps(), 0.0f, 1e-4f);
}

TEST(Engine, AFrameRateThrottlesATimerToAtMostThatManyUpdates) {
  // Frames of a sixty-fourth of a second, throttled to eight updates a
  // second: every eighth frame updates, handed the time since the last.
  Engine engine;
  std::vector<Duration> deltas;
  engine.timer([&deltas](Duration delta) { deltas.push_back(delta); },
               {.frameRate = 8.0});
  for (int frame = 1; frame <= 64; ++frame)
    engine.advance(Duration(frame / 64.0));
  ASSERT_EQ(deltas.size(), 8u);
  for (const Duration delta : deltas) EXPECT_EQ(delta, 125ms);

  // Drawing slower than the throttle, every frame updates.
  Engine slow;
  int updates = 0;
  slow.timer([&updates] { ++updates; }, {.frameRate = 100.0});
  for (int frame = 1; frame <= 64; ++frame) slow.advance(Duration(frame / 64.0));
  EXPECT_EQ(updates, 64);
}
