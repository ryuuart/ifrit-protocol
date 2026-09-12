#include "HitmanVerlet.h"

auto HitmanVerlet::codeLine(const char* s, SkColor4f c, bool caret) -> Element {
  auto row = box().row().gap(4).height(Dimension(12)).shrink(0);
  row.child(t(caret ? "\xe2\x97\x84" : " ", mono(7.0f, caret ? kRed : kInk))
                .width(Dimension(8))
                .shrink(0));
  row.child(t(s, mono(9.5f, c, 0.1f)));
  return row;
}

auto HitmanVerlet::panelA1() -> Element {
  return panel(kPanelAH[0],
               "A1 \xc2\xb7 VERLET \xe2\x80\x94 NO VELOCITY "
               "VARIABLE",
               1)
      .gap(4)
      .child(t("x' = 2x \xe2\x88\x92 x* + a\xc2\xb7\xce\x94t\xc2\xb2      x* "
               "= x",
               monoB(12.0f, kBone, 0.2f))
                 .height(Dimension(16))
                 .shrink(0))
      .child(codeLine("temp    = x[i];", kBlue))
      .child(codeLine("x[i]   += DRAG*(x[i]-oldx[i]) + g;", kBlue))
      .child(codeLine("oldx[i] = temp;", kBlue))
      .child(box().grow(1))
      .child(t("1.99 IS A VELOCITY DAMP, NOT A POSITION ONE",
               ui(7.5f, kRed, 0.5f)))
      .child(t("x' = 1.99x \xe2\x88\x92 0.99x* + a\xce\x94t\xc2\xb2  ==  "
               "x + 0.99(x\xe2\x88\x92x*) + a\xce\x94t\xc2\xb2",
               mono(7.5f, kSteel, 0.1f)))
      .child(t("AS PRINTED, A PARTICLE AT REST AT x = 500 DRIFTS 5 u/STEP "
               "TOWARD THE ORIGIN.",
               ui(7.0f, kTick, 0.4f)));
}

auto HitmanVerlet::panelA2() -> Element {
  return panel(kPanelAH[1], "A2 \xc2\xb7 THE STICK CONSTRAINT, AND A SIGN", 2)
      .child(codeLine("delta = x2-x1;", kBlue))
      .child(codeLine("deltalength = sqrt(delta*delta);", kBlue))
      .child(codeLine("diff = (deltalength-restlength)/deltalength;", kBlue))
      .child(codeLine("x1 -= delta*0.5*diff;", kRed, true))
      .child(codeLine("x2 += delta*0.5*diff;", kRed, true))
      .child(box().height(Dimension(2)).shrink(0))
      .child(t("r = 100, |x2\xe2\x88\x92x1| = 120 \xe2\x86\x92 diff = 1/6, "
               "delta\xc2\xb7"
               "0.5\xc2\xb7"
               "diff = (10, 0)",
               mono(7.5f, kSteel, 0.1f)))
      .child(t("AS PRINTED : x1 = (\xe2\x88\x92"
               "10,0)  x2 = (130,0)  "
               "\xe2\x86\x92 d = 140  DIVERGES",
               mono(8.0f, kRed, 0.1f)))
      .child(t("CORRECTED  : x1 = ( 10,0)  x2 = (110,0)  "
               "\xe2\x86\x92 d = 100  EXACT",
               mono(8.0f, hexColor(0x4FC79E), 0.1f)))
      .child(box().height(Dimension(2)).shrink(0))
      .child(t("FOUR OF THE FIVE STICK LISTINGS CARRY IT: (C2), "
               "STICK-IN-A-BOX, CLOTH, MASS-WEIGHTED.",
               ui(7.5f, kSteel, 0.4f)))
      .child(t("THE FIFTH \xe2\x80\x94 THE SQRT APPROXIMATION, THE ONE THAT "
               "SHIPPED IN HITMAN \xe2\x80\x94 IS CORRECT WITH THE SAME TWO "
               "ASSIGNMENT LINES, BECAUSE ITS FACTOR IS ALREADY NEGATIVE "
               "UNDER TENSION. THE EXPOSITION FORM WAS MADE BY REMOVING THE "
               "APPROXIMATION, AND THE SIGN WENT WITH IT.",
               ui(7.5f, kTick, 0.4f)))
      .child(box().grow(1))
      .child(t("THE PAPER'S OWN STICK CODE PUSHES WHEN IT SHOULD PULL.",
               monoB(9.0f, kRed, 0.2f)));
}

