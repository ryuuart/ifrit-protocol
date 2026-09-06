/** @file
 * The thumbnail store: when a still on disk is the one a sketch is owed,
 * what a run does with a sketch it cannot draw inside its budget, and
 * that a walk let go answers without finishing.
 */

#include <gtest/gtest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "ScratchDir.h"
#include "support/Fixtures.h"

namespace {

using namespace sigil::sketch;
using namespace sigil::compose;

using sigil::sketch::test::assets;
using sigil::sketch::test::fonts;
using sigil::test::ScratchDir;

/** HOW MANY FRAMES THE WALK ACTUALLY DREW, which is what every case
 *  below reads instead of a clock: a still is a fixed number of frames,
 *  so a run that ended early drew fewer of them and by exactly how many
 *  says where it ended. */
std::atomic_int g_frames{0};
/** Raised by the probe below once it has drawn kStopAfter frames — the
 *  stand-in for a window closing mid-render, and deterministic where a
 *  timer would not be. */
std::atomic_bool g_stop{false};
constexpr int kStopAfter = 5;

/** The rate a still is walked at, and a moment far enough out that the
 *  walk is thousands of frames — long enough that any of the endings
 *  below is separated from a finished walk by more than rounding. */
constexpr double kRate = 60.0;
constexpr double kLateMoment = 40.0;
constexpr int kWholeWalk = (int)(kLateMoment * kRate);

/** A sketch that counts the frames it is walked for and stops itself
 *  partway. */
struct Counting : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(kLateMoment);
  }
  void update(double /*elapsed*/, SketchContext& ctx) override {
    if (g_frames.fetch_add(1) + 1 >= kStopAfter) g_stop.store(true);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({1, 0, 0, 1})));
  }
};

/** The same walk, counted, with nothing that stops it — what a budget
 *  and a plate declaration are read against. */
struct Long : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(kLateMoment);
  }
  void update(double /*elapsed*/, SketchContext& ctx) override {
    g_frames.fetch_add(1);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({0, 1, 0, 1})));
  }
};

/** A sketch that declares itself a plate: judged on the cost of its
 *  still rather than on holding a frame rate, which is the declaration
 *  a fill stands down on. */
struct Plate : Sketch {
  void setup(SketchContext& ctx) override {
    ctx.canvas(48, 32);
    ctx.background({0, 0, 0, 1});
    ctx.captureAt(0.1);
    ctx.plate();
  }
  void update(double /*elapsed*/, SketchContext& ctx) override {
    g_frames.fetch_add(1);
    ctx.composer.render(
        box().width(16).height(16).fill(Fill::color({0, 0, 1, 1})));
  }
};

template <class SketchType>
Entry entryOf(const char* name) {
  return Entry{name, name, "Test", "a thumbnail fixture", &kindOf<SketchType>};
}

ThumbnailRun runInto(const std::filesystem::path& out) {
  ThumbnailRun run;
  run.out = out;
  run.maxDimension = 64;
  return run;
}

/** A file with @p text in it, so a key has something to hash. */
void write(const std::filesystem::path& path, std::string_view text) {
  std::ofstream stream(path);
  stream << text;
}

// ---------------------------------------------------------------------------
// The key, and what makes a still on disk the wrong one

TEST(ThumbnailStore, ASourceThatChangedIsANewKey) {
  const ScratchDir dir("sigil_thumbnail_key");
  const std::filesystem::path source = dir.path / "probe.cpp";
  write(source, "// one");
  const std::string first = thumbnailKey(source);
  EXPECT_FALSE(first.empty());
  EXPECT_EQ(first, thumbnailKey(source)) << "the same file keyed twice";

  write(source, "// one, and a second line that makes it longer");
  EXPECT_NE(first, thumbnailKey(source));
}

TEST(ThumbnailStore, TheKeyIsTheSourceAndNothingElse) {
  // A KEY THAT CAME BACK is the whole claim: the key is a function of the
  // source's own size and time, so a source restored to what it was is
  // owed the still it had. Nothing outside the file — no host binary, no
  // clock — may enter, or a source put back would answer differently.
  const ScratchDir dir("sigil_thumbnail_pure");
  const std::filesystem::path source = dir.path / "probe.cpp";
  write(source, "// one");
  const auto when = std::filesystem::last_write_time(source);
  const std::string first = thumbnailKey(source);

  write(source, "// one, and a second line that makes it longer");
  ASSERT_NE(first, thumbnailKey(source));

  write(source, "// one");
  std::filesystem::last_write_time(source, when);
  EXPECT_EQ(first, thumbnailKey(source))
      << "the source is back, so the still it had is fresh again";
}

TEST(ThumbnailStore, AStillIsFreshOnlyUnderTheKeyItWasWrittenAt) {
  const ScratchDir dir("sigil_thumbnail_fresh");
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "aaaa").empty())
      << "nothing is on disk yet";

  const std::filesystem::path at = thumbnailFile(dir.path, "probe", "aaaa");
  write(at, "not really a png");
  EXPECT_EQ(at, freshThumbnail(dir.path, "probe", "aaaa"));
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "bbbb").empty())
      << "a still under another key is the answer to another question";
}

