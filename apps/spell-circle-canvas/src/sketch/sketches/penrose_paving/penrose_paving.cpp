// A Penrose pavement assembled from computed tiles, decorated edges and stone
// materials.

#include "Tiling.h"

struct PenrosePaving : sketch::Sketch {
  std::vector<Tile> tiles;
  GraniteBank bank;
  Audit audit;

  // One raw progress per sett, in [0,1]. bind() reshapes it at the property
  // it feeds, so the fade and the seating overshoot ride the same `grow`
  // value under different curves instead of needing a vector of Outputs each.
  std::vector<choreograph::Output<float>> grow, arcT;
  choreograph::Output<float> sheen{0};
  double t = 0;
  int generation = -1;

  // --- the deflation vignette ---------------------------------------------
  std::vector<std::vector<Tri>> gens;
  double genArea0 = 0;
  int genAreaFails = 0;

  /** THE VERIFICATION, as one table. `Audit` holds what was measured; this
   *  holds the CLAIMS made about it, and every row's verdict is computed
   *  from the two values it reports — so a line that reads PASS cannot
   *  disagree with the arithmetic printed beside it. */
  measure::Table verdict;

  // -------------------------------------------------------------------------
  // One sett: an absolutely-placed box whose OUTLINE is the rhomb, inset by
  // half the saw-cut joint. No rotate() anywhere — the shape carries its own
  // orientation, so the chamfer's raking light and the granite ramp stay
  // world-fixed across all ten tile orientations for free.

  Element sett(const Tile& t, size_t i) {
    SkPoint outer[4], top[4];
    insetQuad(t.v, kJoint * 0.5f, outer, 2.6f);
    insetQuad(t.v, kJoint * 0.5f + kChamfer, top, 2.6f);

    SkRect bb = SkRect::MakeEmpty();
    bb.setBounds({t.v, 4});
    bb.outset(2.0f, 2.0f);
    const SkPoint org{bb.left(), bb.top()};

    SkPath shape = quadPath(outer, org);

    // The chamfer: four real trapezoids between the sett's outer edge and its
    // top face, each shaded by its own outward normal against ONE sun.
    std::array<SkPoint, 4> lo{}, hi{};
    std::array<float, 4> keyed{};
    for (int e = 0; e < 4; ++e) {
      lo[(size_t)e] = {outer[e].x() - org.x(), outer[e].y() - org.y()};
      hi[(size_t)e] = {top[e].x() - org.x(), top[e].y() - org.y()};
    }
    const SkPoint ctr{t.centre.x() - org.x(), t.centre.y() - org.y()};
    for (int e = 0; e < 4; ++e) {
      const SkPoint p = lo[(size_t)e], q = lo[(size_t)((e + 1) % 4)];
      SkVector n = normv({-(q.y() - p.y()), q.x() - p.x()});
      const SkVector mid{(p.x() + q.x()) * 0.5f - ctr.x(),
                         (p.y() + q.y()) * 0.5f - ctr.y()};
      if (n.x() * mid.x() + n.y() * mid.y() < 0) n = {-n.x(), -n.y()};
      keyed[(size_t)e] = -(n.x() * kSunTo.x() + n.y() * kSunTo.y());
    }

    auto chamfer = [lo, hi, keyed](SkCanvas& c, const PaintContext&) {
      SkPaint p;
      p.setAntiAlias(true);
      for (int e = 0; e < 4; ++e) {
        const int f = (e + 1) % 4;
        SkPathBuilder b;
        b.moveTo(lo[(size_t)e]);
        b.lineTo(lo[(size_t)f]);
        b.lineTo(hi[(size_t)f]);
        b.lineTo(hi[(size_t)e]);
        b.close();
        const float k = keyed[(size_t)e];
        // A sawn granite sett is nearly flush: the arris is a 2 mm pencil
        // chamfer on a 600 mm slab. Read the photograph, not the diagram —
        // what separates two setts there is a hairline, not a bevel.
        if (k >= 0)
          p.setColor4f({1.0f, 0.99f, 0.96f, 0.05f + 0.13f * k}, nullptr);
        else
          p.setColor4f({0.05f, 0.05f, 0.06f, 0.05f - 0.15f * k}, nullptr);
        c.drawPath(b.detach(), p);
      }
    };

    Element e =
        box()
            .left(bb.left())
            .top(bb.top())
            .width(bb.width())
            .height(bb.height())
            .shape(heldPath(shape))
            .fill(bank.get(t.fat ? kRoyalWhite : kKobraGrey, t.seed, t.fat))
            .foreground(Decoration(PaintProgram(chamfer)))
            // the saw cut: a hairline of the joint's own colour just
            // inside the silhouette, so neighbouring setts never fuse
            .stroke(stroke(0.7f, Fill::color(hexColor(0x3D4043, 0.34f)),
                           PathFormat::Align::Inner))
            // one Output, two curves: the fade eases out cubic, the
            // seating overshoots — shaped at the property, not in
            // the tick loop
            .opacity(
                bind(&grow[i]).map(choreograph::easeOutCubic).clamp(0.0f, 1.0f))
            .scale(
                bind(&grow[i]).map(ease::outBack(1.32f)).target(0.52f, 1.0f));

    for (int k = 0; k < 2; ++k) e.child(inlay(t, k, org, i));
    return e;
  }

