#include "DunhuangStarChart.h"

auto DunhuangStarChart::ground() -> Element {
  auto g = box()
               .left(0)
               .top(0)
               .width(Dimension(kW))
               .height(Dimension(kH))
               .key("ground");
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(kW))
              .height(Dimension(kH))
              .fill(Paint::linear({0, 0}, {kW, kH},
                                  {{0.0f, hexColor(0x171410)},
                                   {0.5f, hexColor(0x1d1913)},
                                   {1.0f, hexColor(0x120f0c)}}))
              .cache(Cache::Texture));
  return g;
}

auto DunhuangStarChart::scrollBand(float x0, float x1, const char* keyName,
                                   float tilt) -> Element {
  const float w = x1 - x0;
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
  auto g = box()
               .left(x0)
               .top(kBandTop)
               .width(Dimension(w))
               .height(Dimension(kBandH))
               .rotate(tilt)
               .key(keyName)
               .cache(Cache::Texture)
               .opacity(gate(tPaper, tPaper + 1.0f));

  // the sheet itself, with the fibre running along the roll
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(w))
              .height(Dimension(kBandH))
              .fill(Paint::linear({0, 0}, {0, kBandH},
                                  {{0.00f, kKraft},
                                   {0.055f, kPaperDeep},
                                   {0.30f, kPaperMid},
                                   {0.58f, kPaperLit},
                                   {0.88f, kPaperMid},
                                   {0.945f, kPaperDeep},
                                   {1.00f, kKraft}}))
              .cache(Cache::Texture));

  // the fibre — anisotropic grain, luminance not hue, under a bake
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(w))
              .height(Dimension(kBandH))
              .fill(paperGrain)
              .opacity(0.20f)
              .blend(SkBlendMode::kSoftLight)
              .cache(Cache::Texture));
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(w))
              .height(Dimension(kBandH))
              .foreground(Wash{.material = paperSpeck.material(),
                               .blend = SkBlendMode::kMultiply,
                               .amount = 0.55f})
              .cache(Cache::Texture));

  // "replication marks by contact due to long conservation in a rolled
  // state" — the ghost of the adjacent turn, one circumference over,
  // taking the sheet's 244 mm width as the roll's diameter
  const float circ = 244.0f * 3.14159f * kPxMm;  // ~77 cm of scroll
  for (int k = -3; k <= 3; ++k) {
    const float gx = std::fmod(std::abs(x0) + (float)k * circ, w);
    g.child(box()
                .left(gx)
                .top(6)
                .width(Dimension(3.0f))
                .height(Dimension(kBandH - 12))
                .fill(Fill::color(hexColor(0x8b6e45, 0.10f))));
  }

  // top and bottom rules — unequal, per lines::Rails
  g.child(
      box()
          .left(0)
          .top(0)
          .width(Dimension(w))
          .height(Dimension(kBandH))
          .shape(keyedShape(std::string_view("band-rules"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              b.moveTo(0, 1.5f);
                              b.lineTo(s.width(), 1.5f);
                              b.moveTo(0, s.height() - 1.5f);
                              b.lineTo(s.width(), s.height() - 1.5f);
                              return b.detach();
                            }))
          .stroke(Brush{}
                      .shaped(shapers::Jitter{
                          .segmentLength = 34, .deviation = 0.9f, .seed = 3326})
                      .layer(lines::Rails{
                          .rails = {
                              {.across = 0,
                               .width = 1.9f,
                               .fill = Fill::color(hexColor(0x6b573c, 0.62f))},
                              {.across = -4.5f,
                               .width = 0.55f,
                               .fill = Fill::color(hexColor(0x6b573c, 0.34f)),
                               .dash = {9, 6}}}})));
  return g;
}

