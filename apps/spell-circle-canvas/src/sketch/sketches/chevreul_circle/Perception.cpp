#include "ChevreulCircle.h"

auto ChevreulCircle::theHeader() -> Element {
  Element g = box();
  g.child(label("1er CERCLE CHROMATIQUE DE Mr CHEVREUL — RENFERMANT LES "
                "COULEURS FRANCHES",
                sbd(26, kInk, 2.6f), 56, 40, 1700)
              .opacity(bind(&demo).window(0.0f, 0.02f)));
  g.child(label("PL. V · DES COULEURS ET DE LEURS APPLICATIONS AUX ARTS "
                "INDUSTRIELS · J.-B. BAILLIÈRE ET FILS, PARIS, 1864 · "
                "DIGEON SC. · LAMOUREUX IMP. · 37 CM",
                mn(9.5f, kInk2, 0.7f), 58, 84, 1700)
              .opacity(bind(&demo).window(0.01f, 0.04f)));
  g.child(at(56, 104, 1688, 1).fill(Fill::color(kRule)));
  return g;
}

auto ChevreulCircle::theLabPlot() -> Element {
  const float x0 = 852, y0 = 136, S = 380;
  const float cx = x0 + S * 0.5f, cy = y0 + 24 + (S - 48) * 0.5f;
  const float scale = (S - 96) * 0.5f / 60.0f;  // a* b* -60..+60
  // BY VALUE: this mapping is copied into a paint program below, which
  // the kernel invokes long after the frame that declared cx, cy and
  // the scale has returned.
  auto P = [cx, cy, scale](float a, float b) {
    return SkPoint{cx + a * scale, cy - b * scale};
  };
  Element g = box();
  g.child(
      at(x0, y0, S, S)
          .fill(Fill::color(kWell))
          .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
  g.child(label("CIELAB a* b* · THE 72 MEASURED COLOURS · 36 DIAMETERS",
                mn(8.5f, kInk, 0.5f), x0 + 10, y0 + 6, S - 20));

  // axes and ticks, drawn on hand-built geometry through the brush
  // vocabulary — decorations::paintOn is the seam for that.
  // The plot's frame is fixed, so the program is named and the node
  // settles between describes.
  auto axes = [=](SkCanvas& c, const PaintContext& pc) {
    SkPathBuilder ax;
    for (int t = -60; t <= 60; t += 20) {
      const SkPoint a = P((float)t, -60), b = P((float)t, 60);
      ax.moveTo(a.fX - x0, a.fY - y0);
      ax.lineTo(b.fX - x0, b.fY - y0);
      const SkPoint c0 = P(-60, (float)t), d0 = P(60, (float)t);
      ax.moveTo(c0.fX - x0, c0.fY - y0);
      ax.lineTo(d0.fX - x0, d0.fY - y0);
    }
    decorations::paintOn(c, pc, ax.detach(),
                         stroke(0.5f, Fill::color(hexColor(0x8C8578, 0.35f))));
    SkPathBuilder cross;
    const SkPoint o = P(0, 0);
    cross.moveTo(o.fX - x0 - 9, o.fY - y0);
    cross.lineTo(o.fX - x0 + 9, o.fY - y0);
    cross.moveTo(o.fX - x0, o.fY - y0 - 9);
    cross.lineTo(o.fX - x0, o.fY - y0 + 9);
    decorations::paintOn(c, pc, cross.detach(),
                         stroke(1.2f, Fill::color(kInk)));
  };
  g.child(at(x0, y0, S, S)
              .fill(Fill::none())
              .child(custom(std::string_view("lab-axes"), axes).inset(0)));

  // the 36 chords, drawing in one at a time
  for (int n = 0; n < 36; ++n) {
    const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
    const SkPoint B = P(lab[(size_t)n + 36].a, lab[(size_t)n + 36].b);
    SkRect bb = SkRect::MakeLTRB(std::min(A.fX, B.fX), std::min(A.fY, B.fY),
                                 std::max(A.fX, B.fX), std::max(A.fY, B.fY));
    bb.outset(2, 2);
    SkPathBuilder cb;
    cb.moveTo(A.fX - bb.left(), A.fY - bb.top());
    cb.lineTo(B.fX - bb.left(), B.fY - bb.top());
    const SkPath chord = cb.detach();
    const float lo = 0.19f + 0.0026f * (float)n;
    g.child(at(bb.left(), bb.top(), bb.width(), bb.height())
                .key("chord" + std::to_string(n))
                .fill(Fill::none())
                .shape(heldPath(chord))
                .stroke(spans::upTo(bind(&demo).window(lo, lo + 0.012f)),
                        stroke(0.8f, Fill::color(hexColor(0x8C8578, 0.85f)))));
  }
  // the 72 points, each in its own colour
  for (int n = 0; n < 72; ++n) {
    const SkPoint A = P(lab[(size_t)n].a, lab[(size_t)n].b);
    const float lo = 0.005f + 0.0021f * (float)n;
    g.child(kit::disc(A, 3.5f)
                .key("labpt" + std::to_string(n))
                .shape(shapes::circle())
                .fill(Fill::color(corrected[(size_t)n]))
                .stroke(stroke(0.4f, Fill::color(hexColor(0x221F1A, 0.5f))))
                .transformOrigin(0.5f, 0.5f)
                .opacity(bind(&demo).window(lo, lo + 0.01f)));
  }
  // the centroid — the piece's whole argument
  g.child(kit::disc(P(v.centA, v.centB), 9.0f)
              .key("centroid")
              .shape(shapes::circle())
              .fill(Fill::none())
              .stroke(stroke(1.6f, Fill::color(kRed)))
              .transformOrigin(0.5f, 0.5f)
              .scale(bind(&demo)
                         .window(0.275f, 0.30f)
                         .map(ch::EaseFn(ease::outBack(2.0f)))));
  g.child(label("a* →", mn(7, kInk2), x0 + S - 42, y0 + S - 44, 40));
  g.child(label("b* ↑", mn(7, kInk2), cx + 6, y0 + 26, 40));
  g.child(label("+ = a*b* ORIGIN     ○ = CENTROID OF THE 72",
                mn(7.0f, kInk2, 0.3f), x0 + 10, y0 + S - 46, S - 20));
  g.child(slot("chordcount"));
  return g;
}

auto ChevreulCircle::theObservations() -> Element {
  const float x0 = 1268, y0 = 136, W = 476, H = 380;
  Element g = box();
  g.child(
      at(x0, y0, W, H)
          .fill(Fill::color(kWell))
          .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
  g.child(label("LES DIX-SEPT OBSERVATIONS · §21–§37", mn(8.5f, kInk, 0.5f),
                x0 + 10, y0 + 6, W - 20));
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
    row.child(rightAt(std::to_string(o.plate), mn(7.5f, kInk2), 0, 3, 16));
    row.child(at(22, 1, 15, 15).fill(Fill::color(ca)));
    row.child(at(37, 1, 15, 15).fill(Fill::color(cb)));
    row.child(label("→", mn(8, kInk2), 56, 1, 14));
    // the predicted pair fades in AFTER its sources land
    Element pred = box()
                       .key("pr" + std::to_string(i))
                       .opacity(bind(&demo).window(lo + 0.003f, lo + 0.009f));
    pred.child(
        at(72, 1, 15, 15)
            .fill(Fill::color(predicted(ca, kNewton[(size_t)o.b], corrected))));
    pred.child(
        at(87, 1, 15, 15)
            .fill(Fill::color(predicted(cb, kNewton[(size_t)o.a], corrected))));
    row.child(std::move(pred));
    row.child(label(kit::formatted("%s · %s", kNewtonName[(size_t)o.a],
                                   kNewtonName[(size_t)o.b]),
                    mn(7.0f, kInk, 0.2f), 108, 3, 108));
    row.child(label(kit::formatted("%s / %s", o.modA, o.modB), it(8.5f, kInk2),
                    218, 1.5f, 250));
    g.child(std::move(row));
  }
  g.child(label(kit::formatted("C(7,2) = %d − 4 complémentaires = %d      "
                               "(by geometry: %d, or %d — neither is 17)",
                               v.pairs21, v.byName, v.byStrict, v.byLoose),
                mn(8.0f, kRed, 0.2f), x0 + 10, y0 + H - 32, W - 20)
              .opacity(bind(&demo).window(0.79f, 0.80f)));
  g.child(
      label("indigo read as BLEU-VIOLET (54); greenish-yellow as "
            "JAUNE-VERT (30) — both readings, not citations",
            mn(6.5f, kInk2, 0.2f), x0 + 10, y0 + H - 18, W - 20));
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
    g.child(std::move(band));
  }
  return g;
}