  // --- one inlaid arc, in the tile's own (ζ_r, ζ_s) frame ------------------
  // The band's cross-section is a RADIAL ramp centred on the arc's own centre
  // of curvature: at constant radius the band is exactly the ring between
  // R−w/2 and R+w/2, so the gradient runs across the band's width and nowhere
  // else. That is the one thing a linear gradient cannot do on a curve.

  Element inlay(const Tile& t, int k, SkPoint parentOrg, size_t i) {
    const ArcSpec spec = arcAt(t, k, kModule);
    const SkPath world = arcPath(spec, kModule, kJoint * 0.5f + 0.9f);
    SkRect bb = world.getBounds();
    const float pad = kBandW * 0.5f + 2.6f;
    bb.outset(pad, pad);

    const SkPath local =
        world.makeTransform(SkMatrix::Translate(-bb.left(), -bb.top()));
    const SkPoint c{spec.centre.x() - bb.left(), spec.centre.y() - bb.top()};

    const float R = kModule * 0.5f;
    const float gR = R + kBandW;
    const float inner = (R - kBandW * 0.5f) / gR;
    const float outer = (R + kBandW * 0.5f) / gR;
    // A POLISHED INSERT IS FLAT. The bevel is a rim, not a section: the
    // face holds one tone across the middle three quarters of the band and
    // the edge tone is confined to the two millimetres either side of it.
    // Run across the whole width, the same ramp reads as piping with a
    // dark core rather than as a broad flat ribbon.
    const float rim = (outer - inner) * 0.12f;
    Fill band =
        radialGradient(c, gR,
                       {kSteelEdge, kSteelEdge, kSteelBase, kSteelSpec,
                        kSteelBase, kSteelEdge, kSteelEdge},
                       {0.0f, inner, inner + rim, (inner + outer) * 0.5f,
                        outer - rim, outer, 1.0f});

    return box()
        .left(bb.left() - parentOrg.x())
        .top(bb.top() - parentOrg.y())
        .width(bb.width())
        .height(bb.height())
        .shape(heldPath(local))
        .stroke(spans::upTo(&arcT[i]),
                Brush{}  // the milled slot the insert sits in — a hairline of
                         // occlusion either side, not an outline
                    .layer(PathFormat{.width = kBandW + 0.9f,
                                      .strokeFill = Fill::color(kGroove)})
                    // the 30 mm polished insert
                    .layer(PathFormat{.width = kBandW, .strokeFill = band}));
  }

  // -------------------------------------------------------------------------
  // The deflation vignette — the OTHER construction, rendered flat so it
  // reads as the diagram beside the paving rather than as more paving.