auto DunhuangStarChart::mapFrame(int k, int seg) -> Element {
  const float s0 = mapSlotS(k);
  const float xl = segX(seg, s0 + kMapWmm), xr = segX(seg, s0);
  if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4)
    return box().width(0).height(0);
  const float w = xr - xl;
  if (w <= 2) return box().width(0).height(0);
  auto g = box()
               .left(xl - segLo(seg))
               .top(kFrameTop - kSegTop)
               .width(Dimension(w))
               .height(Dimension(kFrameH))
               .key(kit::formatted("map%d_%d", k, seg))
               .opacity(gate(tFold0 - 0.4f, tFold0 + 1.1f));

  // THE APPARATUS SITS UNDER THE INK. The paper has no frames at all —
  // the twelve maps abut and are separated only by the text columns —
  // so the frame, its corner brackets, the RA ladder and its numerals
  // are this study's own scaffolding, carrying its argument about the
  // 18° jump at every boundary. Scaffolding drawn at the weight of the
  // brush competes with the thing it is scaffolding: every one of them
  // is lighter than the faintest star mark on the map it encloses.
  g.child(
      box()
          .left(0)
          .top(0)
          .width(Dimension(w))
          .height(Dimension(kFrameH))
          .stroke(Brush{}
                      .shaped(shapers::Jitter{.segmentLength = 30,
                                              .deviation = 1.1f,
                                              .seed = (uint32_t)(600 + k)})
                      .layer(lines::Line{
                          .width = 1.0f,
                          .fill = Fill::color(hexColor(0x8a7458, 0.42f))}))
          .stroke(spans::corners(15.0f),
                  brush::solid(1.4f, Fill::color(hexColor(0x6b573c, 0.52f)))));

  // the equator — the one line whose position the paper says varies ±5°
  const float yEq = (mapGcDec(k) + 45.0f) / kDecPerMm * kPxMm;
  g.child(box()
              .left(0)
              .top(yEq - 1)
              .width(Dimension(w))
              .height(Dimension(2))
              .shape(keyedShape(std::string_view("equator"),
                                [](SkSize s) {
                                  SkPathBuilder b;
                                  b.moveTo(0, 1);
                                  b.lineTo(s.width(), 1);
                                  return b.detach();
                                }))
              .stroke(PathFormat{
                  .width = 0.75f,
                  .strokeFill = Fill::color(hexColor(0x8a3020, 0.42f)),
                  .dashIntervals = {5, 5},
                  .trimStart = 0.04f,
                  .trimEnd = 0.96f}));

  // the RA ladder: a tick every 6°, numbered every 12°
  for (int t = -24; t <= 24; t += 6) {
    const float x = w * 0.5f + (float)t / kRaPerMm * kPxMm;
    if (x < 1 || x > w - 1) continue;
    const bool big = (t % 12) == 0;
    g.child(box()
                .left(x - 0.4f)
                .top(0)
                .width(Dimension(0.9f))
                .height(Dimension(big ? 8.0f : 4.5f))
                .fill(Fill::color(hexColor(0x8a7458, 0.40f))));
    g.child(box()
                .left(x - 0.4f)
                .top(kFrameH - (big ? 8.0f : 4.5f))
                .width(Dimension(0.9f))
                .height(Dimension(big ? 8.0f : 4.5f))
                .fill(Fill::color(hexColor(0x8a7458, 0.40f))));
    if (big)
      g.child(text(toUtf8(kit::formatted("%d", (int)std::lround(wrap360(
                                                   mapCentre(k) + (float)t)))),
                   type(faceMono, 7.4f, hexColor(0x8a7458, 0.60f)))
                  .left(x - 11)
                  .top(kFrameH + 3)
                  .width(Dimension(24))
                  .textAlign(weave::TextAlignment::kCenter));
  }

  // the map's own number and month, in the margin above
  g.child(text(toUtf8(kit::formatted("%d", k)),
               type(faceDisplay, 13.0f, hexColor(0x4a3b28, 0.82f)))
              .left(w - 20)
              .top(-19)
              .width(Dimension(18))
              .textAlign(weave::TextAlignment::kEnd));

  // THE MANSION BOUNDARIES, ruled where the DETERMINATIVE STARS put them
  // and not at 12.86° apiece — Stellarium's lunar_system.defining_stars,
  // precessed to +700 like everything else. Interrupted rules: they stop
  // short of both edges of the frame.
  for (int m = 0; m < 28; ++m) {
    const float dRa = wrap180(xiuRa[(size_t)m] - mapCentre(k));
    if (std::abs(dRa) > 23.0f) continue;
    const float x = w * 0.5f + dRa / kRaPerMm * kPxMm;
    g.child(box()
                .left(x - 6)
                .top(0)
                .width(Dimension(12))
                .height(Dimension(kFrameH))
                .shape(keyedShape(std::string_view("ra-tick"),
                                  [](SkSize sz) {
                                    SkPathBuilder b;
                                    b.moveTo(sz.width() * 0.5f, 0);
                                    b.lineTo(sz.width() * 0.5f, sz.height());
                                    return b.detach();
                                  }))
                .stroke(PathFormat{
                    .width = 0.7f,
                    .strokeFill = Fill::color(hexColor(0x4a3b28, 0.5f)),
                    .dashIntervals = {7, 5},
                    .trimStart = 0.06f,
                    .trimEnd = 0.94f}));
    g.child(
        text(toUtf8(cat.xiu(m).native), type(faceHan ? faceHan : faceSerif,
                                             11.5f, hexColor(0x2a2118, 0.88f)))
            .left(x - 9)
            .top(-32)
            .width(Dimension(18))
            .textAlign(weave::TextAlignment::kCenter));
    g.child(text(toUtf8(cat.xiu(m).pinyin),
                 type(faceMono, 7.0f, hexColor(0x5d4c37, 0.75f)))
                .left(x - 20)
                .top(-45)
                .width(Dimension(40))
                .textAlign(weave::TextAlignment::kCenter));
  }
  return g;
}

