#include "RotaConvocationis.h"

auto RotaConvocationis::arcus() -> Element {
  Element turn =
      box().key("arcus").absolute().inset(0).hitTestable(false).rotate(
          motion::bind(&arcSpin).target(0.0f, 360.0f));
  turn.child(rule("arc-chalk", kChalkArcs, 1.7f, kIron, tArc, 1.1));
  turn.child(rule("arc-dash", kChalkDash, 1.0f, kIron, tArc - 0.3, 1.0));
  turn.child(line("arc-spokes", arcSpokes, 0.9f, kIron, tArc + 0.2, 0.9));
  turn.child(line("arc-nodes", arcNodes, 1.0f, kIron, tArc + 0.35, 0.8));
  turn.child(emissive("arc-lit", glows[kGlowArc], &litArc));
  return turn;
}

auto RotaConvocationis::spur() -> Element {
  const SkPoint c = P(0.0f, rEdge);
  return box()
      .key("spur")
      .absolute()
      .inset(0)
      .hitTestable(false)
      .child(kit::disc(c, kSpurR)
                 .key("spur-ground")
                 .hitTestable(false)
                 .fill(Fill::color(hexColor(0x0D0A16, 0.92f)))
                 .opacity(beat(0.9, 1.4)))
      .child(line("spur-rules", spurRules, 1.2f, kIron, 1.0, 0.9))
      .child(text(toUtf8(spurText), rune(19.0f, kBone, 0.0f))
                 .key("spur-glyph")
                 .centerAt(c)
                 .hitTestable(false)
                 .fx({.effect = fx::hold(fx::pop(0.5f)),
                      .stagger = {.durationMs = 360},
                      .progress = beat(1.3, 1.8)}))
      .child(emissive("spur-lit", glows[kGlowSpur], &litSpur));
}

auto RotaConvocationis::stella() -> Element {
  Element turn =
      box().key("stella").absolute().inset(0).hitTestable(false).rotate(
          motion::bind(&starSpin).target(0.0f, 360.0f));
  turn.child(rule("star-chalk", kChalkStar, 1.5f, kIron, tStar, 1.3));
  // THE CRESCENTS: three double-arcs laid ACROSS the compound, 120°
  // apart, each closed at both ends by a radial tie and carrying its
  // own short ladder. They are the plate's single THREE-FOLD mark —
  // applied over the mechanism rather than being a band of it — and
  // they ride the compound's rotation, so three things visibly sweep
  // past twelve.
  turn.child(rule("cresc-chalk", kChalkCresc, 1.3f, kIron, tStar + 0.5, 1.0));
  turn.child(line("cage-fence-o", cageFenceOut, 0.9f, kIronDim, tStar, 1.0));
  turn.child(line("cage-fence-i", cageFenceIn, 0.9f, kIronDim, tStar, 1.0));
  turn.child(line("cage-ladder", cageLadder, 0.6f, kIronDim, tStar + 0.1, 1.2));
  turn.child(line("cage-beads", cageBeads, 0.9f, kIron, tStar + 0.3, 0.9));
  turn.child(line("star-spokes", starSpokes, 0.8f, kIronDim, tStar + 0.2, 1.0));
  turn.child(line("star-nodes", starNodes, 1.0f, kIron, tStar + 0.4, 0.9));
  for (int i = 0; i < (int)starSteps.size(); ++i)
    turn.child(box()
                   .key("star-morph" + std::to_string(i))
                   .absolute()
                   .inset(0)
                   .hitTestable(false)
                   .shape(heldPath(starSteps[(size_t)i]))
                   .fill(Fill::none())
                   .stroke(stroke(1.6f, Fill::color(kCore)))
                   .blend(SkBlendMode::kPlus)
                   .opacity(motion::bind(&morphStep)
                                .window((float)i - 1.0f, (float)i + 1.0f)
                                .pingPong()));
  turn.child(emissive("star-lit", glows[kGlowStar], &litStar));
  return turn;
}

