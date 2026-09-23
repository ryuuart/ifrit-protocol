// A Dead Space upgrade bench composed from routed circuit nodes and status
// panels.

// TAGS: Geometry/Diagrams, Interfaces/Game

#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/color/Color.h>

#include "Circuit.h"

struct Ds2Bench {
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
  std::shared_ptr<instancing::CellSheet> pips =
      std::make_shared<instancing::CellSheet>(2.0f);
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
            .stroke(stroke(1.0f,
                           Fill::color(sigil::material::withAlpha(kCyan, 0.5f)),
                           PathFormat::Align::Inner));
    Element empty = box().shape(chevron()).stroke(
        stroke(1.2f, Fill::color(sigil::material::withAlpha(kCyan, 0.30f)),
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
        tints[i] = on ? sigil::material::Color{0.66f + 0.34f * art.ring.r,
                                               0.66f + 0.34f * art.ring.g,
                                               0.66f + 0.34f * art.ring.b, 1.0f}
                      : sigil::material::Color{1, 1, 1, 1};
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
      room.children(
          {box()
               .rect(SkRect::MakeXYWH(x, -40.0f, w, kH + 80))
               .fill(Paint::linear({0, 0}, {0, kH},
                                   {{0.0f, hexColor(0x16262F, a * 0.35f)},
                                    {0.38f, hexColor(0x1C303C, a)},
                                    {1.0f, hexColor(0x080F16, a * 0.2f)}}))
               // the strut melts vertically out of focus: 18 along
               // the vertical, 12 across
               .filter(Effect::directionalBlur(18, 90, 12))
               .zIndex(0)});
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
    room.children({box()
                       .rect(SkRect::MakeXYWH(250.0f, 118.0f, 176.0f, 470.0f))
                       .fill(Paint::linear({0, 0}, {0, 470},
                                           {{0.0f, hexColor(0x2A4A52, 0.34f)},
                                            {0.45f, hexColor(0x3E6A6E, 0.26f)},
                                            {1.0f, hexColor(0x0C1A20, 0.09f)}}))
                       .filter(Effect::directionalBlur(22, 90, 16))
                       .zIndex(0)});
    // …and a bank of pipes crossing the wall behind the right half.
    for (int i = 0; i < 5; ++i)
      room.children(
          {box()
               .rect(SkRect::MakeXYWH(560.0f, 150.0f + (float)i * 96.0f, 560.0f,
                                      13.0f))
               .fill(Paint::linear({0, 0}, {0, 13},
                                   {{0.0f, hexColor(0x25444E, 0.30f)},
                                    {0.5f, hexColor(0x1A3038, 0.20f)},
                                    {1.0f, hexColor(0x0A1218, 0.07f)}}))
               .filter(Effect::directionalBlur(9, 0, 14))
               .zIndex(0),
           box()
               .rect(SkRect::MakeXYWH(-40.0f, 2.0f, kW + 80, 28.0f))
               .fill(Paint::linear({0, 0}, {0, 28},
                                   {{0.0f, hexColor(0x243B47, 0.5f)},
                                    {1.0f, hexColor(0x0A141C, 0.25f)}}))
               // 10 along the vertical, 7 across
               .filter(Effect::directionalBlur(10, 90, 7))
               .zIndex(0)});
    root.children({std::move(room)});
  }

  // -------------------------------------------------------------------
  // the panel plate + its frame vocabulary

  void plate(Element& root) {
    // body: solid + a radial lift + the live scanline field, ONE shader
    // the grain the compressed CRT capture carries — enough to kill the
    // "clean vector art" read without becoming VHS noise
    // The grain is a STATIC procedural shader over the whole panel, and
    // opacity + kOverlay makes the library refuse to bake it automatically,
    // so it is evaluated per pixel per frame unless asked. It never changes,
    // hence the explicit bake. (The plate BASE below it stays live on
    // purpose: its fill carries a scrolling scanField shader.)
    // frame: a soft plus-blended halo under a crisp cyan keyline
    // the inner contour: a thin line whose dipped centre IS the header rule
    // and a dotted echo just inside it (the frame's second, ticked pass)
    root.children(
        {box()
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
                 {{Paint::solid(sigil::material::withAlpha(kBody, 0.70f)),
                   SkBlendMode::kSrcOver},
                  // unit-square ramp: the lift is authored against the
                  // box, not against a pixel extent transcribed by hand
                  {Paint::radialUnit({0.40f, 0.32f}, 1.15f,
                                     {{0.0f, hexColor(0xFFFFFF)},
                                      {0.5f, hexColor(0xC0D0D0)},
                                      {1.0f, hexColor(0x4E6264)}}),
                   SkBlendMode::kMultiply},
                  {scanField(sigil::material::withAlpha(kCyan, 0.075f), 3.0f,
                             &scanClock),
                   SkBlendMode::kScreen}}))
             .zIndex(1),
         box()
             .rect(SkRect::MakeXYWH(kPX, kPY, kPW, kPH))
             .shape(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
             .fill(grain)
             .opacity(0.07f)
             .blendMode(SkBlendMode::kOverlay)
             .cache(Cache::Texture)
             .zIndex(2),
         box()
             .rect(SkRect::MakeXYWH(kPX, kPY, kPW, kPH))
             .shape(panelOuter(kOuterCut, kOuterStep, kOuterShoulder))
             .stroke(LayeredBrush{{
                 {14,
                  sigil::material::withAlpha(kCyan, 0.09f),
                  8,
                  {},
                  0,
                  SkBlendMode::kPlus},
                 {5,
                  sigil::material::withAlpha(kCyan, 0.22f),
                  2.6f,
                  {},
                  0,
                  SkBlendMode::kPlus},
                 {2.4f, sigil::material::withAlpha(hexColor(0xCFF2F5), 0.95f)},
             }})
             .zIndex(6),
         box()
             .rect(SkRect::MakeXYWH(kPX + kInset, kPY + kInset,
                                    kPW - 2 * kInset, kPH - 2 * kInset))
             .shape(panelInner(kInnerCut, kInnerDip, kInnerShoulderL,
                               kInnerShoulderR))
             .stroke(stroke(
                 1.1f, Fill::color(sigil::material::withAlpha(kCyan, 0.55f))))
             .zIndex(6),
         box()
             .rect(SkRect::MakeXYWH(kPX + kInset + 7, kPY + kInset + 7,
                                    kPW - 2 * kInset - 14,
                                    kPH - 2 * kInset - 14))
             .shape(panelInner(kInnerCut - 4, kInnerDip - 7,
                               kInnerShoulderL - 7, kInnerShoulderR - 7))
             .stroke(PathFormat{.width = 1.0f,
                                .strokeFill = Fill::color(
                                    sigil::material::withAlpha(kCyan, 0.26f)),
                                .dashIntervals = {2.0f, 6.0f}})
             .zIndex(6)});
  }

