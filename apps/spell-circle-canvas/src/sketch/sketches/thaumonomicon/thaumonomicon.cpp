// A research web laid over parchment, stamped connectors and brass controls.

#include "ResearchArt.h"

struct Thaumonomicon : sketch::Sketch {
  // ---- motion --------------------------------------------------------------
  ch::Output<float> pulse{0};   // the 600 ms lockstep unlockable pulse
  ch::Output<float> spin{0};    // drawForbidden's swirl
  ch::Output<float> driftX{0};  // locX, in GUI px
  ch::Output<float> driftY{0};

  // ---- baked art (pointer-stable, so every brush bakes exactly once) -------
  std::array<Element, 3> spatter;
  std::array<Element, 3> knot;
  std::array<Element, 3> straight;
  // [tier][2*big + (handed>0)] — small/big elbow, left/right mirror
  std::array<std::array<Element, 4>, 3> elbows;
  Element runTile, cornerTile;
  /** One sprite per reconstructed glyph, held by its `Glyph` id. The art is
   *  a value, so a node's icon is a lookup rather than a switch run inside
   *  a paint program on every describe. */
  std::vector<kit::Sprite> glyphs;

  // ---- baked type ----------------------------------------------------------
  sk_sp<SkTypeface> face;
  PixText tipTitle, tipMissing, tipParent;

  static sk_sp<SkTypeface> systemFace() {
    return weave::ports::face({"Menlo", "Monaco", "Courier New", "Helvetica"});
  }

  // -------------------------------------------------------------------------
  // The backdrop: two planes, translated at the source's 2.0 : 1.5 ratio. Both
  // are STATIC generated art baked with Cache::Texture — the parallax is a
  // bound transform, which is paint-only volatility, so the bakes replay under
  // it and no shader re-runs per frame.

