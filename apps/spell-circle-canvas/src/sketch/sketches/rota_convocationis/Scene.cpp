#include "RotaConvocationis.h"

auto RotaConvocationis::wheel() -> Element {
  Element panel = box().key("rota").absolute().inset(0).hitTestable(false);

  // The ground wash under the figure.
  panel.child(box()
                  .key("rota-wash")
                  .absolute()
                  .inset(0)
                  .hitTestable(false)
                  .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.62f,
                                               {{0.0f, kNightLift},
                                                {0.66f, hexColor(0x0D0A18)},
                                                {1.0f, kNight}})));

  // THE FLOOD: light thrown at the whole sheet from behind the figure.
  // It screens, so it lifts what is already there toward white instead
  // of laying a wash over it, and it is worth its full-panel gradient
  // only while it is on — at gain zero the node is not painted at all.
  panel.child(
      box()
          .key("flood")
          .absolute()
          .inset(-120)
          .hitTestable(false)
          .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.86f,
                                       {{0.0f, hexColor(0xFFD98A, 0.55f)},
                                        {0.42f, hexColor(0xE79A32, 0.30f)},
                                        {1.0f, hexColor(0xC96F1E, 0.0f)}}))
          .blend(SkBlendMode::kScreen)
          .opacity(&floodA));

  // THE RAYS, thrown past the figure at ignition — the reading that
  // makes an ignition a whole-frame event and not a brighter drawing.
  // They pass BEHIND the lettering: light coming out from the figure is
  // occluded by the figure, and rays laid over the type would only be a
  // veil across the words.
  panel.child(
      box()
          .key("rays")
          .absolute()
          .inset(-170)
          .hitTestable(false)
          .fill(mskia::Paint::recipe(sigil::material::Material(raysRecipe()))
                    .uniform("uInk", kHalo))
          .blend(SkBlendMode::kPlus)
          .opacity(&raysA));

  // THE COMPASS WORK. Every rule below is half of a PAIR, and the pair
  // is what makes a band: no line on this plate stands on its own, and
  // nothing is written anywhere except between two of them.
  panel.child(rule("r-edge", kChalkEdge, 2.1f, kIron, 0.35, 1.3));
  panel.child(rule("r-edge-in", kChalkEdgeIn, 0.6f, kIronDim, 0.45, 1.2));
  panel.child(rule("r-rune-out", kChalkRuneOut, 1.1f, kIron, 0.55, 1.3));
  panel.child(rule("r-rune-in", kChalkRuneIn, 1.1f, kIron, 0.62, 1.3));
  panel.child(rule("r-vox-out", kChalkVoxOut, 0.5f, kIronDim, 0.70, 1.2));
  panel.child(rule("r-vox-in", kChalkVoxIn, 1.0f, kIron, 0.76, 1.3));
  panel.child(rule("r-nom-out", kChalkNomOut, 1.2f, kIron, 0.86, 1.3));
  panel.child(rule("r-nom-in", kChalkNomIn, 1.2f, kIron, 0.94, 1.3));
  panel.child(rule("r-nom-case", kChalkNomCase, 0.5f, kIronDim, 1.02, 1.2));

  // THE DIVISION LADDERS: the serration at the rim, and three length
  // classes inside it at two pitches — the density an engraved figure
  // carries and a single ladder cannot. Different bands take different
  // symmetry orders on purpose; a plate whose every ring counted twelve
  // would read as one drawing rather than as several mechanisms.
  panel.child(ladder("teeth", 240, 12, rEdge, rEdgeIn, 0.7f, kIron, 0.5, 1.7));
  // The ladder band is fenced like every other band and split in two:
  // the still classes in its outer half, the turning one in its inner
  // half, so two ladders never stamp the same mark. Every count is a
  // multiple of the twelve the plate is built on, and the mid class is
  // offset HALF A STEP from the long one so the two interleave instead
  // of doubling up.
  panel.child(rule("r-tick-mid", kChalkTickMid, 0.5f, kIronDim, 0.86, 1.2));
  panel.child(ladder("ticks-long", kStations, 0, rTickOut, rTickMid - 0.028f,
                     1.3f, kAshDim, 0.9, 1.4));
  panel.child(ladder("ticks-mid", 24, 2, rTickOut, rTickMid - 0.010f, 0.9f,
                     kIron, 0.9, 1.6, kPitch * 0.5f));
  panel.child(
      ladder("ticks-short", 144, 6, rTickOut, rTickMid, 0.7f, kIron, 0.9, 2.1));

  // THE FAST LAYER: a hairline ladder in the band's inner half, turning
  // at seconds per revolution against everything inside it. It is the
  // layer that can afford the rate — cached geometry replayed under a
  // bound transform, where the lettering it sits beside would have to
  // re-place every glyph.
  panel.child(box()
                  .key("ladder-turn")
                  .absolute()
                  .inset(0)
                  .hitTestable(false)
                  .rotate(motion::bind(&tickSpin).target(0.0f, 360.0f))
                  .child(ladder("ticks-fine", 288, 6, rTickMid - 0.003f,
                                rTickIn, 0.6f, kIron, 1.1, 2.3)));

  // The bands the rules frame.
  panel.child(invocatio());
  panel.child(registrum());
  panel.child(nomina());

  // The texture band and its own frame.
  panel.child(rule("r-tex-out", kChalkTexOut, 0.9f, kIron, tTex - 0.5, 1.0));
  panel.child(rule("r-tex-in", kChalkTexIn, 0.9f, kIron, tTex - 0.35, 1.0));
  panel.child(rule("r-tex-case", kChalkTexCase, 0.5f, kIronDim, tTex, 1.0));
  panel.child(
      rule("r-serr-in", kChalkSerrIn, 0.5f, kIronDim, tTex + 0.15, 1.0));
  panel.child(ladder("serration", 48, 4, rTexCase, rSerrIn, 0.7f, kIron,
                     tTex + 0.1, 1.4, kPitch * 0.25f));
  panel.child(textura());

  // The turning layers, outside in.
  panel.child(arcus());
  panel.child(rule("r-env", kChalkEnv, 1.0f, kIron, tStar + 0.3, 1.0));
  panel.child(rule("r-env-in", kChalkEnvIn, 0.5f, kIronDim, tStar + 0.45, 1.0));
  panel.child(stella());
  panel.child(stellaInterior());
  panel.child(limina());

  // THE HUB'S OWN FRAME is struck with the inner compound, not with the
  // emblem it will hold: the centre of a figure like this is never a
  // hole waiting to be filled, and a plate whose middle is empty for
  // eight seconds reads as unfinished rather than as forming.
  panel.child(rule("r-hub-out", kChalkHubOut, 1.6f, kIron, tInner - 0.4, 0.8));
  panel.child(rule("r-hub-in", kChalkHubIn, 0.6f, kIronDim, tInner - 0.3, 0.8));
  panel.child(
      rule("r-hub-case", kChalkHubCase, 0.9f, kIron, tInner - 0.2, 0.8));
  panel.child(ladder("hub-teeth", 72, 6, rHubOut, rHubIn, 0.6f, kIronDim,
                     tInner - 0.35, 0.9));
  panel.child(
      rule("r-hub-kern", kChalkHubKern, 0.7f, kIron, tInner + 0.1, 0.8));
  panel.child(emblemDisc());
  panel.child(emblema());
  panel.child(monogramma());

  // The light that lands on the struck rules, group by group. The ones
  // that do not turn are painted here; the ones that do are painted
  // inside the layers that turn them.
  panel.child(emissive("rim-lit", glows[kGlowRim], &litRim));
  panel.child(emissive("nom-lit", glows[kGlowNom], &litNom));

  // THE CARRIER: one node holding all twelve seals, turning them about
  // the circle's own centre. It is painted here, after the rim's light,
  // for the same reason each seal carries an opaque ground — a seal
  // SITS ON the plate, and a rule that runs under one does not print
  // across it however hard the rule is burning.
  //
  // A ring of stations that turns is what makes the seals read as
  // MOUNTED rather than as drawn at twelve places: the whole rim is one
  // mechanism, and the exception that proves it is the medallion below,
  // which is at the rim and does not travel — the seals pass behind it.
  Element ferrum =
      box().key("ferrum").absolute().inset(0).hitTestable(false).rotate(
          motion::bind(&sealOrbit).target(0.0f, 360.0f));
  for (int k = 0; k < kSeals; ++k) ferrum.child(sigillum(k));
  panel.child(std::move(ferrum));

  panel.child(spur());

  // The embers: a live pool stamped as one draw, rising off the rim at
  // ignition and drizzling for as long as the circle is charged.
  panel.child(
      box()
          .key("embers")
          .absolute()
          .inset(0)
          .hitTestable(false)
          .opacity(&emberA)
          .child(instancing::instances(
              emberAtlas, embers, instancing::Mode::Live, SkBlendMode::kPlus)));

  // THE CREST'S FRINGE: half a second of the frame beneath re-sampled
  // with its channels pulled apart along the radius.
  panel.child(box()
                  .key("fringe")
                  .absolute()
                  .inset(0)
                  .hitTestable(false)
                  .backdrop(mskia::Effect::shader(fringeFx, {{"uCx", kEye.x()},
                                                             {"uCy", kEye.y()}})
                                .uniform("uSpread", &fringeK))
                  .opacity(&fringeA));

  // The scribe: the point of the pen, led round the band by the writing
  // cascade — placed every frame from the schedule read back, so it
  // cannot drift from the letters it appears to write.
  panel.child(
      box()
          .key("scribe")
          .left(-9)
          .top(-9)
          .width(18)
          .height(18)
          .hitTestable(false)
          .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.5f,
                                       {{0.0f, hexColor(0xFFE9B0)},
                                        {0.35f, hexColor(0xD8A94E, 0.55f)},
                                        {1.0f, hexColor(0xD8A94E, 0.0f)}}))
          .translateX(&scribeX)
          .translateY(&scribeY)
          .opacity(&scribeA));
  return panel;
}