auto DunhuangStarChart::columnBand(int k, int seg) -> Element {
  const float s0 = mapSlotS(k) + kMapWmm;
  const float xl = segX(seg, s0 + kColBandMm), xr = segX(seg, s0);
  if (xr < segLo(seg) - 4 || xl > segHi(seg) + 4)
    return box().width(0).height(0);
  const float w = xr - xl;
  if (w <= 2) return box().width(0).height(0);
  auto g = box()
               .left(xl - segLo(seg))
               .top(kFrameTop + 8 - kSegTop)
               .width(Dimension(w))
               .height(Dimension(kFrameH - 16))
               .key(kit::formatted("cols%d_%d", k, seg))
               .opacity(gate(tFold0 + 0.2f, tFold0 + 1.5f));
  const int nCols = (int)std::round(kColBandMm / kColMm);
  for (int c = 0; c < nCols; ++c) {
    const float u = (float)c / (float)std::max(1, nCols - 1);
    const float drift = std::sin((float)(k * 7 + c) * 1.31f) * 2.4f;
    const float cx = w - 4.0f - u * (w - 10.0f) + drift;
    const int glyphs = 20 + ((k * 5 + c * 3) % 11);
    // The columns are drawn as MARKS, not as characters: a Tang column
    // at this scale is a stack of squarish brush shapes, and the shape
    // is what the eye reads at plate size. Setting the atlas's own
    // asterism names here as vertical runs was tried and the runs do
    // not reach the plate, which is a question about vertical text
    // inside an absolutely-placed leaf rather than about this file.
    for (int gI = 0; gI < glyphs; ++gI) {
      const float gy =
          5.0f + (float)gI * 9.1f + std::sin((float)(c * 13 + gI * 5)) * 0.8f;
      if (gy > kFrameH - 22) break;
      g.child(
          box()
              .left(cx - 3.9f)
              .top(gy)
              .width(Dimension(7.8f))
              .height(Dimension(6.2f))
              .fill(Fill::color(hexColor(0x241d15, 0.80f)))
              .shape(shapes::blob((uint32_t)(k * 97 + c * 13 + gI), 0.42f, 7)));
    }
  }
  return g;
}

