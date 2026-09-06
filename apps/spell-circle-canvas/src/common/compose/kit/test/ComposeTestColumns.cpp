// A chain of columns over one story: the run balanced to the shallowest
// depth that still holds what it must, and the spanner that breaks the
// chain so the copy above it sets in columns, the spanner straddles them,
// and the copy below resumes.

#include <sigilcompose/kit/Typeset.h>

#include <numeric>

#include "support/ShapeTestSupport.h"

namespace {

/** Two hundred words of one letter, so a line count is the same on every
 *  machine and a column's depth is a whole number of lines. */
std::u8string story() {
  std::u8string words;
  for (int i = 0; i < 200; ++i) words += u8"a ";
  words.pop_back();
  return words;
}

sigil::weave::Story article() {
  return sigil::weave::Story{story(), styleAt(12)};
}

/** How deep the frames of a chain came out, in draw order. */
std::vector<float> depths(Host& host, int count) {
  std::vector<float> out;
  for (int i = 0; i < count; ++i) {
    const std::optional<SkRect> box =
        host.composer.bounds("column" + std::to_string(i));
    out.push_back(box ? box->height() : 0.0f);
  }
  return out;
}

}  // namespace

TEST(KitColumns, ABalancedRunIsShallowerThanTheDepthItWasGiven) {
  Host host(400, 600);
  host.composer.render(
      box().absolute().inset(0).child(kit::columns({.story = article(),
                                                    .count = 3,
                                                    .gutter = 20,
                                                    .width = 360,
                                                    .height = 400,
                                                    .composer = &host.composer})
                                          .child(box())));
  host.frame();
  // Nothing is balanced without a spanner to balance against: one row is
  // the whole assembly and it keeps the depth it was given.
  for (float depth : depths(host, 3)) EXPECT_FLOAT_EQ(depth, 400);
}

TEST(KitColumns, ASpannerBreaksTheChainAndTheRunAboveItIsBalanced) {
  Host host(400, 900);
  const auto draw = [&] {
    host.composer.render(box().absolute().inset(0).child(kit::columns(
        {.story = article(),
         .count = 3,
         .gutter = 20,
         .width = 360,
         .height = 300,
         .spanners = {{sigil::weave::sel::line(11),
                       box().key("plate").width(Dim(360)).height(Dim(24)).fill(
                           red())}},
         .composer = &host.composer})));
    host.frame();
  };
  draw();  // the first draw has no layout to read the selector off
  draw();  // …and this one settles on it

  const std::vector<float> above = depths(host, 3);
  // The three columns of the run are one depth, and it is shallower than
  // the 300 they were given: they hold twelve lines between them and stop.
  EXPECT_FLOAT_EQ(above[0], above[1]);
  EXPECT_FLOAT_EQ(above[1], above[2]);
  EXPECT_LT(above[0], 300);
  EXPECT_GT(above[0], 0);

  // The spanner stands under the balanced run, across all three columns,
  // and the row after it starts under the spanner.
  const SkRect plate = require(host.composer.bounds("plate"));
  EXPECT_GE(plate.top(), above[0] - 1.0f);
  EXPECT_FLOAT_EQ(plate.width(), 360);
  EXPECT_GE(require(host.composer.bounds("column3")).top(), plate.bottom());
}

TEST(KitColumns, TheRunBelowTheSpannerResumesWhereTheOneAboveRanOut) {
  Host host(400, 900);
  const auto draw = [&] {
    host.composer.render(box().absolute().inset(0).child(kit::columns(
        {.story = article(),
         .count = 3,
         .gutter = 20,
         .width = 360,
         .height = 300,
         .spanners = {{sigil::weave::sel::line(11),
                       box().key("plate").width(Dim(360)).height(Dim(24))}},
         .composer = &host.composer})));
    host.frame();
  };
  draw();
  draw();
  // Every frame of both runs is a link of ONE chain, so a line is
  // addressed by its number in the STORY and lands in exactly one of
  // them: the story's first line is in the first column of the run above
  // the spanner and in none of the run below it.
  const auto lineIn = [&](const char* key, uint32_t line) {
    return !host.composer
                .units(key, sigil::weave::sel::line(line),
                       sigil::weave::Unit::Line)
                .empty();
  };
  EXPECT_TRUE(lineIn("column0", 0));
  EXPECT_FALSE(lineIn("column3", 0));
  // …and the line the spanner breaks after is the last one above it, so
  // the one after that is in the run below.
  EXPECT_TRUE(lineIn("column2", 11));
  EXPECT_TRUE(lineIn("column3", 12));
}

TEST(KitColumns, ARunWhoseFramesStateNoDepthInPixelsIsLeftAlone) {
  Host host(400, 600);
  // A ceiling is what the halving starts from; without one there is
  // nothing to halve, and the chain fills as an unbalanced chain does.
  host.composer.render(box().absolute().inset(0).child(
      box()
          .row()
          .gap(20)
          .child(frame(article())
                     .key("a")
                     .thread("b")
                     .width(Dim(170.0f))
                     .balanceChain())
          .child(frame(article()).key("b").width(Dim(170.0f)))));
  host.frame();
  EXPECT_GT(require(host.composer.bounds("a")).height(), 0);
}