auto RotaConvocationis::colophon() -> Element {
  return box()
      .key("colophon")
      .absolute()
      .left(0)
      .right(0)
      .bottom(26)
      .column()
      .alignItems(Align::Center)
      .gap(7)
      .hitTestable(false)
      .child(
          text(toUtf8("ROTA CONVOCATIONIS"), label(12.0f, kAshDim, 5.2f))
              .key("titulus")
              // A lozenge stands at the word the whole figure
              // converges on, anchored to the rect the selector
              // resolves rather than to a number a caller measured.
              .mark(weave::selectors::text(u8"ROTA"),
                    box()
                        .key("m-rota")
                        .left(pct(50))
                        .top(pct(126))
                        .width(5)
                        .height(5)
                        .shape(shapes::polygon(4))
                        .fill(Fill::color(kGold))
                        .opacity(beat(tIgnite + 0.9, tIgnite + 1.5)))
              .fx({.effect = fx::rise(10.0f),
                   .stagger = {.eachMs = 0, .amountMs = 420, .durationMs = 520},
                   .progress = beat(0.35, 1.8)}))
      .child(text(toUtf8(std::to_string(totalGlyphs) +
                         " GLYPHS \xc2\xb7 23 CVRVED BASELINES \xc2\xb7 10 "
                         "TVRNING LAYERS \xc2\xb7 EVERY START CHAINED FROM A "
                         "SPAN, NONE FITTED BY HAND"),
                  label(8.5f, hexColor(0x8A8299, 0.42f), 2.4f))
                 .key("colophon-2")
                 .opacity(beat(tIgnite + 0.4, tIgnite + 1.2)));
}
