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
  auto g = box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("asterisms");
  for (size_t i = 0; i < astArt.size(); ++i) {
    const AstArt& A = astArt[i];
    // the node's box is the ASTERISM's box, never the plate's
    g.child(
        box()
            .left(A.box.left())
            .top(A.box.top())
            .width(Dim(A.box.width()))
            .height(Dim(A.box.height()))
            .shape(heldPath(A.local))
            .stroke(spans::upTo(gate(A.t0, A.t0 + 0.9f)),
                    Brush{}
                        .shaped(shapers::Jitter{.segLength = 13.0f,
                                                .deviation = 0.85f,
                                                .seed = (uint32_t)(i * 31 + 7)})
                        .layer(lines::Line{.width = 1.05f,
                                           .fill = Fill::color(A.ink),
                                           .capSize = 0.0f})));
  }
  return g;
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
  auto g = box().left(0).top(0).width(Dim(kW)).height(Dim(kH)).key("m5lab");
  for (int i = 0; i < 20; ++i) {
    const M5Row& r = conc.five(i);
    const SkPoint c = m5Cent[(size_t)i];
    const int region = m5Region[(size_t)i];
    if (region != 5) continue;
    const float fl = segX(1, mapSlotS(5) + kMapWmm), fr = segX(1, mapSlotS(5));
    if (c.fX < fl - 2 || c.fX > fr + 2) continue;
    if (c.fY < kFrameTop || c.fY > kFrameTop + kFrameH) continue;
    const float t = tAudit + (float)i * tAuditEach;

    // the checking panel's own ring, raised on this row's beat. A clean row
    // takes flash() — up on its beat and faded back out three seconds
    // later — while a defect row takes gate() and stays up for the rest of
    // the running score, so from mid-audit onward the plate carries
    // exactly the six documented defects.
    g.child(box()
                .left(c.fX - 30)
                .top(c.fY - 30)
                .width(Dim(60))
                .height(Dim(60))
                .shape(shapes::circle())
                .opacity(!r.defect.empty() ? gate(t, t + 0.3f)
                                           : flash(t, t + 0.3f, t + 3.0f))
                .stroke(PathFormat{
                    .width = 1.1f,
                    .strokeFill = Fill::color(!r.defect.empty()
                                                  ? hexColor(0xb4531f, 0.85f)
                                                  : hexColor(0x2f6d86, 0.7f)),
                    .dashIntervals = {4, 4}}));

    // WHAT IS WRITTEN ON THE PAPER, defects included
    std::string written = r.native;
    float lx = c.fX + 6, ly = c.fY - 30;
    bool none = false;
    const std::string cid = r.cid;
    if (cid == "21A" || cid == "19P") none = true;    // Shen, Jiuliu
    if (cid == "22I") written = conc.five(9).native;  // Shuifu <- Sidu
    if (cid == "22K") written = conc.five(8).native;  // Sidu <- Shuifu
    if (cid == "21D") {
      ly = c.fY - 96;
      lx = c.fX + 26;
    }  // Ping, misplaced
    if (!none)
      g.child(text(toU8(written), type(faceHan ? faceHan : faceSerif, 12.5f,
                                       hexColor(0x241d15, 0.92f)))
                  .left(lx)
                  .top(ly)
                  .width(Dim(60))
                  .opacity(gate(tLine1 - 0.6f, tLine1 + 0.5f)));
    else
      g.child(text(toU8("[no label]"),
                   type(faceMono, 8.2f, hexColor(0xb4531f, 0.9f)))
                  .left(c.fX + 22)
                  .top(c.fY - 40)
                  .width(Dim(70))
                  .opacity(gate(t, t + 0.3f)));
    if (cid == "21D")  // the leader from the misplaced label to its stars
      g.child(
          box()
              .left(std::min(lx, c.fX))
              .top(ly + 10)
              .width(Dim(std::abs(lx - c.fX) + 4))
              .height(Dim(c.fY - ly - 10))
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
  return g;
}

auto DunhuangStarChart::archer() -> Element {
  const float x = segX(0, 2205.0f) - segLo(0);
  const float w = 210.0f, h = 300.0f;
  auto g = box()
               .left(x - w * 0.5f)
               .top(kBandMid - h * 0.52f - kSegTop)
               .width(Dim(w))
               .height(Dim(h))
               .key("archer")
               .opacity(gate(tArch, tArch + 1.1f));

  // the figure — every mark a Ribbon over a real polyline, the width law
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
  for (size_t i = 0; i < bones.size(); ++i) {
    const Bone& b = bones[i];
    SkPathBuilder pb;
    pb.moveTo(b.pts[0]);
    for (size_t j = 1; j < b.pts.size(); ++j) pb.lineTo(b.pts[j]);
    SkPath p = pb.detach();
    float len = 0;
    for (size_t j = 1; j < b.pts.size(); ++j)
      len += SkPoint::Distance(b.pts[j - 1], b.pts[j]);
    const float w0 = b.w0;
    brush::Ribbon rib;
    rib.fill = Fill::color(hexColor(0x241d15, 0.90f));
    rib.step = 2.0f;
    rib.width = BonePress{len, w0};
    g.child(box()
                .left(0)
                .top(0)
                .width(Dim(w))
                .height(Dim(h))
                .shape(heldPath(p))
                .stroke(spans::upTo(gate(tArch + 0.05f * (float)i,
                                         tArch + 0.05f * (float)i + 0.55f)),
                        rib));
  }
  // the bow and the arrow
  g.child(
      box()
          .left(0)
          .top(0)
          .width(Dim(w))
          .height(Dim(h))
          .shape(keyedShape(std::string_view("bow"),
                            [](SkSize) {
                              SkPathBuilder b;
                              b.moveTo(34, 8);
                              b.cubicTo(-10, 56, -10, 126, 34, 176);
                              b.moveTo(34, 8);
                              b.lineTo(20, 92);
                              b.lineTo(34, 176);
                              return b.detach();
                            }))
          .stroke(spans::upTo(gate(tArch + 0.45f, tArch + 1.0f)),
                  lines::Line{.width = 1.9f,
                              .fill = Fill::color(hexColor(0x241d15, 0.88f))}));
  g.child(
      box()
          .left(0)
          .top(0)
          .width(Dim(w))
          .height(Dim(h))
          .shape(keyedShape(std::string_view("arrow"),
                            [](SkSize) {
                              SkPathBuilder b;
                              b.moveTo(140, 90);
                              b.lineTo(6, 92);
                              return b.detach();
                            }))
          .stroke(spans::upTo(gate(tArch + 0.75f, tArch + 1.15f)),
                  lines::presets::arrow(1.5f, Fill::color(kCinnabar), 9.0f)));
  g.child(text(toU8("a bowman in traditional dress, captioned THE GOD OF"),
               type(faceMono, 8.4f, hexColor(0x4a3b28, 0.85f)))
              .left(-18)
              .top(h - 12)
              .width(Dim(300))
              .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
  g.child(text(toU8("LIGHTNING, over a title nobody can read convincingly"),
               type(faceMono, 8.4f, hexColor(0x4a3b28, 0.85f)))
              .left(-18)
              .top(h + 0)
              .width(Dim(300))
              .opacity(gate(tArch + 1.0f, tArch + 1.6f)));
  return g;
}

auto DunhuangStarChart::unreadTitle() -> Element {
  const float x = segX(0, 2118.0f) - segLo(0);
  auto g = box()
               .left(x - 34)
               .top(kBandTop + 44 - kSegTop)
               .width(Dim(68))
               .height(Dim(kBandH - 88))
               .key("title")
               .opacity(gate(tArch + 0.9f, tArch + 1.8f));
  for (int i = 0; i < 6; ++i) {
    const float y = 8.0f + (float)i * 46.0f;
    SkPathBuilder pb;
    const uint32_t seed = (uint32_t)(3326 + i * 17);
    const int nh = 2 + (int)(noise::hash(seed, 1) * 2.99f);
    for (int h = 0; h < nh; ++h) {  // horizontals, the spine of a graph
      const float hy = y + 7 + (float)h * (26.0f / (float)nh) +
                       noise::hash(seed, (uint32_t)(h + 10)) * 3.0f;
      const float x0 = 14 + noise::hash(seed, (uint32_t)(h + 20)) * 7.0f;
      const float x1 = 52 - noise::hash(seed, (uint32_t)(h + 30)) * 8.0f;
      pb.moveTo(x0, hy);
      pb.lineTo(x1, hy + noise::hash(seed, (uint32_t)(h + 40)) * 2.0f - 1.0f);
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
    g.child(
        box()
            .left(0)
            .top(0)
            .width(Dim(68))
            .height(Dim(kBandH - 88))
            .shape(heldPath(pb.detach()))
            .stroke(spans::upTo(gate(tArch + 0.9f + (float)i * 0.08f,
                                     tArch + 1.4f + (float)i * 0.08f)),
                    Brush{}
                        .shaped(shapers::Jitter{
                            .segLength = 9.0f, .deviation = 0.7f, .seed = seed})
                        .layer(lines::Line{
                            .width = 1.9f,
                            .fill = Fill::color(hexColor(0x241d15, 0.78f))})));
  }
  return g;
}
