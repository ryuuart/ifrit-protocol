// What survives a re-describe: a memo that skips describe, keyed children
// that keep their instances, a borrow that lands on the frame it is written,
// the environment a component reads without being handed it, and the
// parameters a wiggled binding has to match before it prunes.

#include "support/CoreTestSupport.h"

TEST(ComposeReconcile, MemoSkipsDescribe) {
  struct Props {
    int value;
    bool operator==(const Props&) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props& p) {
    ++describeCalls;
    return box().width(20 + (float)p.value).height(20).fill(red());
  };

  Host host;
  auto describe = [&](int a, int b) {
    return box()
        .row()
        .child(memo(Props{a}, component).key("a"))
        .child(memo(Props{b}, component).key("b"));
  };

  host.composer.render(describe(1, 2));
  EXPECT_EQ(describeCalls, 2);
  host.composer.render(describe(1, 2));  // nothing changed
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 2u);
  host.composer.render(describe(1, 3));  // one prop changed
  EXPECT_EQ(describeCalls, 3);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);
}

TEST(ComposeReconcile, KeyedReorderKeepsInstances) {
  Host host;
  auto row = [](const char* k, Fill f) {
    return box().key(k).width(40).height(40).fill(std::move(f));
  };
  host.composer.render(
      box().row().child(row("a", red())).child(row("b", green())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorRED);

  host.composer.render(
      box().row().child(row("b", green())).child(row("a", red())));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);  // reordered, not restyled
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

// ---- the order the declared reads imply -------------------------------------

namespace {

/** A decoration that strokes a path BORROWED from a keyed node — the one
 *  kind of mark whose answer is another node's finished geometry. */
struct BorrowedStroke {
  std::string key;
  std::vector<std::string> borrows() const { return {key}; }
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint paint;
    paint.setColor(SK_ColorRED);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(6.0f);
    paint.setAntiAlias(false);
    canvas.drawPath(ctx.borrowedPath(key), paint);
  }
};

}  // namespace

TEST(ComposeDerive, ABorrowOfAConnectorWrittenAfterItLandsOnTheFirstFrame) {
  // The borrower reads the wire's OUTLINE and is written before the wire,
  // so resolving the two in the order they were written hands it a route
  // that has not been laid yet — it dresses the connector's empty box for
  // a whole frame and only catches up on the next one. Both declare what
  // they read, so both are resolved in one pass in the order those
  // declarations imply, and the borrowed route is right the first time.
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box()
                     .absolute()
                     .inset(0, 0, 0, 0)
                     .foreground(Decoration(BorrowedStroke{"wire"})))
          .child(box().key("a").left(20).top(90).width(20).height(20))
          .child(box().key("b").left(160).top(90).width(20).height(20))
          .child(connector("a", "b").key("wire").absolute().inset(0, 0, 0, 0)));
  host.frame();  // THE FIRST frame — a pass behind is visible only here
  // The route runs centre to centre along y=100, and the borrowed stroke
  // is on it. An unrouted borrow dresses the connector's own box instead,
  // whose edges are nowhere near the middle of the canvas.
  EXPECT_EQ(host.pixel(100, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(40, 100), SK_ColorRED);
  EXPECT_EQ(host.pixel(100, 40), SK_ColorBLACK);
}

namespace {

struct EnvPalette {
  SkColor4f surface{1, 0, 0, 1};
  SkColor4f accent{0, 1, 0, 1};
  bool operator==(const EnvPalette&) const = default;
};

/** A component four levels below whoever bound the value, handed nothing
 *  and reading the environment — the `feed::`/decoration case. */
Element envThemedChip() {
  return box().width(20).height(20).fill(
      Fill::color(core::environment::inheritedOr(EnvPalette{}).surface));
}

/** Its sibling, which reads nothing and must never repatch for a theme. */
Element envPlainChip() { return box().width(20).height(20).fill(blue()); }

Element envLevel3() {
  return box().child(envThemedChip()).child(envPlainChip());
}

Element envLevel2() { return box().child(envLevel3()); }

Element envLevel1() { return box().child(envLevel2()); }

/** Describe under a binding, and hand back a tree the binding no longer
 *  touches — the whole design in three lines. */
Element envDescribeWith(EnvPalette p) {
  core::environment::Provide<EnvPalette> theme(p);
  return box().child(envLevel1());
}

}  // namespace