  static Element backdropBase() {
    const float w = g(kScreenX + 4 + 44), h = g(kScreenY + 4 + 44);
    Element e = box().left(g(-22)).top(g(-22)).width(w).height(h).fill(
        // THE BROWSER LOOKS INTO A NEBULA. The mod's own backdrop fills
        // the inner area with cloud and star, and the two-plane parallax
        // this file transcribes has nothing to carry over a flat brown
        // ground — a 2.0 : 1.5 depth ratio only reads when the two planes
        // hold structure the eye can follow.
        Paint::radialUnit({0.44f, 0.38f}, 1.20f,
                          {{0.00f, hexColor(0x6E2A72)},
                           {0.22f, hexColor(0x4A1A56)},
                           {0.46f, hexColor(0x2A1036)},
                           {0.74f, hexColor(0x140A1C)},
                           {1.00f, hexColor(0x060309)}}));
    // The painted plate under it: an alchemical wheel, a ruled margin, and
    // washes — the structure a photographed grimoire page carries and a noise
    // field never will.
    e.overlay(prog([](SkCanvas& c, const PaintContext& in) {
      SkPaint p;
      p.setAntiAlias(true);
      const SkPoint o{in.size.width() * 0.50f, in.size.height() * 0.47f};
      // washes first, so the linework sits on top of them
      // The cloud: warm lobes over the ramp and cold ones under it, so
      // the field has structure rather than a single falloff.
      for (int i = 0; i < 46; ++i) {
        const float x = (0.5f + 0.5f * noise1(i, 1, 5)) * in.size.width();
        const float y = (0.5f + 0.5f * noise1(i, 2, 5)) * in.size.height();
        const float r = g(26.0f + 72.0f * (0.5f + 0.5f * noise1(i, 3, 5)));
        p.setColor4f((i % 2 != 0) ? hexColor(0xB03CC0, 0.055f)
                                  : hexColor(0x120618, 0.20f),
                     nullptr);
        c.drawCircle(x, y, r, p);
      }
      // …and the star field over it. Two magnitudes, seeded, so the near
      // plane's drift is something to watch.
      for (int i = 0; i < 260; ++i) {
        const float x = (0.5f + 0.5f * noise1(i, 11, 7)) * in.size.width();
        const float y = (0.5f + 0.5f * noise1(i, 12, 7)) * in.size.height();
        const float m = 0.5f + 0.5f * noise1(i, 13, 7);
        p.setColor4f(hexColor(0xF2E4FF, 0.16f + 0.60f * m * m), nullptr);
        c.drawCircle(x, y, g(m > 0.86f ? 1.3f : 0.7f), p);
      }
      p.setStyle(SkPaint::kStroke_Style);
      // the wheel: four rules and three rings, plus a 72-tick limb
      for (int i = 0; i < 6; ++i) {
        p.setStrokeWidth(g(i % 3 == 0 ? 2.0f : 0.9f));
        p.setColor4f(
            mskia::scale(kBrassLit, 1.0f, 0.13f + 0.05f * (float)(i % 3)),
            nullptr);
        c.drawCircle(o.fX, o.fY, g(46.0f + (float)i * 30.0f), p);
      }
      p.setStrokeWidth(g(1.0f));
      p.setColor4f(mskia::scale(kBrassLit, 1.0f, 0.17f), nullptr);
      SkPathBuilder t;
      for (int i = 0; i < 72; ++i) {
        const float r0 = g(i % 6 == 0 ? 182.0f : 192.0f), r1 = g(200.0f);
        t.moveTo(arrange::onRing((size_t)i, 72, o, {r0, r0}, 0.0f, 6.2831853f,
                                 arrange::Turn::Closed));
        t.lineTo(arrange::onRing((size_t)i, 72, o, {r1, r1}, 0.0f, 6.2831853f,
                                 arrange::Turn::Closed));
      }
      // an inscribed pentagram and a hexagram, the plate's own furniture
      for (int k = 0; k < 2; ++k) {
        const int n = k ? 6 : 5;
        const float rad = g(k ? 106.0f : 166.0f);
        const float rot = k ? 0.26f : -1.57f;
        for (int i = 0; i < n; ++i) {
          const int j = (i + 2) % n;
          t.moveTo(arrange::onRing((size_t)i, (size_t)n, o, {rad, rad}, rot,
                                   6.2831853f, arrange::Turn::Closed));
          t.lineTo(arrange::onRing((size_t)j, (size_t)n, o, {rad, rad}, rot,
                                   6.2831853f, arrange::Turn::Closed));
        }
      }
      c.drawPath(t.detach(), p);
      // strata: long diagonal scrapes across the plate
      p.setColor4f(hexColor(0xD8C08A, 0.055f), nullptr);
      SkPathBuilder s2;
      for (int i = 0; i < 22; ++i) {
        const float y0 = (0.5f + 0.5f * noise1(i, 9, 4)) * in.size.height();
        p.setStrokeWidth(g(0.6f + 1.8f * (0.5f + 0.5f * noise1(i, 8, 4))));
        s2.moveTo(-g(20), y0);
        s2.lineTo(in.size.width() + g(20), y0 + g(46.0f * noise1(i, 7, 4)));
      }
      c.drawPath(s2.detach(), p);
    }));
    // Paper tooth: the luminance grain, held back so it shades instead of
    // shouting. Inside the Cache::Texture bake, so its pixels are paid once.
    e.child(box()
                .inset(0)
                .opacity(0.42f)
                .blend(SkBlendMode::kOverlay)
                .fill(Paint::recipe(field::grain(0.06f, 5, 3.0f))));
    return e;
  }

  static Element backdropOver() {
    const float w = g(kScreenX + 4 + 44), h = g(kScreenY + 4 + 44);
    return box()
        .left(g(-22))
        .top(g(-22))
        .width(w)
        .height(h)
        .blend(SkBlendMode::kScreen)
        .opacity(0.34f)
        .fill(Paint::blend({{Paint::recipe(field::grain(0.0075f, 4, 11.0f)),
                             SkBlendMode::kSrc},
                            {Paint::radialUnit({0.5f, 0.5f}, 1.0f,
                                               {{0.0f, hexColor(0xFFFFFF)},
                                                {0.7f, hexColor(0x808080)},
                                                {1.0f, hexColor(0x000000)}}),
                             SkBlendMode::kMultiply}}));
  }

