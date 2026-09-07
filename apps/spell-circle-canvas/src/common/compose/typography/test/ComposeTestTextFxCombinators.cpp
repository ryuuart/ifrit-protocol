// THE COMBINATORS OVER WHOLE EFFECTS: fx::seq's phases and crossfades,
// fx::mix's composition, fx::keys' table and its curves, fx::hold's veto,
// and what each answers about displacing its glyphs.
//
// The text binary's share of the content suites, one file per subject.

#include <sigilmaterial/core/Material.h>

#include <memory>

#include "DressedTypeProbes.h"

namespace {

/** An effect that reports a constant dy, so a composition's arithmetic is
 *  readable straight off the returned GlyphMod. */
TextEffect constantDy(float dy) {
  return fx::effect(
      "constantDy" + std::to_string((int)dy),
      [dy](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
        GlyphMod m;
        m.dy = dy;
        return m;
      });
}

/** An effect that reports the local time it was handed. */
TextEffect reportT() {
  return fx::effect("reportT", [](const GlyphInfo&, float t,
                                  sigil::core::noise::Mix64Stream&) {
    GlyphMod m;
    m.dy = t;
    return m;
  });
}

GlyphMod evaluate(const TextEffect& effect, float t) {
  GlyphInfo g;
  sigil::core::noise::Mix64Stream rng(1);
  return effect(g, t, rng);
}

}  // namespace

TEST(ComposeTextFx, SeqRenormalizesEachPhaseOverItsOwnWindow) {
  // Each phase sees a full 0→1 across its slice of local time — that is
  // what makes a sequence a sequence rather than three effects sharing one
  // clock and each playing a third of its curve.
  const TextEffect sequence = fx::seq(reportT().until(0.5f), reportT());
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.0f).dy, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.25f).dy, 0.5f);  // half of phase one
  EXPECT_FLOAT_EQ(evaluate(sequence, 0.75f).dy, 0.5f);  // half of phase two
  EXPECT_FLOAT_EQ(evaluate(sequence, 1.0f).dy, 1.0f);
  // The last phase runs to the end whatever it was declared with.
  const TextEffect short_ =
      fx::seq(reportT().until(0.5f), reportT().until(0.6f));
  EXPECT_FLOAT_EQ(evaluate(short_, 1.0f).dy, 1.0f);
}

TEST(ComposeTextFx, SeqCutsHardByDefaultAndLerpsAcrossAnXfade) {
  const TextEffect cut = fx::seq(constantDy(10).until(0.5f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(cut, 0.49f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(cut, 0.51f).dy, 0.0f);

  const TextEffect faded =
      fx::seq(constantDy(10).until(0.5f).xfade(0.2f), constantDy(0));
  EXPECT_FLOAT_EQ(evaluate(faded, 0.29f).dy, 10.0f);    // before the window
  EXPECT_NEAR(evaluate(faded, 0.40f).dy, 5.0f, 1e-4f);  // halfway across
  EXPECT_NEAR(evaluate(faded, 0.50f).dy, 0.0f, 1e-4f);  // at the joint
  EXPECT_FLOAT_EQ(evaluate(faded, 0.60f).dy, 0.0f);     // past it
}

TEST(ComposeTextFx, MixEvaluatesBothAndComposesByTheTrackAlgebra) {
  const TextEffect both = fx::mix(constantDy(10), constantDy(4));
  EXPECT_FLOAT_EQ(evaluate(both, 0.5f).dy, 14.0f);  // offsets add
  const auto half = [](float scale) {
    return fx::effect(
        "half" + std::to_string((int)(scale * 10)),
        [scale](const GlyphInfo&, float, sigil::core::noise::Mix64Stream&) {
          GlyphMod m;
          m.scale = scale;
          m.alpha = scale;
          return m;
        });
  };
  const TextEffect scaled = fx::mix(half(0.5f), half(0.5f));
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).scale, 0.25f);  // scale multiplies
  EXPECT_FLOAT_EQ(evaluate(scaled, 0.5f).alpha, 0.25f);  // …and so does alpha
}

