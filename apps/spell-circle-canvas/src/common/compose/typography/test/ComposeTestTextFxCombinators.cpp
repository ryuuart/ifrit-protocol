// THE COMBINATORS OVER WHOLE EFFECTS: textFx::sequence's phases and crossfades,
// textFx::mix's composition, textFx::tween's keyframes and its curves, textFx::hold's
// veto, and what each answers about displacing its glyphs.
//
// The text binary's share of the content suites, one file per subject.

#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Material.h>

#include <memory>

#include "DressedTypeProbes.h"
#include <sigilmotion/ease/Ease.h>

namespace {

/** An effect that reports a constant dy, so a composition's arithmetic is
 *  readable straight off the returned GlyphModifier. */
TextEffect constantDy(float dy) {
  return textFx::effect(
      "constantDy" + std::to_string((int)dy),
      [dy](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
        GlyphModifier m;
        m.dy = dy;
        return m;
      });
}

/** An effect that reports the local time it was handed. */
TextEffect reportT() {
  return textFx::effect("reportT", [](const GlyphInfo&, float t,
                                      sigil::core::noise::Mix64Stream&) {
    GlyphModifier m;
    m.dy = t;
    return m;
  });
}

GlyphModifier evaluate(const TextEffect& effect, float t) {
  GlyphInfo g;
  sigil::core::noise::Mix64Stream rng(1);
  return effect(g, t, rng);
}

}  // namespace

TEST(ComposeTextFx, SeqRenormalizesEachPhaseOverItsOwnWindow) {
  // Each phase sees a full 0→1 across its slice of local time — that is
  // what makes a sequence a sequence rather than three effects sharing one
  // clock and each playing a third of its curve.
  const TextEffect sequence =
      textFx::sequence(reportT().until(0.5f), reportT());
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.0f).dy, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.25f).dy, 0.5f);  // half of phase one
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.75f).dy, 0.5f);  // half of phase two
  EXPECT_FLOAT_EQ(evaluate(sequence, 1.0f).dy, 1.0f);
  // The last phase runs to the end whatever it was declared with.
  const TextEffect short_ =
      textFx::sequence(reportT().until(0.5f), reportT().until(0.6f));
  EXPECT_FLOAT_EQ(evaluate(short_, 1.0f).dy, 1.0f);
}

TEST(ComposeTextFx, SeqCutsHardByDefaultAndLerpsAcrossAnXfade) {
  const TextEffect cut =
      textFx::sequence(constantDy(10).until(0.5f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(cut, 0.49f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(cut, 0.51f).dy, 0.0f);

  const TextEffect faded = textFx::sequence(
      constantDy(10).until(0.5f).crossfade(0.2f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(faded, 0.29f).dy, 10.0f);    // before the window
  EXPECT_NEAR(evaluate(faded, 0.40f).dy, 5.0f, 1e-4f);  // halfway across
  EXPECT_NEAR(evaluate(faded, 0.50f).dy, 0.0f, 1e-4f);  // at the joint
  EXPECT_FLOAT_EQ(evaluate(faded, 0.60f).dy, 0.0f);     // past it
}

TEST(ComposeTextFx, MixEvaluatesBothAndComposesByTheTrackAlgebra) {
  const TextEffect both = textFx::mix(constantDy(10), constantDy(4));
  EXPECT_FLOAT_EQ(evaluate(both, 0.5f).dy, 14.0f);  // offsets add
  const auto half = [](float scale) {
    return textFx::effect(
        "half" + std::to_string((int)(scale * 10)),
        [scale](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
          GlyphModifier m;
          m.scale = scale;
          m.alpha = scale;
          return m;
        });
  };
  const TextEffect scaled = textFx::mix(half(0.5f), half(0.5f));
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).scale, 0.25f);  // scale multiplies
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).alpha, 0.25f);  // …and so does alpha
}

