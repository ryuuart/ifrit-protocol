// An asanoha lattice assembled from mitred timber pieces.

#include "Joinery.h"

struct KumikoAsanoha : sketch::Sketch {
  Panel panel;
  TimberBank bank;
  std::vector<choreograph::Output<float>> fade, pop;
  choreograph::Output<float> glow{0}, seat{0}, frameTrim{0};
  double t = 0;

  // --- the lattice, in paint order: ha under jigumi (a butt joint reads as
  // the ha stopping at the jigumi's face), jigumi under the register.
  Element lattice() {
    auto add = [&](Element& into, Role role) {
      for (size_t i = 0; i < panel.strips.size(); ++i)
        if (panel.strips[i].role == role)
          into.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    };
    auto group = box().inset(0, 0, 0, 0).cache(Cache::Group);
    add(group, kRoleDiagonal);
    add(group, kRoleFiller);
    add(group, kRoleLock);
    add(group, kRoleJigumiH);
    add(group, kRoleJigumiV);
    add(group, kRoleRegister);
    // ONE LINE, and it is what makes a lattice of this size affordable, so
    // the reasoning behind it stays.
    //
    // Every strip is a wood-grain SkSL fill plus a BevelEmboss, rotated to
    // its own jig angle, carrying a bound opacity and a bound scale for the
    // entrance. Those bindings keep each strip volatile while the panel is
    // assembling, so no cache above them can hold pixels for the panel as a
    // whole until the last board has landed.
    //
    // Cache::Group is that cache. The whole lattice composites ONCE into a
    // single unrotated device-space layer, so the rotations, arrises and
    // abutments all resolve inside that bake, and the layer is held only
    // while every bound opacity and scale beneath it still reads what it
    // read last frame. The settled panel therefore costs one blit — and the
    // ENTRANCE, which is the half of the cycle this cannot help with, is
    // paid for one board at a time by the per-strip bake in stripElement.
    //
    // A container-level .cache(Cache::Texture) on this box is a no-op and
    // is worth knowing about before trying it: Texture bakes a node's OWN
    // paint, and a fill-less container has none. Giving it a transparent
    // fill or forcing a stacking context does not change that.
    return group;
  }

  Element frame() {
    // The same argument as lattice(), on four members instead of hundreds.
    // The mitred keyaki boards carry the same bound entrance — and they are
    // the largest single boards on the canvas, so replaying their timber
    // shader per frame would cost more than their count suggests.
    auto group = box().inset(0, 0, 0, 0).cache(Cache::Group);
    for (size_t i = 0; i < panel.strips.size(); ++i)
      if (panel.strips[i].role == kRoleFrame)
        group.child(stripElement(panel.strips[i], bank, &fade[i], &pop[i]));
    return group;
  }

