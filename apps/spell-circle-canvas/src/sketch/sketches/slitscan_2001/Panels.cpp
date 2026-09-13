#include "SlitScan2001.h"

auto SlitScan2001::panelShell(const char* heading, int order) -> Element {
  using namespace slit;
  return box()
      .column()
      .width(kSideW)
      .shrink(0)
      .padding(11)
      .gap(4)
      .corners({5})
      .fill(kPanelBg)
      // Every line in a panel is set in the mono face and the type-2 ink
      // unless it says otherwise; a line names its size and, where it
      // differs, its colour or its face.
      .font({.face = monoFace()})
      .ink(kType2)
      .stroke(stroke(1.0f, Fill::color(kRule)))
      .clip()
      .key(kit::formatted("panel%d", order))
      .opacity(animate(from(0.0f).to(1.0f), {300ms, ch::easeOutQuad}))
      .translateX(animate(from(14.0f).to(0.0f), {300ms, ch::easeOutQuad}))
      .children({pl(heading, {.face = uiFace(), .size = 9.5f, .track = 2.2f}),
                 rule(390, kRule)});
}

auto SlitScan2001::s1Quote() -> Element {
  using namespace slit;
  Element p = panelShell("THE MACHINE, IN ITS OWN WORDS", 0);
  p.children(
      {pl("“… this device could produce two seemingly infinite planes "
          "of exposure while holding depth-of-field from a distance of "
          "fifteen feet to one and one-half inches from the lens at an "
          "aperture of F/1.8 with exposures of approximately one minute "
          "per frame using a standard 65mm Mitchell camera.”",
          quo(9.1f, kType))});
  p.children(
      {box()
           .row()
           .gap(5)
           .shrink(0)
           .children(
               {pl("▸", {.face = uiFace(), .size = 9, .color = kRed}).width(7)})
           .children({pl("“holding depth-of-field” IS THE PHRASE THAT "
                         "CANNOT BE TRUE — SEE BELOW",
                         {.size = 7.1f, .color = kRed})
                          .grow(1)})});
  p.children(
      {pl("“… we moved the camera along fourteen feet of track toward "
          "the slit — a full fourteen feet for each exposure. It took "
          "about forty-five seconds to a minute per exposure, and each "
          "frame was made up of two exposures.” [C85]",
          quo(8.4f, al(kType, 0.82f)))});
  p.children({rule(390, kRule)});
  p.children(
      {pl("[T68] Am. Cinematographer 49(6):416–420, 451–453, Jun 1968 · "
          "[C85] Cinefex 85, Apr 2001, at one remove through two "
          "agreeing carriers · [FS] The Film Stage · [NO] Oseman · "
          "[MB] MagicBeans · [GE] Ercolano · [AP] Age of Plastic · "
          "[WP] Wikipedia.",
          {.size = 6.5f, .color = kTick})});
  return p;
}

auto SlitScan2001::s2Lens() -> Element {
  using namespace slit;
  Element p = panelShell("120 : 1, AND A LENS", 1);
  p.children({pl("z0 = 15 ft = 180.0 in   z1 = 1.5 in   z0/z1 = 120.0 : 1",
                 {.size = 7.9f, .color = al(kCold, 0.95f)})});
  p.children(
      {pl("DOF @ 1.5 in, f/1.8, 28.64 mm, c 0.05 = 0.0791 mm   ⇒   "
          "CLAIMED BRACKET 4533.9 mm / ACTUAL DOF = 5.7 × 10⁴",
          {.size = 7.1f, .color = kRed})});
  p.children(
      {pl("READ THE 15 ft AS A HYPERFOCAL NEAR LIMIT, f²/(Nc)+f = 2×4572:",
          {.size = 6.9f})});
  p.children({pl("c .025→20.26   .050→28.64   .075→35.07   .100→40.48 mm",
                 {.size = 7.8f, .color = kAmber})});
  p.children(
      {pl("28 mm T2.8 IS ON PANAVISION’S SUPER PANAVISION 70 LIST AND IN "
          "2001’S OWN CONTINUITY REPORTS [AP] — THE LENS EXISTS. (f/1.8 "
          "IS FASTER THAN ANY OF THEM; 1.5 in NEEDS 87 mm OF BELLOWS.)",
          {.size = 6.5f, .color = al(kCold, 0.78f)})});
  p.children(
      {pl("[C85]’S 14 ft TRACK ⇒ NEAR END 12 in, NOT 1½ in — THE TWO "
          "PUBLISHED FIGURES DISAGREE BY 10.5 in, 6.2500% OF THE TRACK.",
          {.size = 6.5f})});
  p.children({pl(
      "THE NUMBER IN THE SENTENCE IS NOT A DEPTH OF FIELD. "
      "IT IS A LENS.",
      {.face = uiBoldFace(), .size = 10.5f, .color = kType, .track = 0.2f})});
  p.children(
      {pl("AND [T68]’S OWN CAPTION SAYS SO: “SELSYN-DRIVEN FOLLOW-FOCUS "
          "MECHANISM”. WHAT IT HELD WAS FOCUS, SERVOED TO THE TRACK.",
          {.size = 6.5f, .color = kAmber})});
  return p;
}