auto HitmanVerlet::panelA3() -> Element {
  // s_exact(u) = 0.5 - 1/(2u);  s_approx(u) = 0.5 - 1/(1+u^2)
  auto curve = [](bool approx) {
    return shapes::parametric(
        [approx](float u) {
          const float s =
              approx ? 0.5f - 1.0f / (1.0f + u * u) : 0.5f - 1.0f / (2.0f * u);
          const float x = (u - 1.25f) / 0.75f;     // u in [0.5, 2] -> [-1,1]
          const float y = -(s - (-0.1f)) / 0.45f;  // s in [-0.55,0.35], flipped
          return SkPoint{x, std::clamp(y, -1.0f, 1.0f)};
        },
        0.5f, 2.0f, 240);
  };
  auto plotCurve = [&](bool approx, SkColor4f c, float w) {
    PathFormat f = stroke(w, Fill::color(c));
    if (!approx) f.dashIntervals = {3.5f, 3.0f};
    return box()
        .inset(0)
        .shape(curve(approx))
        .stroke(spans::upTo(animate(to(1.0f), {.duration = 520ms,
                                               .ease = ch::easeOutCubic,
                                               .delay = 1400ms})),
                f);
  };
  // A COLUMN PER ITERATION COUNT, growing from its own base — not a
  // fraction along a rail with its name over it, which is what a meter
  // is. What each column says is a MEASURED error against the four
  // beside it, and the reading is the run of them.
  auto bar = [&](int i, const char* label, float h) {
    return box()
        .column()
        .gap(2)
        .width(Dimension(52))
        .shrink(0)
        .alignItems(Align::Center)
        .child(box().grow(1))
        .child(
            box()
                .width(Dimension(30))
                .height(Dimension(h))
                .fill(i == 4 ? kBlue : hexColor(0x6FA8DC, 0.42f))
                .scaleY(animate(from(0.0f).to(1.0f), {.duration = 220ms,
                                                      .ease = ease::outBack(),
                                                      .delay = 1600ms}))
                .transformOrigin(0.5f, 1.0f))
        .child(t(label, mono(7.0f, kSteel)));
  };
  return panel(kPanelAH[2], "A3 \xc2\xb7 THE SQUARE-ROOT APPROXIMATION", 3)
      .child(codeLine("delta *= r*r/(delta*delta+r*r) - 0.5;", kBlue))
      .child(codeLine("x1 -= delta;   x2 += delta;", kBlue))
      .child(box()
                 .height(Dimension(64))
                 .shrink(0)
                 .child(box()  // s = 0
                            .left(Dimension(0))
                            .top(Dimension(39.1f))
                            .width(Dimension(324))
                            .height(Dimension(1))
                            .fill(hexColor(0x2A2E38)))
                 .child(box()  // u = 1
                            .left(Dimension(108))
                            .top(Dimension(0))
                            .width(Dimension(1))
                            .height(Dimension(64))
                            .fill(hexColor(0x2A2E38)))
                 .child(plotCurve(false, kSteel, 1.4f))
                 .child(plotCurve(true, kBlue, 1.8f))
                 .child(t("s_exact", mono(7.0f, kSteel))
                            .left(Dimension(4))
                            .top(Dimension(2)))
                 .child(t("s_approx", mono(7.0f, kBlue))
                            .left(Dimension(4))
                            .top(Dimension(13)))
                 .child(t("u = d/r   0.5 \xe2\x86\x92 2.0", mono(7.0f, kTick))
                            .left(Dimension(244))
                            .top(Dimension(52))))
      .child(t("approx/exact:  0.60\xc3\x97 at u=0.5 \xc2\xb7 0.88 \xc2\xb7 "
               "1.08 \xc2\xb7 1.15 \xc2\xb7 1.20\xc3\x97 at u=2.0",
               mono(7.5f, kSteel, 0.1f)))
      .child(t("AGREES IN VALUE AND SLOPE AT u = 1. DENOMINATOR "
               "d\xc2\xb2+r\xc2\xb2 \xe2\x89\xa5 r\xc2\xb2 > 0, SO IT "
               "CANNOT DIVIDE BY ZERO: \xc2\xa7"
               "7's SINGULARITY NOTE "
               "APPLIES ONLY TO THE EXACT FORM.",
               ui(7.0f, kTick, 0.4f)))
      .child(box()
                 .row()
                 .gap(2)
                 .height(Dimension(38))
                 .shrink(0)
                 .staggerChildren(60ms)
                 .child(bar(0, "60", 12))
                 .child(bar(1, "80", 16))
                 .child(bar(2, "90", 18))
                 .child(bar(3, "95", 19))
                 .child(bar(4, "97.5", 19.5f)))
      .child(t("\xc2\xa7"
               "7 SOFT CONSTRAINTS: HALF THE DEVIATION PER FRAME.",
               ui(7.0f, kTick, 0.4f)));
}

