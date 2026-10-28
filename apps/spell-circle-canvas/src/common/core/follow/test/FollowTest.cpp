/** @file
 * Following over a counter with nothing behind it: a read recorded once at
 * the revision it saw, staleness answered from the counts alone, no
 * recording outside a scope, nested scopes, and a write of a value the
 * followed describe read refused and reported once.
 */

#include <gtest/gtest.h>
#include <sigilcore/follow/Follow.h>
#include <sigilcore/report/Report.h>

#include <memory>
#include <string>
#include <vector>

using namespace sigil::core;

namespace {

/** A number a describe can follow: read through `read()`, written through
 *  `write()`, which advances only on a change. */
struct Counter : Followable {
  int number = 0;
  void write(int next) {
    if (!admitsWrite() || next == number) return;
    number = next;
    advance();
  }
};

int read(const std::shared_ptr<Counter>& counter) {
  noteRead(counter);
  return counter->number;
}

/** Collects every report while it lives. */
struct HeardReports {
  std::vector<std::string> lines;
  Reporter previous;
  HeardReports() {
    previous = installReporter(
        [this](std::string_view, std::string_view message) {
          lines.emplace_back(message);
        });
  }
  ~HeardReports() { installReporter(previous); }
};

}  // namespace

TEST(CoreFollow, AReadInsideAScopeIsRecordedOnceAtItsRevision) {
  auto counter = std::make_shared<Counter>();
  counter->write(3);
  FollowedReads reads;
  {
    FollowScope scope(reads);
    EXPECT_EQ(read(counter), 3);
    EXPECT_EQ(read(counter), 3);
  }
  ASSERT_EQ(reads.size(), 1u);
  EXPECT_EQ(reads.reads().front().revision, counter->revision());
  EXPECT_FALSE(reads.moved());
}

TEST(CoreFollow, AChangingWriteMakesTheRecordingStaleAndAnEqualOneDoesNot) {
  auto counter = std::make_shared<Counter>();
  FollowedReads reads;
  {
    FollowScope scope(reads);
    read(counter);
  }
  counter->write(0);
  EXPECT_FALSE(reads.moved()) << "writing the number it holds changes nothing";
  counter->write(1);
  EXPECT_TRUE(reads.moved());
}

TEST(CoreFollow, NothingIsRecordedOutsideAScope) {
  auto counter = std::make_shared<Counter>();
  EXPECT_EQ(following(), nullptr);
  read(counter);
  FollowedReads reads;
  { FollowScope scope(reads); }
  EXPECT_TRUE(reads.empty());
}

TEST(CoreFollow, AnInnerScopeRecordsIntoItsOwnAndTheOuterResumes) {
  auto outerValue = std::make_shared<Counter>();
  auto innerValue = std::make_shared<Counter>();
  FollowedReads outer, inner;
  {
    FollowScope outerScope(outer);
    read(outerValue);
    {
      FollowScope innerScope(inner);
      read(innerValue);
    }
    EXPECT_EQ(following(), &outer);
  }
  EXPECT_EQ(following(), nullptr);
  ASSERT_EQ(outer.size(), 1u);
  ASSERT_EQ(inner.size(), 1u);
  EXPECT_TRUE(outer.contains(*outerValue));
  EXPECT_TRUE(inner.contains(*innerValue));
}

TEST(CoreFollow, AWriteOfAValueTheDescribeReadIsRefusedAndReportedOnce) {
  HeardReports heard;
  auto counter = std::make_shared<Counter>();
  FollowedReads reads;
  {
    FollowScope scope(reads);
    read(counter);
    counter->write(5);
    counter->write(6);
  }
  EXPECT_EQ(counter->number, 0) << "both writes are refused";
  EXPECT_FALSE(reads.moved());
  EXPECT_EQ(heard.lines.size(), 1u);
  // Outside the describe the same value takes a write as usual.
  counter->write(7);
  EXPECT_EQ(counter->number, 7);
  EXPECT_TRUE(reads.moved());
}

TEST(CoreFollow, AWriteBeforeTheReadLands) {
  HeardReports heard;
  auto counter = std::make_shared<Counter>();
  FollowedReads reads;
  {
    FollowScope scope(reads);
    counter->write(2);
    EXPECT_EQ(read(counter), 2);
  }
  EXPECT_TRUE(heard.lines.empty());
  EXPECT_FALSE(reads.moved()) << "the read saw the written revision";
}