TEST(ComposeTextFx, CombinatorsAreComparableWhenTheirOperandsAre) {
  EXPECT_TRUE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
              fx::seq(fx::rise(20).until(0.5f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(20).until(0.6f), fx::pop()));
  EXPECT_FALSE(fx::seq(fx::rise(20).until(0.5f), fx::pop()) ==
               fx::seq(fx::rise(22).until(0.5f), fx::pop()));
  EXPECT_TRUE(fx::mix(fx::rise(20), fx::slide()) ==
              fx::mix(fx::rise(20), fx::slide()));
  EXPECT_FALSE(fx::mix(fx::rise(20), fx::slide()) ==
               fx::mix(fx::slide(), fx::rise(20)));
}

// THE PLACEMENT FACT IS INFERRED wherever the data allows it, because it is
// the data: a preset's deviation is its own body, a table's mods are its
// entries, and a combinator can only do what its operands do. Only an
// ad-hoc lambda is opaque, and that door assumes motion.
TEST(ComposeTextFx, EveryEffectAnswersWhetherItMovesItsGlyphs) {
  // The presets that move geometry.
  EXPECT_TRUE(fx::rise().displaces());
  EXPECT_TRUE(fx::slide().displaces());
  EXPECT_TRUE(fx::pop().displaces());
  EXPECT_TRUE(fx::spinIn().displaces());
  EXPECT_TRUE(fx::scatter().displaces());
  EXPECT_TRUE(fx::waveLoop().displaces());
  // …and the ones that touch coverage, colour or the outline only, leaving
  // every pen position exactly where the layout put it.
  EXPECT_FALSE(fx::typeOn().displaces());
  EXPECT_FALSE(TextEffect::variableAxis("GRAD", 80).displaces());
  EXPECT_FALSE(fx::variableAxisSweep("GRAD", 0, 80).displaces());
  EXPECT_FALSE(fx::tint(SkColors::kGray, SkColors::kWhite).displaces());
  EXPECT_FALSE(fx::scramble().displaces());

  // A TABLE ANSWERS FROM ITS OWN ENTRIES. Colour and coverage are not
  // placement…
  EXPECT_FALSE(fx::keys({{0.0f, {.alpha = 0.0f}}, {1.0f, {}}}).displaces());
  EXPECT_FALSE(
      fx::keys({{0.0f, {.colorMul = {0.2f, 0.2f, 0.2f, 1}}}, {1.0f, {}}})
          .displaces());
  // …and every lane that is.
  EXPECT_TRUE(fx::keys({{0.0f, {.dx = 12.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.dy = 12.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.scale = 1.4f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.rotateDeg = 8.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.scaleX = 1.2f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.skewXDeg = 6.0f}}, {1.0f, {}}}).displaces());
  EXPECT_TRUE(fx::keys({{0.0f, {.skewYDeg = 6.0f}}, {1.0f, {}}}).displaces());

  // A COMBINATOR DERIVES: any operand it may evaluate moving is enough, and
  // none of them moving is enough the other way. `fx::hold` vetoes with
  // alpha, which places nothing, so it is its operand's answer.
  EXPECT_FALSE(fx::mix(fx::typeOn(), fx::scramble()).displaces());
  EXPECT_TRUE(fx::mix(fx::typeOn(), fx::rise()).displaces());
  EXPECT_FALSE(fx::seq(fx::typeOn().until(0.5f), fx::scramble()).displaces());
  EXPECT_TRUE(fx::seq(fx::typeOn().until(0.5f), fx::rise()).displaces());
  EXPECT_FALSE(fx::hold(fx::scramble()).displaces());
  EXPECT_TRUE(fx::hold(fx::rise()).displaces());
  // Nesting keeps the derivation exact rather than sticky.
  EXPECT_FALSE(fx::mix(fx::seq(fx::tint(SkColors::kGray, SkColors::kWhite)),
                       fx::hold(fx::typeOn()))
                   .displaces());

  // A PASS IS NOT A PLACEMENT: its shader runs over pixels already
  // rasterized at the resting origins.
  struct NoParams {};
  const auto identityPass = std::make_shared<const sigil::material::Recipe>(
      sigil::material::Recipe::of<NoParams>("test.identity-pass")
          .body(sigil::material::Target::SkSL,
                "half4 main(float2 xy) { return uContent.eval(xy); }"));
  EXPECT_FALSE(fx::pass(material::skia::Paint::recipe(
                            sigil::material::Material(identityPass)))
                   .displaces());

  // THE OPAQUE DOOR assumes motion, and takes the author's word otherwise.
  const GlyphModFn still = [](const GlyphInfo&, float,
                              sigil::core::noise::Mix64Stream&) {
    GlyphMod m;
    m.alpha = 0.5f;
    return m;
  };
  EXPECT_TRUE(fx::effect("opaque", still).displaces());
  EXPECT_FALSE(fx::effect("opaque", still).displacing(false).displaces());
  // …and the declaration rides the params, so two bodies under one key that
  // disagree about placement do not prune onto each other.
  EXPECT_FALSE(fx::effect("opaque", still) ==
               fx::effect("opaque", still).displacing(false));
  EXPECT_TRUE(fx::effect("opaque", still).displacing(false) ==
              fx::effect("opaque", still).displacing(false));
}

