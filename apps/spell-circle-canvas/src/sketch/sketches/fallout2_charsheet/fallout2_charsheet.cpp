// fallout2 charsheet: scene assembly and animation.

#include "Fallout2CharSheet.h"

auto Fallout2CharSheet::describe() -> Element {
  using namespace fo;
  Element root = stack()
                     .width(Dimension(kScreenW))
                     .height(Dimension(kScreenH + kCaptionH));

  // ---- the screen -----------------------------------------------------
  Element screen = box()
                       .left(Dimension(0))
                       .top(Dimension(0))
                       .width(Dimension(kScreenW))
                       .height(Dimension(kScreenH))
                       .clip()
                       .fill(plateMat);
  // the cast-metal tooth and the rust, as layer elements: each composites
  // whole against what is already on the canvas
  screen.child(box()
                   .inset(0)
                   .fill(plateTooth)
                   .blend(SkBlendMode::kOverlay)
                   .opacity(0.30f)
                   .cache(Cache::Texture));
  screen.child(box()
                   .inset(0)
                   .fill(rustMat)
                   .blend(SkBlendMode::kSoftLight)
                   .opacity(0.55f)
                   .cache(Cache::Texture));

  screen.child(chrome());

  // The S.P.E.C.I.A.L. column is NOT a well — sampled at (45,60) the
  // reference reads #483828, a LIT metal facet. It is a raised panel with
  // the odometers and plaques recessed into it; drawn as a black well it
  // punches a hole through a fifth of the screen.
  {
    Element sp = atR(box(), kWellSpecial)
                     .corners(Corners{n(4)})
                     .fill(Paint::linearUnit({0, 0}, {0.2f, 1},
                                             {{0.0f, hexColor(0x54462E)},
                                              {0.35f, kPlateLit},
                                              {1.0f, hexColor(0x3A3020)}}));
    sp.child(box()
                 .inset(0)
                 .fill(plateTooth)
                 .blend(SkBlendMode::kOverlay)
                 .opacity(0.34f)
                 .cache(Cache::Texture));
    sp.child(box()
                 .inset(0)
                 .fill(rustMat)
                 .blend(SkBlendMode::kSoftLight)
                 .opacity(0.70f)
                 .cache(Cache::Texture));
    sp.foreground(fo::stamp(1.6f, 2.0f, hexColor(0xB09868, 0.55f),
                            hexColor(0x080604, 0.70f)));
    sp.stroke(stroke(n(1), Fill::color(hexColor(0x1A1610)),
                     PathFormat::Align::Inner));
    for (int i = 0; i < 4; ++i)
      sp.child(
          sigil::compose::kit::disc(
              SkPoint{n(((unsigned)i & 1u) ? kWellSpecial.w - 6 : 6.0f),
                      n(((unsigned)i & 2u) ? kWellSpecial.h - 6 : 6.0f)},
              n(2.6f))
              .fill(rivetMat)
              .foreground(stroke(n(0.6f), Fill::color(hexColor(0x000000, 0.7f)),
                                 PathFormat::Align::Outer)));
    screen.child(sp);
  }
  screen.child(well(kWellStatus));
  screen.child(well(kWellDerived));
  screen.child(well(kWellLevel));
  screen.child(well(kWellSkills, 3.0f));
  screen.child(well(kWellFolder, 3.0f));

  screen.child(specialColumn());
  screen.child(statusBlock());
  screen.child(derivedBlock());
  screen.child(levelBlock());
  screen.child(folder());
  screen.child(box().inset(0).child(slot("skills")));
  screen.child(card());

  // POST: none. No bloom, no scanlines, no vignette, no CRT — a 1998 VGA
  // screen captured off a framebuffer has none of that and the restraint is
  // the point. At most a 4% grain so the metal does not read as vector-flat.
  screen.child(box()
                   .inset(0)
                   .fill(canvasGrain)
                   .blend(SkBlendMode::kOverlay)
                   .opacity(0.04f)
                   .cache(Cache::Texture));
  root.child(screen);

  // ---- the plate caption. NOT part of the artefact: the screen above is
  // exactly 1280x960 and this band sits below it, carrying the audit.
  root.child(captionBand());
  if (sheetAudit.failures() > 0) root.child(failureCard());
  return root;
}