auto DunhuangStarChart::discPlate(int seg) -> Element {
  const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
  const SkPoint c{segX(seg, discCentreS()), kBandMid};
  if (c.fX + rOut < segLo(seg) - 4 || c.fX - rOut > segHi(seg) + 4)
    return box().width(0).height(0);
  const float d = rOut * 2.0f;
  auto g = box()
               .left(c.fX - rOut - segLo(seg))
               .top(c.fY - rOut - kSegTop)
               .width(Dimension(d))
               .height(Dimension(d))
               .key(kit::formatted("disc%d", seg))
               .opacity(gate(tFold0 - 0.2f, tFold0 + 1.4f));

  // the limb: a heavy outer rule and a hairline inner one
  g.child(
      box()
          .left(0)
          .top(0)
          .width(Dimension(d))
          .height(Dimension(d))
          .shape(shapes::circle())
          .stroke(Brush{}
                      .shaped(shapers::Jitter{
                          .segmentLength = 22, .deviation = 1.0f, .seed = 1300})
                      .layer(brush::presets::heavyHairHeavy(
                          1.7f, 0.5f, Fill::color(hexColor(0x3a2e1e, 0.86f)),
                          5.0f))));

  // the DEC rings, at the published 5.10 °/cm — parametric, not stamped
  for (int dec = 60; dec <= 85; dec += 5) {
    const float rr = (kDiscCenDec - (float)dec) / kPolPerMm * kPxMm;
    if (rr <= 3 || rr >= rOut - 1) continue;
    g.child(box()
                .left(rOut - rr)
                .top(rOut - rr)
                .width(Dimension(rr * 2))
                .height(Dimension(rr * 2))
                .shape(shapes::circle())
                .stroke(PathFormat{
                    .width = 0.5f,
                    .strokeFill = Fill::color(hexColor(0x5d4c37, 0.30f)),
                    .dashIntervals = {3, 5}}));
  }

  // the 28 mansion spokes — INTERRUPTED rules, stopping short of both the
  // limb and the pole, ruled at the determinative stars' own +700 RA and
  // NOT at 12.86° apiece
  for (int m = 0; m < 28; ++m) {
    const float ang = kAzGain * wrap180(xiuRa[(size_t)m] - 278.0f);
    g.child(box()
                .left(0)
                .top(0)
                .width(Dimension(d))
                .height(Dimension(d))
                .shape(keyedShape(std::tuple(ang, rOut),
                                  [ang, rOut](SkSize) {
                                    SkPathBuilder b;
                                    const float a = ang * kD;
                                    b.moveTo(arrange::onEllipse(
                                        {rOut, rOut},
                                        {rOut * 0.14f, rOut * 0.14f}, a));
                                    b.lineTo(arrange::onEllipse(
                                        {rOut, rOut}, {rOut, rOut}, a));
                                    return b.detach();
                                  }))
                .stroke(PathFormat{
                    .width = 0.62f,
                    .strokeFill = Fill::color(hexColor(0x4a3b28, 0.44f)),
                    .trimStart = 0.05f,
                    .trimEnd = 0.90f}));
    {
      const float a = ang * kD;
      const SkPoint label =
          arrange::onEllipse({rOut, rOut}, {rOut - 16.0f, rOut - 16.0f}, a);
      const float lx = label.fX, ly = label.fY;
      g.child(text(toUtf8(cat.xiu(m).native),
                   type(faceHan ? faceHan : faceSerif, 11.0f,
                        hexColor(0x2a2118, 0.85f)))
                  .left(lx - 8)
                  .top(ly - 8)
                  .width(Dimension(16))
                  .textAlign(weave::TextAlignment::kCenter));
    }
  }

  // the DISC'S CENTRE and the TRUE POLE are not the same point: Table 3
  // puts the centre at DEC +87.6°, so the +700 pole sits 4.7 mm away.
  const float poleOff = (90.0f - kDiscCenDec) / kPolPerMm * kPxMm;
  g.child(box()
              .left(rOut - 5)
              .top(rOut - 5)
              .width(Dimension(10))
              .height(Dimension(10))
              .shape(shapes::circle())
              .stroke(PathFormat{
                  .width = 0.7f,
                  .strokeFill = Fill::color(hexColor(0x4a3b28, 0.7f))}));
  g.child(box()
              .left(rOut - 3.2f)
              .top(rOut - poleOff - 3.2f)
              .width(Dimension(6.4f))
              .height(Dimension(6.4f))
              .shape(shapes::star(4, 0.34f))
              .fill(Fill::color(kTrace))
              .opacity(gate(tProj - 1.4f, tProj - 0.4f)));
  // "a RED NON-ENCIRCLED star, slightly erased, could be the Pole star"
  // (Table 5, row 15, Beiji). Drawn erased, drawn unnamed, not resolved.
  g.child(box()
              .left(rOut - poleOff * 0.45f - 4.0f)
              .top(rOut - poleOff * 0.7f - 4.0f)
              .width(Dimension(8))
              .height(Dimension(8))
              .shape(shapes::circle())
              .fill(Fill::color(hexColor(0xa8382a, 0.34f)))
              .opacity(gate(tFold1, tFold1 + 0.8f)));
  return g;
}

