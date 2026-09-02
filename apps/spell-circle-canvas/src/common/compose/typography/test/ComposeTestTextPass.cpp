// The fx::pass seam — a track's effect as ONE shader pass over its units'
// rendered pixels — and the uniform doors that feed it: float2/float4/array
// constants, and the live UniformBlock array. Everything draws into a
// raster surface and reads pixels back; the schedule assertions compare
// against Composer::beatsOf, because the pass and that query resolve one
// TrackCascade and must never disagree.

#include <sigilmaterial/core/Material.h>

#include <memory>
#include <vector>

#include "support/TextTestSupport.h"

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

/** These bodies read the runtime's declarations and nothing of their own,
 *  so their recipes carry no ABI. */
struct NoParams {};

/** The pass material over @p source: ONE recipe per source for the whole
 *  TU, because a recipe's identity is the object — two materials over one
 *  definition compare equal, which is what the equality assertions below
 *  rest on, and a fresh definition per call would compile a fresh program
 *  every time. */
material::skia::Paint passOver(const char* source) {
  struct Held {
    const char* source;
    std::shared_ptr<const sigil::material::Recipe> recipe;
  };
  static std::vector<Held> held;
  for (const Held& h : held)
    if (h.source == source)
      return material::skia::Paint::recipe(sigil::material::Material(h.recipe));
  auto recipe = std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<NoParams>("test.pass")
          .body(sigil::material::Target::SkSL, source));
  held.push_back({source, recipe});
  return material::skia::Paint::recipe(sigil::material::Material(recipe));
}

}  // namespace

TEST(TextPass, SpecializationIsOneRecipePerUnitCount) {
  const std::shared_ptr<const sigil::material::Recipe> authored =
      passOver(kFloodSksl).recipeMaterial()->recipePtr();
  const std::shared_ptr<const sigil::material::Recipe> three =
      material::skia::detail::passRecipeFor(authored, 3);
  ASSERT_TRUE(three);
  // The specialization keeps the author's ABI and prepends the runtime's
  // declarations at the count asked for.
  EXPECT_EQ(three->params(), authored->params());
  EXPECT_NE(three->source(sigil::material::Target::SkSL).find("uUnitRect[3]"),
            std::string::npos);
  // One definition per (recipe, count): asking again returns the SAME one,
  // so the program cache holds one program for it however many draws ask.
  EXPECT_EQ(three, material::skia::detail::passRecipeFor(authored, 3));
  // Another count is another definition, compiled and cached apart.
  const std::shared_ptr<const sigil::material::Recipe> five =
      material::skia::detail::passRecipeFor(authored, 5);
  ASSERT_TRUE(five);
  EXPECT_NE(three, five);
  EXPECT_EQ(five, material::skia::detail::passRecipeFor(authored, 5));
}

TEST(TextPass, RecipeMaterialsCompareByDefinition) {
  // Two materials over one recipe compare EQUAL — a helper may rebuild its
  // material every describe and still prune.
  const material::skia::Paint a = passOver(kFloodSksl);
  const material::skia::Paint b = passOver(kFloodSksl);
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a == passOver(kIdentitySksl));
  // And so do the pass effects wrapping them.
  EXPECT_TRUE(fx::pass(a) == fx::pass(b));
  EXPECT_FALSE(fx::pass(a) == fx::pass(passOver(kIdentitySksl)));
}

TEST(TextPass, NonRecipeMaterialRefusedAndGlyphsSurvive) {
  // fx::pass demands the recipe-backed form (the runtime specializes the
  // recipe per unit count). A compiled-effect material is refused: the
  // effect is EMPTY, the track is skipped, and the text draws at rest
  // rather than vanishing.
  const TextEffect refused = fx::pass(material::skia::Paint::sksl(ukEffect()));
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
          .fx({.effect = fx::pass(passOver(kPhaseProbeSksl)),
               .stagger = {.eachMs = 90, .durationMs = 200},
               .over = unit::Cluster,
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
                     .fx({.effect = fx::pass(passOver(kIdentitySksl))}))
          .child(text(u8"ABCDE", whiteStyle(28))
                     .key("five")
                     .fx({.effect = fx::pass(passOver(kIdentitySksl))})));
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
          .fx({.effect = fx::pass(passOver(kFloodSksl)), .reach = 12.0f})));
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

