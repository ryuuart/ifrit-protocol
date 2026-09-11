#include "SigillumAemeth.h"

auto SigillumAemeth::solverOverlay() -> Element {
  auto g = box().inset(0);
  for (int n = 0; n < 7; ++n) {
    const Solved& s = solved[(size_t)n];
    const float t0 = tSolve + (float)n * tSolveEach;
    // ONE path per Name, one CONTOUR per hop. trim() walks every contour
    // as a single arc-length coordinate, so 0 -> 1 marches the walk hop by
    // hop — the whole solve is two nodes per Name instead of sixteen.
    std::vector<SkPoint> from, to, ctrl, land;
    for (size_t h = 0; h < s.cells.size(); ++h) {
      land.push_back(P((float)(s.cells[h] - 1) * 9.0f, rCellLet));
      if (h + 1 >= s.cells.size()) break;
      const int a0 = s.cells[h] - 1, b0 = s.cells[h + 1] - 1;
      const SkPoint a = P((float)a0 * 9.0f, rCellLet - 0.028f);
      const SkPoint b = P((float)b0 * 9.0f, rCellLet - 0.028f);
      int d = b0 - a0;
      while (d > 20) d -= 40;
      while (d < -20) d += 40;
      const float bulge = std::min(0.46f, std::fabs((float)d) * 0.030f);
      const SkPoint mid{(a.fX + b.fX) * 0.5f, (a.fY + b.fY) * 0.5f};
      const SkVector toC{kRR - mid.fX, kRR - mid.fY};
      const float m = std::max(1e-3f, std::hypot(toC.fX, toC.fY));
      from.push_back(a);
      to.push_back(b);
      ctrl.push_back(
          {mid.fX + toC.fX / m * bulge * kR, mid.fY + toC.fY / m * bulge * kR});
    }
    (void)t0;
    if (solvePhase < 0 || n > solvePhase) continue;
    const bool live = (n == solvePhase);
    // `from` is the walk's own point list here, so the entrance factory
    // needs its namespace.
    auto fade = [&](float) -> Animatable<float> {
      return live ? Animatable<float>(
                        animate(motion::from(0.0f).to(1.0f), ramp(0, 220)))
                  : Animatable<float>(0.15f);
    };
    auto reveal = [&](float ms) -> Animatable<float> {
      return live ? Animatable<float>(
                        animate(motion::from(0.0f).to(1.0f), ramp(0, ms)))
                  : Animatable<float>(1.0f);
    };
    g.child(
        box()
            .inset(0)
            .shape(heldPath([&] {
              SkPathBuilder b;
              for (size_t i = 0; i < from.size(); ++i) {
                b.moveTo(from[i]);
                b.quadTo(ctrl[i], to[i]);
              }
              return b.detach();
            }()))
            .fill(Fill::none())
            .stroke(spans::upTo(reveal(760.0f)),
                    lines::Line{.width = 2.6f,
                                .fill = Fill::color(hexColor(0x7fd0f4, 0.95f)),
                                .endCap = lines::Cap::Arrow,
                                .capSize = 15.0f})
            .opacity(fade(0))
            .key("hops" + std::to_string(n)));
    g.child(box()
                .inset(0)
                .shape(heldPath([&] {
                  SkPathBuilder b;
                  for (const SkPoint& q : land)
                    b.addCircle(q.fX, q.fY, 0.030f * kR);
                  return b.detach();
                }()))
                .fill(Fill::none())
                .stroke(spans::upTo(reveal(820.0f)),
                        PathFormat{.width = 2.0f,
                                   .strokeFill =
                                       Fill::color(hexColor(0x59b6e8, 0.92f))})
                .opacity(fade(0))
                .key("lands" + std::to_string(n)));
  }
  return g;
}

