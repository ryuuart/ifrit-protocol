// What survives a re-describe: a memo that skips describe, keyed children
// that keep their instances, a borrow that lands on the frame it is written,
// a library component themed by the rules of the sheet in force, the
// parameters a wiggled binding has to match before it prunes, and the
// structural prune that needs no memo to reach it.

#include <sigildraw/Pen.h>

#include "support/CoreTestSupport.h"
#include "support/Rows.h"

TEST(ComposeReconcile, KeyedReorderKeepsInstances) {
  Host host;
  auto row = [](const char* name, Fill fill) {
    return box().key(name).width(40).height(40).fill(std::move(fill));
  };
  host.composer.render(
      box().row().children({row("a", red()), row("b", green())}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);

  host.composer.render(
      box().row().children({row("b", green()), row("a", red())}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);  // reordered, not restyled
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

// A node whose description changes kind, or whose parent changes layout,
// retires what it no longer has and paints as the new tree rendered fresh.
TEST(ComposeReconcile,
     ANodeChangingKindPaintsAsItsNewDescriptionRenderedFresh) {
  sigil::compose::test::checkRows({
      {"UnkeyedContainerCanBecomeText",
       [] {
         Host host;
         host.composer.render(stack().children(
             {stack().children(
                  {box().key("old").width(20).height(20).fill(red())}),
              text(u8"N", whiteStyle(20))}));
         host.frame();

         const Element next = stack().children({text(u8"N", whiteStyle(20))});
         ASSERT_NO_THROW(host.composer.render(next));
         host.frame();

         Host fresh;
         fresh.composer.render(next);
         fresh.frame();
         EXPECT_TRUE(identicalPixels(host, fresh, 200, 200));
         EXPECT_EQ(host.composer.stats().instances, 2u);
         EXPECT_EQ(host.composer.stats().yogaNodes, 2u);
         EXPECT_FALSE(host.composer.bounds("old"));
       }},
      {"RootContainerCanBecomeText",
       [] {
         Host host;
         host.composer.render(box().key("root").children(
             {box().key("old").width(20).height(20).fill(red())}));
         host.frame();

         ASSERT_NO_THROW(
             host.composer.render(text(u8"N", whiteStyle(20)).key("root")));
         host.frame();
         EXPECT_NE(host.composer.paragraphLayout("root"), nullptr);
         EXPECT_FALSE(host.composer.bounds("old"));
         EXPECT_EQ(host.composer.stats().instances, 1u);
       }},
      {"SlotContentCanBecomeText",
       [] {
         Host host;
         host.composer.render(slot("content"));
         host.composer.renderSlot(
             "content",
             box().children({box().key("old").width(20).height(20)}));
         host.frame();

         ASSERT_NO_THROW(host.composer.renderSlot(
             "content", text(u8"N", whiteStyle(20)).key("text")));
         host.frame();
         EXPECT_NE(host.composer.paragraphLayout("text"), nullptr);
         EXPECT_FALSE(host.composer.bounds("old"));
         EXPECT_EQ(host.composer.stats().instances, 2u);
       }},
      {"AContainerBecomingSlotRetiresItsAuthoredChildren",
       [] {
         Host host;
         host.composer.render(box().children({box().key("content").children(
             {box().key("old").width(20).height(20).fill(red())})}));
         host.frame();

         host.composer.render(box().children({slot("content")}));
         host.frame();
         EXPECT_FALSE(host.composer.bounds("old"));
         EXPECT_EQ(host.composer.stats().instances, 2u);
         EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);

         host.composer.renderSlot(
             "content", box().key("new").width(20).height(20).fill(blue()));
         host.frame();
         EXPECT_TRUE(host.composer.bounds("new"));
         EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);

         host.composer.render(box().children({slot("content")}));
         host.frame();
         EXPECT_TRUE(host.composer.bounds("new"));
         EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);
       }},
      {"LeavingStackRestoresSurvivingChildFlow",
       [] {
         Host host;
         const Element keyedA = box().key("a").width(20).height(20).fill(red());
         const Element keyedB =
             box().key("b").width(20).height(20).fill(blue());
         const Element pinned = box()
                                    .key("pinned")
                                    .absolute()
                                    .left(70)
                                    .top(30)
                                    .width(10)
                                    .height(10)
                                    .fill(green());
         host.composer.render(stack().children({keyedA, keyedB, pinned}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("b")).left(), 0.0f);

         host.composer.render(box().row().children({keyedA, keyedB, pinned}));
         host.frame();
         EXPECT_FLOAT_EQ(require(host.composer.bounds("b")).left(), 20.0f);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("pinned")).left(), 70.0f);
         EXPECT_FLOAT_EQ(require(host.composer.bounds("pinned")).top(), 30.0f);
         EXPECT_EQ(host.pixel(25, 5), SK_ColorBLUE);
       }},
      {"LeavingTextClearsItsBaselineCallback",
       [] {
         Host host;
         const Element peer = text(u8"N", whiteStyle(20));
         host.composer.render(
             box()
                 .row()
                 .alignItems(Align::Baseline)
                 .children({text(u8"N", whiteStyle(30)).key("item"), peer}));
         host.frame();

         const Element next =
             box()
                 .row()
                 .alignItems(Align::Baseline)
                 .children({box().key("item").width(20).height(40).fill(red()),
                            peer});
         host.composer.render(next);
         host.frame();

         Host fresh;
         fresh.composer.render(next);
         fresh.frame();
         EXPECT_EQ(require(host.composer.bounds("item")),
                   require(fresh.composer.bounds("item")));
         EXPECT_TRUE(identicalPixels(host, fresh, 200, 200));
       }},
  });
}