TEST(ComposeTextFx, CombinatorsAreComparableWhenTheirOperandsAre) {
  EXPECT_TRUE(textFx::sequence(textFx::enter(textFx::rise(20)).until(0.5f), textFx::enter(textFx::pop())) ==
              textFx::sequence(textFx::enter(textFx::rise(20)).until(0.5f), textFx::enter(textFx::pop())));
  EXPECT_FALSE(textFx::sequence(textFx::enter(textFx::rise(20)).until(0.5f), textFx::enter(textFx::pop())) ==
               textFx::sequence(textFx::enter(textFx::rise(20)).until(0.6f), textFx::enter(textFx::pop())));
  EXPECT_FALSE(textFx::sequence(textFx::enter(textFx::rise(20)).until(0.5f), textFx::enter(textFx::pop())) ==
               textFx::sequence(textFx::enter(textFx::rise(22)).until(0.5f), textFx::enter(textFx::pop())));
  EXPECT_TRUE(textFx::mix(textFx::enter(textFx::rise(20)), textFx::enter(textFx::slide())) ==
              textFx::mix(textFx::enter(textFx::rise(20)), textFx::enter(textFx::slide())));
  EXPECT_FALSE(textFx::mix(textFx::enter(textFx::rise(20)), textFx::enter(textFx::slide())) ==
               textFx::mix(textFx::enter(textFx::slide()), textFx::enter(textFx::rise(20))));
}

// THE PLACEMENT FACT IS INFERRED wherever the data allows it, because it is
// the data: a preset's deviation is its own body, a table's mods are its
// entries, and a combinator can only do what its operands do. Only an
// ad-hoc lambda is opaque, and that door assumes motion.
TEST(ComposeTextFx, EveryEffectAnswersWhetherItMovesItsGlyphs) {
  // The presets that move geometry.
  EXPECT_TRUE(textFx::enter(textFx::rise()).displaces());
  EXPECT_TRUE(textFx::enter(textFx::slide()).displaces());
  EXPECT_TRUE(textFx::enter(textFx::pop()).displaces());
  EXPECT_TRUE(textFx::enter(textFx::spinIn()).displaces());
  EXPECT_TRUE(textFx::enter(textFx::scatter()).displaces());
  EXPECT_TRUE(textFx::waveLoop().displaces());
  // …and the ones that touch coverage, colour or the outline only, leaving
  // every pen position exactly where the layout put it.
  EXPECT_FALSE(textFx::enter(textFx::typeOn()).displaces());
  EXPECT_FALSE(TextEffect::variableAxis("GRAD", 80).displaces());
  EXPECT_FALSE(textFx::enter(textFx::variableAxisSweep("GRAD", 0, 80)).displaces());
  EXPECT_FALSE(textFx::tint({.from = SkColors::kGray, .to = SkColors::kWhite}).displaces());
  EXPECT_FALSE(textFx::scramble().displaces());

  // A TWEEN ANSWERS FROM ITS OWN STOPS. Colour and coverage are not
  // placement…
  EXPECT_FALSE(textFx::tween({.from = GlyphModifier{.alpha = 0.0f}}).displaces());
  EXPECT_FALSE(textFx::tween({.from = GlyphModifier{.colorMultiplier = {0.2f, 0.2f, 0.2f, 1}}})
                   .displaces());
  // …and every lane that is.
  EXPECT_TRUE(textFx::tween({.from = GlyphModifier{.dx = 12.0f}}).displaces());
  EXPECT_TRUE(textFx::tween({.from = GlyphModifier{.dy = 12.0f}}).displaces());
  EXPECT_TRUE(textFx::tween({.from = GlyphModifier{.scale = 1.4f}}).displaces());
  EXPECT_TRUE(
      textFx::tween({.from = GlyphModifier{.rotateDeg = 8.0f}}).displaces());
  EXPECT_TRUE(textFx::tween({.from = GlyphModifier{.scaleX = 1.2f}}).displaces());
  EXPECT_TRUE(
      textFx::tween({.from = GlyphModifier{.skewXDeg = 6.0f}}).displaces());
  EXPECT_TRUE(
      textFx::tween({.from = GlyphModifier{.skewYDeg = 6.0f}}).displaces());

  // A COMBINATOR DERIVES: any operand it may evaluate moving is enough, and
  // none of them moving is enough the other way. `textFx::hold` vetoes with
  // alpha, which places nothing, so it is its operand's answer.
  EXPECT_FALSE(textFx::mix(textFx::enter(textFx::typeOn()), textFx::scramble()).displaces());
  EXPECT_TRUE(textFx::mix(textFx::enter(textFx::typeOn()), textFx::enter(textFx::rise())).displaces());
  EXPECT_FALSE(
      textFx::sequence(textFx::enter(textFx::typeOn()).until(0.5f), textFx::scramble())
          .displaces());
  EXPECT_TRUE(textFx::sequence(textFx::enter(textFx::typeOn()).until(0.5f), textFx::enter(textFx::rise()))
                  .displaces());
  EXPECT_FALSE(textFx::hold(textFx::scramble()).displaces());
  EXPECT_TRUE(textFx::hold(textFx::enter(textFx::rise())).displaces());
  // Nesting keeps the derivation exact rather than sticky.
  EXPECT_FALSE(textFx::mix(textFx::sequence(
                               textFx::tint({.from = SkColors::kGray, .to = SkColors::kWhite})),
                           textFx::hold(textFx::enter(textFx::typeOn())))
                   .displaces());

  // A PASS IS NOT A PLACEMENT: its shader runs over pixels already
  // rasterized at the resting origins.
  struct NoParameters {};
  const auto identityPass = std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<NoParameters>("test.identity-pass")
          .body(sigil::material::Target::SkSL,
                "half4 main(float2 xy) { return uContent.eval(xy); }"));
  EXPECT_FALSE(textFx::pass(sigil::material::Material(identityPass))
                   .displaces());

  // THE OPAQUE DOOR assumes motion, and takes the author's word otherwise.
  const GlyphModifierFunction still = [](const GlyphInfo&, float,
                                         sigil::core::noise::Mix64Stream&) {
    GlyphModifier m;
    m.alpha = 0.5f;
    return m;
  };
  EXPECT_TRUE(textFx::effect("opaque", still).displaces());
  EXPECT_FALSE(textFx::effect("opaque", still).displacing(false).displaces());
  // …and the declaration rides the params, so two bodies under one key that
  // disagree about placement do not prune onto each other.
  EXPECT_FALSE(textFx::effect("opaque", still) ==
               textFx::effect("opaque", still).displacing(false));
  EXPECT_TRUE(textFx::effect("opaque", still).displacing(false) ==
              textFx::effect("opaque", still).displacing(false));
}

