#include "HitmanVerlet.h"

auto HitmanVerlet::codeLine(const sigil::data::Json& listed, SkColor4f c)
    -> Element {
  // A MARKED LINE is one the paper prints wrong: the caret and the alarm
  // colour are the document's own flag, not the panel's.
  const bool marked = listed["marked"].boolean();
  return box().row().gap(4).height(12).shrink(0).children(
      {t(marked ? "\u25c4" : " ", mono(7.0f, marked ? kRed : kInk))
           .width(8)
           .shrink(0),
       t(listed["line"], mono(9.5f, marked ? kRed : c, 0.1f))});
}

auto HitmanVerlet::panelA1() -> Element {
  const sigil::data::Json& a1 = doc()["a1"];
  return panel(kPanelAH[0], a1["heading"].text(), 1)
      .gap(4)
      .children({t(a1["law"], monoB(12.0f, kBone, 0.2f)).height(16).shrink(0),
                 each(a1["code"].items(),
                      [this](const sigil::data::Json& line) {
                        return codeLine(line, kBlue);
                      }),
                 box().grow(1), t(a1["alarm"], ui(7.5f, kRed, 0.5f)),
                 t(a1["working"], mono(7.5f, kSteel, 0.1f)),
                 t(a1["note"], ui(7.0f, kTick, 0.4f))});
}

auto HitmanVerlet::panelA2() -> Element {
  const sigil::data::Json& a2 = doc()["a2"];
  return panel(kPanelAH[1], a2["heading"].text(), 2)
      .children({each(a2["code"].items(),
                      [this](const sigil::data::Json& line) {
                        return codeLine(line, kBlue);
                      }),
                 box().height(2).shrink(0),
                 t(a2["working"], mono(7.5f, kSteel, 0.1f)),
                 t(a2["printed"], mono(8.0f, kRed, 0.1f)),
                 t(a2["corrected"], mono(8.0f, hexColor(0x4FC79E), 0.1f)),
                 box().height(2).shrink(0),
                 t(a2["carried"], ui(7.5f, kSteel, 0.4f)),
                 t(a2["fifth"], ui(7.5f, kTick, 0.4f)), box().grow(1),
                 t(a2["verdict"], monoB(9.0f, kRed, 0.2f))});
}

auto HitmanVerlet::panelA3() -> Element {
  const sigil::data::Json& a3 = doc()["a3"];
  // WHAT THE TWO AXES MEAN: the ratio u = d/r across, the correction
  // factor s up. Nothing below turns a value into a pixel.
  //   s_exact(u) = 0.5 − 1/(2u)      s_approx(u) = 0.5 − 1/(1+u²)
  const sketch::kit::Plot factor{.x = {.domain = {0.5, 2.0}},
                                 .y = {.domain = {-0.55, 0.35}}};
  const sketch::kit::Anchor fromLeft{.across = Align::Start,
                                     .down = Align::Start};
  // THE FIVE ITERATION COUNTS, each column growing from its own base out
  // to its measured error — not a fraction along a rail with its name
  // over it, which is what a meter is. The percentages ARE the band
  // scale's ticks, so the numbers under the columns and the columns are
  // one datum read twice.
  static constexpr std::array<double, 5> kSoft{60, 80, 90, 95, 97.5};
  const auto column = [](std::size_t i) {
    return box()
        .fill(Fill::currentInk())
        .styleClass(i + 1 == kSoft.size() ? "hit" : "")
        .scaleY(animate(
            from(0.0f).to(1.0f),
            {.duration = 220ms, .ease = ease::outBack(), .delay = 1600ms}))
        .transformOrigin(0.5f, 1.0f);
  };
  return panel(kPanelAH[2], a3["heading"].text(), 3)
      .styleSheet(plotClasses())
      .children(
          {each(a3["code"].items(),
                [this](const sigil::data::Json& line) {
                  return codeLine(line, kBlue);
                }),
           sketch::kit::plot(
               "a3-s", factor,
               {sketch::kit::axis({.at = 0.0, .reach = 0, .numbers = false}),
                sketch::kit::rules({.x = {1.0}}),
                sketch::kit::trace(
                    [](double u) { return 0.5 - 1.0 / (2.0 * u); },
                    {.width = 1.4f, .styleClass = "exact"}),
                sketch::kit::trace(
                    [](double u) { return 0.5 - 1.0 / (1.0 + u * u); },
                    {.width = 1.8f, .styleClass = "approx"}),
                sketch::kit::label("s_exact", 0.52, 0.33,
                                   {.anchor = fromLeft, .styleClass = "exact"}),
                sketch::kit::label(
                    "s_approx", 0.52, 0.17,
                    {.anchor = fromLeft, .styleClass = "approx"}),
                sketch::kit::label(
                    "u = d/r   0.5 → 2.0", 2.0, -0.53,
                    {.anchor = {.across = Align::End, .down = Align::End},
                     .styleClass = "plotTick"})})
               .height(64)
               .shrink(0),
           t(a3["ratios"], mono(7.5f, kSteel, 0.1f)),
           t(a3["note"], ui(7.0f, kTick, 0.4f)),
           sketch::kit::plot(
               "a3-soft",
               {.x = {.transform = sigil::data::Transform::Band,
                      .steps = (int)kSoft.size(),
                      .padding = 0.42},
                .y = {.domain = {0, 20}},
                // The room the numbers under the columns stand in.
                .pad = 12},
               {[&column](const sketch::kit::Plot& f, std::string_view key,
                          std::size_t i) {
                  return sketch::kit::bands(
                             kSoft, {.y = [](double v) { return v * 0.2; },
                                     .part = column})(f, key, i)
                      .staggerChildren(60ms);
                },
                sketch::kit::axis({.line = false,
                                   .reach = 0,
                                   .tickLine =
                                       [](double v) {
                                         return sketch::kit::tickLabel(
                                             kit::formatted(
                                                 "%g", kSoft[(std::size_t)v]));
                                       }})})
               .height(44)
               .shrink(0),
           t(a3["soft"], ui(7.0f, kTick, 0.4f))});
}