auto HitmanVerlet::panelB1() -> Element {
  return panel(kPanelBH[0], "B1 \xc2\xb7 FIGURE 9: THE ANATOMY", 4)
      .gap(4)
      .child(box().height(Dimension(118)).shrink(0))
      .child(t("16 PARTICLES \xc2\xb7 24 STICKS \xc2\xb7 1 INEQUALITY "
               "(KNEES, \xc2\xa7"
               "6)",
               monoB(8.5f, kBone, 0.1f)))
      .child(t("16\xc3\x97"
               "2 \xe2\x88\x92 24 = 8 PLANAR DOF   "
               "(16\xc3\x97"
               "3 \xe2\x88\x92 24 = 24 IN THE PAPER'S 3D)",
               mono(8.0f, kSteel, 0.1f)))
      .child(t("COMPARE \xc2\xa7"
               "5's TETRAHEDRON: 4\xc3\x97"
               "3 \xe2\x88\x92 6 = 6",
               mono(8.0f, kSteel, 0.1f)))
      .child(t("RE-COUNTED AT 600 dpi: THRESHOLD, ERODE BY A DISC r = 8 px "
               "\xe2\x80\x94 EVERY STICK AND EVERY BODY-TEXT STEM DIES AND "
               "EXACTLY 16 COMPONENTS OF 620\xe2\x80\x93"
               "657 px SURVIVE. "
               "THE PAPER PUBLISHES NO COUNT.",
               ui(7.0f, kTick, 0.4f)));
}

auto HitmanVerlet::paintAnatomy(Pen& pen, float x0, float y0, float w) -> void {
  // The rest pose at 104 px tall, centred in the panel's hole.
  const float H = 104.0f;
  const float cx = x0 + w * 0.5f, base = y0 + 112.0f;
  auto P = [&](Norm n, float side) {
    return SkPoint{cx + n.x * side * H, base - n.y * H};
  };
  std::array<SkPoint, NRIG> p = {
      P(kHead, 0), P(kNeck, 0), P(kSh, -1), P(kSh, +1), P(kEl, -1), P(kEl, +1),
      P(kHa, -1),  P(kHa, +1),  P(kWa, -1), P(kWa, +1), P(kHi, -1), P(kHi, +1),
      P(kKn, -1),  P(kKn, +1),  P(kFo, -1), P(kFo, +1)};
  pen.noFill();
  pen.strokeCap(draw::ROUND);
  pen.strokeWeight(1.5f);
  pen.stroke(hexColor(0x8A8F9C, 0.9f));
  for (const physics::Constraint& s : rig.sticks) {
    const SkPoint a = p[s.a], b = p[s.b];
    pen.line(a.fX, a.fY, b.fX, b.fY);
  }
  pen.strokeWeight(1.0f);
  pen.stroke(hexColor(0xC8402F, 0.9f));
  pen.strokeDash({2.0f, 3.0f});
  pen.line(p[LKN].fX, p[LKN].fY, p[RKN].fX, p[RKN].fY);
  pen.noDash();
  pen.noStroke();
  pen.fill(kBone);
  for (const SkPoint& q : p) pen.circle(q.fX, q.fY, 5.2f);
  penMono(pen, 7.0f, kTick);
  pen.textAlign(draw::LEFT, draw::CENTER);
  pen.text("NECK", p[NECK].fX + 7, p[NECK].fY);
  pen.text("WAIST", p[RWA].fX + 7, p[RWA].fY);
  pen.text("HIP", p[RHI].fX + 7, p[RHI].fY);
  penMono(pen, 7.0f, kRed);
  pen.text("|LK\xe2\x88\x92RK| \xe2\x89\xa5 100", p[LKN].fX - 66, p[LKN].fY);
  pen.textAlign(draw::LEFT, draw::TOP);
}