  // The joint pass: every half-lap seam mark and every tenon nub arrives on
  // one beat — the craftsman's final seating tap.
  Element joinery() {
    // The panel's seams are settled when the panel is built, so the whole
    // pass is one named drawing rather than a callable nothing can compare.
    auto seams = panel.seams;
    auto marks = [seams](SkCanvas& c, const PaintContext&) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      for (const Panel::Seam& s : seams) {
        const SkVector n = perp(s.along);
        for (int side = -1; side <= 1; side += 2) {
          const float o = (float)side * s.w * 0.5f;
          const SkPoint m{s.p.x() + n.x() * o, s.p.y() + n.y() * o};
          p.setStrokeWidth(1.5f);
          p.setColor4f({0.28f, 0.18f, 0.07f, 0.55f}, nullptr);
          c.drawLine(m.x() - s.along.x() * s.halfSpan,
                     m.y() - s.along.y() * s.halfSpan,
                     m.x() + s.along.x() * s.halfSpan,
                     m.y() + s.along.y() * s.halfSpan, p);
          p.setStrokeWidth(0.7f);
          p.setColor4f({1, 0.95f, 0.82f, 0.30f}, nullptr);
          const float k = (float)side * 0.9f;
          c.drawLine(m.x() - s.along.x() * s.halfSpan + n.x() * k,
                     m.y() - s.along.y() * s.halfSpan + n.y() * k,
                     m.x() + s.along.x() * s.halfSpan + n.x() * k,
                     m.y() + s.along.y() * s.halfSpan + n.y() * k, p);
        }
      }
    };
    auto group = stack().inset(0, 0, 0, 0).opacity(&seat);
    group.child(custom(std::string_view("joinery"), marks).inset(0, 0, 0, 0));
    for (const Strip& n : panel.nubs)
      group.child(stripElement(n, bank, nullptr, nullptr));
    return group;
  }

  Element backlight() {
    const SkRect& open = kRegOuter;  // the frame's opening
    return box()
        .left(open.left())
        .top(open.top())
        .width(open.width())
        .height(open.height())
        .clip(true)
        .opacity(&glow)
        .background(styles::OuterGlow{hexColor(0xF4E3B8, 0.34f), 70, 6})
        // A SHOJI DIFFUSES. Paper over a lamp is a lit field, not a point
        // source seen through a hole: the ramp falls a third of a stop
        // from the middle of the opening to its corners, and the pattern —
        // which is the whole subject — is readable everywhere across it.
        // A hot core with a dark rim blows out the middle third and puts
        // the corner cells in the dark, which loses the field at both ends
        // at once. The outer radius is the opening's diagonal.
        .child(box()
                   .inset(0, 0, 0, 0)
                   .fill(Paint::radial(
                       {open.width() * 0.5f, open.height() * 0.5f}, 585,
                       {{0.00f, hexColor(0xF7E8C6, 0.88f)},
                        {0.30f, hexColor(0xF2E0B4, 0.85f)},
                        {0.58f, hexColor(0xE6CE9A, 0.79f)},
                        {0.80f, hexColor(0xD3B37C, 0.71f)},
                        {1.00f, hexColor(0xBE9862, 0.62f)}})));
  }

  Element beam(float y, float h, bool top) {
    return box()
        .left(0)
        .top(y)
        .width(kW)
        .height(h)
        .fill(bank.get(kKeyakiShade, h, !top, 7))
        .foreground(
            styles::InnerShadow{{0, 0, 0, 0.65f}, {0, top ? 8.f : -8.f}, 18})
        .foreground(styles::BevelEmboss{2.5f,
                                        4,
                                        top ? 300.0f : 120.0f,
                                        {1, 0.88f, 0.68f, 0.16f},
                                        {0, 0, 0, 0.60f}});
  }

  Element post(float x, float w) {
    return box()
        .left(x)
        .top(118)
        .width(w)
        .height(kRoom - 236)
        .fill(bank.get(kKeyakiShade, w, x > 700, 3, /*swap=*/true))
        .foreground(styles::InnerShadow{
            {0, 0, 0, 0.60f}, {x > 700 ? -7.f : 7.f, 0}, 16});
  }

  // =========================================================================
  // THE SHOP DRAWING — one cell, taken apart
  //
  // The field's whole argument is a rule about ONE cell, and on the panel it
  // is invisible: seven pieces per jigumi cell, each running from a vertex of
  // one of the two right isoceles triangles the diagonal makes to that
  // triangle's INCENTER. Everything else — the 22.5°/45°/67.5° jig angles,
  // the three-way bisection of every right angle, the piece count — follows
  // from the incircle. So the cell is drawn again here at four times the
  // panel's pitch, with the two incircles struck, the incenters marked, and
  // the seven pieces pushed a little off their seats.
  //
  // The pieces are generated by the SAME rule the field uses and drawn by the
  // same `stripElement`, so this cannot drift out of agreement with the panel
  // beside it: change the construction and both change.

  /** The seven ha of one cell of side @p side at @p origin, each pushed
   *  @p burst px along its own axis so the joints open. */
  static std::vector<Strip> cellPieces(SkPoint origin, float side, float stock,
                                       float burst) {
    const float rIn = 0.2928932f;   // incircle radius / leg
    const float rOut = 0.7071068f;  // 1 - rIn
    std::vector<Strip> out;
    auto P = [&](float lx, float ly) {
      return SkPoint{origin.x() + lx, origin.y() + ly};
    };
    const SkPoint A = P(0, 0), B = P(side, 0), C = P(side, side),
                  D = P(0, side);
    const SkPoint I1 = P(side * rOut, side * rIn);  // incenter of ABC
    const SkPoint I2 = P(side * rIn, side * rOut);  // incenter of ACD

    uint32_t seed = 101;
    auto piece = [&](SkPoint from, SkPoint to, Role role, SkVector cut) {
      const SkVector u = norm({to.x() - from.x(), to.y() - from.y()});
      Strip st;
      st.a = {from.x() + u.x() * burst, from.y() + u.y() * burst};
      st.b = {to.x() + u.x() * burst, to.y() + u.y() * burst};
      st.w = stock;
      st.role = role;
      st.timber = &kHinokiTimber;
      st.seed = seed++ * 2654435761u >> 13u;
      st.cutA = (cut.length() < 1e-4f) ? perp(u) : norm(cut);
      st.cutB = perp(u);
      out.push_back(st);
    };
    // 1 diagonal, 2 fillers off the right angles, 4 locking pieces off the
    // 45° corners — the cutting order the sources give.
    piece(A, C, kRoleDiagonal, {0, 0});
    piece(B, I1, kRoleFiller, {0, 0});
    piece(D, I2, kRoleFiller, {0, 0});
    piece(A, I1, kRoleLock, {1, 0});
    piece(A, I2, kRoleLock, {0, 1});
    piece(C, I1, kRoleLock, {0, 1});
    piece(C, I2, kRoleLock, {1, 0});
    return out;
  }

  Element shopDrawing() {
    const float y0 = kRoom;
    const float side = 138.0f;
    const SkPoint org{206, y0 + 38};
    const float rIn = 0.2928932f, rOut = 0.7071068f;
    const SkPoint I1{org.x() + side * rOut, org.y() + side * rIn};
    const SkPoint I2{org.x() + side * rIn, org.y() + side * rOut};
    const float r = side * rIn;  // the incircle radius of each half

    // The drawing's ruling hand, stated once on the band: every line under
    // it is set in it unless it says otherwise.
    Element g = box()
                    .left(0)
                    .top(y0)
                    .width(kW)
                    .height(kBandH)
                    .fill(Fill::color(hexColor(0x120C07)))
                    .font({.size = 10.5f,
                           .color = hexColor(0xC9B78F, 0.75f),
                           .track = 0.9f});
    // the drawing's own ground: a hairline ruled off the room above it
    g.child(box().left(0).top(0).width(kW).height(1).fill(
        Fill::color(hexColor(0x4A3620, 0.9f))));

    Element art = box().left(0).top(0).width(kW).height(kBandH);

    // the jigumi cell it all sits in
    art.child(box()
                  .left(org.x())
                  .top(org.y() - y0)
                  .width(side)
                  .height(side)
                  .stroke(stroke(1.0f, Fill::color(hexColor(0x8E6C3B, 0.85f)),
                                 PathFormat::Align::Center)));
    // the two incircles, struck: the construction the whole pattern is
    // derived from, and the only circles anywhere in a kumiko panel
    for (const SkPoint& c : {I1, I2})
      art.child(kit::disc(SkPoint{c.x(), c.y() - y0}, r)
                    .shape(shapes::circle())
                    .fill(Fill::none())
                    .stroke(PathFormat{
                        .width = 0.9f,
                        .strokeFill = Fill::color(hexColor(0xC79A57, 0.45f)),
                        .dashIntervals = {4.0f, 4.0f}}));
    for (const SkPoint& c : {I1, I2})
      art.child(kit::disc(SkPoint{c.x(), c.y() - y0}, 2.4f)
                    .shape(shapes::circle())
                    .fill(Fill::color(hexColor(0xF4E3B8, 0.9f))));

    // the seven pieces, exploded off their seats
    for (const Strip& st : cellPieces({org.x(), org.y() - y0}, side,
                                      std::max(6.0f, kHaW * 1.6f), 5.0f))
      art.child(stripElement(st, bank, nullptr, nullptr));

    g.child(std::move(art));

    // the three jigs, as the three angles one right angle is cut into
    const char* jig[3] = {"22.5\xc2\xb0", "45\xc2\xb0", "67.5\xc2\xb0"};
    for (int i = 0; i < 3; ++i) {
      const float x = 430.0f + (float)i * 118.0f;
      const float a0 = 22.5f * (float)i;
      g.child(kit::disc(SkPoint{x, 96.0f}, 44.0f)
                  .shape(shapes::sector(-90.0f + a0, 22.5f, 0.0f))
                  .fill(Fill::color(hexColor(0xC79A57, 0.16f)))
                  .stroke(stroke(0.9f, Fill::color(hexColor(0xC79A57, 0.55f)),
                                 PathFormat::Align::Inner)));
      g.child(text(toUtf8(jig[i]))
                  .left(x - 30)
                  .top(150)
                  .width(60)
                  .textAlign(weave::TextAlignment::kCenter));
    }
    g.child(text(toUtf8("THREE JIGS \xe2\x80\x94 AND A RIGHT ANGLE IS "
                        "22.5 + 45 + 22.5"))
                .font({.color = hexColor(0xB7A281, 0.55f), .track = 0.5f})
                .left(392)
                .top(24)
                .width(300));

    // the reading
    g.child(
        text(toUtf8("ONE CELL, TAKEN APART"))
            .font(
                {.size = 12, .color = hexColor(0xE4D5B2, 0.86f), .track = 1.3f})
            .left(760)
            .top(30));
    g.child(
        text(
            toUtf8("The diagonal cuts the cell into two right isoceles "
                   "triangles. In each, the three infill pieces run from the "
                   "triangle's vertices to its INCENTER \xe2\x80\x94 and every "
                   "number the panel is built on falls out of that one rule."))
            .left(760)
            .top(56)
            .width(520));
    g.child(text(toUtf8("incircle r = s(2\xe2\x88\x92\xe2\x88\x9a"
                        "2)/2 = "
                        "0.29289 s  \xc2\xb7  arm off a 45\xc2\xb0 corner = "
                        "atan(\xe2\x88\x9a"
                        "2\xe2\x88\x92"
                        "1) = "
                        "22.5\xc2\xb0"))
                .left(760)
                .top(126)
                .width(520));
    g.child(text(toUtf8("1 diagonal + 2 fillers + 4 locking pieces = 7 per "
                        "cell  \xc2\xb7  60 cells = 420 ha"))
                .left(760)
                .top(148)
                .width(520));
    return g;
  }

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    const SkRect mid = kRegOuter.makeOutset(1.5f, 1.5f);
    return stack()
        .fill(Fill::color(kNight))
        .child(backlight())
        .child(lattice())
        .child(joinery())
        // Backlight bleed: the lamp's halo added OVER the
        // fretwork, so the light visibly wraps the pieces it is behind
        // instead of stopping dead at their silhouettes.
        .child(
            box()
                .left(kRegOuter.left())
                .top(kRegOuter.top())
                .width(kRegOuter.width())
                .height(kRegOuter.height())
                .clip(true)
                .opacity(&glow)
                .blend(SkBlendMode::kPlus)
                .fill(Paint::radial(
                    {kRegOuter.width() * 0.5f, kRegOuter.height() * 0.5f}, 360,
                    {{0.00f, hexColor(0xFFF2D2, 0.13f)},
                     {0.45f, hexColor(0xE6BC7C, 0.07f)},
                     {1.00f, hexColor(0x000000, 0.00f)}})))
        .child(frame())
        // The mitred frame's keyline draws itself on around the perimeter —
        // one continuous reveal, the first beat of the assembly.
        .child(box()
                   .left(mid.left())
                   .top(mid.top())
                   .width(mid.width())
                   .height(mid.height())
                   .stroke(spans::upTo(&frameTrim),
                           PathFormat{.width = 2.2f,
                                      .strokeFill = Fill::color(
                                          hexColor(0xC79A57, 0.60f)),
                                      .align = PathFormat::Align::Center}))
        .child(post(0, 146))
        .child(post(kW - 146, 146))
        .child(beam(0, 122, true))
        .child(beam(kRoom - 122, 122, false))
        .child(text(toUtf8("ASANOHA KUMIKO \xc2\xb7 SQUARE JIGUMI \xc2\xb7 "
                           "HINOKI ON KEYAKI \xc2\xb7 900\xc3\x97"
                           "400mm TYPE"))
                   .font({.size = 12, .color = kCaption, .track = 1.1f})
                   .left(950)
                   .top(916)
                   .width(300)
                   .textAlign(weave::TextAlignment::kEnd))
        // A faint vertical vignette — the near-side room, in shadow. It
        // stops at the room's floor: the shop drawing under it is a
        // drawing, not part of the room.
        .child(box().left(0).top(0).width(kW).height(kRoom).fill(radialGradient(
            {700, 500}, 920, {{0, 0, 0, 0}, {0, 0, 0, 0.30f}, {0, 0, 0, 0.62f}},
            {0.30f, 0.72f, 1.0f})))
        .child(shopDrawing());
  }

  void setup(sketch::SketchContext& ctx) override {
    // This sketch brings its own canvas size and unlit background rather
    // than inheriting a default, and photographs itself mid-hold: the panel
    // is complete and lit from kTGlow + kDGlow onwards, and the loop tears
    // down and reassembles at kPeriod. 4.2 s sits well clear of both edges,
    // so a small timing change on either side cannot catch the plate
    // half-built.
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 4.2, .background = kNight});

    panel = Panel{};
    panel.build();
    fade = std::vector<choreograph::Output<float>>(panel.strips.size());
    pop = std::vector<choreograph::Output<float>>(panel.strips.size());
    for (size_t i = 0; i < fade.size(); ++i) {
      fade[i] = 0.0f;
      pop[i] = 1.0f;
    }
    t = 0;

    ctx.ticker.add([this](double dt) {
      t += dt;
      const double now = std::fmod(t, kPeriod);
      for (size_t i = 0; i < panel.strips.size(); ++i) {
        const Strip& s = panel.strips[i];
        const float raw = clamp01((now - s.delay) / s.dur);
        fade[i] = std::min(1.0f, choreograph::easeOutCubic(raw) * 1.35f);
        pop[i] = 0.55f + 0.45f * choreograph::easeOutBack(raw);
      }
      seat = choreograph::easeOutCubic(clamp01((now - kTSeat) / kDSeat));
      glow = choreograph::easeOutCubic(clamp01((now - kTGlow) / kDGlow));
      frameTrim = choreograph::easeOutCubic(
          clamp01((now - kTFrame) / (kDFrame + 0.35)));
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) override {}
};

SIGIL_SKETCH(KumikoAsanoha, "Study \xc2\xb7 Pattern",
             "A hinoki asanoha ranma \xe2\x80\x94 514 mitred boards, per-piece "
             "assembly staggering")
