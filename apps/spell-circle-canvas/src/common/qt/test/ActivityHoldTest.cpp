/** @file
 * The activity hold: taken when either reason stands, released only when
 * neither does, and announced exactly once per transition. Nothing here
 * needs a window server.
 */

#include <gtest/gtest.h>

#include <vector>

#include "ActivityHold.h"

namespace {

using ifrit::qt::ActivityHold;
using ifrit::qt::ActivityReason;

/** A hold that records every transition it announces. */
struct RecordedHold {
  std::vector<bool> transitions;
  ActivityHold hold{[this](bool held) { transitions.push_back(held); }};
};

int window = 0;
int otherWindow = 0;

TEST(ActivityHold, IsTakenWhenEitherReasonStands) {
  RecordedHold visible;
  visible.hold.set(&window, ActivityReason::Visible, true);
  EXPECT_TRUE(visible.hold.held());
  EXPECT_EQ(visible.transitions, std::vector<bool>{true});

  RecordedHold publishing;
  publishing.hold.set(&window, ActivityReason::Publishing, true);
  EXPECT_TRUE(publishing.hold.held());
  EXPECT_EQ(publishing.transitions, std::vector<bool>{true});
}

TEST(ActivityHold, HidingDoesNotReleaseWhatPublishingHolds) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Publishing, true);
  recorded.hold.set(&window, ActivityReason::Visible, false);
  EXPECT_TRUE(recorded.hold.held());
  EXPECT_EQ(recorded.transitions, std::vector<bool>{true});
}

TEST(ActivityHold, PublishingStoppingDoesNotReleaseWhatVisibilityHolds) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Publishing, true);
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Publishing, false);
  EXPECT_TRUE(recorded.hold.held());
  EXPECT_EQ(recorded.transitions, std::vector<bool>{true});
}

TEST(ActivityHold, IsReleasedOnlyWhenNeitherStands) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Publishing, true);
  recorded.hold.set(&window, ActivityReason::Publishing, false);
  recorded.hold.set(&window, ActivityReason::Visible, false);
  EXPECT_FALSE(recorded.hold.held());
  EXPECT_EQ(recorded.transitions, (std::vector<bool>{true, false}));
}

TEST(ActivityHold, OneVisibleWindowOfTwoKeepsTheHold) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&otherWindow, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Visible, false);
  EXPECT_TRUE(recorded.hold.held());
  recorded.hold.set(&otherWindow, ActivityReason::Visible, false);
  EXPECT_FALSE(recorded.hold.held());
  EXPECT_EQ(recorded.transitions, (std::vector<bool>{true, false}));
}

TEST(ActivityHold, RepeatingAReasonAnnouncesNothing) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&otherWindow, ActivityReason::Visible, false);
  EXPECT_EQ(recorded.transitions, std::vector<bool>{true});
}

TEST(ActivityHold, WithdrawingAHolderDropsEveryReasonItRecorded) {
  RecordedHold recorded;
  recorded.hold.set(&window, ActivityReason::Visible, true);
  recorded.hold.set(&window, ActivityReason::Publishing, true);
  recorded.hold.set(&otherWindow, ActivityReason::Visible, true);
  recorded.hold.withdraw(&window);
  EXPECT_FALSE(recorded.hold.stands(&window, ActivityReason::Publishing));
  EXPECT_TRUE(recorded.hold.held());
  recorded.hold.withdraw(&otherWindow);
  EXPECT_FALSE(recorded.hold.held());
  EXPECT_EQ(recorded.transitions, (std::vector<bool>{true, false}));
}

}  // namespace