TEST(ComposeTextFx, KeysReproducesEveryEntryAtItsOwnPosition) {
  // A published table is a promise about the moments it names. Whatever the
  // curve between them, the deviation AT an entry is the entry.
  const std::vector<fx::Key> table = {
      {0.00f, {}},
      {0.30f, {.scaleX = 1.25f, .scaleY = 0.75f}},
      {0.65f, {.scaleX = 0.95f, .scaleY = 1.05f}},
      {1.00f, {}}};
  const TextEffect rubber = fx::keys(table, &choreograph::easeInOutCubic);
  for (const fx::Key& key : table) {
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleX, key.mod.scaleX)
        << "scaleX at " << key.at;
    EXPECT_FLOAT_EQ(evaluate(rubber, key.at).scaleY, key.mod.scaleY)
        << "scaleY at " << key.at;
  }
  // Outside the table's own span it HOLDS at the ends rather than
  // extrapolating numbers nobody published.
  EXPECT_FLOAT_EQ(evaluate(rubber, -0.5f).scaleX, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(rubber, 2.0f).scaleX, 1.0f);
}

TEST(ComposeTextFx, KeysEasesEachSegmentOnItsOwn) {
  // THE WHOLE CURVE, EVERY SEGMENT — which is what a keyframe list means
  // and what one curve stretched across the table would not be. Three
  // entries are the fewest that can tell the two apart.
  const std::vector<fx::Key> ramp = {
      {0.0f, {}}, {0.5f, {.dy = 10.0f}}, {1.0f, {}}};
  const TextEffect linear = fx::keys(ramp);
  EXPECT_FLOAT_EQ(evaluate(linear, 0.125f).dy, 2.5f);

  const TextEffect eased = fx::keys(ramp, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(eased, 0.5f).dy, 10.0f);  // the entry is still exact
  EXPECT_LT(evaluate(eased, 0.125f).dy, 1.5f)  // …the middle is not linear
      << "a quarter of the way into the first segment the reading is the "
         "linear one, so the curve was not applied to the segment";
  // The second segment runs the same curve over its own span: a quarter in
  // and a quarter from the end of the two segments are mirror readings.
  EXPECT_NEAR(evaluate(eased, 0.375f).dy, evaluate(eased, 0.625f).dy, 1e-4f);

  // A per-entry curve governs the segment that OPENS at that entry, and no
  // other.
  std::vector<fx::Key> mixed = ramp;
  mixed[0].ease = &choreograph::easeNone;
  const TextEffect part = fx::keys(mixed, &choreograph::easeInOutCubic);
  EXPECT_FLOAT_EQ(evaluate(part, 0.125f).dy, 2.5f);
  EXPECT_NEAR(evaluate(part, 0.625f).dy, evaluate(eased, 0.625f).dy, 1e-4f);
}