// A memo calls its component only when its properties change, a change of
// property type included.
TEST(ComposeReconcile, AMemoSkipsItsComponentWhileItsPropertiesCompareEqual) {
  sigil::compose::test::checkRows({
      {"MemoSkipsDescribe",
       [] {
         struct Props {
           int value;
           bool operator==(const Props&) const = default;
         };
         static int describeCalls;
         describeCalls = 0;
         auto component = [](const Props& properties) {
           ++describeCalls;
           return box()
               .width(20 + (float)properties.value)
               .height(20)
               .fill(red());
         };

         Host host;
         auto describe = [&](int first, int second) {
           return box().row().children(
               {memo(Props{first}, component).key("a"),
                memo(Props{second}, component).key("b")});
         };

         host.composer.render(describe(1, 2));
         EXPECT_EQ(describeCalls, 2);
         host.composer.render(describe(1, 2));  // nothing changed
         EXPECT_EQ(describeCalls, 2);
         EXPECT_EQ(host.composer.stats().memoHits, 2u);
         host.composer.render(describe(1, 3));  // one prop changed
         EXPECT_EQ(describeCalls, 3);
         EXPECT_EQ(host.composer.stats().memoHits, 1u);
       }},
      {"ChangingMemoPropertiesTypeDescribesTheNewComponent",
       [] {
         struct Number {
           int value;
           bool operator==(const Number&) const = default;
         };
         struct Name {
           std::string value;
           bool operator==(const Name&) const = default;
         };
         Host host;
         host.composer.render(memo(Number{7}, [](const Number&) {
                                return box().width(20).height(20).fill(red());
                              }).key("item"));
         host.frame();

         ASSERT_NO_THROW(host.composer.render(
             memo(Name{"ready"}, [](const Name&) {
               return box().width(20).height(20).fill(blue());
             }).key("item")));
         host.frame();
         EXPECT_EQ(host.composer.stats().memoHits, 0u);
         EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);
       }},
  });
}

TEST(ComposeReconcile, MemoShellCacheOverridesDoNotMutateSharedElements) {
  const auto describe = [] {
    return box().key("shared").width(20).height(20).fill(red()).cache(
        Cache::Picture);
  };
  const Element shared = describe();
  Host live;
  live.composer.setProfiling(true);
  live.composer.render(memo(0, [shared](int) { return shared; })
                           .cache(Cache::None)
                           .cacheScale(0.5f));
  EXPECT_TRUE(sameDescription(shared, describe()));
  live.frame();
  const Composer::NodeCost* liveRow = requireRow(live.composer, "shared");
  ASSERT_NE(liveRow, nullptr);
  EXPECT_EQ(liveRow->cacheState, Composer::CacheState::Live);

  Host cached;
  cached.composer.setProfiling(true);
  cached.composer.render(shared);
  cached.frame();
  cached.frame();
  const Composer::NodeCost* cachedRow = requireRow(cached.composer, "shared");
  ASSERT_NE(cachedRow, nullptr);
  EXPECT_EQ(cachedRow->cacheState, Composer::CacheState::Picture);
}