auto SlitScan2001::s3Law() -> Element {
  using namespace slit;
  Element p = panelShell("THE 1/ρ LAW — MEASURED, NOT ASSUMED", 2);
  p.children(
      {box()
           .width(386)
           .height(62)
           .shrink(0)
           .fill(al(kBlack, 0.6f))
           .stroke(stroke(1.0f, Fill::color(kRule)))
           .children(
               {box()
                    .inset(4)
                    .shape(shapes::parametric(
                        [](float s) {
                          // log-log axes: exact C/u is a straight
                          // line of slope -1 in this frame.
                          return SkPoint{s, s};
                        },
                        0.0f, 1.0f, 240, false))
                    .stroke(spans::upTo(animate(
                                to(1.0f), {520ms, ch::easeOutCubic, 1500ms})),
                            stroke(1.6f, Fill::color(kAmber)))})
           .children({custom([this](SkCanvas& c, const PaintContext& p2) {
                        drawMeasuredPoints(c, p2);
                      })
                          .inset(4)
                          .cache(Cache::None)})});
  p.children({slot("fit").height(21).shrink(0)});
  p.children(
      {pl("DWELL AT FILM RADIUS u IS f·w/(V·u), AND IRRADIANCE FROM AN "
          "EXTENDED SOURCE IS DISTANCE-INVARIANT AT FIXED APERTURE — SO "
          "EXPOSURE ∝ 1/u. NOTHING PAINTS IT; IT IS THE SUM OF 1624 "
          "STAMPS PER WALL WEIGHTED BY THE CAMERA TRAVEL EACH STANDS "
          "FOR, MEASURED BACK OUT OF AN F16 RASTER OF THE ACCUMULATION "
          "SUBTREE ALONE. Debug.h IS ENTIRELY PATH-LEVEL.",
          {.size = 6.5f})});
  p.children(
      {pl("WHAT THE FILM SHOWS IS DENSITY. WHAT THE MACHINE MADE IS "
          "EXPOSURE. THE 1/ρ LAW IS IN THE SECOND; THE CURVE BETWEEN "
          "THEM IS RECONSTRUCTED.",
          {.size = 6.5f, .color = kAmber})});
  return p;
}

auto SlitScan2001::s4Sampling() -> Element {
  using namespace slit;
  Element p = panelShell("SAMPLING: 406 IS NOT ARBITRARY", 3);
  const char* rowName[2] = {"uniform in  z", "uniform in ln z"};
  for (int r = 0; r < 2; ++r) {
    Element row = box().row().gap(5).alignItems(Align::Center);
    row.children({pl(rowName[r], {.size = 7.0f}).width(80)});
    for (int k = 0; k < 3; ++k) {
      const int idx = r * 3 + k;
      row.children(
          {box()
               .width(98)
               .height(18)
               .shrink(0)
               .fill(kBlack)
               .clip()
               .key(kit::formatted("s4_%d", idx))
               .scaleX(animate(from(0.0f).to(1.0f), {220ms, ease::outBack()}))
               .transformOrigin(0.0f, 0.5f)
               .children({instancing::instances(flatAtlas, s4[(size_t)idx],
                                                instancing::Mode::Data,
                                                SkBlendMode::kPlus)})});
    }
    p.children({row.shrink(0)});
  }
  p.children(
      {box()
           .row()
           .gap(5)
           .shrink(0)
           .children({box().width(80).shrink(0)})
           .children({pl("K = 12", {.size = 6.6f, .color = kTick}).width(98)})
           .children({pl("K = 48", {.size = 6.6f, .color = kTick}).width(98)})
           .children(
               {pl("K = 406", {.size = 6.6f, .color = kTick}).width(98)})});
  p.children(
      {pl("EACH STRIP IS ONE REAL WALL, STAMPED BY THE SAME CODE AS "
          "THE FRAME: FILM RADIUS 0 → 600 px, LEFT TO RIGHT, LINEAR.",
          {.size = 6.5f, .color = kTick})});
  p.children({rule(390, kRule)});
  p.children(
      {pl("Δ(ln u) ≤ ln(1 + w/X0)   K_min = 1 + ln 120 / ln(1+1/84) = "
          "405.6 → 406",
          {.size = 7.1f, .color = al(kCold, 0.9f)})});
  p.children(
      {pl("LINEARISING THE LOGARITHM (4.7875 × 84) GIVES 402; AT K = 400 "
          "THE STAMPS NO LONGER QUITE TOUCH.",
          {.size = 6.5f, .color = kAmber})});
  p.children({slot("ripple").height(19).shrink(0)});
  p.children(
      {pl("X0/w IS THE ONLY NUMBER THAT SETS THIS, AND X0 = 49.2 in PUTS "
          "THE SLIT 4 ft OFF AXIS — OUTSIDE A 6 ft PLATE. THE WEAKEST "
          "JOINT IN THIS RECONSTRUCTION, PRINTED RATHER THAN HIDDEN.",
          {.size = 6.5f})});
  p.children(
      {pl("EQUAL-WEIGHT LOG STAMPS ARE BAND-FREE AND FLAT — WHICH IS "
          "WRONG. THE WEIGHT MUST BE THE CAMERA TRAVEL: ω ∝ z.",
          {.size = 6.5f, .color = kRed})});
  return p;
}

