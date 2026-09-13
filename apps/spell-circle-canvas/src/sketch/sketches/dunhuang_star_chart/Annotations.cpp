#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilsketch/kit/Chart.h>

#include "DunhuangStarChart.h"

namespace {

/** The hairline this plate rules its panels off with, in its own ink. */
Element hairline(float alpha) {
  return kit::line(
      {.thickness = 0.8f, .fill = Fill::color(hexColor(0x8a7458, alpha))});
}

}  // namespace

auto DunhuangStarChart::locator() -> Element {
  const float lw = 1740.0f, lh = lw * kWideMm / kScrollMm;
  const float mm = lw / kScrollMm;
  const float atlasRight = lw - kScrollMm * mm + kAtlasMm * mm;
  const Element rule = box().stroke(PathFormat{
      .width = 0.8f, .strokeFill = Fill::color(hexColor(0x2a2118, 0.9f))});
  // the two windows this plate actually shows, in scroll mm
  struct Win {
    float s0, s1;
  };
  const Win wins[2] = {
      {(kOriginL - kBreakL) / kPxMm, (kOriginL + 90) / kPxMm},
      {(kOriginR - kW - 90) / kPxMm, (kOriginR - kBreakR) / kPxMm}};
  return box()
      .rect(SkRect::MakeXYWH(720, 92, lw, lh))
      .rotate(0.32f)
      .key("locator")
      .opacity(gate(0.3f, 1.2f))
      .children(
          {box()
               .absolute()
               .inset(0)
               .fill(Paint::linear({0, 0}, {0, lh},
                                   {{0.0f, hexColor(0xa2865c)},
                                    {0.5f, hexColor(0xd6bf95)},
                                    {1.0f, hexColor(0xa2865c)}}))
               .stroke(PathFormat{
                   .width = 0.9f,
                   .strokeFill = Fill::color(hexColor(0x2a2118, 0.75f))}),
           // the 26 clouds and the 80 columns of the divination section, at
           // the RIGHT end of the roll
           each(std::views::iota(0, 26),
                [lw](int c) {
                  return box()
                      .rect(SkRect::MakeXYWH(lw - 31.0f - (float)c * 24.0f, 6,
                                             14, 9))
                      .shape(shapes::blob((uint32_t)(700 + c), 0.34f, 6))
                      .fill(Fill::color(hexColor(0x33291c, 0.85f)));
                }),
           each(std::views::iota(0, 80),
                [lw, lh](int c) {
                  return box()
                      .rect(SkRect::MakeXYWH(lw - 20.0f - (float)c * 7.6f, 19,
                                             1.1f, lh - 25))
                      .fill(Fill::color(hexColor(0x33291c, 0.55f)));
                }),
           // the 13 maps of the atlas at the LEFT
           each(std::views::iota(1, 13),
                [rule, atlasRight, lh, mm](int k) {
                  return Element(rule).rect(SkRect::MakeXYWH(
                      atlasRight - (mapSlotS(k) + kMapWmm) * mm, lh * 0.16f,
                      kMapWmm * mm, lh * 0.68f));
                }),
           Element(rule)
               .rect(path::centred({atlasRight - discCentreS() * mm, lh * 0.5f},
                                   {lh * 0.68f, lh * 0.68f}))
               .shape(shapes::circle()),
           // the plate's canvas reaches ~227 mm past the atlas's own left end,
           // so window 0 runs off the strip; each is clamped to the scroll it
           // annotates
           each(wins,
                [lw, lh, atlasRight, mm](const Win& wn) {
                  const float a = std::max(0.0f, atlasRight - wn.s1 * mm);
                  const float b = std::min(lw, atlasRight - wn.s0 * mm);
                  return box()
                      .rect(SkRect::MakeXYWH(a, -4, b - a, lh + 8))
                      .fill(Fill::color(hexColor(0x2f6d86, 0.30f)))
                      .stroke(spans::corners(9.0f),
                              brush::solid(1.4f, Fill::color(kTrace)));
                }),
           text(phrase("locator"))
               .styleClass("caption dim")
               .at({2, lh + 5})
               .width(1700)});
}