namespace {

struct CountArrangements {
  std::shared_ptr<int> calls;
  bool operator==(const CountArrangements&) const = default;
  void arrange(Arrangement&) const { ++*calls; }
};

}  // namespace

// ---- the order the declared reads imply -------------------------------------

namespace {

/** An adding operator that wires two keyed nodes of its scope: what the
 *  kit's connecting operator does, in miniature, so the kernel's own test
 *  needs no catalog. */
struct WireBetween {
  std::string from;
  std::string to;
  bool operator==(const WireBetween&) const = default;
  void add(Scope& scope) const {
    const Scope::Node* start = scope.find(from);
    const Scope::Node* end = scope.find(to);
    if (!start || !end) return;
    Element figure =
        pathFigure(routeBetween({}, start->bounds, end->bounds), 6.0f);
    figure.key(from + "->" + to);
    scope.attach(std::move(figure));
  }
};

/** A decoration that strokes a path BORROWED from a keyed node — the one
 *  kind of mark whose answer is another node's finished geometry. */
struct BorrowedStroke {
  std::string key;
  std::vector<std::string> borrows() const { return {key}; }
  void paint(sigil::draw::Pen& pen, const PaintContext& ctx) const {
    SkCanvas& canvas = *pen.canvas();
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(6.0f);
    paint.setAntiAlias(false);
    canvas.drawPath(geometry::path::toSk(ctx.borrowedPath(key)), paint);
  }
};

}  // namespace

TEST(ComposeDerive, ABorrowOfAnAddedWireLandsOnTheFirstFrame) {
  // The borrower reads the WIRE's outline, and the wire does not exist
  // until the operator has run: it is built from where the two nodes
  // settled, mounted beside them, and laid out with them standing. A
  // borrow resolved against the tree as it was AUTHORED finds nothing and
  // dresses an empty box for a whole frame, catching up only on the next
  // one.
  for (const bool stacked : {false, true}) {
    SCOPED_TRACE(stacked);
    Decoration mark = BorrowedStroke{"a->b"};
    if (stacked) mark = DecorationStack{{mark}};
    Host host;
    host.composer.render(
        positioned()
            .inset(0)
            .children({box().absolute().inset(0).foreground(mark),
                       box().key("a").left(20).top(90).width(20).height(20),
                       box().key("b").left(160).top(90).width(20).height(20)})
            .operators({WireBetween{.from = "a", .to = "b"}}));
    host.frame();
    // Direct and stacked marks must both register the borrow before the
    // first draw. An unresolved borrow has no path through these pixels.
    EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
    EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
    EXPECT_EQ(host.pixel(100, 40), SK_ColorBLACK);
  }
}

TEST(ComposeFeed, ARowTakesTheRuleOfTheStyleItNames) {
  // A library component is themed the way every element is: by the rules of
  // the sheet in force. A text feed's row carries the name of its style as
  // its class, so whoever composed the feed sets it without handing the
  // feed anything.
  feed::TextRing ring;
  ring.append({u8"ready.", "loud"});

  const auto describe = [&](StyleSheet sheet) {
    return box()
        .padding(4)
        .applyStyleSheet(std::move(sheet))
        .children({box().children({feed::feed(ring)})});
  };
  Host themed;
  themed.composer.render(
      describe(StyleSheet{rule(".loud").font(sigil::weave::Type{.size = 30})}));
  themed.frame();
  const SkRect loud = require(themed.composer.bounds(feed::rowKey(1)));

  // The same feed under a sheet that names another class sets the row in
  // the font in force, which is what proves the rule was read.
  Host plain;
  plain.composer.render(describe(
      StyleSheet{rule(".quiet").font(sigil::weave::Type{.size = 30})}));
  plain.frame();
  const SkRect quiet = require(plain.composer.bounds(feed::rowKey(1)));
  EXPECT_GT(quiet.width(), 0.0f);
  EXPECT_GT(loud.width(), quiet.width());
}