  Element diagram(int gen) {
    const std::vector<Tri>& tri = gens[(size_t)std::clamp(gen, 0, 3)];
    // positioned(): every triangle carries its own bb rect — the
    // deflation patch is the field's pattern in miniature, Yoga-free.
    auto group = positioned()
                     .inset(0, 0, 0, 0)
                     .key("gen" + std::to_string(gen))
                     .staggerChildren(9ms, motion::Spread::From::Center)
                     .transformOrigin(0.5f, 0.5f)
                     .scale(animate(from(0.94f).to(1.0f),
                                    Transition{320ms, ease::outBack(1.1f)}));

    for (size_t i = 0; i < tri.size(); ++i) {
      const Tri& src = tri[i];
      // Two antialiased half-triangles sharing a rhomb diagonal leave a
      // hairline seam along it, which makes the whole diagram read as
      // TRIANGLES instead of rhombs — the one thing it must not say. Push
      // each half 0.45 px out from its own centroid so the halves overlap.
      Tri g = src;
      {
        const float cx = (src.a.x() + src.b.x() + src.c.x()) / 3.0f;
        const float cy = (src.a.y() + src.b.y() + src.c.y()) / 3.0f;
        for (SkPoint* q : {&g.a, &g.b, &g.c}) {
          const SkVector d = normv({q->x() - cx, q->y() - cy});
          *q = {q->x() + d.x() * 0.8f, q->y() + d.y() * 0.8f};
        }
      }
      SkPoint pts[3] = {g.a, g.b, g.c};
      SkRect bb = SkRect::MakeEmpty();
      bb.setBounds({pts, 3});
      bb.outset(1, 1);
      SkPathBuilder b;
      b.moveTo(g.a.x() - bb.left(), g.a.y() - bb.top());
      b.lineTo(g.b.x() - bb.left(), g.b.y() - bb.top());
      b.lineTo(g.c.x() - bb.left(), g.c.y() - bb.top());
      b.close();
      SkPath p = b.detach();
      group.child(
          box()
              .key("g" + std::to_string(gen) + "_" + std::to_string(i))
              .left(bb.left())
              .top(bb.top())
              .width(bb.width())
              .height(bb.height())
              .shape(heldPath(p))
              .fill(Fill::color(g.type == 1 ? hexColor(0xB6B2A7)
                                            : hexColor(0x76797E)))
              // NO per-piece scale: scaling each half about its own
              // centre pulls a subdivision apart, and a deflation
              // diagram that shows gaps is saying the opposite of
              // what it is for. The enclosing group takes the
              // entrance transform instead.
              .opacity(animate(from(0.0f).to(1.0f),
                               Transition{260ms, choreograph::easeOutQuad})));
    }

    // The rhomb outlines: every triangle is run b→a→c and left OPEN, so the
    // b→c edge is never drawn — that edge is the diagonal each pair of halves
    // shares, so a Robinson triangle's two real rhomb edges are exactly the
    // two this run does draw. Close the path and the diagram claims a tiling
    // by triangles, which is the one thing it must not say.
    // The generation names the drawing: a generation's triangles are
    // settled before it is described, so the program is a value the
    // node can compare rather than a callable that never matches.
    auto edges = tri;
    group.child(custom(kit::formatted("rhomb-edges-%d", gen),
                       [edges](SkCanvas& c, const PaintContext&) {
                         SkPaint p;
                         p.setAntiAlias(true);
                         p.setStyle(SkPaint::kStroke_Style);
                         p.setStrokeWidth(1.0f);
                         p.setColor4f(hexColor(0x1B1D1E, 0.85f), nullptr);
                         for (const Tri& g : edges) {
                           SkPathBuilder b;
                           b.moveTo(g.b);
                           b.lineTo(g.a);
                           b.lineTo(g.c);
                           c.drawPath(b.detach(), p);
                         }
                       })
                    .inset(0, 0, 0, 0)
                    .cache(Cache::None));
    return group;
  }