  // -------------------------------------------------------------------
  // header

  void header(Element& root) {
    // under the rule: the repair caption at left, and at right the RIG's
    // integrity as an ANNULAR GAUGE — shapes::sector is a closed wedge, so
    // the track and the fill are the same generator twice
    root.children(
        {kit::centred()
             .rect(SkRect::MakeXYWH(kPX, kPY + 20, kPW, kRuleY - kPY - 22))

             .zIndex(7)
             .children(
                 {document::h1("CONTACT BEAM")
                      .key("title")
                      .textFx(
                          {.effect = textFx::typeOn(),
                           .stagger = {.eachMs = 26, .durationMs = 190},
                           .progress = animate(from(0.0f).to(1.0f), {760ms})})
                      .filter(styles::textGlow(
                          sigil::material::withAlpha(kCyan, 0.5f), 5.0f))}),
         box()
             .at({kPX + 34, kRuleY + 13})
             .zIndex(7)
             .children({document::lead("NANOCIRCUIT REPAIR · TIER III")})});
    const float gaugeD = 26, gaugeX = 786, gaugeY = kRuleY + 6;
    // 359.99, not 360: shapes::sector() with a full-turn sweep produces an
    // EMPTY path (SkPathBuilder::arcTo swallows |sweep| == 360), so the
    // gauge's own track — the most obvious call there is — silently
    // disappears at the natural value.
    root.children(
        {box()
             .rect(SkRect::MakeXYWH(gaugeX, gaugeY, gaugeD, gaugeD))
             .shape(shapes::sector(0, 359.99f, 0.58f))
             .fill(Paint::solid(sigil::material::withAlpha(kCyan, 0.18f)))
             .zIndex(7),
         box()
             .rect(SkRect::MakeXYWH(gaugeX, gaugeY, gaugeD, gaugeD))
             .shape(shapes::sector(-90, 360 * 0.78f, 0.58f))
             .fill(Paint::solid(sigil::material::withAlpha(kCyan, 0.9f)))
             .zIndex(7),
         box()
             .at({gaugeX + 34, kRuleY + 13})
             .zIndex(7)
             .children({document::lead("R.I.G. INTEGRITY 78%")})});
  }