  // -------------------------------------------------------------------------
  // One edge: a connector on the transcribed router, dressed with ONE stock
  // brush::Pattern — 24x24 side tiles, a 24x24 or 48x48 corner tile, and
  // cornerLength reserving the elbow's own room so the side run butts against
  // it instead of continuing underneath. Nothing here is a stroke.

  Element edgeEl(const Edge& e, int order) const {
    const Node& child = nodeByKey(e.child);
    const Node& parent = nodeByKey(e.parent);
    const RouteShape shape = shapeOf(child, parent, e.flipped);
    const int t = (int)e.tier;
    const int elbow = !shape.hasCorner ? -1
                                       : (shape.bigCorner ? 2 : 0) +
                                             (shape.handed > 0 ? 1 : 0);

    brush::Pattern pb{.side = straight[t],
                      .advance = kCell,
                      .cornerAngleDeg = 35.0f,
                      .stretchToFit = true,
                      .bleedPx = g(56)};
    if (elbow >= 0) {
      // An elbow of PIPE, not an ornament: entry, exit and a handedness, and
      // elbowTile() authors it with local +x along the outgoing leg. On the
      // bisector this art stamps 45 degrees off — and a 2x2 route is all
      // corner and no side tiles, so the whole edge becomes a chevron. The
      // alignment therefore travels with the art rather than being defaulted.
      pb.corner = brush::CornerArt{elbows[t][(size_t)elbow],
                                   brush::CornerAlign::Outgoing};
      pb.cornerLength = 2.0f * cornerArm(shape.bigCorner);
    }
    Brush br;
    br.layer(std::move(pb));

    // A REVERSE edge walks from the parent: the connector's own from/to
    // is the walk's direction, and the route bends at the START's column.
    return connector(e.flipped ? parent.key : child.key,
                     e.flipped ? child.key : parent.key, thaumRoute())
        .inset(0)
        .key(std::string("edge:") + child.key + "<" + parent.key)
        .zIndex(tierZ(e.tier))
        .stroke(
            spans::upTo(animate(
                from(0.0f).to(1.0f),
                Transition{.duration = 620ms,
                           .ease = ch::easeOutQuad,
                           .delay = std::chrono::milliseconds(60 * order)})),
            br);
  }

  Element arrowEl(const Edge& e) const {
    const Node& child = nodeByKey(e.child);
    const Node& parent = nodeByKey(e.parent);
    const int dy = child.row - parent.row, dx = child.col - parent.col;
    // The arrowhead sits on the walk's leg AT THE CHILD. An ordinary edge
    // leaves the child vertically (the bend is at the child's column); a
    // REVERSE edge starts at the parent instead, so it arrives at the child
    // along the HORIZONTAL leg (the bend is at the parent's column) and the
    // arrowhead has to ride that leg or it lands on empty canvas.
    SkVector travel{0, 0};
    if (!e.flipped) {
      if (dy != 0)
        travel = {0, dy > 0 ? 1.0f : -1.0f};
      else if (dx != 0)
        travel = {dx > 0 ? 1.0f : -1.0f, 0};
    } else {
      if (dx != 0)
        travel = {dx > 0 ? 1.0f : -1.0f, 0};
      else if (dy != 0)
        travel = {0, dy > 0 ? 1.0f : -1.0f};
    }
    if (travel.fX == 0 && travel.fY == 0) return box().width(0).height(0);
    const SkPoint c = centreOf(child.col, child.row);
    const SkColor4f tint = tierTint(e.tier, kInkBody);
    return arrowCell(tint)
        .centerAt({c.fX - travel.fX * g(20), c.fY - travel.fY * g(20)})
        .rotate(std::atan2(travel.fY, travel.fX) * 57.29578f)
        .zIndex(tierZ(e.tier));
  }

