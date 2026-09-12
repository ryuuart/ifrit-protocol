// A Dead Space upgrade bench composed from routed circuit nodes and status
// panels.

// TAGS: Geometry/Diagrams, Interfaces/Game

#include "Circuit.h"

struct Ds2Bench : sketch::Sketch {
  // one bound glow per node — the idle pulse is desynced by index
  std::array<choreograph::Output<float>, 24> glow;
  choreograph::Output<float> socketPulse{1.0f};
  choreograph::Output<float> scanY{0};
  choreograph::Output<float> jitterX{0};
  choreograph::Output<float> holoAlpha{1.0f};
  /** The scanline field's own clock: the scene time stepped at 6 Hz, half
   *  a step off the whole seconds. See `steppedTime`. */
  choreograph::Output<float> scanClock{0};

  // the legend pip masses: one atlas (2 cells), one pool per row
  std::shared_ptr<instancing::Atlas> pips =
      std::make_shared<instancing::Atlas>(2.0f);
  std::array<std::shared_ptr<instancing::Pool>, kStatCount> pipPools;
  int pipFilled = 0, pipEmpty = 1;
  int glowSlot = 0;
  // LUMINANCE grain — the CRT capture's dirt, equal in all three
  // channels so an overlay pass reads as LIGHT rather than as a hue
  // shift. Two octaves is what the dirt is: a fine field with one
  // coarser one under it, and nothing below that.
  Paint grain = Paint::recipe(field::grain(0.9f, 2, 7.0f));

  double nextGlitch = 4.2, glitchEnd = 0;

  // -------------------------------------------------------------------
  // the pip atlas: two chevron cells, tinted per instance

  void bakePips() {
    Element filled =
        box()
            .shape(chevron())
            .fill(Paint::linear({0, 0}, {0, kPipH},
                                {{0.0f, hexColor(0xC8DADA)},
                                 {0.42f, hexColor(0x92AAAC)},
                                 {0.52f, hexColor(0x70898C)},
                                 {1.0f, hexColor(0xB0C6C8)}}))
            .stroke(stroke(1.0f, Fill::color(mskia::withAlpha(kCyan, 0.5f)),
                           PathFormat::Align::Inner));
    Element empty = box().shape(chevron()).stroke(
        stroke(1.2f, Fill::color(mskia::withAlpha(kCyan, 0.30f)),
               PathFormat::Align::Inner));
    pipFilled = pips->cell(std::move(filled), {kPipW, kPipH});
    pipEmpty = pips->cell(std::move(empty), {kPipW, kPipH});

    for (int r = 0; r < kStatCount; ++r) {
      auto pool = std::make_shared<instancing::Pool>();
      // place::repeat IS the Repeater law — linear translate per copy.
      instancing::place::repeat(*pool, (size_t)kStats[r].total,
                                {kPipW * 0.5f, kPipH * 0.5f},
                                {kPipW + kPipGap, 0.0f});
      auto frames = pool->frames();
      auto tints = pool->tints();
      const KindArt art = artOf(kStats[r].kind);
      for (int i = 0; i < kStats[r].total; ++i) {
        const bool on = i < kStats[r].filled;
        frames[i] = on ? pipFilled : pipEmpty;
        // the tint channel carries the row's identity into a shared cell
        tints[i] = on ? SkColor4f{0.66f + 0.34f * art.ring.fR,
                                  0.66f + 0.34f * art.ring.fG,
                                  0.66f + 0.34f * art.ring.fB, 1.0f}
                      : SkColor4f{1, 1, 1, 1};
      }
      pool->commit();
      pipPools[(size_t)r] = std::move(pool);
    }
  }

  // -------------------------------------------------------------------
  // backdrop: the blurred ship interior, implied not modeled

