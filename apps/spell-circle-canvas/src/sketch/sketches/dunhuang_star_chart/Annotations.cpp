#include "DunhuangStarChart.h"

auto DunhuangStarChart::locator() -> Element {
  const float lw = 1740.0f, lh = lw * kWideMm / kScrollMm;
  const float lx = 720.0f, ly = 92.0f;
  const float mm = lw / kScrollMm;
  auto g = box()
               .left(lx)
               .top(ly)
               .width(Dimension(lw))
               .height(Dimension(lh))
               .rotate(0.32f)
               .key("locator")
               .opacity(gate(0.3f, 1.2f));
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(lw))
              .height(Dimension(lh))
              .fill(Paint::linear({0, 0}, {0, lh},
                                  {{0.0f, hexColor(0xa2865c)},
                                   {0.5f, hexColor(0xd6bf95)},
                                   {1.0f, hexColor(0xa2865c)}}))
              .stroke(PathFormat{
                  .width = 0.9f,
                  .strokeFill = Fill::color(hexColor(0x2a2118, 0.75f))}));
  // the 26 clouds and the 80 columns of the divination section, at the RIGHT
  for (int c = 0; c < 26; ++c) {
    const float cx = lw - 24.0f - (float)c * 24.0f;
    g.child(box()
                .left(cx - 7)
                .top(6)
                .width(Dimension(14))
                .height(Dimension(9))
                .shape(shapes::blob((uint32_t)(700 + c), 0.34f, 6))
                .fill(Fill::color(hexColor(0x33291c, 0.85f))));
  }
  for (int c = 0; c < 80; ++c) {
    const float cx = lw - 20.0f - (float)c * 7.6f;
    g.child(box()
                .left(cx)
                .top(19)
                .width(Dimension(1.1f))
                .height(Dimension(lh - 25))
                .fill(Fill::color(hexColor(0x33291c, 0.55f))));
  }
  // the 13 maps
  const float atlasRight = lw - kScrollMm * mm + kAtlasMm * mm;
  for (int k = 1; k <= 12; ++k) {
    const float x0 = atlasRight - (mapSlotS(k) + kMapWmm) * mm;
    g.child(box()
                .left(x0)
                .top(lh * 0.16f)
                .width(Dimension(kMapWmm * mm))
                .height(Dimension(lh * 0.68f))
                .stroke(PathFormat{
                    .width = 0.8f,
                    .strokeFill = Fill::color(hexColor(0x2a2118, 0.9f))}));
  }
  {
    const float dcx = atlasRight - discCentreS() * mm;
    const float dr = lh * 0.34f;
    g.child(box()
                .left(dcx - dr)
                .top(lh * 0.5f - dr)
                .width(Dimension(dr * 2))
                .height(Dimension(dr * 2))
                .shape(shapes::circle())
                .stroke(PathFormat{
                    .width = 0.8f,
                    .strokeFill = Fill::color(hexColor(0x2a2118, 0.9f))}));
  }
  // the two windows this plate actually shows
  struct Win {
    float s0, s1;
  };
  const Win wins[2] = {
      {(kOriginL - kBreakL) / kPxMm, (kOriginL + 90) / kPxMm},
      {(kOriginR - kW - 90) / kPxMm, (kOriginR - kBreakR) / kPxMm}};
  for (const Win& wn : wins) {
    // the plate's canvas reaches ~227 mm past the atlas's own left end, so
    // window 0 runs off the strip; clamp it to the scroll it annotates
    const float a = std::max(0.0f, atlasRight - wn.s1 * mm);
    const float b = std::min(lw, atlasRight - wn.s0 * mm);
    g.child(box()
                .left(a)
                .top(-4)
                .width(Dimension(b - a))
                .height(Dimension(lh + 8))
                .fill(Fill::color(hexColor(0x2f6d86, 0.30f)))
                .stroke(spans::corners(9.0f),
                        brush::solid(1.4f, Fill::color(kTrace))));
  }
  g.child(text(toUtf8("THE WHOLE SCROLL, 1:16 \xc2\xb7 3,940 \xc3\x97 244 mm "
                      "\xc2\xb7 right: "
                      "26 cloud drawings over 80 columns of uranomancy "
                      "\xc2\xb7 left: the "
                      "13-map atlas, 2,100 mm \xc2\xb7 shaded: what this plate "
                      "shows"),
               type(faceMono, 8.6f, hexColor(0x9a8a68, 0.9f)))
              .left(2)
              .top(lh + 5)
              .width(Dimension(1700)));
  return g;
}