  // -------------------------------------------------------------------------

  Element nodePlate(const Node& n) const {
    const SkPoint c = centreOf(n.col, n.row);
    // Cache::Texture, not the automatic picture. A plate is a torn-square
    // outline + a radial + an Sk2D hatch + a sketchy double rule, and a
    // PICTURE replays all of those path effects on every frame; baked to a
    // texture each plate is one blit. The pulsing plates keep their bake too,
    // because a bound opacity is paint-only volatility.
    Element wrap = box()
                       .left(c.fX - g(16))
                       .top(c.fY - g(16))
                       .width(g(32))
                       .height(g(32))
                       .key(n.key);
    // :598 culls the whole node, art and all, but drawLine still draws its
    // edges — so a culled node keeps its KEYED, empty box (the connector
    // still needs somewhere to route to) and paints nothing.
    if (culled(n.col, n.row)) return wrap;
    wrap.cache(Cache::Texture);
    if (n.state == kUnlockable)
      wrap.opacity(bind(&pulse).target(0.5f, 1.0f));
    else if (n.state == kLocked)
      wrap.opacity(0.3f);

    const uint32_t seed = (uint32_t)(n.col * 31 + n.row * 17 + 101);
    wrap.child(plateArt(n.meta, seed, spatter[0]).inset(g(2)));
    if (n.meta & kSpiky) wrap.child(spikyOverlay(seed + 7));
    wrap.child(iconEl(glyphs[(size_t)n.icon], n.state == kLocked ? 0.6f : 1.0f,
                      n.state == kLocked)
                   .left(g(8))
                   .top(g(8)));
    return wrap;
  }

  Element nodeBadges(const Node& n) const {
    const SkPoint c = centreOf(n.col, n.row);
    // The source's -9/+9 badge offsets are measured from the ICON'S TOP-LEFT
    // (iconX, iconY), which sits 8 GUI px up-left of centreOf(); the + terms
    // then step from each badge cell's top-left to the art's own centre for
    // centerAt().
    const float tlx = c.fX - g(8), tly = c.fY - g(8);
    Element g0 = box().inset(0);
    if (culled(n.col, n.row)) return g0;
    if (n.flagResearch)
      g0.child(
          researchBadge().centerAt({tlx - g(9) + g(8), tly - g(9) + g(8)}));
    if (n.flagPage)
      g0.child(pageBadge().centerAt({tlx - g(9) + g(6), tly + g(9) + g(7)}));
    return g0;
  }

  // -------------------------------------------------------------------------
  // The frame: the stock brush::Pattern corner path (closed rect, four breaks).

  Element frameBand() const {
    const float in = g(9);
    return box()
        .inset(0)
        .shape(keyedShape(in,
                          [in](SkSize s) {
                            SkPathBuilder p;
                            p.addRect(SkRect::MakeLTRB(in, in, s.width() - in,
                                                       s.height() - in));
                            return p.detach();
                          }))
        // Bisector, spelled out even though it is the default. edgeEl() needs
        // Outgoing because its elbow art is drawn along the outgoing leg and a
        // 2x2 route is all corner; this corner art is a rotationally forgiving
        // lozenge on a closed rect, so the bisector is the right alignment for
        // it. Stating it keeps the two brushes' choices readable side by side.
        .stroke(brush::Pattern{
            .side = runTile,
            .corner =
                brush::CornerArt{cornerTile, brush::CornerAlign::Bisector},
            .advance = g(64),
            .cornerAngleDeg = 35.0f,
            .cornerLength = g(20),
            .stretchToFit = true,
            .bleedPx = g(26)});
  }

