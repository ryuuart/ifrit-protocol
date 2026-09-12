#include "SlitScan2001.h"

auto SlitScan2001::panelShell(const char* heading, int order) -> Element {
  using namespace slit;
  return box()
      .column()
      .width(Dimension(kSideW))
      .shrink(0)
      .padding(11)
      .gap(4)
      .corners({5})
      .fill(kPanelBg)
      .stroke(stroke(1.0f, Fill::color(kRule)))
      .clip()
      .key(kit::formatted("panel%d", order))
      .opacity(animate(from(0.0f).to(1.0f), {300ms, ch::easeOutQuad}))
      .translateX(animate(from(14.0f).to(0.0f), {300ms, ch::easeOutQuad}))
      .child(pl(heading, ui(9.5f, kType2, 2.2f)))
      .child(rule(390, kRule));
}

auto SlitScan2001::s1Quote() -> Element {
  using namespace slit;
  Element p = panelShell("THE MACHINE, IN ITS OWN WORDS", 0);
  p.child(
      pl("“… this device could produce two seemingly infinite planes "
         "of exposure while holding depth-of-field from a distance of "
         "fifteen feet to one and one-half inches from the lens at an "
         "aperture of F/1.8 with exposures of approximately one minute "
         "per frame using a standard 65mm Mitchell camera.”",
         quo(9.1f, kType)));
  p.child(box()
              .row()
              .gap(5)
              .shrink(0)
              .child(pl("▸", ui(9, kRed)).width(7))
              .child(pl("“holding depth-of-field” IS THE PHRASE THAT "
                        "CANNOT BE TRUE — SEE BELOW",
                        mono(7.1f, kRed))
                         .grow(1)));
  p.child(
      pl("“… we moved the camera along fourteen feet of track toward "
         "the slit — a full fourteen feet for each exposure. It took "
         "about forty-five seconds to a minute per exposure, and each "
         "frame was made up of two exposures.” [C85]",
         quo(8.4f, al(kType, 0.82f))));
  p.child(rule(390, kRule));
  p.child(
      pl("[T68] Am. Cinematographer 49(6):416–420, 451–453, Jun 1968 · "
         "[C85] Cinefex 85, Apr 2001, at one remove through two "
         "agreeing carriers · [FS] The Film Stage · [NO] Oseman · "
         "[MB] MagicBeans · [GE] Ercolano · [AP] Age of Plastic · "
         "[WP] Wikipedia.",
         mono(6.5f, kTick)));
  return p;
}

auto SlitScan2001::s2Lens() -> Element {
  using namespace slit;
  Element p = panelShell("120 : 1, AND A LENS", 1);
  p.child(pl("z0 = 15 ft = 180.0 in   z1 = 1.5 in   z0/z1 = 120.0 : 1",
             mono(7.9f, al(kCold, 0.95f))));
  p.child(
      pl("DOF @ 1.5 in, f/1.8, 28.64 mm, c 0.05 = 0.0791 mm   ⇒   "
         "CLAIMED BRACKET 4533.9 mm / ACTUAL DOF = 5.7 × 10⁴",
         mono(7.1f, kRed)));
  p.child(pl("READ THE 15 ft AS A HYPERFOCAL NEAR LIMIT, f²/(Nc)+f = 2×4572:",
             mono(6.9f, kType2)));
  p.child(pl("c .025→20.26   .050→28.64   .075→35.07   .100→40.48 mm",
             mono(7.8f, kAmber)));
  p.child(
      pl("28 mm T2.8 IS ON PANAVISION’S SUPER PANAVISION 70 LIST AND IN "
         "2001’S OWN CONTINUITY REPORTS [AP] — THE LENS EXISTS. (f/1.8 "
         "IS FASTER THAN ANY OF THEM; 1.5 in NEEDS 87 mm OF BELLOWS.)",
         mono(6.5f, al(kCold, 0.78f))));
  p.child(
      pl("[C85]’S 14 ft TRACK ⇒ NEAR END 12 in, NOT 1½ in — THE TWO "
         "PUBLISHED FIGURES DISAGREE BY 10.5 in, 6.2500% OF THE TRACK.",
         mono(6.5f, kType2)));
  p.child(
      pl("THE NUMBER IN THE SENTENCE IS NOT A DEPTH OF FIELD. "
         "IT IS A LENS.",
         uiB(10.5f, kType, 0.2f)));
  p.child(
      pl("AND [T68]’S OWN CAPTION SAYS SO: “SELSYN-DRIVEN FOLLOW-FOCUS "
         "MECHANISM”. WHAT IT HELD WAS FOCUS, SERVOED TO THE TRACK.",
         mono(6.5f, kAmber)));
  return p;
}

