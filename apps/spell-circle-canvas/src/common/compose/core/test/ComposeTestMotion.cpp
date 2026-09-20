// Values that move: a transition ramping and retargeting from where it
// stands, an unmount cancelling what it drove, a binding driving paint with
// no re-describe and waking again after it settled, an ease adapter bound to
// a shape parameter, a plain snap landing after a transition, and travel as a
// fraction of arc length across every contour — and, beside them, what
// a binding shapes on its way to a property and what an aggregate's
// empty curve means, and the value each shape of the authoring grammar
// builds.

#include "support/CoreTestSupport.h"

TEST(ComposeTransitions, RampsAndRetargetsFromCurrent) {
  Host host;
  auto at = [&](float target) {
    return box().children(
        {box().key("m").width(50).height(50).fill(red()).translateX(animate(
            sigil::motion::to(target), {400ms, &choreograph::easeNone}))});
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(at(100.0f));            // start ramp 0 → 100
  host.frame(0.2);                             // half way (linear ease)
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED);  // box around x=50..100
  EXPECT_EQ(host.pixel(10, 25), SK_ColorBLACK);

  host.composer.render(at(0.0f));  // retarget back from ~50
  host.frame(0.2);                 // halfway back → ~25
  EXPECT_EQ(host.pixel(45, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(90, 25), SK_ColorBLACK);

  host.frame(1.0);  // settle
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_FALSE(host.ticker.active());  // motion removed on finish
}

TEST(ComposeTransitions, AppearIsTheMountEntranceWrittenOnce) {
  // The one sentence a card, a panel and a pass all say as they arrive,
  // and the longhand it stands for: the two draw the same frames.
  const auto plate = [](bool longhand) {
    Element node = box().key("card").width(50).height(50).fill(red());
    if (longhand)
      node.opacity(animate(sigil::motion::from(0.0f).to(1.0f),
                           {400ms, &choreograph::easeNone}));
    else
      node.appear({400ms, &choreograph::easeNone});
    return box().children({std::move(node)});
  };
  Host writ;
  Host said;
  writ.composer.render(plate(true));
  said.composer.render(plate(false));
  writ.frame(0.2);
  said.frame(0.2);
  // Half way through a linear entrance, and the same half way.
  const SkColor half = said.pixel(25, 25);
  EXPECT_EQ(half, writ.pixel(25, 25));
  EXPECT_NE(half, SK_ColorRED);
  EXPECT_NE(half, SK_ColorBLACK);
  writ.frame(1.0);
  said.frame(1.0);
  EXPECT_EQ(said.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(writ.pixel(25, 25), SK_ColorRED);
}

TEST(ComposeTransitions, UnmountCancelsMotions) {
  Host host;
  host.composer.render(
      box().children({box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(500.0f), {1000ms}))}));
  host.frame();
  host.composer.render(
      box().children({box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(0.0f), {1000ms}))}));
  host.frame(0.1);
  EXPECT_TRUE(host.ticker.active());
  host.composer.render(box());  // unmount mid-flight
  host.frame(0.1);              // stepping must not touch dead outputs
  EXPECT_FALSE(host.ticker.active());
}

TEST(ComposeBindings, OutputDrivesPaintWithoutRender) {
  Host host;
  choreograph::Output<float> x = 0.0f;
  host.composer.render(
      box().children({box().width(40).height(40).fill(blue()).translateX(&x)}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLUE);

  x = 120.0f;  // direct mutation, no render()
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(140, 20), SK_ColorBLUE);
}

TEST(ComposeBindings, ActiveWakesForABindingThatSettledAndMovedAgain) {
  // The gate a host draws on. `dirty()` answers the describe and the
  // layout; `active()` also answers whether another draw could produce
  // different pixels — and the case that separates them is an externally
  // driven binding that has STOPPED moving. The volatility walk releases
  // such a node once it has held still, and nothing reconciles when the
  // host assigns the output again, so a host polling `dirty()` alone
  // would sleep through the change.
  Host host;
  choreograph::Output<Fill> bar{Fill::color({1, 0, 0, 1})};
  host.composer.render(box().children(
      {box().absolute().left(20).top(20).width(60).height(60).fill(&bar)}));
  host.frame();

  // Held still long enough for the walk to release the binding.
  for (int frame = 0; frame < 12; ++frame) host.frame();
  EXPECT_FALSE(host.composer.dirty());
  EXPECT_FALSE(host.composer.active()) << "a settled scene has nothing to draw";

  bar = Fill::color({0, 1, 0, 1});  // the host wrote the output itself
  EXPECT_FALSE(host.composer.dirty()) << "no describe and no layout ran";
  EXPECT_TRUE(host.composer.active()) << "…and the next draw is a new colour";

  host.frame();
  EXPECT_GT(SkColorGetG(host.pixel(50, 50)), 180u);
}