TEST(ComposeEnv, InheritedValueReachesAComponentNobodyHandedIt) {
  Host host;
  Element tree = envDescribeWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  EXPECT_FALSE(
      core::environment::bound<EnvPalette>());  // the scope ended; the VALUE is
  host.composer.render(tree);                   // already baked into the tree
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);   // the themed chip
  EXPECT_EQ(host.pixel(5, 25), SK_ColorBLUE);  // its plain sibling

  // Unbound: the component's own default, exactly like a React context's.
  Host bare;
  bare.composer.render(box().child(envLevel1()));
  bare.frame();
  EXPECT_EQ(bare.pixel(5, 5), SK_ColorRED);
  EXPECT_FALSE(
      core::environment::bound<EnvPalette>());  // and the scope unwound
}

TEST(ComposeEnv, UnchangedEnvironmentStillPrunes) {
  // THE PRUNING PIN. A theme-reading node whose theme did not change must
  // prune like any other structurally-equal description — no patch, no
  // re-record, host free to skip the frame.
  Host host;
  const EnvPalette dark{{0, 0, 1, 1}, {1, 1, 0, 1}};
  auto renderWith = [&](EnvPalette p) {
    host.composer.render(envDescribeWith(p));
  };
  renderWith(dark);
  host.frame();
  ASSERT_EQ(host.pixel(5, 5), SK_ColorBLUE);  // it IS the inherited colour —
                                              // without this the pin below
                                              // would pass on a tree that
                                              // never read the environment

  renderWith(dark);  // a DISTINCT palette object, equal by operator==
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  EXPECT_FALSE(host.composer.dirty());
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
}

TEST(ComposeEnv, ThemeChangeRepatchesOnlyTheNodesThatMoved) {
  // The other half: a change costs exactly the readers whose properties moved,
  // not the provider's subtree. Four container levels and one plain
  // sibling sit between the binding and the reader; none of them repatch.
  Host host;
  auto renderWith = [&](EnvPalette p) {
    host.composer.render(envDescribeWith(p));
  };
  renderWith(EnvPalette{{0, 0, 1, 1}, {1, 1, 0, 1}});
  host.frame();

  renderWith(EnvPalette{{0, 1, 0, 1}, {1, 1, 0, 1}});  // surface moved only
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u);
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(5, 25), SK_ColorBLUE);
}

TEST(ComposeEnv, MemoIsAPureFunctionOfPropsAndEnvironment) {
  // memo is the ONE deferred describe in the library, so it is the one
  // place an inherited value could go stale: its `fn` runs inside the
  // reconciler, long after the scope that bound the palette ended.
  struct Props {
    int id = 0;
    bool operator==(const Props&) const = default;
  };
  static int describeCalls;
  describeCalls = 0;
  auto component = [](const Props&) {
    ++describeCalls;
    return box().width(20).height(20).fill(
        Fill::color(core::environment::inheritedOr(EnvPalette{}).surface));
  };

  Host host;
  // DESCRIBED inside the scope, RECONCILED after it ends — which is the
  // whole difficulty: `component` runs during render(), by which time the
  // Provide below has been destroyed. Building the tree and handing it to
  // the composer are two statements, deliberately.
  auto describeWith = [&component](EnvPalette p) {
    core::environment::Provide<EnvPalette> theme(p);
    return box().child(memo(Props{1}, component).key("m"));
  };
  auto renderWith = [&](EnvPalette p) {
    Element tree = describeWith(p);
    ASSERT_FALSE(
        core::environment::bound<EnvPalette>());  // the binding is gone by here
    host.composer.render(tree);
  };

  renderWith(EnvPalette{{0, 0, 1, 1}, {}});
  host.frame();
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLUE);  // the captured stack reached fn

  renderWith(
      EnvPalette{{0, 0, 1, 1}, {}});  // same properties, EQUAL environment
  EXPECT_EQ(describeCalls, 1);
  EXPECT_EQ(host.composer.stats().memoHits, 1u);

  renderWith(
      EnvPalette{{0, 1, 0, 1}, {}});  // same properties, environment moved
  EXPECT_EQ(describeCalls, 2);
  EXPECT_EQ(host.composer.stats().memoHits, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorGREEN);  // not the stale blue
}