  // -------------------------------------------------------------------
  // the entry socket: the bracket-and-arrow the current flows in through

  void entrySocket(Element& root, SkPoint at, bool big) {
    if (!big) {
      // A pure function of the node's size: it closes over nothing, so its
      // own name is the whole of its identity.
      root.children(
          {pen(
               "socket.arrow",
               [](Pen& q) {
                 const float w = q.width, h = q.height;
                 q.noStroke();
                 q.fill(sigil::material::withAlpha(kCyan, 0.8f));
                 q.triangle(0, h * 0.16f, w * 0.8f, h * 0.5f, 0, h * 0.84f);
               },
               Cache::Texture)
               .rect(SkRect::MakeXYWH(at.fX - 24, at.fY - 9, 16.0f, 18.0f))
               .opacity(&socketPulse)
               .zIndex(8)});
      return;
    }
    root.children(
        {pen(
             "socket.housing",
             [](Pen& q) {
               const float w = q.width, h = q.height;
               q.noFill();
               q.stroke(sigil::material::withAlpha(kCyan, 0.78f));
               q.strokeWeight(1.6f);
               // the housing: a rectangle broken on the left, where the
               // feed enters, and the bracket that receives it
               q.beginShape();
               for (SkPoint at : {SkPoint{w * 0.30f, h * 0.34f},
                                  {w * 0.30f, h * 0.06f},
                                  {w * 0.99f, h * 0.06f},
                                  {w * 0.99f, h * 0.94f},
                                  {w * 0.30f, h * 0.94f},
                                  {w * 0.30f, h * 0.66f}})
                 q.vertex(at.fX, at.fY);
               q.endShape();
               q.beginShape();
               for (SkPoint at : {SkPoint{w * 0.58f, h * 0.26f},
                                  {w * 0.44f, h * 0.26f},
                                  {w * 0.44f, h * 0.74f},
                                  {w * 0.58f, h * 0.74f}})
                 q.vertex(at.fX, at.fY);
               q.endShape();
               q.noStroke();
               q.fill(sigil::material::withAlpha(kCyan, 0.78f));
               q.triangle(w * 0.02f, h * 0.31f, w * 0.24f, h * 0.50f, w * 0.02f,
                          h * 0.69f);
             },
             Cache::Texture)
             .rect(SkRect::MakeXYWH(at.fX - 100, at.fY - 46, 108.0f, 92.0f))
             .opacity(&socketPulse)
             .zIndex(8)});
  }

  // -------------------------------------------------------------------
  // one circuit: traces first (so node glow sits on the wire), then nodes