  void backdrop(Element& root) {
    // ONE BAKED PLANE. The room is fourteen out-of-focus rectangles and
    // every one of them carries a directional blur; nothing in it ever
    // changes, so it is rasterised once and blitted after — uncached it
    // re-runs fourteen blurs over the whole canvas on every frame, and it
    // is behind a panel that is now translucent, so it is composited on
    // every frame too.
    Element room = box().key("room").inset(0).cache(Cache::Texture).zIndex(0);
    auto strut = [&](float x, float w, float a) {
      room.child(
          box()
              .rect(SkRect::MakeXYWH(x, -40.0f, w, kH + 80))
              .fill(Paint::linear({0, 0}, {0, kH},
                                  {{0.0f, hexColor(0x16262F, a * 0.35f)},
                                   {0.38f, hexColor(0x1C303C, a)},
                                   {1.0f, hexColor(0x080F16, a * 0.2f)}}))
              // the strut melts vertically out of focus: 18 along
              // the vertical, 12 across
              .effect(Effect::directionalBlur(18, 90, 12))
              .zIndex(0));
    };
    // THE MACHINE ROOM, so the panel has something to be glass OVER.
    // Dead Space's bench is a hologram standing in a room and the wall
    // structure reads through the whole of it; struts confined to the two
    // margins leave the panel floating on the frame's own black, which is
    // the one thing a translucent panel cannot do.
    strut(-30, 96, 0.9f);
    strut(66, 30, 0.5f);
    strut(1108, 92, 0.9f);
    strut(1040, 26, 0.45f);
    strut(196, 54, 0.42f);
    strut(430, 38, 0.30f);
    strut(690, 46, 0.34f);
    strut(902, 32, 0.28f);
    // A lit doorway behind the panel's left third — one warm rectangle is
    // what tells an eye the wall is a wall and not a backdrop.
    room.child(box()
                   .rect(SkRect::MakeXYWH(250.0f, 118.0f, 176.0f, 470.0f))
                   .fill(Paint::linear({0, 0}, {0, 470},
                                       {{0.0f, hexColor(0x2A4A52, 0.34f)},
                                        {0.45f, hexColor(0x3E6A6E, 0.26f)},
                                        {1.0f, hexColor(0x0C1A20, 0.09f)}}))
                   .effect(Effect::directionalBlur(22, 90, 16))
                   .zIndex(0));
    // …and a bank of pipes crossing the wall behind the right half.
    for (int i = 0; i < 5; ++i)
      room.child(box()
                     .rect(SkRect::MakeXYWH(560.0f, 150.0f + (float)i * 96.0f,
                                            560.0f, 13.0f))
                     .fill(Paint::linear({0, 0}, {0, 13},
                                         {{0.0f, hexColor(0x25444E, 0.30f)},
                                          {0.5f, hexColor(0x1A3038, 0.20f)},
                                          {1.0f, hexColor(0x0A1218, 0.07f)}}))
                     .effect(Effect::directionalBlur(9, 0, 14))
                     .zIndex(0));
    room.child(box()
                   .rect(SkRect::MakeXYWH(-40.0f, 2.0f, kW + 80, 28.0f))
                   .fill(Paint::linear({0, 0}, {0, 28},
                                       {{0.0f, hexColor(0x243B47, 0.5f)},
                                        {1.0f, hexColor(0x0A141C, 0.25f)}}))
                   // 10 along the vertical, 7 across
                   .effect(Effect::directionalBlur(10, 90, 7))
                   .zIndex(0));
    root.child(std::move(room));
  }

  // -------------------------------------------------------------------
  // the panel plate + its frame vocabulary