auto ChevreulCircle::theIllusion() -> Element {
  Element g = box();
  g.child(label("THE CHEVREUL ILLUSION · TWENTY FLAT BANDS", mn(9, kInk, 0.6f),
                852, 552, 500));
  g.child(
      rightAt("§164 read as equal REFLECTANCE   ·   the modern equal "
              "code-value ramp   ·   §164 under γ 2.2",
              mn(7.5f, kInk2, 0.3f), 1100, 553, 644));

  g.child(
      at(kStairX, kStairYA - 2, kBandW * kBandN, kStairH + 4)
          .background(styles::dropShadow(hexColor(0x3A352D, 0.22f), {2, 2}, 5))
          .fill(Fill::color(kWell)));
  g.child(aStaircase(gamme, kStairYA, kStairH, "sa", true));
  g.child(label("§164 · Y = (20−t)/19, sRGB-encoded · tone 10 = " +
                    hexOf(gamme[9]) + " (not #808080)",
                mn(7.0f, kInk2, 0.2f), kStairX, kStairYA + kStairH + 5, 520));

  g.child(
      at(kStairX, kStairYB - 2, kBandW * kBandN, kStairH + 4)
          .background(styles::dropShadow(hexColor(0x3A352D, 0.22f), {2, 2}, 5))
          .fill(Fill::color(kWell)));
  g.child(aStaircase(gammeCode, kStairYB, kStairH, "sb", true));
  g.child(label("equal code value · tone 10 = " + hexOf(gammeCode[9]) +
                    " (Y = 0.216, not 0.526) — the lerp this piece "
                    "deliberately does not do",
                mn(7.0f, kInk2, 0.2f), kStairX, kStairYB + kStairH + 5, 720));

  // the OCIO strip
  if (v.ocioAvailable) {
    g.child(aStaircase(gamme, kStairYC, 28.0f, "sc", false, true));
    g.child(label(
        kit::formatted("§164 ramp under ocio::exponent(2.2) — an OCIO-baked "
                       "LUT Effect: tone 10 %s measures %s through it",
                       hexOf(gamme[9]).c_str(), v.ocioSample.c_str()),
        mn(7.0f, kInk2, 0.2f), kStairX, kStairYC + 32, 760));
  } else {
    g.child(label("OCIO: not compiled in", mn(9.0f, kRed, 0.4f), kStairX,
                  kStairYC + 10, 400));
  }

  // every fourth band's hex, inked against its own band
  for (int b = 0; b < kBandN; b += 4)
    g.child(
        centred(hexOf(gamme[(size_t)b]), mn(6.8f, b < 12 ? kInk : kWhite, 0),
                kStairX + (float)b * kBandW, kStairYA + kStairH - 12, kBandW));

  g.child(
      label("“the light tone will appear lighter, and the deep tone "
            "deeper, commencing at the line of contact” — Introduction",
            it(9.5f, kInk), 852, 774, 600));
  g.child(
      rightAt(kit::formatted("%d bands · per-band σ = %.2f · %d/%d hexes exact "
                             "byte for byte",
                             v.bands, v.bandSigmaMax, v.bandsExact, v.bands),
              mn(8.5f, kRed, 0.2f), 1300, 776, 444));
  return g;
}

