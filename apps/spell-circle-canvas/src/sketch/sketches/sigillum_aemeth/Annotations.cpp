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
    g.children(
        {box()
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
             .key("hops" + std::to_string(n))});
    g.children({box()
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
                                       .strokeFill = Fill::color(
                                           hexColor(0x59b6e8, 0.92f))})
                    .opacity(fade(0))
                    .key("lands" + std::to_string(n))});
  }
  return g;
}

auto SigillumAemeth::nameRows() -> Element {
  const float w = 690;
  // the seven rows sit between ruled lines, as on a prepared sheet: the rules
  // are the block's own outline, so they cost one node and no placement
  return box()
      .width(w)
      .height(324)
      .column()
      .padding(0, 8)
      .shape(keyedShape(w,
                        [w] {
                          SkPathBuilder b;
                          for (int n = 0; n <= 7; ++n) {
                            b.moveTo(0, 4 + (float)n * 46);
                            b.lineTo(w, 4 + (float)n * 46);
                          }
                          return b.detach();
                        }))
      .fill(Fill::none())
      .stroke(PathFormat{.width = 0.8f,
                         .strokeFill = Fill::color(hexColor(0xc7ab74, 0.14f))})
      .children(each(std::views::iota(0, 7), [this](int n) {
        const Solved& s = solved[(size_t)n];
        const float at = (tSolve + (float)n * tSolveEach) * 1000;
        std::string chain;
        for (size_t i = 0; i < s.cells.size(); ++i)
          chain += (i ? "·" : "") + std::to_string(s.cells[i]);
        const auto lit = [](float delay) {
          return animate(from(0.0f).to(1.0f), ramp(delay, 420));
        };
        return box()
            .row()
            .height(46)
            .alignItems(Align::Baseline)
            .children({text(std::to_string(n + 1) + ".")
                           .styleClass("index")
                           .width(34)
                           .opacity(lit(at)),
                       text(kNames[(size_t)n].name)
                           .styleClass("name")
                           .width(178)
                           .opacity(lit(at + 120)),
                       text(s.raw == s.reduced ? "" : "⟨" + s.raw + "⟩")
                           .styleClass("raw")
                           .width(108)
                           .opacity(lit(at + 240)),
                       text(chain).styleClass("chain").grow(1).opacity(
                           lit(at + 60))});
      }));
}

