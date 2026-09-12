// Values that move: a transition ramping and retargeting from where it
// stands, an unmount cancelling what it drove, a binding driving paint with
// no re-describe and waking again after it settled, an ease adapter bound to
// a shape parameter, a plain snap landing after a transition, and travel as a
// fraction of arc length across every contour.

#include "support/CoreTestSupport.h"

TEST(ComposeTransitions, RampsAndRetargetsFromCurrent) {
  Host host;
  auto at = [&](float target) {
    return box().child(
        box().key("m").width(50).height(50).fill(red()).translateX(animate(
            sigil::motion::to(target), {400ms, &choreograph::easeNone})));
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

TEST(ComposeTransitions, UnmountCancelsMotions) {
  Host host;
  host.composer.render(
      box().child(box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(500.0f), {1000ms}))));
  host.frame();
  host.composer.render(
      box().child(box().key("gone").width(10).height(10).translateX(
          animate(sigil::motion::to(0.0f), {1000ms}))));
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
      box().child(box().width(40).height(40).fill(blue()).translateX(&x)));
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
  host.composer.render(box().child(
      box().absolute().left(20).top(20).width(60).height(60).fill(&bar)));
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
    return box().child(
        box().key("m").width(50).height(50).fill(red()).translateX(
            std::move(x)));
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
  return box().child(box()
                         .key("frame")
                         .absolute()
                         .rect(SkRect::MakeXYWH(20, 20, 160, 160))
                         .child(std::move(rider)));
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
  host.composer.render(
      travelFrame(rider({.path = [](SkSize) { return SkPath(); }, .t = 0.5f})
                      .translateX(40)));
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
    return box().child(box()
                           .key("bar")
                           .absolute()
                           .rect(SkRect::MakeXYWH(0, 0, 40, 40))
                           .transformOrigin(0, 0)
                           .fill(red())
                           .scaleX(sx));
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
      bar(2.0f).child(box()
                          .key("y")
                          .absolute()
                          .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                          .transformOrigin(0, 0)
                          .fill(green())
                          .scaleY(1.0f)));
  host.frame();
  host.composer.render(
      bar(2.0f).child(box()
                          .key("y")
                          .absolute()
                          .rect(SkRect::MakeXYWH(0, 60, 40, 40))
                          .transformOrigin(0, 0)
                          .fill(green())
                          .scaleY(2.0f)));
  host.frame();
  EXPECT_EQ(host.pixel(20, 130), SK_ColorGREEN)
      << "a CHANGED scaleY pruned into the old description";
}