TEST(ComposeMotion, EaseAdaptersBindTheShapeParameter) {
  // choreograph's back/elastic/bounce take a shape parameter with a
  // default, so &choreograph::easeOutBack does not convert to an EaseFn.
  // These adapters bind it — and outBack must actually OVERSHOOT, which
  // is the only reason to reach for it.
  const choreograph::EaseFn back = motion::ease::outBack();
  float peak = 0.0f;
  for (int i = 0; i <= 100; ++i) peak = std::max(peak, back((float)i / 100.0f));
  EXPECT_GT(peak, 1.05f) << "outBack did not overshoot";
  EXPECT_NEAR(back(0.0f), 0.0f, 1e-4f);
  EXPECT_NEAR(back(1.0f), 1.0f, 1e-4f);
}

TEST(ComposeTransitions, PlainSnapAfterTransitionLands) {
  // Describing a PLAIN value after a transition must land immediately. A
  // running ramp resolves ahead of the described value, so unless the snap
  // disconnects it the ramp shadows the description for as long as it lives
  // — and a settled ramp holds its target forever.
  Host host;
  auto at = [](sigil::motion::Animatable<float> x) {
    return box().children(
        {box().key("m").width(50).height(50).fill(red()).translateX(
            std::move(x))});
  };
  host.composer.render(at(0.0f));
  host.frame();
  host.composer.render(
      at(animate(sigil::motion::to(100.0f), {400ms, &choreograph::easeNone})));
  host.frame(0.2);  // mid-ramp, box around x=50..100
  EXPECT_EQ(host.pixel(75, 25), SK_ColorRED);
  host.composer.render(at(0.0f));  // PLAIN: must snap home
  host.frame();
  EXPECT_EQ(host.pixel(25, 25), SK_ColorRED);
  EXPECT_EQ(host.pixel(75, 25), SK_ColorBLACK);  // not stuck mid-ramp
}

namespace {

/** The centroid of every pixel of @p color, or (-1,-1) when none. Motion is
 *  a VISUAL feature: these pins scan the frame, they do not read floats out
 *  of the resolver. */
SkPoint inkCentroid(Host& host, SkColor color, int w, int h) {
  double sx = 0, sy = 0;
  int n = 0;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (host.pixel(x, y) == color) {
        sx += x;
        sy += y;
        ++n;
      }
  if (n == 0) return {-1, -1};
  return {(float)(sx / n), (float)(sy / n)};
}

/** The bounding box of everything even faintly @p color-ish — enough to
 *  ask "is this bar lying flat or standing up", which is what an
 *  orientation pin actually wants to know. */
SkIRect inkBounds(Host& host, int w, int h) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 100) {
        const SkIRect one = SkIRect::MakeXYWH(x, y, 1, 1);
        if (box.isEmpty())
          box = one;
        else
          box.join(one);
      }
  return box;
}

/** A 160x160 frame inset at (20,20) of a 200x200 canvas, carrying one small
 *  square that travels. The inscribed circle is then centre (100,100) r=80
 *  in CANVAS coordinates, so every quadrant point is on-screen. */
Element travelFrame(Element rider) {
  return box().children({box()
                             .key("frame")
                             .absolute()
                             .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                             .children({std::move(rider)})});
}

Element rider(MotionPath along, float size = 8) {
  return box()
      .key("dot")
      .absolute()
      .rect(SkRect::MakeXYWH(0, 0, size, size))
      .fill(red())
      .travel(std::move(along));
}

}  // namespace

