// An asanoha lattice assembled from mitred timber pieces.

// TAGS: Patterns/Tiling

#include "Joinery.h"

struct KumikoAsanoha {
  Panel panel;
  TimberBank bank;
  /** THE SHOP DRAWING'S WORDS: its caption, the three jig angles and the
   *  reading under them stand in `data/content.json` beside this sketch,
   *  read in setup, so the code is the drawing and the file is what it
   *  says about it. */
  sketch::kit::Document doc;
  std::vector<choreograph::Output<float>> fade, pop;
  choreograph::Output<float> glow{0}, seat{0}, frameTrim{0};
  double t = 0;

  // --- the lattice, in paint order: ha under jigumi (a butt joint reads as
  // the ha stopping at the jigumi's face), jigumi under the register.
  Element lattice() {
    auto add = [&](Element& into, Role role) {
      for (size_t i = 0; i < panel.strips.size(); ++i)
        if (panel.strips[i].role == role)
          into.children(
              {stripElement(panel.strips[i], bank, &fade[i], &pop[i])});
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
        group.children(
            {stripElement(panel.strips[i], bank, &fade[i], &pop[i])});
    return group;
  }

  // The joint pass: every half-lap seam mark and every tenon nub arrives on
  // one beat — the craftsman's final seating tap.
  Element joinery() {
    // The panel's seams are settled when the panel is built, so the whole
    // pass is one named drawing rather than a callable nothing can compare.
    auto seams = panel.seams;
    auto marks = [seams](SkCanvas& c) {
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
    group.children(
        {custom(std::string_view("joinery"), marks).inset(0, 0, 0, 0)});
    for (const Strip& n : panel.nubs)
      group.children({stripElement(n, bank, nullptr, nullptr)});
    return group;
  }

  Element backlight() {
    const SkRect& open = kRegOuter;  // the frame's opening
    return box()
        .rect(open)
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
        .children({box()
                       .inset(0, 0, 0, 0)
                       .fill(Paint::radial(
                           {open.width() * 0.5f, open.height() * 0.5f}, 585,
                           {{0.00f, hexColor(0xF7E8C6, 0.88f)},
                            {0.30f, hexColor(0xF2E0B4, 0.85f)},
                            {0.58f, hexColor(0xE6CE9A, 0.79f)},
                            {0.80f, hexColor(0xD3B37C, 0.71f)},
                            {1.00f, hexColor(0xBE9862, 0.62f)}}))});
  }

  Element beam(float y, float h, bool top) {
    return kit::at(0, y, kW, h)
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
    return kit::at(x, 118, w, kRoom - 236)
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
    Element g = kit::at(0, y0, kW, kBandH)
                    .fill(Fill::color(hexColor(0x120C07)))
                    .font({.size = 10.5f,
                           .color = hexColor(0xC9B78F, 0.75f),
                           .track = 0.9f});
    // the drawing's own ground: a hairline ruled off the room above it
    g.children(
        {kit::at(0, 0, kW, 1).fill(Fill::color(hexColor(0x4A3620, 0.9f)))});

    Element art = kit::at(0, 0, kW, kBandH);

    // the jigumi cell it all sits in
    art.children(
        {kit::at(org.x(), org.y() - y0, side, side)
             .stroke(stroke(1.0f, Fill::color(hexColor(0x8E6C3B, 0.85f)),
                            PathFormat::Align::Center))});
    // the two incircles, struck, and the incenters they are struck about:
    // the construction the whole pattern is derived from, and the only
    // circles anywhere in a kumiko panel
    const std::array<SkPoint, 2> incentre{
        {{I1.x(), I1.y() - y0}, {I2.x(), I2.y() - y0}}};
    art.children({each(incentre,
                       [r](SkPoint c) {
                         return ring(c, r,
                                     PathFormat{.width = 0.9f,
                                                .strokeFill = Fill::color(
                                                    hexColor(0xC79A57, 0.45f)),
                                                .dashIntervals = {4.0f, 4.0f}});
                       }),
                  each(incentre, [](SkPoint c) {
                    return dot(c, 2.4f, Fill::color(hexColor(0xF4E3B8, 0.9f)));
                  })});

    // the seven pieces, exploded off their seats
    for (const Strip& st : cellPieces({org.x(), org.y() - y0}, side,
                                      std::max(6.0f, kHaW * 1.6f), 5.0f))
      art.children({stripElement(st, bank, nullptr, nullptr)});

    g.children({std::move(art)});

    // the three jigs, as the three angles one right angle is cut into: a
    // wedge each, with the angle named under it
    const data::Json& jigs = doc["jigs"];
    g.children(
        {each(jigs["angles"].items(),
              [](const data::Json& angle, std::size_t i) {
                const float x = 430.0f + (float)i * 118.0f;
                return box().inset(0).children(
                    {kit::disc(SkPoint{x, 96.0f}, 44.0f)
                         .shape(shapes::sector(-90.0f + 22.5f * (float)i, 22.5f,
                                               0.0f))
                         .fill(Fill::color(hexColor(0xC79A57, 0.16f)))
                         .stroke(stroke(0.9f,
                                        Fill::color(hexColor(0xC79A57, 0.55f)),
                                        PathFormat::Align::Inner)),
                     text(angle).left(x - 30).top(150).width(60).block(
                         {.alignment = weave::TextAlignment::kCenter})});
              }),
         text(jigs["note"])
             .font({.color = hexColor(0xB7A281, 0.55f), .track = 0.5f})
             .left(392)
             .top(24)
             .width(300),
         // THE READING, and it FLOWS: a title over its three
         // paragraphs at one gap, so a longer line pushes the rest
         // down instead of running through a top typed for it
         kit::at(760, 30, 520, kBandH - 60)
             .gap(12)
             .children(
                 {text(doc["reading"]["title"])
                      .font({.size = 12,
                             .color = hexColor(0xE4D5B2, 0.86f),
                             .track = 1.3f}),
                  each(doc["reading"]["lines"].items(),
                       [](const data::Json& line) { return text(line); })})});
    return g;
  }

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    const SkRect mid = kRegOuter.makeOutset(1.5f, 1.5f);
    return stack()
        .fill(Fill::color(kNight))
        .children({backlight(), lattice(), joinery()})
        // Backlight bleed: the lamp's halo added OVER the
        // fretwork, so the light visibly wraps the pieces it is behind
        // instead of stopping dead at their silhouettes.
        .children(
            {box()
                 .rect(kRegOuter)
                 .clip(true)
                 .opacity(&glow)
                 .blend(SkBlendMode::kPlus)
                 .fill(Paint::radial(
                     {kRegOuter.width() * 0.5f, kRegOuter.height() * 0.5f}, 360,
                     {{0.00f, hexColor(0xFFF2D2, 0.13f)},
                      {0.45f, hexColor(0xE6BC7C, 0.07f)},
                      {1.00f, hexColor(0x000000, 0.00f)}})),
             frame()})
        // The mitred frame's keyline draws itself on around the perimeter —
        // one continuous reveal, the first beat of the assembly.
        .children({box().rect(mid).stroke(
                       spans::upTo(&frameTrim),
                       PathFormat{
                           .width = 2.2f,
                           .strokeFill = Fill::color(hexColor(0xC79A57, 0.60f)),
                           .align = PathFormat::Align::Center}),
                   post(0, 146), post(kW - 146, 146), beam(0, 122, true),
                   beam(kRoom - 122, 122, false),
                   text(doc["caption"])
                       .font({.size = 12, .color = kCaption, .track = 1.1f})
                       .left(950)
                       .top(916)
                       .width(300)
                       .block({.alignment = weave::TextAlignment::kEnd})})
        // A faint vertical vignette — the near-side room, in shadow. It
        // stops at the room's floor: the shop drawing under it is a
        // drawing, not part of the room.
        .children({kit::at(0, 0, kW, kRoom)
                       .fill(radialGradient(
                           {700, 500}, 920,
                           {{0, 0, 0, 0}, {0, 0, 0, 0.30f}, {0, 0, 0, 0.62f}},
                           {0.30f, 0.72f, 1.0f})),
                   shopDrawing()});
  }

  void setup(sketch::SketchContext& ctx) {
    // This sketch brings its own canvas size and unlit background rather
    // than inheriting a default, and photographs itself mid-hold: the panel
    // is complete and lit from kTGlow + kDGlow onwards, and the loop tears
    // down and reassembles at kPeriod. 4.2 s sits well clear of both edges,
    // so a small timing change on either side cannot catch the plate
    // half-built.
    sketch::kit::stage(
        ctx,
        {.size = SkSize::Make(kW, kH), .captureAt = 4.2, .background = kNight});

    doc = sketch::kit::Document(ctx, "data/content.json");

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
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) {}
};

SIGIL_SKETCH(KumikoAsanoha, "Study · Pattern",
             "A hinoki asanoha ranma — 514 mitred boards, per-piece "
             "assembly staggering")
