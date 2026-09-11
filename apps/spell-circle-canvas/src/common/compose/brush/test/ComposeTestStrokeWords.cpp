// The words a node's change is spelled with: the motion ramps and their
// entrances, the derive connectors, the one word that declares volatility,
// and the shape that overrides a node's rect.

#include "support/BrushTestSupport.h"

TEST(ComposeShapeRename, ShapeOverridesTheBox) {
  // `shape()` replaces the node's rect with a generated path — for fill,
  // for stroking, and for hit testing. The rect it was given is not
  // intersected with the shape, it is discarded.
  Host host(200, 200);
  Element e = box().rect(SkRect::MakeXYWH(20, 20, 100, 100)).fill(red());
  e.shape(geometry::shapes::circle());
  host.composer.render(stack().child(std::move(e)));
  host.frame();
  EXPECT_EQ(host.pixel(70, 70), SK_ColorRED) << "inside the circle";
  EXPECT_EQ(host.pixel(24, 24), SK_ColorBLACK)
      << "the rect's corner is outside the shape";
}

// ---------------------------------------------------------------------------
// The authoring spellings, each checked against the mechanism it is sugar
// for. These say what a word MEANS — animate(to()) ramps where a bare value
// snaps, motion::from().to() is a mount entrance, a bound source/target pair is
// two explicit stages — because the words are close enough that a wrong one
// produces a plausible picture rather than an error.

// ---- animate(to(v), spec) --------------------------------------------------
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
        inner.opacity(animate(sigil::motion::to(opacity), {200ms}));
      return stack().child(std::move(inner));
    };
    host.composer.render(describe(1.0f));
    host.frame();
    host.composer.render(describe(0.0f));  // the CHANGE
    host.frame(0.1);                       // half way down the ramp
    return host.pixel(50, 50);
  };
  const SkColor ramped = run(false);
  const SkColor snapped = run(true);
  EXPECT_NE(ramped, snapped) << "animate(to(v)) must ramp where a bare "
                                "value snaps";
  // …and it is genuinely mid-ramp, not "already gone" or "not started".
  EXPECT_GT((int)SkColorGetR(ramped), 20);
  EXPECT_LT((int)SkColorGetR(ramped), 235);
  EXPECT_EQ((int)SkColorGetR(snapped), 0) << "the bare value is already there";
}

TEST(ComposeMotionWords, ToAloneHasNoEntranceAndFromToDoes) {
  // The whole distinction between the two, as pixels: to() mounts already
  // holding its value, and motion::from().to() plays a path on first
  // appearance.
  auto mountedOpacity = [](bool withEntrance) {
    Host host(200, 200);
    Element inner = box().width(100).height(100).fill(red());
    if (withEntrance)
      inner.opacity(animate(motion::from(0.0f).to(1.0f), {400ms}));
    else
      inner.opacity(animate(sigil::motion::to(1.0f), {400ms}));
    host.composer.render(stack().child(std::move(inner)));
    host.frame(0.001);
    return (int)SkColorGetR(host.pixel(50, 50));
  };
  EXPECT_GT(mountedOpacity(false), 240) << "to() alone must not fade in";
  EXPECT_LT(mountedOpacity(true), 60)
      << "motion::from().to() is a mount entrance";
}

namespace {

/** A scheme that declares its volatility with the one recognised word.
 *  There is exactly one spelling the concept duck-types on. */
struct SaysAnimated {
  bool live = true;
  bool isAnimated() const { return live; }
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
                "only isAnimated() declares volatility");
  EXPECT_TRUE(Decoration(SaysAnimated{true}).isAnimated());
  EXPECT_FALSE(Decoration(SaysAnimated{false}).isAnimated());
  // A near-miss spelling declares nothing: it wraps, it paints, it is static.
  EXPECT_FALSE(Decoration(SaysTheDeadWord{true}).isAnimated());
}

TEST(ComposeVolatility, EveryLibrarySchemeDeclaresItWithTheSameWord) {
  lines::Line line;
  choreograph::Output<float> phase;
  line.dashPhaseBinding = &phase;
  EXPECT_TRUE(line.isAnimated());

  PathFormat pf;
  EXPECT_FALSE(pf.isAnimated());

  // A Material answers the same question in the same word as every other
  // scheme, so a consumer never has to know which kind it is holding.
  const material::skia::Paint stat = material::skia::Paint::solid({1, 0, 0, 1});
  EXPECT_FALSE(stat.isAnimated());
}

