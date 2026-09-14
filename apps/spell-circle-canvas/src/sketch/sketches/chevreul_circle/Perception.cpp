#include "ChevreulCircle.h"

auto ChevreulCircle::theHeader() -> Element {
  Element g = box();
  g.children(
      {label(doc["title"], 56, 40, 1700)
           .font(
               {.face = serifBold(), .size = 26, .color = kInk, .track = 2.6f})
           .opacity(bind(&demo).window(0.0f, 0.02f)),
       label(doc["imprint"], 58, 84, 1700)
           .font({.size = 9.5f, .track = 0.7f})
           .opacity(bind(&demo).window(0.01f, 0.04f)),
       at(56, 104, 1688, 1).fill(Fill::color(kRule))});
  return g;
}

auto ChevreulCircle::theLabPlot() -> Element {
  const float x0 = 852, y0 = 136, S = 380;
  // WHAT THE TWO AXES MEAN: a* across, b* up, ±60 of each, with the
  // engraving's own margin kept inside the well. No point, chord, label
  // or centroid below turns a colour measurement into a pixel.
  const sketch::kit::Plot field{
      .x = {.domain = {-60, 60}}, .y = {.domain = {-60, 60}}, .pad = 48};
  // THE 36 DIAMETERS: each a line between two of the measurements, drawn
  // on one at a time. Every chord stands in the bounds of its own two ends
  // and is shaped by the layer, so it states only the pen it is stroked
  // with and nothing here turns a colour measurement into a pixel.
  const auto chord = [this](std::size_t n, std::size_t) {
    const float lo = 0.19f + 0.0026f * (float)n;
    return box()
        .key("chord" + std::to_string(n))
        .stroke(spans::upTo(bind(&demo).window(lo, lo + 0.012f)),
                stroke(0.8f, Fill::currentInk()));
  };
  // THE 72 MEASUREMENTS, each a dot in its own measured colour, dealt in
  // one at a time; and the centroid that is the piece's whole argument.
  const auto measured = [this](const Lab&, std::size_t n) {
    const float lo = 0.005f + 0.0021f * (float)n;
    return box()
        .width(7)
        .height(7)
        .key("labpt" + std::to_string(n))
        .shape(shapes::circle())
        .fill(Fill::color(corrected[n]))
        .stroke(stroke(0.4f, Fill::color(hexColor(0x221F1A, 0.5f))))
        .transformOrigin(0.5f, 0.5f)
        .opacity(bind(&demo).window(lo, lo + 0.01f));
  };
  const auto centroid = [this](const sketch::kit::Datum&) {
    return box()
        .width(18)
        .height(18)
        .key("centroid")
        .shape(shapes::circle())
        .stroke(stroke(1.6f, Fill::currentInk()))
        .transformOrigin(0.5f, 0.5f)
        .scale(bind(&demo)
                   .window(0.275f, 0.30f)
                   .map(ch::EaseFn(ease::outBack(2.0f))));
  };
  const std::array<sketch::kit::Datum, 1> centre{{{v.centA, v.centB}}};
  const std::vector<double> ladder{-60, -40, -20, 0, 20, 40, 60};
  return box().children(
      {at(x0, y0, S, S)
           .fill(Fill::color(kWell))
           .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)),
       label(doc["lab.head"], x0 + 10, y0 + 6, S - 20).styleClass("heading"),
       sketch::kit::plot(
           "lab", field,
           {sketch::kit::rules(
                {.x = ladder, .y = ladder, .pen = {.width = 0.5f}}),
            sketch::kit::axis({.of = sketch::kit::Axis::X,
                               .at = 0.0,
                               .width = 1.2f,
                               .reach = 0,
                               .numbers = false}),
            sketch::kit::axis({.of = sketch::kit::Axis::Y,
                               .at = 0.0,
                               .width = 1.2f,
                               .reach = 0,
                               .numbers = false}),
            sketch::kit::segments(
                std::views::iota(std::size_t{0}, std::size_t{36}), chord,
                {.x = [this](const std::size_t& n) { return lab[n].a; },
                 .y = [this](const std::size_t& n) { return lab[n].b; },
                 .toX = [this](const std::size_t& n) { return lab[n + 36].a; },
                 .toY = [this](const std::size_t& n) { return lab[n + 36].b; },
                 .styleClass = "chord"}),
            sketch::kit::marks(lab, measured, {.x = &Lab::a, .y = &Lab::b}),
            sketch::kit::marks(centre, centroid,
                               {.x = &sketch::kit::Datum::x,
                                .y = &sketch::kit::Datum::y,
                                .styleClass = "centroid"}),
            sketch::kit::label("a* →", 58, -54,
                               {.anchor = {.across = Align::End}}),
            sketch::kit::label("b* ↑", 3, 54,
                               {.anchor = {.across = Align::Start}}),
            sketch::kit::label(
                "+ = a*b* ORIGIN     ○ = CENTROID OF THE 72", -58, -58,
                {.anchor = {.across = Align::Start, .down = Align::End}})})
           .rect({x0, y0, x0 + S, y0 + S}),
       slot("chordcount")});
}