  /** The inner rule: four OPEN contours that STOP SHORT of the corners — a
   *  doubled rule whose inner line is dotted. Not a rounded rect anywhere. */
  static Element innerRule() {
    const float m = g(22), cut = g(26);
    Element e =
        box().inset(0).shape(keyedShape(std::tuple{m, cut}, [m, cut](SkSize s) {
          const float l = m, t = m, r = s.width() - m, b = s.height() - m;
          SkPathBuilder p;
          p.moveTo(l + cut, t);
          p.lineTo(r - cut, t);
          p.moveTo(r, t + cut);
          p.lineTo(r, b - cut);
          p.moveTo(r - cut, b);
          p.lineTo(l + cut, b);
          p.moveTo(l, b - cut);
          p.lineTo(l, t + cut);
          return p.detach();
        }));
    Brush br;
    lines::Line outer;
    outer.width = g(1.2f);
    outer.fill = Fill::color(mskia::scale(kBrassDark, 1.0f, 0.85f));
    br.layer(outer);
    lines::Line dotted;
    dotted.width = g(0.8f);
    dotted.fill = Fill::color(mskia::scale(kBrassLit, 0.85f, 0.65f));
    dotted.dashIntervals = {g(1.2f), g(3.0f)};
    br.layer(dotted, {shapers::Offset{.px = -g(2.5f), .step = g(3)}});
    e.stroke(br);
    return e;
  }

  // -------------------------------------------------------------------------
  // The tab rail (:219-233, :1089-1103). Left column at x = 1, and the source
  // stacks the seven buttons at y = 10 + i*24. The loop below starts one cell
  // lower — y = 10 + (i+1)*24 — because these coordinates are measured from
  // the screen origin rather than from the GUI's top inset, and at the source
  // offset the first tab would sit under the frame band.
  //
  // There is no right-hand rail here, and that is the source's answer rather
  // than an omission. updateResearch() splits categories with
  // `for (String tcc : ConfigResearch.TCCategories) if (tcc.equals(rcl))` —
  // anything in that array is added to categoriesTC and buttoned at x = 1,
  // everything else lands in categoriesOther at x = width-17. And
  // ConfigResearch:334 sets TCCategories to all seven vanilla categories, so
  // the right column and its two GuiScrollButtons only ever appear once an
  // ADDON registers one. Naming addon categories would be inventing text, so
  // the right margin here carries what the mod actually puts there at this
  // scroll: the web running off the edge under the frame band.
  // the 22x22 plate at (x-3, y-3) is UV(13,13) — the SAME cell as the frame's
  // four corners, which is why one art element serves both here.

  struct Category {
    const char* name;
    uint32_t aspect;
    int rune;
  };

