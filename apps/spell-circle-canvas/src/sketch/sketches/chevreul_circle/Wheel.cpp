#include "ChevreulCircle.h"

auto ChevreulCircle::buildQuadrantPool() -> void {
  quadAtlas = std::make_shared<instancing::Atlas>(2.0f);
  // ONE white cell. Every one of the 200 colours arrives as a tint, which
  // is precisely the fidelity question nobody had asked of this path.
  quadFrame =
      quadAtlas->cell(box().fill(Fill::color(kWhite)), {kQCellW, kQCellH});
  quadPool = std::make_shared<instancing::Pool>();
  quadPool->resize(200);
  auto pos = quadPool->positions();
  auto tint = quadPool->tints();
  auto fr = quadPool->frames();
  for (int k = 0; k < 10; ++k)
    for (int t = 0; t < 20; ++t) {
      const size_t i = (size_t)k * 20 + (size_t)t;
      pos[i] = {(float)k * (kQCellW + kQGapX) + kQCellW * 0.5f,
                (float)t * (kQCellH + kQGapY) + kQCellH * 0.5f};
      tint[i] = quadrantCell(corrected[0], k + 1, t + 1);
      fr[i] = quadFrame;
    }
  quadPool->commit();
}

auto ChevreulCircle::theWheel(sketch::SketchContext& ctx) -> Element {
  Element g = box();

  // the panel's own shadow, attached FIRST so the fill paints over it
  g.child(
      kit::disc(kC, kRSweepOut + 6)
          .shape(shapes::circle())
          .background(styles::dropShadow(hexColor(0x3A352D, 0.30f), {3, 3}, 8))
          .fill(Fill::color(kPaper)));

  // ---- the limb's tint and its two engraved circles ---------------
  g.child(kit::disc(kC, kRLimbOut)
              .shape(shapes::annulus(kRLimbIn / kRLimbOut))
              .fill(Fill::color(kWell))
              .opacity(bind(&demo).window(0.15f, 0.19f)));
  for (float r : {kRLimbIn, kRLimbOut})
    g.child(kit::disc(kC, r)
                .key(kit::formatted("limb%.0f", r))
                .shape(shapes::circle())
                .fill(Fill::none())
                .stroke(spans::upTo(bind(&demo).window(0.14f, 0.20f)),
                        stroke(1.0f, Fill::color(kRule))));

  // ---- the 72 couleurs franches -----------------------------------
  // THE GAP IS ANGULAR, which is what makes it taper. On the engraving
  // the paper between two blades is wide at the rim and closes toward
  // the medallion: it is a constant fraction of the sector PITCH, so its
  // width in pixels grows with the radius. A constant-width hairline —
  // which is what a stroked separator gives — reads as one continuous
  // ring of colour with rules drawn on it, and that is the difference
  // between a measuring instrument and a colour wheel. kBladeGapDeg of
  // the 5 deg pitch leaves a quarter of the pitch as paper at the rim.
  // Check 11 runs against the un-gapped geometry, which is the pitch the
  // plate is a statement about.
  for (int n = 0; n < 72; ++n) {
    const float lo = 0.005f + 0.0021f * (float)n;
    g.child(kit::disc(kC, kRColour)
                .key("sector" + std::to_string(n))
                .shape(shapes::sector(sectorStart(n) + kBladeGapDeg * 0.5f,
                                      kSectorDeg - kBladeGapDeg, kInner))
                .fill(Fill::color(corrected[(size_t)n]))
                .transformOrigin(0.5f, 0.5f)
                .opacity(bind(&demo).window(lo, lo + 0.010f))
                .scale(bind(&demo)
                           .window(lo, lo + 0.014f)
                           .map(ch::EaseFn(ease::outBack(1.2f)))
                           .target(0.86f, 1.0f)));
  }

  // The plate's seventy-two white radii are the GAPS, not a decoration
  // drawn over them: paper showing between blades, tapering with the
  // pitch. A stroked radial family here would be a second, constant-width
  // white on top of a tapering one.

  // plate tone: real intaglio leaves the whole printed area faintly
  // toned. Clipped to the wheel, cached as a texture.
  if (kPlateTone)
    g.child(kit::disc(kC, kRColour)
                .shape(shapes::circle())
                .fill(Fill::none())
                .foreground(decorations::wash(plateTone, SkBlendMode::kMultiply,
                                              0.055f))
                .cache(Cache::Texture));

  // ---- the continuous-sweep ring ----------------------------------
  // The same 72 measured values as ONE gradient: 144 stops (doubled, so
  // the steps stay franches) in one shader, against the discrete sectors
  // beside it. Continuous against franche is the distinction the plate's
  // own title makes.
  g.child(kit::disc(kC, kRSweepOut)
              .key("sweepring")
              .shape(shapes::annulus(kRSweepIn / kRSweepOut))
              .fill(sweepRing)
              .opacity(bind(&demo).window(0.17f, 0.22f)));

  // ---- the medallion ----------------------------------------------
  const float rMed = kRColour * kInner;
  g.child(kit::disc(kC, rMed + 3)
              .shape(shapes::circle())
              .fill(Paint::glowUnit({0.5f, 0.5f}, 1.0f,
                                    {{0.0f, hexColor(0x8C8578, 0.0f)},
                                     {0.72f, hexColor(0x8C8578, 0.0f)},
                                     {1.0f, hexColor(0x8C8578, 0.22f)}})));
  g.child(kit::disc(kC, rMed)
              .shape(shapes::circle())
              .fill(Fill::color(kPaper))
              .stroke(stroke(1.0f, Fill::color(kRule))));
  {
    // the plate's own engraved caption, five lines, its own line breaks
    struct Cap {
      const char* s;
      float size;
      float track;
      bool bold;
      float dy;
    };
    const std::array<Cap, 6> caps = {{
        {"1er", 11, 0, false, -66},
        {"CERCLE CHROMATIQUE", 13.5f, 0.7f, false, -46},
        {"DE", 8, 2.0f, false, -25},
        {"Mr CHEVREUL", 17, 0.9f, true, -6},
        {"RENFERMANT", 7.5f, 1.6f, false, 17},
        {"LES·COULEURS·FRANCHES.", 9.5f, 0.5f, false, 36},
    }};
    for (size_t i = 0; i < caps.size(); ++i) {
      const Cap& c = caps[i];
      g.child(centred(c.s,
                      c.bold ? sbd(c.size, kInk, c.track)
                             : sr(c.size, kInk, c.track),
                      kC.fX - 97, kC.fY + c.dy, 194)
                  .key("cap" + std::to_string(i))
                  .opacity(bind(&demo).window(0.185f + 0.006f * (float)i,
                                              0.205f + 0.006f * (float)i)));
    }
    g.child(at(kC.fX - 48, kC.fY + 54, 96, 1)
                .fill(Fill::color(kInk))
                .transformOrigin(0.5f, 0.5f)
                .scale(bind(&demo).window(0.22f, 0.24f)));
  }

  // ---- the limb: 12 names + 60 numerals, TANGENTIAL, glyph-up inward
  //      (the plate's convention, read off the scan — see the header)
  const weave::TextStyle nameSt = sr(8.5f, kInk, 0.55f);
  const weave::TextStyle numSt = sr(9.5f, kInk2, 0);
  const float rMid = (kRLimbIn + kRLimbOut) * 0.5f;
  // TextPath::offset positions the BASELINE, and glyph-up points inward
  // here, so centring a run in the limb band needs its cap height —
  // which is what compose::metrics() is for. Guessing it puts every one
  // of the seventy-two labels half a cap off centre.
  const float capName =
      ctx.fonts ? sigil::compose::metrics(nameSt, *ctx.fonts).capHeight : 6.0f;
  const float capNum =
      ctx.fonts ? sigil::compose::metrics(numSt, *ctx.fonts).capHeight : 6.6f;
  for (int n = 0; n < 72; ++n) {
    const float f = (float)n / 72.0f;  // exact, by construction of rimBaseline
    const float lo = 0.20f + 0.0009f * (float)n;
    constexpr bool kNoLimb = !kLimb;
    auto run = [&](const std::string& s, const weave::TextStyle& st,
                   float offset, const std::string& key) {
      if (kNoLimb) return;
      // The BASELINE resolves against the text node's OWN laid-out box, so
      // the run has to be the disc-sized element itself; wrapping it in a
      // sized parent silently collapses all 72 labels onto one point.
      g.child(text(U(s), st)
                  .key(key)
                  .width(Dim(2 * rMid))
                  .height(Dim(2 * rMid))
                  .centerAt(kC)
                  .onPath(TextPath{.path = rimBaseline(),
                                   .at = f,
                                   .align = TextPath::Align::Center,
                                   .offset = offset,
                                   .autoFlip = false,
                                   .orient = TextPath::Orient::Tangent})
                  .opacity(bind(&demo).window(lo, lo + 0.02f)));
    };
    if (n % 6 == 0) {
      const ScaleName& nm = kNames[(size_t)(n / 6)];
      if (nm.line2[0] == '\0') {
        run(nm.line1, nameSt, -capName * 0.5f, "nm" + std::to_string(n));
      } else {
        run(nm.line1, nameSt, capName * 0.5f + 1.0f, "nm" + std::to_string(n));
        run(nm.line2, nameSt, -capName * 1.5f - 1.0f,
            "nm2" + std::to_string(n));
      }
    } else {
      run(std::to_string(n % 6), numSt, -capNum * 0.5f,
          "nu" + std::to_string(n));
    }
    // the cell divider, on the sector boundary
    const float bd = sectorStart(n) * 3.14159265f / 180.0f;
    g.child(box()
                .left(Dim(kC.fX - kRLimbOut))
                .top(Dim(kC.fY - kRLimbOut))
                .width(Dim(2 * kRLimbOut))
                .height(Dim(2 * kRLimbOut))
                .key("div" + std::to_string(n))
                .fill(Fill::none())
                .shape(keyedShape(bd,
                                  [bd](SkSize s) {
                                    const float cx = s.width() * 0.5f,
                                                cy = s.height() * 0.5f;
                                    SkPathBuilder p;
                                    p.moveTo(arrange::onEllipse(
                                        {cx, cy}, {kRLimbIn, kRLimbIn}, bd));
                                    p.lineTo(arrange::onEllipse(
                                        {cx, cy}, {kRLimbOut, kRLimbOut}, bd));
                                    return p.detach();
                                  }))
                .stroke(stroke(0.7f, Fill::color(kRule)))
                .opacity(bind(&demo).window(0.18f, 0.21f)));
  }

  // ---- the index ring: NOT ON THE PLATE ---------------------------
  // Twelve numerals set RADIALLY just outside the medallion, so the +36
  // rule is readable straight off the figure. This is the one thing here
  // that genuinely radiates, which is what Orient::Radial is for.
  for (int i = 0; i < 12; ++i) {
    const int n = i * 6;
    g.child(text(U(std::to_string(n)), mn(10.0f, kRed, 0.3f))
                .key("ix" + std::to_string(n))
                .width(Dim(2 * (kRSweepOut + 11)))
                .height(Dim(2 * (kRSweepOut + 11)))
                .centerAt(kC)
                .onPath(TextPath{.path = rimBaseline(),
                                 .at = (float)n / 72.0f,
                                 .align = TextPath::Align::Center,
                                 .offset = 0.0f,
                                 .autoFlip = false,
                                 .orient = TextPath::Orient::Radial})
                .opacity(bind(&demo).window(0.24f, 0.27f)));
  }

  // ---- beat 2: a diameter rides the wheel -------------------------
  g.child(kit::disc(kC, kRColour)
              .key("diam")
              .fill(Fill::none())
              .shape(diameter())
              .stroke(stroke(1.2f, Fill::color(kRed)))
              .transformOrigin(0.5f, 0.5f)
              .rotate(bind(&demo).window(0.18f, 0.30f).target(0.0f, 180.0f))
              .opacity(bind(&demo).window(0.18f, 0.30f).map(pulses(1))));

  // ---- the "161 years of paper" inset ------------------------------
  {
    const float x0 = 58, y0 = 762, w = 19, h = 21;
    Element ins = box();
    ins.child(
        label("161 YEARS OF PAPER", mn(7.0f, kInk2, 0.5f), x0, y0 - 14, 200));
    for (int i = 0; i < 8; ++i) {
      const int n = i * 9;
      ins.child(at(x0 + (float)i * (w + 1), y0, w, h)
                    .fill(Fill::color(scanned[(size_t)n])));
      ins.child(at(x0 + (float)i * (w + 1), y0 + h + 1, w, h)
                    .fill(Fill::color(corrected[(size_t)n])));
    }
    ins.child(label("scanned / corrected", mn(6.5f, kInk2, 0.2f), x0,
                    y0 + 2 * h + 3, 160));
    ins.opacity(bind(&demo).window(0.26f, 0.29f));
    g.child(std::move(ins));
  }

  // ---- the two constructions, printed --------------------------------
  {
    const std::array<std::pair<std::string, SkColor4f>, 4> lines = {{
        {"§161  " + derivation1 + ";   each scale 5.00 deg", kInk2},
        {kit::formatted(
             "built at ROUGE = %.1f deg — the plate's own composition; the "
             "scan measures ROUGE %.1f, VERT %.1f, delta %.2f",
             kRougeDeg, kScanRouge, kScanVert, v.plateDelta),
         kInk2},
        {kit::formatted("test::coverage over an SkPath REGION: %d/%d of %d · "
                        "endpointDegrees: %zu closed contours, %zu endpoints",
                        v.covUncovered, v.covDoubled, v.covSamples,
                        v.closedContours, v.endpointPoints),
         kInk2},
        {"outer band = the same 72 values as ONE 146-stop sweep gradient · "
         "outer numerals = index n, NOT ON THE PLATE",
         kRed},
    }};
    for (size_t i = 0; i < lines.size(); ++i)
      g.child(label(lines[i].first, mn(7.4f, lines[i].second, 0.15f), 56,
                    864 + (float)i * 11.8f, 760)
                  .opacity(bind(&demo).window(0.26f, 0.29f)));
  }
  return g;
}

