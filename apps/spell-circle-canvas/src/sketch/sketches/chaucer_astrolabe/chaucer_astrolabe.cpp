// chaucer astrolabe: scene assembly and animation.

// TAGS: Data/Astronomy

#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::describe(sketch::SketchContext&) -> Element {
  // The plate's theme stands for everything described below it, so a kit
  // component four levels down is set in this plate's registers without
  // being handed them; the sheet on the root is the same statement for the
  // classes, and the font and the ink under it are the plate's running
  // voice.
  sketch::kit::Provide look(sheetLook);
  return stack()
      .fill(Fill::color(kVellum))
      .styleSheet(cardLook)
      .font(sheetLook.font(sheetLook.type.captionNote))
      .ink(kInk)
      .children(
          {// vellum: grain at very low contrast over the whole canvas, on
           // the same bake rule as the mater's passes — and the one that
           // needs it most, because a bare fill leaf records no picture at
           // all (one drawRect is cheaper than a nested recording), so
           // undeclared this evaluates the noise across 2400x1600 every
           // frame. Texture, not Picture: replaying a picture re-runs the
           // shader; an image blits.
           box()
               .inset(0)
               .key("vgrain")
               .cache(Cache::Texture)
               .fill(vellumGrain)
               .opacity(0.13f)
               .blend(SkBlendMode::kSoftLight),
           // the case: the object sits in a vitrine, not on the page
           kit::at(56, 140, 1132, 1258)
               .key("case")
               .corners({3})
               .fill(Fill::color(kCase))
               .opacity(rise(ramp(tGround * 1000, 700))),
           kit::at(56, 140, 1132, 1258)
               .key("vignette")
               .corners({3})
               .cache(Cache::Texture)
               .fill(Paint::glowUnit({0.50f, 0.46f}, 1.05f,
                                     {{0.0f, hexColor(0x33405a, 0.55f)},
                                      {0.62f, hexColor(0x1d222d, 0.0f)},
                                      {1.0f, hexColor(0x080a10, 0.75f)}}))
               .opacity(rise(ramp(tGround * 1000, 900))),
           // the contact shadow
           kit::at(kCx - kMaterR * 1.02f, kCy + kMaterR * 0.86f,
                   kMaterR * 2.04f, kMaterR * 0.30f)
               .key("contact")
               .shape(shapes::circle())
               .cache(Cache::Texture)
               .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                     {{0.0f, hexColor(0x05070c, 0.75f)},
                                      {1.0f, hexColor(0x05070c, 0.0f)}}))
               .opacity(rise(ramp(tMater * 1000, 900))),
           titleStrip(), limb(), plate(),
           // the rete is a physically raised sheet: it casts onto the plate
           box().inset(0).key("retewrap").children({reteShadow(), reteGroup()}),
           // the only saturated thing on the canvas, and it reads as a line
           // drawn ON the brass rather than engraved into it — no groove,
           // no bevel
           rule(), slot("sun"), construction(), slot("readout"), rack(),
           consolePanel()});
}