TEST(ComposeTextFx, ATweenReproducesEveryStopAtItsOwnShare) {
  // A published path is a promise about the moments it names. Whatever the
  // curve between them, the deviation AT a stop is the stop, and a stop
  // lands where the shares before it sum to.
  const GlyphModifier squash{.scaleX = 1.25f, .scaleY = 0.75f};
  const GlyphModifier stretch{.scaleX = 0.95f, .scaleY = 1.05f};
  const TextEffect rubber = textFx::tween(
      {.keyframes = {{.to = squash, .duration = 300ms},
                     {.to = stretch, .duration = 350ms},
                     {.to = {}, .duration = 350ms}},
       .ease = sigil::motion::ease::inOutCubic});
  const std::vector<std::pair<float, GlyphModifier>> stops = {
      {0.00f, {}}, {0.30f, squash}, {0.65f, stretch}, {1.00f, {}}};
  for (const auto& [at, stop] : stops) {
    EXPECT_NEAR(evaluate(rubber, at).scaleX, stop.scaleX, 1e-5f)
        << "scaleX at " << at;
    EXPECT_NEAR(evaluate(rubber, at).scaleY, stop.scaleY, 1e-5f)
        << "scaleY at " << at;
  }
  // Outside local time it HOLDS at the ends rather than extrapolating
  // numbers nobody published.
  EXPECT_FLOAT_EQ(evaluate(rubber, -0.5f).scaleX, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(rubber, 2.0f).scaleX, 1.0f);
  // Keyframes with no duration take EQUAL shares, Motion's rule.
  const TextEffect even = textFx::tween(
      {.keyframes = {{.to = squash}, {.to = stretch}, {.to = {}}}});
  EXPECT_NEAR(evaluate(even, 1.0f / 3.0f).scaleX, squash.scaleX, 1e-5f);
  EXPECT_NEAR(evaluate(even, 2.0f / 3.0f).scaleX, stretch.scaleX, 1e-5f);
}

TEST(ComposeTextFx, ATweenWithNoCurveIsStraightNotMotionsDefault) {
  // Motion's own default curve is outQuad; a tween over glyph deviations
  // reads an unnamed curve as STRAIGHT, because the stops are what the
  // author placed. A quarter of the way along is a quarter of the offset.
  const TextEffect straight =
      textFx::tween({.to = GlyphModifier{.dx = 100.0f}});
  EXPECT_FLOAT_EQ(evaluate(straight, 0.25f).dx, 25.0f)
      << "an unnamed curve bent the segment — outQuad reads 43.75 here";
  EXPECT_FLOAT_EQ(evaluate(straight, 0.0f).dx, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(straight, 1.0f).dx, 100.0f);
}