auto DunhuangStarChart::poleDrift() -> Element {
  // WHERE THE POLE WAS, AS A WHEEL: the angle is right ascension and the
  // radius is polar distance, so nothing below turns a degree into a pixel.
  // The paper dates the chart partly from the pole — "the measured shift in
  // polar distance of the pole reference point between the S.3326 map and the
  // sky at date +700 is only marginally significant with a difference of
  // (3.9±2.9)°" — and this wheel draws that track off the same IAU 1976
  // matrix the 1,460 stars ride.
  const sketch::kit::Plot wheel{.x = {.domain = {0, 360}},
                                .y = {.domain = {0, 30}},
                                .pad = 12,
                                .polar = sketch::kit::Polar{.sweep = {0, 360}}};
  // The pole of J2000 in an epoch's OWN coordinates, which is the circle the
  // pole is drawn walking round.
  const auto pole = [](float epoch) {
    return precession((epoch - 2000.0f) * 0.01f)({.lonDeg = 0, .latDeg = 90});
  };
  // TWO RUNS OF THAT WALK, each a layer of this study's own reading the frame
  // for its points: the score's own 1,500 years, drawn on as the precession
  // runs, and the whole 26,000-year circle faint behind it.
  const auto walk = [pole](const sketch::kit::Plot& f, int from, int to,
                           int step) {
    return keyedShape(
        std::tuple(from, to, step), [pole, f, from, to, step](SkSize s) {
          SkPathBuilder b;
          for (int e = from; step < 0 ? e >= to : e <= to; e += step) {
            const path::Spherical p = pole((float)e);
            if (90.0f - p.latDeg > 30.0f) continue;
            const SkPoint q = f.at(p.lonDeg, 90.0f - (double)p.latDeg, s);
            b.countPoints() == 0 ? b.moveTo(q) : b.lineTo(q);
          }
          return b.detach();
        });
  };
  // alp UMi is 0.74° from the J2000 pole, so its dot lands ON the hub marker
  // and an up-right label would land on "J2000 pole". The three captions are
  // ranged away from their dots instead, each on the side that is clear.
  struct Ref {
    float ra, dec;
    Align across, down;
  };
  const Ref refs[3] = {{37.9529f, 89.2641f, Align::Start, Align::Start},
                       {222.6764f, 74.1555f, Align::Start, Align::End},
                       {211.0973f, 64.3758f, Align::End, Align::End}};
  const std::vector<Line> names = lines("poleMarks");
  const auto polarDistance = [](const Ref& r) { return 90.0 - (double)r.dec; };
  const path::Spherical at700 = pole(700.0f);
  const std::array<sketch::kit::Datum, 1> hub{{{0, 0}}};
  const std::array<sketch::kit::Datum, 1> mark700{
      {{at700.lonDeg, 90.0 - (double)at700.latDeg}}};
  std::vector<sketch::kit::Layer> layers{
      sketch::kit::rules({.y = {10, 20, 30}, .width = 0.5f}),
      [walk](const sketch::kit::Plot& f) {
        return box()
            .absolute()
            .inset(0)
            .styleClass("ghost")
            .shape(walk(f, -24000, 4000, 250))
            .stroke(PathFormat{.width = 0.7f,
                               .strokeFill = Fill::currentInk(),
                               .dashIntervals = {3, 4}});
      },
      [this, walk](const sketch::kit::Plot& f) {
        return box()
            .absolute()
            .inset(0)
            .styleClass("plotTrace")
            .shape(walk(f, 2000, 500, -25))
            .stroke(spans::upTo(gate(tPrec0, tPrec1)),
                    lines::Line{.width = 2.0f, .fill = Fill::currentInk()});
      },
      sketch::kit::marks(refs,
                         [](const Ref&) {
                           return box()
                               .width(6)
                               .height(6)
                               .shape(shapes::circle())
                               .fill(Fill::currentInk());
                         },
                         {.x = &Ref::ra, .y = polarDistance}),
      sketch::kit::marks(
          mark700,
          [this](const sketch::kit::Datum&) {
            return box()
                .width(12)
                .height(12)
                .shape(shapes::star(4, 0.30f))
                .fill(Fill::color(kTrace))
                .opacity(gate(tPrec1 - 0.4f, tPrec1 + 0.3f));
          },
          {.x = &sketch::kit::Datum::x, .y = &sketch::kit::Datum::y}),
      sketch::kit::marks(
          hub,
          [](const sketch::kit::Datum&) {
            return box()
                .width(6)
                .height(6)
                .shape(shapes::circle())
                .stroke(stroke(0.9f, Fill::color(hexColor(0xe0cfa6, 0.8f))));
          },
          {.x = &sketch::kit::Datum::x, .y = &sketch::kit::Datum::y}),
      sketch::kit::label(names.size() > 3 ? names[3].words : "", 0, 0,
                         {.anchor = {Align::End, Align::Start}})};
  for (size_t i = 0; i < 3 && i < names.size(); ++i)
    layers.push_back(
        sketch::kit::label(names[i].words, refs[i].ra, polarDistance(refs[i]),
                           {.anchor = {refs[i].across, refs[i].down}}));
  return sketch::kit::plot("pole", wheel, std::move(layers))
      .rect(SkRect::MakeXYWH(150, 234, 132, 132))
      .opacity(gate(tPrec0 - 0.8f, tPrec0 + 0.2f));
}