TEST(ComposeEnv, InnerProvideShadowsAndUnwinds) {
  struct EnvOther {
    int v = 0;
    bool operator==(const EnvOther&) const = default;
  };
  core::environment::Provide<EnvPalette> outer(EnvPalette{{1, 0, 0, 1}, {}});
  ASSERT_TRUE(core::environment::bound<EnvPalette>());
  EXPECT_TRUE(core::environment::inherited<EnvPalette>()->surface ==
              SkColor4f({1, 0, 0, 1}));
  {
    core::environment::Provide<EnvPalette> inner(EnvPalette{{0, 0, 1, 1}, {}});
    core::environment::Provide<EnvOther> other(EnvOther{7});
    EXPECT_TRUE(core::environment::inherited<EnvPalette>()->surface ==
                SkColor4f({0, 0, 1, 1}));
    EXPECT_EQ(core::environment::inherited<EnvOther>()->v,
              7);  // keyed by TYPE, no crosstalk
  }
  EXPECT_TRUE(core::environment::inherited<EnvPalette>()->surface ==
              SkColor4f({1, 0, 0, 1}));
  EXPECT_FALSE(core::environment::bound<EnvOther>());
}

TEST(ComposeEnv, OutOfOrderDestructionCannotUnbindASibling) {
  // LIFO nesting is the contract, and violating it must be detected in
  // every build: a destructor that popped the top unconditionally would
  // remove a SIBLING's binding when scopes die out of order, corrupting an
  // environment the sibling still believes it provides. Heap providers
  // force the wrong order deliberately.
  auto outer = std::make_unique<core::environment::Provide<EnvPalette>>(
      EnvPalette{{1, 0, 0, 1}, {}});
  auto inner = std::make_unique<core::environment::Provide<EnvPalette>>(
      EnvPalette{{0, 0, 1, 1}, {}});
  ::testing::internal::CaptureStderr();
  outer.reset();  // destroyed FIRST, from under the inner scope
  EXPECT_NE(
      ::testing::internal::GetCapturedStderr().find("environment::Provide"),
      std::string::npos)
      << "the misuse must be loud";
  // The surviving scope's binding still resolves — the misused destructor
  // removed its own entry, not the top of the stack.
  const EnvPalette* survivor = core::environment::inherited<EnvPalette>();
  ASSERT_NE(survivor, nullptr);
  EXPECT_TRUE(survivor->surface == SkColor4f({0, 0, 1, 1}));
  // The inner scope's own destruction is now below its recorded depth, so
  // it too takes the identity path; the stack still fully unwinds.
  ::testing::internal::CaptureStderr();
  inner.reset();
  (void)::testing::internal::GetCapturedStderr();
  EXPECT_FALSE(core::environment::bound<EnvPalette>());
}

TEST(ComposeEnv, ALibraryComponentReadsTheEnvironmentByItsOwnPropsType) {
  // The entry's actual complaint: a library component had to be handed its
  // colours by whoever composed it. The environment key is feed::TextOptions —
  // the component's OWN properties type — so no library-wide Theme exists or
  // needs to.
  feed::TextRing ring;
  ring.append({u8"ready."});

  feed::TextOptions themed;
  themed.styles.base(whiteStyle(12));
  themed.window.gap = 7.0f;

  Element tree = [&] {
    core::environment::Provide<feed::TextOptions> style(themed);
    return box().padding(4).child(box().child(feed::feed(ring)));
  }();
  ASSERT_FALSE(core::environment::bound<feed::TextOptions>());

  Host host;
  host.composer.render(tree);
  host.frame();
  const SkRect got = require(host.composer.bounds(feed::rowKey(1)));
  EXPECT_GT(got.width(), 0.0f);

  // The unbound spelling still compiles to the component's own default —
  // and a DIFFERENT default, which is what proves the binding was read.
  Host bare;
  bare.composer.render(box().padding(4).child(
      box().child(feed::feed(ring, feed::TextOptions{}))));
  bare.frame();
  EXPECT_NE(require(bare.composer.bounds(feed::rowKey(1))).width(),
            got.width());
}

