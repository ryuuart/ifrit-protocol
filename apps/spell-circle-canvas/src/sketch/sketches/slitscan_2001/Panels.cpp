#include "SlitScan2001.h"

auto SlitScan2001::panelShell(const data::Json& said, int order) -> Element {
  using namespace slit;
  // THE SHEET A PANEL IS SET ON: a class per ink it prints in, so a line
  // of prose names a colour by what it MEANS — a reading, a claim the
  // study disputes, a quotation, a reference — and never spells one. It
  // is stated on the PANEL rather than on the sheet's root because a
  // panel is also measured on its own, by the layout self-check.
  weave::StyleSheet classes;
  classes.set("type", {.color = sigil::material::skia::toSkColor(kType)})
      .set("cold", {.color = sigil::material::skia::toSkColor(al(kCold, 0.95f))})
      .set("coldQuiet", {.color = sigil::material::skia::toSkColor(al(kCold, 0.8f))})
      .set("amber", {.color = sigil::material::skia::toSkColor(kAmber)})
      .set("red", {.color = sigil::material::skia::toSkColor(kRed)})
      .set("tick", {.color = sigil::material::skia::toSkColor(kTick)})
      .set("quote", quo(9.1f, kType))
      .set("quoteQuiet", quo(8.4f, al(kType, 0.82f)))
      .set("headline", {.face = uiBoldFace(), .color = sigil::material::skia::toSkColor(kType), .track = 0.2f});
  return box()
      .styleSheet(std::move(classes))
      .column()
      .width(kSideW)
      .flexShrink(0)
      .padding(11)
      .gap(4)
      .borderRadius({5})
      .fill(kPanelBg)
      // Every line in a panel is set in the mono face and the type-2 ink
      // unless it says otherwise; a line names its size and, where it
      // differs, the CLASS its colour and its face come from.
      .font({.face = monoFace()})
      .ink(kType2)
      .stroke(stroke(1.0f, Fill::color(kRule)))
      .overflow(Overflow::Clip)
      .key(kit::formatted("panel%d", order))
      .opacity(animate(from(0.0f).to(1.0f), {300ms, ch::easeOutQuad}))
      .translateX(animate(from(14.0f).to(0.0f), {300ms, ch::easeOutQuad}))
      .children({t(std::string(said["heading"].text()),
                   {.face = uiFace(), .size = 9.5f, .track = 2.2f}),
                 rule(390, kRule)});
}

auto SlitScan2001::prose(const data::Json& said) -> std::vector<Element> {
  using namespace slit;
  // ONE LINE OF A PANEL, as the document writes it: the words, the size
  // they are set at, and the class its colour and its face come from. A
  // record with no words is the furniture between them — a rule, or the
  // slot a live reading is rendered into. A MARKED line carries a red
  // pointer in the margin, which is how this sheet says "read this one".
  return each(said.items(), [](const data::Json& n) -> Element {
    if (n["rule"].boolean()) return rule(390, kRule);
    if (!n["slot"].text().empty())
      return slot(std::string(n["slot"].text()))
          .height((float)n["height"].number(19.0))
          .flexShrink(0);
    Element line = text(n["words"])
                       .font({.size = (float)n["size"].number(6.5)})
                       .styleClass(std::string(n["style"].text()))
                       .flexShrink(0);
    if (!n["marked"].boolean()) return line;
    return box().row().gap(5).flexShrink(0).children(
        {text("▸")
             .font({.face = uiFace(), .size = 9})
             .styleClass("red")
             .width(7),
         line.flexGrow(1)});
  });
}

auto SlitScan2001::s1Quote() -> Element {
  const data::Json& said = doc()["panels"][0];
  Element p = panelShell(said, 0);
  p.children({prose(said["lines"])});
  return p;
}

auto SlitScan2001::s2Lens() -> Element {
  const data::Json& said = doc()["panels"][1];
  Element p = panelShell(said, 1);
  p.children({prose(said["lines"])});
  return p;
}

auto SlitScan2001::s3Law() -> Element {
  using namespace slit;
  const data::Json& said = doc()["panels"][2];
  Element p = panelShell(said, 2);
  // THE MEASURED PROFILE against the analytic C/u, on log-log axes: the
  // frame is normalised so an exact 1/u law is the box diagonal, which
  // makes any departure from p = 1 a visible bow rather than a number.
  p.children(
      {box()
           .width(386)
           .height(62)
           .flexShrink(0)
           .fill(al(kBlack, 0.6f))
           .stroke(stroke(1.0f, Fill::color(kRule)))
           .children(
               {box()
                    .inset(4)
                    .shape(shapes::parametric(
                        [](float s) { return SkPoint{s, s}; }, 0.0f, 1.0f, 240,
                        false))
                    .stroke(spans::upTo(animate(
                                to(1.0f), {520ms, ch::easeOutCubic, 1500ms})),
                            stroke(1.6f, Fill::color(kAmber))),
                pen([this](Pen& p2) { drawMeasuredPoints(p2); }).inset(4)}),
       slot("fit").height(21).flexShrink(0)});
  p.children({prose(said["lines"])});
  return p;
}