TEST(ComposeTextFx, ATweenEasesEachSegmentOnItsOwn) {
  // THE WHOLE CURVE, EVERY SEGMENT — which is what a keyframe list means
  // and what one curve stretched across the path would not be. Two
  // segments are the fewest that can tell the two apart.
  const GlyphModifier peak{.dy = 10.0f};
  const TextEffect linear =
      textFx::tween({.keyframes = {{.to = peak}, {.to = {}}}});
  EXPECT_FLOAT_EQ(evaluate(linear, 0.125f).dy, 2.5f);

  const TextEffect eased =
      textFx::tween({.keyframes = {{.to = peak}, {.to = {}}},
                     .ease = sigil::motion::ease::inOutCubic});
  EXPECT_FLOAT_EQ(evaluate(eased, 0.5f).dy, 10.0f);  // the stop is still exact
  EXPECT_LT(evaluate(eased, 0.125f).dy, 1.5f)  // …the middle is not linear
      << "a quarter of the way into the first segment the reading is the "
         "linear one, so the curve was not applied to the segment";
  // The second segment runs the same curve over its own span: a quarter in
  // and a quarter from the end of the two segments are mirror readings.
  EXPECT_NEAR(evaluate(eased, 0.375f).dy, evaluate(eased, 0.625f).dy, 1e-4f);

  // A keyframe's own curve governs the segment that ARRIVES at it, and no
  // other.
  const TextEffect part = textFx::tween(
      {.keyframes = {{.to = peak, .ease = sigil::motion::ease::linear},
                     {.to = {}}},
       .ease = sigil::motion::ease::inOutCubic});
  EXPECT_FLOAT_EQ(evaluate(part, 0.125f).dy, 2.5f);
  EXPECT_NEAR(evaluate(part, 0.625f).dy, evaluate(eased, 0.625f).dy, 1e-4f);
}

TEST(ComposeTextFx, ATweenCutsASubstitutionAndLerpsAMatchingAxis) {
  // The sequence crossfade's rules, because it is the same arithmetic: there is
  // no half-way glyph between two outlines, and an axis is the one
  // substitution with a continuum — and only between two stops naming the
  // SAME axis.
  const TextEffect letters =
      textFx::tween({.from = GlyphModifier{.codepoint = U'A'},
                     .to = GlyphModifier{.codepoint = U'B'}});
  EXPECT_EQ(evaluate(letters, 0.49f).codepoint, U'A');
  EXPECT_EQ(evaluate(letters, 0.51f).codepoint, U'B')
      << "a code point was not cut at the middle of its segment";

  const sigil::weave::FontVariation light("GRAD", 400.0f);
  const sigil::weave::FontVariation heavy("GRAD", 800.0f);
  const TextEffect swept = textFx::tween(
      {.from = GlyphModifier{.axis = light}, .to = GlyphModifier{.axis = heavy}});
  const GlyphModifier midway = evaluate(swept, 0.5f);
  ASSERT_TRUE(midway.axis.has_value());
  EXPECT_FLOAT_EQ(midway.axis.value_or(sigil::weave::FontVariation()).value,
                  600.0f);

  const sigil::weave::FontVariation slant("slnt", -10.0f);
  const TextEffect crossed = textFx::tween(
      {.from = GlyphModifier{.axis = light}, .to = GlyphModifier{.axis = slant}});
  const auto tagOf = [](const GlyphModifier& mod) {
    return mod.axis ? std::string(mod.axis->tag, 4) : std::string("(unset)");
  };
  EXPECT_EQ(tagOf(evaluate(crossed, 0.49f)), "GRAD");
  EXPECT_FLOAT_EQ(evaluate(crossed, 0.49f).axis->value, 400.0f)
      << "a differing axis was lerped rather than held until the cut";
  EXPECT_EQ(tagOf(evaluate(crossed, 0.51f)), "slnt")
      << "two different axes were averaged, which names a coordinate on "
         "neither of them";

  // The cut is Motion's too: the same line `motion::interpolate` finds.
  EXPECT_EQ(sigil::motion::interpolate(GlyphModifier{.codepoint = U'A'},
                                       GlyphModifier{.codepoint = U'B'}, 0.5f)
                .codepoint,
            U'B');
  static_assert(sigil::motion::Interpolable<GlyphModifier>);
}