auto RotaConvocationis::stellaInterior() -> Element {
  Element turn = box()
                     .key("stella-int")
                     .absolute()
                     .inset(0)
                     .hitTestable(false)
                     .rotate(motion::bind(&innerSpin).target(0.0f, 360.0f));
  turn.child(rule("inner-chalk", kChalkInner, 1.2f, kIron, tInner, 1.2));
  turn.child(
      line("inner-spokes", innerSpokes, 0.7f, kIronDim, tInner + 0.2, 1.0));
  turn.child(emissive("inner-lit", glows[kGlowInner], &litInner));
  return turn;
}

auto RotaConvocationis::limina() -> Element {
  Element fig = kit::disc(kEye, rStar * kR).key("limina").hitTestable(false);
  const Shape chordPath =
      shapes::chords({.sides = kStations, .step = 3, .inset = 74.0f});
  for (int k = 0; k < kLimens; ++k) {
    fig.child(text(toUtf8(kLimina[k]), label(11.0f, kAsh, 2.0f))
                  .key("limen" + std::to_string(k))
                  .absolute()
                  .inset(0)
                  .hitTestable(false)
                  .onPath({.path = chordPath,
                           .at = ((float)(k * 2) + 0.5f) / (float)kStations,
                           .align = TextPath::Align::Center,
                           .offset = 5.0f,
                           .autoFlip = true})
                  .fx({.effect = fx::typeOn(),
                       .stagger = {.eachMs = 30, .durationMs = 120},
                       .progress = beat(limenAt[k], limenAt[k] + limenSpanS)}));
  }
  return fig;
}