auto DunhuangStarChart::poleDrift() -> Element {
  const float S = 132.0f, cx = S * 0.5f, cy = S * 0.5f;
  const float pxPerDeg = (S * 0.5f - 12.0f) / 30.0f;
  auto g = box()
               .left(150)
               .top(234)
               .width(Dimension(S))
               .height(Dimension(S))
               .key("pole")
               .opacity(gate(tPrec0 - 0.8f, tPrec0 + 0.2f));
  auto poleAt = [](float epoch, float& ra, float& dec) {
    // where the pole of J2000 stands in that epoch's own coordinates,
    // which is the circle the pole is drawn walking round
    const path::Spherical p =
        precession((epoch - 2000.0f) * 0.01f)({.lonDeg = 0, .latDeg = 90});
    ra = p.lonDeg;
    dec = p.latDeg;
  };
  auto plot = [&](float ra, float dec) {
    const float r = (90.0f - dec) * pxPerDeg;
    const float a = ra * kD;
    return arrange::onEllipse({cx, cy}, {r, r}, a);
  };
  for (int ring = 10; ring <= 30; ring += 10) {
    const float rr = (float)ring * pxPerDeg;
    g.child(box()
                .left(cx - rr)
                .top(cy - rr)
                .width(Dimension(rr * 2))
                .height(Dimension(rr * 2))
                .shape(shapes::circle())
                .stroke(PathFormat{
                    .width = 0.5f,
                    .strokeFill = Fill::color(hexColor(0x8a7458, 0.30f)),
                    .dashIntervals = {2, 5}}));
  }
  // the track, drawn BACKWARD from J2000 as the precession runs
  SkPathBuilder tb;
  for (int e = 2000; e >= 500; e -= 25) {
    float ra, dec;
    poleAt((float)e, ra, dec);
    const SkPoint q = plot(ra, dec);
    (e == 2000) ? tb.moveTo(q) : tb.lineTo(q);
  }
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(S))
              .height(Dimension(S))
              .shape(heldPath(tb.detach()))
              .stroke(spans::upTo(gate(tPrec0, tPrec1)),
                      lines::Line{.width = 2.0f, .fill = Fill::color(kTrace)}));
  // the whole 26,000-year circle, faint, for context
  SkPathBuilder wb;
  for (int e = -24000; e <= 4000; e += 250) {
    float ra, dec;
    poleAt((float)e, ra, dec);
    const SkPoint q = plot(ra, dec);
    if (90.0f - dec > 30.0f) continue;
    (wb.countPoints() == 0) ? wb.moveTo(q) : wb.lineTo(q);
  }
  g.child(box()
              .left(0)
              .top(0)
              .width(Dimension(S))
              .height(Dimension(S))
              .shape(heldPath(wb.detach()))
              .stroke(PathFormat{
                  .width = 0.7f,
                  .strokeFill = Fill::color(hexColor(0x8a7458, 0.45f)),
                  .dashIntervals = {3, 4}}));
  // alp UMi is 0.74° from the J2000 pole, so its dot lands ON the centre
  // marker and the default up-right label lands ON "J2000 pole". The two
  // captions flank the coincident pair on one line instead.
  struct Ref {
    float ra, dec;
    const char* name;
    float lx, ly;
  };
  const Ref refs[3] = {{37.9529f, 89.2641f, "alp UMi", 6.0f, 6.0f},
                       {222.6764f, 74.1555f, "bet UMi", 5.0f, -5.0f},
                       {211.0973f, 64.3758f, "alp Dra", -8.0f, -13.0f}};
  for (const Ref& r : refs) {
    const SkPoint q = plot(r.ra, r.dec);
    g.child(box()
                .left(q.fX - 3)
                .top(q.fY - 3)
                .width(Dimension(6))
                .height(Dimension(6))
                .shape(shapes::circle())
                .fill(Fill::color(kCinnabar)));
    g.child(text(toUtf8(r.name), type(faceMono, 7.4f, hexColor(0x9a8a68)))
                .left(q.fX + r.lx)
                .top(q.fY + r.ly)
                .width(Dimension(60)));
  }
  {
    float ra, dec;
    poleAt(700.0f, ra, dec);
    const SkPoint q = plot(ra, dec);
    g.child(box()
                .left(q.fX - 6)
                .top(q.fY - 6)
                .width(Dimension(12))
                .height(Dimension(12))
                .shape(shapes::star(4, 0.30f))
                .fill(Fill::color(kTrace))
                .opacity(gate(tPrec1 - 0.4f, tPrec1 + 0.3f)));
  }
  g.child(box()
              .left(cx - 3)
              .top(cy - 3)
              .width(Dimension(6))
              .height(Dimension(6))
              .shape(shapes::circle())
              .stroke(PathFormat{
                  .width = 0.9f,
                  .strokeFill = Fill::color(hexColor(0xe0cfa6, 0.8f))}));
  g.child(text(toUtf8("J2000 pole"), type(faceMono, 7.4f, hexColor(0x6d6249)))
              .left(cx - 52)
              .top(cy + 6)
              .width(Dimension(70)));
  return g;
}

