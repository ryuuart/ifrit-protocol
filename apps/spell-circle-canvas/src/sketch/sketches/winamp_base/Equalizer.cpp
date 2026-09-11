#include "WinampBase.h"

auto WinampBase::eqWindow() -> Element {
  using namespace wa;
  Element w = box().width(Dim(n(275))).height(Dim(n(116)));
  w.child(box().inset(0).fill(steel).cache(Cache::Texture));
  raised(w, kWellHi, kWellLo);
  w.child(titleBar(275, "WINAMP EQUALIZER", false, false));

  // ON / AUTO / PRESETS
  Element on = key(14, 18, 26, 12, box());
  on.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
  on.child(box().width(Dim(n(3))).height(Dim(n(3))).fill(wa::kGreen));
  on.child(box().width(Dim(n(2))));
  on.child(t("ON", pix(4.4f, hexColor(0x121A24))));
  w.child(on);

  Element autoB = key(40, 18, 32, 12, box());
  autoB.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
  autoB.child(
      box().width(Dim(n(3))).height(Dim(n(3))).fill(hexColor(0x3C4A58)));
  autoB.child(box().width(Dim(n(2))));
  autoB.child(t("AUTO", pix(4.4f, hexColor(0x121A24))));
  w.child(autoB);

  w.child(textKey(217, 18, 44, 12, "PRESETS", 4.4f));

  // the response graph (native 86,17,113,19) — its curve is the SAME 10
  // Outputs the faders below ride, so the two widgets can never disagree.
  Element graph = at(box(), 86, 17, 113, 19).fill(graphMat);
  sunken(graph, mskia::withAlpha(hexColor(0x4A4A70), 0.6f), hexColor(0x08080E));
  graph.child(box().inset(0).fill(graphGrid.material()));
  graph.child(eqCurve().inset(0).cache(Cache::None));
  w.child(graph);

  // the eleven faders: preamp at native x21, bands on an 18 px pitch
  // from x78 — the real, non-skinnable positions.
  for (int i = 0; i < 11; ++i) {
    const float x = i == 0 ? 21.0f : 78.0f + 18.0f * (float)(i - 1);
    Element trough = at(box(), x, 38, 14, 63).fill(hexColor(0x14141F));
    sunken(trough, mskia::withAlpha(hexColor(0x4A4A70), 0.55f),
           hexColor(0x08080E));
    trough.child(at(box(), 2, 1, 10, 61).fill(faderTrack));
    // thumb 11x11, travel 0..52 native. bind() turns the [-1,1] gain
    // straight into pixels — no second Output in slider units.
    Element th = at(box(), 1, 0, 12, 11)
                     .fill(mskia::Paint::linearUnit(
                         {0, 0}, {0, 1},
                         {{0.0f, mskia::lighten(kBtnFace, 0.14f)},
                          {1.0f, dark(kBtnFace, 0.32f)}}))
                     .translateY(motion::bind(&gain[(size_t)i])
                                     .source(-1.0f, 1.0f)
                                     .target(n(52), n(0)));
    raised(th);
    th.child(at(box(), 2, 5, 8, 1).fill(mskia::withAlpha(kBtnLo, 0.85f)));
    trough.child(th);
    w.child(trough);
  }

  // +12dB / 0dB / -12dB, baked pixel art in the real EQMAIN.BMP, printed
  // here in the gap between the preamp and the first band.
  static const char* db[3] = {"+12DB", "+0DB", "-12DB"};
  static const float dby[3] = {38, 62, 89};
  const SkColor4f dbc[3] = {dark(kEqTop, 0.15f), dark(kEqMid, 0.28f),
                            dark(kEqBot, 0.10f)};
  for (int i = 0; i < 3; ++i)
    w.child(at(box(), 38, dby[i], 38, 7)
                .justify(Justify::End)
                .alignItems(Align::Center)
                .child(t(db[i], pix(3.6f, dbc[i]))));

  // PREAMP + the ten band captions, tight against the fader feet.
  w.child(at(box(), 3, 104, 30, 7)
              .alignItems(Align::Center)
              .child(t("PREAMP", pix(3.6f, hexColor(0x8E8EB4)))));
  static const char* bands[10] = {"60", "170", "310", "600", "1K",
                                  "3K", "6K",  "12K", "14K", "16K"};
  for (int i = 0; i < 10; ++i)
    w.child(at(box(), 76.0f + 18.0f * (float)i, 104, 18, 7)
                .justify(Justify::Center)
                .alignItems(Align::Center)
                .child(t(bands[i], pix(3.6f, hexColor(0x8E8EB4)))));
  return w;
}

auto WinampBase::eqCurve() -> Element {
  // KEYLESS: the curve is read off ten live Outputs at paint time.
  return custom([this](SkCanvas& canvas, const PaintContext& ctx) {
    const float w = ctx.size.width(), h = ctx.size.height();
    const float mid = h * 0.5f;
    SkPaint zero;
    zero.setColor4f(mskia::withAlpha(wa::kGrid, 0.9f), nullptr);
    canvas.drawRect(SkRect::MakeXYWH(0, mid - wa::n(0.5f), w, wa::n(1)), zero);

    const int kN = 10;
    std::array<SkPoint, kN> pts{};
    for (int i = 0; i < kN; ++i) {
      const float g = gain[(size_t)i + 1].value();
      pts[(size_t)i] = {w * ((float)i + 0.5f) / (float)kN,
                        mid - g * (h * 0.42f)};
    }
    SkPathBuilder b;
    b.moveTo(0, pts[0].fY);
    // Catmull-Rom through the ten band values, so the graph is one curve
    // rather than a polyline of segments.
    const int kSteps = 96;
    for (int s = 0; s <= kSteps; ++s) {
      const float u = (float)s / (float)kSteps * (float)(kN - 1);
      const int i = std::clamp((int)u, 0, kN - 2);
      const float f = u - (float)i;
      auto P = [&](int k) { return pts[(size_t)std::clamp(k, 0, kN - 1)]; };
      const SkPoint p0 = P(i - 1), p1 = P(i), p2 = P(i + 1), p3 = P(i + 2);
      const float y =
          0.5f * ((2 * p1.fY) + (-p0.fY + p2.fY) * f +
                  (2 * p0.fY - 5 * p1.fY + 4 * p2.fY - p3.fY) * f * f +
                  (-p0.fY + 3 * p1.fY - 3 * p2.fY + p3.fY) * f * f * f);
      const float x =
          0.5f * ((2 * p1.fX) + (-p0.fX + p2.fX) * f +
                  (2 * p0.fX - 5 * p1.fX + 4 * p2.fX - p3.fX) * f * f +
                  (-p0.fX + 3 * p1.fX - 3 * p2.fX + p3.fX) * f * f * f);
      if (s == 0) b.moveTo(0, y);
      b.lineTo(x, y);
      if (s == kSteps) b.lineTo(w, y);
    }
    SkPath path = b.detach();
    // hand-rolled trim: the reveal window over a LIVE path.
    const float draw = std::clamp(graphDraw.value(), 0.0f, 1.0f);
    canvas.save();
    canvas.clipRect(SkRect::MakeWH(w * draw, h));
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(wa::n(1));
    p.setColor4f(hexColor(0x1AE81A), nullptr);
    canvas.drawPath(path, p);
    p.setStrokeWidth(wa::n(2.5f));
    p.setColor4f(hexColor(0x1AE81A, 0.20f), nullptr);
    canvas.drawPath(path, p);
    canvas.restore();
  });
}