TEST(ComposeTextFx, ATweenIsComparableByItsStopsAndItsCurves) {
  const auto path = [](float peak) {
    return sigil::motion::Tween<GlyphModifier>{
        .keyframes = {{.to = GlyphModifier{.scaleY = peak}}, {.to = {}}}};
  };
  const auto eased = [&path](float peak, sigil::motion::Easing ease) {
    sigil::motion::Tween<GlyphModifier> out = path(peak);
    out.ease = std::move(ease);
    return out;
  };
  EXPECT_TRUE(textFx::tween(path(1.25f)) == textFx::tween(path(1.25f)));
  EXPECT_FALSE(textFx::tween(path(1.25f)) == textFx::tween(path(1.30f)));
  // Where a stop falls is part of it.
  sigil::motion::Tween<GlyphModifier> early = path(1.25f);
  early.keyframes[0].duration = 50ms;
  EXPECT_FALSE(textFx::tween(path(1.25f)) == textFx::tween(early));
  // The curve is part of the identity. A path re-eased is a different
  // motion, and an effect comparing equal to the one it replaced would go
  // on drawing the old one with no diagnostic.
  EXPECT_FALSE(textFx::tween(path(1.25f)) ==
               textFx::tween(eased(1.25f, sigil::motion::ease::inOutCubic)));
  EXPECT_FALSE(textFx::tween(eased(1.25f, sigil::motion::ease::outQuad)) ==
               textFx::tween(eased(1.25f, sigil::motion::ease::inOutCubic)));
  EXPECT_TRUE(textFx::tween(eased(1.25f, sigil::motion::ease::inOutCubic)) ==
              textFx::tween(eased(1.25f, sigil::motion::ease::inOutCubic)));
}

TEST(ComposeTextFx, ATweenedTrackPrunesWhenItsStopsAreUnchanged) {
  // EQUALITY BY VALUE: two tweens built apart from equal stops are one
  // effect, so a re-described node with an unchanged path prunes.
  Host host;
  const auto tree = [](float lift) {
    return box().padding(10).children(
        {text(u8"KEYS", whiteStyle(28))
             .key("k")
             .textFx({.effect = textFx::tween(
                          {.keyframes = {{.to = GlyphModifier{.dy = lift}},
                                         {.to = {}}},
                           .ease = sigil::motion::ease::inOutCubic})})});
  };
  host.composer.render(tree(-8.0f));
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  host.composer.render(tree(-8.0f));  // fresh Elements, an identical path
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame(0.016);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // The control: a path with one number moved is a different value, and
  // the node it describes has to be patched.
  host.composer.render(tree(-9.0f));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeTextFx, TypeOnAndTheAxisSweepAreTweensOfDisplaced) {
  // The typewriter is the opacity lane stepping at the middle of the beat:
  // absent before, simply there from the middle on.
  const TextEffect typed = textFx::enter(textFx::typeOn());
  EXPECT_FLOAT_EQ(evaluate(typed, 0.0f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(typed, 0.49f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(typed, 0.5f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(typed, 1.0f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(typed, 0.75f).dy, 0.0f);

  // The sweep is the axis lane, straight from one coordinate to the other.
  const TextEffect swept =
      textFx::enter(textFx::variableAxisSweep("GRAD", 0.0f, 80.0f));
  const GlyphModifier quarter = evaluate(swept, 0.25f);
  ASSERT_TRUE(quarter.axis.has_value());
  EXPECT_EQ(std::string(quarter.axis->tag, 4), "GRAD");
  EXPECT_FLOAT_EQ(quarter.axis->value, 20.0f);
  EXPECT_FLOAT_EQ(quarter.alpha, 1.0f);
  // Both are values: equal arguments are one effect.
  EXPECT_TRUE(textFx::enter(textFx::variableAxisSweep("GRAD", 0.0f, 80.0f)) ==
              swept);
  EXPECT_FALSE(textFx::enter(textFx::variableAxisSweep("GRAD", 0.0f, 90.0f)) ==
               swept);
}

TEST(ComposeTextFx, HoldWithholdsTheEffectUntilTheBeatOpens) {
  const TextEffect held = textFx::hold(constantDy(10));
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).dy, 0.0f)
      << "the wrapped effect ran on a beat that had not opened";
  // …and the moment it opens, EXACTLY the wrapped effect.
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 1.0f).dy, 10.0f);

  EXPECT_TRUE(textFx::hold(textFx::enter(textFx::rise(20))) == textFx::hold(textFx::enter(textFx::rise(20))));
  EXPECT_FALSE(textFx::hold(textFx::enter(textFx::rise(20))) ==
               textFx::hold(textFx::enter(textFx::rise(22))));
  EXPECT_FALSE(textFx::hold(textFx::enter(textFx::rise(20))) == textFx::enter(textFx::rise(20)));
  EXPECT_FLOAT_EQ(textFx::hold(textFx::enter(textFx::rise(20))).reach(),
                  textFx::enter(textFx::rise(20)).reach());
}
