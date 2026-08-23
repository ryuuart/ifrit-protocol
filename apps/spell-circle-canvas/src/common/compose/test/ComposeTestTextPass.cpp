// The fx::pass seam — a track's effect as ONE shader pass over its units'
// rendered pixels — and the uniform doors that feed it: float2/float4/array
// constants, and the live UniformBlock array. Everything draws into a
// raster surface and reads pixels back; the schedule assertions compare
// against Composer::beatsOf, because the pass and that query resolve one
// TrackCascade and must never disagree.

#include "ComposeTestSupport.h"

namespace {

/** A pass that paints each unit's LOCAL TIME into the red channel across
 *  that unit's own rect, ignoring the content — the per-unit uniforms made
 *  visible, one readback per beat. */
constexpr const char* kPhaseProbeSksl =
    "half4 main(float2 xy) {"
    "  float p = 0.0;"
    "  float hit = 0.0;"
    "  for (int i = 0; i < kUnitCount; ++i) {"
    "    float4 r = uUnitRect[i];"
    "    float inside = step(r.x, xy.x) * step(xy.x, r.x + r.z) *"
    "                   step(r.y, xy.y) * step(xy.y, r.y + r.w);"
    "    p = mix(p, uUnitPhase[i].x, inside);"
    "    hit = max(hit, inside);"
    "  }"
    "  return half4(half(p * hit), 0.0, 0.0, half(hit));"
    "}";

/** A pass that paints solid green over its whole footprint — the bounding
 *  assertions' probe: wherever this lands is exactly where a pass may
 *  paint. */
constexpr const char* kFloodSksl =
    "half4 main(float2 xy) { return half4(0.0, 1.0, 0.0, 1.0); }";

/** The identity pass: the units' layer, unchanged — what order-of-
 *  composition is measured through, because it shows exactly the pixels
 *  the pass was handed. */
constexpr const char* kIdentitySksl =
    "half4 main(float2 xy) { return uContent.eval(xy); }";

/** A pass that paints nothing — proves an addressed glyph draws ONLY
 *  through its pass, never directly as well. */
constexpr const char* kEraseSksl =
    "half4 main(float2 xy) { return half4(0.0, 0.0, 0.0, 0.0); }";

}  // namespace

TEST(TextPass, SpecializationCompilesAndCachesPerUnitCount) {
  const std::string source = kFloodSksl;
  const sk_sp<SkRuntimeEffect> three = detail::passEffectFor(source, 3);
  ASSERT_TRUE(three);  // the prelude + source compile, arrays at [3]
  // One compile per (source, count): asking again returns the SAME effect.
  EXPECT_EQ(three, detail::passEffectFor(source, 3));
  // Another count is another specialization, compiled and cached apart.
  const sk_sp<SkRuntimeEffect> five = detail::passEffectFor(source, 5);
  ASSERT_TRUE(five);
  EXPECT_NE(three, five);
  EXPECT_EQ(five, detail::passEffectFor(source, 5));
}

TEST(TextPass, SourceMaterialsCompareByRecipe) {
  // The source form's whole point: two materials minted from one source
  // string share the cached effect and compare EQUAL — a helper may
  // rebuild its material every describe and still prune.
  const Material a = Material::sksl(kFloodSksl);
  const Material b = Material::sksl(kFloodSksl);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == Material::sksl(kIdentitySksl));
  // And so do the pass effects wrapping them.
  EXPECT_TRUE(fx::pass(a) == fx::pass(b));
  EXPECT_FALSE(fx::pass(a) == fx::pass(Material::sksl(kIdentitySksl)));
}

TEST(TextPass, NonSourceMaterialRefusedAndGlyphsSurvive) {
  // fx::pass demands the source-carrying form (the runtime must bake the
  // unit count into the compiled shader). A compiled-effect material is
  // refused: the effect is EMPTY, the track is skipped, and the text draws
  // at rest rather than vanishing.
  const TextEffect refused = fx::pass(Material::sksl(ukEffect()));
  EXPECT_FALSE(refused);

  Host host;
  host.composer.render(box().padding(30).child(
      text(u8"REST", whiteStyle(40)).key("rest").fx({.effect = refused})));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(20, 20, 160, 100)));
}