auto ChevreulCircle::theObservations() -> Element {
  const float x0 = 1268, y0 = 136, W = 476, H = 380;
  Element g = box();
  g.children(
      {at(x0, y0, W, H)
           .fill(Fill::color(kWell))
           .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)),
       label(doc["obs.head"], x0 + 10, y0 + 6, W - 20).styleClass("heading")});
  const float rowH = 19.4f, top = y0 + 24;
  for (size_t i = 0; i < kObs.size(); ++i) {
    const Observation& o = kObs[i];
    const float y = top + (float)i * rowH;
    const float lo = 0.645f + 0.0075f * (float)i;
    const SkColor4f ca = corrected[(size_t)kNewton[(size_t)o.a]];
    const SkColor4f cb = corrected[(size_t)kNewton[(size_t)o.b]];
    Element row = at(x0 + 8, y, W - 16, rowH - 2)
                      .key("obs" + std::to_string(i))
                      .opacity(bind(&demo).window(lo, lo + 0.006f));
    row.children(
        {rightAt(std::to_string(o.plate), 0, 3, 16).font({.size = 7.5f}),
         at(22, 1, 15, 15).fill(Fill::color(ca)),
         at(37, 1, 15, 15).fill(Fill::color(cb)),
         label("→", 56, 1, 14).font({.size = 8})});
    // the predicted pair fades in AFTER its sources land
    Element pred = box()
                       .key("pr" + std::to_string(i))
                       .opacity(bind(&demo).window(lo + 0.003f, lo + 0.009f));
    pred.children(
        {at(72, 1, 15, 15)
             .fill(Fill::color(predicted(ca, kNewton[(size_t)o.b], corrected))),
         at(87, 1, 15, 15)
             .fill(
                 Fill::color(predicted(cb, kNewton[(size_t)o.a], corrected)))});
    row.children(
        {std::move(pred),
         label(kit::formatted("%s · %s", kNewtonName[(size_t)o.a],
                              kNewtonName[(size_t)o.b]),
               108, 3, 108)
             .styleClass("note")
             .ink(kInk),
         label(kit::formatted("%s / %s", o.modA, o.modB), 218, 1.5f, 250)
             .styleClass("quote")});
    g.children({std::move(row)});
  }
  g.children({label(kit::formatted("C(7,2) = %d − 4 complémentaires = %d      "
                                   "(by geometry: %d, or %d — neither is 17)",
                                   v.pairs21, v.byName, v.byStrict, v.byLoose),
                    x0 + 10, y0 + H - 32, W - 20)
                  .styleClass("finding")
                  .opacity(bind(&demo).window(0.79f, 0.80f)),
              label(doc["obs.note"], x0 + 10, y0 + H - 18, W - 20)
                  .styleClass("column")});
  return g;
}

auto ChevreulCircle::aStaircase(const std::array<SkColor4f, 20>& ramp, float y,
                                float h, const char* keyBase, bool withGap,
                                bool graded) -> Element {
  Element g = box();
  for (int b = 0; b < kBandN; ++b) {
    Element band = at(kStairX + (float)b * kBandW, y, kBandW, h)
                       .key(kit::formatted("%s%d", keyBase, b))
                       .fill(Fill::color(ramp[(size_t)b]));
    if (graded)
      band.effect(Effect::recipe(ocio::exponent(2.2f))).cache(Cache::Texture);
    if (withGap)
      band.translateX(bind(&demo)
                          .window(0.30f, 0.50f)
                          .map(pulses(2))
                          .scale(((float)b - 9.5f) * 2.6f));
    g.children({std::move(band)});
  }
  return g;
}