auto DunhuangStarChart::poleText() -> Element {
  const std::vector<Line> epochs = lines("poleEpochs");
  const float bw = 430.0f;
  return box()
      .at({300, 228})
      .width(452)
      .column()
      .gap(6)
      .key("poletext")
      .opacity(gate(tPrec0 - 0.6f, tPrec0 + 0.4f))
      .children(
          {text(phrase("poleTitle"))
               .styleClass("heading")
               .font({.size = 12.0f}),
           noteStack("pole"),
           // THE EPOCH, RUNNING. One Output remapped three ways: it turns the
           // star field's rotation matrix, walks the pole's track above, and
           // slides this marker — bind() doing the unit conversion at each
           // call site instead of three Outputs in the tick loop.
           box().height(30).children(
               {box()
                    .rect(SkRect::MakeXYWH(0, 0, bw, 9))
                    .shape(keyedShape(
                        std::string_view("ruler-scale"),
                        [](SkSize sz) {
                          SkPathBuilder b;
                          b.moveTo(0, 0);
                          b.lineTo(0, sz.height());
                          b.moveTo(0, sz.height() * 0.5f);
                          b.lineTo(sz.width(), sz.height() * 0.5f);
                          b.moveTo(sz.width(), 0);
                          b.lineTo(sz.width(), sz.height());
                          for (int c = 1; c < 13; ++c) {
                            const float x = sz.width() * (float)c / 13.0f;
                            b.moveTo(x, sz.height() * 0.5f - 2.5f);
                            b.lineTo(x, sz.height() * 0.5f + 2.5f);
                          }
                          return b.detach();
                        }))
                    .stroke(lines::Line{
                        .width = 0.9f,
                        .fill = Fill::color(hexColor(0x9a8a68, 0.8f))}),
                box()
                    .rect(SkRect::MakeXYWH(-4, -5, 8, 19))
                    .shape(shapes::polygon(3, 180.0f))
                    .fill(Fill::color(kCinnabar))
                    .translateX(
                        settled ? Animatable<float>(0.0f)
                                : Animatable<float>(bind(&scribe)
                                                        .window(tPrec0, tPrec1)
                                                        .invert()
                                                        .target(0.0f, bw))),
                text(epochs.empty() ? "" : epochs[0].words)
                    .styleClass("caption")
                    .at({0, 12}),
                text(epochs.size() > 1 ? epochs[1].words : "")
                    .styleClass("caption")
                    .at({bw - 40, 12})
                    .width(40)
                    .block({.alignment = weave::TextAlignment::kEnd})}),
           text(phrase("poleSweep")).styleClass("caption gold")});
}

auto DunhuangStarChart::logStyle() -> feed::TextOptions {
  feed::TextOptions s;
  // One voice, and the levels are CLASSES over it: a colour each, the
  // face and the size the base's.
  s.styles
      .base(weave::textStyle(
          {.face = faceMono, .size = 9.2f, .color = hexColor(0x9a8a68)}))
      .set("dim", weave::Type{.color = hexColor(0x6d6249)})
      .set("heading", weave::Type{.color = hexColor(0xc9a35c)})
      .set("pass", weave::Type{.color = hexColor(0x6ba87e)})
      .set("number", weave::Type{.color = hexColor(0xcf6a4a)})
      .set("fail", weave::Type{.color = hexColor(0xc4483a)});
  s.window.gap = 1.0f;
  s.window.visible = 12;
  return s;
}