auto SigillumAemeth::margin() -> Element {
  const float w = 690;
  auto g = box()
               .rect(SkRect::MakeXYWH(1660 * kS, 56 * kS, w, 1588))
               .scale(kS)
               .transformOrigin(0.0f, 0.0f);

  g.child(text(toU8("SIGILLVM DEI \xc3\x86M\xc3\x86TH"),
               type(faceDisplay, 46, kVellum, 2.6f))
              .at({0, 0}));
  g.child(text(toU8("EMETH nuncupatum \xc2\xb7 Mortlake by Richemond \xc2\xb7 "
                    "21 Martii 1582"),
               type(faceItalic, 19, hexColor(0xc7ab74)))
              .at({2, 58}));
  g.child(
      text(toU8("BL Sloane MS 3188 f.30r \xc2\xb7 wax disc BM 1838,1232.90.a "
                "\xc2\xb7 23.2 cm"),
           type(faceSerif, 15, hexColor(0x8d7a58)))
          .at({2, 86}));
  g.child(box()
              .rect(SkRect::MakeXYWH(0, 114, w, 2))
              .fill(Fill::none())
              .shape(keyedShape(w,
                                [w](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(0, 1);
                                  b.lineTo(w, 1);
                                  return b.detach();
                                }))
              .stroke(lines::rails(
                  {{.across = 0.0f,
                    .width = 2.4f,
                    .fill = Fill::color(hexColor(0xc7ab74, 0.75f))},
                   {.across = -5.0f,
                    .width = 0.8f,
                    .fill = Fill::color(hexColor(0xc7ab74, 0.40f)),
                    .dash = {2.0f, 5.0f}}})));

  // the seven Names, printing as the walk finds them
  g.child(text(toU8("THE SEVEN NAMES, WALKED OFF THE RIM"),
               type(faceMono, 15, kRubric, 1.6f))
              .at({0, 136}));
  g.child(box()
              .rect(SkRect::MakeXYWH(0, 158, w, 324))
              .shape(keyedShape(w,
                                [w](SkSize) {
                                  SkPathBuilder b;
                                  for (int n = 0; n <= 7; ++n) {
                                    b.moveTo(0, 4 + (float)n * 46);
                                    b.lineTo(w, 4 + (float)n * 46);
                                  }
                                  return b.detach();
                                }))
              .fill(Fill::none())
              .stroke(PathFormat{
                  .width = 0.8f,
                  .strokeFill = Fill::color(hexColor(0xc7ab74, 0.14f))}));
  for (int n = 0; n < 7; ++n) {
    const Solved& s = solved[(size_t)n];
    const float y = 166 + (float)n * 46;
    const float at = (tSolve + (float)n * tSolveEach) * 1000;
    std::string chain;
    for (size_t i = 0; i < s.cells.size(); ++i)
      chain += (i ? "\xc2\xb7" : "") + std::to_string(s.cells[i]);
    g.child(text(toU8(std::to_string(n + 1) + "."),
                 type(faceMono, 17, hexColor(0x8d7a58)))
                .at({0, y + 6})
                .opacity(animate(from(0.0f).to(1.0f), ramp(at, 300))));
    g.child(
        text(toU8(kNames[(size_t)n].name), type(faceDisplay, 30, kVellum, 1.2f))
            .at({34, y})
            .opacity(animate(from(0.0f).to(1.0f), ramp(at + 120, 420))));
    g.child(
        text(toU8(s.raw == s.reduced ? ""
                                     : "\xe2\x9f\xa8" + s.raw + "\xe2\x9f\xa9"),
             type(faceItalic, 15, hexColor(0x6f5f45)))
            .at({212, y + 10})
            .opacity(animate(from(0.0f).to(1.0f), ramp(at + 240, 420))));
    g.child(text(toU8(chain), type(faceMono, 14, kTrace))
                .at({320, y + 10})
                .opacity(animate(from(0.0f).to(1.0f), ramp(at + 60, 420))));
  }

  // the leftovers
  {
    std::string un, unl;
    for (int i = 0; i < 40; ++i)
      if (!visited[(size_t)i]) {
        un += (un.empty() ? "" : "\xc2\xb7") + std::to_string(i + 1);
        unl += kRing[(size_t)i].glyph;
      }
    g.child(
        text(toU8(kit::formatted(
                 "%d of 40 cells consumed \xc2\xb7 %d never visited", usedCells,
                 40 - usedCells)),
             type(faceMono, 15, hexColor(0x8d7a58)))
            .at({0, 492})
            .opacity(animate(from(0.0f).to(1.0f), ramp(tDark * 1000, 500))));
    g.child(text(toU8("unvisited  " + un + "   =  " + unl),
                 type(faceMono, 15, kRubric))
                .at({0, 514})
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tDark * 1000 + 200, 500))));
    g.child(text(toU8("\xe2\x86\xb3 the same rule reads them as YMON 22\xc2\xb7"
                      "7\xc2\xb7\x31\x33\xc2\xb7\x33\x31 and BORAOTH "
                      "26\xc2\xb7\x33\x36\xc2\xb7\x31\x39\xc2\xb7\xe2\x80\xa6"),
                 type(faceItalic, 14, hexColor(0x6f5f45)))
                .at({0, 536})
                .opacity(animate(from(0.0f).to(1.0f),
                                 ramp(tDark * 1000 + 400, 500))));
  }

  // the 7×7 square the birds delivered; read DOWN the columns
  g.child(text(toU8("SEVEN BASKETS, SEVEN BIRDS \xc2\xb7 READ DOWN"),
               type(faceMono, 15, kRubric, 1.6f))
              .at({0, 580}));
  // The seven angles UNROLLED, not tabulated. On the plate these rows lie
  // along seven sides of a heptagon; here they lie on seven nested arcs of
  // the same fan, so a "column" is a RADIAL RAY and reading down a column
  // is reading outward — which is what the columns do on the object. A
  // leader curves from each ray to the archangel it spells.
  const float fanCx = 250.0f, fanCy = 1028.0f;
  const float fanR0 = 420.0f, fanDR = 33.0f, fanSpan = 46.0f;
  auto fanPt = [&](int row, int col, float dr) {
    const float a =
        (-fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f) * kD;
    const float rr = fanR0 - (float)row * fanDR + dr;
    return arrange::onEllipse({fanCx, fanCy}, {rr, rr}, a - 1.5707963f);
  };
  auto fanAngle = [&](int col) {
    return -fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f;
  };
  // the seven arcs the rows sit on — ruled first, as on a prepared sheet
  g.child(box()
              .rect(SkRect::MakeXYWH(0, 560, w, 300))
              .shape(keyedShape(
                  std::tuple{fanCx, fanCy, fanR0, fanDR, fanSpan},
                  [fanCx, fanCy, fanR0, fanDR, fanSpan](SkSize) {
                    SkPathBuilder b;
                    for (int r = 0; r <= 7; ++r) {
                      const float rr = fanR0 - (float)r * fanDR + fanDR * 0.5f;
                      for (int i = 0; i <= 24; ++i) {
                        const float a = (-fanSpan * 0.54f +
                                         fanSpan * 1.08f * (float)i / 24.0f) *
                                        kD;
                        const SkPoint q = arrange::onEllipse(
                            {fanCx, fanCy - 560.0f}, {rr, rr}, a - 1.5707963f);
                        i == 0 ? b.moveTo(q) : b.lineTo(q);
                      }
                    }
                    return b.detach();
                  }))
              .fill(Fill::none())
              .stroke(PathFormat{
                  .width = 0.8f,
                  .strokeFill = Fill::color(hexColor(0xc7ab74, 0.15f))}));
  // the column rays light in sequence, and each drags a leader out to its
  // name
  for (int c = 0; c < 7; ++c) {
    const float delay = tBirds * 1000 + 1900 + (float)c * 90;
    const SkPoint a0 = fanPt(0, c, fanDR * 0.55f);
    const SkPoint a1 = fanPt(6, c, -fanDR * 0.55f);
    const SkPoint nameAt{452.0f, 612.0f + (float)c * 33.0f};
    g.child(box()
                .inset(0)
                .shape(keyedShape(std::tuple{a0.fX, a0.fY, a1.fX, a1.fY},
                                  [a0, a1](SkSize) {
                                    SkPathBuilder b;
                                    b.moveTo(a0);
                                    b.lineTo(a1);
                                    return b.detach();
                                  }))
                .fill(Fill::none())
                .stroke(lines::rails(
                    {{.across = 19.0f,
                      .width = 0.9f,
                      .fill = Fill::color(hexColor(0x62b0dc, 0.60f))},
                     {.across = -19.0f,
                      .width = 0.9f,
                      .fill = Fill::color(hexColor(0x62b0dc, 0.60f))}}))
                .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 360))));
    g.child(
        box()
            .inset(0)
            .shape(keyedShape(std::tuple{a1.fX, a1.fY, nameAt.fX, nameAt.fY},
                              [a1, nameAt](SkSize) {
                                SkPathBuilder b;
                                b.moveTo(a1);
                                b.quadTo(
                                    {(a1.fX + nameAt.fX) * 0.5f, a1.fY - 6.0f},
                                    {nameAt.fX - 8.0f, nameAt.fY + 12.0f});
                                return b.detach();
                              }))
            .fill(Fill::none())
            .stroke(spans::upTo(
                        animate(from(0.0f).to(1.0f), ramp(delay + 120, 420))),
                    lines::Line{.width = 0.9f,
                                .fill = Fill::color(hexColor(0x2f6f9c, 0.55f)),
                                .endCap = lines::Cap::Dot,
                                .capSize = 4.0f})
            .opacity(animate(from(0.0f).to(1.0f), ramp(delay + 120, 300))));
    g.child(text(toU8(kArchangels[c]), type(faceQuill, 21, hexColor(0xd8c08a)))
                .at({nameAt.fX, nameAt.fY})
                .opacity(animate(from(0.0f).to(1.0f), ramp(delay + 220, 360))));
  }
  // the 49 letters, one per (row, column) slot on the fan
  for (int r = 0; r < 7; ++r)
    for (int c = 0; c < 7; ++c) {
      const float delay = tBirds * 1000 + (float)r * 260 + (float)c * 34;
      const bool isCross = kAngles[r][c] == std::string("\xe2\x80\xa0");
      const SkPoint at = fanPt(r, c, 0.0f);
      g.child(text(toU8(kAngles[r][c]),
                   type(faceSeal, 23, isCross ? kRubric : kVellum))
                  .width(30)
                  .height(30)
                  .centerAt(at)
                  .rotate(fanAngle(c))
                  .textAlign(weaveNs::TextAlignment::kCenter)
                  .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 300))));
    }
  g.child(text(toU8("48 letters, and one is noted by a Cross: which maketh "
                    "the 49th."),
               type(faceItalic, 15, hexColor(0x8d7a58)))
              .at({0, 840})
              .opacity(animate(from(0.0f).to(1.0f),
                               ramp(tBirds * 1000 + 2600, 400))));

  // the four orders and their tablets
  const char* kLegend[4] = {
      "Fili\xc3\xa6 Lucis \xc2\xb7 blue tablet in the forehead",
      "Filii Lucis \xc2\xb7 round gold tablet on the breast",
      "Fili\xc3\xa6 Filiarum \xc2\xb7 four-square white ivory",
      "Filii Filiorum \xc2\xb7 three-cornered green"};
  const SkColor4f kLegendTint[4] = {
      hexColor(0xb9c6da, 0.95f), hexColor(0xe6bf63, 0.95f),
      hexColor(0xf7f1e2, 0.95f), hexColor(0x9dbfa2, 0.95f)};
  g.child(text(toU8("THE FOUR ORDERS OF THE CHILDREN OF LIGHT"),
               type(faceMono, 15, kRubric, 1.6f))
              .at({0, 870}));
  for (int i = 0; i < 4; ++i) {
    Element swatch = box()
                         .rect(SkRect::MakeXYWH(2, 898 + (float)i * 26, 16, 16))
                         .fill(Fill::color(kLegendTint[i]));
    if (i == 1)
      swatch.shape(shapes::circle());
    else if (i == 3)
      swatch.shape(shapes::polygon(3));
    else if (i == 0)
      swatch.shape(shapes::sector(-100.0f, 200.0f, 0.55f));
    g.child(std::move(swatch));
    g.child(text(toU8(kLegend[i]), type(faceSerif, 15, hexColor(0x9d8a66)))
                .at({28, 896 + (float)i * 26}));
  }
  return g;
}