TEST(TextPass, UnitRectAndPhaseAgreeWithBeatsOf) {
  Host host;
  host.composer.render(box().padding(20).child(
      text(u8"ABC DEF", whiteStyle(30))
          .key("probe")
          .fx({.effect = fx::pass(Material::sksl(kPhaseProbeSksl)),
               .stagger =
                   stagger(unit::Cluster, {.eachMs = 90, .durationMs = 200}),
               .progress = 0.55f})));
  host.frame();

  const std::vector<Beat> beats = host.composer.beatsOf("probe", 0);
  ASSERT_GT(beats.size(), 4u);
  // The cascade spreads the beats, so the local times differ across units —
  // and each unit's rect carries ITS OWN time in the red channel, which is
  // the per-unit uniforms observed end to end.
  bool sawDistinct = false;
  for (const Beat& beat : beats) {
    const SkColor probe =
        host.pixel((int)beat.rect.centerX(), (int)beat.rect.centerY());
    const float painted = (float)SkColorGetR(probe) / 255.0f;
    EXPECT_NEAR(painted, beat.localT, 0.02f)
        << "unit " << beat.unitIndex << " painted a different local time "
        << "than beatsOf reports";
    sawDistinct |= std::abs(beat.localT - beats.front().localT) > 0.05f;
  }
  EXPECT_TRUE(sawDistinct) << "every beat read the same time — the cascade "
                              "never reached the uniforms";
}

TEST(TextPass, TwoUnitCountsInOneSession) {
  // Two nodes, two unit counts, one source: each track compiles (or finds)
  // its own specialization and both draw.
  Host host;
  host.composer.render(
      box()
          .column()
          .padding(16)
          .gap(12)
          .child(text(u8"AB", whiteStyle(28))
                     .key("two")
                     .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))}))
          .child(text(u8"ABCDE", whiteStyle(28))
                     .key("five")
                     .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))})));
  host.frame();
  const std::vector<Beat> two = host.composer.beatsOf("two", 0);
  const std::vector<Beat> five = host.composer.beatsOf("five", 0);
  ASSERT_FALSE(two.empty());
  ASSERT_FALSE(five.empty());
  EXPECT_NE(two.size(), five.size());
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 40)));
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 50, 180, 60)));
}

TEST(TextPass, BoundedByBoxPlusReach) {
  // The flood pass returns opaque green at EVERY coordinate — so wherever
  // green lands is exactly the pass's footprint. It must fill the node's
  // box grown by the track's reach and nothing beyond it.
  Host host;
  host.composer.render(box().padding(60).child(
      text(u8"IN", whiteStyle(30))
          .key("bounded")
          .fx({.effect = fx::pass(Material::sksl(kFloodSksl)),
               .reach = 12.0f})));
  host.frame();
  const std::optional<SkRect> laidOut = host.composer.bounds("bounded");
  ASSERT_TRUE(laidOut.has_value());
  const SkRect box = laidOut.value_or(SkRect::MakeEmpty());
  const SkColor green = SkColorSetRGB(0, 255, 0);
  // Inside the box: flooded.
  EXPECT_EQ(host.pixel((int)box.centerX(), (int)box.centerY()), green);
  // Inside the reach band, outside the box: still the pass's to paint.
  EXPECT_EQ(host.pixel((int)box.left() - 6, (int)box.centerY()), green);
  // Beyond box + reach: untouched, unlike a whole-canvas effect pass.
  EXPECT_EQ(host.pixel((int)box.left() - 20, (int)box.centerY()),
            SK_ColorBLACK);
  EXPECT_EQ(host.pixel((int)box.centerX(), (int)box.top() - 20), SK_ColorBLACK);
}

