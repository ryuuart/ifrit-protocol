// The words a node's change is spelled with: the motion ramps and their
// entrances, the one word that declares volatility, and the shape that
// overrides a node's rect.

#include <sigilmaterial/skia/Paint.h>
#include "support/BrushTestSupport.h"

TEST(ComposeShapeRename, ShapeOverridesTheBox) {
  // `shape()` replaces the node's rect with a generated path — for fill,
  // for stroking, and for hit testing. The rect it was given is not
  // intersected with the shape, it is discarded.
  Host host(200, 200);
  Element e = box().rect(20, 20, 100, 100).fill(red());
  e.shape(geometry::shapes::circle());
  host.composer.render(stack().children({std::move(e)}));
  host.frame();
  EXPECT_EQ(host.pixel(70, 70), SK_ColorRED) << "inside the circle";
  EXPECT_EQ(host.pixel(24, 24), SK_ColorBLACK)
      << "the rect's corner is outside the shape";
}

// ---------------------------------------------------------------------------
// The authoring spellings, each checked against the mechanism it is sugar
// for. These say what a word MEANS — animate({.to = }) ramps where a bare value
// snaps, a named .from is a mount entrance, a bound source/target pair is
// two explicit stages — because the words are close enough that a wrong one
// produces a plausible picture rather than an error.

// ---- animate({.to = v, …}) --------------------------------------------------
// ----------------------------------------------
TEST(ComposeMotionWords, AnimateToIsTheChangeRamp) {
  // Read the property MID-RAMP, which is the only place a snap and a ramp
  // differ — both agree at the endpoints. The second arm describes the same
  // value with no animate() at all and must snap, which is what makes the
  // first arm's reading meaningful.
  auto run = [](bool plain) {
    Host host(200, 200);
    auto describe = [&](float opacity) {
      Element inner = box().width(100).height(100).fill(red());
      if (plain)
        inner.opacity(opacity);
      else
        inner.opacity(motion::animate({.to = opacity, .duration = 200ms}));
      return stack().children({std::move(inner)});
    };
    host.composer.render(describe(1.0f));
    host.frame();
    host.composer.render(describe(0.0f));  // the CHANGE
    host.frame(0.1);                       // half way down the ramp
    return host.pixel(50, 50);
  };
  const SkColor ramped = run(false);
  const SkColor snapped = run(true);
  EXPECT_NE(ramped, snapped) << "animate({.to = v}) must ramp where a bare "
                                "value snaps";
  // …and it is genuinely mid-ramp, not "already gone" or "not started".
  EXPECT_GT((int)SkColorGetR(ramped), 20);
  EXPECT_LT((int)SkColorGetR(ramped), 235);
  EXPECT_EQ((int)SkColorGetR(snapped), 0) << "the bare value is already there";
}

TEST(ComposeMotionWords, ToAloneHasNoEntranceAndFromToDoes) {
  // The whole distinction between the two, as pixels: a tween with `.to`
  // alone mounts already holding its value, and one that names `.from`
  // plays a path on first appearance.
  auto mountedOpacity = [](bool withEntrance) {
    Host host(200, 200);
    Element inner = box().width(100).height(100).fill(red());
    if (withEntrance)
      inner.opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms}));
    else
      inner.opacity(motion::animate({.to = 1.0f, .duration = 400ms}));
    host.composer.render(stack().children({std::move(inner)}));
    host.frame(0.001);
    return (int)SkColorGetR(host.pixel(50, 50));
  };
  EXPECT_GT(mountedOpacity(false), 240) << ".to alone must not fade in";
  EXPECT_LT(mountedOpacity(true), 60)
      << "a named .from is a mount entrance";
}

namespace {

/** A scheme that declares its volatility with the one recognised word.
 *  There is exactly one spelling the concept duck-types on. */
struct SaysAnimated {
  bool live = true;
  bool isRunning() const { return live; }
  void paint(SkCanvas& c, const PaintContext&) const {
    SkPaint p;
    p.setColor4f({1, 0, 0, 1}, nullptr);
    c.drawRect(SkRect::MakeWH(40, 40), p);
  }
  bool operator==(const SaysAnimated&) const = default;
};

/** The same scheme spelling a NEAR-MISS of that word. It must not satisfy
 *  the concept: duck typing means a scheme spelling it wrongly is read as
 *  static, its node is cached, and it stops animating with no diagnostic —
 *  which is why exactly one spelling is recognised and this case exists. */
struct SaysTheDeadWord {
  bool live = true;
  bool animates() const { return live; }
  void paint(SkCanvas&, const PaintContext&) const {}
  bool operator==(const SaysTheDeadWord&) const = default;
};

}  // namespace