auto DunhuangStarChart::poleText() -> Element {
  auto g = box()
               .left(300)
               .top(228)
               .width(Dimension(452))
               .key("poletext")
               .opacity(gate(tPrec0 - 0.6f, tPrec0 + 0.4f));
  g.child(text(toUtf8("THE CHART DATES ITSELF"),
               type(faceDisplay, 12.0f, hexColor(0xc9a35c), 1.0f))
              .left(0)
              .top(0)
              .width(Dimension(430)));
  const char* rows[7] = {
      "the celestial pole's own track, from the SAME IAU 1976",
      "matrix the 1,460 stars ride. rings at 10/20/30 deg.",
      "polar distance at +700, computed here:",
      "   alp UMi 7.88   bet UMi 10.66   alp Dra 19.14",
      "the paper measures the map's pole 3.9 +/- 2.9 deg from",
      "the sky at +700 and calls it \"fully consistent\". so does",
      "this: 3.9 deg is HALF alp UMi's own distance at that date.",
  };
  for (int i = 0; i < 7; ++i)
    g.child(text(toUtf8(rows[i]),
                 type(faceMono, 9.0f, i == 3 ? kChalk : hexColor(0x9a8a68)))
                .left(0)
                .top(18.0f + (float)i * 12.2f)
                .width(Dimension(430)));

  // THE EPOCH, RUNNING. One Output remapped three ways: it turns the star
  // field's rotation matrix, walks the pole's track above, and slides this
  // marker — bind() doing the unit conversion at each call site instead of
  // three Outputs in the tick loop.
  const float bw = 430.0f;
  g.child(
      box()
          .left(0)
          .top(112)
          .width(Dimension(bw))
          .height(Dimension(9))
          .shape(keyedShape(std::string_view("ruler-scale"),
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
          .stroke(lines::Line{.width = 0.9f,
                              .fill = Fill::color(hexColor(0x9a8a68, 0.8f))}));
  g.child(text(toUtf8("+700"), type(faceMono, 8.0f, hexColor(0x9a8a68)))
              .left(0)
              .top(124)
              .width(Dimension(40)));
  g.child(text(toUtf8("J2000"), type(faceMono, 8.0f, hexColor(0x9a8a68)))
              .left(bw - 40)
              .top(124)
              .width(Dimension(40))
              .textAlign(weave::TextAlignment::kEnd));
  g.child(box()
              .left(-4)
              .top(107)
              .width(Dimension(8))
              .height(Dimension(19))
              .shape(shapes::polygon(3, 180.0f))
              .fill(Fill::color(kCinnabar))
              .translateX(settled
                              ? Animatable<float>(0.0f)
                              : Animatable<float>(bind(&scribe)
                                                      .window(tPrec0, tPrec1)
                                                      .invert()
                                                      .target(0.0f, bw))));
  g.child(text(toUtf8("13.00 Julian centuries \xc2\xb7 the sky slides "
                      "18.5\xc2\xb0 in RA"),
               type(faceMono, 8.4f, hexColor(0xc9a35c)))
              .left(0)
              .top(136)
              .width(Dimension(430)));
  return g;
}

auto DunhuangStarChart::logStyle() -> feed::TextOptions {
  feed::TextOptions s;
  // One voice, and the levels are CLASSES over it: a colour each, the
  // face and the size the base's.
  s.styles.base(type(faceMono, 9.2f, hexColor(0x9a8a68)))
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
  auto g = box()
               .left(96)
               .top(1046)
               .width(Dimension(700))
               .key("proj")
               .opacity(gate(tProj, tProj + 0.9f));
  g.child(text(toUtf8("TWO QUESTIONS THE CHART CANNOT ANSWER, AND WHY"),
               type(faceDisplay, 13.0f, hexColor(0xc9a35c), 1.1f))
              .left(0)
              .top(0)
              .width(Dimension(690)));

  // curve A: the Mercator ordinate against its own best-fit line
  const float pw = 320.0f, ph = 132.0f;
  struct Plot {
    float x, lo, hi;
    bool merc;
    const char* cap;
  };
  const Plot plots[2] = {{0, -27, 43, true,
                          "map 5 DEC \xe2\x88\x92"
                          "27\xc2\xb0\xe2\x80\xa6"
                          "+43\xc2\xb0"},
                         {366, 0, 38, false,
                          "map 13 polar distance 0\xc2\xb0\xe2\x80\xa6"
                          "38\xc2\xb0"}};
  for (const auto& pl : plots) {
    auto p =
        box().left(pl.x).top(30).width(Dimension(pw)).height(Dimension(ph));
    p.child(
        box()
            .left(0)
            .top(0)
            .width(Dimension(pw))
            .height(Dimension(ph))
            .stroke(spans::edges(16.0f),
                    brush::solid(0.9f, Fill::color(hexColor(0x8a7458, 0.5f)))));
    const float lo = pl.lo, hi = pl.hi;
    const bool merc = pl.merc;
    // THE DEPARTURE CURVE, self-normalised — the same residual the
    // number above reports, drawn along the range. Cooked HERE and
    // handed over as a held path: inside the shape callable it was
    // eighty-one samples and a regression re-run on every layout of
    // this node, and a callable compares equal to nothing so the node
    // could never prune either.
    const std::vector<float> xs = abscissa(lo, hi, 80);
    const std::vector<float> ys = ordinate(lo, hi, merc, 80);
    const measure::LineFit<float> fit = measure::lineFit<float>(xs, ys);
    float mx = 1e-9f;
    std::vector<float> dep;
    dep.reserve(xs.size());
    for (size_t i = 0; i < xs.size(); ++i) {
      dep.push_back(fit.residual(xs[i], ys[i]) / fit.slope);
      mx = std::max(mx, std::abs(dep[i]));
    }
    SkPathBuilder curve;
    for (size_t i = 0; i < dep.size(); ++i)
      (i ? curve.lineTo(pw * (float)i / 80.0f,
                        ph * 0.5f - dep[i] / mx * ph * 0.40f)
         : curve.moveTo(0.0f, ph * 0.5f - dep[0] / mx * ph * 0.40f));
    p.child(
        box()
            .left(0)
            .top(0)
            .width(Dimension(pw))
            .height(Dimension(ph))
            .shape(heldPath(curve.detach()))
            .stroke(lines::Line{.width = 1.5f, .fill = Fill::color(kTrace)}));
    // the chart's own residual band, to the same vertical scale
    const Departure& dp = merc ? depMerc : depStereo;
    const float resid = merc ? 1.61f : 3.29f;
    const float halfRaw = resid / dp.maxDeg * ph * 0.40f;
    const float half = std::min(halfRaw, ph * 0.5f);
    p.child(box()
                .left(0)
                .top(ph * 0.5f - half)
                .width(Dimension(pw))
                .height(Dimension(half * 2))
                .fill(Fill::color(hexColor(0xa8382a, 0.13f)))
                .stroke(PathFormat{
                    .width = 0.6f,
                    .strokeFill = Fill::color(hexColor(0xa8382a, 0.45f)),
                    .dashIntervals = {4, 4}}));
    p.child(text(toUtf8(pl.cap), type(faceMono, 8.4f, hexColor(0x9a8a68)))
                .left(0)
                .top(ph + 4)
                .width(Dimension(pw)));
    p.child(text(toUtf8(merc ? "linear \xe2\x88\x92 Mercator (blue) vs the "
                               "hand (red band)"
                             : "equidist. \xe2\x88\x92 stereo. (blue); the "
                               "hand is 7.6\xc3\x97 "
                               "the plot, off scale"),
                 type(faceMono, 8.4f, hexColor(0x6d6249)))
                .left(0)
                .top(ph + 15)
                .width(Dimension(pw)));
    g.child(std::move(p));
  }
  const char* lines_[6] = {
      "Mercator parts from linear by %.3f\xc2\xb0 max = %.2f mm of paper;",
      "  map 5's own DEC residual is 1.61\xc2\xb0. signal/noise %.2f, n=15,",
      "  SE(r) %.4f -> the published 0.002 is %.2f sigma. NOT A RESULT.",
      "stereographic parts from equidistant by %.3f\xc2\xb0 = %.2f mm;",
      "  radial residual 3.29\xc2\xb0. signal/noise %.2f, n=19, SE(r) %.4f",
      "  -> the published 0.013 is %.2f sigma. NOT A RESULT EITHER.",
  };
  const std::string rows[6] = {
      kit::formatted(lines_[0], depMerc.maxDeg, depMerc.mm),
      kit::formatted(lines_[1], depMerc.ratio),
      kit::formatted(lines_[2], depMerc.sigma, 0.002f / depMerc.sigma),
      kit::formatted(lines_[3], depStereo.maxDeg, depStereo.mm),
      kit::formatted(lines_[4], depStereo.ratio, depStereo.sigma),
      kit::formatted(lines_[5], 0.013f / depStereo.sigma),
  };
  for (int i = 0; i < 6; ++i)
    g.child(text(toUtf8(rows[(size_t)i]), type(faceMono, 9.6f, kChalk))
                .left(0)
                .top(196 + (float)i * 13.4f)
                .width(Dimension(690)));
  g.child(text(toUtf8("all three maps favour PURE CYLINDRICAL (0.974/0.972, "
                      "0.975/0.974, 0.996/0.994) \xe2\x80\x94 3 of 3, p=0.125"),
               type(faceMono, 9.6f, hexColor(0xcf6a4a)))
              .left(0)
              .top(280)
              .width(Dimension(690)));
  g.child(text(toUtf8("the disc cannot decide BECAUSE IT STOPS AT +52\xc2\xb0: "
                      "over a "
                      "full hemisphere the pair would part by 7.00\xc2\xb0"),
               type(faceMono, 9.6f, hexColor(0x6d6249)))
              .left(0)
              .top(294)
              .width(Dimension(690)));
  return g;
}

auto DunhuangStarChart::auditPanel() -> Element {
  auto g = box()
               .left(840)
               .top(1046)
               .width(Dimension(880))
               .key("audit")
               .opacity(gate(tAudit - 0.9f, tAudit - 0.2f));
  g.child(text(toUtf8("MAP 5 \xc2\xb7 THE ORION REGION \xc2\xb7 TABLE 4 OF "
                      "BONNET-BIDAUD, PRADERIE & WHITFIELD 2009"),
               type(faceDisplay, 13.0f, hexColor(0xc9a35c), 1.0f))
              .left(0)
              .top(0)
              .width(Dimension(880)));
  g.child(text(toUtf8("month 4 \xc2\xb7 xiu Zui, Shen, Jing \xc2\xb7 listed "
                      "N\xe2\x86\x92"
                      "S, "
                      "W\xe2\x86\x92"
                      "E, i.e. by increasing RA \xc2\xb7 R=Shi shi  B=Gan shi  "
                      "W=Wu Xian shi"),
               type(faceMono, 8.6f, hexColor(0x9a8a68)))
              .left(0)
              .top(16)
              .width(Dimension(880)));
  // the subtitle above runs top 16..24 at 8.6 px; the column header needs
  // its own line, not the same one
  const float y0 = 40.0f, rowH = 15.2f;
  // one hand-spaced monospace string cannot land on these columns: the rows
  // are absolutely placed, the numeric block is a THIRD size, and the CJK
  // pair in the middle is double-advance. Each head sits on its own column.
  struct Head {
    float x;
    const char* s;
  };
  const Head heads[9] = {
      {11, "#"},    {30, "ASTERISM"}, {160, "\xe4\xb8\xad\xe6\x96\x87"},
      {228, "COL"}, {253, "SXC"},     {281, "MAP"},
      {312, "CZ"},  {362, "CONF"},    {400, "DEFECT"}};
  for (const Head& h : heads)
    g.child(text(toUtf8(h.s), type(faceMono, 8.6f, hexColor(0x6d6249)))
                .left(h.x)
                .top(y0 - 13)
                .width(Dimension(120)));
  for (int i = 0; i < 20; ++i) {
    const M5Row& r = conc.five(i);
    const float y = y0 + (float)i * rowH;
    const float t = tAudit + (float)i * tAuditEach;
    auto row = box()
                   .left(0)
                   .top(y)
                   .width(Dimension(880))
                   .height(Dimension(rowH))
                   .opacity(gate(t, t + 0.35f));
    int cz = 0;
    for (int a = 0; a < nAst; ++a)
      if (cat.ast(a).id == r.cid) cz = astUnique(cat, cat.ast(a));
    row.child(text(toUtf8(kit::formatted("%3d", i + 1)),
                   type(faceMono, 9.4f, hexColor(0x6d6249)))
                  .left(0)
                  .top(0)
                  .width(Dimension(26)));
    row.child(text(toUtf8(r.pinyin), type(faceMono, 9.4f, kChalk))
                  .left(30)
                  .top(0)
                  .width(Dimension(126)));
    row.child(text(toUtf8(r.native), type(faceHan ? faceHan : faceSerif, 10.4f,
                                          schoolInk(r.school)))
                  .left(160)
                  .top(-2)
                  .width(Dimension(64)));
    row.child(box()
                  .left(232)
                  .top(3.4f)
                  .width(Dimension(8))
                  .height(Dimension(8))
                  .shape(shapes::circle())
                  .fill(Fill::color(schoolInk(r.school)))
                  .stroke(PathFormat{.width = 0.8f,
                                     .strokeFill = Fill::color(kInk)}));
    row.child(
        text(toUtf8(kit::formatted("%4d %4d %4d", r.sxc, r.map, cz)),
             type(faceMono, 9.4f,
                  r.sxc == r.map ? hexColor(0x9a8a68) : hexColor(0xcf6a4a)))
            .left(250)
            .top(0)
            .width(Dimension(94)));
    // the confidence index, as five cells
    for (int c = 0; c < 5; ++c)
      row.child(
          box()
              .left(356 + (float)c * 7.0f)
              .top(3.6f)
              .width(Dimension(5.2f))
              .height(Dimension(7.0f))
              .fill(Fill::color(c < r.confidence ? hexColor(0xc9a35c, 0.85f)
                                                 : hexColor(0x6d6249, 0.28f))));
    if (!r.defect.empty())
      row.child(text(toUtf8(r.defect), type(faceMono, 9.0f, hexColor(0xb4531f)))
                    .left(400)
                    .top(0)
                    .width(Dimension(478)));
    g.child(std::move(row));
  }
  const float yT = y0 + 20.0f * rowH + 8.0f;
  g.child(box()
              .left(0)
              .top(yT - 4)
              .width(Dimension(878))
              .height(Dimension(0.8f))
              .fill(Fill::color(hexColor(0x8a7458, 0.5f)))
              .opacity(gate(tAudit + 5.4f, tAudit + 5.9f)));
  const std::string tot = kit::formatted(
      "TOTALS  SXC %d   map %d   Chen Zhuo %d distinct (Fa's 3 in, Sanzhu's "
      "9 absent \xe2\x80\x94 5 + 9 = SXC's 14 for Wuche, exactly)",
      m5Sxc, m5Map, m5ChenZhuo);
  g.child(text(toUtf8(tot), type(faceMono, 9.4f, kChalk))
              .left(0)
              .top(yT)
              .width(Dimension(878))
              .opacity(gate(tAudit + 5.5f, tAudit + 6.0f)));
  g.child(
      text(toUtf8("Table 4's own n(map) column sums to 108. Its stated total "
                  "is 109. The census is soft, and the paper says so."),
           type(faceMono, 9.4f, hexColor(0xcf6a4a)))
          .left(0)
          .top(yT + 13)
          .width(Dimension(878))
          .opacity(gate(tAudit + 5.7f, tAudit + 6.2f)));
  g.child(text(toUtf8("6 documented defects in 20 asterisms, drawn AS FOUND "
                      "\xe2\x80\x94 "
                      "ringed on map 5 above. A study that corrects them has "
                      "destroyed the object."),
               type(faceMono, 9.4f, hexColor(0xb4531f)))
              .left(0)
              .top(yT + 26)
              .width(Dimension(878))
              .opacity(gate(tAudit + 5.9f, tAudit + 6.4f)));
  return g;
}

auto DunhuangStarChart::map13Panel() -> Element {
  auto g = box()
               .left(96)
               .top(1362)
               .width(Dimension(700))
               .key("m13")
               .opacity(gate(tAudit + 4.6f, tAudit + 5.4f));
  g.child(text(toUtf8("MAP 13 \xc2\xb7 THE CIRCUMPOLAR DISC \xc2\xb7 TABLE 5"),
               type(faceDisplay, 12.0f, hexColor(0xc9a35c), 1.0f))
              .left(0)
              .top(0)
              .width(Dimension(700)));
  const char* rows[10] = {
      "34 asterisms, stated total 142 stars; the n(map) column sums to 141",
      "(its Tianpei row reads \"5 or 6\", which is where the one goes).",
      "\xe7\xb4\xab\xe5\xbe\xae Ziwei: two walls, E and W, 14 red + 1 black "
      "\xe2\x80\x94 Chen Zhuo's",
      "  \xe6\x9d\xb1\xe5\x9e\xa3 8 + \xe8\xa5\xbf\xe5\x9e\xa3 7 = 15. "
      "CLOSES EXACTLY.",
      "\xe8\x8f\xaf\xe8\x93\x8b Huagai: 7 stars \"+ 6\" the authors cannot "
      "account for.",
      "  Chen Zhuo HAS \xe6\x9d\xa0 Gang appended to it \xe2\x80\x94 but 9 "
      "stars, not 6.",
      "  consistent, and it does not close. drawn on the disc, unlabelled.",
      "NI 1: one star, no character, east of Gouchen. "
      "\xe5\xa4\xa9\xe6\xa3\x93 Tianpei is the",
      "  SECOND mixed-colour asterism (\"5 R, 1 B?\"), not Ziwei alone.",
      "\xe4\xb8\x89\xe5\x85\xac Sangong: Chen Zhuo files one copy under WU "
      "XIAN, the other",
  };
  for (int i = 0; i < 10; ++i)
    g.child(text(toUtf8(rows[i]),
                 type(faceMono, 9.2f,
                      i == 3 || i == 5 ? kChalk : hexColor(0x9a8a68)))
                .left(0)
                .top(18.0f + (float)i * 12.4f)
                .width(Dimension(700)));
  g.child(
      text(toUtf8(
               "under GAN. The map draws both BLACK. Printed, not corrected."),
           type(faceMono, 9.2f, hexColor(0xb4531f)))
          .left(0)
          .top(18.0f + 10 * 12.4f)
          .width(Dimension(700)));
  return g;
}

auto DunhuangStarChart::consolePanel() -> Element {
  const float x = 1768, y = 1042, w = 700, h = 452;
  auto g = box()
               .left(x)
               .top(y)
               .width(Dimension(w))
               .height(Dimension(h))
               .fill(Fill::color(hexColor(0x100e0b, 0.86f)))
               .stroke(stroke(1.0f, Fill::color(hexColor(0x8a7458, 0.24f)),
                              PathFormat::Align::Inner))
               .key("console");
  g.child(
      box()
          .left(12)
          .top(9)
          .width(Dimension(w - 24))
          .height(Dimension(h - 18))
          .column()
          .gap(6)
          .child(feed::feed(logA, logStyle()))
          .child(box().height(1).fill(Fill::color(hexColor(0x8a7458, 0.16f))))
          .child(feed::feed(logB, logStyle()))
          .child(box().height(1).fill(Fill::color(hexColor(0x8a7458, 0.16f))))
          .child(feed::feed(logC, logStyle())));
  return g;
}

auto DunhuangStarChart::ruleNote() -> Element {
  auto g = box()
               .left(766)
               .top(228)
               .width(Dimension(770))
               .key("rulenote")
               .opacity(gate(tFold1 - 0.2f, tFold1 + 0.8f));
  const char* rows[6] = {
      "THE THIRD QUESTION, AND THE PAPER DOES NOT ASK IT",
      "2,100 mm / 13 maps / 50 columns, RA scale 4.24-4.56 deg/cm,",
      "map extension 45-48 deg (Table 3). 48 deg at 4.56 = 10.53 cm",
      "of drawn map; 12 of those + a 20.4 cm disc leaves 63 cm for 50",
      "columns = 12.7 mm each, a NORMAL Tang column. But 12 x 48 = 576",
      "deg over a 360 deg sky: the gold bars below SHARE 18 deg each.",
  };
  for (int i = 0; i < 6; ++i)
    g.child(
        text(toUtf8(rows[i]),
             type(i ? faceMono : faceDisplay, i ? 9.0f : 12.0f,
                  i ? hexColor(0x9a8a68) : hexColor(0xc9a35c), i ? 0.0f : 1.0f))
            .left(0)
            .top(i ? 16.0f + (float)i * 12.2f : 0.0f)
            .width(Dimension(700)));
  g.child(
      text(toUtf8("take 30 deg per map instead (12 x 30 = 360, one dot per "
                  "star, matching the 1,339 census) and the columns come out"),
           type(faceMono, 9.0f, hexColor(0xcf6a4a)))
          .left(0)
          .top(92)
          .width(Dimension(700)));
  g.child(
      text(toUtf8("21.6 mm wide, which is not a Tang column. NEITHER READING "
                  "CLOSES. This plate draws the first, so you can see it."),
           type(faceMono, 9.0f, hexColor(0xcf6a4a)))
          .left(0)
          .top(104)
          .width(Dimension(700)));
  return g;
}

auto DunhuangStarChart::headings() -> Element {
  auto g = box()
               .left(0)
               .top(0)
               .width(Dimension(kW))
               .height(Dimension(kH))
               .key("head");
  g.child(text(toUtf8("THE DUNHUANG STAR CHART, REPROJECTED"),
               type(faceDisplay, 27.0f, hexColor(0xe0cfa6), 2.4f))
              .left(96)
              .top(16)
              .width(Dimension(1200)));
  g.child(text(toUtf8("British Library Or.8210/S.3326 \xc2\xb7 Mogao Cave 17, "
                      "Dunhuang \xc2\xb7 +649\xe2\x80\x93"
                      "684 \xc2\xb7 3,940 \xc3\x97 244 mm, "
                      "pure mulberry fibre 0.04 mm \xc2\xb7 1,339 dots in 257 "
                      "asterisms"),
               type(faceMono, 10.2f, hexColor(0x9a8a68)))
              .left(98)
              .top(46)
              .width(Dimension(1500)));
  g.child(text(toUtf8("NOT TRACED. 1,460 real stars precessed J2000 "
                      "\xe2\x86\x92 +700 "
                      "(IAU 1976) and pushed through Table 3's own measured "
                      "projection."),
               type(faceMono, 10.2f, hexColor(0xc9a35c)))
              .left(1660)
              .top(16)
              .width(Dimension(830)));
  g.child(
      text(toUtf8("PLATE I \xc2\xb7 north up, WEST AT RIGHT, RA increasing "
                  "right-to-left \xe2\x80\x94 the direction the scroll reads"),
           type(faceMono, 9.4f, hexColor(0x6d6249)))
          .left(1660)
          .top(34)
          .width(Dimension(830)));
  // the scale bar, in cm of real paper
  const float barMm = 100.0f;
  g.child(
      box()
          .left(96)
          .top(1546)
          .width(Dimension(barMm * kPxMm))
          .height(Dimension(7))
          .shape(keyedShape(std::string_view("scale-bar"),
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
          .stroke(lines::Line{.width = 1.0f,
                              .fill = Fill::color(hexColor(0x9a8a68, 0.8f))}));
  g.child(text(toUtf8("10 cm of scroll \xc2\xb7 IDP scan 204.8 px/cm"),
               type(faceMono, 8.6f, hexColor(0x6d6249)))
              .left(96 + barMm * kPxMm + 10)
              .top(1544)
              .width(Dimension(420)));
  g.child(text(toUtf8("data: Stellarium chinese_chenzhuo (GPL) \xc2\xb7 "
                      "astronexus/HYG v4.1 \xc2\xb7 arXiv:0906.3034 Tables "
                      "3\xe2\x80\x93"
                      "5 "
                      "\xc2\xb7 IDP 7861395E5F814419BA05483EAB254832"),
               type(faceMono, 8.6f, hexColor(0x6d6249)))
              .left(1660)
              .top(1544)
              .width(Dimension(880)));
  return g;
}