TEST(ComposeReconcile, TwoSeedsShakeIndependentlyOnScreen) {
  // The whole path in PIXELS: two seeds, two marks, and the displacement
  // the paint actually produced. (Not a prune pin — keyed siblings never
  // prune into one another; AnEqualDescriptionPrunesAndAChangedOnePatches
  // holds the prune. This one proves the seed survives Element → reconciler →
  // paint, which is what makes a two-axis shake possible instead of a diagonal
  // slide.)
  static motion::Animatable<float> phase = motion::animatable(0.0f);
  phase = 0.0f;
  Host host(200, 200);
  auto tree = [] {
    // stack(): both marks lay out at the origin, so each row below is
    // unambiguously one of them.
    return stack().children(
        {box()
             .key("x")
             .width(8)
             .height(8)
             .fill(red())
             .translateX(motion::bind(
                 phase,
                 {.to = {100.0f, 100.0f},
                  .wiggle = {.amount = 40.0f, .frequency = 3.0f, .seed = 1}}))
             .translateY(30.0f),
         box()
             .key("y")
             .width(8)
             .height(8)
             .fill(green())
             .translateX(motion::bind(
                 phase,
                 {.to = {100.0f, 100.0f},
                  .wiggle = {.amount = 40.0f, .frequency = 3.0f, .seed = 2}}))
             .translateY(90.0f)});
  };
  host.composer.render(tree());

  // bounds() is the LAYOUT rect and a translate is paint-only, so the
  // observation has to be pixels: where the mark actually landed.
  const auto centerOf = [&host](int row, SkColor want) {
    int low = -1, high = -1;
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, row) == want) {
        if (low < 0) low = x;
        high = x;
      }
    return low < 0 ? -1.0f : 0.5f * (float)(low + high);
  };

  bool everApart = false, xMoved = false, yMoved = false;
  float firstX = -1, firstY = -1;
  for (int frame = 0; frame < 90; ++frame) {
    phase = (float)frame / 30.0f;
    host.frame();
    const float x = centerOf(34, SK_ColorRED);
    const float y = centerOf(94, SK_ColorGREEN);
    ASSERT_GE(x, 0.0f) << "frame " << frame;
    ASSERT_GE(y, 0.0f) << "frame " << frame;
    if (firstX < 0) {
      firstX = x;
      firstY = y;
    }
    if (std::fabs(x - firstX) > 4.0f) xMoved = true;
    if (std::fabs(y - firstY) > 4.0f) yMoved = true;
    if (std::fabs(x - y) > 6.0f) everApart = true;
  }
  EXPECT_TRUE(xMoved) << "the x shake never moved";
  EXPECT_TRUE(yMoved) << "the y shake never moved";
  EXPECT_TRUE(everApart)
      << "two seeds produced the same displacement every frame — either the "
         "seed is not reaching the noise, or the second node pruned into the "
         "first";
}