TEST(TextPass, ProgressAdvancesWithCascadeAndSettles) {
  // The pass's per-unit times ride the track's progress: a transition
  // moves them frame over frame, and once it settles the picture holds
  // byte for byte.
  Host host;
  const auto describe = [&](float target) {
    return box().padding(20).child(
        text(u8"ABCD", whiteStyle(30))
            .key("run")
            .fx({.effect = fx::pass(Material::sksl(kPhaseProbeSksl)),
                 .stagger =
                     stagger(unit::Cluster, {.eachMs = 60, .durationMs = 200}),
                 .progress =
                     animate(to(target), Transition{.duration = 200ms})}));
  };
  host.composer.render(describe(0.0f));
  host.frame();
  host.composer.render(describe(1.0f));
  host.frame(0.05);
  const std::vector<SkColor> early = grab(host);
  host.frame(0.05);
  const std::vector<SkColor> later = grab(host);
  EXPECT_NE(early, later) << "the pass never saw the progress move";
  // Run the transition out, then two more frames must be identical.
  for (int i = 0; i < 20; ++i) host.frame(0.05);
  const std::vector<SkColor> settledA = grab(host);
  host.frame(0.05);
  EXPECT_EQ(settledA, grab(host)) << "a settled pass kept repainting "
                                     "differently";
}

TEST(TextPass, ComposesDownstreamOfDeviationTracks) {
  // Deviations apply FIRST; the pass reads the deviated pixels. An
  // alpha-zero deviation empties the layer, so the identity pass shows
  // nothing — where without the deviation it shows the letters.
  const TextEffect hide =
      fx::effect("test-hide", [](const GlyphInfo&, float, Rng&) {
        GlyphMod m;
        m.alpha = 0.0f;
        return m;
      });
  Host hidden;
  hidden.composer.render(box().padding(30).child(
      text(u8"GONE", whiteStyle(40))
          .key("t")
          .fx({.effect = hide})
          .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))})));
  hidden.frame();
  EXPECT_FALSE(anyWhiteIn(hidden, SkIRect::MakeXYWH(10, 10, 180, 180)));

  Host shown;
  shown.composer.render(box().padding(30).child(
      text(u8"GONE", whiteStyle(40))
          .key("t")
          .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))})));
  shown.frame();
  EXPECT_TRUE(anyWhiteIn(shown, SkIRect::MakeXYWH(10, 10, 180, 180)));
}

TEST(TextPass, AddressedGlyphsDrawOnlyThroughTheirPass) {
  // A pass that returns transparent erases its units: were the glyphs also
  // drawn directly, the letters would still show underneath.
  Host host;
  host.composer.render(box().padding(30).child(
      text(u8"ERASED", whiteStyle(40))
          .key("t")
          .fx({.effect = fx::pass(Material::sksl(kEraseSksl))})));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180)));
}

TEST(TextPass, RidesAPathBaseline) {
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING", whiteStyle(22))
          .key("ring")
          .width(180)
          .height(180)
          .onPath({.path = shapes::circle()})
          .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))})));
  host.frame();
  // The identity pass hands back the curved lettering it was given.
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180)));
  // And the schedule stands on the curve, exactly as beatsOf reports it.
  EXPECT_FALSE(host.composer.beatsOf("ring", 0).empty());
}

TEST(TextPass, RidesAVerticalColumn) {
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"VERTICAL", whiteStyle(22))
          .key("col")
          .width(160)
          .height(180)
          .writingMode(sigil::weave::WritingMode::kVerticalRL)
          .fx({.effect = fx::pass(Material::sksl(kIdentitySksl))})));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180)));
  EXPECT_FALSE(host.composer.beatsOf("col", 0).empty());
}

// ---------------------------------------------------------------------------
// The uniform doors the pass leans on, exercised as plain fills.

