#include <sigilgeometry/path/Frame.h>

#include "DunhuangStarChart.h"

auto DunhuangStarChart::buildAsterismArt() -> void {
  astArt.clear();
  const path::Rotation M = precession(-13.0f);
  for (int a = 0; a < nAst; ++a) {
    const AstRec& A = cat.ast(a);
    SkPathBuilder pb;
    // Bounds accumulated by hand, NOT with SkRect::join — join EARLY-OUTS
    // on an empty rect, and a single point IS an empty rect, so growing a
    // box one point at a time leaves it inverted and every asterism draws
    // nothing.
    float bl = 1e9f, bt = 1e9f, br = -1e9f, bb = -1e9f;
    bool open = false;
    int pts = 0;
    SkPoint prev{0, 0};
    int prevRegion = -1;
    bool havePrev = false;
    for (int w = 0; w < A.words; ++w) {
      const uint16_t v = cat.verts[A.first + w];
      if (v == kVSep) {
        open = false;
        havePrev = false;
        prevRegion = -1;
        continue;
      }
      float ra, dec;
      precess(M, cat.ra(v), cat.dec(v), ra, dec);
      SkPoint p;
      int region = 0;
      if (!paperPoint(ra, dec, p, region)) {
        open = false;
        havePrev = false;
        prevRegion = -1;
        continue;
      }
      if (p.fX < -70 || p.fX > kW + 70) {
        open = false;
        havePrev = false;
        prevRegion = -1;
        continue;
      }
      // an asterism whose stars fall on DIFFERENT maps is drawn BROKEN —
      // the scroll's RA axis is discontinuous at every map boundary, so a
      // join across one is a line that does not exist on the paper
      if (havePrev &&
          (region != prevRegion || SkPoint::Distance(prev, p) > 230.0f))
        open = false;
      prevRegion = region;
      if (!open) {
        pb.moveTo(p);
        open = true;
      } else
        pb.lineTo(p);
      bl = std::min(bl, p.fX);
      bt = std::min(bt, p.fY);
      br = std::max(br, p.fX);
      bb = std::max(bb, p.fY);
      prev = p;
      havePrev = true;
      ++pts;
    }
    if (pts < 2) continue;
    SkPath path = pb.detach();
    const SkRect bounds = SkRect::MakeLTRB(bl - 9, bt - 9, br + 9, bb + 9);
    SkPathBuilder shift;
    shift.addPath(path, -bounds.left(), -bounds.top());
    const float u = wrap360(astRa[(size_t)a] - 296.0f) / 360.0f;
    astArt.push_back({shift.detach(), bounds,
                      tLine0 + u * (tLine1 - tLine0 - 0.9f),
                      astSchool[(size_t)a] == 'W' ? hexColor(0x6a5a3f, 0.80f)
                                                  : hexColor(0x24201a, 0.86f)});
  }
}

auto DunhuangStarChart::asterismLines() -> Element {
  // each node's box is the ASTERISM's box, never the plate's
  return box()
      .cover()
      .key("asterisms")
      .children(each(astArt, [this](const AstArt& A, size_t i) {
        return box()
            .rect(A.box)
            .shape(heldPath(A.local))
            .stroke(spans::upTo(gate(A.t0, A.t0 + 0.9f)),
                    Brush{}
                        .shaped(shapers::Jitter{.segmentLength = 13.0f,
                                                .deviation = 0.85f,
                                                .seed = (uint32_t)(i * 31 + 7)})
                        .layer(lines::Line{.width = 1.05f,
                                           .fill = Fill::color(A.ink),
                                           .capSize = 0.0f}));
      }));
}