  /** THE VERIFICATION, ENGRAVED BESIDE THE PLAQUE. A dualization that is
   *  subtly wrong still renders a plausible field of rhombs, so the tiling
   *  is checked numerically before any of the surface treatment is worth
   *  looking at — and a check nobody can see is a check nobody reads. Each
   *  row's verdict is computed from the two values it reports; the ratio of
   *  the two prototiles is a statement about the PAVING and stands apart
   *  from the claims about the construction. */
  Element verificationCard() const {
    sketch::kit::Theme look;
    look.palette.ash = kCaption;
    look.palette.figure = hexColor(0xDCE0E2);
    look.type.captionNote = {9.5f, 0.3f};
    look.type.captionLabel = {9.5f, 0.3f, true};
    look.spacing.rowGap = 4;
    const SkColor4f held = hexColor(0x7FA87F), broken = hexColor(0xC0564B);
    std::vector<sketch::kit::Row> rows;
    for (const measure::Check& c : verdict.rows) {
      if (c.standing == measure::Standing::Heading) {
        rows.push_back({{toUtf8(c.label)}, {}});
        continue;
      }
      // A reading has no verdict to mark, but it keeps the mark's WIDTH:
      // an unmarked row that also loses the indent reads as a heading.
      if (!c.judged()) {
        rows.push_back({{toUtf8(c.label), toUtf8(c.actual), u8""},
                        Fill::color({0, 0, 0, 0})});
        continue;
      }
      rows.push_back(
          {{toUtf8(c.label), toUtf8(c.actual),
            toUtf8(c.pass ? std::string("PASS") : "FAIL want " + c.expected)},
           Fill::color(c.pass ? held : broken)});
    }
    const std::string summary = kit::formatted(
        "VERIFIED AT STARTUP \xc2\xb7 %d CHECKS, %s", verdict.checks(),
        verdict.pass() ? "ALL PASSED" : "ONE OR MORE FAILED");
    sketch::kit::Provide bound(look);
    return box()
        .left(1096)
        .top(944)
        .width(448)
        .height(236)
        .fill(Fill::color(hexColor(0x121517, 0.84f)))
        .stroke(stroke(1.0f, Fill::color(hexColor(0x5E6163, 0.55f)),
                       PathFormat::Align::Inner))
        .background(styles::dropShadow(hexColor(0x000000, 0.55f), {0, 6}, 22))
        .column()
        .padding(14)
        .gap(9)
        .child(text(
            toUtf8(summary),
            weave::textStyle(
                {.size = 10.5f, .color = hexColor(0x8E9295), .track = 1.0f})))
        .child(sketch::kit::table(
            std::move(rows),
            {.columns = {{202}, {92, true}, {}}, .gap = 8, .swatchSide = 7}));
  }

  Element inset() {
    const SkRect r = SkRect::MakeXYWH(1178, 76, 348, 268);
    return box()
        .left(r.left())
        .top(r.top())
        .width(r.width())
        .height(r.height())
        .fill(Fill::color(hexColor(0x121517, 0.84f)))
        .stroke(stroke(1.0f, Fill::color(hexColor(0x5E6163, 0.55f)),
                       PathFormat::Align::Inner))
        .background(styles::dropShadow(hexColor(0x000000, 0.55f), {0, 6}, 22))
        .child(
            text(toUtf8("DEFLATION \xc2\xb7 FAT \xe2\x86\x92 2 FAT + 1 THIN, "
                        "\xc3\x97"
                        "1/\xcf\x86"),
                 weave::textStyle({.size = 10.5f,
                                   .color = hexColor(0x8E9295),
                                   .track = 1.0f}))
                .left(14)
                .top(12))
        .child(box().left(10).top(34).width(kDiagW).height(kDiagH).child(
            slot("deflate")));
  }

  // -------------------------------------------------------------------------

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    // A positioned leaf set: every sett already carries its own computed
    // rect, so there is nothing for a layout pass to solve. Under a plain
    // box() each sett and its two inlays would mount three flex nodes;
    // positioned() mounts them with none.
    auto field = positioned().inset(0, 0, 0, 0);
    for (size_t i = 0; i < tiles.size(); ++i) field.child(sett(tiles[i], i));

    const std::string spec = kit::formatted(
        "DE BRUIJN PENTAGRID  \xce\xb3=1/5 (\xce\x93=0)  s=%.0f px  "
        "%d SETTS  FAT:THIN = %.3f  (\xcf\x86 = 1.618)",
        kModule, audit.tiles, audit.ratio);