auto HitmanVerlet::panelB2() -> Element {
  return panel(kPanelBH[1],
               "B2 \xc2\xb7 RELAXATION: 1 \xc2\xb7 4 \xc2\xb7 "
               "10",
               5)
      .gap(4)
      .child(box().height(Dimension(156)).shrink(0))
      .child(box().height(Dimension(34)).shrink(0))
      .child(t("\"ITERATIONS USED IN HITMAN VARY BETWEEN 1 AND 10 WITH THE "
               "KIND OF OBJECT SIMULATED.\" \xe2\x80\x94 \xc2\xa7"
               "7. "
               "ORDER MATTERS AS MUCH AS COUNT: LISTED FROM THE PIN A CHAIN "
               "CONVERGES IN ONE SWEEP AND ALL THREE ARE IDENTICAL. THESE "
               "ARE LISTED FROM THE FREE END.",
               ui(7.0f, kTick, 0.4f)));
}

auto HitmanVerlet::paintChains(Pen& pen, float x0, float y0) -> void {
  pen.push();
  pen.translate(x0, y0);
  pen.noFill();
  pen.strokeCap(draw::ROUND);
  pen.strokeWeight(2.2f);
  for (int k = 0; k < 3; ++k) {
    const Body& b = chains[(size_t)k];
    for (const physics::Constraint& s : b.sticks) {
      const float e = std::abs(len(b.at(s.b) - b.at(s.a)) - s.rest) / s.rest;
      const SkPoint a = drawnWorld(b, s.a);
      const SkPoint z = drawnWorld(b, s.b);
      pen.stroke(errColor(e));
      pen.line(a.fX, a.fY, z.fX, z.fY);
    }
    pen.noStroke();
    for (size_t i = 0; i < b.count(); ++i) {
      const SkPoint q = drawnWorld(b, i);
      pen.fill(b.invm(i) <= 0 ? kRed : kBone);
      pen.circle(q.fX, q.fY, 4.0f);
    }
    pen.noFill();
  }
  pen.noStroke();
  penMonoB(pen, 9.0f, kBone);
  pen.textAlign(draw::LEFT, draw::BASELINE);
  const char* labels[3] = {"1", "4", "10"};
  const float xs[3] = {54, 156, 256};
  for (int k = 0; k < 3; ++k) pen.text(labels[k], xs[k], 12);
  pen.textAlign(draw::LEFT, draw::TOP);
  pen.pop();

  // The A/B's own numbers, and the monotonicity claim COMPUTED from the
  // three means rather than asserted beside them.
  const std::string a = kit::formatted("MEAN e   %5.2f%%    %5.2f%%    %5.2f%%",
                                       chainMean[0] * 100, chainMean[1] * 100,
                                       chainMean[2] * 100);
  const std::string b =
      kit::formatted("MAX  e   %5.2f%%    %5.2f%%    %5.2f%%",
                     chainMax[0] * 100, chainMax[1] * 100, chainMax[2] * 100);
  const bool monotone =
      chainMean[0] > chainMean[1] && chainMean[1] > chainMean[2];
  penMonoB(pen, 8.5f, kBone, 0.1f);
  pen.text(a, x0, y0 + 160);
  penMono(pen, 8.5f, kSteel, 0.1f);
  pen.text(b, x0, y0 + 171);
  penMono(pen, 7.5f, monotone ? hexColor(0x4FC79E) : kRed, 0.1f);
  pen.text(monotone ? "mean e(1) > mean e(4) > mean e(10)  \xe2\x9c\x93"
                    : "MONOTONICITY FAILED THIS FRAME",
           x0, y0 + 182);
}

