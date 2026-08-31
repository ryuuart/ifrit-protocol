#include "support/CoreTestSupport.h"

namespace {

/** A 100×100 box at (20,20) whose boundary is the ring `boundaryRing`
 *  samples, dressed with one red stroke. The masking family's fixture. */
Element maskBox() { return box().rect(SkRect::MakeXYWH(20, 20, 100, 100)); }

/** How much red ink is anywhere in a 200×200 host. */
int redInk(Host& host, int x0 = 0, int y0 = 0, int x1 = 200, int y1 = 200) {
  int n = 0;
  for (int y = y0; y < y1; ++y)
    for (int x = x0; x < x1; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 140 &&
          SkColorGetG(host.pixel(x, y)) < 90)
        ++n;
  return n;
}

}  // namespace

// ---- the positioned leaf set --------------------------------------------

TEST(ComposePositioned, RectsAreHonoredAndYogaFree) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(box().key("a").left(10).top(20).width(50).height(30).fill(
              green()))
          .child(box().key("b").left(70).top(90).width(40).height(40).fill(
              Fill::color({1, 0, 0, 1}))));
  host.frame();
  const auto a = host.composer.bounds("a");
  const auto b = host.composer.bounds("b");
  ASSERT_TRUE(a && b);
  EXPECT_EQ(*a, SkRect::MakeXYWH(10, 20, 50, 30));
  EXPECT_EQ(*b, SkRect::MakeXYWH(70, 90, 40, 40));
  EXPECT_EQ(host.pixel(30, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(90, 110), SK_ColorRED);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorBLACK);
  // The whole point: root + container carry Yoga nodes, the leaves do not.
  const Composer::Stats& stats = host.composer.stats();
  EXPECT_EQ(stats.instances, 3u);
  EXPECT_EQ(stats.yogaNodes, 1u);
}

TEST(ComposePositioned, NestedRectsComposeYogaFreeAllTheWayDown) {
  Host host;
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(
              box().key("outer").left(40).top(40).width(100).height(100).child(
                  box().key("inner").left(10).top(20).width(30).height(30))));
  host.frame();
  const auto inner = host.composer.bounds("inner");
  ASSERT_TRUE(inner);
  // bounds() is absolute: outer's origin + inner's own rect.
  EXPECT_EQ(*inner, SkRect::MakeXYWH(50, 60, 30, 30));
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.composer.stats().instances, 3u);
}

TEST(ComposePositioned, PctAndOpposingInsetsResolve) {
  Host host;  // 200x200 canvas; container fills it
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          // pct dims resolve against the container's rect
          .child(
              box().key("half").left(0).top(0).width(pct(50)).height(pct(25)))
          // open width + right inset pins the far edge
          .child(box().key("pinned").left(20).top(100).right(30).height(10)));
  host.frame();
  const auto half = host.composer.bounds("half");
  const auto pinned = host.composer.bounds("pinned");
  ASSERT_TRUE(half && pinned);
  EXPECT_EQ(*half, SkRect::MakeXYWH(0, 0, 100, 50));
  EXPECT_EQ(*pinned, SkRect::MakeXYWH(20, 100, 150, 10));
}

TEST(ComposePositioned, TextMeasuresAgainstItsSuppliedWidth) {
  Host host;
  sigil::weave::TextStyle style;
  style.shaping.fontSize = 14;
  style.paint.foreground.setColor(SK_ColorWHITE);
  host.composer.render(
      positioned()
          .inset(0, 0, 0, 0)
          .child(text(u8"wrap me across a narrow measure", style)
                     .key("t")
                     .left(10)
                     .top(10)
                     .width(90)));
  host.frame();
  const auto t = host.composer.bounds("t");
  ASSERT_TRUE(t);
  EXPECT_EQ(t->left(), 10);
  EXPECT_EQ(t->width(), 90);
  // Height is left open, so it comes from measurement: a wrapped run is
  // taller than a single line.
  EXPECT_GT(t->height(), 20.0f);
}

TEST(ComposePositioned, StructuralPruneAndMovesStillWork) {
  Host host;
  auto tree = [](float x) {
    return positioned()
        .inset(0, 0, 0, 0)
        .child(
            box().key("m").left(x).top(10).width(40).height(40).fill(green()));
  };
  host.composer.render(tree(10));
  host.frame();
  host.composer.render(tree(10));  // identical: prune, no cache writes
  host.frame();
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);
  host.composer.render(tree(120));  // moved: repaints, and the rect follows
  host.frame();
  EXPECT_EQ(host.pixel(140, 30), SK_ColorGREEN);
  EXPECT_EQ(host.pixel(30, 30), SK_ColorBLACK);
  const auto m = host.composer.bounds("m");
  ASSERT_TRUE(m);
  EXPECT_EQ(m->left(), 120);
}