auto RotaConvocationis::sigillum(int k) -> Element {
  const Seal& s = kSealTable[k];
  const SkPoint c = P(station(k), rSealRide);
  const double at = tSeal[k];
  const std::string id = "seal" + std::to_string(k);

  Element seal = kit::disc(c, kSealR).key(id);
  // The ground: occludes the bands under the seal — a seal SITS ON the
  // plate rather than being drawn into it — and wears an aura for a
  // breath at ignition.
  seal.child(box()
                 .key(id + "-ground")
                 .absolute()
                 .inset(0)
                 .corners({kSealR})
                 .hitTestable(false)
                 .fill(Fill::color(hexColor(0x0D0A16, 0.94f)))
                 // The ground is dressed rather than shaded: an inner glow is
                 // a blurred band hugging its own edge, a value decoration
                 // that records once with the disc it sits on. It gives the
                 // seal a lip of light without a second node and without a
                 // shader.
                 .overlay(styles::innerGlow(hexColor(0xE79A32, 0.30f), 8.0f))
                 .opacity(beat(at, at + 0.4)));
  // The seal's own emissive rule. A seal is a small magic circle, so it
  // lights like one — but its two rules are concentric and cross
  // nothing, which is the case the SDF answers in one pass: silhouette,
  // core and halo are three uniforms of one shader rather than a union
  // and four fills.
  {
    const sdf::Style lit{
        .borderWidth = 1.1f,
        .borderColor = {kCore.fR, kCore.fG, kCore.fB, kCore.fA},
        .glowRadius = 6.0f,
        .glowColor = sigil::material::rgb(0xFFC152, 0.42f)};
    const float side = sdf::minBoxFor(lit, 2.0f * kSealR);
    // BAKED, because the seal count is the whole point of the rim: the
    // figure is one static shader and only its gain moves, so twelve of
    // them evaluating the field every frame is twelve times a cost that
    // is paid once. The blend and the gain ride the blit.
    seal.child(
        box()
            .key(id + "-lit")
            .absolute()
            .inset(kSealR - side * 0.5f)
            .hitTestable(false)
            .cache(Cache::Texture)
            .fill(mskia::Paint::recipe(sdf::material(sdf::circle(), lit)))
            .blend(SkBlendMode::kPlus)
            .opacity(&litSeal[k]));
  }
  // The rules, struck as sweeps.
  seal.child(box()
                 .key(id + "-rule-out")
                 .absolute()
                 .inset(0)
                 .corners({kSealR})
                 .hitTestable(false)
                 .fill(Fill::none())
                 .stroke(spans::upTo(beat(at, at + 0.55)),
                         stroke(1.2f, Fill::color(kIron))))
      .child(box()
                 .key(id + "-rule-in")
                 .absolute()
                 .inset(kSealR - kSealRing + 7.0f)
                 .corners({kSealRing - 7.0f})
                 .hitTestable(false)
                 .fill(Fill::none())
                 .stroke(spans::upTo(beat(at + 0.15, at + 0.7)),
                         stroke(0.7f, Fill::color(kIronDim))));
  // THE TURNING BODY: everything in the seal that is not a circle and
  // not the ordinal. The rules and the ground are concentric discs and
  // a disc under rotation is the same disc, so they stay outside this
  // node and nothing pays for turning them.
  Element body = box()
                     .key(id + "-body")
                     .absolute()
                     .inset(0)
                     .hitTestable(false)
                     .rotate(motion::bind(&sealSpin[k]).target(0.0f, 360.0f));
  // The order-sided polygon, turning AGAINST its own seal once lit, so
  // the figure inside a seal and the seal around it are visibly two
  // mechanisms and not one drawing.
  body.child(box()
                 .key(id + "-poly")
                 .absolute()
                 .inset(kSealR - 16.0f)
                 .hitTestable(false)
                 .shape(shapes::polygon(s.order))
                 .fill(Fill::none())
                 .stroke(stroke(0.9f, Fill::color(kIron)))
                 .rotate(motion::bind(&sealCog[k]).target(0.0f, 360.0f))
                 .opacity(beat(at + 0.3, at + 0.9)));
  // The ring: two words of the register, tumbling onto the circle and
  // then carried round by the body it belongs to. Its phase is a plain
  // number — a twelfth per station, so no two seals open their text at
  // the same clock angle — because the turning is the body's and a run
  // that is not driving its own placement can rest at whole pixels
  // until an ancestor moves it.
  body.child(text(toUtf8(sealText[k]), ring(sealSize[k], kBone, 1.4f))
                 .key(id + "-ring")
                 .absolute()
                 .inset(kSealR - kSealRing)
                 .hitTestable(false)
                 // …and baked for the same reason: once its cascade has
                 // landed the run is a settled picture that the body
                 // turns, and a replay would re-draw every glyph of every
                 // seal on every frame.
                 .cache(Cache::Texture)
                 .onPath({.path = shapes::circle(),
                          .at = (float)k / (float)kSeals,
                          .align = TextPath::Align::Start,
                          .offset = -sealSize[k] * 0.34f,
                          .autoFlip = false})
                 .fx({.effect = fx::hold(fx::spinIn(70.0f, 9.0f)),
                      .stagger = {.eachMs = 30, .durationMs = 480},
                      .progress = beat(at + 0.25, at + 0.25 + sealSpanS)}));
  seal.child(std::move(body));
  // THE ORDINAL at the centre, decoding — held, so a numeral waiting
  // its beat is absent rather than churning wrong — and GIMBALLED: it
  // carries the carrier's rotation backwards, so the one mark on the
  // seal that has to be read stands upright at every station the rim
  // brings it to while everything around it turns.
  seal.child(
      text(toUtf8(s.ordo), mono(12.0f, kGold, 1.0f))
          .key(id + "-ordo")
          .centerAt({kSealR, kSealR})
          .hitTestable(false)
          .rotate(motion::bind(&sealUpright).target(0.0f, 360.0f))
          .effect(styles::textGlow(kHalo, 3.0f))
          .fx({.effect = fx::hold(fx::scramble(U"IVXLC", 12)),
               .stagger = {.eachMs = 90, .durationMs = 620},
               .progress = beat(at + 0.55, at + 0.55 + sealSpanS * 0.9)})
          .fx({.effect = fx::keys({{0.00f, {}},
                                   {0.80f, {}},
                                   {0.90f, {.colorAdd = kCore}},
                                   {1.00f, {}}}),
               .stagger = {.eachMs = 90, .durationMs = 620},
               .progress = beat(at + 0.55, at + 0.55 + sealSpanS * 0.9)}));
  return seal;
}