  void plate(Element& root) {
    // body: solid + a radial lift + the live scanline field, ONE shader
    root.child(
        box()
            .key("plate")
            .rect(SkRect::MakeXYWH(kPX, kPY, kPW, kPH))
            .shape(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
            // THE PANEL IS GLASS. Dropping the 3D framing is
            // stated; dropping the translucency was not, and it is
            // the separate half — Dead Space's hologram is a sheet
            // of light with the room behind it visible through
            // every part of it, and an opaque body makes the same
            // drawing a flat rectangle on black.
            .fill(Paint::blend(
                {{Paint::solid(mskia::withAlpha(kBody, 0.70f)),
                  SkBlendMode::kSrcOver},
                 // unit-square ramp: the lift is authored against the
                 // box, not against a pixel extent transcribed by hand
                 {Paint::radialUnit({0.40f, 0.32f}, 1.15f,
                                    {{0.0f, hexColor(0xFFFFFF)},
                                     {0.5f, hexColor(0xC0D0D0)},
                                     {1.0f, hexColor(0x4E6264)}}),
                  SkBlendMode::kMultiply},
                 {scanField(mskia::withAlpha(kCyan, 0.075f), 3.0f, &scanClock),
                  SkBlendMode::kScreen}}))
            .zIndex(1));

    // the grain the compressed CRT capture carries — enough to kill the
    // "clean vector art" read without becoming VHS noise
    // The grain is a STATIC procedural shader over the whole panel, and
    // opacity + kOverlay makes the library refuse to bake it automatically,
    // so it is evaluated per pixel per frame unless asked. It never changes,
    // hence the explicit bake. (The plate BASE below it stays live on
    // purpose: its fill carries a scrolling scanField shader.)
    root.child(box()
                   .rect(SkRect::MakeXYWH(kPX, kPY, kPW, kPH))
                   .shape(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
                   .fill(grain)
                   .opacity(0.07f)
                   .blend(SkBlendMode::kOverlay)
                   .cache(Cache::Texture)
                   .zIndex(2));

    // frame: a soft plus-blended halo under a crisp cyan keyline
    root.child(box()
                   .rect(SkRect::MakeXYWH(kPX, kPY, kPW, kPH))
                   .shape(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
                   .stroke(LayeredBrush{{
                       {14,
                        mskia::withAlpha(kCyan, 0.09f),
                        8,
                        {},
                        0,
                        SkBlendMode::kPlus},
                       {5,
                        mskia::withAlpha(kCyan, 0.22f),
                        2.6f,
                        {},
                        0,
                        SkBlendMode::kPlus},
                       {2.4f, mskia::withAlpha(hexColor(0xCFF2F5), 0.95f)},
                   }})
                   .zIndex(6));

    // the inner contour: a thin line whose dipped centre IS the header rule
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kPX + kInset, kPY + kInset, kPW - 2 * kInset,
                                   kPH - 2 * kInset))
            .shape(panelInner(kInnerCut, kInnerDip, kInnerShoulderL,
                              kInnerShoulderR))
            .stroke(stroke(1.1f, Fill::color(mskia::withAlpha(kCyan, 0.55f))))
            .zIndex(6));
    // and a dotted echo just inside it (the frame's second, ticked pass)
    root.child(box()
                   .rect(SkRect::MakeXYWH(kPX + kInset + 7, kPY + kInset + 7,
                                          kPW - 2 * kInset - 14,
                                          kPH - 2 * kInset - 14))
                   .shape(panelInner(kInnerCut - 4, kInnerDip - 7,
                                     kInnerShoulderL - 7, kInnerShoulderR - 7))
                   .stroke(PathFormat{.width = 1.0f,
                                      .strokeFill = Fill::color(
                                          mskia::withAlpha(kCyan, 0.26f)),
                                      .dashIntervals = {2.0f, 6.0f}})
                   .zIndex(6));
  }

  // -------------------------------------------------------------------
  // header