  void circuit(Element& root, const Circuit& c) {
    // The traces are operators of the box that holds the pads: each is
    // built from where its two pads settled, and revealed 30ms behind the
    // one before it — the stagger written on each wire's own reveal, since
    // what a trace draws belongs to the trace and not to a container.
    std::vector<Operator> wires;
    for (int i = 0; i < c.edgeCount; ++i) {
      const EdgeDef& e = c.edges[i];
      wires.push_back(
          Operator(
              connect::Between{
                  .from = c.key(e.a),
                  .to = c.key(e.b),
                  .router = pcb(9.0f, e.jog),
                  .mask = by::spans(spans::upTo(animate(
                      from(0.0f).to(1.0f),
                      Transition{.duration = 620ms, .delay = 30ms * i}))),
                  .style =
                      LayerStyle{
                          .over = {LayeredBrush{{{7.0f,
                                                  sigil::material::withAlpha(
                                                      kCyan, 0.075f),
                                                  3.4f,
                                                  {},
                                                  0,
                                                  SkBlendMode::kPlus}}},
                                   lines::presets::cased(
                                       1.2f,
                                       Fill::color(sigil::material::withAlpha(
                                           kCyan, c.traceAlpha)),
                                       c.typedDia > 24 ? 4.2f : 3.4f)}},
                  .key = std::string(c.tag) + "e" + std::to_string(i)})
              .zIndex(4));
    }
    root.operators(std::move(wires));

    auto layer =
        box().inset(0).zIndex(5).staggerChildren(30ms, Spread::From::Start);
    for (int i = 0; i < c.nodeCount; ++i) {
      const SkPoint at = c.at(i);
      const KindArt art = artOf(c.nodes[i].kind);
      const bool typed = c.nodes[i].kind != Blank;
      const float dia = typed ? c.typedDia : c.blankDia;

      const sdf::Style st{
          .fill = art.fill,
          .borderWidth = typed ? 2.4f : 1.7f,
          .borderColor = art.ring,
          .glowRadius = typed ? 5.2f : 3.4f,
          .glowColor = sigil::material::withAlpha(kCyan, typed ? 0.32f : 0.22f),
          .shadowOffset = {0, 0},
          .shadowBlur = typed ? 6.0f : 4.5f,
          .shadowColor = hexColor(0x01080A, 1.0f)};
      Paint m = Paint::recipe(sdf::material(sdf::circle(), st))
                    .uniform("uGlowR",
                             &glow[(size_t)(glowSlot++ % (int)glow.size())]);

      const float boxSize = sdf::minBoxFor(st, dia);
      layer.children(
          {box()
               .key(c.key(i))
               .width(boxSize)
               .height(boxSize)
               .centerAt(at)
               .fill(std::move(m))
               .opacity(animate(from(0.0f).to(1.0f), {260ms}))
               .scale(animate(from(0.72f).to(1.0f),
                              Transition{.duration = 260ms,
                                         .ease =
                                             [](float t) {
                                               return choreograph::easeOutBack(
                                                   t);
                                             }}))
               .zIndex(typed ? 3 : 2)});

      if (!typed) continue;
      // the speckled corona + the type label, both keyed leaves: the
      // instancing atlas has no per-instance string, so labels stay text
      layer.children(
          {box()
               .width(dia + 24)
               .height(dia + 24)
               .centerAt(at)
               .shape(burst(24, 0.72f))
               .stroke(stroke(
                   0.9f, Fill::color(sigil::material::withAlpha(kCyan, 0.20f))))
               .opacity(animate(from(0.0f).to(1.0f), {320ms}))
               .zIndex(4),
           box()
               .width(dia * 0.42f)
               .height(dia * 0.42f)
               .centerAt({at.fX - dia * 0.09f, at.fY - dia * 0.10f})
               .fill(Paint::radial(
                   {dia * 0.21f, dia * 0.21f}, dia * 0.28f,
                   {{0.0f, sigil::material::withAlpha(art.ring, 0.42f)},
                    {1.0f, sigil::material::withAlpha(art.ring, 0.0f)}}))
               .zIndex(5),
           text(art.label)
               .styleClass("node")
               .font({.size = c.labelSize, .track = 0.11f * c.labelSize})
               .centerAt({at.fX + dia * 0.88f, at.fY + c.labelDy})
               .opacity(animate(from(0.0f).to(1.0f), {320ms}))
               .zIndex(5)});
    }
    root.children({std::move(layer)});

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
    root.children(
        {box()
             .at({c.x0 - 34, c.y0 - 58})
             .width(kRuleW)
             .row()
             .alignItems(Align::Center)
             .justifyContent(Justify::SpaceBetween)
             .zIndex(8)
             .children({text(c.caption).styleClass("circuit"),
                        text(slots).styleClass("slots")}),
         box()
             .rect(SkRect::MakeXYWH(c.x0 - 34, c.y0 - 32, kRuleW, 1.0f))
             .shape(hline())
             .stroke(stroke(
                 1.0f, Fill::color(sigil::material::withAlpha(kCyan, 0.28f))))
             .zIndex(8)});
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
        .height(24.0f)
        .children(
            {box()
                 .width(160.0f)
                 .alignItems(Align::End)
                 .children({text(s.label).styleClass("spec")}),
             box()
                 .width(9.0f)
                 .height(9.0f)
                 .margin(0, 13)
                 .shape(shapes::polygon(12))
                 .fill(Paint::radial({4.5f, 4.5f}, 5.0f,
                                     {{0.0f, art.ring}, {1.0f, art.fill}})),
             box()
                 .width(barW)
                 .height(kPipH)
                 .opacity(animate(from(0.0f).to(1.0f), {320ms}))
                 .translateX(animate(from(-16.0f).to(0.0f), {380ms}))
                 .children({instancing::instances(pips, pipPools[(size_t)r])}),
             box().flexGrow(1),
             box().width(84.0f).children({text(s.value).styleClass("value")})});
  }

