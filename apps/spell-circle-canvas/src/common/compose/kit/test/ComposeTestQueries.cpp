// What the composer answers about a laid-out tree: the bounds of a node
// that has not been laid out yet, the shell that opts out of hit
// testing, and the slot that survives its content and says so when its
// key is renamed.

#include <string>

#include "support/StudioTestSupport.h"

TEST(ComposeQuery, AKeyedShellCanOptOutOfHitTesting) {
  // hitTest returns any keyed node whose box contains the point, whether
  // or not it paints. That is correct, and it means a keyed full-bleed
  // layout SHELL with no fill swallows every hit in the frame: a study's
  // four stat-bar groups were transparent containers carrying their
  // bars' keys, and every point on screen came back as the last of them.
  // Silent and total.
  auto tree = [](bool shellTestable) {
    Element shell = box().key("shell").absolute().inset(0);
    shell.hitTestable(shellTestable);
    shell.children(
        {box().key("bar").absolute().left(20).top(20).width(40).height(40).fill(
            red())});
    return box().children({std::move(shell)});
  };

  Host greedy(200, 200);
  greedy.composer.render(tree(true));
  greedy.frame();
  EXPECT_EQ(greedy.composer.hitTest({150, 150}), "shell");
  EXPECT_EQ(greedy.composer.hitTest({40, 40}), "bar");

  Host polite(200, 200);
  polite.composer.render(tree(false));
  polite.frame();
  // The shell no longer answers for empty space…
  EXPECT_FALSE(polite.composer.hitTest({150, 150}).has_value());
  // …and its CHILDREN are still tested, which is the whole distinction.
  EXPECT_EQ(polite.composer.hitTest({40, 40}), "bar");
}

TEST(ComposeQuery, BoundsIsAbsentRatherThanNaNBeforeLayout) {
  // Layout runs inside draw(), so a query issued in the same update() as the
  // render() before it is reading an UNLAID tree. It must answer "no value"
  // rather than a rect full of NaN: a NaN rect propagates silently into
  // whatever arithmetic the caller does with it.
  Host host(200, 200);
  host.composer.render(box().children(
      {box().key("cell").absolute().left(10).top(10).width(50).height(50)}));
  // No frame() yet: nothing has been laid out, so the answer is ABSENCE.
  // An answer here at all would be a rect built out of an unlaid node.
  EXPECT_FALSE(host.composer.bounds("cell").has_value())
      << "a query before layout answered with a rect";

  host.frame();
  const auto after = host.composer.bounds("cell");
  ASSERT_TRUE(after.has_value());
  EXPECT_TRUE(after->isFinite());
  EXPECT_FLOAT_EQ(after->width(), 50.0f);

  // A key that was never in the tree is still absent, not NaN.
  EXPECT_FALSE(host.composer.bounds("nope").has_value());
}

TEST(ComposeSlots, ASlotSurvivesItsContentCarryingTheSameKey) {
  // slot(name) sets node->key = name, so resolving renderSlot through the
  // shared key index makes a slot collidable with ordinary keys. Give the
  // slot's CONTENT a root .key(name) and — a child being indexed after its
  // parent, last writer wins — the content shadows the slot: every later
  // renderSlot() returns silently and the slot freezes on its first value,
  // with no warning. Slots therefore keep their own index.
  Host host(200, 200);
  host.composer.render(box().children({slot("readout").absolute().inset(0)}));
  host.composer.renderSlot(
      "readout", box().key("readout").absolute().inset(0).fill(red()));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);

  // Control: with NO colliding key the second update has always worked.
  Host control(200, 200);
  control.composer.render(box().children({slot("r2").absolute().inset(0)}));
  control.composer.renderSlot("r2", box().absolute().inset(0).fill(red()));
  control.frame();
  control.composer.renderSlot("r2", box().absolute().inset(0).fill(green()));
  control.frame();
  ASSERT_GT(SkColorGetG(control.pixel(100, 100)), 180)
      << "the plain slot path is broken, not the collision fix";

  // The second update must land. Before the fix this returned silently.
  host.composer.renderSlot(
      "readout", box().key("readout").absolute().inset(0).fill(green()));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
  EXPECT_LT(SkColorGetR(host.pixel(100, 100)), 80);

  // And a slot's name still answers bounds(), so the two indexes coexist.
  EXPECT_TRUE(host.composer.bounds("readout").has_value());
}

TEST(ComposeSlots, KeyOnASlotRenamesItAndSaysSoOnce) {
  // A slot's NAME is its key — one field — so `.key()` on a slot RENAMES
  // the mount. renderSlot() on the original name then no-ops and the symptom
  // is a zero-height layout rather than an error. The warning has to fire at
  // the call that causes it, which is the only place both names are still in
  // hand.
  ::testing::internal::CaptureStderr();
  {
    Host quiet(200, 200);
    quiet.composer.render(box().children({slot("gauges").absolute().inset(0)}));
    quiet.composer.renderSlot("gauges", box().absolute().inset(0).fill(red()));
    quiet.frame();
    EXPECT_GT(SkColorGetR(quiet.pixel(100, 100)), 180);
  }
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "")
      << "naming a slot once must be silent";

  ::testing::internal::CaptureStderr();
  Element renamed = slot("dials").key("panel");
  const std::string log = ::testing::internal::GetCapturedStderr();
  EXPECT_NE(log.find("dials"), std::string::npos) << log;
  EXPECT_NE(log.find("panel"), std::string::npos) << log;

  // Once per rename, not per frame — the same call in a describe loop
  // must not print sixty lines a second.
  ::testing::internal::CaptureStderr();
  (void)slot("dials").key("panel");
  EXPECT_EQ(::testing::internal::GetCapturedStderr(), "");

  // And the warning is telling the truth: the mount answers to the NEW
  // name only.
  Host host(200, 200);
  host.composer.render(box().children({renamed.absolute().inset(0)}));
  host.composer.renderSlot("panel", box().absolute().inset(0).fill(green()));
  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
}