auto SlitScan2001::sidebar() -> Element {
  using namespace slit;
  return box()
      .column()
      .width(kSideW)
      .height(kBodyH)
      .shrink(0)
      .justify(Justify::SpaceBetween)
      .staggerChildren(85ms)
      .children({s1Quote(), s2Lens(), s3Law(), s4Sampling()});
}

auto SlitScan2001::readoutEl() -> Element {
  using namespace slit;
  const float z = kZ0In * std::pow(kR, -(float)tau);
  const int stampIdx = (int)(tau * (double)(kKDisplay - 1));
  const float u = kUFar * std::pow(kR, (float)tau);
  const float omega = kUFar / std::max(u, 1e-3f);
  return box()
      .column()
      .gap(2)
      .width(262)
      .children(
          {box()
               .row()
               .gap(12)
               .children(
                   {t(kit::formatted("z = %06.2f in", z),
                      {.face = monoBoldFace(), .size = 9, .color = kAmber})})
               .children(
                   {t(kit::formatted("m = ×%0.3f", kZ0In / std::max(z, 1e-3f)),
                      {.size = 9, .color = kType2})}),
           box()
               .row()
               .gap(12)
               .children(
                   {t(kit::formatted("stamp %04d / %d", stampIdx, kKDisplay),
                      {.size = 9, .color = kType2})})
               .children({t(kit::formatted("ω = %0.4f", omega),
                            {.size = 9, .color = al(kCold, 0.9f)})}),
           t(kit::formatted("ONE ATLAS · %d×%d SHEET · ONE BAKE %.0f ms · "
                            "texWindows()",
                            sheetW, sheetH, bakeMs),
             {.size = 6.8f, .color = kTick})});
}

auto SlitScan2001::expoEl() -> Element {
  using namespace slit;
  return t(kit::formatted("SWEEP %3d%%  ·  z %06.2f in  ·  %d / %d STAMPS LAID",
                          (int)(tau * 100.0), kZ0In * std::pow(kR, -(float)tau),
                          (int)(tau * (double)kK), kK),
           {.size = 7.2f, .color = al(kCold, 0.8f)});
}

auto SlitScan2001::fitEl() -> Element {
  using namespace slit;
  if (fixedStatus.clamped)
    return t("FIT SUPPRESSED — THIS FRAME DROPPED SIMULATED TIME",
             {.size = 8.2f, .color = kRed});
  return box().column().gap(1).children(
      {t(kit::formatted("FIT  E(u) = C / u^p     p = %0.4f     R² = %0.5f",
                        fitP, fitR2),
         {.face = monoBoldFace(), .size = 8.2f, .color = al(kCold, 0.95f)}),
       t(kit::formatted("RESIDUAL u ∈ [8, 520] px  p95 %0.2f%%  max %0.2f%%  "
                        "(%d rays, %d pts)",
                        fitP95 * 100.0f, fitResid * 100.0f, fitRays, fitPts),
         {.size = 7.2f})});
}

auto SlitScan2001::rippleEl() -> Element {
  using namespace slit;
  return box().column().gap(1).children(
      {t(kit::formatted("AND K_min REMOVES GAPS, NOT RIPPLE: AT K = 406 THE "
                        "MEASURED MAX RESIDUAL IS %0.0f%%,",
                        fitResidMin * 100.0f),
         {.size = 7.0f, .color = al(kCold, 0.85f)}),
       t(kit::formatted("AT 4× IT IS %0.0f%% — AND p MOVES ONLY %0.4f → %0.4f. "
                        "THE LAW SURVIVES ITS OWN QUANTISATION.",
                        fitResid * 100.0f, fitPMin, fitP),
         {.size = 7.0f, .color = al(kCold, 0.85f)})});
}