auto DunhuangStarChart::projectionPanel() -> Element {
  // THE TWO DEPARTURE CURVES. Each is how far one projection's ordinate
  // parts from its own best-fit straight line, in DEGREES of declination,
  // self-normalised so both panels read at one height — and the chart's own
  // published residual is ruled across the same field, to the same scale, so
  // the drawn curve and the number under it come off one set of sums.
  struct Curve {
    float lo, hi;
    bool mercator;
  };
  const Curve curves[2] = {{-27, 43, true}, {0, 38, false}};
  const auto field = [this](const Curve& c, size_t i) {
    const std::vector<float> xs = abscissa(c.lo, c.hi, 80);
    const std::vector<float> ys = ordinate(c.lo, c.hi, c.mercator, 80);
    const measure::LineFit<float> fit = measure::lineFit<float>(xs, ys);
    float mx = 1e-9f;
    for (size_t k = 0; k < xs.size(); ++k)
      mx = std::max(mx, std::abs(fit.residual(xs[k], ys[k]) / fit.slope));
    const bool merc = c.mercator;
    // the chart's own published residual, in the same normalised units the
    // curve is drawn in, clamped to the field it has to fit inside
    const float hand = std::min(
        1.25f, (merc ? 1.61f : 3.29f) / (merc ? depMerc : depStereo).maxDeg);
    const std::string caption =
        doc ? std::string((*doc)["projectionPlots"][i]["caption"].text())
            : std::string();
    const std::string note =
        doc ? std::string((*doc)["projectionPlots"][i]["note"].text())
            : std::string();
    return box().width(320).column().gap(4).children(
        {sketch::kit::plot(
             i ? "dep1" : "dep0",
             {.x = {.domain = {c.lo, c.hi}}, .y = {.domain = {-1.25, 1.25}}},
             {sketch::kit::rules(
                  {.y = {-hand, hand}, .width = 0.6f, .styleClass = "hand"}),
              sketch::kit::trace(
                  [fit, mx, merc](double v) {
                    const float y =
                        (merc ? kMercator : kStereographic).radiusAt((float)v);
                    return (double)(fit.residual((float)v, y) / fit.slope / mx);
                  },
                  {.width = 1.5f, .samples = 80})})
             .height(132)
             .shrink(0)
             .stroke(spans::edges(16.0f),
                     brush::solid(0.9f, Fill::color(hexColor(0x8a7458, 0.5f)))),
         text(caption).styleClass("caption"),
         text(note).styleClass("caption dim")});
  };
  return box()
      .at({96, 1046})
      .width(700)
      .column()
      .gap(12)
      .key("proj")
      .opacity(gate(tProj, tProj + 0.9f))
      .children({text(phrase("projectionTitle"))
                     .styleClass("heading")
                     .font({.size = 13.0f, .track = 1.1f}),
                 box().row().gap(46).children(each(curves, field)),
                 noteStack("projection").font({.size = 9.6f})});
}

auto DunhuangStarChart::auditRow(int i) -> Element {
  const M5Row& r = conc.five(i);
  const float t = tAudit + (float)i * tAuditEach;
  int cz = 0;
  for (int a = 0; a < nAst; ++a)
    if (cat.ast(a).id == r.cid) cz = astUnique(cat, cat.ast(a));
  return box()
      .row()
      .gap(6)
      .alignItems(Align::Center)
      .opacity(gate(t, t + 0.35f))
      .children(
          {text(kit::formatted("%3d", i + 1)).styleClass("dim").width(30),
           text(r.pinyin).styleClass("chalk").width(126),
           box()
               .row()
               .gap(6)
               .width(86)
               .alignItems(Align::Center)
               .children({box()
                              .width(8)
                              .height(8)
                              .shape(shapes::circle())
                              .fill(Fill::color(schoolInk(r.school)))
                              .stroke(stroke(0.8f, Fill::color(kInk))),
                          text(r.native).styleClass(schoolClass(r.school))}),
           text(kit::formatted("%4d %4d %4d", r.sxc, r.map, cz))
               .styleClass(r.sxc == r.map ? "note" : "number")
               .width(100),
           // the confidence index, as five cells
           box().row().gap(1.8f).width(44).children(each(
               std::views::iota(0, 5),
               [&r](int c) {
                 return box().width(5.2f).height(7.0f).fill(
                     Fill::color(c < r.confidence ? hexColor(0xc9a35c, 0.85f)
                                                  : hexColor(0x6d6249, 0.28f)));
               })),
           text(r.defect).styleClass("flag").grow(1)});
}

