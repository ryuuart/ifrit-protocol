#include "ChaucerAstrolabe.h"

auto ChaucerAstrolabe::reteShadow() -> Element {
  auto inner = box().inset(0).rotate(&reteRot).transformOrigin(0.5f, 0.5f);
  const Fill dark = Fill::color(hexColor(0x140e04, 0.50f));
  for (size_t i = 0; i < pieces.size(); ++i) {
    const Piece& p = pieces[i];
    if (p.kind == Part::Ecl)
      continue;  // the zodiac band below IS the ecliptic's body
    const float w = (p.kind == Part::Arm    ? kBarW * kR
                     : p.kind == Part::Ring ? kRingW * kR
                                            : 0.030f * kR) +
                    3.0f;
    auto n =
        pathFigure(p.path, w).key("sh" + std::to_string(i)).fill(Fill::none());
    if (p.kind == Part::Thorn)
      n.foreground(brush::presets::taper(w, 3.0f, dark));
    else
      n.foreground(PathFormat{.width = w, .strokeFill = dark});
    inner.child(std::move(n));
  }
  {
    const float cyO = bandCy(-6.0f), rO = bandR(-6.0f);
    const float cyI = bandCy(6.0f), rI = bandR(6.0f);
    const SkPoint c = PL(0, cyO), ci = PL(0, cyI);
    const float ro = rO * kR, ri = rI * kR;
    inner.child(
        box()
            .rect(SkRect::MakeXYWH(c.fX - ro, c.fY - ro, 2 * ro, 2 * ro))
            .key("shband")
            .shape(keyedShape(
                std::tuple(ro, ri, c.fX, c.fY, ci.fX, ci.fY),
                [ro, ri, c, ci](SkSize) {
                  SkPathBuilder b;
                  b.setFillType(SkPathFillType::kEvenOdd);
                  b.addOval(SkRect::MakeWH(2 * ro, 2 * ro));
                  b.addOval(SkRect::MakeLTRB(
                      ci.fX - c.fX + ro - ri, ci.fY - c.fY + ro - ri,
                      ci.fX - c.fX + ro + ri, ci.fY - c.fY + ro + ri));
                  return b.detach();
                }))
            .fill(dark));
  }
  return box()
      .rect(SkRect::MakeXYWH(kCx - kR, kCy - kR, 2 * kR, 2 * kR))
      .key("reteshadow")
      .translateX(7.0f)
      .translateY(9.0f)
      .opacity(animate(from(0.0f).to(1.0f), ramp(tRete * 1000 + 200, 900)))
      .child(std::move(inner));
}