auto SlitScan2001::s4Sampling() -> Element {
  using namespace slit;
  const data::Json& said = doc()["panels"][3];
  Element p = panelShell(said, 3);
  // SIX REAL WALLS, RENDERED SMALL: two spacing rules against three K,
  // each an instances() leaf over its own pool, so the argument is drawn
  // by the same code that draws the picture.
  p.children({each(
      wordsOf(said["rows"]), [this](const Utf8& name, size_t r) -> Element {
        return box()
            .row()
            .gap(5)
            .flexShrink(0)
            .alignItems(Align::Center)
            .children(
                {text(name).font({.size = 7.0f}).width(80),
                 each(std::array<int, 3>{0, 1, 2}, [this, r](int k) -> Element {
                   const size_t idx = r * 3 + (size_t)k;
                   return box()
                       .width(98)
                       .height(18)
                       .flexShrink(0)
                       .fill(kBlack)
                       .overflow(Overflow::Clip)
                       .key(kit::formatted("s4_%d", (int)idx))
                       .scaleX(animate(from(0.0f).to(1.0f),
                                       {220ms, ease::outBack()}))
                       .transformOrigin(pct(0), pct(50))
                       .children({instancing::instances(flatAtlas, s4[idx],
                                                        instancing::Mode::Data,
                                                        SkBlendMode::kPlus)});
                 })});
      })});
  p.children({box().row().gap(5).flexShrink(0).children(
      {box().width(80).flexShrink(0),
       each(wordsOf(said["keys"]), [](const Utf8& key) -> Element {
         return text(key).font({.size = 6.6f}).styleClass("tick").width(98);
       })})});
  p.children({prose(said["lines"])});
  return p;
}

auto SlitScan2001::sidebar() -> Element {
  using namespace slit;
  return box()
      .column()
      .width(kSideW)
      .height(kBodyH)
      .flexShrink(0)
      .justifyContent(Justify::SpaceBetween)
      .staggerChildren(85ms)
      .children({s1Quote(), s2Lens(), s3Law(), s4Sampling()});
}

auto SlitScan2001::readoutEl() -> Element {
  using namespace slit;
  const float z = kZ0In * std::pow(kR, -(float)tau);
  const int stampIdx = (int)(tau * (double)(kKDisplay - 1));
  const float u = kUFar * std::pow(kR, (float)tau);
  const float omega = kUFar / std::max(u, 1e-3f);
  return box().column().gap(2).width(262).children(
      {box().row().gap(12).children(
           {t(kit::formatted("z = %06.2f in", z),
              {.face = monoBoldFace(), .size = 9, .color = sigil::material::skia::toSkColor(kAmber)}),
            t(kit::formatted("m = ×%0.3f", kZ0In / std::max(z, 1e-3f)),
              {.size = 9, .color = sigil::material::skia::toSkColor(kType2)})}),
       box().row().gap(12).children(
           {t(kit::formatted("stamp %04d / %d", stampIdx, kKDisplay),
              {.size = 9, .color = sigil::material::skia::toSkColor(kType2)}),
            t(kit::formatted("ω = %0.4f", omega),
              {.size = 9, .color = sigil::material::skia::toSkColor(al(kCold, 0.9f))})}),
       t(kit::formatted("ONE ATLAS · %d×%d SHEET · ONE BAKE %.0f ms · "
                        "texWindows()",
                        sheetW, sheetH, bakeMs),
         {.size = 6.8f, .color = sigil::material::skia::toSkColor(kTick)})});
}

auto SlitScan2001::expoEl() -> Element {
  using namespace slit;
  return t(kit::formatted("SWEEP %3d%%  ·  z %06.2f in  ·  %d / %d STAMPS LAID",
                          (int)(tau * 100.0), kZ0In * std::pow(kR, -(float)tau),
                          (int)(tau * (double)kK), kK),
           {.size = 7.2f, .color = sigil::material::skia::toSkColor(al(kCold, 0.8f))});
}

auto SlitScan2001::fitEl() -> Element {
  using namespace slit;
  if (fixedStatus.clamped)
    return t("FIT SUPPRESSED — THIS FRAME DROPPED SIMULATED TIME",
             {.size = 8.2f, .color = sigil::material::skia::toSkColor(kRed)});
  return box().column().gap(1).children(
      {t(kit::formatted("FIT  E(u) = C / u^p     p = %0.4f     R² = %0.5f",
                        fitP, fitR2),
         {.face = monoBoldFace(), .size = 8.2f, .color = sigil::material::skia::toSkColor(al(kCold, 0.95f))}),
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
         {.size = 7.0f, .color = sigil::material::skia::toSkColor(al(kCold, 0.85f))}),
       t(kit::formatted("AT 4× IT IS %0.0f%% — AND p MOVES ONLY %0.4f → %0.4f. "
                        "THE LAW SURVIVES ITS OWN QUANTISATION.",
                        fitResid * 100.0f, fitPMin, fitP),
         {.size = 7.0f, .color = sigil::material::skia::toSkColor(al(kCold, 0.85f))})});
}