auto ChaucerAstrolabe::setup(sketch::SketchContext& ctx) -> void {
  // A 2400×1600 sheet over nine cached material passes under one live
  // rotation of the whole rete: its subject is the size of the sheet,
  // so --bench judges it on the still it is photographed as, not on
  // holding 60 FPS at that size. Never a timeout override; the plate
  // sweep is untouched.
  // The still has to name its moment: this is a 26 s loop of named states,
  // and tStill is CHAUCER'S MOMENT — the 12 March 1391 trace at full
  // opacity [21.9, 24.8]. Any other state asserts a different date, and an
  // undeclared capture lands mid-assembly, with the rete not yet there.
  // The EARLIEST frame of that state at which nothing is still moving,
  // because a still is stepped to frame by frame and every later one is
  // paid for in full canvases.
  sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                           .captureAt = tStill,
                           .background = kVellum,
                           .plateOnly = true});

  // Three lettering systems, genuinely different: a chiselled Latin
  // majuscule for what is engraved on the RETE, an engraver's copperplate
  // for the ruled LIMB, and a modern-legible serif + mono for the
  // commentary, which never touches the object.
  // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
  // library's own walk: the first installed family wins, and a machine
  // with none of them gets the default face AT THE WEIGHT ASKED FOR
  // rather than silently at Normal.
  faceEngrave = weave::ports::face({"Herculanum", "Optima", "Baskerville"});
  faceLimb = weave::ports::face({"Copperplate", "Optima", "Baskerville"});
  faceSerif = sketch::kit::houseFace(sketch::kit::Voice::Book);
  faceItalic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                      SkFontStyle::kItalic_Slant);
  faceBold = sketch::kit::houseFace(sketch::kit::Voice::Book, 700);
  faceMono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);

  // THE PLATE'S OWN SHEET, once the faces exist to name it with. Every
  // kit component reads the theme in scope, and this plate is letterpress
  // on vellum rather than the house sheet, so it binds its own: the three
  // lettering systems it is set in, its ink and rubric, and the air the
  // masthead's lines stand apart by.
  sheetLook = {};
  sheetLook.palette.ground = kVellum;
  sheetLook.palette.ink = kInk;
  sheetLook.palette.ash = kRubric;
  sheetLook.palette.rule = hexColor(0x241c15, 0.3f);
  sheetLook.palette.figure = kInk;
  sheetLook.type.sans = faceSerif;
  sheetLook.type.mono = faceMono;
  sheetLook.type.title = {34, 2.4f, false, faceEngrave};
  sheetLook.type.subtitle = {19, 0, false, faceItalic};
  sheetLook.type.captionNote = {15, 0, false, faceSerif};
  sheetLook.spacing.subtitleGap = 12;
  sheetLook.spacing.contentGap = 28;
  sheetLook.spacing.rowGap = 5;

  // THE PLATE'S WORDS, and the registers they are set in. Every heading,
  // gloss, listing and verdict on the commentary is read out of the
  // document beside this file, and every class a card writes is registered
  // here once — over the theme's own eight registers and the eight a chart
  // draws, so a plot on a card is dressed without the card saying anything.
  doc = sketch::kit::Document(ctx, "data/content.json");
  cardLook = sheetLook.styleSheet();
  cardLook.set("title", partial(faceLimb, 15, kRubric, 1.9f));
  cardLook.set("subtitle", partial(faceItalic, 14, hexColor(0x6b5a44)));
  cardLook.set("gloss", partial(faceItalic, 13, kRubric));
  cardLook.set("note", partial(faceItalic, 12, hexColor(0x7b6a54)));
  cardLook.set("quote", partial(faceItalic, 13.5f, kInk));
  cardLook.set("figure", partial(faceMono, 12, kInk));
  cardLook.set("engrave", partial(faceLimb, 9, hexColor(0x33240c, 0.85f)));
  cardLook.set("captionLabel", partial(faceLimb, 11.5f, kInk, 1.2f));
  cardLook.set("captionNote", partial(faceMono, 9.5f, hexColor(0x7b6a54)));
  cardLook.set("plotTick", partial(faceMono, 9.5f, kInk));
  cardLook.set("plotLabel", partial(faceItalic, 11, hexColor(0x7b6a54)));
  cardLook.set("dial", partial(faceLimb, 10, hexColor(0x8a99b0), 1.4f));
  cardLook.set("readout", partial(faceMono, 19, hexColor(0xd8c79c)));
  cardLook.set("time", partial(faceMono, 19, hexColor(0xffdc8b)));

  brassGrain = Paint::recipe(field::grain(0.9f, 3, 11.0f, 0.30f));
  verdigris = patterns::speckle(420, 16, 1.6f, 5.0f,
                                {skia::toColor(hexColor(0x2f5a44, 0.09f))});
  verdigris.seed(1326);
  vellumGrain = Paint::recipe(field::grain(0.02f, 4, 5.0f));

  rete = buildRete();
  pieces = retePieces(rete);

  // the 360 limb ticks: ONE cell, three LENGTHS through Pool::sizes()
  tickAtlas = std::make_shared<instancing::Atlas>(3.0f);
  tickAtlas->cell(
      box().width(24).height(2.0f).fill(Fill::color(hexColor(0x33240c, 0.92f))),
      {26, 4});
  tickPool = std::make_shared<instancing::Pool>();
  instancing::place::ring(*tickPool, 360, {kCx, kCy}, 1.118f * kR, 0.0f, false);
  {
    auto pos = tickPool->positions();
    auto rot = tickPool->rotations();
    auto sz = tickPool->sizes();  // the non-uniform lane, opt-in
    auto tint = tickPool->tints();
    const float outer = 1.150f * kR;
    for (int i = 0; i < 360; ++i) {
      // division i is at plate angle ψ = 90 − i, so canvas angle −ψ
      const float psi = 90.0f - (float)i;
      const float ca = -psi * kD;
      const float lenMul =
          (i % 30 == 0) ? 0.72f : ((i % 5 == 0) ? 0.55f : 0.32f);
      const float len = 24.0f * lenMul;
      const float rr = outer - len * 0.5f;
      pos[i] = arrange::onEllipse({kCx, kCy}, {rr, rr}, ca);
      rot[i] = ca;  // the tick lies along its spoke
      sz[i] = {lenMul, (i % 30 == 0) ? 1.5f : ((i % 5 == 0) ? 1.15f : 0.9f)};
      tint[i] = {1, 1, 1, (i % 5 == 0) ? 1.0f : 0.78f};
    }
    tickPool->commit();
  }

  verify();

  ctx.ticker.add([this](double dt) {
    now += dt;
    const float t = (float)std::fmod(now, (double)tLoop);

    float lam = 1.0f, H = -46.550124f;
    float traceTo = 0.0f;
    if (t < tDay) {
      lam = 1.0f;
      H = -46.550124f;
    } else if (t < tYear) {
      // THE DAY: one full revolution clockwise = 24 hours in 8 s
      lam = 1.0f;
      H = -180.0f + 360.0f * (t - tDay) / (tYear - tDay);
    } else if (t < tChaucer) {
      // THE YEAR: the rete HOLDS and the sun walks the ecliptic — so its
      // hour angle is not free. A point at rete-frame angle α appears at
      // plate angle ψ = α − ρ, so with ρ pinned, H = 90 − α + ρ: as the
      // sun's right ascension grows through the year its hour angle falls.
      // Its declination circle grows and shrinks between 0.424 R and
      // 1.000 R, and its rising point slides along the horizon with it.
      lam = std::fmod(1.0f + 360.0f * (t - tYear) / (tChaucer - tYear), 360.0f);
      const float rho0 = sunRA(1.0f) - 90.0f - 46.550124f;
      H = std::fmod(90.0f - sunRA(lam) + rho0 + 900.0f, 360.0f) - 180.0f;
    } else {
      // CHAUCER'S MOMENT
      lam = 1.0f;
      H = -46.550124f;
      traceTo = std::min(1.0f, (t - tChaucer) / 0.9f) *
                std::min(1.0f, (tLoop - t) / 1.2f);
    }
    sunLam = lam;
    hourAngle = H;
    trace = traceTo;

    const float dec = sunDec(lam);
    const float ra = sunRA(lam);
    // the rete turns because the sky turns: RA must land on ψ = 90 − H
    reteRot = ra - 90.0f + H;
    const float sh =
        std::sin(kPhi * kD) * std::sin(dec * kD) +
        std::cos(kPhi * kD) * std::cos(dec * kD) * std::cos(H * kD);
    sunAlt = std::asin(std::clamp(sh, -1.0f, 1.0f)) / kD;
    latHours = 12.0f + H / 15.0f;
    if (latHours < 0) latHours += 24.0f;
    if (latHours >= 24.0f) latHours -= 24.0f;

    // which seasonal night-hour the sun is in, read on the same 12 curves
    const float h0 = H0(dec);
    float Hw = std::fmod(H + 540.0f, 360.0f) - 180.0f;
    nightHour = 0;
    if (sunAlt.value() <= 0) {
      const float from = std::fmod(Hw - h0 + 720.0f, 360.0f);
      nightHour = std::clamp((int)(from / seasonalStep(dec)) + 1, 1, 12);
    }

    // the lettre in the bordure lights as the label passes it
    const int n = ((int)std::lround(H / 15.0f) + 96) % 24;
    for (int i = 0; i < 24; ++i) {
      const int letter = (i + 1);
      const bool on = (letter % 24) == (n % 24);
      letterGlow[i] = on ? 0.85f : 0.0f;
    }

    signMark = std::floor(lam / 30.0f);
    // The projection demonstration is the one thing on the canvas that
    // never settles: P walks the meridian for as long as the sketch runs.
    // ANCHORED ON THE STILL, so the still catches it at the far end of the
    // swing, δ = −ε, where the ray lands on the Tropic of Capricorn — the
    // plate's own outer circle, r = R_cap. That end reads as a statement
    // rather than as a moment caught anywhere along the way, and it is an
    // identity: tan((90+ε)/2) is the reciprocal of R_eq, so r there is 1 R.
    projDec = -kEps * std::cos(((float)now - tStill) * 0.75f);
  });

  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("sun", sunMark());
  ctx.composer.renderSlot("readout", readout());
  ctx.composer.renderSlot("projray", projRay());
  ctx.composer.renderSlot("projread", projRead());
  ctx.composer.renderSlot("chaucer", chaucerBody());
}

auto ChaucerAstrolabe::update(double, sketch::SketchContext& ctx) -> void {
  // The four live readouts change their TEXT every frame, and an Animatable
  // carries values, not strings. renderSlot is the right answer and is
  // cheap: the surrounding ~350-node tree is untouched and keeps its
  // caches.
  ctx.composer.renderSlot("sun", sunMark());
  ctx.composer.renderSlot("readout", readout());
  ctx.composer.renderSlot("projray", projRay());
  ctx.composer.renderSlot("projread", projRead());
}

SIGIL_SKETCH(ChaucerAstrolabe, "Study · Science",
             "A planispheric astrolabe for Oxford 51° 50′ "
             "— an instrument that tells the time")
