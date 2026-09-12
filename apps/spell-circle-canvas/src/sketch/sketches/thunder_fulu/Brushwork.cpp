#include "ThunderFulu.h"

auto ThunderFulu::inkStroke(const Stroke& s) const -> Element {
  brush::Ribbon rib;
  rib.fill = Fill::color(s.ink);
  rib.step = 2.2f;

  // Both laws are PX-KEYED profiles — see StrokePress / FootPress for
  // why the key is px and not a fraction under the reveal below.
  if (s.spans.empty())
    rib.width = StrokePress{s.len, s.w0};
  else
    rib.width = FootPress{s.spans, s.w0};  // 一氣立斷 — the foot

  Brush brush;
  brush.layer(rib);
  if (s.dry) {
    // 飛白. At speed a dry brush's hair bundles separate and the ground
    // shows through in LONGITUDINAL streaks. Two dashed rails offset off
    // the stroke's own centreline are tangent-aligned by construction —
    // no per-stroke shader matrix, which is what a canvas-axis grain
    // would have needed.
    // The streaks are the GROUND, not a darker ink: where the hairs part
    // the iron shows through. A dark hairline over cinnabar is a line
    // drawn on a wet stroke; the plate's own mid-tone at this width is a
    // stroke that has run dry. Three rails at three offsets, each on its
    // own dash phase, so no two open at the same place along the run.
    PathFormat hair;
    hair.width = 2.6f;
    hair.strokeFill = Fill::color(kIronMid);
    hair.cap = SkPaint::kButt_Cap;
    hair.dashIntervals = {7.0f, 3.0f};
    hair.dashPhase = 2.0f;
    brush.layer(hair, {shapers::Offset{-2.6f, 3.0f}});
    PathFormat hair2 = hair;
    hair2.width = 1.8f;
    hair2.dashIntervals = {5.0f, 4.2f};
    hair2.dashPhase = 5.5f;
    brush.layer(hair2, {shapers::Offset{2.8f, 3.0f}});
    PathFormat hair3 = hair;
    hair3.width = 1.2f;
    hair3.dashIntervals = {4.0f, 6.0f};
    hair3.dashPhase = 9.0f;
    brush.layer(hair3, {shapers::Offset{0.2f, 3.0f}});
  }

  const SkRect f = s.frame;
  const SkPath local = s.path.makeOffset(-f.left(), -f.top());
  Element e = box()
                  .left(f.left())
                  .top(f.top())
                  .width(Dimension(f.width()))
                  .height(Dimension(f.height()))
                  .shape(heldPath(local))
                  .fill(Fill::none())
                  .stroke(std::move(brush))
                  .key(s.key);
  // The wet 頓 pool riding the head of the self-drawing line. A decoration
  // receives the ALREADY-trimmed outline, so its own window is a fraction
  // of the revealed part — this needs no second node.
  PathFormat wet;
  wet.width = s.w0 * 0.30f;
  wet.strokeFill =
      Fill::color({kCinnaWet.fR, kCinnaWet.fG, kCinnaWet.fB, 0.7f});
  wet.cap = SkPaint::kButt_Cap;
  wet.trimStart = 0.93f;
  wet.trimEnd = 1.0f;
  e.foreground(wet);
  e.mask(by::spans(spans::upTo(bind(&scribe).window(s.t0, s.t1))));
  return e;
}

auto ThunderFulu::push(SkPath p, float w0, int cls, float t0, float t1,
                       SkColor4f ink, std::string key, bool dry,
                       std::vector<std::array<float, 3>> spans) -> void {
  Stroke s;
  s.len = pathLength(p);
  s.w0 = w0;
  s.cls = cls;
  s.t0 = t0;
  s.t1 = t1;
  s.ink = ink;
  s.dry = dry;
  s.spans = std::move(spans);
  const float pad = w0 * 1.4f + 6.0f;
  s.frame = p.getBounds().makeOutset(pad, pad);
  s.path = std::move(p);
  s.key = std::move(key);
  strokes.push_back(std::move(s));
}

auto ThunderFulu::cloudGraph(const std::vector<std::pair<int, float>>& parts,
                             SkPoint at, float size, float t, float each,
                             float amp, int tag) -> void {
  float y = at.fY;
  float total = 0;
  for (const auto& [g, h] : parts) total += h;
  int idx = 0;
  for (const auto& [g, share] : parts) {
    const std::vector<Poly> ms = medians(font, g);
    const float bh = size * share / total;
    for (const auto& m : ms) {
      const int cls = classify(m);
      Poly q = place(m, {at.fX - size * 0.5f, y}, size, bh);
      SkPath sp = cloud(smoothPath(q), amp, size * 0.42f);
      push(sp, w0ForClass(cls) * size * 0.92f, cls, t + (float)idx * each,
           t + (float)(idx + 1) * each - 0.03f, kCinnabar,
           kit::formatted("body%d_%d", tag, (int)idx));
      ++idx;
    }
    y += bh;
  }
}

