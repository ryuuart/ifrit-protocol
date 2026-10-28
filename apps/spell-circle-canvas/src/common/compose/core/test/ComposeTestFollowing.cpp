// A memo follows the live values its describe reads: one that changed is
// described again at the next draw, alone among its siblings, once however
// many changes landed, and in the frame the change precedes; a peek takes
// the number without following it; a describe that writes what it read is
// refused, reported once, and does not describe again. A placement query
// asked inside a memo — a track's beats — follows the node's arrival, its
// layout and the track's progress.

#include <sigilcompose/typography/Typography.h>
#include <sigilcore/report/Report.h>
#include <sigilmotion/values/Animatable.h>

#include <string>
#include <string_view>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

/** Every report said while it lives. */
struct HeardReports {
  std::vector<std::string> lines;
  sigil::core::Reporter previous;
  HeardReports() {
    previous = sigil::core::installReporter(
        [this](std::string_view, std::string_view message) {
          lines.emplace_back(message);
        });
  }
  ~HeardReports() { sigil::core::installReporter(previous); }
  size_t refusals() const {
    size_t count = 0;
    for (const std::string& line : lines)
      if (line.find("wrote a live value it had read") != std::string::npos)
        ++count;
    return count;
  }
};

/** A row of two memos: one whose describe reads @p counter, as its width,
 *  and a sibling that reads nothing live. Each counts its describes. */
struct CounterRow {
  motion::Animatable<float> counter = motion::animatable(0.0f);
  int counterDescribes = 0;
  int siblingDescribes = 0;

  Element describe() {
    return box().row().children({memo(0,
                                      [this](int) {
                                        ++counterDescribes;
                                        return box()
                                            .width(20 + counter.value() * 10)
                                            .height(20)
                                            .fill(red());
                                      })
                                     .key("counter"),
                                 memo(0, [this](int) {
                                   ++siblingDescribes;
                                   return box().width(20).height(20).fill(
                                       green());
                                 }).key("sibling")});
  }
};

}  // namespace

TEST(ComposeFollowing, AChangedCounterDescribesItsMemoAgainAndNotItsSibling) {
  Host host;
  CounterRow row;
  host.composer.render(row.describe());
  host.frame();
  EXPECT_EQ(row.counterDescribes, 1);
  EXPECT_EQ(row.siblingDescribes, 1);
  row.counter = 3.0f;
  host.frame();
  EXPECT_EQ(row.counterDescribes, 2);
  EXPECT_EQ(row.siblingDescribes, 1);
  // The sibling moved over by the width the counter describes.
  EXPECT_FLOAT_EQ(require(host.composer.bounds("sibling")).fLeft, 50.0f);
}

TEST(ComposeFollowing, NothingIsDescribedAgainWhenNothingChanged) {
  Host host;
  CounterRow row;
  host.composer.render(row.describe());
  host.frame();
  host.frame();
  row.counter = 0.0f;  // the number it holds
  host.frame();
  EXPECT_EQ(row.counterDescribes, 1);
  EXPECT_EQ(row.siblingDescribes, 1);
  EXPECT_FALSE(host.composer.isRunning());
}

TEST(ComposeFollowing, TwoChangesBeforeOneDrawDescribeOnce) {
  Host host;
  CounterRow row;
  host.composer.render(row.describe());
  host.frame();
  row.counter = 1.0f;
  EXPECT_TRUE(host.composer.isRunning()) << "a followed change asks for a draw";
  row.counter = 2.0f;
  host.frame();
  EXPECT_EQ(row.counterDescribes, 2);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("sibling")).fLeft, 40.0f);
}

TEST(ComposeFollowing, TheDescribeLandsInTheFrameTheChangePrecedes) {
  Host host;
  motion::Animatable<float> lit = motion::animatable(0.0f);
  host.composer.render(memo(0, [&lit](int) {
                         return box().width(40).height(40).fill(
                             lit.value() > 0.5f ? green() : red());
                       }).key("swatch"));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);
  lit = 1.0f;
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
}

TEST(ComposeFollowing, APeekTakesTheNumberWithoutFollowingIt) {
  Host host;
  motion::Animatable<float> counter = motion::animatable(0.0f);
  int describes = 0;
  host.composer.render(
      memo(0, [&](int) {
        ++describes;
        return box().width(20 + counter.peek() * 10).height(20).fill(red());
      }).key("peeked"));
  host.frame();
  counter = 4.0f;
  host.frame();
  host.frame();
  EXPECT_EQ(describes, 1);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("peeked")).width(), 20.0f);
}

TEST(ComposeFollowing, AMemoWritingWhatItReadIsReportedOnceAndDoesNotLoop) {
  HeardReports heard;
  Host host;
  motion::Animatable<float> counter = motion::animatable(0.0f);
  int describes = 0;
  host.composer.render(memo(0, [&](int) {
                         ++describes;
                         const float read = counter.value();
                         counter = read + 1.0f;
                         return box().width(20 + read).height(20).fill(red());
                       }).key("writer"));
  host.frame();
  host.frame();
  host.frame();
  EXPECT_EQ(describes, 1);
  EXPECT_EQ(counter.peek(), 0.0f) << "the write is refused";
  EXPECT_EQ(heard.refusals(), 1u);
}

TEST(ComposeFollowing, AMemoInsideSlotContentFollowsToo) {
  Host host;
  motion::Animatable<float> counter = motion::animatable(0.0f);
  int describes = 0;
  host.composer.render(box().children({slot("panel")}));
  host.composer.renderSlot(
      "panel",
      memo(0, [&](int) {
        ++describes;
        return box().width(20 + counter.value() * 10).height(20).fill(red());
      }).key("inside"));
  host.frame();
  counter = 2.0f;
  host.frame();
  EXPECT_EQ(describes, 2);
  EXPECT_FLOAT_EQ(require(host.composer.bounds("inside")).width(), 40.0f);
}

TEST(ComposeFollowingQueries, AMemoReadingBeatsFollowsTheLayoutAndTheProgress) {
  Host host;
  motion::Animatable<float> progress = motion::animatable(0.0f);
  std::vector<std::vector<Beat>> seen;
  host.composer.render(box().padding(20).children(
      {text(u8"EXIT", whiteStyle(24))
           .key("title")
           .textFx(textFx::entrance(textFx::rise(20), {.progress = progress})),
       memo(0, [&](int) {
         seen.push_back(host.composer.beatsOf("title", 0));
         return box().width(10).height(10);
       }).key("meter")}));
  host.frame();
  ASSERT_FALSE(seen.empty());
  ASSERT_EQ(seen.back().size(), 4u)
      << "the first draw describes the meter again once the title is laid out";
  const size_t described = seen.size();
  host.frame();
  EXPECT_EQ(seen.size(), described) << "nothing it asked about moved";
  progress = 0.5f;
  host.frame();
  ASSERT_EQ(seen.size(), described + 1);
  ASSERT_EQ(seen.back().size(), 4u);
  EXPECT_GT(seen.back().front().localProgress,
            seen[described - 1].front().localProgress);
}