auto DunhuangStarChart::astCentroid(std::string_view cid, SkPoint& out,
                                    int& region) const -> bool {
  for (int a = 0; a < nAst; ++a) {
    if (cat.ast(a).id != cid) continue;
    const path::Rotation M = precession(-13.0f);
    double sx = 0, sy = 0;
    int n = 0, reg = 0;
    for (int w = 0; w < cat.ast(a).words; ++w) {
      const uint16_t v = cat.verts[cat.ast(a).first + w];
      if (v == kVSep) continue;
      float ra, dec;
      precess(M, cat.ra(v), cat.dec(v), ra, dec);
      SkPoint p;
      if (!paperPoint(ra, dec, p, reg)) continue;
      sx += p.fX;
      sy += p.fY;
      ++n;
    }
    if (!n) return false;
    out = {(float)(sx / n), (float)(sy / n)};
    region = reg;
    return true;
  }
  return false;
}

auto DunhuangStarChart::buildFixtures() -> void {
  const path::Rotation M = precession(-13.0f);
  for (int m = 0; m < 28; ++m) {
    float ra = 0, dec = 0;
    precess(M, cat.ra(cat.xiu(m).star), cat.dec(cat.xiu(m).star), ra, dec);
    xiuRa[(size_t)m] = ra;
  }
  for (int i = 0; i < 20; ++i) {
    m5Region[(size_t)i] = 0;
    m5Cent[(size_t)i] = {0, 0};
    astCentroid(conc.five(i).cid, m5Cent[(size_t)i], m5Region[(size_t)i]);
  }
}

auto DunhuangStarChart::map5Labels() -> Element {
  // MAP 5'S TWENTY, LABELLED AS THE SCRIBE LABELLED THEM. Two labels are
  // interchanged, two asterisms carry none at all, and one sits where it does
  // not belong — all six defects drawn, none corrected, each flagged. The
  // flags are the audit's, not the chart's: the chart just has them.
  const float fl = segX(1, mapSlotS(5) + kMapWmm), fr = segX(1, mapSlotS(5));
  std::vector<Element> labels;
  for (int i = 0; i < 20; ++i) {
    const M5Row& r = conc.five(i);
    const SkPoint c = m5Cent[(size_t)i];
    if (m5Region[(size_t)i] != 5) continue;
    if (c.fX < fl - 2 || c.fX > fr + 2) continue;
    if (c.fY < kFrameTop || c.fY > kFrameTop + kFrameH) continue;
    const float t = tAudit + (float)i * tAuditEach;
    const bool flawed = !r.defect.empty();

    // the checking panel's own ring, raised on this row's beat. A clean row
    // takes flash() — up on its beat and faded back out three seconds
    // later — while a defect row takes gate() and stays up for the rest of
    // the running score, so from mid-audit onward the plate carries
    // exactly the six documented defects.
    labels.push_back(
        box()
            .rect(path::centred(c, {60, 60}))
            .shape(shapes::circle())
            .opacity(flawed ? gate(t, t + 0.3f) : flash(t, t + 0.3f, t + 3.0f))
            .stroke(PathFormat{
                .width = 1.1f,
                .strokeFill = Fill::color(flawed ? hexColor(0xb4531f, 0.85f)
                                                 : hexColor(0x2f6d86, 0.7f)),
                .dashIntervals = {4, 4}}));

    // WHAT IS WRITTEN ON THE PAPER, defects included
    const std::string& cid = r.cid;
    std::string written = r.native;
    if (cid == "22I") written = conc.five(9).native;  // Shuifu <- Sidu
    if (cid == "22K") written = conc.five(8).native;  // Sidu <- Shuifu
    const bool misplaced = cid == "21D";              // Ping, misplaced
    const SkPoint word = misplaced ? SkPoint{c.fX + 26, c.fY - 96}
                                   : SkPoint{c.fX + 6, c.fY - 30};
    if (cid == "21A" || cid == "19P")  // Shen, Jiuliu: no label at all
      labels.push_back(text(doc.phrase("auditNoLabel"))
                           .styleClass("caption flag")
                           .at({c.fX + 22, c.fY - 40})
                           .width(70)
                           .opacity(gate(t, t + 0.3f)));
    else
      labels.push_back(
          text(written).styleClass("brush").at(word).width(60).opacity(
              gate(tLine1 - 0.6f, tLine1 + 0.5f)));
    if (misplaced)  // the leader from the misplaced label to its stars
      labels.push_back(
          box()
              .rect(SkRect::MakeXYWH(std::min(word.fX, c.fX), word.fY + 10,
                                     std::abs(word.fX - c.fX) + 4,
                                     c.fY - word.fY - 10))
              .shape(keyedShape(std::string_view("label-leader"),
                                [](SkSize sz) {
                                  SkPathBuilder b;
                                  b.moveTo(sz.width(), 0);
                                  b.lineTo(0, sz.height());
                                  return b.detach();
                                }))
              .opacity(gate(t, t + 0.3f))
              .stroke(lines::Line{.width = 0.7f,
                                  .fill = Fill::color(hexColor(0xb4531f, 0.8f)),
                                  .startCap = lines::Cap::Dot,
                                  .capSize = 4.0f}));
  }
  return box().cover().key("m5lab").children(labels);
}