auto ThunderFulu::ironGround() -> Element {
  auto g = box().inset(0);

  // the plate stands off the altar cloth
  g.child(box()
              .left(-16)
              .top(-8)
              .width(Dimension(kPW + 46))
              .height(Dimension(kPH + 44))
              .shape(shapes::chamfered(26.0f))
              .fill(Paint::radialUnit({0.5f, 0.5f}, 0.78f,
                                      {{0.0f, hexColor(0x000000, 0.66f)},
                                       {0.72f, hexColor(0x000000, 0.40f)},
                                       {1.0f, hexColor(0x000000, 0.0f)}}))
              .key("ironshadow"));

  // the plate itself: hammered iron, warm under an altar lamp. The edge is
  // NOT a radius — it is what a hammer leaves.
  g.child(box()
              .inset(0)
              .shape(shapes::shaped(shapes::chamfered(17.0f),
                                    shapers::Jitter{46.0f, 2.6f, 1356}))
              // linearUnit, not linear: linear() is in NODE PIXELS, so
              // a {0.1,0} -> {0.9,1} ramp is one pixel wide at the corner
              // and clamps the whole plate to its last stop.
              .fill(Paint::linearUnit({0.10f, -0.06f}, {0.96f, 1.0f},
                                      {{0.0f, hexColor(0x736a5b)},
                                       {0.18f, hexColor(0x4f4840)},
                                       {0.46f, kIronMid},
                                       {0.78f, hexColor(0x201e1d)},
                                       {1.0f, hexColor(0x161514)}}))
              .foreground(lines::presets::hatch(
                  Fill::color(hexColor(0xa79a83, 0.075f)), 13.0f, 1.6f, -18.0f))
              .foreground(lines::presets::hatch(
                  Fill::color(hexColor(0x000000, 0.13f)), 31.0f, 3.4f, 24.0f))
              .foreground(Wash{.material = ironGrain,
                               .blend = SkBlendMode::kOverlay,
                               .amount = 0.30f})
              .foreground(Wash{.material = ironSpeck.material(),
                               .blend = SkBlendMode::kMultiply,
                               .amount = 0.85f})
              .cache(Cache::Texture)
              .key("iron"));

  // the beaten edge, and the corners rounded BY HAMMERING. brush::Pattern
  // corner tiles: a facet, not a fillet.
  //
  // `cornerAlign` is spelled out below even though the value it names is
  // the default, because the frame a corner stamp is drawn in is part of
  // the art and should not be inherited silently. Bisector is right here:
  // a hammer lands on the CORNER, and the flat it leaves straddles both
  // legs instead of lying along one of them. The facet is a stubby lozenge
  // with no strong axis, so forcing Outgoing instead rotates each one by
  // half its corner's turn without anything snapping into or out of
  // alignment — a mild difference, which is exactly why the choice has to
  // be written down rather than left to whatever the default happens to
  // be.
  g.child(
      box()
          .inset(0)
          .shape(shapes::shaped(shapes::chamfered(17.0f),
                                shapers::Jitter{38.0f, 3.1f, 46}))
          .fill(Fill::none())
          .stroke(
              Brush{}
                  .layer(lines::rails(
                      {{.across = 0.0f,
                        .width = 3.0f,
                        .fill = Fill::color(hexColor(0x5d564a, 0.85f))},
                       {.across = -5.5f,
                        .width = 1.1f,
                        .fill = Fill::color(hexColor(0x0a0909, 0.75f))}}))
                  .layer(brush::Pattern{
                      .side = hammerTile,
                      .corner = brush::CornerArt{hammerCorner,
                                                 brush::CornerAlign::Bisector},
                      .advance = 26.0f,
                      .cornerAngleDeg = 30.0f,
                      .cornerLength = 34.0f,
                      .bleedPx = 22.0f}))
          .cache(Cache::Texture)
          .key("ironedge"));
  return g;
}

auto ThunderFulu::ironWash() -> Element {
  return box()
      .inset(0)
      .shape(shapes::chamfered(17.0f))
      .fill(ironGrain)
      .opacity(0.085f)
      .blend(SkBlendMode::kSoftLight)
      .cache(Cache::Texture)
      .key("wash");
}