auto ChevreulCircle::theIllusion() -> Element {
  Element g = box();
  g.children(
      {label(doc["illusion.head"], 852, 552, 500)
           .font({.size = 9, .color = kInk, .track = 0.6f}),
       rightAt(doc["illusion.strips"], 1100, 553, 644)
           .font({.size = 7.5f, .track = 0.3f}),
       at(kStairX, kStairYA - 2, kBandW * kBandN, kStairH + 4)
           .background(styles::dropShadow(hexColor(0x3A352D, 0.22f), {2, 2}, 5))
           .fill(Fill::color(kWell)),
       aStaircase(gamme, kStairYA, kStairH, "sa", true),
       label("§164 · Y = (20−t)/19, sRGB-encoded · tone 10 = " +
                 hexOf(gamme[9]) + " (not #808080)",
             kStairX, kStairYA + kStairH + 5, 520)
           .styleClass("note"),
       at(kStairX, kStairYB - 2, kBandW * kBandN, kStairH + 4)
           .background(styles::dropShadow(hexColor(0x3A352D, 0.22f), {2, 2}, 5))
           .fill(Fill::color(kWell)),
       aStaircase(gammeCode, kStairYB, kStairH, "sb", true),
       label("equal code value · tone 10 = " + hexOf(gammeCode[9]) +
                 " (Y = 0.216, not 0.526) — the lerp this piece "
                 "deliberately does not do",
             kStairX, kStairYB + kStairH + 5, 720)
           .styleClass("note")});

  // the OCIO strip
  if (v.ocioAvailable) {
    g.children(
        {aStaircase(gamme, kStairYC, 28.0f, "sc", false, true),
         label(kit::formatted("§164 ramp under ocio::exponent(2.2) — an "
                              "OCIO-baked LUT Effect: tone 10 %s measures "
                              "%s through it",
                              hexOf(gamme[9]).c_str(), v.ocioSample.c_str()),
               kStairX, kStairYC + 32, 760)
             .styleClass("note")});
  } else {
    g.children({label(doc["illusion.noOcio"], kStairX, kStairYC + 10, 400)
                    .font({.size = 9, .color = kRed, .track = 0.4f})});
  }

  // every fourth band's hex, inked against its own band
  for (int b = 0; b < kBandN; b += 4)
    g.children({centred(hexOf(gamme[(size_t)b]), kStairX + (float)b * kBandW,
                        kStairYA + kStairH - 12, kBandW)
                    .font({.size = 6.8f, .color = b < 12 ? kInk : kWhite})});

  g.children(
      {label(doc["illusion.quote"], 852, 774, 600)
           .styleClass("quote")
           .font({.size = 9.5f, .color = kInk}),
       rightAt(kit::formatted("%d bands · per-band σ = %.2f · %d/%d hexes "
                              "exact byte for byte",
                              v.bands, v.bandSigmaMax, v.bandsExact, v.bands),
               1300, 776, 444)
           .styleClass("finding")
           .font({.size = 8.5f})});
  return g;
}