TEST(ComposeTravel, TIsAFractionOfTotalArcLengthAcrossEveryContour) {
  // An L with legs of 100 and 20: half the LENGTH is 60 px along the long
  // leg. Anything parameter-flavoured (per verb, per contour) lands at the
  // bend instead.
  Host host(200, 200);
  choreograph::Output<float> t{0.5f};
  const auto bent = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(100, 0);
    b.lineTo(100, 20);
    return b.detach();
  };
  host.composer.render(travelFrame(rider({.path = bent, .t = &t})));
  host.frame();
  SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 80.0f, 1.5f) << "t=0.5 is not 60 px along a 120 px L";
  EXPECT_NEAR(ink.y(), 20.0f, 1.5f);

  // Two contours of 20 and 100: half the total (60) is 40 into the SECOND,
  // which no per-contour split can produce.
  const auto twoRuns = [](SkSize) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(20, 0);
    b.moveTo(0, 100);
    b.lineTo(100, 100);
    return b.detach();
  };
  host.composer.render(travelFrame(rider({.path = twoRuns, .t = &t})));
  host.frame();
  ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 60.0f, 1.5f)
      << "the contours are not concatenated by LENGTH";
  EXPECT_NEAR(ink.y(), 120.0f, 1.5f);
}

TEST(ComposeTravel, APathWithNoMeasurableLengthLeavesTheLanesStanding) {
  Host host(200, 200);
  host.composer.render(travelFrame(
      rider({.path = [] { return SkPath(); }, .t = 0.5f}).translateX(40)));
  host.frame();
  const SkPoint ink = inkCentroid(host, SK_ColorRED, 200, 200);
  EXPECT_NEAR(ink.x(), 63.5f, 1.5f)
      << "an empty path engaged anyway and swallowed the translate lane";
  EXPECT_NEAR(ink.y(), 23.5f, 1.5f);
}

TEST(ComposeTravel, PerAxisScaleParticipatesInReconcilerEquality) {
  // Per-axis scale has to reach propertiesEqual like every other paint field.
  // Left out, two descriptions differing only in scaleX compare EQUAL: the
  // patch prunes, the node is never marked paint-dirty, and the old picture
  // replays at the old scale — a wrong picture with no failure anywhere.
  Host host(200, 200);
  const auto bar = [](float sx) {
    return box().children({box()
                               .key("bar")
                               .absolute()
                               .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                               .transformOrigin(pct(0), pct(0))
                               .fill(red())
                               .scaleX(sx)});
  };
  host.composer.render(bar(1.0f));
  host.frame();
  EXPECT_EQ(host.pixel(60, 20), SK_ColorBLACK);

  host.composer.render(bar(1.0f));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u)
      << "an unchanged scaleX patched — the counter is measuring something "
         "else";

  host.composer.render(bar(2.0f));
  host.frame();
  EXPECT_EQ(host.composer.stats().patchedNodes, 1u)
      << "a CHANGED scaleX pruned into the old description";
  EXPECT_EQ(host.pixel(60, 20), SK_ColorRED)
      << "…and the stale picture replayed";

  host.composer.render(
      bar(2.0f).children({box()
                              .key("y")
                              .absolute()
                              .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                              .transformOrigin(pct(0), pct(0))
                              .fill(green())
                              .scaleY(1.0f)}));
  host.frame();
  host.composer.render(
      bar(2.0f).children({box()
                              .key("y")
                              .absolute()
                              .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                              .transformOrigin(pct(0), pct(0))
                              .fill(green())
                              .scaleY(2.0f)}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 130), SK_ColorGREEN)
      << "a CHANGED scaleY pruned into the old description";
}

// -------------------------------------------------------------------------
// What a binding shapes on its way to a property, and the curve an
// aggregate leaves empty.

TEST(ComposeBindings, AShapedBindingDrivesThePropertyInPixels) {
  // One Output, two units. A phase in [0,1] is what a reveal or an opacity
  // wants; a translation wants PIXELS. Without a shaping map on the binding,
  // driving both from one motion means carrying a second Output updated
  // alongside the first — two things to keep in step for no reason.
  Host host(200, 200);
  choreograph::Output<float> phase{0.0f};
  host.composer.render(
      box().children({box()
                          .width(20)
                          .height(20)
                          .absolute()
                          .left(0)
                          .top(90)
                          .fill(red())
                          .translateX(motion::bind(&phase).target(0, 160))}));
  auto redAt = [&](int x) { return SkColorGetR(host.pixel(x, 100)) > 180; };

  host.frame();
  EXPECT_TRUE(redAt(10));  // phase 0 → x = 0
  EXPECT_FALSE(redAt(170));

  phase = 1.0f;
  host.frame();
  EXPECT_FALSE(redAt(10));
  EXPECT_TRUE(redAt(170));  // phase 1 → x = 160, unscaled would be x = 1

  phase = 0.5f;
  host.frame();
  EXPECT_TRUE(redAt(85));  // and it is linear in between
}