auto HitmanVerlet::panelB3() -> Element {
  auto restRow = [&](const char* name, const char* val, bool anchor) {
    return box()
        .row()
        .height(Dimension(11))
        .shrink(0)
        .child(t(name, mono(8.0f, anchor ? kBlue : kSteel, 0.1f)).grow(1))
        .child(t(val,
                 anchor ? monoB(8.0f, kBlue, 0.1f) : mono(8.0f, kBone, 0.1f)));
  };
  return panel(kPanelBH[2], "B3 \xc2\xb7 REST LENGTHS & PRODUCTION", 6)
      .gap(3)
      .child(restRow("head \xe2\x80\x93 neck", "56.0", false))
      .child(restRow("shoulder bar", "168.0", false))
      .child(restRow("neck \xe2\x80\x93 waist (brace)", "176.0", false))
      .child(restRow("hip bar", "140.0", false))
      .child(restRow("hip \xe2\x80\x93 knee  (THE ANCHOR)", "100.0", true))
      .child(restRow("knee \xe2\x80\x93 foot", "98.0", false))
      .child(t("restlength = 100 ON THE THIGH FIXES THE FIGURE AT 486.2 "
               "UNITS \xe2\x80\x94 48.6% OF THE PAPER'S OWN CUBE. THIGH "
               "100.0 / SHANK 98.0 IS 1.91% APART, SO THE DRILLIS & CONTINI "
               "CROSS-CHECK IS DROPPED: NO PRIMARY SCAN, AND THE DIAGRAM "
               "WOULD HAVE FAILED IT.",
               ui(7.0f, kTick, 0.4f)))
      .child(box().height(Dimension(4)).shrink(0))
      .child(t("IO INTERACTIVE / EIDOS \xc2\xb7 19 NOV 2000 \xc2\xb7 GLACIER "
               "\xc2\xb7 DirectX 7.0a \xc2\xb7 GDC 2001, SAN JOSE",
               ui(7.0f, kSteel, 0.3f)))
      .child(t("\"THE PRESS OXYMORON: LIFELIKE DEATH ANIMATIONS\"",
               ui(7.0f, kSteel, 0.3f)))
      .child(t("HITMAN.INI: \"enableconsole 1\" + \"consolecmd ip_debug 1\" "
               "\xe2\x80\x94 SHIFT+F9 BOMBS AN NPC, K = FREE CAM",
               ui(7.0f, kSteel, 0.3f)));
}

auto HitmanVerlet::header() -> Element {
  Track rise{.effect = fx::rise(22.0f),
             .stagger = {.eachMs = 24, .durationMs = 440},
             .progress = animate(from(0.0f).to(1.0f), {.duration = 1100ms,
                                                       .ease = ch::easeOutQuad,
                                                       .delay = 120ms})};
  return box()
      .column()
      .height(Dimension(kHeaderH))
      .shrink(0)
      .gap(3)
      .child(t("STATE AND CONTACT", ui(10.0f, kSteel, 2.6f))
                 .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
                 .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})))
      .child(t("THE HITMAN RAGDOLL, 2000", faced(heavyFace(), 42, kBone, -0.3f))
                 .key("title")
                 .fx(std::move(rise)))
      .child(t("Thomas Jakobsen, IO Interactive \xe2\x80\x94 \"Advanced "
               "Character Physics\", GDC 2001 \xc2\xb7 shipped in Hitman: "
               "Codename 47 (Eidos, 19 Nov 2000, Glacier engine, DirectX "
               "7.0a) \xc2\xb7 every stick coloured by its LIVE constraint "
               "error",
               ui(10.5f, kSteel, 0.1f))
                 .opacity(animate(from(0.0f).to(1.0f),
                                  {.duration = 240ms, .delay = 400ms})))
      .child(box().grow(1))
      .child(box()
                 .height(Dimension(1))
                 .shrink(0)
                 .fill(kKeyline)
                 .opacity(animate(from(0.0f).to(1.0f),
                                  {.duration = 400ms, .delay = 320ms})));
}
