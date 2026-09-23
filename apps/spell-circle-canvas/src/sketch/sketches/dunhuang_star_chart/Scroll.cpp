#include <sigilgeometry/path/Frame.h>

#include "DunhuangStarChart.h"

namespace {

/** A circle of @p diameter about @p centre, in its parent's coordinates. */
Element ring(SkPoint centre, float diameter) {
  return box()
      .rect(path::centred(centre, {diameter, diameter}))
      .shape(shapes::circle());
}

}  // namespace

auto DunhuangStarChart::ground() -> Element {
  return box()
      .cover()
      .key("ground")
      .fill(Paint::linear({0, 0}, {kW, kH},
                          {{0.0f, hexColor(0x171410)},
                           {0.5f, hexColor(0x1d1913)},
                           {1.0f, hexColor(0x120f0c)}}))
      .cache(Cache::Texture);
}

auto DunhuangStarChart::scrollBand(float x0, float x1, const char* keyName,
                                   float tilt) -> Element {
  const float w = x1 - x0;
  // "replication marks by contact due to long conservation in a rolled
  // state" — the ghost of the adjacent turn, one circumference over, taking
  // the sheet's 244 mm width as the roll's diameter
  const float circ = 244.0f * 3.14159f * kPxMm;  // ~77 cm of scroll
  // ASK FOR THE BAKE BY NAME. The band carries a paper gradient, an
  // anisotropic grain and a speckle wash over its whole area, and it is
  // ROTATED (a scroll does not lie square) — which disqualifies it from
  // the library's automatic device-space promotion, since a bake pinned to
  // one device rect is not matrix-independent. Without this explicit cache
  // the three shaders re-run over every pixel of both bands on every frame,
  // although nothing here ever moves. The rule generalises: a node is
  // refused the automatic bake for a decorative half-degree tilt exactly as
  // it is for animation, so any rotated node with area must be baked by
  // name.
  return box()
      .rect(SkRect::MakeXYWH(x0, kBandTop, w, kBandH))
      .rotate(tilt)
      .key(keyName)
      .cache(Cache::Texture)
      .opacity(gate(tPaper, tPaper + 1.0f))
      .children(
          {// the sheet itself, with the fibre running along the roll
           box()
               .cover()
               .fill(Paint::linear({0, 0}, {0, kBandH},
                                   {{0.00f, kKraft},
                                    {0.055f, kPaperDeep},
                                    {0.30f, kPaperMid},
                                    {0.58f, kPaperLit},
                                    {0.88f, kPaperMid},
                                    {0.945f, kPaperDeep},
                                    {1.00f, kKraft}}))
               .cache(Cache::Texture),
           // the fibre — anisotropic grain, luminance not hue, under a bake
           box()
               .cover()
               .fill(paperGrain)
               .opacity(0.20f)
               .blendMode(SkBlendMode::kSoftLight)
               .cache(Cache::Texture),
           box()
               .cover()
               .foreground(Wash{.material = paperSpeck.material(),
                                .blend = SkBlendMode::kMultiply,
                                .amount = 0.55f})
               .cache(Cache::Texture),
           each(std::views::iota(-3, 4),
                [x0, w, circ](int k) {
                  return box()
                      .rect(SkRect::MakeXYWH(
                          std::fmod(std::abs(x0) + (float)k * circ, w), 6, 3.0f,
                          kBandH - 12))
                      .fill(Fill::color(hexColor(0x8b6e45, 0.10f)));
                }),
           // top and bottom rules — unequal, per lines::Rails
           box()
               .cover()
               .shape(keyedShape(std::string_view("band-rules"),
                                 [](SkSize s) {
                                   SkPathBuilder b;
                                   b.moveTo(0, 1.5f);
                                   b.lineTo(s.width(), 1.5f);
                                   b.moveTo(0, s.height() - 1.5f);
                                   b.lineTo(s.width(), s.height() - 1.5f);
                                   return b.detach();
                                 }))
               .stroke(
                   Brush{}
                       .shaped(shapers::Jitter{.segmentLength = 34,
                                               .deviation = 0.9f,
                                               .seed = 3326})
                       .layer(lines::Rails{
                           .rails = {
                               {.across = 0,
                                .width = 1.9f,
                                .fill = Fill::color(hexColor(0x6b573c, 0.62f))},
                               {.across = -4.5f,
                                .width = 0.55f,
                                .fill = Fill::color(hexColor(0x6b573c, 0.34f)),
                                .dash = {9, 6}}}}))});
}