auto ChaucerAstrolabe::reteGroup() -> Element {
  auto g = box()
               .rect(SkRect::MakeXYWH(kCx - kR, kCy - kR, 2 * kR, 2 * kR))
               .key("rete")
               .rotate(&reteRot)
               .transformOrigin(0.5f, 0.5f);

  const Paint bandMat = brass(0.64f);

  // --- the zodiac band: Chaucer's ±6° of ecliptic latitude --------------
  // Clipped to the Capricorn disc, so it merges with the outer ring near
  // 0° Capricorn — which is what surviving retes actually look like.
  {
    const float cyO = bandCy(-6.0f), rO = bandR(-6.0f);
    const float cyI = bandCy(6.0f), rI = bandR(6.0f);
    auto clipped =
        box().inset(0).key("bandclip").shape(shapes::circle()).clip(true);
    const SkPoint c = PL(0, cyO);
    const float ro = rO * kR, ri = rI * kR;
    const SkPoint ci = PL(0, cyI);
    clipped.child(
        box()
            .rect(SkRect::MakeXYWH(c.fX - ro, c.fY - ro, 2 * ro, 2 * ro))
            .key("band")
            .shape(keyedShape(
                std::tuple(ro, ri, c.fX, c.fY, ci.fX, ci.fY),
                [ro, ri, c, ci](SkSize) {
                  SkPathBuilder b;
                  b.setFillType(SkPathFillType::kEvenOdd);
                  b.addOval(SkRect::MakeWH(2 * ro, 2 * ro));
                  b.addOval(SkRect::MakeLTRB(
                      ci.fX - c.fX + ro - ri, ci.fY - c.fY + ro - ri,
                      ci.fX - c.fX + ro + ri, ci.fY - c.fY + ro + ri));
                  return b.detach();
                }))
            .fill(bandMat)
            .foreground(
                styles::BevelEmboss{.depth = 2,
                                    .size = 3,
                                    .angleDeg = 125,
                                    .highlight = hexColor(0xffe9b0, 0.55f),
                                    .shadow = hexColor(0x2a1d08, 0.5f)})
            .opacity(
                animate(from(0.0f).to(1.0f), ramp(tRete * 1000 + 620, 700))));

    // the 360 degree divisions INSIDE the signs, at the projection's own
    // non-uniform spacing — they visibly bunch toward Cancer, and that is
    // the projection made legible
    for (int d = 0; d < 360; ++d) {
      if (d % 30 == 0) continue;
      const float a = ringAngle((float)d);
      const float inner = (d % 5 == 0) ? 0.955f : 0.972f;
      const SkPoint p0 = eclPoint(a), c0{0, kEclCy};
      const SkPoint q0{c0.fX + (p0.fX - c0.fX) * inner,
                       c0.fY + (p0.fY - c0.fY) * inner};
      const SkPoint A = PL(p0.fX, p0.fY), B = PL(q0.fX, q0.fY);
      SkPathBuilder pb;
      pb.moveTo(A);
      pb.lineTo(B);
      clipped.child(pathFigure(pb.detach(), 2)
                        .key("zd" + std::to_string(d))
                        .fill(Fill::none())
                        .stroke(stroke((d % 5 == 0) ? 1.2f : 0.8f,
                                       Fill::color(hexColor(0x4a3410, 0.62f))))
                        .opacity(animate(
                            from(0.0f).to(1.0f),
                            ramp(tRete * 1000 + 900 + (float)d * 0.6f, 300))));
    }

    // the band's own two edges, engraved: β = +6° and β = −6°, Chaucer's
    // "latitude of twelve degrees" (I.21). The outer edge reaches 1.1246 R
    // — 12.5% OUTSIDE the rete — so it is clipped, and the band appears to
    // fuse with the outer ring near 0° Capricorn. That fusion is not a
    // stylisation; it is what the stated width forces.
    clipped.child(cut({0, cyI}, rI, 1.8f, 0.62f, 0.34f, "bandin"));
    clipped.child(cut({0, cyO}, rO, 1.8f, 0.62f, 0.34f, "bandout"));

    // the ecliptic line itself, engraved down the middle of the band
    clipped.child(cut({0, kEclCy}, kEclR, 2.0f, 0.72f, 0.36f, "eclline")
                      .mask(by::spans(spans::upTo(animate(
                          from(0.0f).to(1.0f),
                          ramp(tRete * 1000 + 700, 1100, ch::easeOutQuint))))));

    // the twelve sign names, tangential — running lettering, engraver's
    // convention, no autoFlip. Each is centred at its OWN cell midpoint,
    // and the cells are not equal.
    for (int i = 0; i < 12; ++i) {
      const float a0 = ringAngle((float)(i * 30));
      float a1 = ringAngle((float)((i + 1) * 30));
      if (a1 < a0) a1 += 360.0f;
      const float mid = (a0 + a1) * 0.5f;
      const float span = (a1 - a0) * kD * kEclR * kR;  // px of ring
      // shapes::circle() on this box parameterises clockwise from +x in
      // canvas coords, so the ring angle maps to 1 − a/360.
      const float f = std::fmod(1.0f - mid / 360.0f + 1.0f, 1.0f);
      const float size =
          std::min(0.052f * kR, span / (float)std::strlen(kSigns[i]) * 1.62f);
      clipped.child(text(toUtf8(kSigns[i]),
                         type(faceEngrave, size, hexColor(0x33240c, 0.88f),
                              size * 0.055f))
                        .width(Dimension(2 * kEclR * kR))
                        .height(Dimension(2 * kEclR * kR))
                        .centerAt(PL(0, kEclCy))
                        .key("sign" + std::to_string(i))
                        .onPath(TextPath{.path = shapes::circle(),
                                         .at = f,
                                         .align = TextPath::Align::Center,
                                         .offset = -0.030f * kR,
                                         .autoFlip = false,
                                         .orient = TextPath::Orient::Tangent})
                        .opacity(animate(
                            from(0.0f).to(1.0f),
                            ramp(tRete * 1000 + 1200 + (float)i * 45, 400))));
    }
    g.child(std::move(clipped));
  }

  // --- the bars and the outer ring, stroked from the SKELETON -----------
  // The graph endpointDegrees audits is the same graph the metal is cut
  // from, which is the only way the check means anything.
  static const Part kOrder[3] = {Part::Arm, Part::Ring, Part::Thorn};
  for (const Part want : kOrder)
    for (size_t i = 0; i < pieces.size(); ++i) {
      const Piece& p = pieces[i];
      if (p.kind != want) continue;
      const float w = p.kind == Part::Arm    ? kBarW * kR
                      : p.kind == Part::Ring ? kRingW * kR
                                             : 0.034f * kR;
      const float delay =
          tRete * 1000 +
          (p.kind == Part::Thorn ? 1500.0f + (float)i * 26.0f : 120.0f);
      // The box pathFigure gives this node, which the brass gradient is
      // ranged over: a fill measured in canvas px has to know the box.
      SkRect bb = p.path.getBounds();
      bb.outset(w + 4, w + 4);

      auto node = pathFigure(p.path, w + 4)
                      .key("bar" + std::to_string(i))
                      .fill(Fill::none());
      if (p.kind == Part::Thorn) {
        // a Gothic thorn: springs tangentially off its host and tapers to a
        // point. THE TIP IS THE STAR'S POSITION — the thorn is drawn so its
        // point lands on the computed (r, α), not so its centroid does.
        node.foreground(
                brush::presets::taper(0.032f * kR + 2.0f, 2.4f,
                                      Fill::color(hexColor(0x3d2b0c, 0.85f))))
            .foreground(brush::presets::taper(0.032f * kR, 1.0f,
                                              brassStroke(bb, 0.66f)))
            .opacity(animate(from(0.0f).to(1.0f),
                             ramp(delay, 420, ease::outBack())));
      } else {
        // a milled edge on both sides of every bar: the keyline is what makes
        // the sheet read as a sheet rather than as a stroke on a diagram
        node.foreground(
                PathFormat{.width = w + 2.4f,
                           .strokeFill = Fill::color(hexColor(0x2a1d08, 0.7f))})
            .foreground(
                PathFormat{.width = w, .strokeFill = brassStroke(bb, 0.66f)})
            .foreground(PathFormat{
                .width = w * 0.30f,
                .strokeFill = Fill::color(hexColor(0xffedc0, 0.30f))})
            .mask(by::spans(spans::upTo(animate(
                from(0.0f).to(1.0f), ramp(delay, 900, ch::easeOutQuint)))));
      }
      g.child(std::move(node));
    }

  // --- the star names, engraved ALONG the outer ring -------------------
  for (size_t i = 0; i < kStars.size(); ++i) {
    const SkPoint p = projRA(kStars[i].dec1326, kStars[i].ra1326);
    const float a = std::atan2(p.fY, p.fX) / kD;
    const float f = std::fmod(1.0f - a / 360.0f + 1.0f, 1.0f);
    g.child(
        text(toUtf8(kStars[i].name),
             type(faceEngrave, 0.026f * kR, hexColor(0x33240c, 0.82f), 0.4f))
            .width(Dimension(2 * kR * (1.0f - kRingW * 0.5f)))
            .height(Dimension(2 * kR * (1.0f - kRingW * 0.5f)))
            .centerAt(PL(0, 0))
            .key("sname" + std::to_string(i))
            .onPath(TextPath{.path = shapes::circle(),
                             .at = f,
                             .align = TextPath::Align::Center,
                             .offset = 0.0f,
                             .autoFlip = false,
                             .orient = TextPath::Orient::Tangent})
            .opacity(animate(from(0.0f).to(1.0f),
                             ramp(tRete * 1000 + 1600 + (float)i * 90, 400))));
  }

  // --- the star tips, and the dog's head for Sirius --------------------
  for (size_t i = 0; i < kStars.size(); ++i) {
    const SkPoint p = projRA(kStars[i].dec1326, kStars[i].ra1326);
    const SkPoint c = PL(p.fX, p.fY);
    const float delay = tRete * 1000 + 1500 + (float)i * 90;
    if (i == 3) {
      // ALHABOR — the dog-star. The 1326 maker gave it a dog's head, and
      // it is the joke he built into the object.
      g.child(kit::disc(c, 0.030f * kR)
                  .key("dog")
                  .shape(shapes::blob(7u, 0.30f, 7))
                  .fill(Fill::color(kBrassP70))
                  .foreground(
                      styles::BevelEmboss{.depth = 2,
                                          .size = 2,
                                          .angleDeg = 125,
                                          .highlight = hexColor(0xffe9b0, 0.6f),
                                          .shadow = hexColor(0x2a1d08, 0.55f)})
                  .rotate(-40.0f)
                  .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 420))));
      // the ear and the muzzle
      g.child(kit::disc(SkPoint{c.fX - 0.020f * kR, c.fY - 0.020f * kR},
                        0.011f * kR)
                  .key("dogear")
                  .shape(shapes::polygon(3, 20))
                  .fill(Fill::color(kBrassP70))
                  .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 420))));
      g.child(
          kit::disc(c, 3.0f)
              .key("dogeye")
              .shape(shapes::circle())
              .fill(Fill::color(hexColor(0x2a1d08, 0.8f)))
              .opacity(animate(from(0.0f).to(1.0f), ramp(delay + 120, 300))));
    } else {
      g.child(
          kit::disc(c, 4.2f)
              .key("tip" + std::to_string(i))
              .shape(shapes::circle())
              .fill(Fill::color(kBrassP90))
              .foreground(stroke(1.0f, Fill::color(hexColor(0x2a1d08, 0.6f))))
              .opacity(animate(from(0.0f).to(1.0f), ramp(delay, 380))));
    }
  }

  // --- PRECESSION IS VISIBLE ON THIS INSTRVMENT ------------------------
  // A faint ghost of each pointer at its J2000 position beside its 1326
  // one. The ~8.6° fan is why a rete has a service life of a century or two
  // before the stars have to be re-cut — and it is the reason the star list
  // above had to be precessed at all rather than looked up.
  for (size_t i = 0; i < kStars.size(); ++i) {
    const SkPoint now1326 = projRA(kStars[i].dec1326, kStars[i].ra1326);
    const SkPoint j2000 = projRA(kStars2000[i].fY, kStars2000[i].fX);
    const SkPoint a = PL(now1326.fX, now1326.fY), b = PL(j2000.fX, j2000.fY);
    SkPathBuilder pb;
    pb.moveTo(a);
    pb.lineTo(b);
    g.child(
        pathFigure(pb.detach(), 3)
            .key("prec" + std::to_string(i))
            .fill(Fill::none())
            .stroke(
                PathFormat{.width = 1.0f,
                           .strokeFill = Fill::color(hexColor(0x2a1d08, 0.4f)),
                           .dashIntervals = {2.5f, 3.5f}})
            .opacity(animate(from(0.0f).to(1.0f),
                             ramp(tRete * 1000 + 2400 + (float)i * 30, 500))));
    g.child(
        kit::disc(b, 2.6f)
            .key("ghost" + std::to_string(i))
            .shape(shapes::circle())
            .fill(Fill::color(hexColor(0x2a1d08, 0.45f)))
            .opacity(animate(from(0.0f).to(1.0f),
                             ramp(tRete * 1000 + 2400 + (float)i * 30, 500))));
  }

  // --- the quatrefoil above, the trefoil below (MHS 45133) -------------
  auto foil = [&](int lobes, float rad, SkPoint at, const char* key,
                  float delay) {
    const float d = rad * 0.52f;
    SkPath u;
    for (int i = 0; i < lobes; ++i) {
      const SkPoint on = arrange::onRing((size_t)i, (size_t)lobes, {rad, rad},
                                         {d, d}, -SK_FloatPI / 2,
                                         2 * SK_FloatPI, arrange::Turn::Closed);
      SkPathBuilder cbb;
      cbb.addCircle(on.fX, on.fY, rad * 0.50f);
      const SkPath c = cbb.detach();
      SkPath tmp;
      Op(u, c, kUnion_SkPathOp, &tmp);
      u = tmp;
    }
    SkPathBuilder hbb;
    hbb.addCircle(rad, rad, rad * 0.40f);
    const SkPath hub = hbb.detach();
    SkPath solid;
    Op(u, hub, kUnion_SkPathOp, &solid);
    SkPath ring;
    SkPath inner = solid;
    SkMatrix m = SkMatrix::Translate(rad, rad);
    m.preScale(0.60f, 0.60f);
    m.preTranslate(-rad, -rad);
    inner = solid.makeTransform(m);
    Op(solid, inner, kDifference_SkPathOp, &ring);
    return kit::disc(at, rad)
        .key(key)
        .shape(heldPath(ring))
        .fill(brass(0.66f))
        .foreground(styles::BevelEmboss{.depth = 2,
                                        .size = 2.5f,
                                        .angleDeg = 125,
                                        .highlight = hexColor(0xffe9b0, 0.55f),
                                        .shadow = hexColor(0x2a1d08, 0.55f)})
        .opacity(
            animate(from(0.0f).to(1.0f), ramp(delay, 520, ease::outBack())));
  };
  g.child(
      foil(4, 0.100f * kR, PL(0, 0.72f), "quatrefoil", tRete * 1000 + 1000));
  g.child(foil(3, 0.090f * kR, PL(0, -0.66f), "trefoil", tRete * 1000 + 1100));

  // --- the central rosette --------------------------------------------
  g.child(
      kit::disc(PL(0, 0), 0.110f * kR)
          .key("rosette")
          .shape(shapes::star(12, 0.52f, 0.16f))
          .fill(brass(0.68f))
          .foreground(styles::BevelEmboss{.depth = 2,
                                          .size = 2,
                                          .angleDeg = 125,
                                          .highlight = hexColor(0xffe9b0, 0.6f),
                                          .shadow = hexColor(0x2a1d08, 0.6f)})
          .opacity(animate(from(0.0f).to(1.0f),
                           ramp(tRete * 1000 + 900, 500, ease::outBack()))));

  // --- the almury: "the Denticle of Capricorne" (I.23), at 0° Capricorn -
  g.child(kit::disc(PL(0, -1.0f + 0.055f), 0.048f * kR)
              .key("almury")
              .shape(shapes::polygon(3, 180))
              .fill(brass(0.68f))
              .opacity(animate(from(0.0f).to(1.0f), ramp(tPin * 1000, 420))));

  return g;
}