auto SigillumAemeth::logStyle() -> feed::TextOptions {
  // 10.2, and the ceiling is arithmetic. The panel is 690 wide, less 24 of
  // padding, less a 14 gap either side of the 1 px divider: 318.5 px to a
  // column. This mono advances 0.62 em, so 11 pt would fit 46 characters
  // and the 47th onward would be cut with no ellipsis and no warning —
  // feed rows simply lose their tails. The longest line printed below
  // is 50 characters, so the size has to be at most 11 x 47/50.
  constexpr float kMono = 10.2f;
  feed::TextOptions s;
  s.styles = kit::tinted(faceMono, kMono, hexColor(0x9d8a66),
                         {{"dim", hexColor(0x6b5c44)},
                          {"heading", kRubric},
                          {"pass", hexColor(0x59b98a)},
                          {"number", hexColor(0x62b0dc)}});
  s.window.gap = 1.0f;
  s.window.visible = 16;
  return s;
}

auto SigillumAemeth::consolePanel() -> Element {
  const float px = 1660 * kS, py = 1058 * kS, pw = 690, ph = 468;
  // Four feeds, two per column: the kit's console is `plate` over `feed`
  // with the voice threaded through once, which is what this panel spelled
  // out by hand.
  return kit::console(
             {.feeds = {&logA, &logC, &logB, &logD},
              .style = logStyle(),
              .stacked = 2,
              .stackGap = 6,
              .plate = {.paddingX = 12,
                        .paddingY = 8,
                        .gap = 14,
                        .fill = Fill::color(hexColor(0x1b1e26, 0.86f)),
                        .border = Fill::color(hexColor(0xc7ab74, 0.22f)),
                        .divider = Fill::color(hexColor(0xc7ab74, 0.16f))}})
      .rect(SkRect::MakeXYWH(px, py, pw, ph))
      .scale(kS)
      .transformOrigin(0.0f, 0.0f);
}