  void header(Element& root) {
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kPX, kPY + 20, kPW, kRuleY - kPY - 22))
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .zIndex(7)
            .child(text(toUtf8("CONTACT BEAM"))
                       .font(benchType(31, kTitle, 0.10f))
                       .key("title")
                       .fx({.effect = fx::typeOn(),
                            .stagger = {.eachMs = 26, .durationMs = 190},
                            .progress = animate(from(0.0f).to(1.0f), {760ms})})
                       .effect(styles::textGlow(mskia::withAlpha(kCyan, 0.5f),
                                                5.0f))));

    // under the rule: the repair caption at left, and at right the RIG's
    // integrity as an ANNULAR GAUGE — shapes::sector is a closed wedge, so
    // the track and the fill are the same generator twice
    root.child(
        box()
            .at({kPX + 34, kRuleY + 13})
            .zIndex(7)
            .child(text(toUtf8("NANOCIRCUIT REPAIR · TIER III"))
                       .font(benchType(10.5f, mskia::withAlpha(kCyan, 0.5f),
                                       0.2f, false))));
    const float gaugeD = 26, gaugeX = 786, gaugeY = kRuleY + 6;
    // 359.99, not 360: shapes::sector() with a full-turn sweep produces an
    // EMPTY path (SkPathBuilder::arcTo swallows |sweep| == 360), so the
    // gauge's own track — the most obvious call there is — silently
    // disappears at the natural value.
    root.child(box()
                   .rect(SkRect::MakeXYWH(gaugeX, gaugeY, gaugeD, gaugeD))
                   .shape(shapes::sector(0, 359.99f, 0.58f))
                   .fill(Paint::solid(mskia::withAlpha(kCyan, 0.18f)))
                   .zIndex(7));
    root.child(box()
                   .rect(SkRect::MakeXYWH(gaugeX, gaugeY, gaugeD, gaugeD))
                   .shape(shapes::sector(-90, 360 * 0.78f, 0.58f))
                   .fill(Paint::solid(mskia::withAlpha(kCyan, 0.9f)))
                   .zIndex(7));
    root.child(
        box()
            .at({gaugeX + 34, kRuleY + 13})
            .zIndex(7)
            .child(text(toUtf8("R.I.G. INTEGRITY 78%"))
                       .font(benchType(10.5f, mskia::withAlpha(kCyan, 0.5f),
                                       0.2f, false))));
  }

  // -------------------------------------------------------------------
  // the entry socket: the bracket-and-arrow the current flows in through

  void entrySocket(Element& root, SkPoint at, bool big) {
    if (!big) {
      // A pure function of the node's size: it closes over nothing, so its
      // own name is the whole of its identity.
      root.child(
          custom("socket.arrow",
                 [](SkCanvas& canvas, const PaintContext& ctx) {
                   const float w = ctx.size.width(), h = ctx.size.height();
                   SkPaint p;
                   p.setAntiAlias(true);
                   p.setColor4f(mskia::withAlpha(kCyan, 0.8f), nullptr);
                   SkPathBuilder t;
                   t.moveTo(0, h * 0.16f);
                   t.lineTo(w * 0.8f, h * 0.5f);
                   t.lineTo(0, h * 0.84f);
                   t.close();
                   canvas.drawPath(t.detach(), p);
                 })
              .rect(SkRect::MakeXYWH(at.fX - 24, at.fY - 9, 16.0f, 18.0f))
              .opacity(&socketPulse)
              .zIndex(8));
      return;
    }
    root.child(
        custom("socket.housing",
               [](SkCanvas& canvas, const PaintContext& ctx) {
                 const float w = ctx.size.width(), h = ctx.size.height();
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setStyle(SkPaint::kStroke_Style);
                 p.setStrokeWidth(1.6f);
                 p.setColor4f(mskia::withAlpha(kCyan, 0.78f), nullptr);
                 // the socket housing: a rectangle broken on the left, where
                 // the feed enters
                 SkPathBuilder b;
                 b.moveTo(w * 0.30f, h * 0.34f);
                 b.lineTo(w * 0.30f, h * 0.06f);
                 b.lineTo(w * 0.99f, h * 0.06f);
                 b.lineTo(w * 0.99f, h * 0.94f);
                 b.lineTo(w * 0.30f, h * 0.94f);
                 b.lineTo(w * 0.30f, h * 0.66f);
                 // the inner bracket
                 b.moveTo(w * 0.58f, h * 0.26f);
                 b.lineTo(w * 0.44f, h * 0.26f);
                 b.lineTo(w * 0.44f, h * 0.74f);
                 b.lineTo(w * 0.58f, h * 0.74f);
                 canvas.drawPath(b.detach(), p);
                 SkPathBuilder t;
                 t.moveTo(w * 0.02f, h * 0.31f);
                 t.lineTo(w * 0.24f, h * 0.50f);
                 t.lineTo(w * 0.02f, h * 0.69f);
                 t.close();
                 p.setStyle(SkPaint::kFill_Style);
                 canvas.drawPath(t.detach(), p);
               })
            .rect(SkRect::MakeXYWH(at.fX - 100, at.fY - 46, 108.0f, 92.0f))
            .opacity(&socketPulse)
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // one circuit: traces first (so node glow sits on the wire), then nodes

  void circuit(Element& root, const Circuit& c) {
    auto wires =
        box().inset(0).zIndex(4).staggerChildren(30ms, Spread::From::Start);
    for (int i = 0; i < c.edgeCount; ++i) {
      const EdgeDef& e = c.edges[i];
      wires.child(
          connector(c.key(e.a), c.key(e.b), pcb(9.0f, e.jog))
              .key(std::string(c.tag) + "e" + std::to_string(i))
              .inset(0)
              .mask(
                  by::spans(spans::upTo(animate(from(0.0f).to(1.0f), {620ms}))))
              .stroke(LayeredBrush{{{7.0f,
                                     mskia::withAlpha(kCyan, 0.075f),
                                     3.4f,
                                     {},
                                     0,
                                     SkBlendMode::kPlus}}})
              .stroke(lines::presets::cased(
                  1.2f, Fill::color(mskia::withAlpha(kCyan, c.traceAlpha)),
                  c.typedDia > 24 ? 4.2f : 3.4f)));
    }
    root.child(std::move(wires));

    auto layer =
        box().inset(0).zIndex(5).staggerChildren(30ms, Spread::From::Start);
    for (int i = 0; i < c.nodeCount; ++i) {
      const SkPoint at = c.at(i);
      const KindArt art = artOf(c.nodes[i].kind);
      const bool typed = c.nodes[i].kind != Blank;
      const float dia = typed ? c.typedDia : c.blankDia;

      const sdf::Style st{
          .fill = toColor(art.fill),
          .borderWidth = typed ? 2.4f : 1.7f,
          .borderColor = toColor(art.ring),
          .glowRadius = typed ? 5.2f : 3.4f,
          .glowColor = toColor(mskia::withAlpha(kCyan, typed ? 0.32f : 0.22f)),
          .shadowOffset = {0, 0},
          .shadowBlur = typed ? 6.0f : 4.5f,
          .shadowColor = toColor(hexColor(0x01080A, 1.0f))};
      Paint m = Paint::recipe(sdf::material(sdf::circle(), st))
                    .uniform("uGlowR",
                             &glow[(size_t)(glowSlot++ % (int)glow.size())]);

      const float boxSize = sdf::minBoxFor(st, dia);
      layer.child(box()
                      .key(c.key(i))
                      .width(Dimension(boxSize))
                      .height(Dimension(boxSize))
                      .centerAt(at)
                      .fill(std::move(m))
                      .opacity(animate(from(0.0f).to(1.0f), {260ms}))
                      .scale(animate(
                          from(0.72f).to(1.0f),
                          Transition{.duration = 260ms,
                                     .ease =
                                         [](float t) {
                                           return choreograph::easeOutBack(t);
                                         }}))
                      .zIndex(typed ? 3 : 2));

      if (!typed) continue;
      // the speckled corona + the type label, both keyed leaves: the
      // instancing atlas has no per-instance string, so labels stay text
      layer.child(
          box()
              .width(Dimension(dia + 24))
              .height(Dimension(dia + 24))
              .centerAt(at)
              .shape(burst(24, 0.72f))
              .stroke(stroke(0.9f, Fill::color(mskia::withAlpha(kCyan, 0.20f))))
              .opacity(animate(from(0.0f).to(1.0f), {320ms}))
              .zIndex(4));
      layer.child(
          box()
              .width(Dimension(dia * 0.42f))
              .height(Dimension(dia * 0.42f))
              .centerAt({at.fX - dia * 0.09f, at.fY - dia * 0.10f})
              .fill(Paint::radial({dia * 0.21f, dia * 0.21f}, dia * 0.28f,
                                  {{0.0f, mskia::withAlpha(art.ring, 0.42f)},
                                   {1.0f, mskia::withAlpha(art.ring, 0.0f)}}))
              .zIndex(5));
      layer.child(text(toUtf8(art.label))
                      .font(benchType(c.labelSize,
                                      mskia::withAlpha(kCyan, 0.78f), 0.11f))
                      .centerAt({at.fX + dia * 0.88f, at.fY + c.labelDy})
                      .opacity(animate(from(0.0f).to(1.0f), {320ms}))
                      .zIndex(5));
    }
    root.child(std::move(layer));

    entrySocket(root, c.at(c.entryIndex), c.bigSocket);

    if (!c.caption) return;
    int typedCount = 0;
    for (int i = 0; i < c.nodeCount; ++i)
      typedCount += c.nodes[i].kind != Blank;
    const std::string slots =
        kit::formatted("%d / %d NODES", typedCount, c.nodeCount);
    // The rule spans the tree it heads, and the caption row spans the rule:
    // name at the left end, count at the right. Butting the count against
    // the name puts it under the weapon circuit's low-hanging DMG node,
    // whose corona then eats the digits — and the lattice geometry is read
    // off the reference frame, so it is the caption that moves.
    constexpr float kRuleW = 350.0f;
    root.child(
        box()
            .at({c.x0 - 34, c.y0 - 58})
            .width(Dimension(kRuleW))
            .row()
            .alignItems(Align::Center)
            .justify(Justify::SpaceBetween)
            .zIndex(8)
            .child(
                text(toUtf8(c.caption))
                    .font(benchType(11, mskia::withAlpha(kCyan, 0.62f), 0.18f)))
            .child(text(toUtf8(slots))
                       .font(benchType(9.5f, mskia::withAlpha(kCyan, 0.4f),
                                       0.18f, false))));
    root.child(
        box()
            .rect(SkRect::MakeXYWH(c.x0 - 34, c.y0 - 32, kRuleW, 1.0f))
            .shape(hline())
            .stroke(stroke(1.0f, Fill::color(mskia::withAlpha(kCyan, 0.28f))))
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // legend: bracket-framed stat rows with instanced chevron pips
  //
  // NOT a readout and not a table: between the specification and its value
  // stand a kind marker whose fill is a radial ramp and a load bar that is
  // an instanced pool, and a table's columns carry words. What is shared
  // between the rows is the four widths, which is what the row says.

  Element statRow(int r) {
    const StatRow& s = kStats[r];
    const KindArt art = artOf(s.kind);
    const float barW = (float)s.total * kPipW + (float)(s.total - 1) * kPipGap;
    return box()
        .row()
        .alignItems(Align::Center)
        .height(Dimension(24.0f))
        .child(box()
                   .width(Dimension(160.0f))
                   .alignItems(Align::End)
                   .child(text(toUtf8(s.label))
                              .font(benchType(
                                  14, mskia::withAlpha(kCyan, 0.95f), 0.10f))))
        .child(box()
                   .width(Dimension(9.0f))
                   .height(Dimension(9.0f))
                   .margin(13, 0, 13, 0)
                   .shape(shapes::polygon(12))
                   .fill(Paint::radial({4.5f, 4.5f}, 5.0f,
                                       {{0.0f, art.ring}, {1.0f, art.fill}})))
        .child(box()
                   .width(Dimension(barW))
                   .height(Dimension(kPipH))
                   .opacity(animate(from(0.0f).to(1.0f), {320ms}))
                   .translateX(animate(from(-16.0f).to(0.0f), {380ms}))
                   .child(instancing::instances(pips, pipPools[(size_t)r])))
        .child(box().grow(1))
        .child(box()
                   .width(Dimension(84.0f))
                   .child(text(toUtf8(s.value))
                              .font(benchType(13, hexColor(0xDCEEF2), 0.02f,
                                              false))));
  }

  void legend(Element& root) {
    auto card =
        box()
            .key("legend")
            .rect(SkRect::MakeXYWH(kLegX, kBandY, kLegW, kBandH))
            .fill(Paint::blend(
                {{Paint::solid(mskia::withAlpha(kStrip, 0.6f)),
                  SkBlendMode::kSrcOver},
                 {scanField(mskia::withAlpha(kCyan, 0.05f), 3.0f, &scanClock),
                  SkBlendMode::kScreen}}))
            .shape(chamfer(12))
            .zIndex(7)
            .column()
            .padding(20, 12)
            .gap(3)
            .staggerChildren(70ms, Spread::From::Start);

    // the column heads: one register on the row, three bare runs under it
    card.child(
        box()
            .row()
            .height(Dimension(14.0f))
            .font(benchType(9, mskia::withAlpha(kCyan, 0.42f), 0.22f, false))
            .child(box()
                       .width(Dimension(160.0f))
                       .alignItems(Align::End)
                       .child(text(toUtf8("SPECIFICATION"))))
            .child(box().width(Dimension(35.0f)))
            .child(text(toUtf8("NANOCIRCUIT LOAD")))
            .child(box().grow(1))
            .child(box().width(Dimension(84.0f)).child(text(toUtf8("VALUE")))));
    for (int r = 0; r < kStatCount; ++r) card.child(statRow(r));
    root.child(std::move(card));

    root.child(
        box()
            .rect(SkRect::MakeXYWH(kLegX - 8, kBandY - 8, kLegW + 16,
                                   kBandH + 16))
            .shape(cornerBrackets(26))
            .stroke(stroke(1.5f, Fill::color(mskia::withAlpha(kCyan, 0.72f))))
            .zIndex(8));
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kLegX + 16, kBandY + 32, kLegW - 32, 1.0f))
            .shape(hline())
            .stroke(stroke(1.0f, Fill::color(mskia::withAlpha(kCyan, 0.26f))))
            .zIndex(8));
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kLegX + kLegW - 108, kBandY + 14, 1.0f,
                                   kBandH - 28))
            .shape(vline())
            .stroke(stroke(1.0f, Fill::color(mskia::withAlpha(kCyan, 0.26f))))
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // the NODES counter — the one warm-metal object on an all-cyan screen

  void counter(Element& root) {
    root.child(
        box()
            .key("counter")
            .rect(SkRect::MakeXYWH(kCntX, kBandY, kCntW, kBandH))
            .shape(chamfer(12))
            .fill(Paint::blend(
                {{Paint::solid(mskia::withAlpha(kStrip, 0.6f)),
                  SkBlendMode::kSrcOver},
                 {scanField(mskia::withAlpha(kCyan, 0.05f), 3.0f, &scanClock),
                  SkBlendMode::kScreen}}))
            .column()
            .alignItems(Align::Center)
            .padding(16, 11)
            .gap(2)
            .zIndex(7)
            .child(
                box()
                    .width(Dimension(112.0f))
                    .height(Dimension(21.0f))
                    .alignItems(Align::Center)
                    .justify(Justify::Center)
                    .shape(chamfer(6))
                    .stroke(stroke(1.0f,
                                   Fill::color(mskia::withAlpha(kCyan, 0.45f))))
                    .child(text(toUtf8("NODES"))
                               .font(benchType(
                                   12, mskia::withAlpha(kCyan, 0.95f), 0.16f))))
            // the brass power-node puck: side wall, top face, bore ring
            .child(
                box()
                    .width(Dimension(66.0f))
                    .height(Dimension(46.0f))
                    .margin(0, 6, 0, 0)
                    .child(
                        box()
                            .rect(SkRect::MakeXYWH(2.0f, 14.0f, 62.0f, 28.0f))
                            .corners({14})
                            .fill(Paint::linear({0, 0}, {0, 28},
                                                {{0.0f, kBrassLo},
                                                 {0.45f, hexColor(0x7E6318)},
                                                 {1.0f, kBrassDk}})))
                    .child(
                        box()
                            .rect(SkRect::MakeXYWH(2.0f, 2.0f, 62.0f, 27.0f))
                            .shape(shapes::squircle(2.0f))
                            .fill(Paint::linear({0, 0}, {52, 27},
                                                {{0.0f, kBrassHi},
                                                 {0.4f, hexColor(0xD3AA33)},
                                                 {1.0f, hexColor(0x8E6F1E)}}))
                            .stroke(stroke(
                                1.0f, Fill::color(hexColor(0xF3DC94, 0.75f)))))
                    .child(
                        box()
                            .rect(SkRect::MakeXYWH(22.0f, 8.0f, 24.0f, 13.0f))
                            .shape(shapes::squircle(2.0f))
                            .stroke(stroke(
                                1.3f, Fill::color(hexColor(0x74590F, 0.9f))))))
            .child(text(toUtf8("2"))
                       .font(benchType(40, kTitle, 0.0f))
                       .key("nodecount")
                       .transition({.duration = 200ms})));

    root.child(
        box()
            .rect(SkRect::MakeXYWH(kCntX - 8, kBandY - 8, kCntW + 16,
                                   kBandH + 16))
            .shape(cornerBrackets(22))
            .stroke(stroke(1.5f, Fill::color(mskia::withAlpha(kCyan, 0.72f))))
            .zIndex(8));
  }

  // -------------------------------------------------------------------
  // the hardware hint row along the panel's bottom rail

  void hints(Element& root) {
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kPX + 32, kHintY, kPW - 64, 1.0f))
            .shape(hline())
            .stroke(stroke(1.0f, Fill::color(mskia::withAlpha(kCyan, 0.36f))))
            .zIndex(8));

    // the hints: one register on the row, bare runs under it
    root.child(
        box()
            .rect(SkRect::MakeXYWH(kPX, kHintY + 8, kPW, 26.0f))
            .row()
            .alignItems(Align::Center)
            .justify(Justify::Center)
            .gap(56)
            .zIndex(8)
            .font(benchType(12, mskia::withAlpha(kCyan, 0.78f), 0.06f, false))
            .child(
                box()
                    .row()
                    .alignItems(Align::Center)
                    .gap(8)
                    .child(
                        // KEYLESS, and it has to be: the inner disc breathes
                        // off the paint's own clock, which no key can name.
                        custom([](SkCanvas& canvas, const PaintContext& ctx) {
                          const float r = ctx.size.width() * 0.5f;
                          SkPaint p;
                          p.setAntiAlias(true);
                          p.setStyle(SkPaint::kStroke_Style);
                          p.setStrokeWidth(1.3f);
                          p.setColor4f(mskia::withAlpha(kCyan, 0.82f), nullptr);
                          canvas.drawCircle(r, r, r - 1.1f, p);
                          const float k =
                              0.5f +
                              0.5f * std::sin((float)ctx.elapsedSeconds * 3.4f);
                          p.setStyle(SkPaint::kFill_Style);
                          p.setColor4f(mskia::withAlpha(kCyan, 0.3f + 0.5f * k),
                                       nullptr);
                          canvas.drawCircle(r, r, r * 0.4f, p);
                          p.setStyle(SkPaint::kStroke_Style);
                          for (int i = 0; i < 4; ++i) {
                            const SkPoint in = arrange::onRing(
                                (size_t)i, 4, {r, r}, {r * 0.6f, r * 0.6f},
                                0.0f, 6.2831853f, arrange::Turn::Closed);
                            const SkPoint out = arrange::onRing(
                                (size_t)i, 4, {r, r}, {r * 0.9f, r * 0.9f},
                                0.0f, 6.2831853f, arrange::Turn::Closed);
                            canvas.drawLine(in.fX, in.fY, out.fX, out.fY, p);
                          }
                        })
                            .width(Dimension(15.0f))
                            .height(Dimension(15.0f))
                            .cache(Cache::None))
                    .child(text(toUtf8("Navigate"))))
            .child(text(toUtf8("[Enter] Select")))
            .child(text(toUtf8("[Esc] Exit"))));

    // the empty hardware sockets the bezel carries at its bottom corners
    for (float x : {kPX + 34, kPR - 46}) {
      root.child(
          box()
              .rect(SkRect::MakeXYWH(x, kHintY + 13, 12.0f, 12.0f))
              .stroke(stroke(1.2f, Fill::color(mskia::withAlpha(kCyan, 0.45f))))
              .zIndex(8));
    }
  }

  // -------------------------------------------------------------------
  // the CRT overlay: the refresh bar that warps what it passes over

  void overlay(Element& root) {
    root.child(box()
                   .rect(SkRect::MakeXYWH(kPX + 6, kPY + 30, kPW - 12, 34.0f))
                   .translateY(&scanY)
                   .backdrop(styles::ripple(1.0f, 130.0f, 0.0f))
                   .fill(Paint::linear({0, 0}, {0, 34},
                                       {{0.0f, mskia::withAlpha(kCyan, 0.0f)},
                                        {0.5f, mskia::withAlpha(kCyan, 0.05f)},
                                        {1.0f, mskia::withAlpha(kCyan, 0.0f)}}))
                   .blend(SkBlendMode::kPlus)
                   .cache(Cache::None)
                   .zIndex(9));
    root.child(box()
                   .inset(0)
                   .fill(Paint::radial({kW * 0.5f, kH * 0.46f}, kW * 0.60f,
                                       {{0.0f, hexColor(0x000000, 0.0f)},
                                        {0.55f, hexColor(0x000000, 0.14f)},
                                        {1.0f, hexColor(0x01050A, 0.86f)}}))
                   .zIndex(11));
  }

  // -------------------------------------------------------------------

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    glowSlot = 0;
    auto root = stack().fill(Paint::radial({kW * 0.5f, kH * 0.5f}, 880,
                                           {{0.0f, hexColor(0x09131B)},
                                            {0.6f, hexColor(0x050B11)},
                                            {1.0f, hexColor(0x020406)}}));
    backdrop(root);

    // everything that belongs to the hologram rides one jittering group,
    // so a glitch reads as "the whole panel stuttered"
    auto holo = box()
                    .inset(0)
                    .translateX(&jitterX)
                    .opacity(&holoAlpha)
                    .scale(animate(from(0.955f).to(1.0f), {380ms}))
                    .transformOriginPx({kW * 0.5f, kH * 0.5f})
                    .zIndex(2);
    plate(holo);
    header(holo);
    circuit(holo, kBeam);
    circuit(holo, kRig);
    circuit(holo, kStasis);
    legend(holo);
    counter(holo);
    hints(holo);
    overlay(holo);
    root.child(std::move(holo));
    return root;
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = SkSize::Make((int)kW, (int)kH),
                             .captureAt = 2.5,
                             .background = hexColor(0x02060A)});
    bakePips();

    ctx.ticker.add([this, &ticker = ctx.ticker](double) {
      const double t = ticker.elapsed();
      for (size_t i = 0; i < glow.size(); ++i)
        glow[i] = 6.0f + 2.1f * (float)std::sin(t * 2.75 + (double)i * 0.62);
      socketPulse = 0.78f + 0.22f * (float)std::sin(t * 3.9);
      scanY = (float)std::fmod(t * 96.0, (double)(kPH - 40));
      scanClock = steppedTime(t);

      // the hologram stutter: ~100 ms of snap (not eased) jitter every
      // 4-7 s, scheduled deterministically so a capture is reproducible
      if (t >= nextGlitch && t > glitchEnd) {
        glitchEnd = t + 0.10;
        nextGlitch = t + 4.0 + std::fmod(t * 7.31, 3.0);
      }
      if (t < glitchEnd) {
        const int step = (int)((glitchEnd - t) * 30.0) % 3;
        jitterX = step == 0 ? 3.0f : step == 1 ? -2.0f : 1.0f;
        holoAlpha = 0.76f;
      } else {
        jitterX = 0.0f;
        holoAlpha = 1.0f;
      }
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) override {}
};

SIGIL_SKETCH(
    Ds2Bench, "Study \xc2\xb7 Game UI",
    "Dead Space 2's Nanocircuit bench (2011) \xe2\x80\x94 routers, rails, "
    "a diegetic hologram")