auto DunhuangStarChart::auditPanel() -> Element {
  // described into its own slot, so the voices stand on this panel.
  // ONE LIST OF WIDTHS the heads and the cells share: a hand-spaced
  // monospace string cannot land on these columns, because the numeric block
  // is a third size and the CJK pair in the middle is double-advance.
  static constexpr float kHeads[6] = {30, 126, 86, 100, 44, 0};
  const std::vector<Line> heads = lines("auditHeads");
  return box()
      .at({840, 1046})
      .width(880)
      .column()
      .gap(5)
      .key("audit")
      .opacity(gate(tAudit - 0.9f, tAudit - 0.2f))
      .styleSheet(voices())
      .children(
          {text(phrase("auditTitle"))
               .styleClass("heading")
               .font({.size = 13.0f}),
           text(phrase("auditLead")).styleClass("caption"),
           box().row().gap(6).children(
               each(heads,
                    [](const Line& h, size_t i) {
                      Element cell = text(h.words).styleClass("caption dim");
                      return i + 1 < std::size(kHeads) ? cell.width(kHeads[i])
                                                       : cell.grow(1);
                    })),
           each(std::views::iota(0, 20), [this](int i) { return auditRow(i); }),
           hairline(0.5f).opacity(gate(tAudit + 5.4f, tAudit + 5.9f)),
           noteStack("auditFoot")
               .font({.size = 9.4f})
               .opacity(gate(tAudit + 5.4f, tAudit + 5.9f))});
}

auto DunhuangStarChart::map13Panel() -> Element {
  // THE DISC'S OWN ERRATA. Table 5 is 34 asterisms and 142 stars — and its
  // n(map) column sums to 141. Everything here is quoted, nothing resolved.
  return box()
      .at({96, 1362})
      .width(700)
      .column()
      .gap(6)
      .key("m13")
      .opacity(gate(tAudit + 4.6f, tAudit + 5.4f))
      .children({text(phrase("map13Title"))
                     .styleClass("heading")
                     .font({.size = 12.0f}),
                 noteStack("map13").font({.size = 9.2f})});
}

auto DunhuangStarChart::consolePanel() -> Element {
  return box()
      .rect(SkRect::MakeXYWH(1768, 1042, 700, 452))
      .fill(Fill::color(hexColor(0x100e0b, 0.86f)))
      .stroke(stroke(1.0f, Fill::color(hexColor(0x8a7458, 0.24f)),
                     PathFormat::Align::Inner))
      .key("console")
      .column()
      .gap(6)
      .padding(12, 9)
      .children({feed::feed(logA, logStyle()), hairline(0.16f),
                 feed::feed(logB, logStyle()), hairline(0.16f),
                 feed::feed(logC, logStyle())});
}

auto DunhuangStarChart::ruleNote() -> Element {
  // THE THIRD QUESTION, DRAWN. Twelve maps of 48 deg on a 30 deg pitch: the
  // frames butt on the paper but their RA windows OVERLAP by 18 deg, so the
  // scroll's own RA axis jumps BACKWARD at every map boundary.
  return box()
      .at({766, 228})
      .width(770)
      .column()
      .gap(6)
      .key("rulenote")
      .opacity(gate(tFold1 - 0.2f, tFold1 + 0.8f))
      .children({text(phrase("ruleTitle"))
                     .styleClass("heading")
                     .font({.size = 12.0f}),
                 noteStack("rule")});
}

auto DunhuangStarChart::headings() -> Element {
  // the scale bar, in cm of real paper
  const float barMm = 100.0f;
  return box().absolute().inset(0).key("head").children(
      {box().at({96, 16}).width(1500).column().gap(4).children(
           {text(phrase("title"))
                .font({.face = faceDisplay,
                       .size = 27.0f,
                       .color = hexColor(0xe0cfa6),
                       .track = 2.4f}),
            text(phrase("provenance")).font({.size = 10.2f})}),
       box()
           .at({1660, 16})
           .width(880)
           .column()
           .gap(3)
           .children(
               {text(phrase("claim")).styleClass("gold").font({.size = 10.2f}),
                text(phrase("plate")).styleClass("dim").font({.size = 9.4f})}),
       box()
           .at({96, 1544})
           .row()
           .gap(10)
           .alignItems(Align::Center)
           .children({box()
                          .width(barMm * kPxMm)
                          .height(7)
                          .shape(keyedShape(
                              std::string_view("scale-bar"),
                              [](SkSize s) {
                                SkPathBuilder b;
                                b.moveTo(0, 6);
                                b.lineTo(0, 0);
                                b.lineTo(s.width(), 0);
                                b.lineTo(s.width(), 6);
                                for (int i = 1; i < 10; ++i) {
                                  b.moveTo(s.width() * (float)i / 10.0f, 0);
                                  b.lineTo(s.width() * (float)i / 10.0f,
                                           i % 5 ? 3 : 5);
                                }
                                return b.detach();
                              }))
                          .stroke(lines::Line{
                              .width = 1.0f,
                              .fill = Fill::color(hexColor(0x9a8a68, 0.8f))}),
                      text(phrase("scale")).styleClass("caption dim")}),
       text(phrase("sources"))
           .styleClass("caption dim")
           .at({1660, 1544})
           .width(880)});
}