// ---------------------------------------------------------------------------
// wiggle() and the reconciler — the prune behaviour of BoundFloat's noise
// stage.

TEST(ComposeReconcile, WiggledBindingsPruneOnlyWhenEveryParameterMatches) {
  // THE TRAP, pinned. `boundMapEqual()` in Reconcile.cpp compares BoundFloat
  // FIELD BY FIELD, and a field left out of that list fails INVISIBLY: two
  // different wiggles compare equal, the node prunes, and the instance keeps
  // applying the OLD map forever while every other test still passes. So
  // each of the five wiggle fields gets its own re-describe here.
  //
  // If this test fails, do not relax it — a field is missing from
  // boundMapEqual().
  static choreograph::Output<float> phase;
  phase = 0.35f;
  struct Rig {
    float amount = 12.0f;
    float frequency = 7.0f;
    uint32_t seed = 1;
    int octaves = 1;
    float falloff = 0.5f;
  };
  auto tree = [](Rig r) {
    return box().child(
        box().key("shaken").width(40).height(40).fill(red()).translateX(
            motion::bind(&phase)
                .target(-70.0f, 170.0f)
                .wiggle(r.amount, r.frequency, r.seed, r.octaves, r.falloff)));
  };

  Host host;
  host.composer.render(tree({}));
  host.frame();

  // The prune half: an identical re-describe costs nothing.
  host.composer.render(tree({}));
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an identical wiggle must still prune";
  EXPECT_FALSE(host.composer.dirty());

  // The over-prune half, one field at a time. Each of these is a DIFFERENT
  // wiggle and must reach the instance.
  const Rig moved[] = {
      {.amount = 20.0f}, {.frequency = 11.0f}, {.seed = 2},
      {.octaves = 3},    {.falloff = 0.9f},
  };
  const char* named[] = {"amount", "frequency", "seed", "octaves", "falloff"};
  for (size_t i = 0; i < std::size(moved); ++i) {
    host.composer.render(tree({}));  // back to the baseline rig
    host.frame();
    host.composer.render(tree(moved[i]));
    EXPECT_GT(host.composer.stats().patchedNodes, 0u)
        << named[i]
        << " changed and the node PRUNED — boundMapEqual() is "
           "missing that field";
    EXPECT_TRUE(host.composer.dirty()) << named[i];
  }
}

TEST(ComposeReconcile, TwoSeedsShakeIndependentlyOnScreen) {
  // The whole path in PIXELS: two seeds, two marks, and the displacement
  // the paint actually produced. (Not a prune pin — keyed siblings never
  // prune into one another; the prune pin is the test above. This one
  // proves the seed survives Element → reconciler → paint, which is what
  // makes a two-axis shake possible instead of a diagonal slide.)
  static choreograph::Output<float> t;
  t = 0.0f;
  Host host(200, 200);
  auto tree = [] {
    // stack(): both marks lay out at the origin, so each row below is
    // unambiguously one of them.
    return stack()
        .child(box()
                   .key("x")
                   .width(8)
                   .height(8)
                   .fill(red())
                   .translateX(
                       sigil::motion::wiggle(&t, 40.0f, 3.0f, 1).offset(100.0f))
                   .translateY(30.0f))
        .child(box()
                   .key("y")
                   .width(8)
                   .height(8)
                   .fill(green())
                   .translateX(
                       sigil::motion::wiggle(&t, 40.0f, 3.0f, 2).offset(100.0f))
                   .translateY(90.0f));
  };
  host.composer.render(tree());

  // bounds() is the LAYOUT rect and a translate is paint-only, so the
  // observation has to be pixels: where the mark actually landed.
  const auto centerOf = [&host](int row, SkColor want) {
    int lo = -1, hi = -1;
    for (int x = 0; x < 200; ++x)
      if (host.pixel(x, row) == want) {
        if (lo < 0) lo = x;
        hi = x;
      }
    return lo < 0 ? -1.0f : 0.5f * (float)(lo + hi);
  };

  bool everApart = false, xMoved = false, yMoved = false;
  float firstX = -1, firstY = -1;
  for (int frame = 0; frame < 90; ++frame) {
    t = (float)frame / 30.0f;
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