auto DunhuangStarChart::discNotes(int seg) -> Element {
  const float rOut = (kDiscCenDec - 52.0f) / kPolPerMm * kPxMm;
  const float cx = segX(seg, discCentreS()) - segLo(seg);
  if (cx + rOut < -4 || cx - rOut > segHi(seg) - segLo(seg) + 4)
    return box().width(0).height(0);
  auto g = box()
               .left(cx - rOut - 8)
               .top(kBandMid + rOut - kSegTop + 16)
               .width(Dimension(rOut * 2 + 16))
               .key(kit::formatted("discnote%d", seg))
               .opacity(gate(tProj - 1.2f, tProj - 0.3f));
  const std::string rows[4] = {
      "MAP 13 \xc2\xb7 azimuthal, RA at 1.05\xc2\xb0/cm of circumference,",
      "DEC radial at 5.10\xc2\xb0/cm, +90\xc2\xb0 to +52\xc2\xb0 (Table 3).",
      kit::formatted(
          "disc CENTRE is DEC +87.6\xc2\xb0, NOT the pole: the +700 pole"),
      kit::formatted("falls %.1f mm away, marked. and a red UNENCIRCLED star,",
                     (90.0f - kDiscCenDec) / kPolPerMm),
  };
  for (int i = 0; i < 4; ++i)
    g.child(
        text(toUtf8(rows[(size_t)i]), type(faceMono, 8.0f,
                                           i >= 2 ? hexColor(0x8a3020, 0.95f)
                                                  : hexColor(0x4a3b28, 0.9f)))
            .left(0)
            .top((float)i * 10.4f)
            .width(Dimension(rOut * 2 + 16)));
  g.child(text(toUtf8("slightly erased, sits near it \xe2\x80\x94 \"could be "
                      "the Pole "
                      "star\". Drawn as found."),
               type(faceMono, 8.0f, hexColor(0x8a3020, 0.95f)))
              .left(0)
              .top(41.6f)
              .width(Dimension(rOut * 2 + 16)));
  return g;
}