namespace {
sk_sp<SkRuntimeEffect> wideUniformEffect() {
  // ONE effect for the TU: a compiled-effect material compares by effect
  // pointer, so the equality assertions below need every material built
  // over the same compile.
  static const sk_sp<SkRuntimeEffect> effect = [] {
    auto [compiled, err] = SkRuntimeEffect::MakeForShader(
        SkString("uniform float2 uPair;"
                 "uniform float4 uQuad;"
                 "uniform float uVals[4];"
                 "half4 main(float2 p) {"
                 "  return half4(half(uPair.x), half(uQuad.y),"
                 "               half(uVals[2]), 1.0);"
                 "}"));
    return compiled;
  }();
  return effect;
}
}  // namespace

TEST(TextPass, WideAndArrayUniformsBindByDeclaredSize) {
  Host host;
  host.composer.render(box().child(box().width(60).height(60).fill(
      Material::sksl(wideUniformEffect())
          .uniform("uPair", std::array<float, 2>{1, 0})
          .uniform("uQuad", std::array<float, 4>{0, 1, 0, 0})
          .uniform("uVals", std::vector<float>{0, 0, 1, 0}))));
  host.frame();
  EXPECT_EQ(host.pixel(30, 30), SK_ColorWHITE);  // all three lanes landed
}

TEST(TextPass, MisSizedUniformsWarnOnceAndAreIgnored) {
  // An undeclared name, and a declared one at the wrong TOTAL size, are
  // both dropped at the door — so the material still equals one that never
  // made the call, and nothing was stored for the builder to refuse.
  const Material base = Material::sksl(wideUniformEffect());
  Material wrong = Material::sksl(wideUniformEffect());
  wrong.uniform("uVals", std::vector<float>{1, 2, 3});       // [4] wants 4
  wrong.uniform("uNothing", std::vector<float>{1, 2, 3});    // undeclared
  wrong.uniform("uPair", std::array<float, 4>{1, 2, 3, 4});  // float2 slot
  EXPECT_TRUE(base == wrong);

  Effect effect = Effect::shader(wideUniformEffect());
  Effect wrongEffect = Effect::shader(wideUniformEffect());
  wrongEffect.uniform("uVals", std::vector<float>{1, 2, 3});
  wrongEffect.uniform("uNothing", 1.0f);
  EXPECT_TRUE(effect == wrongEffect);
}

TEST(TextPass, EffectConstantLanesParticipateInEquality) {
  Effect a = Effect::shader(wideUniformEffect());
  a.uniform("uPair", std::array<float, 2>{1, 0});
  a.uniform("uVals", std::vector<float>{1, 2, 3, 4});
  Effect b = Effect::shader(wideUniformEffect());
  b.uniform("uPair", std::array<float, 2>{1, 0});
  b.uniform("uVals", std::vector<float>{1, 2, 3, 4});
  EXPECT_TRUE(a == b);
  b.uniform("uQuad", std::array<float, 4>{1, 0, 0, 0});
  EXPECT_FALSE(a == b);
}

TEST(TextPass, UniformBlockIsLiveAndReadsOnCommit) {
  auto block = std::make_shared<UniformBlock>(4);
  Material live = Material::sksl(wideUniformEffect()).uniform("uVals", block);
  // The binding declares volatility — the node paints live, no cache can
  // freeze the table — exactly as a bound scalar Output does.
  EXPECT_TRUE(live.isAnimated());
  // A block at the wrong size is refused and declares nothing.
  auto wrong = std::make_shared<UniformBlock>(3);
  EXPECT_FALSE(
      Material::sksl(wideUniformEffect()).uniform("uVals", wrong).isAnimated());

  Host host;
  host.composer.render(
      box().child(box().key("live").width(60).height(60).fill(live)));
  host.frame();
  EXPECT_EQ(SkColorGetB(host.pixel(30, 30)), 0u);  // uVals[2] still 0
  // An UNcommitted write changes nothing on screen: the resolve memo holds
  // the previous shader until the revision moves, which is the contract
  // that makes commit() the publish.
  block->values()[2] = 1.0f;
  host.frame(0.016);
  EXPECT_EQ(SkColorGetB(host.pixel(30, 30)), 0u);
  block->commit();
  host.frame(0.016);
  EXPECT_EQ(SkColorGetB(host.pixel(30, 30)), 255u);
}