TEST(ComposeTextFx, KeysCutsASubstitutionAndLerpsAMatchingAxis) {
  // The seq crossfade's rules, because it is the same arithmetic: there is
  // no half-way glyph between two outlines, and an axis is the one
  // substitution with a continuum — and only between two entries naming the
  // SAME axis.
  const TextEffect letters =
      fx::keys({{0.0f, {.codepoint = U'A'}}, {1.0f, {.codepoint = U'B'}}});
  EXPECT_EQ(evaluate(letters, 0.40f).codepoint, U'A');
  EXPECT_EQ(evaluate(letters, 0.60f).codepoint, U'B');

  const sigil::weave::FontVariation light("GRAD", 400.0f);
  const sigil::weave::FontVariation heavy("GRAD", 800.0f);
  const TextEffect swept =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = heavy}}});
  const GlyphMod midway = evaluate(swept, 0.5f);
  ASSERT_TRUE(midway.axis.has_value());
  EXPECT_FLOAT_EQ(midway.axis.value_or(sigil::weave::FontVariation()).value,
                  600.0f);

  const sigil::weave::FontVariation slant("slnt", -10.0f);
  const TextEffect crossed =
      fx::keys({{0.0f, {.axis = light}}, {1.0f, {.axis = slant}}});
  const auto tagOf = [](const GlyphMod& mod) {
    return mod.axis ? std::string(mod.axis->tag, 4) : std::string("(unset)");
  };
  EXPECT_EQ(tagOf(evaluate(crossed, 0.4f)), "GRAD");
  EXPECT_EQ(tagOf(evaluate(crossed, 0.6f)), "slnt")
      << "two different axes were averaged, which names a coordinate on "
         "neither of them";
}

TEST(ComposeTextFx, AKeyTableIsComparableByItsNumbersAndItsCurves) {
  const auto table = [](float peak) {
    return std::vector<fx::Key>{
        {0.0f, {}}, {0.5f, {.scaleY = peak}}, {1.0f, {}}};
  };
  EXPECT_TRUE(fx::keys(table(1.25f)) == fx::keys(table(1.25f)));
  EXPECT_FALSE(fx::keys(table(1.25f)) == fx::keys(table(1.30f)));
  // The curve is part of the identity. A table re-eased is a different
  // motion, and an effect comparing equal to the one it replaced would go
  // on drawing the old one with no diagnostic.
  EXPECT_FALSE(fx::keys(table(1.25f)) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_FALSE(fx::keys(table(1.25f), &choreograph::easeOutQuad) ==
               fx::keys(table(1.25f), &choreograph::easeInOutCubic));
  EXPECT_TRUE(fx::keys(table(1.25f), &choreograph::easeInOutCubic) ==
              fx::keys(table(1.25f), &choreograph::easeInOutCubic));
}

TEST(ComposeTextFx, AKeyedTrackPrunesWhenItsTableIsUnchanged) {
  Host host;
  const auto tree = [] {
    return box().padding(10).child(
        text(u8"KEYS", whiteStyle(28))
            .key("k")
            .fx({.effect =
                     fx::keys({{0.0f, {}}, {0.5f, {.dy = -8.0f}}, {1.0f, {}}},
                              &choreograph::easeInOutCubic)}));
  };
  host.composer.render(tree());
  for (int i = 0; i < 4; ++i) host.frame(0.016);
  host.composer.render(tree());  // fresh Elements, an identical table
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
  host.frame(0.016);
  EXPECT_EQ(host.composer.stats().picturesRecorded, 0u);

  // The control: a table with one number moved is a different value, and
  // the node it describes has to be patched.
  host.composer.render(box().padding(10).child(
      text(u8"KEYS", whiteStyle(28))
          .key("k")
          .fx({.effect =
                   fx::keys({{0.0f, {}}, {0.5f, {.dy = -9.0f}}, {1.0f, {}}},
                            &choreograph::easeInOutCubic)})));
  EXPECT_GT(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposeTextFx, HoldWithholdsTheEffectUntilTheBeatOpens) {
  const TextEffect held = fx::hold(constantDy(10));
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).alpha, 0.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.0f).dy, 0.0f)
      << "the wrapped effect ran on a beat that had not opened";
  // …and the moment it opens, EXACTLY the wrapped effect.
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).alpha, 1.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 0.001f).dy, 10.0f);
  EXPECT_FLOAT_EQ(evaluate(held, 1.0f).dy, 10.0f);

  EXPECT_TRUE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(20)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::hold(fx::rise(22)));
  EXPECT_FALSE(fx::hold(fx::rise(20)) == fx::rise(20));
  EXPECT_FLOAT_EQ(fx::hold(fx::rise(20)).reach(), fx::rise(20).reach());
}