  void legend(Element& root) {
    auto card =
        box()
            .key("legend")
            .rect(SkRect::MakeXYWH(kLegX, kBandY, kLegW, kBandH))
            .fill(Paint::blend(
                {{Paint::solid(sigil::material::withAlpha(kStrip, 0.6f)),
                  SkBlendMode::kSrcOver},
                 {scanField(sigil::material::withAlpha(kCyan, 0.05f), 3.0f,
                            &scanClock),
                  SkBlendMode::kScreen}}))
            .shape(chamfer(12))
            .zIndex(7)
            .column()
            .padding(12, 20)
            .gap(3)
            .staggerChildren(70ms, Spread::From::Start);

    // the column heads: one register on the row, three bare runs under it
    card.children({box().row().height(14.0f).styleClass("head").children(
        {box()
             .width(160.0f)
             .alignItems(Align::End)
             .children({text("SPECIFICATION")}),
         box().width(35.0f), text("NANOCIRCUIT LOAD"), box().flexGrow(1),
         box().width(84.0f).children({text("VALUE")})})});
    for (int r = 0; r < kStatCount; ++r) card.children({statRow(r)});
    root.children(
        {std::move(card),
         box()
             .rect(SkRect::MakeXYWH(kLegX - 8, kBandY - 8, kLegW + 16,
                                    kBandH + 16))
             .shape(cornerBrackets(26))
             .stroke(stroke(
                 1.5f, Fill::color(sigil::material::withAlpha(kCyan, 0.72f))))
             .zIndex(8),
         box()
             .rect(SkRect::MakeXYWH(kLegX + 16, kBandY + 32, kLegW - 32, 1.0f))
             .shape(hline())
             .stroke(stroke(
                 1.0f, Fill::color(sigil::material::withAlpha(kCyan, 0.26f))))
             .zIndex(8),
         box()
             .rect(SkRect::MakeXYWH(kLegX + kLegW - 108, kBandY + 14, 1.0f,
                                    kBandH - 28))
             .shape(vline())
             .stroke(stroke(
                 1.0f, Fill::color(sigil::material::withAlpha(kCyan, 0.26f))))
             .zIndex(8)});
  }

  // -------------------------------------------------------------------
  // the NODES counter — the one warm-metal object on an all-cyan screen