TEST(ThumbnailStore, ANoteStandsInForAStillUnderTheSameKey) {
  const ScratchDir dir("sigil_thumbnail_note");
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "aaaa").empty());
  ASSERT_TRUE(noteThumbnail(dir.path, "probe", "aaaa", "ran past its budget"));
  EXPECT_EQ("ran past its budget", thumbnailNote(dir.path, "probe", "aaaa"));
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "bbbb").empty())
      << "a note is asked and answered at one key only";
}

TEST(ThumbnailStore, WhatTheKeyNoLongerNamesIsRemoved) {
  const ScratchDir dir("sigil_thumbnail_prune");
  write(thumbnailFile(dir.path, "probe", "aaaa"), "an old still");
  ASSERT_TRUE(noteThumbnail(dir.path, "probe", "aaaa", "an old answer"));
  EXPECT_TRUE(freshThumbnail(dir.path, "probe", "aaaa").empty())
      << "writing the note took the still of the same key with it";

  const std::filesystem::path kept = thumbnailFile(dir.path, "probe", "bbbb");
  write(kept, "the still now");
  pruneThumbnails(dir.path, "probe", kept);
  EXPECT_EQ(kept, freshThumbnail(dir.path, "probe", "bbbb"));
  EXPECT_TRUE(thumbnailNote(dir.path, "probe", "aaaa").empty())
      << "the note of a spent key went with its still";
}

// ---------------------------------------------------------------------------
// What one render does with a sketch it cannot draw in the time it has

TEST(ThumbnailRender, AWalkLetGoAnswersWithoutFinishingIt) {
  const ScratchDir dir("sigil_thumbnail_stopped");
  g_frames.store(0);
  g_stop.store(false);
  const Entry entry = entryOf<Counting>("thumbnail_counting");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "counting", "aaaa"));
  run.budget = std::chrono::milliseconds::zero();  // only the stop ends it
  run.stop = &g_stop;

  EXPECT_EQ(ThumbnailOutcome::Stopped,
            renderThumbnail(entry, fonts(), assets(), run));
  // The stop is read between frames, so the frame that raised it is the
  // last one drawn — and every one of the thousands after it is not.
  EXPECT_EQ(kStopAfter, g_frames.load());
  EXPECT_LT(g_frames.load(), kWholeWalk);
  EXPECT_FALSE(std::filesystem::exists(run.out))
      << "a walk let go leaves no still behind";
}

TEST(ThumbnailRender, AWalkPastItsBudgetIsAbandoned) {
  const ScratchDir dir("sigil_thumbnail_budget");
  g_frames.store(0);
  const Entry entry = entryOf<Long>("thumbnail_long");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "long", "aaaa"));
  // The smallest budget there is: whatever this machine's speed, one
  // frame of anything outlasts it, and what is asserted is that the walk
  // ended far short of its whole rather than when.
  run.budget = std::chrono::milliseconds(1);

  EXPECT_EQ(ThumbnailOutcome::OverBudget,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_LT(g_frames.load(), kWholeWalk);
  EXPECT_FALSE(std::filesystem::exists(run.out));
}

TEST(ThumbnailRender, ASketchThatDeclaredItselfAPlateIsNotWalked) {
  const ScratchDir dir("sigil_thumbnail_heavy");
  g_frames.store(0);
  const Entry entry = entryOf<Plate>("thumbnail_plate");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "plate", "aaaa"));

  EXPECT_EQ(ThumbnailOutcome::Heavy,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_EQ(0, g_frames.load()) << "setup ran; the walk did not";
  EXPECT_FALSE(std::filesystem::exists(run.out));

  // …and a run asked for the heavy ones draws it like any other.
  run.heavy = true;
  EXPECT_EQ(ThumbnailOutcome::Wrote,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_TRUE(std::filesystem::exists(run.out));
}

TEST(ThumbnailRender, AWalkInsideItsBudgetWritesTheStill) {
  const ScratchDir dir("sigil_thumbnail_wrote");
  g_frames.store(0);
  const Entry entry = entryOf<Plate>("thumbnail_plate_again");
  ThumbnailRun run = runInto(thumbnailFile(dir.path, "plate", "cccc"));
  run.heavy = true;
  run.budget = std::chrono::milliseconds::zero();

  EXPECT_EQ(ThumbnailOutcome::Wrote,
            renderThumbnail(entry, fonts(), assets(), run));
  EXPECT_GT(g_frames.load(), 0);
  EXPECT_TRUE(std::filesystem::exists(run.out));
  EXPECT_TRUE(thumbnailNote(dir.path, "plate", "cccc").empty())
      << "a still that landed leaves nothing standing in for it";
}

}  // namespace