auto SlitScan2001::s3Law() -> Element {
  using namespace slit;
  Element p = panelShell("THE 1/ρ LAW — MEASURED, NOT ASSUMED", 2);
  p.child(
      box()
          .width(386)
          .height(62)
          .shrink(0)
          .fill(al(kBlack, 0.6f))
          .stroke(stroke(1.0f, Fill::color(kRule)))
          .child(box()
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
                             stroke(1.6f, Fill::color(kAmber))))
          .child(custom([this](SkCanvas& c, const PaintContext& p2) {
                   drawMeasuredPoints(c, p2);
                 })
                     .inset(4)
                     .cache(Cache::None)));
  p.child(slot("fit").height(Dimension(21)).shrink(0));
  p.child(
      pl("DWELL AT FILM RADIUS u IS f·w/(V·u), AND IRRADIANCE FROM AN "
         "EXTENDED SOURCE IS DISTANCE-INVARIANT AT FIXED APERTURE — SO "
         "EXPOSURE ∝ 1/u. NOTHING PAINTS IT; IT IS THE SUM OF 1624 "
         "STAMPS PER WALL WEIGHTED BY THE CAMERA TRAVEL EACH STANDS "
         "FOR, MEASURED BACK OUT OF AN F16 RASTER OF THE ACCUMULATION "
         "SUBTREE ALONE. Debug.h IS ENTIRELY PATH-LEVEL.",
         mono(6.5f, kType2)));
  p.child(
      pl("WHAT THE FILM SHOWS IS DENSITY. WHAT THE MACHINE MADE IS "
         "EXPOSURE. THE 1/ρ LAW IS IN THE SECOND; THE CURVE BETWEEN "
         "THEM IS RECONSTRUCTED.",
         mono(6.5f, kAmber)));
  return p;
}

auto SlitScan2001::s4Sampling() -> Element {
  using namespace slit;
  Element p = panelShell("SAMPLING: 406 IS NOT ARBITRARY", 3);
  const char* rowName[2] = {"uniform in  z", "uniform in ln z"};
  for (int r = 0; r < 2; ++r) {
    Element row = box().row().gap(5).alignItems(Align::Center);
    row.child(pl(rowName[r], mono(7.0f, kType2)).width(80));
    for (int k = 0; k < 3; ++k) {
      const int idx = r * 3 + k;
      row.child(
          box()
              .width(98)
              .height(18)
              .shrink(0)
              .fill(kBlack)
              .clip()
              .key(kit::formatted("s4_%d", idx))
              .scaleX(animate(from(0.0f).to(1.0f), {220ms, ease::outBack()}))
              .transformOrigin(0.0f, 0.5f)
              .child(instancing::instances(flatAtlas, s4[(size_t)idx],
                                           instancing::Mode::Data,
                                           SkBlendMode::kPlus)));
    }
    p.child(row.shrink(0));
  }
  p.child(box()
              .row()
              .gap(5)
              .shrink(0)
              .child(box().width(80).shrink(0))
              .child(pl("K = 12", mono(6.6f, kTick)).width(98))
              .child(pl("K = 48", mono(6.6f, kTick)).width(98))
              .child(pl("K = 406", mono(6.6f, kTick)).width(98)));
  p.child(
      pl("EACH STRIP IS ONE REAL WALL, STAMPED BY THE SAME CODE AS "
         "THE FRAME: FILM RADIUS 0 → 600 px, LEFT TO RIGHT, LINEAR.",
         mono(6.5f, kTick)));
  p.child(rule(390, kRule));
  p.child(
      pl("Δ(ln u) ≤ ln(1 + w/X0)   K_min = 1 + ln 120 / ln(1+1/84) = "
         "405.6 → 406",
         mono(7.1f, al(kCold, 0.9f))));
  p.child(
      pl("LINEARISING THE LOGARITHM (4.7875 × 84) GIVES 402; AT K = 400 "
         "THE STAMPS NO LONGER QUITE TOUCH.",
         mono(6.5f, kAmber)));
  p.child(slot("ripple").height(Dimension(19)).shrink(0));
  p.child(
      pl("X0/w IS THE ONLY NUMBER THAT SETS THIS, AND X0 = 49.2 in PUTS "
         "THE SLIT 4 ft OFF AXIS — OUTSIDE A 6 ft PLATE. THE WEAKEST "
         "JOINT IN THIS RECONSTRUCTION, PRINTED RATHER THAN HIDDEN.",
         mono(6.5f, kType2)));
  p.child(
      pl("EQUAL-WEIGHT LOG STAMPS ARE BAND-FREE AND FLAT — WHICH IS "
         "WRONG. THE WEIGHT MUST BE THE CAMERA TRAVEL: ω ∝ z.",
         mono(6.5f, kRed)));
  return p;
}