auto ChevreulCircle::theContrast() -> Element {
  const float x0 = 852, y0 = 838, W = 380;
  Element g = box();
  g.children(
      {label("SIMULTANEOUS CONTRAST · TWELVE IDENTICAL PATCHES", x0, y0, W)
           .styleClass("heading")});
  const float cw = 88, chh = 66, gx = x0 + 6, gy = y0 + 18;
  // Beat 4's grounds arrive and withdraw as a DIRECTIONAL WIPE at 90 deg
  // (downward), which is what Chevreul's own method looks like: take the
  // ground away and the twelve patches are plainly identical.
  // wipe() reveals the fraction of THE NODE'S OWN LAID-OUT BOX before the
  // edge, so the container has to be a real box: a bare box() holding
  // absolutely-positioned children measures zero and the wipe hides the
  // whole subtree with no diagnostic at all. Hence the explicit rect.
  Element lattice =
      at(gx, gy, 4 * cw, 3 * chh)
          .key("grounds")
          .mask(by::edge(
              90.0f, bind(&demo).window(0.50f, 0.64f).map(upHoldAwayBack())));
  for (int i = 0; i < 12; ++i) {
    const SkRect patch = arrange::cellRect(arrange::cellAt((size_t)i, 4),
                                           {cw - 3, chh - 3}, {3, 3});
    lattice.children({at(patch.fLeft, patch.fTop, patch.width(), patch.height())
                          .fill(Fill::color(corrected[(size_t)i * 6]))});
  }
  g.children({std::move(lattice)});
  for (int i = 0; i < 12; ++i) {
    const SkRect patch = arrange::cellRect(arrange::cellAt((size_t)i, 4),
                                           {cw - 3, chh - 3}, {3, 3}, {gx, gy});
    g.children({at(patch.fLeft + (cw - 3 - 30) * 0.5f,
                   patch.fTop + (chh - 3 - 30) * 0.5f, 30, 30)
                    .fill(Fill::color(gamme[14]))});  // Chevreul's grey tone 15
  }
  const float ry = gy + 3 * chh + 6;
  for (int i = 0; i < 12; ++i)
    g.children({at(arrange::cellRect({i, 0}, {24, 24}, {3, 0}, {gx, ry}).fLeft,
                   ry, 24, 24)
                    .fill(Fill::color(gamme[14]))});
  g.children(
      {label(kit::formatted("all twelve patches are %s — Chevreul's grey, "
                            "tone 15",
                            hexOf(gamme[14]).c_str()),
             x0, ry + 28, W)
           .styleClass("finding"),
       label("§16: “they will appear as dissimilar as possible”", x0, ry + 42,
             W)
           .styleClass("quote")});
  return g;
}

auto ChevreulCircle::theVerification() -> Element {
  const float x0 = 1268, y0 = 838, W = 476;
  Element g = box();
  // the justified law, at a real measure
  weave::ParagraphLayoutOptions o;
  o.alignment = weave::TextAlignment::kJustify;
  o.lineBreakStrategy = weave::LineBreakStrategy::kKnuthPlass;
  o.hyphenation.enabled = true;
  o.hyphenation.penalty = 45.0f;
  o.justification.spaceStretch = 0.55f;
  o.justification.spaceShrink = 0.30f;
  o.justification.lastLineAlignment = weave::TextAlignment::kStart;
  o.knuthPlass.tolerance = 6000.0f;
  o.lineMetrics.height = 16.0f;
  if (lawPara)
    g.children({at(x0, y0, 380, 96).children({text(lawPara, o).width(380)})});

  // The words are the run's own — the label each claim was made under,
  // the figure it came to, and the verdict `measure::Check` computed from
  // the two, so there is no second hand-typed one beside it. What the
  // plate adds is the MARK before each row, which carries that verdict as
  // colour: a claim that failed is red, a finding that failed is red too
  // (its failing is Chevreul's, and the summary counts the two apart), a
  // reading is the quiet rule grey it has no verdict to earn.
  const float ty0 = y0 + 88, lh = 11.0f;
  const size_t rows = verdict.rows.size();
  g.children(
      {at(x0 - 8, ty0 - 8, W - 4, (float)rows * lh + 16)
           .fill(Fill::color(kWell))
           .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)),
       label("VERIFIED AT STARTUP, NOT ASSERTED", x0, ty0 - 22, W)
           .font({.size = 7.5f, .track = 0.5f})});
  std::vector<sketch::kit::Row> lines;
  lines.reserve(rows);
  for (size_t i = 0; i < rows; ++i) {
    const measure::Check& c = verdict.rows[i];
    std::string verdictWord;
    if (c.judged()) verdictWord = c.pass ? "PASS" : "FAIL want " + c.expected;
    lines.push_back(
        {.cells = {c.label, c.actual, verdictWord},
         .swatch = Fill::color(!c.judged() ? kRule : (c.pass ? kInk : kRed)),
         .key = "vr" + std::to_string(i)});
  }
  const float lo = 0.30f, hi = 0.30f + 0.034f * (float)(rows - 1) + 0.012f;
  g.children(
      {at(x0, ty0, W - 20, (float)rows * lh)
           .opacity(bind(&demo).window(lo, hi))
           .children({sketch::kit::table(
               std::move(lines), {.columns = {{.width = 222},
                                              {.width = 66, .figure = true},
                                              {}}})}),
       label(doc["contrast.quote"], x0, ty0 + (float)rows * lh + 12, W)
           .styleClass("quote")});
  return g;
}