auto ThunderFulu::buildStrokes() -> void {
  strokes.clear();

  // --- 封界 THE ENCLOSING STROKES -------------------------------------
  // A fu is BOUNDED. The pair of long wavy verticals down either side of
  // the column, each hooked into a small loop at head and foot, is what
  // makes the writing a talisman rather than a line of characters left
  // on a plate; without them the column floats on the iron with nothing
  // saying where the charm begins or ends, and the plate reads two
  // thirds empty because the column stops where the writing stops
  // instead of where the boundary does.
  //
  // They are written FIRST, as the enclosure is: the head is set down
  // inside a space already claimed.
  for (int side = 0; side < 2; ++side) {
    const float x = side == 0 ? kCol - 132.0f : kCol + 148.0f;
    const float dir = side == 0 ? -1.0f : 1.0f;
    const float y0 = 30.0f, y1 = 1006.0f;
    SkPathBuilder b;
    // the head hook: a small loop turning outward
    b.moveTo(x + dir * 26.0f, y0 + 30.0f);
    b.quadTo(x + dir * 30.0f, y0, x, y0 + 6.0f);
    b.quadTo(x - dir * 18.0f, y0 + 14.0f, x, y0 + 40.0f);
    // the shaft: a slow wave, four bellies down the plate
    const int steps = 48;
    for (int i = 1; i <= steps; ++i) {
      const float u = (float)i / (float)steps;
      const float y = y0 + 40.0f + (y1 - 60.0f - y0) * u;
      const float wob = std::sin(u * 4.0f * 3.14159265f) * 13.0f;
      b.lineTo(x + dir * wob, y);
    }
    // the foot hook, turning outward as the head's does
    b.quadTo(x + dir * 30.0f, y1 - 34.0f, x + dir * 22.0f, y1 - 6.0f);
    b.quadTo(x + dir * 4.0f, y1 + 12.0f, x - dir * 8.0f, y1 - 4.0f);
    push(b.detach(), 9.5f, SHU, tLoad + (float)side * 0.30f,
         tLoad + 0.62f + (float)side * 0.30f, kCinnabar,
         kit::formatted("bound%d", side));
  }

  // --- 符頭 三勾 ------------------------------------------------------
  // Three hooks for the 三清 — 元始天尊, 靈寶天尊, 道德天尊 — written
  // FIRST, one silent chant line per stroke: 踏符頭, "treading the head".
  // A descending stair, each starting further LEFT and LOWER, not three
  // parallel marks.
  for (int k = 0; k < 3; ++k) {
    const float sx = 246.0f - (float)k * 32.0f;
    const float sy = 42.0f + (float)k * 47.0f;
    const float w = 150.0f, h = 44.0f;
    Poly p = {{sx, sy},
              {sx + 0.34f * w, sy + 0.09f * h},
              {sx + 0.66f * w, sy + 0.30f * h},
              {sx + 0.88f * w, sy + 0.66f * h},
              {sx + 0.78f * w, sy + 1.00f * h},
              {sx + 0.56f * w, sy + 0.88f * h}};
    const float t0 = tHead + (float)k * (tHeadEach + tHeadGap);
    push(smoothPath(p), 9.6f - (float)k * 0.6f, TURN, t0, t0 + tHeadEach,
         kCinnabar, kit::formatted("hook%d", k));
  }
  nHead = (int)strokes.size();

  // --- 符竅 -----------------------------------------------------------
  // A plain circle between head and foot, struck in ONE revolution — the
  // ends do not meet, because that is what a one-stroke ring looks like.
  // 《帝令寶珠五雷祈禱大法》 draws it exactly so, with a spirit visualised
  // inside it.
  {
    const float r = 58.0f;
    SkPathBuilder b;
    const int n = 72;
    for (int i = 0; i <= n; ++i) {
      const SkPoint q = arrange::onRing((size_t)i, (size_t)n + 1,
                                        {kCol, 242.0f}, {r * 1.02f, r * 0.96f},
                                        -1.35f, 6.02f, arrange::Turn::Open);
      i == 0 ? b.moveTo(q) : b.lineTo(q);
    }
    push(b.detach(), 7.4f, TURN, tRing, tRing + tRingDur, kCinnabar, "ring");
  }

  // --- 符身 ------------------------------------------------------------
  // The cloud-seal command graphs. No reader can read these, and that is
  // the point: they are real medians run through the cloud-wander, so the
  // illegibility is derived rather than invented.
  //   G1  YU over WU — 雨 (the 青城 符座) over 五, for 五雷      8 + 4 = 12
  //   G2  YUN — 雲, wandered hardest                                    12
  //   G3  GUI — 鬼, the 神霄 符座, SET ON THE LEFT per 《高上神霄玉清真王
  //       紫書大法》卷六                                                  9
  const int before = (int)strokes.size();
  cloudGraph({{YU, 0.58f}, {WU, 0.42f}}, {kCol + 6, 300.0f}, 158.0f, tBody,
             tBodyEach, 3.4f, 1);
  cloudGraph({{YUN, 1.0f}}, {kCol + 30, 430.0f}, 150.0f, tBody + 12 * tBodyEach,
             tBodyEach, 5.2f, 2);
  cloudGraph({{GUI, 1.0f}}, {kCol - 44, 556.0f}, 148.0f, tBody + 24 * tBodyEach,
             tBodyEach, 4.2f, 3);
  nBody = (int)strokes.size() - before;

  // --- 符膽 罡 ---------------------------------------------------------
  // The soul of the fu; without it the sheet is 空符, waste paper. TEN
  // strokes for the ten Heavenly Stems, and larger than anything else on
  // the plate. NOT wandered: its ten strokes are the argument, so it has
  // to stay countable.
  {
    const std::vector<Poly> ms = medians(font, GANG);
    const float size = 196.0f;
    for (size_t i = 0; i < ms.size(); ++i) {
      const int cls = classify(ms[i]);
      gangCls[i] = cls;
      Poly q = place(ms[i], {kCol - size * 0.5f, 574.0f}, size, size);
      const float t0 = tGall + (float)i * tGallEach;
      push(smoothPath(q), w0ForClass(cls) * size, cls, t0,
           t0 + tGallEach - 0.04f, kCinnabar, kit::formatted("gang%d", (int)i));
    }
    nGall = 10;
  }

  // --- 倒頓筆頭 --------------------------------------------------------
  // Invert the brush and tap the butt-end once. 「若對象為孕子或患有眼疾
  // 者，則萬萬不可倒頓筆頭」 — never for a pregnant woman or a patient with
  // eye disease. Drawn, and noted in the margin.
  {
    SkPathBuilder b;
    b.moveTo(kCol + 96, 790);
    b.lineTo(kCol + 99, 795);
    push(b.detach(), 12.0f, DIAN, tTap, tTap + tTapDur, kCinnaWet, "tap");
  }

  // --- 符腳 急急如律令 --------------------------------------------------
  // ONE PATH, ONE CONTOUR. 38 strokes chained by 37 ligatures — the brush
  // does not leave the plate between them, it only lightens. Written at
  // 0.034 s a stroke against the body's 0.240.
  {
    const int seq[5] = {JI, JI, RU, LV, LING};
    const float sizes[5] = {76.0f, 71.0f, 68.0f, 68.0f, 92.0f};
    const float ys[5] = {816.0f, 858.0f, 897.0f, 934.0f, 982.0f};
    const float xs[5] = {kCol - 18, kCol + 24, kCol - 22, kCol + 20, kCol - 4};
    SkPathBuilder b;
    std::vector<std::array<float, 3>> spans;
    std::vector<SkPoint> flat;
    std::vector<std::pair<size_t, size_t>> strokeRanges;
    std::vector<float> strokeW;
    for (int c = 0; c < 5; ++c) {
      const std::vector<Poly> ms = medians(font, seq[c]);
      for (const Poly& m : ms) {
        Poly q = place(m, {xs[c] - sizes[c] * 0.5f, ys[c] - sizes[c] * 0.5f},
                       sizes[c], sizes[c]);
        const size_t start = flat.size();
        for (const SkPoint& pt : q) flat.push_back(pt);
        strokeRanges.emplace_back(start, flat.size());
        strokeW.push_back(w0ForClass(classify(m)) / w0ForClass(SHU));
      }
    }
    nFootStrokes = (int)strokeRanges.size();
    // chain: every stroke, then a ligature to the next stroke's head
    b.moveTo(flat[0]);
    for (size_t i = 1; i < flat.size(); ++i) b.lineTo(flat[i]);
    SkPath chained = b.detach();
    // measure the span table on the CHAINED contour
    SkContourMeasureIter it(chained, false);
    sk_sp<SkContourMeasure> cm = it.next();
    const float total = cm ? cm->length() : 1.0f;
    float d = 0;
    for (size_t s = 0; s < strokeRanges.size(); ++s) {
      const auto [a, bIdx] = strokeRanges[s];
      float run = 0;
      for (size_t i = a; i + 1 < bIdx; ++i)
        run += SkPoint::Distance(flat[i], flat[i + 1]);
      spans.push_back({d, d + run, strokeW[s]});
      d += run;
      if (bIdx < flat.size()) {  // the ligature to the next head
        const float lig = SkPoint::Distance(flat[bIdx - 1], flat[bIdx]);
        spans.push_back({d, d + lig, 0.0f});
        d += lig;
      }
    }
    (void)total;
    push(chained, 5.9f, SHU, tFoot, tFoot + (float)nFootStrokes * tFootEach,
         kCinnaDry, "foot", true, std::move(spans));
  }
}