auto SlitScan2001::sidebar() -> Element {
  using namespace slit;
  return box()
      .column()
      .width(Dimension(kSideW))
      .height(Dimension(kBodyH))
      .shrink(0)
      .justify(Justify::SpaceBetween)
      .staggerChildren(85ms)
      .child(s1Quote())
      .child(s2Lens())
      .child(s3Law())
      .child(s4Sampling());
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
      .width(Dimension(262))
      .child(
          box()
              .row()
              .gap(12)
              .child(t(kit::formatted("z = %06.2f in", z), monoB(9, kAmber)))
              .child(t(kit::formatted("m = ×%0.3f", kZ0In / std::max(z, 1e-3f)),
                       mono(9, kType2))))
      .child(
          box()
              .row()
              .gap(12)
              .child(t(kit::formatted("stamp %04d / %d", stampIdx, kKDisplay),
                       mono(9, kType2)))
              .child(t(kit::formatted("ω = %0.4f", omega),
                       mono(9, al(kCold, 0.9f)))))
      .child(t(kit::formatted("ONE ATLAS · %d×%d SHEET · ONE BAKE %.0f ms · "
                              "texWindows()",
                              sheetW, sheetH, bakeMs),
               mono(6.8f, kTick)));
}

auto SlitScan2001::expoEl() -> Element {
  using namespace slit;
  return t(kit::formatted("SWEEP %3d%%  ·  z %06.2f in  ·  %d / %d STAMPS LAID",
                          (int)(tau * 100.0), kZ0In * std::pow(kR, -(float)tau),
                          (int)(tau * (double)kK), kK),
           mono(7.2f, al(kCold, 0.8f)));
}

auto SlitScan2001::fitEl() -> Element {
  using namespace slit;
  if (fixedStatus.clamped)
    return t("FIT SUPPRESSED — THIS FRAME DROPPED SIMULATED TIME",
             mono(8.2f, kRed));
  return box()
      .column()
      .gap(1)
      .child(
          t(kit::formatted("FIT  E(u) = C / u^p     p = %0.4f     R² = %0.5f",
                           fitP, fitR2),
            monoB(8.2f, al(kCold, 0.95f))))
      .child(t(
          kit::formatted("RESIDUAL u ∈ [8, 520] px  p95 %0.2f%%  max %0.2f%%  "
                         "(%d rays, %d pts)",
                         fitP95 * 100.0f, fitResid * 100.0f, fitRays, fitPts),
          mono(7.2f, kType2)));
}

auto SlitScan2001::rippleEl() -> Element {
  using namespace slit;
  return box()
      .column()
      .gap(1)
      .child(
          t(kit::formatted("AND K_min REMOVES GAPS, NOT RIPPLE: AT K = 406 THE "
                           "MEASURED MAX RESIDUAL IS %0.0f%%,",
                           fitResidMin * 100.0f),
            mono(7.0f, al(kCold, 0.85f))))
      .child(t(kit::formatted(
                   "AT 4× IT IS %0.0f%% — AND p MOVES ONLY %0.4f → %0.4f. "
                   "THE LAW SURVIVES ITS OWN QUANTISATION.",
                   fitResid * 100.0f, fitPMin, fitP),
               mono(7.0f, al(kCold, 0.85f))));
}