auto HitmanVerlet::panelB1() -> Element {
  const sigil::data::Json& b1 = doc()["b1"];
  // The hole: the anatomy is drawn into this panel by the pen, so the
  // panel keeps the room for it and says nothing about what stands there.
  return panel(kPanelBH[0], b1["heading"].text(), 4)
      .gap(4)
      .children({box().height(118).shrink(0),
                 t(b1["count"], monoB(8.5f, kBone, 0.1f)),
                 t(b1["dof"], mono(8.0f, kSteel, 0.1f)),
                 t(b1["compare"], mono(8.0f, kSteel, 0.1f)),
                 t(b1["note"], ui(7.0f, kTick, 0.4f))});
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
  pen.text("|LK−RK| ≥ 100", p[LKN].fX - 66, p[LKN].fY);
  pen.textAlign(draw::LEFT, draw::TOP);
}

auto HitmanVerlet::panelB2() -> Element {
  const sigil::data::Json& b2 = doc()["b2"];
  return panel(kPanelBH[1], b2["heading"].text(), 5)
      .gap(4)
      .children({box().height(156).shrink(0), box().height(34).shrink(0),
                 t(b2["note"], ui(7.0f, kTick, 0.4f))});
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
  pen.text(monotone ? "mean e(1) > mean e(4) > mean e(10)  ✓"
                    : "MONOTONICITY FAILED THIS FRAME",
           x0, y0 + 182);
}

auto HitmanVerlet::panelB3() -> Element {
  // ONE ROW PER MEASURED REST LENGTH: the name at the left, the figure at
  // the right, and the anchor the whole figure is scaled from in its own
  // colour because the document says which one it is.
  const auto restRow = [](const sigil::data::Json& rest) {
    const bool anchor = rest["anchor"].boolean();
    return box().row().height(11).shrink(0).children(
        {t(rest["name"], mono(8.0f, anchor ? kBlue : kSteel, 0.1f)).grow(1),
         t(rest["value"],
           anchor ? monoB(8.0f, kBlue, 0.1f) : mono(8.0f, kBone, 0.1f))});
  };
  const sigil::data::Json& b3 = doc()["b3"];
  return panel(kPanelBH[2], b3["heading"].text(), 6)
      .gap(3)
      .children(
          {each(b3["rests"].items(), restRow),
           t(b3["note"], ui(7.0f, kTick, 0.4f)), box().height(4).shrink(0),
           each(b3["production"].items(), [](const sigil::data::Json& line) {
             return t(line, ui(7.0f, kSteel, 0.3f));
           })});
}

auto HitmanVerlet::header() -> Element {
  Track rise{.effect = fx::rise(22.0f),
             .stagger = {.eachMs = 24, .durationMs = 440},
             .progress = animate(from(0.0f).to(1.0f), {.duration = 1100ms,
                                                       .ease = ch::easeOutQuad,
                                                       .delay = 120ms})};
  const sigil::data::Json& head = doc()["header"];
  return box().column().height(kHeaderH).shrink(0).gap(3).children(
      {t(head["eyebrow"], ui(10.0f, kSteel, 2.6f))
           .opacity(animate(from(0.0f).to(1.0f), {.duration = 260ms}))
           .translateY(animate(from(8.0f).to(0.0f), {.duration = 260ms})),
       t(head["title"], faced(heavyFace(), 42, kBone, -0.3f))
           .key("title")
           .fx(std::move(rise)),
       t(head["credit"], ui(10.5f, kSteel, 0.1f))
           .opacity(animate(from(0.0f).to(1.0f),
                            {.duration = 240ms, .delay = 400ms})),
       box().grow(1),
       kit::line({.fill = Fill::color(kKeyline)})
           .shrink(0)
           .opacity(animate(from(0.0f).to(1.0f),
                            {.duration = 400ms, .delay = 320ms}))});
}