// ---- Bound::source / ::target ----------------------------------------------
// -------------------------------------------
TEST(ComposeMotionWords, TargetIsScaleAndOffsetWrittenAsTwoBounds) {
  choreograph::Output<float> hp;
  hp = 25.0f;
  const sigil::motion::BoundFloat named =
      motion::bind(&hp).source(0, 100).target(-70, 170).value();
  // target(lo, hi) is sugar: the same mapping written as an explicit scale
  // and offset must agree with it everywhere, including outside the source
  // range, since neither form clamps.
  const sigil::motion::BoundFloat manual =
      motion::bind(&hp).source(0, 100).scale(240).offset(-70).value();
  for (float v : {0.0f, 25.0f, 50.0f, 100.0f, 137.0f})
    EXPECT_FLOAT_EQ(named.apply(v), manual.apply(v)) << "at " << v;
  EXPECT_FLOAT_EQ(named.apply(0.0f), -70.0f);
  EXPECT_FLOAT_EQ(named.apply(100.0f), 170.0f);
}

TEST(ComposeMotionWords, WindowIsSourceThatClamps) {
  choreograph::Output<float> t;
  const sigil::motion::BoundFloat w =
      motion::bind(&t).window(0.2f, 0.4f).value();
  const sigil::motion::BoundFloat s =
      motion::bind(&t).source(0.2f, 0.4f).value();
  EXPECT_FLOAT_EQ(w.apply(0.3f), s.apply(0.3f));
  EXPECT_FLOAT_EQ(w.apply(0.9f), 1.0f) << "window clamps";
  EXPECT_GT(s.apply(0.9f), 1.0f) << "source does not";
}

// ---- the derive family -----------------------------------------------------
// --------------------------------------------------
TEST(ComposeDeriveWords, TheQualifiedAndPlainConnectorDrawOnePicture) {
  // Aliases, so the same picture, term for term.
  auto draw = [](bool qualified) {
    Host host(200, 200);
    Element a = box().key("a").rect(SkRect::MakeXYWH(20, 20, 40, 40));
    Element b = box().key("b").rect(SkRect::MakeXYWH(120, 120, 40, 40));
    Element wire =
        qualified ? derive::connector("a", "b") : connector("a", "b");
    wire.absolute().inset(0).foreground(stroke(4, red()));
    host.composer.render(
        stack().child(std::move(a)).child(std::move(b)).child(std::move(wire)));
    host.frame();
    host.frame();  // derive resolves against the first layout
    std::vector<SkColor> out;
    for (int i = 20; i < 160; i += 4) out.push_back(host.pixel(i, i));
    return out;
  };
  const std::vector<SkColor> qualified = draw(true);
  EXPECT_EQ(qualified, draw(false));
  EXPECT_GT(inkedCount(qualified), 10u) << "the wire actually drew";
}

TEST(ComposeDeriveWords, TheFreeFlowAroundVerbIsTheMethod) {
  auto draw = [](bool freeVerb) {
    Host host(300, 200);
    // whiteStyle, not styleAt: the default foreground is BLACK and so is the
    // host's ground, so with the default style both arms would compare two
    // blank grids and agree perfectly. The liveness bound at the end is the
    // second guard against that.
    Element para = text(
        u8"one two three four five six seven eight nine ten "
        u8"eleven twelve thirteen fourteen",
        whiteStyle(16));
    if (freeVerb)
      para = derive::flowAround(std::move(para), "cut", 6.0f);
    else
      para.flowAround("cut", 6.0f);
    host.composer.render(
        stack()
            .child(box().key("cut").rect(SkRect::MakeXYWH(10, 10, 90, 60)))
            .child(box().absolute().inset(0).child(std::move(para))));
    host.frame();
    host.frame();
    std::vector<SkColor> out;
    for (int y = 0; y < 200; y += 3)
      for (int x = 0; x < 300; x += 3) out.push_back(host.pixel(x, y));
    return out;
  };
  const std::vector<SkColor> freeVerb = draw(true);
  EXPECT_EQ(freeVerb, draw(false));
  EXPECT_GT(inkedCount(freeVerb), 20u) << "the paragraph actually drew";
}

TEST(ComposeVolatility, ALiveMaterialOnASpanPassDeclaresItself) {
  // spanVolatile reads the PASS BRUSH's isAnimated(), which means a live
  // MATERIAL on a span pass must declare itself just as a bound endpoint
  // does: a stroke whose colour comes from a uTime shader has to repaint
  // every frame with no re-describe anywhere. The static arm is the other
  // half — declaring volatility unconditionally would be equally wrong.
  auto paintedPerFrame = [](bool live) {
    Host host(200, 200);
    PathFormat mark = stroke(8, red());
    mark.strokeFill = material::skia::Paint::sksl(heavyEffect(live));
    host.composer.render(
        stack().child(revealBox().stroke(spans::upTo(0.6f), std::move(mark))));
    host.frame();
    host.frame();  // no re-describe: only declared volatility can paint now
    return host.composer.stats().nodesPainted;
  };
  EXPECT_GT(paintedPerFrame(true), 0u)
      << "a live stroke material on a span pass must declare isAnimated()";
  EXPECT_EQ(paintedPerFrame(false), 0u) << "…and a static one must still cache";
}
