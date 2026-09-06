// The auto table: how a column takes its width from what is in it, how a
// span tops up the tracks it covers, and where a child that claimed no
// cells lands. The layout is the kernel's — a scheme the kit configures
// but does not own — so its cases stand here, with the seam a child
// claims its cells through.

#include "support/CoreTestSupport.h"

TEST(ComposeTable, AColumnIsAsWideAsWhatIsInItAndSharesTheSurplus) {
  // Three columns of unequal content in a 300-wide table with no spacing:
  // each column starts at its widest child, and the 300 − 180 left over is
  // shared out IN PROPORTION, so the widest column takes the most of it.
  Host host;
  Table table{.width = 300};
  host.composer.render(
      box().child(layout(table)
                      .width(pct(100))
                      .grow(1)
                      .child(box().key("a").width(30).height(20).cells(0, 0))
                      .child(box().key("b").width(60).height(20).cells(1, 0))
                      .child(box().key("c").width(90).height(20).cells(2, 0))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  const auto c = host.composer.bounds("c");
  ASSERT_TRUE(a && b && c);
  // 30 : 60 : 90 of 180, scaled to 300 — the proportional share.
  EXPECT_NEAR(a->left(), 0, 0.01f);
  EXPECT_NEAR(b->left(), 50, 0.01f);
  EXPECT_NEAR(c->left(), 150, 0.01f);
  // The children keep their measured size; the COLUMN grew, not the child.
  EXPECT_NEAR(a->width(), 30, 0.01f);
}

TEST(ComposeTable, ASpanTopsUpColumnsAndRowsDifferently) {
  // The asymmetry that is the browsers' and not a slip: a colspan's
  // deficit is shared across the columns it covers, a rowspan's lands
  // entirely on the LAST row it covers.
  Host host;
  Table table{.columns = 2, .rows = 2, .width = 100};
  host.composer.render(box().child(
      layout(table)
          .width(200)
          .height(200)
          .child(box().key("wide").width(100).height(10).cells(0, 0, 2, 1))
          .child(box().key("tall").width(10).height(100).cells(0, 1, 1, 2))
          .child(box().key("small").width(10).height(10).cells(1, 1))));
  host.frame();
  const auto wide = host.composer.bounds("wide");
  const auto tall = host.composer.bounds("tall");
  const auto small = host.composer.bounds("small");
  ASSERT_TRUE(wide && tall && small);
  // The two columns start at 10 (the tall child) and 10 (the small one),
  // and the 100-wide span tops both up equally — 50 each, which is the
  // whole 100-wide table, so nothing is left over to share.
  EXPECT_NEAR(small->left(), 50, 0.01f);
  // Row 1 holds a 10-tall child and the first row of a 100-tall span; the
  // span's deficit goes to row 2, so row 1 stays at its own content.
  EXPECT_NEAR(small->top(), 10, 0.01f);
  EXPECT_NEAR(tall->top(), 10, 0.01f);
}

TEST(ComposeTable, WhatNoChildClaimedFlowsAndAlignsInsideItsCell) {
  // A child that says nothing takes the next cell NO declared child
  // claimed — never one already spoken for, which is the failure of a
  // scheme that counts its flow from zero.
  Host host;
  Table table{.columns = 2, .width = 200};
  host.composer.render(box().child(
      layout(table)
          .width(200)
          .height(100)
          .child(box().key("pinned").width(20).height(20).cells(1, 0))
          .child(box().key("flowed").width(20).height(20))
          .child(box().key("right").width(20).height(20).cells(1, 1).cellAlign(
              Align::End, Align::Start))));
  host.frame();
  const auto pinned = host.composer.bounds("pinned");
  const auto flowed = host.composer.bounds("flowed");
  const auto right = host.composer.bounds("right");
  ASSERT_TRUE(pinned && flowed && right);
  EXPECT_NEAR(flowed->left(), 0, 0.01f) << "cell (1,0) was taken";
  EXPECT_NEAR(flowed->top(), 0, 0.01f);
  EXPECT_GT(pinned->left(), flowed->left());
  // Both are in column 1, which the surplus grew to 100 wide. The pinned
  // child sits at its start; the end-aligned one is flush with the far
  // side of the same cell.
  EXPECT_NEAR(pinned->right(), 120, 0.01f);
  EXPECT_NEAR(right->right(), 200, 0.01f);
  EXPECT_GT(right->top(), 0.0f) << "the second row";
}