TEST(ComposeVolatility, IsAnimatedIsTheOnlyWordThatDeclaresIt) {
  static_assert(AnimatedDecoration<SaysAnimated>);
  static_assert(!AnimatedDecoration<SaysTheDeadWord>,
                "only isRunning() declares volatility");
  EXPECT_TRUE(Decoration(SaysAnimated{true}).isRunning());
  EXPECT_FALSE(Decoration(SaysAnimated{false}).isRunning());
  // A near-miss spelling declares nothing: it wraps, it paints, it is static.
  EXPECT_FALSE(Decoration(SaysTheDeadWord{true}).isRunning());
}

TEST(ComposeVolatility, EveryLibrarySchemeDeclaresItWithTheSameWord) {
  lines::Line line;
  motion::Animatable<float> phase = motion::animatable(0.0f);
  line.dashPhaseBinding = phase;
  EXPECT_TRUE(line.isRunning());

  PathFormat pf;
  EXPECT_FALSE(pf.isRunning());

  // A Material answers the same question in the same word as every other
  // scheme, so a consumer never has to know which kind it is holding.
  const material::Paint stat = material::Paint::solid({1, 0, 0, 1});
  EXPECT_FALSE(stat.isRunning());
}

// ---- Binding::from / ::to --------------------------------------------------
TEST(ComposeMotionWords, ToPutsTheSourceRangeOntoTheOutputWithoutClamping) {
  const motion::Binding mapping{.from = {0, 100}, .to = {-70.0f, 170.0f}};
  EXPECT_FLOAT_EQ(mapping.apply(0.0f), -70.0f);
  EXPECT_FLOAT_EQ(mapping.apply(25.0f), -10.0f);
  EXPECT_FLOAT_EQ(mapping.apply(100.0f), 170.0f);
  // Outside the source range the line goes on: neither end clamps.
  EXPECT_NEAR(mapping.apply(137.0f), 258.8f, 1e-3f);

  // A bound value reads the live one through the same stages.
  motion::Animatable<float> hitPoints = motion::animatable(0.0f);
  hitPoints = 25.0f;
  EXPECT_FLOAT_EQ(motion::bind(hitPoints, mapping).value(), -10.0f);
}

TEST(ComposeMotionWords, ClampFromIsTheSourceRangeThatClamps) {
  const motion::Binding clamped{.from = {0.2f, 0.4f}, .clampFrom = true};
  const motion::Binding open{.from = {0.2f, 0.4f}};
  EXPECT_FLOAT_EQ(clamped.apply(0.3f), open.apply(0.3f));
  EXPECT_FLOAT_EQ(clamped.apply(0.9f), 1.0f) << "clampFrom clamps";
  EXPECT_GT(open.apply(0.9f), 1.0f) << "from alone does not";
}

TEST(ComposeVolatility, ALiveMaterialOnASpanPassDeclaresItself) {
  // spanVolatile reads the PASS BRUSH's isRunning(), which means a live
  // MATERIAL on a span pass must declare itself just as a bound endpoint
  // does: a stroke whose colour comes from a uTime shader has to repaint
  // every frame with no re-describe anywhere. The static arm is the other
  // half — declaring volatility unconditionally would be equally wrong.
  auto paintedPerFrame = [](bool live) {
    Host host(200, 200);
    PathFormat mark = stroke(8, red());
    mark.strokeFill = material::skia::sksl(heavyEffect(live));
    host.composer.render(stack().children(
        {revealBox().stroke(spans::upTo(0.6f), std::move(mark))}));
    host.frame();
    host.frame();  // no re-describe: only declared volatility can paint now
    return host.composer.stats().nodesPainted;
  };
  EXPECT_GT(paintedPerFrame(true), 0u)
      << "a live stroke material on a span pass must declare isRunning()";
  EXPECT_EQ(paintedPerFrame(false), 0u) << "…and a static one must still cache";
}