  Element tabRail() const {
    static const Category kCats[7] = {
        {"Fundamentals", aspect::kHerba, 0},
        {"Auromancy", aspect::kAuram, 1},
        {"Alchemy", aspect::kAlkimia, 2},
        {"Artifice", aspect::kMachina, 3},
        {"Arcane Infusion", aspect::kPraecantatio, 4},
        {"Golemancy", aspect::kHumanus, 5},
        {"Eldritch", aspect::kAlienis, 6},
    };
    Element rail = box().inset(0);
    for (int i = 0; i < 7; ++i) {
      const float y = 10.0f + (float)(i + 1) * 24.0f;
      const bool selected = i == 2;  // ALCHEMY
      rail.child(cornerPlate(selected ? SkColor4f{0.6f, 1.0f, 1.0f, 1}
                                      : SkColor4f{1, 1, 1, 1})
                     .left(g(-2 - 1))
                     .top(g(y - 3 - 1))
                     .opacity(selected ? 1.0f : 0.86f));
      const Category& cat = kCats[i];
      rail.child(box()
                     .left(g(1))
                     .top(g(y))
                     .width(g(16))
                     .height(g(16))
                     .opacity(selected ? 1.0f : 0.8f)
                     .background(prog(
                         [cat, selected](SkCanvas& c, const PaintContext&) {
                           const kit::PixelInk k{c, U};
                           const SkColor4f col =
                               hexColor(cat.aspect, selected ? 1.0f : 0.66f);
                           // seven distinct runes, one per category
                           switch (cat.rune) {
                             case 0:
                               k.rect(7, 2, 2, 12, col);
                               k.rect(3, 6, 10, 2, col);
                               break;
                             case 1:
                               k.rect(3, 4, 10, 2, col);
                               k.rect(3, 10, 10, 2, col);
                               k.rect(7, 4, 2, 8, col);
                               break;
                             case 2:
                               k.rect(4, 3, 8, 2, col);
                               k.rect(4, 3, 2, 10, col);
                               k.rect(10, 3, 2, 10, col);
                               k.rect(4, 11, 8, 2, col);
                               k.rect(7, 7, 2, 2, col);
                               break;
                             case 3:
                               k.rect(3, 7, 10, 2, col);
                               k.rect(6, 3, 2, 10, col);
                               k.rect(10, 3, 2, 10, col);
                               break;
                             case 4:
                               k.rect(7, 2, 2, 12, col);
                               k.rect(3, 5, 10, 2, col);
                               k.rect(5, 11, 6, 2, col);
                               break;
                             case 5:
                               k.rect(6, 2, 4, 4, col);
                               k.rect(4, 7, 8, 4, col);
                               k.rect(5, 11, 2, 3, col);
                               k.rect(9, 11, 2, 3, col);
                               break;
                             default:
                               k.rect(3, 3, 2, 10, col);
                               k.rect(11, 3, 2, 10, col);
                               k.rect(5, 7, 6, 2, col);
                               k.rect(7, 3, 2, 4, col);
                               break;
                           }
                         })));
    }
    // the search button (:170, UV 160,16 at x=1, y=height-17), 0.8 grey
    rail.child(box()
                   .left(g(1))
                   .top(g(kGuiH - 17))
                   .width(g(16))
                   .height(g(16))
                   .opacity(0.8f)
                   .background(prog([](SkCanvas& c, const PaintContext&) {
                     SkPaint p;
                     p.setAntiAlias(true);
                     p.setStyle(SkPaint::kStroke_Style);
                     p.setStrokeWidth(g(1.6f));
                     p.setColor4f(kBrassLit, nullptr);
                     c.drawCircle(g(6.5f), g(6.5f), g(4.2f), p);
                     c.drawLine(g(9.5f), g(9.5f), g(13.5f), g(13.5f), p);
                   })));
    return rail;
  }

  // -------------------------------------------------------------------------
  // The tooltip. UtilsFX.drawCustomTooltip at (mx+3, my-3) for the hovered
  // node (:761-796): a gold title, then the missing-research block in red with
  // one yellow line per unknown parent. THAUMATORIUM is hovered; its parents
  // are CENTRIFUGE (not complete) and ESSENTIASMELTERTHAUMIUM (complete), so
  // exactly one line follows. It overlaps two edges and bleeds past the inner
  // rule, which is what a tooltip at the right-hand end of the tree does.