// An equal description prunes with no memo and repeats no layout; a changed
// field patches in full, every field of a wiggle included.
TEST(ComposeReconcile, AnEqualDescriptionPrunesAndAChangedOnePatches) {
  sigil::compose::test::checkRows({
      {"StructuralPruneNeedsNoMemo",
       [] {
         // memo() is an optimisation for expensive DESCRIBES, not the thing
         // that makes pruning work. A subtree whose new description equals its
         // old one is skipped wholesale either way, so plain boxes, text and
         // images built from value-comparable properties re-render for free.
         Host host;
         auto tree = [] {
           return box().row().gap(8).padding(12).children(
               {box().width(40).height(40).borderRadius({6}).fill(red()),
                text(u8"static", styleAt(18)).key("t"),
                box().flexGrow(1).fill(blue()).opacity(0.9f)});
         };
         host.composer.render(tree());
         host.frame();

         host.composer.render(tree());  // brand-new Elements, identical values
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         EXPECT_FALSE(host.composer.dirty());  // hosts may skip the redraw
         host.frame();
         EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
       }},
      {"AnUnchangedDescribeDoesNotRepeatLayout",
       [] {
         Host host;
         const auto calls = std::make_shared<int>(0);
         auto describe = [&] {
           return box()
               .operators({CountArrangements{calls}})
               .children({box().key("a").width(20).height(20),
                          box().key("b").width(20).height(20)});
         };
         host.composer.render(describe());
         host.frame();
         ASSERT_GT(*calls, 0);
         const int firstLayoutCalls = *calls;

         host.composer.render(describe());
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         host.frame();
         EXPECT_EQ(*calls, firstLayoutCalls);
         host.frame();
         EXPECT_EQ(*calls, firstLayoutCalls);
       }},
      {"APrunedPatchLeavesTheComputedStyleStanding",
       [] {
         // The style a mounted node stands in is written where the description
         // is PATCHED. A pruned describe swaps a description in without
         // patching, so nothing writes the style that frame — and nothing has
         // to, because the description it swapped in was proved to carry the
         // same properties.
         //
         // WHAT THIS PINS, exactly: that a patch arriving AFTER a prune takes
         // in full, and that a patch moving one property does not drop the one
         // an earlier patch wrote. A fill that wrote only the fields this
         // describe changed, or that stopped writing at all, reddens it.
         //
         // A change that reaches the node without touching its OWN declarations
         // — a rule, or a value inherited from an ancestor — prunes, and the
         // cascade pass re-folds it instead; ComposeRuleScope pins the rule
         // half and ComposeDeclarations the inherited one.
         Host host(200, 200);
         auto describe = [](float width, float opacity) {
           return box().children({box()
                                      .key("a")
                                      .width(width)
                                      .height(20)
                                      .borderRadius({0})
                                      .opacity(opacity)
                                      .fill(red())});
         };
         host.composer.render(describe(100, 1.0f));
         host.frame();
         ASSERT_TRUE(host.composer.bounds("a").has_value());
         EXPECT_FLOAT_EQ(host.composer.bounds("a")->width(), 100.0f);
         EXPECT_EQ(host.pixel(50, 10), SK_ColorRED);

         host.composer.render(describe(100, 1.0f));  // identical: prunes
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
         host.frame();
         EXPECT_FLOAT_EQ(host.composer.bounds("a")->width(), 100.0f);
         EXPECT_EQ(host.pixel(50, 10), SK_ColorRED);

         host.composer.render(describe(40, 1.0f));  // the layout property moves
         host.frame();
         EXPECT_FLOAT_EQ(host.composer.bounds("a")->width(), 40.0f);
         EXPECT_EQ(host.pixel(50, 10), SK_ColorBLACK);

         host.composer.render(describe(40, 0.0f));  // …and a paint property
         host.frame();
         EXPECT_EQ(host.pixel(20, 10), SK_ColorBLACK);
         // The paint patch rewrote the whole style, so the width the PREVIOUS
         // patch wrote is still standing: a fill that copied only what this
         // describe changed would answer the property's default here.
         EXPECT_FLOAT_EQ(host.composer.bounds("a")->width(), 40.0f);
       }},
      {"WiggledBindingsPruneOnlyWhenEveryParameterMatches",
       [] {
         // THE TRAP, pinned. Binding's equality compares a binding FIELD BY
         // FIELD, and a field left out of it fails INVISIBLY: two different
         // wiggles compare equal, the node prunes, and the instance keeps
         // applying the OLD stages forever while every other test still passes.
         // So each of the five wiggle fields gets its own re-describe here.
         //
         // If this test fails, do not relax it — a field is missing from
         // Binding's equality.
         static motion::Animatable<float> phase = motion::animatable(0.0f);
         phase = 0.35f;
         struct Rig {
           float amount = 12.0f;
           float frequency = 7.0f;
           uint32_t seed = 1;
           int octaves = 1;
           float falloff = 0.5f;
         };
         auto tree = [](Rig rig) {
           return box().children(
               {box().key("shaken").width(40).height(40).fill(red()).translateX(
                   motion::bind(phase, {.to = {-70.0f, 170.0f},
                                        .wiggle = {.amount = rig.amount,
                                                   .frequency = rig.frequency,
                                                   .seed = rig.seed,
                                                   .octaves = rig.octaves,
                                                   .falloff = rig.falloff}}))});
         };

         Host host;
         host.composer.render(tree({}));
         host.frame();

         // The prune half: an identical re-describe costs nothing.
         host.composer.render(tree({}));
         EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
             << "an identical wiggle must still prune";
         EXPECT_FALSE(host.composer.dirty());

         // The over-prune half, one field at a time. Each of these is a
         // DIFFERENT wiggle and must reach the instance.
         const Rig moved[] = {
             {.amount = 20.0f}, {.frequency = 11.0f}, {.seed = 2},
             {.octaves = 3},    {.falloff = 0.9f},
         };
         const char* named[] = {"amount", "frequency", "seed", "octaves",
                                "falloff"};
         for (size_t index = 0; index < std::size(moved); ++index) {
           host.composer.render(tree({}));  // back to the baseline rig
           host.frame();
           host.composer.render(tree(moved[index]));
           EXPECT_GT(host.composer.stats().patchedNodes, 0u)
               << named[index]
               << " changed and the node PRUNED — Binding's equality is "
                  "missing that field";
           EXPECT_TRUE(host.composer.dirty()) << named[index];
         }
       }},
  });
}