auto DunhuangStarChart::archer() -> Element {
  const float w = 210.0f, h = 300.0f;
  // THE FIGURE — every mark a Ribbon over a real polyline, the width law
  // keyed in PX of arc length (Profile + alongIsPx) so the press does not
  // slide under the spans::upTo reveal each bone is drawn with. A fraction
  // would be a fraction of the REVEALED bone and the heavy 起 head would
  // walk down the limb as it draws.
  struct Bone {
    std::vector<SkPoint> pts;
    float w0;
  };
  const std::vector<Bone> bones = {
      // the cap, then the head
      {{{86, 30}, {104, 22}, {122, 30}, {118, 38}, {90, 38}, {86, 30}}, 2.6f},
      {{{92, 40},
        {112, 40},
        {116, 52},
        {110, 62},
        {96, 62},
        {90, 52},
        {92, 40}},
       2.8f},
      {{{103, 62}, {102, 78}}, 3.6f},                           // neck
      {{{102, 78}, {100, 108}, {99, 132}}, 5.6f},               // spine
      {{{100, 82}, {72, 88}, {46, 92}, {34, 94}}, 4.6f},        // bow arm
      {{{101, 84}, {126, 96}, {140, 84}, {138, 70}}, 4.6f},     // draw arm
      {{{78, 84}, {60, 122}, {52, 176}, {58, 214}}, 3.2f},      // left robe
      {{{124, 84}, {142, 124}, {150, 178}, {144, 214}}, 3.2f},  // right robe
      {{{58, 214}, {84, 222}, {118, 222}, {144, 214}}, 3.0f},   // hem
      {{{80, 222}, {74, 250}, {66, 274}}, 4.4f},                // left leg
      {{{122, 222}, {130, 250}, {140, 274}}, 4.4f},             // right leg
      {{{60, 274}, {78, 278}}, 3.0f},                           // feet
      {{{134, 274}, {152, 278}}, 3.0f},
      {{{74, 132}, {126, 130}}, 2.4f},            // belt
      {{{99, 66}, {92, 100}, {104, 128}}, 2.0f},  // lapel
  };
  // A DRAWING ON THE FIGURE'S OWN BOX: the bones, the bow and the arrow are
  // all strokes of a held path over it, each revealed on its own beat.
  const auto over = [w, h](SkPath p) {
    return box().rect(SkRect::MakeWH(w, h)).shape(heldPath(std::move(p)));
  };
  return box()
      .rect(path::centred(
          {segX(0, 2205.0f) - segLo(0), kBandMid - h * 0.02f - kSegTop},
          {w, h}))
      .key("archer")
      .opacity(gate(tArch, tArch + 1.1f))
      .children(
          {each(bones,
                [this, over](const Bone& b, size_t i) {
                  SkPathBuilder pb;
                  pb.moveTo(b.pts[0]);
                  float len = 0;
                  for (size_t j = 1; j < b.pts.size(); ++j) {
                    pb.lineTo(b.pts[j]);
                    len += SkPoint::Distance(b.pts[j - 1], b.pts[j]);
                  }
                  return over(pb.detach())
                      .stroke(
                          spans::upTo(gate(tArch + 0.05f * (float)i,
                                           tArch + 0.05f * (float)i + 0.55f)),
                          brush::Ribbon{
                              .fill = Fill::color(hexColor(0x241d15, 0.90f)),
                              .step = 2.0f,
                              .width = BonePress{len, b.w0}});
                }),
           // the bow and the arrow
           over(SkPathBuilder()
                    .moveTo(34, 8)
                    .cubicTo(-10, 56, -10, 126, 34, 176)
                    .moveTo(34, 8)
                    .lineTo(20, 92)
                    .lineTo(34, 176)
                    .detach())
               .stroke(
                   spans::upTo(gate(tArch + 0.45f, tArch + 1.0f)),
                   lines::Line{.width = 1.9f,
                               .fill = Fill::color(hexColor(0x241d15, 0.88f))}),
           over(SkPathBuilder().moveTo(140, 90).lineTo(6, 92).detach())
               .stroke(
                   spans::upTo(gate(tArch + 0.75f, tArch + 1.15f)),
                   lines::presets::arrow(1.5f, Fill::color(kCinnabar), 9.0f)),
           noteStack("archer")
               .styleClass("paper")
               .at({-18, h - 12})
               .width(300)
               .opacity(gate(tArch + 1.0f, tArch + 1.6f))});
}