TEST(ComposeBindings, AChangedShapeRepatchesRatherThanPruning) {
  // The map is read LIVE through the pointer, so a pruned node would keep
  // shaping through the OLD one forever. Same Output, different range.
  Host host(200, 200);
  choreograph::Output<float> phase{1.0f};
  auto tree = [&](float far) {
    return box().children(
        {box()
             .key("dot")
             .width(20)
             .height(20)
             .absolute()
             .left(0)
             .top(90)
             .fill(red())
             .translateX(motion::bind(&phase).target(0, far))});
  };
  host.composer.render(tree(40.0f));
  host.frame();
  EXPECT_TRUE(SkColorGetR(host.pixel(50, 100)) > 180);

  host.composer.render(tree(150.0f));
  host.frame();
  EXPECT_FALSE(SkColorGetR(host.pixel(50, 100)) > 180);
  EXPECT_TRUE(SkColorGetR(host.pixel(160, 100)) > 180);
}

TEST(ComposeBindings, AFillCanBeBoundLive) {
  // A Fill can be bound, which is easy to miss and expensive to work
  // around — the alternative is rebuilding the widget on renderSlot().
  // The Output holds a Fill, and you write it from the
  // same steppable that computes the number driving everything else.
  Host host(200, 200);
  choreograph::Output<Fill> bar{Fill::color({1, 0, 0, 1})};
  host.composer.render(box().children(
      {box().absolute().left(20).top(80).width(160).height(40).fill(&bar)}));
  host.frame();
  EXPECT_GT(SkColorGetR(host.pixel(100, 100)), 180);
  EXPECT_LT(SkColorGetG(host.pixel(100, 100)), 80);

  bar = Fill::color({0, 1, 0, 1});  // no re-render, no re-describe
  host.frame();
  EXPECT_LT(SkColorGetR(host.pixel(100, 100)), 80);
  EXPECT_GT(SkColorGetG(host.pixel(100, 100)), 180);
}

TEST(ComposeMotion, AnEmptyEasingMeansTheDefaultRatherThanACrash) {
  // motion::Transition is an aggregate, so `{360ms, {}, 220ms}` — the obvious
  // way to write "default curve, but I need to name the delay" — initialises
  // `ease` to an EMPTY std::function. It compiles, so the only options are
  // throwing bad_function_call on the first frame or treating empty as "the
  // default curve". It is the latter.
  Host host(200, 200);
  host.composer.render(
      box().children({box()
                          .width(40)
                          .height(40)
                          .absolute()
                          .left(0)
                          .top(80)
                          .fill(red())
                          .translateX(animate(motion::from(0.0f).to(120.0f),
                                              {200ms, {}, 0ms}))}));
  host.frame();     // would throw here
  host.frame(0.4);  // land the entrance
  EXPECT_TRUE(SkColorGetR(host.pixel(130, 100)) > 180);
}

// -------------------------------------------------------------------------
// The authoring grammar: animate(motion::from(a).to(b)) /
// animate(through({...})). What is pinned is the VALUE each argument
// shape builds, because that value is the only thing the engine ever
// sees — the argument spellings are pure sugar over it.

TEST(ComposeMotion, EachArgumentShapeBuildsItsOwnTransitioned) {
  const sigil::motion::Transition spec{200ms, &choreograph::easeNone, 40ms};

  const sigil::motion::Transitioned<float> ramp =
      animate(sigil::motion::to(1.0f), spec);
  EXPECT_EQ(ramp.value, 1.0f);
  EXPECT_FALSE(ramp.from.has_value()) << "to() alone is not an entrance";
  EXPECT_TRUE(ramp.waypoints.empty());
  EXPECT_EQ(ramp.spec.duration, 200ms);
  EXPECT_EQ(ramp.spec.delay, 40ms);

  const sigil::motion::Transitioned<float> entrance =
      animate(motion::from(0.0f).to(1.0f), spec);
  EXPECT_EQ(entrance.value, 1.0f);
  ASSERT_TRUE(entrance.from.has_value());
  EXPECT_EQ(*entrance.from, 0.0f);
  EXPECT_TRUE(entrance.waypoints.empty());
  EXPECT_EQ(entrance.spec.duration, 200ms);
  EXPECT_EQ(entrance.spec.delay, 40ms);
  EXPECT_FLOAT_EQ(entrance.spec.easing()(0.25f), 0.25f);

  const std::vector<std::pair<std::chrono::milliseconds, float>> path{
      {0ms, 40.0f}, {200ms, -20.0f}, {400ms, 0.0f}};
  const sigil::motion::Transitioned<float> phrasedPath =
      animate(sigil::motion::through(path), &choreograph::easeNone);
  EXPECT_EQ(phrasedPath.value, 0.0f);
  ASSERT_TRUE(phrasedPath.from.has_value());
  EXPECT_EQ(*phrasedPath.from, 40.0f);
  EXPECT_EQ(phrasedPath.waypoints, path);
  EXPECT_EQ(phrasedPath.spec.duration, 400ms);
  // The ease is the one field the waypoint overload writes itself —
  // dropping it would default to easeOutQuad silently.
  EXPECT_FLOAT_EQ(phrasedPath.spec.easing()(0.25f), 0.25f);
}