auto Fallout2CharSheet::setup(sketch::SketchContext& ctx) -> void {
  using namespace fo;
  // The plate at exactly 2x. One 1998 pixel is two canvas px and four
  // device px, so quartering the capture recovers the 640x480 screen and
  // it overlays the reference.
  sketch::kit::stage(ctx, {.size = SkSize::Make(kScreenW, kScreenH + kCaptionH),
                           .captureAt = 6.0,
                           .background = hexColor(0x050604),
                           .oversample = 2});

  nargTraits.gifted = true;
  nargTraits.heavyHanded = true;

  // ---- the arithmetic, before anything is drawn ----------------------
  sheetAudit = audit();
  // Qualified: `derive` is also a compose namespace, and this file's own
  // derive() arrives through `using namespace fo`, so an unqualified call
  // here is ambiguous.
  stats = fo::derive(narg, nargTraits, kLevel, kToughness);
  tagged[0] = tagged[4] = tagged[5] = true;  // Small Guns, Melee, Throwing
  invested[0] = 12;                          // 24% on a tagged skill
  invested[4] = 15;                          // 30%
  invested[5] = 7;                           // 14%
  invested[6] = 6;  // First Aid, untagged: 6 points = 6%
  invested[7] = 6;  // Doctor
  for (int i = 0; i < 18; ++i)
    skillPct[(size_t)i] =
        skillValue(i, narg, nargTraits, tagged[(size_t)i], invested[(size_t)i]);
  pointsEarned = (kLevel - 1) * skillPointsPerLevel(narg, nargTraits, 0);
  pointsSpent = 0;
  for (int i = 0; i < 18; ++i) pointsSpent += invested[(size_t)i];

  buildSurfaces();

  // ---- measured type. The body size comes from the face's real advance,
  // not from an assumed em: the reason Fallout's fixed value columns work is
  // that its small font is near-monospaced, so the substitute has to land on
  // kBodyAdvance original px per character.
  {
    // THE PROBE IS THE CAPTURE'S OWN SAMPLE, not a row of Ms. A
    // proportional face has no single advance, so the size is solved
    // against the same 119 characters the 752 px was measured over —
    // an M sample would set the size by the widest glyph in the face
    // and print the whole sheet a third too small.
    static const char* kProbe =
        "Armor ClassAction PointsCarry WeightMelee DamageDamage Res."
        "Poison Res.Radiation Res.Sequence Healing RateCritical Chance";
    const float probeChars = (float)std::strlen(kProbe);
    const float w =
        ctx.measure(t(kProbe, fo::sheetType(bodyFace(), 100.0f, kGreen)))
            .width();
    if (w > 1.0f) bodyEm = w / (probeChars * 100.0f);
    // The line-top -> cap-top slack. Fallout's draw y is the top of the
    // glyph cell; SigilWeave's node top is the line box's top, and the two
    // differ by (ascent - capHeight). Taken as a fraction of the measured
    // line height, which lands every row on the reference's own y.
    bodyRise =
        std::max(0.0f, ctx.measure(t("H", body(kGreen))).height() * 0.20f);
    // font 102: squeeze the substitute until "Melee Weapons" lands on the
    // reference's ~6.8 original px per character.
    const float raw =
        ctx.measure(t("Melee Weapons",
                      fo::sheetType(titleFace(), n(23.8f), kInk, n(-0.1f))))
            .width();
    if (raw > 1.0f)
      titleCondense = std::clamp(n(6.8f * 13.0f) / raw, 0.30f, 1.0f);
    titleRise =
        std::max(0.0f, ctx.measure(t("H", titleStyle())).height() * 0.20f);
    // The engraved condense, same discipline: "ST-" is 36 original px of
    // ink on the capture, so squeeze the substitute onto that measured
    // width rather than guessing a factor.
    const float rawSt =
        ctx.measure(t("ST-", plaqueType(fo::n(29.0f), kGold, 1.0f, 0.2f)))
            .width();
    if (rawSt > 1.0f)
      engravedCondense = std::clamp(fo::n(36.0f) / rawSt, 0.30f, 1.0f);
    const float eh = ctx.measure(t("H", plaqueType(100.0f, kGold))).height();
    if (eh > 1.0f)
      engravedRiseFrac = std::clamp(eh * 0.20f / 100.0f, 0.05f, 0.40f);
  }

  // the leader-row advances: two measures per row, nine rows.
  for (int i = 0; i < 9; ++i) {
    killNameW[(size_t)i] =
        ctx.measure(t(kills()[(size_t)i].name, body(kGreen))).width();
    killCountW[(size_t)i] =
        ctx.measure(t(std::to_string(kills()[(size_t)i].count), body(kGreen)))
            .width();
  }
  // the card title advances, for the formula's hand-computed x
  {
    const int idx[3] = {0, 7, 4};
    for (int k = 0; k < 3; ++k)
      titleAdvance[k] =
          ctx.measure(t(skills()[(size_t)idx[k]].name, titleStyle())).width();
  }

  ctx.ticker.add([this](double dt) {
    step(dt);
    return true;
  });

  ctx.composer.render(describe());
  pushSlots(ctx, true);
}