  Element tooltip() const {
    const PixText a = tipTitle, b = tipMissing, d = tipParent;
    const float wd = (float)std::max({a.w, b.w, d.w}) + 1.0f;
    const float ht = 3.0f * 10.0f;
    // THAUMATORIUM's hover box is 20x20 at (iconX-2, iconY-2); the cursor
    // sits inside it, and drawCustomTooltip anchors at (mx+3, my-3). At this
    // column that would run off the right edge, so it FLIPS to the cursor's
    // left — the vanilla clamp, and the reason the card ends up lying across
    // three edges and two neighbours instead of hanging in clear space.
    const SkPoint hc = centreOf(10, -2);
    const float mx = hc.fX / U + 2, my = hc.fY / U + 4;
    const float x = (mx + 3 + wd + 4 <= kGuiW) ? mx + 3 : mx - 3 - wd;
    const float y = my - 3;
    return box().inset(0).background(
        prog([a, b, d, x, y, wd, ht](SkCanvas& c, const PaintContext&) {
          SkPaint p;
          p.setAntiAlias(false);
          // Vanilla GuiScreen.drawHoveringText fills k1-3 .. k1+j1+3 under a
          // k1-4 .. k1-3 top strip, i.e. THREE px of sill below the last
          // line — not one. One px of sill is not enough room for a descender
          // plus its 1 px shadow, which then cross the inner border.
          const SkRect r = SkRect::MakeLTRB(g(x - 4), g(y - 4), g(x + wd + 4),
                                            g(y + ht + 3));
          p.setColor4f(hexColor(0x100010, 0.94f), nullptr);
          c.drawRect(r, p);
          // the vanilla two-tone inner border
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeWidth(g(1));
          p.setColor4f(hexColor(0x5000FF, 0.31f), nullptr);
          c.drawRect(r.makeInset(g(1), g(1)), p);
          p.setColor4f(hexColor(0x28007F, 0.31f), nullptr);
          c.drawRect(r.makeInset(g(2), g(2)), p);
          blitText(c, a, x, y, kTextGold);
          blitText(c, b, x, y + 10, kTextRed);
          blitText(c, d, x, y + 20, kTextYellow);
        }));
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    // The plate at exactly 2x. One GUI px is two canvas px and four device
    // px, so every stamped tile on the 24-px lattice lands whole.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kCanvasW, kCanvasH),
                             .captureAt = 6.0,
                             .background = hexColor(0x0B0906),
                             .oversample = 2});

    face = systemFace();
    if (ctx.fonts) {
      // The size is load-bearing; see kPixSizePx and the note above it.
      tipTitle =
          bakeText("Alchemical Automation", *ctx.fonts, face, kPixSizePx);
      tipMissing =
          bakeText("Missing required research:", *ctx.fonts, face, kPixSizePx);
      tipParent =
          bakeText(" - Essentia Centrifuge", *ctx.fonts, face, kPixSizePx);
    }

    for (int t = 0; t < 3; ++t) {
      const SkColor4f tint = tierTint((EdgeTier)t, kInkBody);
      spatter[t] = spatterCell(tint);
      knot[t] = knotCell(tint);
      straight[t] = straightTile(tint, spatter[t], knot[t]);
      for (int q = 0; q < 4; ++q) {
        const bool big = q >= 2;
        const float handed = ((unsigned)q & 1u) ? 1.0f : -1.0f;
        elbows[t][(size_t)q] =
            elbowTile(cornerArm(big), handed, tint, spatter[t], knot[t]);
      }
    }
    runTile = frameRun();
    cornerTile = cornerPlate({1, 1, 1, 1});
    for (int glyph = 0; glyph <= gSprayer; ++glyph)
      glyphs.push_back(glyphSprite(glyph));

    // ---- motion ----------------------------------------------------------
    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      // :610 — sin(systemTime % 600 / 600 * 2pi) * 0.25 + 0.75, wall clock,
      // so every unlockable node is in lockstep with zero phase offset.
      pulse =
          (float)(std::sin(std::fmod(t, 0.6) / 0.6 * 6.2831853) * 0.25 + 0.75);
      spin = (float)std::fmod(t * 0.06, 1.0);
      driftX = (float)(std::sin(t * 0.17) * 26.0);
      driftY = (float)(std::cos(t * 0.11) * 16.0);
      return true;
    });

    // ---- the tree --------------------------------------------------------
    Element root = box().inset(0);

    // 0. drawDefaultBackground (the world under the GUI, then the vanilla
    //    0xC0101010 -> 0xD0101010 wash) is NOT drawn, because at this
    //    resolution nothing of it would be visible: the painted backdrop
    //    plate covers GUI 14..413 x 14..253 opaquely and the frame band covers
    //    0..20 and 407..427 / 247..267, and their union is the whole canvas.
    //    Drawing it anyway would cost a full-canvas gradient every frame — a
    //    SHADER, which a cached picture replays as a draw call rather than as
    //    pixels — for no visible result. The clear colour stands in for it
    //    wherever the frame's corner art leaves a hairline.

    // 1. the parallax pair, clipped to (startX-2, startY-2, screenX+4,
    //    screenY+4) and moved at the source's 2.0 : 1.5 divisors.
    Element plate = box()
                        .left(g(kStartX - 2))
                        .top(g(kStartY - 2))
                        .width(g(kScreenX + 4))
                        .height(g(kScreenY + 4))
                        .clip();
    plate.child(backdropBase()
                    .cache(Cache::Texture)
                    .translateX(bind(&driftX).scale(-U / 2.0f))
                    .translateY(bind(&driftY).scale(-U / 2.0f)));
    plate.child(backdropOver()
                    .cache(Cache::Texture)
                    .translateX(bind(&driftX).scale(-U / 1.5f))
                    .translateY(bind(&driftY).scale(-U / 1.5f)));
    root.child(std::move(plate));

    // 2. the web. NOT clipped — the mod culls whole nodes at the viewport
    //    edge (:598) and lets everything else run out under the frame band,
    //    which is drawn last and covers the margin. That is why the tree
    //    reads as a viewport onto something larger.
    Element inner = box().inset(0);

    // 2a. warp swirls go under everything (drawForbidden, :603).
    for (const Node& n : kNodes)
      if (n.warp > 0 && !culled(n.col, n.row)) {
        const SkPoint c = centreOf(n.col, n.row);
        inner.child(warpSwirl(&spin, n.warp).centerAt(c).zIndex(-1));
      }

    // 2b. the edges, in unlock order — staggerChildren cascades the
    //     animate(from().to()) span entrance outward from BASEALCHEMY.
    Element edges = box().inset(0).zIndex(0);
    std::vector<int> order = edgeOrder();
    int k = 0;
    for (int i : order) edges.child(edgeEl(kEdges[i], k++));
    for (int i : order)
      if (kEdges[i].tier != kSiblingKnown) edges.child(arrowEl(kEdges[i]));
    inner.child(std::move(edges));

    // 2c. the plates, then the badges at full brightness over them.
    Element plates = box().inset(0).zIndex(4);
    for (const Node& n : kNodes) plates.child(nodePlate(n));
    for (const Node& n : kNodes)
      if (n.flagResearch || n.flagPage) plates.child(nodeBadges(n));
    inner.child(std::move(plates));

    root.child(std::move(inner));

    // 3. the frame, drawn last (genResearchBackgroundFixedPost).
    root.child(innerRule());
    root.child(frameBand());
    root.child(tabRail());

    // 4. the hover tooltip, over everything including the frame.
    root.child(tooltip());

    ctx.composer.render(root);
  }

  /** BFS depth from BASEALCHEMY, so the draw-on cascade runs outward the way
   *  the research actually unlocks. */
  static std::vector<int> edgeOrder() {
    std::array<int, kNodeCount> depth{};
    depth.fill(99);
    depth[(size_t)indexByKey("BASEALCHEMY")] = 0;
    for (int pass = 0; pass < 8; ++pass)
      for (const Edge& e : kEdges) {
        const int ci = indexByKey(e.child), pi = indexByKey(e.parent);
        depth[(size_t)ci] = std::min(depth[(size_t)ci], depth[(size_t)pi] + 1);
      }
    std::vector<int> idx;
    idx.reserve((size_t)kEdgeCount);
    for (int i = 0; i < kEdgeCount; ++i) idx.push_back(i);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
      return depth[(size_t)indexByKey(kEdges[a].child)] <
             depth[(size_t)indexByKey(kEdges[b].child)];
    });
    return idx;
  }
};

SIGIL_SKETCH(
    Thaumonomicon, "Study \xc2\xb7 Game UI",
    "Thaumcraft 6's research browser (2018) \xe2\x80\x94 edges that are "
    "stamped art, not strokes")