auto DunhuangStarChart::unreadTitle() -> Element {
  // THE CLOSING TITLE. The circulating reading 'Qi jie meng ji dian jing yi
  // juan' ignores the first character and reads a non-standard third
  // character as 夢; 蔑 is likelier. Six graphs, drawn ILLEGIBLE on purpose:
  // the spine of each is a run of horizontals with one vertical through them
  // and a falling stroke either side where the hash says so.
  const float h = kBandH - 88;
  return box()
      .rect(SkRect::MakeXYWH(segX(0, 2118.0f) - segLo(0) - 34,
                             kBandTop + 44 - kSegTop, 68, h))
      .key("title")
      .opacity(gate(tArch + 0.9f, tArch + 1.8f))
      .children(each(6, [this, h](int i) {
        const float y = 8.0f + (float)i * 46.0f;
        const uint32_t seed = (uint32_t)(3326 + i * 17);
        SkPathBuilder pb;
        const int nh = 2 + (int)(noise::hash(seed, 1) * 2.99f);
        for (int n = 0; n < nh; ++n) {  // horizontals, the spine of a graph
          const float hy = y + 7 + (float)n * (26.0f / (float)nh) +
                           noise::hash(seed, (uint32_t)(n + 10)) * 3.0f;
          pb.moveTo(14 + noise::hash(seed, (uint32_t)(n + 20)) * 7.0f, hy);
          pb.lineTo(52 - noise::hash(seed, (uint32_t)(n + 30)) * 8.0f,
                    hy + noise::hash(seed, (uint32_t)(n + 40)) * 2.0f - 1.0f);
        }
        const float vx = 26 + noise::hash(seed, 50) * 12.0f;
        pb.moveTo(vx, y + 4);
        pb.lineTo(vx + noise::hash(seed, 51) * 4.0f - 2.0f, y + 36);
        if (noise::hash(seed, 52) > 0.45f) {  // a left-falling stroke
          pb.moveTo(34, y + 6);
          pb.lineTo(15, y + 34);
        }
        if (noise::hash(seed, 53) > 0.55f) {  // and a right-falling one
          pb.moveTo(30, y + 10);
          pb.lineTo(50, y + 34);
        }
        return box()
            .rect(SkRect::MakeWH(68, h))
            .shape(heldPath(pb.detach()))
            .stroke(
                spans::upTo(gate(tArch + 0.9f + (float)i * 0.08f,
                                 tArch + 1.4f + (float)i * 0.08f)),
                Brush{}
                    .shaped(shapers::Jitter{
                        .segmentLength = 9.0f, .deviation = 0.7f, .seed = seed})
                    .layer(lines::Line{
                        .width = 1.9f,
                        .fill = Fill::color(hexColor(0x241d15, 0.78f))}));
      }));
}