auto DunhuangStarChart::mapFrame(int k, int seg) -> Element {
  const float s0 = mapSlotS(k);
  const float xl = segX(seg, s0 + kMapWmm), xr = segX(seg, s0);
  if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4) return box();
  const float w = xr - xl;
  if (w <= 2) return box();

  // THE APPARATUS SITS UNDER THE INK. The paper has no frames at all —
  // the twelve maps abut and are separated only by the text columns —
  // so the frame, its corner brackets, the RA ladder and its numerals
  // are this study's own scaffolding, carrying its argument about the
  // 18° jump at every boundary. Scaffolding drawn at the weight of the
  // brush competes with the thing it is scaffolding: every one of them
  // is lighter than the faintest star mark on the map it encloses.
  const float yEq = (mapGcDec(k) + 45.0f) / kDecPerMm * kPxMm;
  std::vector<Element> marks{
      box()
          .cover()
          .stroke(Brush{}
                      .shaped(shapers::Jitter{.segmentLength = 30,
                                              .deviation = 1.1f,
                                              .seed = (uint32_t)(600 + k)})
                      .layer(lines::Line{
                          .width = 1.0f,
                          .fill = Fill::color(hexColor(0x8a7458, 0.42f))}))
          .stroke(spans::corners(15.0f),
                  brush::solid(1.4f, Fill::color(hexColor(0x6b573c, 0.52f)))),
      // the equator — the one line whose position the paper says varies ±5°
      box()
          .rect(SkRect::MakeXYWH(0, yEq - 1, w, 2))
          .shape(keyedShape(std::string_view("equator"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              b.moveTo(0, 1);
                              b.lineTo(s.width(), 1);
                              return b.detach();
                            }))
          .stroke(
              PathFormat{.width = 0.75f,
                         .strokeFill = Fill::color(hexColor(0x8a3020, 0.42f)),
                         .dashIntervals = {5, 5},
                         .trimStart = 0.04f,
                         .trimEnd = 0.96f}),
      // the map's own number and month, in the margin above
      text(kit::formatted("%d", k))
          .styleClass("mapno")
          .at({w - 20, -19})
          .width(18)
          .paragraph({.alignment = weave::TextAlignment::kEnd})};

  // the RA ladder: a tick every 6°, numbered every 12°
  for (int t = -24; t <= 24; t += 6) {
    const float x = w * 0.5f + (float)t / kRaPerMm * kPxMm;
    if (x < 1 || x > w - 1) continue;
    const float reach = (t % 12) == 0 ? 8.0f : 4.5f;
    const Fill ink = Fill::color(hexColor(0x8a7458, 0.40f));
    marks.push_back(
        box().rect(SkRect::MakeXYWH(x - 0.4f, 0, 0.9f, reach)).fill(ink));
    marks.push_back(
        box()
            .rect(SkRect::MakeXYWH(x - 0.4f, kFrameH - reach, 0.9f, reach))
            .fill(ink));
    if ((t % 12) == 0)
      marks.push_back(
          text(kit::formatted(
                   "%d", (int)std::lround(wrap360(mapCentre(k) + (float)t))))
              .styleClass("tick")
              .at({x - 11, kFrameH + 3})
              .width(24)
              .paragraph({.alignment = weave::TextAlignment::kCenter}));
  }

  // THE MANSION BOUNDARIES, ruled where the DETERMINATIVE STARS put them
  // and not at 12.86° apiece — Stellarium's lunar_system.defining_stars,
  // precessed to +700 like everything else. Interrupted rules: they stop
  // short of both edges of the frame.
  for (int m = 0; m < 28; ++m) {
    const float dRa = wrap180(xiuRa[(size_t)m] - mapCentre(k));
    if (std::abs(dRa) > 23.0f) continue;
    const float x = w * 0.5f + dRa / kRaPerMm * kPxMm;
    marks.push_back(box()
                        .rect(SkRect::MakeXYWH(x - 6, 0, 12, kFrameH))
                        .shape(keyedShape(std::string_view("ra-tick"),
                                          [](SkSize sz) {
                                            SkPathBuilder b;
                                            b.moveTo(sz.width() * 0.5f, 0);
                                            b.lineTo(sz.width() * 0.5f,
                                                     sz.height());
                                            return b.detach();
                                          }))
                        .stroke(PathFormat{
                            .width = 0.7f,
                            .strokeFill = Fill::color(hexColor(0x4a3b28, 0.5f)),
                            .dashIntervals = {7, 5},
                            .trimStart = 0.06f,
                            .trimEnd = 0.94f}));
    marks.push_back(
        text(cat.xiu(m).native)
            .styleClass("han")
            .at({x - 9, -32})
            .width(18)
            .paragraph({.alignment = weave::TextAlignment::kCenter}));
    marks.push_back(
        text(cat.xiu(m).pinyin)
            .styleClass("xiu")
            .at({x - 20, -45})
            .width(40)
            .paragraph({.alignment = weave::TextAlignment::kCenter}));
  }

  return box()
      .rect(SkRect::MakeXYWH(xl - segLo(seg), kFrameTop - kSegTop, w, kFrameH))
      .key(kit::formatted("map%d_%d", k, seg))
      .opacity(gate(tFold0 - 0.4f, tFold0 + 1.1f))
      .children(marks);
}