auto RotaConvocationis::emblemDisc() -> Element {
  const sdf::Style emblem{.fill = sigil::material::rgb(0x1A1008, 0.66f),
                          .glowRadius = 34.0f,
                          .glowColor = sigil::material::rgb(0xFFB13A, 0.5f)};
  const float side = sdf::minBoxFor(emblem, 2.0f * rEmblem * kR);
  return kit::disc(kEye, side * 0.5f)
      .key("emblem-disc")
      .hitTestable(false)
      .fill(mskia::Paint::recipe(sdf::material(sdf::circle(), emblem)))
      .blend(SkBlendMode::kPlus)
      .opacity(&litHub);
}

auto RotaConvocationis::emblema() -> Element {
  Element hub =
      box().key("emblem").absolute().inset(0).hitTestable(false).rotate(
          motion::bind(&hexSpin).target(0.0f, 360.0f));
  hub.child(rule("hex-chalk", kChalkHexagram, 1.1f, kIron, tInner, 0.8));
  hub.child(line("hub-dots", hubDots, 1.0f, kIron, tInner + 0.3, 0.7));
  hub.child(line("hub-motes", hubMotes, 0.9f, kIronDim, tInner + 0.5, 0.7));
  hub.child(emissive("hub-lit", glows[kGlowHub], &litHub));
  hub.child(
      text(toUtf8(hubRuneText), rune(13.0f, kAsh, 0.0f))
          .key("hub-ring")
          .absolute()
          .inset(0)
          .hitTestable(false)
          .onPath({.path = shapes::chords(
                       {.sides = 6,
                        .step = 2,
                        .radius = rHexagram * kR / (std::min(kW, kH) * 0.5f),
                        .inset = 14.0f}),
                   .at = 0.0f,
                   .align = TextPath::Align::Start,
                   .offset = 4.0f,
                   .autoFlip = true})
          .fx({.effect = fx::hold(fx::typeOn()),
               .stagger = {.eachMs = 26, .durationMs = 260},
               .progress = beat(tHub - 0.3, tHub + 0.9)}));
  return hub;
}

auto RotaConvocationis::monogramma() -> Element {
  motion::Spread letters = {.eachMs = 240, .durationMs = 760};
  return text(toUtf8(emblemText), rune(52.0f, kBone, 6.0f))
      .key("monogramma")
      .centerAt(kEye)
      .hitTestable(false)
      .effect(styles::textGlow(kHalo, 7.0f))
      .fx({.effect = fx::hold(fx::spinIn(90.0f, 14.0f)),
           .stagger = letters,
           .progress = beat(tHub, tHub + hubSpanS)})
      .fx({.effect = fx::keys({{0.00f, {}},
                               {0.74f, {}},
                               {0.88f, {.colorAdd = kHalo}},
                               {1.00f, {}}}),
           .stagger = letters,
           .progress = beat(tHub, tHub + hubSpanS)});
}