auto ChevreulCircle::theQuadrant() -> Element {
  Element g = box();
  g.child(
      label("CHEVREUL'S QUADRANT · §163–§165 · ROUGE, TEN RADII × TWENTY "
            "TONES = 200 CELLS",
            mn(9.0f, kInk, 0.6f), 56, 918, 760));
  const float gw = 10 * (kQCellW + kQGapX) - kQGapX;
  const float gh = 20 * (kQCellH + kQGapY) - kQGapY;

  // column heads
  const std::array<const char*, 10> heads = {{"1/10", "2/10", "3/10", "4/10",
                                              "5/10", "6/10", "7/10", "8/10",
                                              "9/10", "NOIR"}};
  for (int k = 0; k < 10; ++k)
    g.child(centred(heads[(size_t)k], mn(6.5f, kInk2, 0.2f),
                    kQX + (float)k * (kQCellW + kQGapX), 936, kQCellW)
                .opacity(bind(&demo).window(0.80f + 0.012f * (float)k,
                                            0.815f + 0.012f * (float)k)));
  // row numbers, and 15 marked as the normal tone
  for (int t : {1, 5, 10, 15, 20})
    g.child(rightAt(std::to_string(t),
                    t == 15 ? mn(6.5f, kRed, 0) : mn(6.5f, kInk2, 0), 56,
                    kQY + (float)(t - 1) * (kQCellH + kQGapY) - 2.0f, 34));
  g.child(at(kQX - 4, kQY + 14.0f * (kQCellH + kQGapY) - 1, gw + 8, 1)
              .fill(Fill::color(hexColor(0x8E2F26, 0.55f)))
              .opacity(bind(&demo).window(0.93f, 0.95f)));

  g.child(
      at(kQX, kQY, gw, gh)
          .background(styles::dropShadow(hexColor(0x3A352D, 0.22f), {2, 2}, 5))
          .fill(Fill::color(kWell))
          .child(instancing::instances(quadAtlas, quadPool,
                                       instancing::Mode::Live)));

  g.child(label(derivation2 + "   — mixed in LINEAR light, per §164's "
                              "quantities of pigment",
                mn(8.0f, kInk2, 0.2f), 56, kQY + gh + 6, 760));
  g.child(label(
      kit::formatted("instanced: 1 atlas cell, 200 tints, %d/%d colour-exact "
                     "on readback (max channel dev %d)",
                     v.tintExact, v.tintCells, v.tintMaxDev),
      mn(8.0f, v.tintExact == v.tintCells ? kInk2 : kRed, 0.2f), 56,
      kQY + gh + 20, 760));
  return g;
}

auto ChevreulCircle::chordCounter() -> Element {
  const float x0 = 852, y0 = 136, S = 380;
  Element g = box();
  g.child(
      label(counterText, mn(8.0f, kRed, 0.2f), x0 + 10, y0 + S - 32, S - 20));
  g.child(label(kit::formatted("centroid a* %.2f  b* %.2f   ·   mean C* %.1f",
                               v.centA, v.centB, v.meanChroma),
                mn(7.5f, kInk2, 0.2f), x0 + 10, y0 + S - 18, S - 20));
  return g;
}