auto DunhuangStarChart::raRuler(int seg) -> Element {
  auto g = box()
               .left(0)
               .top(0)
               .width(Dimension(segHi(seg) - segLo(seg)))
               .height(Dimension(kSegH))
               .key(kit::formatted("raruler%d", seg))
               .opacity(gate(tFold1 - 0.4f, tFold1 + 0.6f));
  const float y = kBandTop - kSegTop - 46.0f;
  if (seg == 1)
    g.child(text(toUtf8("EACH FRAME SPANS 48\xc2\xb0 OF RA ON A 30\xc2\xb0 "
                        "PITCH \xc2\xb7 "
                        "HATCHED: the 18\xc2\xb0 it shares with its neighbour "
                        "\xc2\xb7 "
                        "the axis JUMPS BACK at every boundary"),
                 type(faceMono, 8.4f, hexColor(0xc9a35c, 0.9f)))
                .left(600)
                .top(y - 26)
                .width(Dimension(900)));  // just clear of the sheet
  for (int k = 1; k <= 12; ++k) {
    const float s0 = mapSlotS(k);
    const float xl = segX(seg, s0 + kMapWmm) - segLo(seg);
    const float xr = segX(seg, s0) - segLo(seg);
    if (xr < -6 || xl > segHi(seg) - segLo(seg) + 6) continue;
    g.child(
        box()
            .left(xl)
            .top(y)
            .width(Dimension(xr - xl))
            .height(Dimension(11))
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
    g.child(text(toUtf8(kit::formatted(
                     "%d\xc2\xb0",
                     (int)std::lround(wrap360(mapCentre(k) - 24.0f)))),
                 type(faceMono, 7.6f, hexColor(0xc9a35c, 0.85f)))
                .left(xr - 26)
                .top(y + 13)
                .width(Dimension(28))
                .textAlign(weave::TextAlignment::kEnd));
    g.child(text(toUtf8(kit::formatted(
                     "%d\xc2\xb0",
                     (int)std::lround(wrap360(mapCentre(k) + 24.0f)))),
                 type(faceMono, 7.6f, hexColor(0xc9a35c, 0.85f)))
                .left(xl - 2)
                .top(y + 13)
                .width(Dimension(28)));
    // the 18 deg this frame shares with its LEFT neighbour, hatched
    const float ov = 18.0f / kRaPerMm * kPxMm;
    g.child(
        box()
            .left(xl)
            .top(y - 11)
            .width(Dimension(ov))
            .height(Dimension(9))
            .fill(Fill::color(hexColor(0xb4531f, 0.16f)))
            .background(lines::presets::hatch(
                Fill::color(hexColor(0xb4531f, 0.75f)), 3.6f, 0.7f, 45.0f)));
  }
  return g;
}

auto DunhuangStarChart::breakMark() -> Element {
  auto g = box()
               .left(kBreakL - 8)
               .top(kBandTop - 16)
               .width(Dimension(kBreakR - kBreakL + 16))
               .height(Dimension(kBandH + 32))
               .key("break")
               .opacity(gate(tPaper + 0.4f, tPaper + 1.4f));
  const float w = kBreakR - kBreakL + 16, h = kBandH + 32;
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(w))
              .height(Dimension(h))
              .fill(Fill::color(hexColor(0x171410, 0.96f))));
  for (int i = 0; i < 2; ++i) {
    const float x = 8.0f + (float)i * (w - 16.0f);
    g.child(
        box()
            .left(x - 9)
            .top(0)
            .width(Dimension(18))
            .height(Dimension(h))
            .shape(keyedShape(std::string_view("break-zigzag"),
                              [](SkSize s) {
                                SkPathBuilder b;
                                b.moveTo(s.width() * 0.5f, 0);
                                // the loop walks a distance; the accumulated
                                // float is the position
                                // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
                                for (float y = 0; y < s.height(); y += 22.0f) {
                                  b.lineTo(s.width() * 0.5f + 6, y + 5.5f);
                                  b.lineTo(s.width() * 0.5f - 6, y + 16.5f);
                                  b.lineTo(s.width() * 0.5f, y + 22.0f);
                                }
                                return b.detach();
                              }))
            .stroke(lines::Line{
                .width = 1.2f, .fill = Fill::color(hexColor(0x8a7458, 0.8f))}));
  }
  const float sL = (kOriginR - kBreakR) / kPxMm,
              sR = (kOriginL - kBreakL) / kPxMm;
  g.child(text(toUtf8(kit::formatted("%d mm", (int)std::lround(sR - sL))),
               type(faceMono, 8.2f, hexColor(0x9a8a68, 0.85f)))
              .left(-16)
              .top(h + 4)
              .width(Dimension(w + 32))
              .textAlign(weave::TextAlignment::kCenter));
  return g;
}
