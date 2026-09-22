#include "RotaConvocationis.h"

auto RotaConvocationis::wheel() -> Element {
  // THE COMPASS WORK, AS A TABLE. Every rule is half of a PAIR, and the pair
  // is what makes a band: no line on this plate stands on its own, and nothing
  // is written anywhere except between two of them. Each row is the key, the
  // chalk path it is struck along, the width, the ink, the second it opens and
  // how long the sweep takes.
  struct Ruled {
    const char* key;
    int chalk;
    float width;
    sigil::material::Color ink;
    double at, dur;
  };
  const Ruled kRuled[] = {
      {"r-edge", kChalkEdge, 2.1f, kIron, 0.35, 1.3},
      {"r-edge-in", kChalkEdgeIn, 0.6f, kIronDim, 0.45, 1.2},
      {"r-rune-out", kChalkRuneOut, 1.1f, kIron, 0.55, 1.3},
      {"r-rune-in", kChalkRuneIn, 1.1f, kIron, 0.62, 1.3},
      {"r-vox-out", kChalkVoxOut, 0.5f, kIronDim, 0.70, 1.2},
      {"r-vox-in", kChalkVoxIn, 1.0f, kIron, 0.76, 1.3},
      {"r-tick-mid", kChalkTickMid, 0.5f, kIronDim, 0.86, 1.2},
      {"r-nom-out", kChalkNomOut, 1.2f, kIron, 0.86, 1.3},
      {"r-nom-in", kChalkNomIn, 1.2f, kIron, 0.94, 1.3},
      {"r-nom-case", kChalkNomCase, 0.5f, kIronDim, 1.02, 1.2},
      {"r-tex-out", kChalkTexOut, 0.9f, kIron, tTex - 0.5, 1.0},
      {"r-tex-in", kChalkTexIn, 0.9f, kIron, tTex - 0.35, 1.0},
      {"r-tex-case", kChalkTexCase, 0.5f, kIronDim, tTex, 1.0},
      {"r-serr-in", kChalkSerrIn, 0.5f, kIronDim, tTex + 0.15, 1.0},
      {"r-env", kChalkEnv, 1.0f, kIron, tStar + 0.3, 1.0},
      {"r-env-in", kChalkEnvIn, 0.5f, kIronDim, tStar + 0.45, 1.0},
      {"r-hub-out", kChalkHubOut, 1.6f, kIron, tInner - 0.4, 0.8},
      {"r-hub-in", kChalkHubIn, 0.6f, kIronDim, tInner - 0.3, 0.8},
      {"r-hub-case", kChalkHubCase, 0.9f, kIron, tInner - 0.2, 0.8},
      {"r-hub-kern", kChalkHubKern, 0.7f, kIron, tInner + 0.1, 0.8}};

  // THE DIVISION LADDERS: the serration at the rim, three length classes
  // inside it at two pitches, the serration round the texture band and the
  // hub's own teeth — the density an engraved figure carries and a single
  // ladder cannot. Different bands take different symmetry orders on purpose;
  // a plate whose every ring counted twelve would read as one drawing rather
  // than as several mechanisms. Every count is a multiple of the twelve the
  // plate is built on, and the mid class is offset HALF A STEP from the long
  // one so the two interleave instead of doubling up.
  struct Ladder {
    const char* key;
    int count, skip;
    float outer, inner, width;
    sigil::material::Color ink;
    double at, dur;
    float from;
  };
  const Ladder kLadders[] = {
      {"teeth", 240, 12, rEdge, rEdgeIn, 0.7f, kIron, 0.5, 1.7, 0.0f},
      {"ticks-long", kStations, 0, rTickOut, rTickMid - 0.028f, 1.3f,
       sigil::material::skia::toSkColor(kAsh) Dim, 0.9, 1.4, 0.0f},
      {"ticks-mid", 24, 2, rTickOut, rTickMid - 0.010f, 0.9f, kIron, 0.9, 1.6,
       kPitch * 0.5f},
      {"ticks-short", 144, 6, rTickOut, rTickMid, 0.7f, kIron, 0.9, 2.1, 0.0f},
      {"serration", 48, 4, rTexCase, rSerrIn, 0.7f, kIron, tTex + 0.1, 1.4,
       kPitch * 0.25f},
      {"hub-teeth", 72, 6, rHubOut, rHubIn, 0.6f, kIronDim, tInner - 0.35, 0.9,
       0.0f}};

  return layer("rota").children(
      {// the ground wash under the figure
       layer("rota-wash")
           .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.62f,
                                        {{0.0f, kNightLift},
                                         {0.66f, hexColor(0x0D0A18)},
                                         {1.0f, kNight}})),
       // THE FLOOD: light thrown at the whole sheet from behind the figure.
       // It screens, so it lifts what is already there toward white instead
       // of laying a wash over it, and it is worth its full-panel gradient
       // only while it is on — at gain zero the node is not painted at all.
       layer("flood", -120)
           .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.86f,
                                        {{0.0f, hexColor(0xFFD98A, 0.55f)},
                                         {0.42f, hexColor(0xE79A32, 0.30f)},
                                         {1.0f, hexColor(0xC96F1E, 0.0f)}}))
           .blendMode(SkBlendMode::kScreen)
           .opacity(&floodA),
       // THE RAYS, thrown past the figure at ignition — the reading that
       // makes an ignition a whole-frame event and not a brighter drawing.
       // They pass BEHIND the lettering: light coming out from the figure is
       // occluded by the figure, and rays laid over the type would only be a
       // veil across the words.
       layer("rays", -170)
           .fill(mskia::Paint::recipe(sigil::material::Material(raysRecipe()))
                     .uniform("uInk", kHalo))
           .blendMode(SkBlendMode::kPlus)
           .opacity(&raysA),
       each(kRuled,
            [this](const Ruled& r) {
              return rule(r.key, r.chalk, r.width, r.ink, r.at, r.dur);
            }),
       each(kLadders,
            [this](const Ladder& l) {
              return ladder(l.key, l.count, l.skip, l.outer, l.inner, l.width,
                            l.ink, l.at, l.dur, l.from);
            }),
       // THE FAST LAYER: a hairline ladder in the tick band's inner half,
       // turning at seconds per revolution against everything inside it. It is
       // the layer that can afford the rate — cached geometry replayed under a
       // bound transform, where the lettering it sits beside would have to
       // re-place every glyph.
       layer("ladder-turn")
           .rotate(motion::bind(&tickSpin).target(0.0f, 360.0f))
           .children({ladder("ticks-fine", 288, 6, rTickMid - 0.003f, rTickIn,
                             0.6f, kIron, 1.1, 2.3)}),
       // the bands the rules frame
       invocatio(), registrum(), nomina(), textura(),
       // the turning layers, outside in
       arcus(), stella(), stellaInterior(), limina(),
       // THE HUB'S OWN FRAME is struck with the inner compound, not with the
       // emblem it will hold: the centre of a figure like this is never a hole
       // waiting to be filled, and a plate whose middle is empty for eight
       // seconds reads as unfinished rather than as forming.
       emblemDisc(), emblema(), monogramma(),
       // the light that lands on the struck rules, group by group. The ones
       // that do not turn are painted here; the ones that do are painted
       // inside the layers that turn them.
       emissive("rim-lit", glows[kGlowRim], &litRim),
       emissive("nom-lit", glows[kGlowNom], &litNom),
       // THE CARRIER: one node holding all twelve seals, turning them about
       // the circle's own centre. It is painted here, after the rim's light,
       // for the same reason each seal carries an opaque ground — a seal SITS
       // ON the plate, and a rule that runs under one does not print across it
       // however hard the rule is burning.
       //
       // A ring of stations that turns is what makes the seals read as MOUNTED
       // rather than as drawn at twelve places: the whole rim is one
       // mechanism, and the exception that proves it is the medallion below,
       // which is at the rim and does not travel — the seals pass behind it.
       layer("ferrum")
           .rotate(motion::bind(&sealOrbit).target(0.0f, 360.0f))
           .children(each(kSeals, [this](int k) { return sigillum(k); })),
       spur(),
       // the embers: a live pool stamped as one draw, rising off the rim at
       // ignition and drizzling for as long as the circle is charged
       layer("embers").opacity(&emberA).children({instancing::instances(
           emberAtlas, embers, instancing::Mode::Live, SkBlendMode::kPlus)}),
       // THE CREST'S FRINGE: half a second of the frame beneath re-sampled
       // with its channels pulled apart along the radius.
       layer("fringe")
           .backdropFilter(mskia::Effect::shader(
                               fringeFx, {{"uCx", kEye.x()}, {"uCy", kEye.y()}})
                               .uniform("uSpread", &fringeK))
           .opacity(&fringeA),
       // the scribe: the point of the pen, led round the band by the writing
       // cascade — placed every frame from the schedule read back, so it
       // cannot drift from the letters it appears to write
       box()
           .key("scribe")
           .rect(SkRect::MakeXYWH(-9, -9, 18, 18))
           .hitTestable(false)
           .fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 0.5f,
                                        {{0.0f, hexColor(0xFFE9B0)},
                                         {0.35f, hexColor(0xD8A94E, 0.55f)},
                                         {1.0f, hexColor(0xD8A94E, 0.0f)}}))
           .translateX(&scribeX)
           .translateY(&scribeY)
           .opacity(&scribeA)});
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
      .styleClass("label")
      .children(
          {text("ROTA CONVOCATIONIS")
               .font({.size = 12.0f,
                      .color = sigil::material::skia::toSkColor(kAshDim),
                      .track = 5.2f})
               .key("titulus")
               // A lozenge stands at the word the whole figure
               // converges on, anchored to the rect the selector
               // resolves rather than to a number a caller measured.
               .textAttach(weave::selectors::text(u8"ROTA"),
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
                    .stagger = {.eachMs = 0,
                                .amountMs = 420,
                                .durationMs = 520},
                    .progress = beat(0.35, 1.8)}),
           text(std::to_string(totalGlyphs) +
                " GLYPHS · 23 CVRVED BASELINES · 10 "
                "TVRNING LAYERS · EVERY START CHAINED FROM A "
                "SPAN, NONE FITTED BY HAND")
               .font({.size = 8.5f,
                      .color = sigil::material::skia::toSkColor(
                          hexColor(0x8A8299, 0.42f)),
                      .track = 2.4f})
               .key("colophon-2")
               .opacity(beat(tIgnite + 0.4, tIgnite + 1.2))});
}