// A guard, not a reproduction: an indeterminate value can happen to hold the
// number this test wants, so a passing run is weaker evidence than usual.
// Both spellings are checked, and the pixel arm at the bottom is what makes
// the claim about behaviour rather than about one struct field.
TEST(ComposeMotion, AnEmptyKeyframePathIsDETERMINATE) {
  // An empty waypoint list is a degenerate ask that must still produce a
  // definite answer. `Transitioned<T>::value` has to be value-initialized:
  // default-initialized, `animate(through({}))` would leave a float property
  // reading whatever was on the stack — once, silently, with no failure to
  // observe anywhere. Zero is the answer.
  const sigil::motion::Transitioned<float> empty =
      animate(sigil::motion::through({}));
  EXPECT_EQ(empty.value, 0.0f);
  EXPECT_FALSE(empty.from.has_value());
  EXPECT_TRUE(empty.waypoints.empty());

  const std::vector<std::pair<std::chrono::milliseconds, float>> none;
  const sigil::motion::Transitioned<float> phrased =
      animate(sigil::motion::through(none));
  EXPECT_EQ(phrased.value, 0.0f);

  // And through the property slot: the node paints AT that determinate
  // value rather than at a number nobody chose.
  Host host;
  host.composer.render(
      box().children({box().width(80).height(80).fill(red()).opacity(
          animate(sigil::motion::through({})))}));
  host.frame();
  EXPECT_EQ(host.pixel(20, 20), SK_ColorBLACK);  // opacity 0, not garbage
}

TEST(ComposeMotion, AnimateThroughDeducesAFloatPath) {
  // A nested braced list is a non-deduced context, so the generic form
  // normally has to be told `<float>`. This overload exists so it does not.
  // Compiling with no explicit template argument IS the test — the
  // assertions below only confirm it deduced the right thing.
  const sigil::motion::Transitioned<float> t =
      animate(sigil::motion::through({{0ms, 0.0f}, {100ms, 1.0f}}));
  ASSERT_EQ(t.waypoints.size(), 2u);
  EXPECT_EQ(t.waypoints.front().second, 0.0f);
  EXPECT_EQ(t.waypoints.back().second, 1.0f);
  ASSERT_TRUE(t.from.has_value());
  EXPECT_EQ(*t.from, 0.0f);
  EXPECT_EQ(t.value, 1.0f);
  EXPECT_EQ(t.spec.duration, 100ms);
}

TEST(ComposeMotion, AnimatePlaysEntranceOnMount) {
  Host host;
  auto tree = [] {
    return box().children(
        {box().width(80).height(80).fill(red()).opacity(animate(
            motion::from(0.0f).to(1.0f), {200ms, &choreograph::easeNone}))});
  };
  host.composer.render(tree());
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorBLACK);  // enters invisible
  host.frame(0.1);                               // half the linear ramp
  const SkColor mid = host.pixel(40, 40);
  EXPECT_GT(SkColorGetR(mid), 90u);
  EXPECT_LT(SkColorGetR(mid), 165u);
  EXPECT_EQ(SkColorGetG(mid), 0u);
  host.frame(0.2);  // settled
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);

  host.composer.render(tree());  // identical re-describe prunes clean
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}

TEST(ComposeMotion, AnimateColorSweepsOnMount) {
  Host host;
  host.composer.render(
      box().children({box().width(80).height(80).fill(motion::Animatable<Fill>(
          animate(motion::from(Fill::color({1, 1, 1, 1})).to(red()),
                  {200ms, &choreograph::easeNone})))}));
  host.frame();
  EXPECT_EQ(host.pixel(40, 40), SK_ColorWHITE);  // the declared "from"
  host.frame(0.3);
  EXPECT_EQ(host.pixel(40, 40), SK_ColorRED);
}