auto ChevreulCircle::theContrast() -> Element {
  const float x0 = 852, y0 = 838, W = 380;
  Element g = box();
  g.child(label("SIMULTANEOUS CONTRAST · TWELVE IDENTICAL PATCHES",
                mn(8.5f, kInk, 0.5f), x0, y0, W));
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
    lattice.child(at(patch.fLeft, patch.fTop, patch.width(), patch.height())
                      .fill(Fill::color(corrected[(size_t)i * 6])));
  }
  g.child(std::move(lattice));
  for (int i = 0; i < 12; ++i) {
    const SkRect patch = arrange::cellRect(arrange::cellAt((size_t)i, 4),
                                           {cw - 3, chh - 3}, {3, 3}, {gx, gy});
    g.child(at(patch.fLeft + (cw - 3 - 30) * 0.5f,
               patch.fTop + (chh - 3 - 30) * 0.5f, 30, 30)
                .fill(Fill::color(gamme[14])));  // Chevreul's grey tone 15
  }
  const float ry = gy + 3 * chh + 6;
  for (int i = 0; i < 12; ++i)
    g.child(at(arrange::cellRect({i, 0}, {24, 24}, {3, 0}, {gx, ry}).fLeft, ry,
               24, 24)
                .fill(Fill::color(gamme[14])));
  g.child(label(
      kit::formatted("all twelve patches are %s — Chevreul's grey, tone 15",
                     hexOf(gamme[14]).c_str()),
      mn(8.0f, kRed, 0.2f), x0, ry + 28, W));
  g.child(label("§16: “they will appear as dissimilar as possible”",
                it(8.5f, kInk2), x0, ry + 42, W));
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
    g.child(at(x0, y0, 380, 96).child(text(lawPara, o).width(Dim(380))));

  // The words are the run's own — the label each claim was made under,
  // the figure it came to, and the verdict `measure::Check` computed from
  // the two, so there is no second hand-typed one beside it. What the
  // plate adds is the MARK before each row, which carries that verdict as
  // colour: a claim that failed is red, a finding that failed is red too
  // (its failing is Chevreul's, and the summary counts the two apart), a
  // reading is the quiet rule grey it has no verdict to earn.
  const float ty0 = y0 + 88, lh = 11.0f;
  const size_t rows = verdict.rows.size();
  g.child(
      at(x0 - 8, ty0 - 8, W - 4, (float)rows * lh + 16)
          .fill(Fill::color(kWell))
          .foreground(stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
  g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", mn(7.5f, kInk2, 0.5f), x0,
                ty0 - 22, W));
  std::vector<sketch::kit::Row> lines;
  lines.reserve(rows);
  for (size_t i = 0; i < rows; ++i) {
    const measure::Check& c = verdict.rows[i];
    std::string verdictWord;
    if (c.judged()) verdictWord = c.pass ? "PASS" : "FAIL want " + c.expected;
    lines.push_back(
        {.cells = {U(c.label), U(c.actual), U(verdictWord)},
         .swatch = Fill::color(!c.judged() ? kRule : (c.pass ? kInk : kRed)),
         .key = "vr" + std::to_string(i)});
  }
  const float lo = 0.30f, hi = 0.30f + 0.034f * (float)(rows - 1) + 0.012f;
  g.child(at(x0, ty0, W - 20, (float)rows * lh)
              .opacity(bind(&demo).window(lo, hi))
              .child(sketch::kit::table(std::move(lines),
                                        {.columns = {{222}, {66, true}, {}}})));
  g.child(
      label("§38: “do we know, at the present day, of two coloured "
            "bodies … Certainly not!”",
            it(8.5f, kInk2), x0, ty0 + (float)rows * lh + 12, W));
  return g;
}