TEST(TextPass, ReachGrowsBoundsWithoutMovingContent) {
  // `Track::reach` grows the pass's painted BOUNDS and touches nothing
  // else: the layer's pixels land exactly where the glyphs were placed,
  // and the band beyond the box is the pass's to paint. Both halves are
  // asserted against ONE pair of renders, because either alone is
  // satisfiable by a broken mapping — a layer stretched or shifted into
  // the grown rect still covers the band, and a rect never grown keeps
  // the content in place — and only the pair pins the contract. The lift
  // deviation stands each glyph's top proud of the box, so the box edge
  // separates the two assertions cleanly.
  const TextEffect lift =
      fx::effect("test-lift",
                 [](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
                   GlyphMod m;
                   m.dy = -14.0f;
                   return m;
                 });
  const auto describe = [&](float reach) {
    return box().padding(60).child(
        text(u8"HOIST", whiteStyle(34))
            .key("hoist")
            .fx({.effect = lift})
            .fx({.effect = fx::pass(passOver(kIdentitySksl)), .reach = reach}));
  };
  Host snug;
  snug.composer.render(describe(0.0f));
  snug.frame();
  Host wide;
  wide.composer.render(describe(40.0f));
  wide.frame();
  const std::optional<SkRect> laidOut = wide.composer.bounds("hoist");
  ASSERT_TRUE(laidOut.has_value());
  const SkRect box = laidOut.value_or(SkRect::MakeEmpty());

  // Inside the box, byte for byte the same picture: the reach must not
  // move, scale or resample what the glyphs painted. Probed per pixel —
  // and there must BE glyph pixels here, or the equality proved nothing.
  const std::vector<SkColor> snugPx = grab(snug);
  const std::vector<SkColor> widePx = grab(wide);
  SkIRect inside = box.round();
  inside.inset(1, 1);
  int mismatches = 0;
  int glyphPixels = 0;
  for (int y = inside.top(); y < inside.bottom(); ++y)
    for (int x = inside.left(); x < inside.right(); ++x) {
      const size_t i = (size_t)y * 200 + (size_t)x;
      if (snugPx[i] != widePx[i]) ++mismatches;
      if (snugPx[i] == SK_ColorWHITE) ++glyphPixels;
    }
  EXPECT_EQ(mismatches, 0) << "reach moved the layer's content";
  EXPECT_GT(glyphPixels, 0) << "no glyph pixels inside the box — the "
                               "placement comparison compared nothing";

  // And the band beyond the box is painted: the lifted glyph tops the
  // snug pass clips at the box edge survive under the wide one.
  const SkIRect band = SkIRect::MakeLTRB((int)box.left(), (int)box.top() - 16,
                                         (int)box.right(), (int)box.top() - 1);
  EXPECT_TRUE(anyWhiteIn(wide, band));
  EXPECT_FALSE(anyWhiteIn(snug, band));
  // Beyond box + reach stays the pass's hard edge, reach or no reach.
  EXPECT_EQ(wide.pixel((int)box.centerX(), (int)box.top() - 50), SK_ColorBLACK);
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
            .fx({.effect = fx::pass(passOver(kPhaseProbeSksl)),
                 .stagger = {.eachMs = 60, .durationMs = 200},
                 .over = unit::Cluster,
                 .progress = animate(sigil::motion::to(target),
                                     motion::Transition{.duration = 200ms})}));
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
      fx::effect("test-hide",
                 [](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
                   GlyphMod m;
                   m.alpha = 0.0f;
                   return m;
                 });
  Host hidden;
  hidden.composer.render(box().padding(30).child(
      text(u8"GONE", whiteStyle(40))
          .key("t")
          .fx({.effect = hide})
          .fx({.effect = fx::pass(passOver(kIdentitySksl))})));
  hidden.frame();
  EXPECT_FALSE(anyWhiteIn(hidden, SkIRect::MakeXYWH(10, 10, 180, 180)));

  Host shown;
  shown.composer.render(box().padding(30).child(
      text(u8"GONE", whiteStyle(40))
          .key("t")
          .fx({.effect = fx::pass(passOver(kIdentitySksl))})));
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
          .fx({.effect = fx::pass(passOver(kEraseSksl))})));
  host.frame();
  EXPECT_FALSE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180)));
}