  void counter(Element& root) {
    root.children(
        {box()
             .key("counter")
             .rect(SkRect::MakeXYWH(kCntX, kBandY, kCntW, kBandH))
             .shape(chamfer(12))
             .fill(Paint::blend(
                 {{Paint::solid(sigil::material::withAlpha(kStrip, 0.6f)),
                   SkBlendMode::kSrcOver},
                  {scanField(sigil::material::withAlpha(kCyan, 0.05f), 3.0f,
                             &scanClock),
                   SkBlendMode::kScreen}}))
             .column()
             .alignItems(Align::Center)
             .padding(11, 16)
             .gap(2)
             .zIndex(7)
             .children({kit::centred()
                            .width(112.0f)
                            .height(21.0f)

                            .shape(chamfer(6))
                            .stroke(stroke(
                                1.0f, Fill::color(sigil::material::withAlpha(
                                          kCyan, 0.45f))))
                            .children({text("NODES").styleClass("nodes")})})
             // the brass power-node puck: side wall, top face, bore ring
             .children(
                 {box()
                      .width(66.0f)
                      .height(46.0f)
                      .margin(6, 0, 0, 0)
                      .children(
                          {box()
                               .rect(
                                   SkRect::MakeXYWH(2.0f, 14.0f, 62.0f, 28.0f))
                               .borderRadius({14})
                               .fill(Paint::linear({0, 0}, {0, 28},
                                                   {{0.0f, kBrassLo},
                                                    {0.45f, hexColor(0x7E6318)},
                                                    {1.0f, kBrassDk}})),
                           box()
                               .rect(SkRect::MakeXYWH(2.0f, 2.0f, 62.0f, 27.0f))
                               .shape(shapes::squircle(2.0f))
                               .fill(
                                   Paint::linear({0, 0}, {52, 27},
                                                 {{0.0f, kBrassHi},
                                                  {0.4f, hexColor(0xD3AA33)},
                                                  {1.0f, hexColor(0x8E6F1E)}}))
                               .stroke(stroke(1.0f, Fill::color(hexColor(
                                                        0xF3DC94, 0.75f)))),
                           box()
                               .rect(
                                   SkRect::MakeXYWH(22.0f, 8.0f, 24.0f, 13.0f))
                               .shape(shapes::squircle(2.0f))
                               .stroke(stroke(1.3f, Fill::color(hexColor(
                                                        0x74590F, 0.9f))))}),
                  text("2")
                      .styleClass("count")
                      .key("nodecount")
                      .transition({.duration = 200ms})}),
         box()
             .rect(SkRect::MakeXYWH(kCntX - 8, kBandY - 8, kCntW + 16,
                                    kBandH + 16))
             .shape(cornerBrackets(22))
             .stroke(stroke(
                 1.5f, Fill::color(sigil::material::withAlpha(kCyan, 0.72f))))
             .zIndex(8)});
  }

  // -------------------------------------------------------------------
  // the hardware hint row along the panel's bottom rail

  void hints(Element& root) {
    // the hints: one register on the row, bare runs under it
    root.children(
        {box()
             .rect(SkRect::MakeXYWH(kPX + 32, kHintY, kPW - 64, 1.0f))
             .shape(hline())
             .stroke(stroke(
                 1.0f, Fill::color(sigil::material::withAlpha(kCyan, 0.36f))))
             .zIndex(8),
         kit::centred()
             .rect(SkRect::MakeXYWH(kPX, kHintY + 8, kPW, 26.0f))
             .row()

             .gap(56)
             .zIndex(8)
             .styleClass("hint")
             .children(
                 {box()
                      .row()
                      .alignItems(Align::Center)
                      .gap(8)
                      .children(
                          {// KEYLESS, and it has to be: the inner disc
                           // breathes off the pen's own clock, which no key
                           // can name.
                           box().width(15.0f).height(15.0f).children(
                               {pen([](Pen& q) {
                                 const float r = q.width * 0.5f;
                                 q.noFill();
                                 q.stroke(
                                     sigil::material::withAlpha(kCyan, 0.82f));
                                 q.strokeWeight(1.3f);
                                 q.circle(r, r, 2.0f * (r - 1.1f));
                                 for (int i = 0; i < 4; ++i) {
                                   const SkPoint in = arrange::onRing(
                                       (size_t)i, 4, {r, r},
                                       {r * 0.6f, r * 0.6f}, 0.0f, 6.2831853f,
                                       arrange::Turn::Closed);
                                   const SkPoint out = arrange::onRing(
                                       (size_t)i, 4, {r, r},
                                       {r * 0.9f, r * 0.9f}, 0.0f, 6.2831853f,
                                       arrange::Turn::Closed);
                                   q.line(in.fX, in.fY, out.fX, out.fY);
                                 }
                                 q.noStroke();
                                 q.fill(sigil::material::withAlpha(
                                     kCyan,
                                     0.3f + 0.5f * (0.5f +
                                                    0.5f * std::sin(q.millis() *
                                                                    0.0034f))));
                                 q.circle(r, r, r * 0.8f);
                               })}),
                           text("Navigate")}),
                  text("[Enter] Select"), text("[Esc] Exit")})});

    // the empty hardware sockets the bezel carries at its bottom corners
    for (float x : {kPX + 34, kPR - 46}) {
      root.children(
          {box()
               .rect(SkRect::MakeXYWH(x, kHintY + 13, 12.0f, 12.0f))
               .stroke(stroke(
                   1.2f, Fill::color(sigil::material::withAlpha(kCyan, 0.45f))))
               .zIndex(8)});
    }
  }