auto DunhuangStarChart::columnBand(int k, int seg) -> Element {
  const float s0 = mapSlotS(k) + kMapWmm;
  const float xl = segX(seg, s0 + kColBandMm), xr = segX(seg, s0);
  if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4) return box();
  const float w = xr - xl;
  if (w <= 2) return box();
  const int nCols = (int)std::round(kColBandMm / kColMm);
  // The columns are drawn as MARKS, not as characters: a Tang manuscript
  // column at this scale is a stack of squarish brush shapes, and the shape
  // is what the eye reads at plate size. Real Tang columns are UNRULED — the
  // discipline is in the hand — so what is drawn is the drift, not a lattice.
  std::vector<Element> glyphs;
  for (int c = 0; c < nCols; ++c) {
    const float u = (float)c / (float)std::max(1, nCols - 1);
    const float drift = std::sin((float)(k * 7 + c) * 1.31f) * 2.4f;
    const float cx = w - 4.0f - u * (w - 10.0f) + drift;
    const int count = 20 + ((k * 5 + c * 3) % 11);
    for (int g = 0; g < count; ++g) {
      const float gy =
          5.0f + (float)g * 9.1f + std::sin((float)(c * 13 + g * 5)) * 0.8f;
      if (gy > kFrameH - 22) break;
      glyphs.push_back(
          box()
              .rect(SkRect::MakeXYWH(cx - 3.9f, gy, 7.8f, 6.2f))
              .fill(Fill::color(hexColor(0x241d15, 0.80f)))
              .shape(shapes::blob((uint32_t)(k * 97 + c * 13 + g), 0.42f, 7)));
    }
  }
  return box()
      .rect(SkRect::MakeXYWH(xl - segLo(seg), kFrameTop + 8 - kSegTop, w,
                             kFrameH - 16))
      .key(kit::formatted("cols%d_%d", k, seg))
      .opacity(gate(tFold0 + 0.2f, tFold0 + 1.5f))
      .children(glyphs);
}