auto Fallout2CharSheet::pushSlots(sketch::SketchContext& ctx, bool force)
    -> void {
  if (force || selected != shownSelected || presses != shownPresses) {
    const bool cardChanged = force || selected != shownSelected;
    shownSelected = selected;
    shownPresses = presses;
    ctx.composer.renderSlot("skills", skillsColumn());
    if (cardChanged) ctx.composer.renderSlot("card", cardContent(selected));
  }
  if (force || odoTens != shownTens || odoOnes != shownOnes) {
    shownTens = odoTens;
    shownOnes = odoOnes;
    ctx.composer.renderSlot(
        "points", odometer(522, 228, pointsEarned - pointsSpent - presses,
                           odoTens, odoOnes));
  }
}

auto Fallout2CharSheet::step(double) -> void {
  const double t = std::fmod(now, kLoop);

  // ---- the selection walk. The row recolours INSTANTLY: there is no tween
  // in the game, and the card's title, formula, body and figure swap with
  // it at 0 ms (Fallout plays a sound here and draws no transition).
  int sel = kWalk[2];
  if (t < kDwell)
    sel = kWalk[0];
  else if (t < 2 * kDwell)
    sel = kWalk[1];
  if (sel != selected) {
    selected = sel;
    presses = 0;
  }

  // ---- three `+` presses on Melee Weapons: +2% each, SKILL POINTS 04 -> 01
  int p = 0;
  if (selected == kWalk[2])
    for (int i = 0; i < 3; ++i)
      if (t >= kSpend0 + kSpendGap * (double)i) p = i + 1;
  presses = p;

  // ---- the odometer flip. On each change the ONES digit is replaced by
  // the sprite sheet's blank cell, held 123 ms, then the new digit; then, if
  // the tens digit changed, the same for tens. A mechanical counter.
  const int value = pointsEarned - pointsSpent - presses;
  const int prevValue = pointsEarned - pointsSpent - std::max(0, presses - 1);
  double sinceChange = 1e9;
  if (presses > 0 && selected == kWalk[2])
    sinceChange = t - (kSpend0 + kSpendGap * (double)(presses - 1));
  const int tensNow = (value / 10) % 10, onesNow = value % 10;
  const int tensPrev = (prevValue / 10) % 10;
  odoTens = std::to_string(tensNow);
  odoOnes = std::to_string(onesNow);
  if (sinceChange >= 0 && sinceChange < kBlank)
    odoOnes = " ";  // blank cell
  else if (tensNow != tensPrev && sinceChange >= kBlank &&
           sinceChange < 2 * kBlank)
    odoTens = " ";

  // ---- the `+` lamp and the DONE lamp: 90 ms, invented and minimal
  const float flash = (sinceChange >= 0 && sinceChange < 0.09) ? 1.0f : 0.0f;
  plusFlash = flash;
  lampFlash = flash;
}

SIGIL_SKETCH(
    Fallout2CharSheet, "Study \xc2\xb7 Game UI",
    "Fallout 2's character screen (1998) at 2\xc3\x97 \xe2\x80\x94 ~134 runs, "
    "five alignment regimes")