TEST(ComposePositioned, HitTestAndZOrderSeeChildren) {
  Host host;
  host.composer.render(positioned()
                           .inset(0, 0, 0, 0)
                           .child(box()
                                      .key("under")
                                      .left(20)
                                      .top(20)
                                      .width(60)
                                      .height(60)
                                      .fill(Fill::color({1, 0, 0, 1}))
                                      .hitTestable(true))
                           .child(box()
                                      .key("over")
                                      .left(50)
                                      .top(50)
                                      .width(60)
                                      .height(60)
                                      .zIndex(1)
                                      .fill(green())
                                      .hitTestable(true)));
  host.frame();
  EXPECT_EQ(host.pixel(60, 60), SK_ColorGREEN);  // zIndex 1 paints on top
  const auto hitOver = host.composer.hitTest({60, 60});
  const auto hitUnder = host.composer.hitTest({25, 25});
  ASSERT_TRUE(hitOver && hitUnder);
  EXPECT_EQ(*hitOver, "over");
  EXPECT_EQ(*hitUnder, "under");
}

TEST(ComposePositioned, TogglingPositionedRemountsCleanly) {
  Host host;
  auto child = []() {
    return box().key("c").left(10).top(10).width(30).height(30).fill(green());
  };
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  // The same child under a plain box: rejoins the Yoga world.
  host.composer.render(box().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 2u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);  // absolute insets agree
  // And back again.
  host.composer.render(positioned().inset(0, 0, 0, 0).child(child()));
  host.frame();
  EXPECT_EQ(host.composer.stats().yogaNodes, 1u);
  EXPECT_EQ(host.pixel(20, 20), SK_ColorGREEN);
}

namespace {

/** The profile row for the node keyed `key`, from the last draw (labels
 *  are "<key> (<kind> WxH)"). */
const Composer::NodeCost* rowOf(Host& host, const char* key) {
  const std::string prefix = std::string(key) + " (";
  for (const Composer::NodeCost& row : host.composer.profile())
    if (row.label.rfind(prefix, 0) == 0) return &row;
  return nullptr;
}

}  // namespace

// ---- the memo carve-outs and the lanes they must not forget ---------------
//
// The content-volatility terms are enumerated in several places: once for
// `ownContent`, once for the group memo, and once inside each memo
// carve-out. Every copy has to name every term. A carve-out that omits, say,
// a bound fill or a live effect lets a node carrying one of those AND an
// animated gate take a memo it has no right to — and replay a recording that
// baked the old colour.
//
// This is why the terms are named once and each consumer subtracts from
// them, rather than each site listing what it cares about.

TEST(ComposeCache, ALiveEffectMovingOverAHeldMaterialRepaints) {
  // The OTHER memo, same hole: liveMatOnly holds the recording while the
  // MATERIAL's resolved shader pointer is stable, and its refusal list
  // does not mention a live effect either.
  static sk_sp<SkRuntimeEffect> fx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader content; uniform float amt;"
                 "half4 main(float2 p) { half4 c = content.eval(p);"
                 "  return half4(c.r * half(amt), c.g, c.b, c.a); }"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  static sk_sp<SkRuntimeEffect> matfx = [] {
    auto [e, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float lift;"
                 "half4 main(float2 p) { return half4(1.0, half(lift), 0.0,"
                 "                                    1.0); }"));
    if (!e) ADD_FAILURE() << err.c_str();
    return e;
  }();
  choreograph::Output<float> lift{0.0f};  // the material's own bound uniform
  choreograph::Output<float> amt{1.0f};   // the effect's
  Host host(200, 200);
  host.composer.render(box().child(
      maskBox()
          .fill(Material::sksl(matfx).uniform("lift", &lift))
          .effect(Effect::shader(fx, {{"amt", 1.0f}}).uniform("amt", &amt))));
  host.frame();
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  EXPECT_GT(redInk(host, 25, 25, 115, 115), 4000) << "red to begin with";
  amt = 0.0f;  // the material holds; only the EFFECT moves
  host.frame(0.016);
  EXPECT_LT(redInk(host, 25, 25, 115, 115), 100)
      << "the live effect moved and the live-material memo replayed stale";
}