auto DunhuangStarChart::discPlate(int seg) -> Element {
  const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
  const SkPoint c{segX(seg, discCentreS()), kBandMid};
  if (c.fX + rOut < segLo(seg) - 4 || c.fX - rOut > segHi(seg) + 4)
    return box();
  const float d = rOut * 2.0f;
  const SkPoint hub{rOut, rOut};
  // the limb: a heavy outer rule and a hairline inner one
  std::vector<Element> parts{
      box()
          .cover()
          .shape(shapes::circle())
          .stroke(Brush{}
                      .shaped(shapers::Jitter{
                          .segmentLength = 22, .deviation = 1.0f, .seed = 1300})
                      .layer(brush::presets::heavyHairHeavy(
                          1.7f, 0.5f, Fill::color(hexColor(0x3a2e1e, 0.86f)),
                          5.0f)))};
  // the DEC rings, at the published 5.10 °/cm — parametric, not stamped
  for (int dec = 60; dec <= 85; dec += 5) {
    const float rr = (kDiscCenDec - (float)dec) / kPolPerMm * kPxMm;
    if (rr <= 3 || rr >= rOut - 1) continue;
    parts.push_back(ring(hub, rr * 2)
                        .stroke(PathFormat{.width = 0.5f,
                                           .strokeFill = Fill::color(
                                               hexColor(0x5d4c37, 0.30f)),
                                           .dashIntervals = {3, 5}}));
  }
  // the 28 mansion spokes — INTERRUPTED rules, stopping short of both the
  // limb and the pole, ruled at the determinative stars' own +700 RA and
  // NOT at 12.86° apiece
  for (int m = 0; m < 28; ++m) {
    const float ang = kAzGain * wrap180(xiuRa[(size_t)m] - 278.0f);
    parts.push_back(
        box()
            .cover()
            .shape(keyedShape(
                std::tuple(ang, rOut),
                [ang, rOut] {
                  SkPathBuilder b;
                  const float a = ang * kD;
                  b.moveTo(arrange::onEllipse({rOut, rOut},
                                              {rOut * 0.14f, rOut * 0.14f}, a));
                  b.lineTo(arrange::onEllipse({rOut, rOut}, {rOut, rOut}, a));
                  return b.detach();
                }))
            .stroke(
                PathFormat{.width = 0.62f,
                           .strokeFill = Fill::color(hexColor(0x4a3b28, 0.44f)),
                           .trimStart = 0.05f,
                           .trimEnd = 0.90f}));
    parts.push_back(
        text(cat.xiu(m).native)
            .styleClass("han")
            .rect(path::centred(
                arrange::onEllipse({rOut, rOut}, {rOut - 16.0f, rOut - 16.0f},
                                   ang * kD),
                {16, 16}))
            .paragraph({.alignment = weave::TextAlignment::kCenter}));
  }
  // the DISC'S CENTRE and the TRUE POLE are not the same point: Table 3
  // puts the centre at DEC +87.6°, so the +700 pole sits 4.7 mm away.
  const float poleOff = (90.0f - kDiscCenDec) / kPolPerMm * kPxMm;
  parts.push_back(ring(hub, 10).stroke(PathFormat{
      .width = 0.7f, .strokeFill = Fill::color(hexColor(0x4a3b28, 0.7f))}));
  parts.push_back(box()
                      .rect(path::centred({rOut, rOut - poleOff}, {6.4f, 6.4f}))
                      .shape(shapes::star(4, 0.34f))
                      .fill(Fill::color(kTrace))
                      .opacity(gate(tProj - 1.4f, tProj - 0.4f)));
  // "a RED NON-ENCIRCLED star, slightly erased, could be the Pole star"
  // (Table 5, row 15, Beiji). Drawn erased, drawn unnamed, not resolved.
  parts.push_back(
      box()
          .rect(path::centred({rOut - poleOff * 0.45f, rOut - poleOff * 0.7f},
                              {8, 8}))
          .shape(shapes::circle())
          .fill(Fill::color(hexColor(0xa8382a, 0.34f)))
          .opacity(gate(tFold1, tFold1 + 0.8f)));
  return box()
      .rect(path::centred({c.fX - segLo(seg), c.fY - kSegTop}, {d, d}))
      .key(kit::formatted("disc%d", seg))
      .opacity(gate(tFold0 - 0.2f, tFold0 + 1.4f))
      .children(parts);
}

auto DunhuangStarChart::discNotes(int seg) -> Element {
  // a note written beside the drawing, which is where a note goes
  const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
  const float cx = segX(seg, discCentreS()) - segLo(seg);
  if (cx + rOut < -4 || cx - rOut > segHi(seg) - segLo(seg) + 4) return box();
  return noteStack("disc")
      .at({cx - rOut - 8, kBandMid + rOut - kSegTop + 16})
      .width(rOut * 2 + 16)
      .font({.size = 8.0f, .color = hexColor(0x4a3b28, 0.9f)})
      .key(kit::formatted("discnote%d", seg))
      .opacity(gate(tProj - 1.2f, tProj - 0.3f));
}