  // -------------------------------------------------------------------
  // the CRT overlay: the refresh bar that warps what it passes over

  void overlay(Element& root) {
    root.children(
        {box()
             .rect(SkRect::MakeXYWH(kPX + 6, kPY + 30, kPW - 12, 34.0f))
             .translateY(&scanY)
             .backdropFilter(styles::ripple(1.0f, 130.0f, 0.0f))
             .fill(Paint::linear(
                 {0, 0}, {0, 34},
                 {{0.0f, sigil::material::withAlpha(kCyan, 0.0f)},
                  {0.5f, sigil::material::withAlpha(kCyan, 0.05f)},
                  {1.0f, sigil::material::withAlpha(kCyan, 0.0f)}}))
             .blendMode(SkBlendMode::kPlus)
             .cache(Cache::None)
             .zIndex(9),
         box()
             .inset(0)
             .fill(Paint::radial({kW * 0.5f, kH * 0.46f}, kW * 0.60f,
                                 {{0.0f, hexColor(0x000000, 0.0f)},
                                  {0.55f, hexColor(0x000000, 0.14f)},
                                  {1.0f, hexColor(0x01050A, 0.86f)}}))
             .zIndex(11)});
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
    // THE HOLOGRAM'S OWN SHEET: one class per line the bench sets, so a
    // word names the register it is set in and never carries a size, a
    // face and a colour of its own. Every one of them is the interface
    // face condensed 1.16, which is what benchType() is.
    sigil::compose::StyleSheet classes{
        sigil::compose::rule("h1").font(benchType(31, kTitle, 0.10f)),
        sigil::compose::rule("lead").font(benchType(
            10.5f, sigil::material::withAlpha(kCyan, 0.5f), 0.2f, false)),
        sigil::compose::rule(".node").font(
            benchType(11, sigil::material::withAlpha(kCyan, 0.78f), 0.08f)),
        sigil::compose::rule(".circuit")
            .font(
                benchType(11, sigil::material::withAlpha(kCyan, 0.62f), 0.18f)),
        sigil::compose::rule(".slots").font(benchType(
            9.5f, sigil::material::withAlpha(kCyan, 0.4f), 0.18f, false)),
        sigil::compose::rule(".spec").font(
            benchType(14, sigil::material::withAlpha(kCyan, 0.95f), 0.10f)),
        sigil::compose::rule(".value").font(
            benchType(13, hexColor(0xDCEEF2), 0.02f, false)),
        sigil::compose::rule(".head").font(benchType(
            9, sigil::material::withAlpha(kCyan, 0.42f), 0.22f, false)),
        sigil::compose::rule(".nodes").font(
            benchType(12, sigil::material::withAlpha(kCyan, 0.95f), 0.16f)),
        sigil::compose::rule(".count").font(benchType(40, kTitle, 0.0f)),
        sigil::compose::rule(".hint").font(benchType(
            12, sigil::material::withAlpha(kCyan, 0.78f), 0.06f, false))};
    auto holo = box()
                    .applyStyleSheet(std::move(classes))
                    .inset(0)
                    .translateX(&jitterX)
                    .opacity(&holoAlpha)
                    .scale(animate(from(0.955f).to(1.0f), {380ms}))
                    .transformOrigin(Dimension(kW * 0.5f), Dimension(kH * 0.5f))
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
    root.children({std::move(holo)});
    return root;
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = SkSize::Make((int)kW, (int)kH),
                             .captureAt = 2.5,
                             .background = hexColor(0x02060A)});
    bakePips();

    ctx.ticker.add([this, &ticker = ctx.ticker] {
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
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext&) {}
};

SIGIL_SKETCH(Ds2Bench, "Study · Game UI",
             "Dead Space 2's Nanocircuit bench (2011) — routers, rails, "
             "a diegetic hologram")