auto SigillumAemeth::colophon() -> Element {
  auto g = box()
               .rect(SkRect::MakeXYWH(1660 * kS, 1552 * kS, 690, 120))
               .scale(kS)
               .transformOrigin(0.0f, 0.0f);
  g.child(box()
              .rect(SkRect::MakeXYWH(0, 0, 690, 2))
              .shape(keyedShape(std::string_view("colophon-rule"),
                                [](SkSize) {
                                  SkPathBuilder b;
                                  b.moveTo(0, 1);
                                  b.lineTo(690, 1);
                                  return b.detach();
                                }))
              .fill(Fill::none())
              .stroke(lines::rails(
                  {{.across = 0.0f,
                    .width = 1.8f,
                    .fill = Fill::color(hexColor(0xc7ab74, 0.55f))},
                   {.across = -4.0f,
                    .width = 0.7f,
                    .fill = Fill::color(hexColor(0xc7ab74, 0.30f)),
                    .dash = {1.6f, 4.4f}}})));
  g.child(
      text(toU8("\xe2\x80\x9cThis is the Seale, whose Name is \xc3\x86meth: "
                "and it is to be made of perfect wax.\xe2\x80\x9d"),
           type(faceItalic, 17, hexColor(0xb59a6c)))
          .left(0)
          .top(16)
          .width(690));
  g.child(text(toU8("Uriel, 14 March 1582 \xc2\xb7 reconstruction from the "
                    "rule, not a tracing \xc2\xb7 SigilCompose study"),
               type(faceMono, 12, hexColor(0x6f5f45)))
              .at({0, 62}));
  return g;
}