auto SigillumAemeth::basketFan() -> Element {
  const float w = 690, h = 260;
  // the fan's hub stands BELOW its own box: seven nested arcs opening upward,
  // one per row of the angles, so a column is a ray and reading down is
  // reading outward
  const float fanCx = 250.0f, fanCy = 468.0f;
  const float fanR0 = 420.0f, fanDR = 33.0f, fanSpan = 46.0f;
  const auto fanAngle = [fanSpan](int col) {
    return -fanSpan * 0.5f + fanSpan * ((float)col + 0.5f) / 7.0f;
  };
  const auto fanPt = [fanCx, fanCy, fanR0, fanDR, fanAngle](int row, int col,
                                                            float dr) {
    const float rr = fanR0 - (float)row * fanDR + dr;
    return arrange::onEllipse({fanCx, fanCy}, {rr, rr},
                              fanAngle(col) * kD - 1.5707963f);
  };
  const auto lit = [](float delay, float ms) {
    return animate(from(0.0f).to(1.0f), ramp(delay, ms));
  };
  // the column rays light in sequence, and each drags a leader out to its name
  std::vector<Element> rays;
  for (int c = 0; c < 7; ++c) {
    const float delay = tBirds * 1000 + 1900 + (float)c * 90;
    const SkPoint a0 = fanPt(0, c, fanDR * 0.55f);
    const SkPoint a1 = fanPt(6, c, -fanDR * 0.55f);
    const SkPoint nameAt{452.0f, 52.0f + (float)c * 33.0f};
    rays.push_back(box()
                       .cover()
                       .shape(keyedShape(std::tuple{a0.fX, a0.fY, a1.fX, a1.fY},
                                         [a0, a1] {
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
                       .opacity(lit(delay, 360)));
    rays.push_back(
        box()
            .cover()
            .shape(keyedShape(std::tuple{a1.fX, a1.fY, nameAt.fX, nameAt.fY},
                              [a1, nameAt] {
                                SkPathBuilder b;
                                b.moveTo(a1);
                                b.quadTo(
                                    {(a1.fX + nameAt.fX) * 0.5f, a1.fY - 6.0f},
                                    {nameAt.fX - 8.0f, nameAt.fY + 12.0f});
                                return b.detach();
                              }))
            .fill(Fill::none())
            .stroke(spans::upTo(lit(delay + 120, 420)),
                    lines::Line{.width = 0.9f,
                                .fill = Fill::color(hexColor(0x2f6f9c, 0.55f)),
                                .endCap = lines::Cap::Dot,
                                .capSize = 4.0f})
            .opacity(lit(delay + 120, 300)));
    rays.push_back(text(kArchangels[c])
                       .styleClass("archangel")
                       .at(nameAt)
                       .opacity(lit(delay + 220, 360)));
  }
  // the 49 letters, one per (row, column) slot on the fan
  for (int r = 0; r < 7; ++r)
    for (int c = 0; c < 7; ++c)
      rays.push_back(
          text(kAngles[r][c])
              .styleClass(kAngles[r][c] == std::string("†") ? "fan rubric"
                                                            : "fan")
              .rect(path::centred(fanPt(r, c, 0.0f), {30, 30}))
              .rotate(fanAngle(c))
              .block({.alignment = weaveNs::TextAlignment::kCenter})
              .opacity(
                  lit(tBirds * 1000 + (float)r * 260 + (float)c * 34, 300)));
  return box()
      .width(w)
      .height(h)
      .shape(keyedShape(
          std::tuple{fanCx, fanCy, fanR0, fanDR, fanSpan},
          [fanCx, fanCy, fanR0, fanDR, fanSpan] {
            SkPathBuilder b;
            for (int r = 0; r <= 7; ++r) {
              const float rr = fanR0 - (float)r * fanDR + fanDR * 0.5f;
              for (int i = 0; i <= 24; ++i) {
                const float a =
                    (-fanSpan * 0.54f + fanSpan * 1.08f * (float)i / 24.0f) *
                    kD;
                const SkPoint q = arrange::onEllipse({fanCx, fanCy}, {rr, rr},
                                                     a - 1.5707963f);
                i == 0 ? b.moveTo(q) : b.lineTo(q);
              }
            }
            return b.detach();
          }))
      .fill(Fill::none())
      .stroke(PathFormat{.width = 0.8f,
                         .strokeFill = Fill::color(hexColor(0xc7ab74, 0.15f))})
      .children(rays);
}

auto SigillumAemeth::margin() -> Element {
  const float w = 690;
  // THE PANEL IS ONE COLUMN, so every block stands where the one above it
  // leaves off and no line names a y. Its voice is the terminal face at the
  // body size in the note ink; a heading is the rubric, tracked; every other
  // line says only what differs, by class.
  const std::vector<sketch::kit::Document::Line> orders = doc.run("orders");
  const SkColor4f kTablet[4] = {
      hexColor(0xb9c6da, 0.95f), hexColor(0xe6bf63, 0.95f),
      hexColor(0xf7f1e2, 0.95f), hexColor(0x9dbfa2, 0.95f)};
  // the tablet each order wears: an arc-segment in the forehead, a round gold
  // plate on the breast, a FOUR-SQUARE white ivory — the box's own shape, so
  // it names none — and a three-cornered green
  const std::optional<Shape> kTabletShape[4] = {
      shapes::sector(-100.0f, 200.0f, 0.55f), shapes::circle(), std::nullopt,
      shapes::polygon(3)};
  const auto tablet = [&kTablet, &kTabletShape](size_t i) {
    Element mark = box().width(16).height(16).fill(Fill::color(kTablet[i]));
    if (kTabletShape[i]) mark.shape(*kTabletShape[i]);
    return mark;
  };
  const auto lit = [](float delay) {
    return animate(from(0.0f).to(1.0f), ramp(delay, 500));
  };
  return box()
      .rect(SkRect::MakeXYWH(1660 * kS, 56 * kS, w, 1588))
      .scale(kS)
      .transformOrigin(0.0f, 0.0f)
      .column()
      .gap(10)
      .font({.face = faceMono, .size = 15})
      .ink(hexColor(0x8d7a58))
      .styleSheet(voices())
      .children(
          {box().column().gap(4).children(
               {text(doc.phrase("title")).styleClass("title"),
                text(doc.phrase("subtitle")).styleClass("subtitle"),
                text(doc.phrase("provenance")).styleClass("serif")}),
           // the double rule under the masthead: heavy, with a dotted
           // companion held off it
           kit::line({.length = Dimension(w),
                      .thickness = 2.4f,
                      .fill = Fill::color(hexColor(0xc7ab74, 0.75f)),
                      .pair = {{.thickness = 0.8f,
                                .gap = 3.4f,
                                .fill = Fill::color(hexColor(0xc7ab74, 0.40f)),
                                .dash = {2.0f, 5.0f}}}}),
           text(doc.phrase("namesHeading")).styleClass("heading"), nameRows(),
           // the leftovers
           box().column().gap(4).children(
               {text(doc.phrase("consumed")).opacity(lit(tDark * 1000)),
                text(doc.phrase("unvisited"))
                    .styleClass("rubric")
                    .opacity(lit(tDark * 1000 + 200)),
                text(doc.phrase("leftovers"))
                    .styleClass("gloss")
                    .opacity(lit(tDark * 1000 + 400))}),
           text(doc.phrase("basketsHeading")).styleClass("heading"),
           basketFan(),
           text(doc.phrase("crossNote"))
               .styleClass("italic")
               .opacity(lit(tBirds * 1000 + 2600)),
           text(doc.phrase("ordersHeading")).styleClass("heading"),
           // the four orders, each with the tablet the record gives it: an
           // arc-segment worn in the forehead, a round gold plate on the
           // breast, a four-square white ivory, a three-cornered green
           box().column().gap(10).children(each(
               orders,
               [&tablet](const sketch::kit::Document::Line& line, size_t i) {
                 return box()
                     .row()
                     .gap(10)
                     .alignItems(Align::Center)
                     .children(
                         {tablet(i), text(line.words).styleClass("legend")});
               }))});
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
  return box()
      .rect(SkRect::MakeXYWH(1660 * kS, 1552 * kS, 690, 120))
      .scale(kS)
      .transformOrigin(0.0f, 0.0f)
      .column()
      .gap(16)
      .styleSheet(voices())
      .children(
          {kit::line({.length = Dimension(690),
                      .thickness = 1.8f,
                      .fill = Fill::color(hexColor(0xc7ab74, 0.55f)),
                      .pair = {{.thickness = 0.7f,
                                .gap = 2.7f,
                                .fill = Fill::color(hexColor(0xc7ab74, 0.30f)),
                                .dash = {1.6f, 4.4f}}}}),
           text(doc.phrase("seal"))
               .font({.face = faceItalic,
                      .size = 17,
                      .color = hexColor(0xb59a6c)})
               .width(690),
           text(doc.phrase("imprint"))
               .font({.face = faceMono,
                      .size = 12,
                      .color = hexColor(0x6f5f45)})});
}