auto RotaConvocationis::bakeGeometry() -> void {
  namespace operations = sigil::geometry::path::operations;
  namespace geom = sigil::geometry::path;

  chalk.assign((size_t)kChalkCount, SkPath());
  auto chalkRing = [&](int slot, float rNorm, uint32_t seed) {
    chalk[(size_t)slot] = chalked(ringPath(rNorm), seed);
  };
  chalkRing(kChalkEdge, rEdge, 11);
  chalkRing(kChalkEdgeIn, rEdgeIn, 23);
  chalkRing(kChalkRuneOut, rRuneOut, 37);
  chalkRing(kChalkRuneIn, rRuneIn, 53);
  chalkRing(kChalkVoxOut, rVoxOut, 71);
  chalkRing(kChalkVoxIn, rVoxIn, 89);
  chalkRing(kChalkTickMid, rTickMid, 97);
  chalkRing(kChalkNomOut, rNomOut, 101);
  chalkRing(kChalkNomIn, rNomIn, 113);
  chalkRing(kChalkNomCase, rNomCase, 127);
  chalkRing(kChalkTexOut, rTexOut, 139);
  chalkRing(kChalkTexIn, rTexIn, 151);
  chalkRing(kChalkTexCase, rTexCase, 163);
  chalkRing(kChalkSerrIn, rSerrIn, 167);
  chalkRing(kChalkEnv, rEnv, 173);
  chalkRing(kChalkEnvIn, rEnvIn, 181);
  chalkRing(kChalkHubOut, rHubOut, 191);
  chalkRing(kChalkHubIn, rHubIn, 197);
  chalkRing(kChalkHubCase, rHubCase, 211);
  chalkRing(kChalkHubKern, rHubKern, 217);

  // THE FIGURES. A star compound is `kit::chords` with a step: twelve
  // stations stepped by three is three squares, stepped by four is four
  // triangles, and the library returns exactly gcd(sides, step) closed
  // rings rather than treating the non-coprime case as an error.
  const sigil::geometry::path::Frame frame{.centre = kEye, .radius = kR};
  const SkPath star = shapes::chords(
      frame, {.sides = kStations, .step = 3, .radius = rStar, .closed = true});
  const SkPath inner = shapes::chords(
      frame, {.sides = kStations, .step = 4, .radius = rInner, .closed = true});
  const SkPath hexagram = shapes::chords(
      frame, {.sides = 6, .step = 2, .radius = rHexagram, .closed = true});
  // The arcs: two rings of twelve, each arc short of its own pitch, the
  // inner ring turned half a pitch so the pair reads as a mechanism and
  // not as two circles.
  const SkPath arcs = [&] {
    SkPathBuilder b;
    b.addPath(arcRing(rArcOut, kStations, kPitch * 0.80f, 0.0f));
    b.addPath(arcRing(rArcIn, kStations, kPitch * 0.62f, kPitch * 0.5f));
    return b.detach();
  }();
  arcSpokes = spokeRing(kStations, rArcIn, rArcOut, kPitch * 0.5f);
  arcNodes = nodeRing(kStations, (rArcIn + rArcOut) * 0.5f, 5.0f, 0.0f);
  starSpokes = spokeRing(kStations, rEnv, rStar, 0.0f);
  starNodes = nodeRing(kStations, rStar, 9.0f, 0.0f);
  // THE CAGE FLOOR. Twelve spokes across an annulus this deep leave the
  // largest bare region on the plate, and the density rule this circle
  // is built to — no empty annulus anywhere — forbids exactly that. It
  // is fenced like every other band and filled at the plate's own two
  // pitches: a fine tick class at six per station, and a bead course
  // between them at two.
  cageFenceOut = ringPath(rCageOut);
  cageFenceIn = ringPath(rCageIn);
  cageLadder = spokeRing(kStations * 6, rCageIn + 0.004f, rCageOut - 0.004f,
                         kPitch / 12.0f);
  cageBeads = nodeRing(kStations * 2, (rCageIn + rCageOut) * 0.5f, 2.6f,
                       kPitch * 0.25f);
  // THE EYE. The centre was bare inside the emblem's own kern ring but
  // for the hexagram, which is the second of the two empty regions; six
  // motes on a half-pitch step the centre down instead of stopping it
  // dead.
  hubMotes = nodeRing(6, rHubMote, 2.2f, kPitch * 0.5f);
  innerSpokes = spokeRing(kStations, rHubOut, rInner, kPitch * 0.5f);
  hubDots = nodeRing(6, rHubKern, 4.0f, kPitch);
  // A DASHED RING is a ring at a duty cycle: sixty marks of a little
  // under half a pitch each, which the eye reads as broken rather than
  // as sixty things.
  const SkPath dashed =
      arcRing(rNomCase - 0.010f, 60, (360.0f / 60.0f) * 0.44f, 0.0f);
  // THE CRESCENTS: three, where everything else counts twelve.
  const SkPath crescents =
      crescentRing(rCrescOut, rCrescIn, 3, 66.0f, kPitch * 2.5f, 6);
  // THE OFF-ORDER MARK: one medallion straddling the outermost rule at
  // twelve o'clock, cutting the serration and the register alike. A
  // plate that is perfectly regular everywhere reads as a pattern; one
  // mark that obeys nothing is what makes the rest read as a drawing.
  {
    const SkPoint c = P(0.0f, rEdge);
    SkPathBuilder b;
    b.addOval(SkRect::MakeLTRB(c.fX - kSpurR, c.fY - kSpurR, c.fX + kSpurR,
                               c.fY + kSpurR));
    const float ri = kSpurR * 0.60f;
    b.addOval(SkRect::MakeLTRB(c.fX - ri, c.fY - ri, c.fX + ri, c.fY + ri));
    spurRules = b.detach();
  }

  chalk[kChalkArcs] = chalked(arcs, 223, 0.9f);
  chalk[kChalkDash] = chalked(dashed, 239, 0.6f);
  chalk[kChalkCresc] = chalked(crescents, 241, 0.8f);
  chalk[kChalkStar] = chalked(star, 227, 0.9f);
  chalk[kChalkInner] = chalked(inner, 229, 0.8f);
  chalk[kChalkHexagram] = chalked(hexagram, 233, 0.6f);

  // THE LIGHTING GROUPS. Each is a set of lines that ignite together
  // and turn together; nothing in one crosses anything in another,
  // which is the invariant that lets seven unions be rotated
  // independently and still add cleanly.
  glows.assign((size_t)kGlowCount, Glow{});
  glows[kGlowRim] = bakeGlow({ringPath(rEdge), ringPath(rEdgeIn),
                              ringPath(rRuneOut), ringPath(rRuneIn)},
                             0.85f);
  glows[kGlowNom] =
      bakeGlow({ringPath(rVoxIn), ringPath(rNomOut), ringPath(rNomIn)}, 0.7f);
  glows[kGlowArc] = bakeGlow({arcs, arcSpokes, arcNodes, dashed}, 0.7f);
  glows[kGlowStar] = bakeGlow({star, starSpokes, starNodes, crescents}, 0.85f);
  glows[kGlowInner] = bakeGlow({inner, innerSpokes}, 0.75f);
  glows[kGlowHub] = bakeGlow({ringPath(rHubOut), ringPath(rHubCase),
                              ringPath(rHubKern), hexagram, hubDots},
                             0.8f);
  glows[kGlowSpur] = bakeGlow({spurRules}, 0.8f);

  // THE STRIKE-MORPH: the light arrives as a scribble and resolves onto
  // the true figure. Both ends are resampled to the same point count on
  // the same arc-length parameterisation and the ladder is the lerp
  // between them, so step 0 is the scribble, the last step is the
  // compound exactly, and nothing in between has to be computed again.
  constexpr int kSteps = 10;
  constexpr int kSamples = 200;
  const SkPath scribble = operations::Roughen{
      .amplitude = 15.0f, .segmentPx = 16.0f, .seed = 211}(star);
  const std::vector<geom::Sampled> from = geom::resample(scribble, kSamples);
  const std::vector<geom::Sampled> to = geom::resample(star, kSamples);
  starSteps.assign((size_t)kSteps, SkPath());
  for (int i = 0; i < kSteps; ++i) {
    const float t = (float)i / (float)(kSteps - 1);
    SkPathBuilder b;
    for (size_t c = 0; c < to.size() && c < from.size(); ++c)
      b.addPath(geom::toPath(geom::lerp(from[c], to[c], t), true));
    starSteps[(size_t)i] = b.detach();
  }

  // THE EMBERS: one cell baked from a soft dot, and a pool the ticker
  // owns. The stamp is one draw whatever the count, which is what lets
  // the drizzle keep running for the whole charged idle.
  emberAtlas = std::make_shared<instancing::Atlas>(2.0f);
  emberFrame = emberAtlas->cell(
      box().fill(mskia::Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                        {{0.0f, hexColor(0xFFF3D2)},
                                         {0.22f, hexColor(0xFFD277, 0.85f)},
                                         {0.55f, hexColor(0xE79A32, 0.30f)},
                                         {1.0f, hexColor(0xC96F1E, 0.0f)}})),
      {22, 22});
  embers = std::make_shared<instancing::Pool>();
  embers->resize(kEmbers);
}