auto DunhuangStarChart::raRuler(int seg) -> Element {
  // THE THIRD QUESTION, DRAWN. Twelve maps of 48 deg on a 30 deg pitch: the
  // frames butt on the paper but their RA windows OVERLAP by 18 deg, so the
  // scroll's own RA axis jumps BACKWARD at every map boundary. This ruler is
  // the two readings side by side.
  const float y = kBandTop - kSegTop - 46.0f;
  std::vector<Element> spans;
  if (seg == 1)  // just clear of the sheet
    spans.push_back(text(doc.phrase("raRuler"))
                        .styleClass("caption gold")
                        .at({600, y - 26})
                        .width(900));
  for (int k = 1; k <= 12; ++k) {
    const float s0 = mapSlotS(k);
    const float xl = segX(seg, s0 + kMapWmm) - segLo(seg);
    const float xr = segX(seg, s0) - segLo(seg);
    if (xr < -6 || xl > segHi(seg) - segLo(seg) + 6) continue;
    spans.push_back(
        box()
            .rect(SkRect::MakeXYWH(xl, y, xr - xl, 11))
            .shape(keyedShape(std::string_view("span-bracket"),
                              [](SkSize sz) {
                                SkPathBuilder b;
                                b.moveTo(0, 0);
                                b.lineTo(0, sz.height());
                                b.moveTo(0, sz.height() * 0.5f);
                                b.lineTo(sz.width(), sz.height() * 0.5f);
                                b.moveTo(sz.width(), 0);
                                b.lineTo(sz.width(), sz.height());
                                return b.detach();
                              }))
            .stroke(lines::Line{
                .width = 1.0f, .fill = Fill::color(hexColor(0xc9a35c, 0.7f))}));
    spans.push_back(text(kit::formatted("%d°", (int)std::lround(wrap360(
                                                   mapCentre(k) - 24.0f))))
                        .styleClass("ruler")
                        .at({xr - 26, y + 13})
                        .width(28)
                        .paragraph({.alignment = weave::TextAlignment::kEnd}));
    spans.push_back(text(kit::formatted("%d°", (int)std::lround(wrap360(
                                                   mapCentre(k) + 24.0f))))
                        .styleClass("ruler")
                        .at({xl - 2, y + 13})
                        .width(28));
    // the 18 deg this frame shares with its LEFT neighbour, hatched
    spans.push_back(
        box()
            .rect(SkRect::MakeXYWH(xl, y - 11, 18.0f / kRaPerMm * kPxMm, 9))
            .fill(Fill::color(hexColor(0xb4531f, 0.16f)))
            .background(lines::presets::hatch(
                Fill::color(hexColor(0xb4531f, 0.75f)), 3.6f, 0.7f, 45.0f)));
  }
  return box()
      .rect(SkRect::MakeXYWH(0, 0, segHi(seg) - segLo(seg), kSegH))
      .key(kit::formatted("raruler%d", seg))
      .opacity(gate(tFold1 - 0.4f, tFold1 + 0.6f))
      .children(spans);
}

auto DunhuangStarChart::breakMark() -> Element {
  const float w = kBreakR - kBreakL + 16, h = kBandH + 32;
  const float sL = (kOriginR - kBreakR) / kPxMm,
              sR = (kOriginL - kBreakL) / kPxMm;
  return box()
      .rect(SkRect::MakeXYWH(kBreakL - 8, kBandTop - 16, w, h))
      .key("break")
      .opacity(gate(tPaper + 0.4f, tPaper + 1.4f))
      .children({box().cover().fill(Fill::color(hexColor(0x171410, 0.96f))),
                 each(2,
                      [w, h](int i) {
                        return box()
                            .rect(SkRect::MakeXYWH(
                                8.0f + (float)i * (w - 16.0f) - 9, 0, 18, h))
                            .shape(keyedShape(
                                std::string_view("break-zigzag"),
                                [](SkSize s) {
                                  SkPathBuilder b;
                                  b.moveTo(s.width() * 0.5f, 0);
                                  // the loop walks a distance; the accumulated
                                  // float is the position
                                  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                                  for (float y = 0; y < s.height();
                                       y += 22.0f) {
                                    b.lineTo(s.width() * 0.5f + 6, y + 5.5f);
                                    b.lineTo(s.width() * 0.5f - 6, y + 16.5f);
                                    b.lineTo(s.width() * 0.5f, y + 22.0f);
                                  }
                                  return b.detach();
                                }))
                            .stroke(lines::Line{
                                .width = 1.2f,
                                .fill = Fill::color(hexColor(0x8a7458, 0.8f))});
                      }),
                 text(kit::formatted("%d mm", (int)std::lround(sR - sL)))
                     .styleClass("caption")
                     .at({-16, h + 4})
                     .width(w + 32)
                     .paragraph({.alignment = weave::TextAlignment::kCenter})});
}
