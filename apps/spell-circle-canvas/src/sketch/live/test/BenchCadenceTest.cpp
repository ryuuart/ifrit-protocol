/** @file
 * The cadence a window sweep keeps: what starts the clock, what the
 * measured stretch covers, and what is stood down.
 */

#include <gtest/gtest.h>
#include <sigilsketch/live/BenchCadence.h>

namespace {

using sigil::sketch::BenchCadence;
using Step = BenchCadence::Step;

BenchCadence::Times times() {
  BenchCadence::Times t;
  t.warmupSeconds = 1.0;
  t.measureSeconds = 2.0;
  t.ceilingSeconds = 10.0;
  return t;
}

/** THE CLOCK STARTS AT THE FIRST FRAME OF THE SELECTION, not at the ask.
 *  A session whose first frame costs seconds would otherwise spend its
 *  whole warm-up, and part of the stretch that is measured, before
 *  anything of it was on screen. */
TEST(BenchCadence, WarmupBeginsAtTheFirstPresentedFrame) {
  BenchCadence cadence(times());
  cadence.select(0.0);
  // Five seconds asked for and nothing presented: no warm-up has run.
  for (double now = 0.1; now < 5.0; now += 0.1)
    EXPECT_EQ(cadence.advance(now, false), Step::Wait) << "at " << now;
  EXPECT_EQ(cadence.advance(5.0, true), Step::Wait);
  // The warm-up runs from there, and only then does measuring begin.
  EXPECT_EQ(cadence.advance(5.9, true), Step::Wait);
  EXPECT_EQ(cadence.advance(6.0, true), Step::Begin);
  EXPECT_EQ(cadence.advance(7.9, true), Step::Wait);
  EXPECT_EQ(cadence.advance(8.0, true), Step::Read);
}

/** The measured stretch is the stated one however late the sketch
 *  arrived: a slow opener is measured over as long as a fast one. */
TEST(BenchCadence, MeasuresTheStatedStretchHoweverLateItArrives) {
  BenchCadence cadence(times());
  cadence.select(0.0);
  EXPECT_EQ(cadence.advance(9.0, true), Step::Wait);    // presented at 9
  EXPECT_EQ(cadence.advance(10.0, true), Step::Begin);  // warmed by 10
  EXPECT_EQ(cadence.advance(11.9, true), Step::Wait);
  EXPECT_EQ(cadence.advance(12.0, true), Step::Read);
}

/** A SKETCH THAT NEVER REACHES THE SCREEN IS STOOD DOWN, not read as a
 *  rate: the ceiling is what separates a slow opener from a session the
 *  window never presented. */
TEST(BenchCadence, StandsDownWhatNeverPresents) {
  BenchCadence cadence(times());
  cadence.select(0.0);
  EXPECT_EQ(cadence.advance(9.9, false), Step::Wait);
  EXPECT_EQ(cadence.advance(10.0, false), Step::Skip);
}

/** The ceiling counts the wait for a frame and nothing else: once the
 *  sketch is presenting, a warm-up and a measured stretch that together
 *  outlast the ceiling are not a stand-down. */
TEST(BenchCadence, TheCeilingEndsAtTheFirstFrame) {
  BenchCadence cadence(times());
  cadence.select(0.0);
  EXPECT_EQ(cadence.advance(9.0, true), Step::Wait);
  for (double now = 9.1; now < 10.0; now += 0.1)
    EXPECT_EQ(cadence.advance(now, false), Step::Wait) << "at " << now;
  EXPECT_EQ(cadence.advance(10.1, true), Step::Begin);
}

/** Selecting the next sketch starts the whole cadence over, which is
 *  what makes one sweep a row of independent measurements. */
TEST(BenchCadence, SelectingAgainStartsOver) {
  BenchCadence cadence(times());
  cadence.select(0.0);
  cadence.advance(0.1, true);
  cadence.advance(1.1, true);  // Begin
  cadence.select(2.0);
  EXPECT_EQ(cadence.advance(2.1, false), Step::Wait);
  EXPECT_EQ(cadence.advance(2.2, true), Step::Wait);
  EXPECT_EQ(cadence.advance(3.2, true), Step::Begin);
  EXPECT_EQ(cadence.advance(5.2, true), Step::Read);
  // Read is said once; a step after it asks for nothing further.
  EXPECT_EQ(cadence.advance(9.0, true), Step::Wait);
}

}  // namespace