TEST(TextPass, RestsAtSkipsTheShaderWhenEveryUnitSitsOnADeclaredPhase) {
  // The erase pass hides its letters whenever it actually RUNS, which is
  // what makes the skip observable: at a phase covered by the declaration
  // the batches draw directly and the letters show, while any phase off
  // the declaration still runs the shader and erases them.
  const auto lettersShow = [](TextEffect effect, sigil::motion::Spread cascade,
                              float master) {
    Host host;
    host.composer.render(
        box().padding(30).child(text(u8"REST", whiteStyle(40))
                                    .key("t")
                                    .fx({.effect = std::move(effect),
                                         .stagger = std::move(cascade),
                                         .over = unit::Cluster,
                                         .progress = master})));
    host.frame();
    return anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180));
  };
  const sigil::motion::Spread oneShot{.eachMs = 60, .durationMs = 200};
  const TextEffect erase = fx::pass(passOver(kEraseSksl));

  // Undeclared: the pass runs at every phase, both ends included.
  EXPECT_FALSE(lettersShow(erase, oneShot, 0.0f));
  EXPECT_FALSE(lettersShow(erase, oneShot, 1.0f));

  // Declared at both ends: a one-shot cascade clamps every unit to exactly
  // 0 at master 0 and exactly 1 at master 1, so both ends skip — and the
  // middle, where the units straddle their beats, still runs the shader.
  const TextEffect rests = erase.restsAt(0.0f, 1.0f);
  EXPECT_TRUE(lettersShow(rests, oneShot, 0.0f));
  EXPECT_FALSE(lettersShow(rests, oneShot, 0.5f));
  EXPECT_TRUE(lettersShow(rests, oneShot, 1.0f));

  // One declared end says nothing about the other.
  EXPECT_TRUE(lettersShow(erase.restsAt(0.0f), oneShot, 0.0f));
  EXPECT_FALSE(lettersShow(erase.restsAt(0.0f), oneShot, 1.0f));

  // A LOOPING cascade: units genuinely rest at exactly 1 between beats, so
  // restsAt(1) engages whenever no beat is mid-cycle — and does not while
  // any unit is mid-beat.
  sigil::motion::Spread loop{.eachMs = 60, .durationMs = 100};
  loop.loopMs = 1000;
  EXPECT_TRUE(lettersShow(erase.restsAt(1.0f), loop, 0.5f));
  EXPECT_FALSE(lettersShow(erase.restsAt(1.0f), loop, 0.05f));
}

TEST(TextPass, RestDeclarationRidesEqualityAndNeedsAPass) {
  // The declaration is part of the effect's identity: two passes differing
  // only in their rests must compare unequal, or a re-described track
  // would prune onto the old declaration and keep (or keep skipping) a
  // shader the author changed their mind about.
  const material::skia::Paint m = passOver(kEraseSksl);
  EXPECT_FALSE(fx::pass(m).restsAt(0.0f) == fx::pass(m));
  EXPECT_TRUE(fx::pass(m).restsAt(0.0f, 1.0f) ==
              fx::pass(m).restsAt(0.0f, 1.0f));
  EXPECT_FALSE(fx::pass(m).restsAt(0.0f) == fx::pass(m).restsAt(1.0f));
  const TextEffect both = fx::pass(m).restsAt(0.0f, 1.0f);
  const std::span<const float> declared = both.restPhases();
  ASSERT_EQ(declared.size(), 2u);
  EXPECT_EQ(declared[0], 0.0f);
  EXPECT_EQ(declared[1], 1.0f);

  // On a per-glyph effect the declaration is about a shader that does not
  // exist: it warns once and the effect comes back unchanged.
  const TextEffect plain = fx::rise(10);
  EXPECT_TRUE(plain.restsAt(0.0f) == plain);
  EXPECT_TRUE(plain.restsAt(0.0f).restPhases().empty());
}

TEST(TextPass, RidesAPathBaseline) {
  Host host;
  host.composer.render(box().padding(10).child(
      text(u8"AROUND THE RING", whiteStyle(22))
          .key("ring")
          .width(180)
          .height(180)
          .onPath({.path = geometry::shapes::circle()})
          .fx({.effect = fx::pass(passOver(kIdentitySksl))})));
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
          .fx({.effect = fx::pass(passOver(kIdentitySksl))})));
  host.frame();
  EXPECT_TRUE(anyWhiteIn(host, SkIRect::MakeXYWH(10, 10, 180, 180)));
  EXPECT_FALSE(host.composer.beatsOf("col", 0).empty());
}

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
      material::skia::Paint::sksl(wideUniformEffect())
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
  const material::skia::Paint base =
      material::skia::Paint::sksl(wideUniformEffect());
  material::skia::Paint wrong =
      material::skia::Paint::sksl(wideUniformEffect());
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
  auto block = std::make_shared<sigil::material::UniformBlock>(4);
  material::skia::Paint live =
      material::skia::Paint::sksl(wideUniformEffect()).uniform("uVals", block);
  // The binding declares volatility — the node paints live, no cache can
  // freeze the table — exactly as a bound scalar Output does.
  EXPECT_TRUE(live.isAnimated());
  // A block at the wrong size is refused and declares nothing.
  auto wrong = std::make_shared<sigil::material::UniformBlock>(3);
  EXPECT_FALSE(material::skia::Paint::sksl(wideUniformEffect())
                   .uniform("uVals", wrong)
                   .isAnimated());

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