    return stack()
        .fill(Fill::color(kJointBed))
        // the bedding course showing through the saw cuts
        // A procedural grain evaluated over every pixel of the canvas, and
        // the most expensive node in the frame by a wide margin. Nothing it
        // depends on animates, so Cache::Texture bakes it once and the cache
        // never invalidates; the node's opacity is what keeps it out of the
        // automatic bake, so the cache has to be asked for by hand.
        .child(
            box()
                .inset(0, 0, 0, 0)
                .fill(Paint::recipe(field::grain(0.9f, 1, 12.0f, 0.55f, 1.0f)))
                .opacity(0.20f)
                .cache(Cache::Texture))
        .child(field)
        // Weathering at PLAZA scale — cells a couple of hundred px across,
        // i.e. metres of traffic staining that crosses joints because dirt
        // does not know where the setts are. Baked: it never changes, and a
        // two-octave grain over the whole canvas is not worth re-evaluating
        // once a frame to get the same pixels back.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kMultiply)
                   .opacity(0.42f)
                   .cache(Cache::Texture)
                   .fill(Paint::blend({{Paint::solid(hexColor(0xFFFFFF)),
                                        SkBlendMode::kSrcOver},
                                       {Paint::recipe(field::grain(
                                            0.0042f, 2, 91.0f, 0.62f, 1.15f)),
                                        SkBlendMode::kSoftLight}})))
        // ---- daylight. One multiply pass carries the sun's falloff across
        // the plaza. It is SHALLOW: the header calls this a plan view and
        // the forecourt is photographed in flat daylight, so a key bright
        // enough to be read as a studio light is reading as a light rather
        // than as a floor.
        // Static, so it is baked: nothing here depends on the clock and the
        // gradient covers the whole canvas.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kMultiply)
                   .cache(Cache::Texture)
                   .fill(radialGradient({470, 280}, 1280,
                                        {hexColor(0xFAFAF8), hexColor(0xE6E6E4),
                                         hexColor(0xB2B4B8), hexColor(0x74777C),
                                         hexColor(0x42454A)},
                                        {0.0f, 0.22f, 0.50f, 0.78f, 1.0f})))
        // the sun pool itself, added back — also static, baked for the same
        // reason as the pass above
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kPlus)
                   .opacity(0.5f)
                   .cache(Cache::Texture)
                   .fill(radialGradient(
                       {470, 280}, 1100,
                       {hexColor(0xFFF8E8, 0.13f), hexColor(0xFFF3DA, 0.075f),
                        hexColor(0xFFF0D0, 0.025f), hexColor(0x000000, 0.0f)},
                       {0.0f, 0.34f, 0.68f, 1.0f})))
        // wet-stone sheen — a broad, low raking band that sweeps once per
        // loop as the arcs finish, so the field reads as a wet surface
        // catching the sky rather than as flat fill
        .child(box()
                   .inset(0, 0, 0, 0)
                   .blend(SkBlendMode::kScreen)
                   .opacity(&sheen)
                   .fill(linearGradient(
                       {180, 0}, {1500, 1200},
                       {hexColor(0x000000, 0.0f), hexColor(0xBFD2E0, 0.09f),
                        hexColor(0x000000, 0.0f)},
                       {0.30f, 0.50f, 0.72f})))
        .child(inset())
        // ---- the site plaque. A civic plaque sits on the paving, so give
        // it a shadowed band to sit in rather than dropping 10 px type onto
        // speckled granite where it cannot be read at any exposure.
        .child(box().left(0).top(kH - 190).width(kW).height(190).fill(
            linearGradient({0, kH - 190}, {0, kH},
                           {hexColor(0x000000, 0.0f), hexColor(0x08090A, 0.42f),
                            hexColor(0x08090A, 0.72f)},
                           {0.0f, 0.5f, 1.0f})))
        .child(box()
                   .left(56)
                   .top(1084)
                   .width(1010)
                   .height(96)
                   .fill(Fill::color(hexColor(0x101314, 0.90f)))
                   .stroke(stroke(1.0f, Fill::color(hexColor(0x676B6D, 0.45f)),
                                  PathFormat::Align::Inner))
                   .background(styles::dropShadow(hexColor(0x000000, 0.5f),
                                                  {0, 5}, 18)))
        .child(text(toUtf8("PENROSE TILING \xc2\xb7 P3 RHOMBI \xc2\xb7 ROYAL "
                           "WHITE & KOBRA GREY GRANITE \xc2\xb7 POLISHED 30 mm "
                           "STAINLESS INSERTS"),
                    weave::textStyle({.size = 13.0f,
                                      .color = hexColor(0xDCE0E2),
                                      .track = 1.9f}))
                   .left(76)
                   .top(1100)
                   .opacity(1.0f))
        .child(text(toUtf8("MATHEMATICAL INSTITUTE, ANDREW WILES BUILDING, "
                           "OXFORD \xc2\xb7 R. PENROSE 1974 / PAVING 2012"),
                    weave::textStyle({.size = 11.5f,
                                      .color = hexColor(0xA9AEB1),
                                      .track = 1.5f}))
                   .left(76)
                   .top(1126)
                   .opacity(1.0f))
        .child(text(toUtf8(spec), weave::textStyle({.size = 10.5f,
                                                    .color = hexColor(0x8E9598),
                                                    .track = 1.3f}))
                   .left(76)
                   .top(1152)
                   .opacity(1.0f))
        .child(verificationCard());
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    // The finished plaza. 0.75 catches the crystal front mid-growth over the
    // exact five-fold centre, 1.45 the inlay chaining on.
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 4.6, .background = kNight});

    tiles = buildField(kModule, kModule * 1.2f);
    audit = verify(tiles, kModule);

    verdict = {};
    verdict.add(measure::heading("THE PAVING"));
    verdict.add(measure::reading(
        kit::formatted("setts  %d fat + %d thin", audit.fat, audit.thin),
        (long)audit.tiles));
    // The ratio of the two prototiles tends to phi over the WHOLE tiling; a
    // finite patch of a few hundred setts only approaches it, so this is a
    // statement about the paving rather than about the dualization that
    // drew it, and its verdict is never counted against the run.
    verdict.add(measure::finding(measure::check("fat : thin, tends to \xcf\x86",
                                                1.6180, audit.ratio, 0.02)));
    verdict.add(
        measure::reading("interior vertices", (long)audit.interiorVerts));
    verdict.add(measure::check("vertices not closing at 360\xc2\xb0", 0,
                               audit.badVerts));
    verdict.add(measure::check("worst angle-sum error, deg", 0.0,
                               audit.worstVertErr, 0.5));
    verdict.add(measure::check("arc endpoints left unchained", 0,
                               audit.danglingInterior));
    verdict.add(measure::check("worst endpoint off its midpoint, px", 0.0,
                               audit.worstMidErr, 1e-3));
    verdict.add(measure::check("worst arc tangent \xc2\xb7 its edge", 0.0,
                               audit.worstTangentErr, 1e-4));

    // --- the deflation vignette's own construction + area audit ------------
    {
      // Seed: ONE fat rhomb = two obtuse Robinson halves mirrored across its
      // long diagonal (which joins the 72° corners and has length φ·s).
      const double a0 = -0.9424777961;  // −54°: the first edge's direction
      const glm::dvec2 u{std::cos(a0), std::sin(a0)};
      const glm::dvec2 v{std::cos(a0 + 1.2566370614),
                         std::sin(a0 + 1.2566370614)};
      auto P = [&](glm::dvec2 p) { return SkPoint{(float)p.x, (float)p.y}; };
      const SkPoint V0 = P({0, 0}), V1 = P(u), V2p = P(u + v), V3 = P(v);
      gens.clear();
      gens.push_back({{1, V1, V0, V2p}, {1, V3, V2p, V0}});
      genArea0 = 0;
      for (const Tri& t : gens[0]) genArea0 += triArea(t);
      genAreaFails = 0;
      int genOutside = 0;
      for (int g = 0; g < 3; ++g) {
        const std::vector<Tri> parents = gens.back();
        gens.push_back(deflate(parents));
        double area = 0;
        for (const Tri& t : gens.back()) area += triArea(t);
        if (std::abs(area - genArea0) > genArea0 * 1e-4) genAreaFails++;
        // every child must lie inside SOME parent — equal areas alone would
        // happily accept a correctly-sized child dropped in the wrong place
        for (const Tri& ch : gens.back()) {
          bool ok = false;
          for (const Tri& pa : parents)
            if (insideTri(pa, ch.a, 1e-6) && insideTri(pa, ch.b, 1e-6) &&
                insideTri(pa, ch.c, 1e-6)) {
              ok = true;
              break;
            }
          if (!ok) genOutside++;
        }
      }
      // Deflation subdivides the SAME region, so one fit computed on the
      // seed lands every generation: the patch holds still while its tiles
      // get finer by 1/φ — the shrink-expand view, read the other way round.
      SkRect bb = SkRect::MakeEmpty();
      {
        SkPoint pts[4] = {V0, V1, V2p, V3};
        bb.setBounds({pts, 4});
      }
      const float m = 16.0f;
      const float k = std::min((kDiagW - 2 * m) / bb.width(),
                               (kDiagH - 2 * m) / bb.height());
      const float ox = (kDiagW - bb.width() * k) * 0.5f - bb.left() * k;
      const float oy = (kDiagH - bb.height() * k) * 0.5f - bb.top() * k;
      for (auto& gen : gens)
        for (Tri& tr : gen)
          for (SkPoint* q : {&tr.a, &tr.b, &tr.c})
            *q = {q->x() * k + ox, q->y() * k + oy};

      // Point-sampled coverage: the audit that actually sees a hole. Area
      // sums and containment both pass on a subdivision that overlaps here
      // and gaps there.
      {
        SkRect bb0 = SkRect::MakeEmpty();
        {
          SkPoint pts[6] = {gens[0][0].a, gens[0][0].b, gens[0][0].c,
                            gens[0][1].a, gens[0][1].b, gens[0][1].c};
          bb0.setBounds({pts, 6});
        }
        int uncovered = 0, doubled = 0, inside = 0;
        for (int iy = 0; iy < 120; ++iy)
          for (int ix = 0; ix < 120; ++ix) {
            const SkPoint q{
                bb0.left() + bb0.width() * ((float)ix + 0.5f) / 120,
                bb0.top() + bb0.height() * ((float)iy + 0.5f) / 120};
            int inSeed = 0;
            for (const Tri& t : gens[0])
              if (insideTri(t, q, -1e-4)) inSeed++;
            if (inSeed == 0) continue;
            inside++;
            int n = 0;
            for (const Tri& t : gens[3])
              if (insideTri(t, q, -1e-4)) n++;
            if (n == 0)
              uncovered++;
            else if (n > 1)
              doubled++;
          }
        verdict.add(measure::heading("THE DEFLATION"));
        verdict.add(measure::reading(
            "rhombs, seed \xe2\x86\x92 gen 3",
            kit::formatted("%zu \xe2\x86\x92 %zu \xe2\x86\x92 %zu "
                           "\xe2\x86\x92 %zu",
                           gens[0].size() / 2, gens[1].size() / 2,
                           gens[2].size() / 2, gens[3].size() / 2)));
        verdict.add(
            measure::check("generations off the seed's area", 0, genAreaFails));
        verdict.add(
            measure::check("children outside their parent", 0, genOutside));
        verdict.add(measure::reading("seed samples", (long)inside));
        verdict.add(measure::check("of them uncovered at gen 3", 0, uncovered));
        verdict.add(measure::check("of them double-covered", 0, doubled));
      }
    }

    grow = std::vector<choreograph::Output<float>>(tiles.size());
    arcT = std::vector<choreograph::Output<float>>(tiles.size());
    for (size_t i = 0; i < tiles.size(); ++i) {
      grow[i] = 0.0f;
      arcT[i] = 0.0f;
    }
    t = 0;
    generation = -1;

    ctx.ticker.add([this](double dt) {
      t += dt;
      const double now = std::fmod(t, kPeriod);
      for (size_t i = 0; i < tiles.size(); ++i) {
        const double u = (double)tiles[i].radius / (double)kCorner;
        // the ripple front: a raw linear progress, nothing shaped here
        grow[i] = (float)std::clamp(
            (now - (kTileT0 + kTileSweep * u)) / kTileDur, 0.0, 1.0);
        arcT[i] = choreograph::easeOutCubic((float)std::clamp(
            (now - (kArcT0 + kArcSweep * u)) / kArcDur, 0.0, 1.0));
      }
      const float sp = (float)std::clamp((now - kSheen0) / kSheenDur, 0.0, 1.0);
      sheen = std::sin(sp * 3.14159265f);  // one pass, then gone
      return true;
    });

    ctx.composer.render(describe(ctx));
    generation = 0;
    ctx.composer.renderSlot("deflate", diagram(0));
  }

  void update(double, sketch::SketchContext& ctx) override {
    const double now = std::fmod(t, kPeriod);
    int g = 0;
    for (int i = 3; i >= 0; --i)
      if (now >= kGenAt[i]) {
        g = i;
        break;
      }
    if (now < kGenAt[0]) g = 0;
    if (g != generation) {
      generation = g;
      // Only the vignette's mount point re-renders. The paving's setts and
      // their baked caches are outside this slot and are not re-described.
      ctx.composer.renderSlot("deflate", diagram(g));
    }
  }
};

SIGIL_SKETCH(
    PenrosePaving, "Study \xc2\xb7 Pattern",
    "Penrose's 2012 P3 paving, Oxford \xe2\x80\x94 549 setts from de Bruijn's "
    "pentagrid")
