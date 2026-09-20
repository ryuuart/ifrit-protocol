// An ability check composed from the die, skill modifiers and result panels.

// TAGS: Interfaces/Game

#include "AbilityCheck.h"

struct Bg3DiceRoll {
  // ---- bound Outputs (all paint-only) -------------------------------------
  choreograph::Output<float> bezelSpin{0};
  choreograph::Output<float> rosetteSpin{0};
  choreograph::Output<float> hatchSpacing{14.0f};
  choreograph::Output<float> hatchAngle{12.0f};

  // ---- state that changes CONTENT (re-describe) ---------------------------
  double clock = 0;
  float totalAnim = (float)bg3::kNaturalRoll;
  int shownTotal = bg3::kNaturalRoll;
  int rowsMounted = 0;
  bool outcomeMounted = false;

  bg3::Die solid;
  sk_sp<SkTypeface> serif, mono;
  float ax = 0, ay = 0, az = 0;  ///< the tumble, decaying to nothing
  glm::mat3 settle{1.0f};        ///< the attitude the die lands in

  // ------------------------------------------------------------------- type
  /** A line's own fields over the serif the root sets every line in: the
   *  mono face only where a figure asks for it. */
  weave::Type line(float size, SkColor4f col, float track, bool useMono) const {
    weave::Type t{.size = size, .color = col, .track = track};
    if (useMono) t.face = mono;
    return t;
  }
  Element label(const std::string& s, float x, float y, float size,
                SkColor4f col, float track = 0.0f, bool useMono = false) const {
    return box().left(x).top(y).children(
        {text(bg3::u8(s)).font(line(size, col, track, useMono))});
  }
  /** Right-aligned, since a numeral column must align on its units digit and
   *  Yoga is not the skeleton here. `right` is in the PARENT's space, so the
   *  parent's width has to be named — pinning `right` against the canvas
   *  width inside a 528 px row is the bug this parameter exists to stop. */
  Element labelR(const std::string& s, float right, float y, float size,
                 SkColor4f col, bool useMono = false,
                 float parentWidth = bg3::kW) const {
    return box()
        .right(parentWidth - right)
        .top(y)
        .children({text(bg3::u8(s)).font(line(size, col, 0.0f, useMono))});
  }

  /** A bare rule as its own tiny node — a stroke wants a box the size of the
   *  stroke, never the canvas. */
  static Element rule(float x, float y, float w, Decoration d, float h = 1.0f) {
    return kit::at(box().foreground(std::move(d)), x, y, w, h);
  }

  // -------------------------------------------------------------- the solid
  /** The die, projected honestly. Immediate-mode: 20 faces and 30 edges is
   *  cheap, and it changes every frame. */
  Element die(float radius, float opacity, bool numerals, float spin) const {
    const float boxSize = radius * 2.28f;
    return custom([this, radius, opacity, numerals, spin](
                      SkCanvas& c, const PaintContext& ctx) {
             const float cx = ctx.size.width() * 0.5f;
             const float cy = ctx.size.height() * 0.5f;
             // The solid stands on the unit sphere, so the radius IS the
             // scale, and the attitude is the tumble over the settle.
             const float s = radius;
             const glm::mat3 attitude = bg3::turn(ax, ay + spin, az) * settle;

             std::array<SkPoint, 12> proj{};
             for (int i = 0; i < 12; ++i) {
               const bg3::V3 r = attitude * solid.cage.positions[(size_t)i];
               proj[(size_t)i] = {cx + r.x * s, cy - r.y * s};
             }
             const int nf = (int)solid.faces();
             std::vector<float> nz((size_t)nf);
             std::vector<bool> vis((size_t)nf);
             for (int f = 0; f < nf; ++f) {
               const bg3::V3 r = attitude * solid.normal[(size_t)f];
               nz[(size_t)f] = r.z;
               vis[(size_t)f] = r.z > 0.0f;
             }

             // Carved bone. Each visible face gets its own tone from its own
             // normal — the only shading on the die, and derived from the
             // geometry rather than painted.
             SkPaint body;
             body.setAntiAlias(true);
             body.setStyle(SkPaint::kFill_Style);
             for (int f = 0; f < nf; ++f) {
               if (!vis[(size_t)f]) continue;
               const std::array<int, 3> t = solid.corners((size_t)f);
               // A WIDER RANGE THAN A DIAGRAM WANTS. On cream, faces
               // a quarter apart read as a solid; on black the same
               // spread reads as a flat net, and the die is the one lit
               // object in the frame.
               const float k = 0.34f + 0.66f * nz[(size_t)f];
               body.setColor4f({bg3::kBone.fR * k, bg3::kBone.fG * k,
                                bg3::kBone.fB * k, opacity},
                               nullptr);
               SkPathBuilder tri;
               tri.moveTo(proj[(size_t)t[0]]);
               tri.lineTo(proj[(size_t)t[1]]);
               tri.lineTo(proj[(size_t)t[2]]);
               tri.close();
               c.drawPath(tri.detach(), body);
             }

             // THE THREE-TIER EDGE WEIGHT.
             SkPaint edge;
             edge.setAntiAlias(true);
             edge.setStyle(SkPaint::kStroke_Style);
             edge.setStrokeCap(SkPaint::kRound_Cap);
             SkPathBuilder interior, silhouette;
             for (const mesh::Edge& e : solid.edges) {
               const int seen =
                   (vis[e.face] ? 1 : 0) +
                   (e.opposite != mesh::kNoFace && vis[e.opposite] ? 1 : 0);
               if (seen == 0) continue;  // back edge: omitted entirely
               SkPathBuilder& into = seen == 2 ? interior : silhouette;
               into.moveTo(proj[e.from]);
               into.lineTo(proj[e.to]);
             }
             edge.setStrokeWidth(1.1f);
             edge.setColor4f(mskia::withAlpha(bg3::kGiltDark, opacity * 0.75f),
                             nullptr);
             c.drawPath(interior.detach(), edge);
             edge.setStrokeWidth(2.6f);
             edge.setColor4f(mskia::withAlpha(bg3::kInk, opacity), nullptr);
             c.drawPath(silhouette.detach(), edge);

             if (!numerals || !serif) return;
             // Ink-filled numerals at each visible face's centroid, scaled by
             // that face's own foreshortening and dropped when it turns too far
             // away.
             SkPaint glyph;
             glyph.setAntiAlias(true);
             glyph.setColor4f(mskia::withAlpha(bg3::kInk, opacity), nullptr);
             for (int f = 0; f < nf; ++f) {
               if (nz[(size_t)f] < 0.34f) continue;
               const bg3::V3 r = attitude * solid.centroid[(size_t)f];
               const float fx = cx + r.x * s, fy = cy - r.y * s;
               const float fs =
                   radius * 0.30f * (0.55f + 0.45f * nz[(size_t)f]);
               SkFont font(serif, fs);
               const std::string pip =
                   kit::formatted("%d", solid.pip[(size_t)f]);
               const float w = font.measureText(pip.c_str(), pip.size(),
                                                SkTextEncoding::kUTF8);
               glyph.setAlphaf(opacity * (0.35f + 0.65f * nz[(size_t)f]));
               c.drawString(pip.c_str(), fx - w * 0.5f, fy + fs * 0.34f, font,
                            glyph);
             }
           })
        .left(bg3::kCx - boxSize * 0.5f)
        .top(bg3::kCy - boxSize * 0.5f)
        .width(boxSize)
        .height(boxSize)
        .cache(Cache::None);
  }

  // ------------------------------------------------------------- the bezel
  /** The 20-gon, carrying the whole border vocabulary at twenty vertices.
   *  Static content on a rotating node: baked once, blitted spinning. */
  Element bezel() const {
    const float o = bg3::kBezelOuter, i = bg3::kBezelInner;
    const float a = bg3::kCornerAngle;

    // The gilt fleuron that sits ON each vertex, on the bisector.
    brush::Pattern ornament;
    ornament.side = box()
                        .width(13)
                        .height(7)
                        .shape(shapes::polygon(4, 45.0f))
                        .foreground(stroke(0.9f, bg3::giltDark(0.85f)));
    // A four-pointed rosette, symmetric about its own bisector: an
    // ORNAMENT, which is what Bisector is for.
    ornament.corner =
        brush::CornerArt{box()
                             .width(22)
                             .height(22)
                             .shape(shapes::star(4, 0.34f, 0.14f))
                             .fill(mskia::withAlpha(bg3::kGilt, 0.92f))
                             .foreground(stroke(0.8f, bg3::ink(0.55f))),
                         brush::CornerAlign::Bisector};
    ornament.advance = 21.0f;
    ornament.cornerLength = 26.0f;
    ornament.bleedPx = 26.0f;
    // brush::Pattern has its OWN threshold, and its default is 35 — higher
    // than Border's 30. It needs telling separately.
    ornament.cornerAngleDeg = a;

    Element ring =
        kit::at(stack(), bg3::kCx - o, bg3::kCy - o, o * 2, o * 2)
            // The outer 20-gon: the rule THICKENS at each of the twenty
            // vertices — the brass-rule move.
            .children(
                {box()
                     .inset(0)
                     .shape(shapes::polygon(20))
                     .foreground(decorations::weightedCorners(
                         1.5f, 5.5f, bg3::gilt(), 20.0f, 0.0f, a)),
                 // A twenty-tick bracket ladder just inside it: one tick per
                 // face of the die, so the ornament IS the index.
                 box()
                     .inset(17)
                     .shape(shapes::polygon(20))
                     .stroke(spans::corners(15.0f, a),
                             brush::solid(2.4f, bg3::ink(0.7f))),
                 // The illuminated border proper: fleuron on every vertex.
                 box().inset(36).shape(shapes::polygon(20)).stroke(ornament),
                 // The inner 20-gon: stops short at every flat, so vellum
                 // breathes between the two rules.
                 box()
                     .inset(o - i)
                     .shape(shapes::polygon(20))
                     .stroke(spans::edges(32.0f, a),
                             brush::solid(2.4f, bg3::ink(0.85f)))
                     .stroke(spans::corners(11.0f, a),
                             brush::solid(3.4f, bg3::gilt()))});
    return ring.rotate(&bezelSpin)
        .transformOrigin(pct(50), pct(50))
        .cache(Cache::Texture);
  }

  /** The band the bezel's rules sit on: a filled 20-gon carrying the
   *  concentric rules, with the rosette disc laid over its middle — so
   *  `lines::concentric` shows only in the band, which is where a bezel's
   *  turned rings belong. */
  Element bezelBand() const {
    const float o = bg3::kBezelOuter;
    return kit::at(box().shape(shapes::polygon(20)), bg3::kCx - o, bg3::kCy - o,
                   o * 2, o * 2)
        // Light enough that the rules read as ornament ON vellum rather than
        // ink on a brown ring — this is gilt, not bronze.
        .fill(linearGradient({0, 0}, {o * 1.4f, o * 2},
                             {mskia::withAlpha(bg3::kGilt, 0.16f),
                              mskia::withAlpha(bg3::kGiltDark, 0.09f),
                              mskia::withAlpha(bg3::kGilt, 0.14f)},
                             {0.0f, 0.55f, 1.0f}))
        // OVERLAY, not background: `background()` paints BENEATH the fill
        // (the CSS box-shadow slot), so turned rings put there vanish under
        // any opaque surface. `overlay()` is over the fill, under children.
        .overlay(lines::presets::concentric(bg3::giltDark(0.42f), 5, 0.8f))
        // No drop shadow here: `background()` is UNDER the fill, and this
        // fill is 14% alpha — so the blurred silhouette showed straight
        // through and turned a gilt band into a mud-olive ring.
        .cache(Cache::Texture);
  }

  /** The rosette: a radial fan clipped to the annulus between the two
   *  20-gons, counter-rotating under the die. */
  Element rosette() const {
    const float r = bg3::kBezelInner - 6.0f;
    return kit::at(box().shape(shapes::polygon(20, 9.0f)), bg3::kCx - r,
                   bg3::kCy - r, r * 2, r * 2)
        // Opaque, so it masks the band's concentric rules down to the band.
        .fill(radialGradient(
            {r, r}, r,
            {bg3::kVellum, mskia::withAlpha(bg3::kVellumDeep, 1.0f)}))
        .overlay(lines::presets::radialHatch(bg3::giltDark(0.34f), 60, 0.6f))
        .rotate(&rosetteSpin)
        .transformOrigin(pct(50), pct(50))
        .cache(Cache::Texture);
  }

  // -------------------------------------------------------- the DC plate
  Element dcPlate() const {
    // A hand-built Rails set: one rail's dashPhase slid against its
    // neighbours is what makes a doubled rule read as ENGRAVED.
    lines::Rails engraved = lines::rails({
        {.across = 4.0f, .width = 1.8f, .fill = bg3::gilt()},
        {.across = 0.0f,
         .width = 0.9f,
         .fill = bg3::giltDark(),
         .dash = {3.0f, 5.0f}},
        {.across = -4.0f,
         .width = 1.8f,
         .fill = bg3::gilt(),
         .dash = {9.0f, 9.0f},
         .dashPhase = 4.5f},
    });

    // Two flourishes springing off the plate — a calligraphic nib, so the
    // width varies; a constant-width stroke would look printed.
    //
    // NOTE: `shapes::parametric` samples f over UNIT coordinates — ±1 spans
    // the box, it is not px. Returning px here sent the curve off-canvas as
    // one enormous gold arc through the whole composition. Documented at the
    // call site; still the easiest thing in Shapes.h to get wrong.
    auto flourish = [&](bool mirror) {
      brush::Ribbon nib;
      nib.fill = bg3::gilt(0.9f);
      nib.widthStart = 5.6f;
      nib.widthEnd = 0.6f;
      nib.nibAngleDeg = mirror ? 118.0f : 62.0f;
      nib.nibContrast = 0.18f;
      const float dir = mirror ? -1.0f : 1.0f;
      return kit::at(
          box()
              .shape(shapes::parametric(
                  [dir](float t) {
                    // Unit space: a hooked spur that curls back on itself.
                    const float u = t * 3.14159f;
                    return SkPoint{
                        dir * (-0.92f + 1.72f * t),
                        -0.55f * std::sin(u) * std::cos(u * 0.62f) + 0.30f * t};
                  },
                  0.0f, 1.0f, 96))
              .background(nib),
          mirror ? 18.0f : 388.0f, 26.0f, 118, 62);
    };

    return kit::at(
        stack().children(
            {box()
                 .left(148)
                 .top(0)
                 .width(228)
                 .height(118)
                 .shape(shapes::notched(30.0f, 13.0f, shapes::Corner::All))
                 .fill(mskia::withAlpha(bg3::kVellumDeep, 0.96f))
                 .background(shadow({0.1f, 0.07f, 0.04f, 0.30f}, {0, 4}, 12))
                 .style(decorations::doubleBorder(
                     decorations::border(1.9f, bg3::ink(0.9f), 5.0f),
                     decorations::border(0.7f, bg3::giltDark(), 10.0f))),
             flourish(false), flourish(true),
             // The hanger: the plate reads as a hung sign, so it hangs from a
             // rule.
             rule(166.0f, 118.0f, 192.0f, engraved, 1.0f),
             // The plate box (148..376) and its hanger (166..358) are both
             // centred on local 262, and the three type runs have to be centred
             // on it too. The offsets below are INK centres, not advances: "D
             // C" carries a trailing 7 px letterspace and the caption 1.2, so
             // centring on the measured advance leaves each run visibly
             // off-axis on a symmetric sign, in different directions.
             label("D C", 148.0f + 90.0f, 10.0f, 21.0f,
                   mskia::withAlpha(bg3::kInk, 0.72f), 7.0f),
             label(std::to_string(bg3::kDC), 148.0f + 91.5f, 26.0f, 42.0f,
                   bg3::kInk),
             // The caption lives INSIDE the plate: below it the plate
             // deliberately bleeds over the bezel's top flat, and type on the
             // band is unreadable.
             label("SkillCheck · StatsRollType", 148.0f + 26.5f, 82.0f, 9.5f,
                   mskia::withAlpha(bg3::kInk, 0.5f), 1.2f, true)}),
        bg3::kCx - 262.0f, 112.0f, 524, 150);
  }

  // ------------------------------------------------------- the skill ladder
  /** Correction 1, drawn: the engine's ordinal table IS the ability grouping.
   *  Eighteen ticks at their true ordinals, five blocks bracketed, the active
   *  skill marked. Ornament that is data. */
  Element skillLadder() const {
    constexpr float kX = 74.0f;  // the tick ladder's spine
    constexpr float kTop = 286.0f;
    constexpr float kPitch = 25.0f;
    constexpr float kBlockGap = 27.0f;  ///< the header's OWN row

    // The ordinal is the true index; the gap between blocks is drawn, so the
    // grouping the ordinals encode is visible rather than asserted.
    auto yOf = [](int ordinal) {
      int blockIndex = 0;
      for (const auto& b : bg3::kBlocks)
        if (ordinal >= b.first + b.count) ++blockIndex;
      return kTop + (float)ordinal * kPitch + (float)blockIndex * kBlockGap;
    };

    Element g = kit::at(stack(), 0, 0, bg3::kW, bg3::kH);
    const float yLast = yOf(17);
    g.children({rule(kX, kTop - 12.0f, 1.2f,
                     lines::presets::hatch(bg3::ink(0.42f), 4.0f, 1.2f, 90.0f),
                     yLast - kTop + 26.0f)});

    for (const auto& blk : bg3::kBlocks) {
      const float y0 = yOf(blk.first);
      const float y1 = yOf(blk.first + blk.count - 1);
      // A bracket that only exists at the block's ends.
      g.children({kit::at(box()
                              .shape(shapes::chamfered(
                                  5.0f, shapes::Corner::AntiDiagonal))
                              .stroke(spans::corners(11.0f, 24.0f),
                                      brush::solid(1.6f, bg3::giltDark())),
                          kX - 14.0f, y0 - 6.0f, 14.0f, y1 - y0 + 12.0f),
                  // The header sits in the gap ABOVE its block, on its own row.
                  label(blk.ability, kX + 22.0f, y0 - 21.0f, 9.0f,
                        mskia::withAlpha(bg3::kGiltDark, 0.95f), 2.4f, true),
                  label("AbilityId " + std::to_string(blk.abilityOrdinal),
                        kX + 128.0f, y0 - 20.0f, 7.5f,
                        mskia::withAlpha(bg3::kInk, 0.34f), 0.8f, true)});
    }

    for (const auto& sk : bg3::kSkills) {
      const float y = yOf(sk.ordinal);
      const bool live = sk.ordinal == bg3::kActiveSkill;
      g.children(
          {rule(
               kX, y, live ? 26.0f : 15.0f,
               stroke(live ? 2.4f : 1.0f, live ? bg3::gilt() : bg3::ink(0.55f)),
               live ? 2.4f : 1.0f),
           label(sk.name, kX + (live ? 34.0f : 22.0f), y - 8.0f,
                 live ? 15.0f : 11.5f,
                 live ? bg3::kInk : mskia::withAlpha(bg3::kInk, 0.58f),
                 live ? 1.6f : 0.6f),
           labelR(std::to_string(sk.ordinal), kX - 19.0f, y - 6.0f, 9.5f,
                  mskia::withAlpha(bg3::kInk, live ? 0.85f : 0.34f), true)});
    }
    return g.cache(Cache::Texture);
  }

  // ------------------------------------------------------ the modifier column
  /** Straight, because the reference stacks them — but DOCKED ON A RADIUS,
   *  each row's leader tick running back to the bezel's lower-right flat, so
   *  the column participates in the radial composition without pretending
   *  BG3 fans them on an arc. */
  Element modifierColumn() const {
    constexpr float kX = 648.0f;
    constexpr float kTop = 866.0f;
    constexpr float kPitch = 50.0f;
    constexpr float kRight = 1076.0f;
    constexpr float kRowW = kRight - kX + 112.0f;

    Element col = kit::at(box().staggerChildren(110ms), 0, 0, bg3::kW, bg3::kH);

    for (int i = 0; i < rowsMounted && i < (int)bg3::kBonuses.size(); ++i) {
      const auto& b = bg3::kBonuses[(size_t)i];
      const float y = kTop + (float)i * kPitch;
      const std::string amount = b.numDice > 0
                                     ? "+" + std::to_string(b.numDice) +
                                           std::string(b.diceSize) + " → " +
                                           std::to_string(b.resolved)
                                     : "+" + std::to_string(b.bonus);

      // The row is its OWN node and the entrance binds on IT — a bound
      // opacity is a saveLayer the size of the node's box, so the box is
      // the row, never the canvas.
      Element row =
          kit::at(stack()
                      .appear({260ms, choreograph::easeOutQuad})
                      .translateX(animate(from(18.0f).to(0.0f),
                                          {300ms, choreograph::easeOutQuad})),
                  kX - 96.0f, y - 10.0f, kRowW, 44.0f)
              // The leader tick, running back toward the bezel.
              .children(
                  {rule(0.0f, 20.0f, 84.0f,
                        brush::presets::heavyHairHeavy(
                            1.4f, 0.5f, bg3::giltDark(0.8f), 2.6f),
                        1.0f),
                   label(b.sourceName, 96.0f, 2.0f, 21.0f, bg3::kInk, 0.8f),
                   label(b.description, 96.0f, 26.0f, 9.5f,
                         mskia::withAlpha(bg3::kInk, 0.45f), 0.7f, true),
                   labelR(amount, kRowW - 16.0f, 0.0f,
                          b.numDice > 0 ? 20.0f : 24.0f, bg3::kInk, false,
                          kRowW)});
      col.children({std::move(row)});
    }
    return col;
  }

  /** The total: NaturalRoll and Total are separate fields, so they are
   *  separate numbers on the plate, and the second visibly accumulates. */
  Element totalBlock() const {
    constexpr float kX = 648.0f;
    constexpr float kY = 1024.0f;
    constexpr float kRight = 1076.0f;
    Element g = kit::at(stack(), 0, 0, bg3::kW, bg3::kH);
    g.children(
        {rule(kX + 96.0f, kY - 12.0f, kRight - kX - 96.0f + 16.0f,
              brush::presets::heavyHairHeavy(2.0f, 0.7f, bg3::ink(0.85f), 4.0f),
              1.0f),
         label("Total", kX + 96.0f, kY + 4.0f, 22.0f,
               mskia::withAlpha(bg3::kInk, 0.8f), 3.0f),
         label("StatsRollResult.Total", kX + 96.0f, kY + 32.0f, 9.0f,
               mskia::withAlpha(bg3::kInk, 0.42f), 0.8f, true),
         labelR(std::to_string(shownTotal), kRight, kY - 14.0f, 56.0f,
                bg3::kInk)});
    return g;
  }

  /** The outcome. `Hatch` with BOTH bindings — pitch and angle — on a box
   *  the size of the banner, never the canvas. */
  Element outcome() const {
    if (!outcomeMounted) return kit::at(0, 0, 1, 1);
    lines::Hatch wash;
    wash.strokeFill = Fill::color(mskia::withAlpha(bg3::kViridian, 0.34f));
    wash.width = 1.5f;
    wash.spacingBinding = &hatchSpacing;
    wash.angleBinding = &hatchAngle;
    return kit::at(
        stack()
            .shape(shapes::chamfered(14.0f, shapes::Corner::Diagonal))
            .fill(mskia::withAlpha(bg3::kViridian, 0.10f))
            .overlay(wash)
            .foreground(decorations::weightedCorners(1.2f, 3.4f, bg3::gilt(),
                                                     16.0f, 0.0f, 30.0f))
            .children(
                {label("SUCCESS", 34.0f, 12.0f, 30.0f, bg3::kViridian, 8.0f),
                 label("RollCritical.None 0  ·  Total 20 ≥ DC 15", 34.0f, 44.0f,
                       9.0f, mskia::withAlpha(bg3::kInk, 0.55f), 0.9f, true)})
            .appear({380ms, choreograph::easeOutQuad}),
        636.0f, 1094.0f, 452.0f, 64.0f);
  }

  // ------------------------------------------------------------- marginalia
  Element portrait() const {
    constexpr float cx = 372.0f, cy = 742.0f, r = 64.0f;
    return kit::at(
        stack()
            .shape(shapes::chamfered(10.0f))
            .fill(mskia::withAlpha(bg3::kVellumDeep, 0.95f))
            .background(shadow({0.1f, 0.07f, 0.04f, 0.32f}, {0, 3}, 10))
            .style(decorations::doubleBorder(
                decorations::border(1.8f, bg3::ink(0.9f)),
                decorations::border(0.7f, bg3::giltDark(), 5.0f)))
            .children({box()
                           .left(16.0f)
                           .top(16.0f)
                           .width(r * 2)
                           .height(r * 2)
                           .shape(shapes::polygon(20, 9.0f))
                           .fill(radialGradient(
                               {r, r * 0.8f}, r * 1.25f,
                               {mskia::withAlpha(bg3::kVellum, 1.0f),
                                mskia::withAlpha(bg3::kGiltDark, 0.55f)}))
                           .overlay(lines::presets::concentric(
                               bg3::giltDark(0.45f), 4, 0.6f))
                           .overlay(lines::presets::radialHatch(
                               bg3::giltDark(0.3f), 20, 0.5f))
                           .stroke(spans::corners(10.0f, bg3::kCornerAngle),
                                   brush::solid(2.0f, bg3::ink(0.8f)))})
            .cache(Cache::Texture),
        cx - r - 16.0f, cy - r - 16.0f, r * 2 + 32.0f, r * 2 + 32.0f);
  }

  /** BG3's dialogue DCs sit on a 5-step ladder. Drawn as a scale in the right
   *  gutter with BOTH the target and the achieved total on it, so
   *  `Total >= DC` is a distance you can see rather than a claim in a
   *  caption — and it balances the skill ladder on the left. */
  Element dcLadder() const {
    constexpr float kX = 1102.0f;
    constexpr float kBottom = 762.0f;
    constexpr float kStep = 68.0f;  // per 5 points of DC
    auto yOf = [](float dc) { return kBottom - (dc - 5.0f) / 5.0f * kStep; };

    Element g = kit::at(stack(), 0, 0, bg3::kW, bg3::kH);
    g.children({rule(kX, yOf(30.0f) - 14.0f, 1.2f,
                     lines::presets::hatch(bg3::ink(0.42f), 4.0f, 1.2f, 90.0f),
                     kBottom - yOf(30.0f) + 28.0f),
                label("DC LADDER", kX - 30.0f, yOf(30.0f) - 34.0f, 9.0f,
                      mskia::withAlpha(bg3::kInk, 0.5f), 2.2f, true)});

    for (int dc = 5; dc <= 30; dc += 5) {
      const float y = yOf((float)dc);
      const bool target = dc == bg3::kDC;
      g.children({rule(kX - (target ? 20.0f : 11.0f), y, target ? 20.0f : 11.0f,
                       stroke(target ? 2.6f : 1.0f,
                              target ? bg3::gilt() : bg3::ink(0.5f)),
                       target ? 2.6f : 1.0f),
                  label(std::to_string(dc), kX + 8.0f, y - 8.0f,
                        target ? 15.0f : 11.5f,
                        target ? bg3::kInk : mskia::withAlpha(bg3::kInk, 0.55f),
                        target ? 1.4f : 0.6f)});
    }
    // The margin actually cleared: DC 15 -> Total 20, five points.
    const float yDC = yOf((float)bg3::kDC);
    const float yTotal = yOf((float)bg3::kFinalTotal);
    g.children(
        {kit::at(
             box()
                 .shape(shapes::chamfered(5.0f, shapes::Corner::AntiDiagonal))
                 .fill(mskia::withAlpha(bg3::kViridian, 0.14f))
                 .stroke(spans::corners(9.0f, 24.0f),
                         brush::solid(1.8f, Fill::color(bg3::kViridian))),
             kX - 34.0f, yTotal, 20.0f, yDC - yTotal),
         rule(kX - 34.0f, yTotal, 34.0f,
              stroke(2.6f, Fill::color(bg3::kViridian)), 2.6f),
         label("TOTAL", kX + 8.0f, yTotal - 30.0f, 9.0f,
               mskia::withAlpha(bg3::kViridian, 0.95f), 1.8f, true),
         label("+5", kX - 62.0f, (yDC + yTotal) * 0.5f - 7.0f, 11.0f,
               mskia::withAlpha(bg3::kViridian, 0.9f), 0.6f, true),
         label("DC", kX - 52.0f, yDC - 7.0f, 9.0f,
               mskia::withAlpha(bg3::kGiltDark, 0.95f), 1.4f, true)});
    return g.cache(Cache::Texture);
  }

  Element marginalia() const {
    Element g = kit::at(stack(), 0, 0, bg3::kW, bg3::kH);
    // Registration marks: brackets on a chamfered frame that bleeds.
    g.children(
        {kit::at(box()
                     .shape(shapes::chamfered(26.0f))
                     .stroke(spans::corners(30.0f, 24.0f),
                             brush::solid(1.4f, bg3::ink(0.42f))),
                 26, 26, bg3::kW - 52, bg3::kH - 52)
             // A Border rather than a stroke pass: an inset rule has no
             // stroke-pass spelling, since a pass rides the node's outline.
             .foreground(Border{.width = 0.6f,
                                .fill = bg3::giltDark(0.55f),
                                .inset = 8.0f,
                                .mode = Border::Mode::Gapped,
                                .corner = 46.0f,
                                .cornerAngleDeg = 24.0f}),
         label("BALDUR’S GATE 3  ·  DIALOGUE ABILITY CHECK", 56.0f, 44.0f,
               13.0f, mskia::withAlpha(bg3::kInk, 0.62f), 3.4f)});

    // The advantage note: in the top-left margin, clear of both the skill
    // ladder and the bezel. The leader running from it down to the discarded
    // die is NOT part of this block — see advantageLeader() below.
    constexpr float kAx2 = 56.0f, kAy2 = 138.0f;
    g.children({label("ADVANTAGE", kAx2, kAy2, 12.0f,
                      mskia::withAlpha(bg3::kAdvantage, 0.95f), 3.2f),
                rule(kAx2, kAy2 + 17.0f, 188.0f,
                     stroke(1.2f, Fill::color(mskia::withAlpha(bg3::kAdvantage,
                                                               0.75f))),
                     1.2f),
                label("2d20 keep highest · DiscardedDiceTotal  " +
                          std::to_string(bg3::kDiscardedDiceTotal),
                      kAx2, kAy2 + 22.0f, 9.0f,
                      mskia::withAlpha(bg3::kInk, 0.55f), 0.8f, true)});
    // The leader to the discarded die is NOT drawn here — see
    // advantageLeader(). Marginalia paints under the roundel, and the rosette
    // is opaque by design, so a leader laid down at this point loses its last
    // 29% at the rosette's rim and ends in mid-air on the gilt band.

    // NaturalRoll — the single most important number on the plate before the
    // modifiers accumulate, so it gets the margin to itself: a leader running
    // out of the bezel to a label and a numeral nothing crosses.
    constexpr float kNx = 962.0f, kNy = 246.0f;
    g.children(
        {rule(kNx - 74.0f, kNy + 44.0f, 74.0f,
              brush::presets::dottedCore(1.5f, 2.4f, bg3::giltDark(0.95f), 3.6f,
                                         7.0f),
              1.0f),
         label("NaturalRoll", kNx, kNy, 12.0f,
               mskia::withAlpha(bg3::kInk, 0.62f), 2.6f),
         label("StatsRollResult", kNx, kNy + 16.0f, 8.0f,
               mskia::withAlpha(bg3::kInk, 0.38f), 0.8f, true),
         label(std::to_string(bg3::kNaturalRoll), kNx, kNy + 28.0f, 52.0f,
               bg3::kInk),
         rule(kNx, kNy + 92.0f, 118.0f,
              brush::presets::heavyHairHeavy(1.8f, 0.6f, bg3::gilt(), 3.4f),
              1.0f),
         label("before modifiers", kNx, kNy + 98.0f, 8.5f,
               mskia::withAlpha(bg3::kGiltDark, 0.85f), 1.0f, true)});
    return g.cache(Cache::Texture);
  }

  /** The vellum, edge to edge, with its stain. A gradient, because a gradient
   *  lowers to a SIMD blitter and an SkSL mix chain does not.
   *
   *  It lives on its OWN cached child rather than on the root's fill: the
   *  root has animated children, so the root is live-paint forever, and a
   *  fill sitting on it re-runs the gradient over the entire canvas every
   *  frame for a picture that never changes. Same pixels, one blit. */
  Element ground() const {
    return box()
        .inset(0)
        .width(bg3::kW)
        .height(bg3::kH)
        .fill(linearGradient(
            {0, 0}, {bg3::kW * 0.7f, bg3::kH},
            {bg3::kVellumDeep, bg3::kVellum, {0.016f, 0.013f, 0.011f, 1.0f}},
            {0.0f, 0.62f, 1.0f}))
        .cache(Cache::Texture);
  }

  /** The outermost ornamental ring, running off all four edges. */
  Element outerRing() const {
    return kit::at(box()
                       .shape(shapes::polygon(40, 4.5f))
                       .style(decorations::doubleBorder(
                           decorations::border(2.6f, bg3::giltDark(0.42f)),
                           decorations::border(0.9f, bg3::ink(0.3f), 16.0f)))
                       .background(lines::presets::concentric(
                           bg3::giltDark(0.10f), 3, 1.0f))
                       .cache(Cache::Texture),
                   bg3::kCx - 700.0f, bg3::kCy - 660.0f, 1400, 1400);
  }

  /** The discarded die of the advantage pair: behind, up-left, rotated,
   *  dimmed. DiscardedDiceTotal is a field, not an inference. */
  Element discardedDie() const {
    return kit::at(
        box()
            .translateX(-40.0f)
            .translateY(-40.0f)
            .rotate(-23.0f)
            .transformOrigin(Dimension(bg3::kCx), Dimension(bg3::kCy))
            .children({die(bg3::kDieRadius * 0.86f, 0.45f, false, 1.9f)}),
        0, 0, bg3::kW, bg3::kH);
  }

  /** The leader from the ADVANTAGE block to the discarded die. It has to be
   *  laid down AFTER the roundel — the rosette is opaque, and its rim sits at
   *  t = 0.71 of this curve, so drawn with the rest of the marginalia the
   *  callout stopped short of the thing it calls out. */
  Element advantageLeader() const {
    constexpr float kAx2 = 56.0f, kAy2 = 138.0f;
    return kit::at(
        box()
            .shape(shapes::parametric(
                [](float t) {
                  return SkPoint{-1.0f + 2.0f * t, -1.0f + 2.0f * t * t};
                },
                0.0f, 1.0f, 48))
            .background(
                stroke(1.0f, Fill::color(mskia::withAlpha(bg3::kInk, 0.32f)))),
        kAx2 + 238.0f, kAy2 + 64.0f, 232.0f, 196.0f);
  }

  /** The winner, punching in on a keyframe path: one ramp cannot shape a
   *  landing. */
  Element heroDie() const {
    return kit::at(
        box()
            .transformOrigin(Dimension(bg3::kCx), Dimension(bg3::kCy))
            .scale(animate(through({{0ms, 0.80f},
                                    {1100ms, 1.0f},
                                    {1210ms, 1.075f},
                                    {1300ms, 0.975f},
                                    {1390ms, 1.02f},
                                    {1450ms, 1.0f}})))
            .children({die(bg3::kDieRadius, 1.0f, true, 0.0f)}),
        0, 0, bg3::kW, bg3::kH);
  }

  // ---------------------------------------------------------------- describe
  /** Only three things here change with STATE — the column, the total and
   *  the outcome. Everything else is behind `memo(0, …)`, whose body runs
   *  only when its properties change, i.e. never.
   *
   *  That matters more than it looks. The counter re-describes on every
   *  integer it passes through (12 → 20), so `describe()` runs about ten times
   *  during the reveal. Without memo, each run rebuilds the ornament from
   *  scratch — and `brush::Pattern` keeps its baked-tile cache IN THE BRUSH
   *  VALUE, so a freshly constructed brush gets an EMPTY one and all twenty
   *  fleurons plus every side tile are re-baked on every pass. Memoised, the
   *  reveal costs what the settled frame does. */
  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    auto once = [](auto fn) {
      return memo(0, [fn](const int&) { return fn(); });
    };
    return stack()
        // The serif every line is set in, stated once; a call names the
        // mono face itself.
        .font({.face = serif})
        .children({once([this] { return ground(); }),
                   once([this] { return outerRing(); }),
                   once([this] { return marginalia(); }),
                   once([this] { return skillLadder(); }),
                   once([this] { return dcLadder(); }),
                   once([this] { return bezelBand(); }),
                   once([this] { return rosette(); }),
                   once([this] { return discardedDie(); }),
                   once([this] { return advantageLeader(); }),
                   once([this] { return bezel(); }),
                   once([this] { return portrait(); }),
                   once([this] { return heroDie(); }),
                   once([this] { return dcPlate(); }), modifierColumn(),
                   totalBlock(), outcome(),
                   once([this] { return captionBand(); })});
  }

  /** THE CAPTION BAND, outside the overlay. What the study is grounded in
   *  — the extender's generated type surface and the enum ordinals it
   *  reads — was printed onto the overlay as four footer lines, a plate
   *  number, a source citation and two identifier lines beside the skill
   *  ladder. Baldur's Gate 3 shows none of that on a roll, so the
   *  citations are the header's and the one thing worth reading beside
   *  the picture stands under it. */
  Element captionBand() const {
    Element band = box()
                       .key("caption")
                       .left(0)
                       .right(0)
                       .top(bg3::kH)
                       .height(bg3::kBandH)
                       .column()
                       .justifyContent(Justify::Center)
                       .padding(56.0f, 0.0f)
                       .gap(7.0f)
                       .zIndex(30)
                       .fill(Fill::color({0.020f, 0.017f, 0.014f, 1.0f}));
    band.children(
        {label("BALDUR’S GATE 3 · DIALOGUE ABILITY "
               "CHECK, THE INSTANT AFTER THE DIE LANDS",
               0.0f, 0.0f, 12.0f, mskia::withAlpha(bg3::kGilt, 0.92f), 2.8f,
               true)
             .left(0.0f)
             .top(0.0f)
             .absolute()
             .left(56.0f)
             .top(30.0f),
         label("Every ordinal and enum name off Norbyte/bg3se's generated Lua "
               "type surface · the modifiers are added AFTER the "
               "natural roll",
               56.0f, 58.0f, 10.0f, mskia::withAlpha(bg3::kInk, 0.46f), 0.6f,
               true)});
    return band;
  }

  // ------------------------------------------------------------------- setup
  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = SkSize::Make(bg3::kW, bg3::kCanvasH),
                             .captureAt = 6.0,
                             .background = bg3::kVellum});

    // BG3 sets a humanist old-style with tall caps; these are the closest
    // faces present on the host.
    serif = weave::ports::face({"Baskerville", "Palatino", "Hoefler Text",
                                "Georgia", "Times New Roman", "Helvetica"});
    mono = weave::ports::face(
        {"Menlo", "SF Mono", "Monaco", "Courier New", "Helvetica"});

    // The face that settles square to the viewer is the one that carries
    // the roll, and its antipode takes 21 - roll, like a real die.
    solid = bg3::buildDie(bg3::kSettledFace, bg3::kNaturalRoll);
    settle = bg3::settleAttitude(solid, bg3::kSettledFace);
    bg3::tumbleAngles(0.0, ax, ay, az);

    ctx.ticker.add([this](double dt) {
      clock += dt;
      bg3::tumbleAngles(clock, ax, ay, az);
      bezelSpin = (float)std::fmod(clock * 1.5, 360.0);      // 1.5 deg/s
      rosetteSpin = (float)std::fmod(-clock * 0.42, 360.0);  // counter

      // The total accumulates as each row lands — retargeted, so the number
      // visibly climbs rather than stepping.
      int target = bg3::kNaturalRoll;
      for (const auto& b : bg3::kBonuses)
        if (clock >= b.landsAt) target += b.numDice > 0 ? b.resolved : b.bonus;
      totalAnim +=
          ((float)target - totalAnim) * (1.0f - std::exp(-(float)dt * 13.0f));

      if (clock >= bg3::kOutcomeAt) {
        const float u =
            (float)std::clamp((clock - bg3::kOutcomeAt) / 0.38, 0.0, 1.0);
        const float e = 1.0f - (1.0f - u) * (1.0f - u);
        hatchSpacing = 14.0f - 11.0f * e;  // 14 px -> 3 px
        hatchAngle = 12.0f + 12.0f * e;    // a 12 degree swing
      }
    });

    ctx.composer.render(describe(ctx));
  }

  /** Content changes — the digits, and the rows' MOUNT, which is what makes
   *  the column a dependency chain instead of a stagger. */
  void update(double elapsed, sketch::SketchContext& ctx) {
    (void)elapsed;
    bool dirty = false;
    const int rounded = (int)std::lround(totalAnim);
    if (rounded != shownTotal) {
      shownTotal = rounded;
      dirty = true;
    }
    // Rows do not EXIST until the die is still; staggerChildren cascades
    // their entrances from that mount.
    if (clock >= bg3::kSettleAt && rowsMounted < (int)bg3::kBonuses.size()) {
      rowsMounted = (int)bg3::kBonuses.size();
      dirty = true;
    }
    if (!outcomeMounted && clock >= bg3::kOutcomeAt) {
      outcomeMounted = true;
      dirty = true;
    }
    if (dirty) ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(Bg3DiceRoll, "Study · Game UI",
             "Baldur's Gate 3's ability check — an icosahedron, and an "
             "enum whose ordinals are 5e's skill table")

// -----------------------------------------------------------------------------
// THREE THINGS WORTH KNOWING BEFORE EDITING THIS PLATE
//
// 1. THE CORNER THRESHOLD HAS TO BE PASSED, ON EVERY DECORATION SEPARATELY.
//    On this bezel, at the default 30 degrees, every `spans::corners` and
//    `spans::edges` pass and `weightedCorners` find ZERO corners and the
//    ornament renders blank — all
//    twenty ticks gone, the outer rule flattened to one uniform hairline
//    (weighted corners with no corners is a plain rule over the whole contour),
//    the inner gapped rule running continuous instead of stopping at each flat,
//    and every fleuron missing from `brush::Pattern`, leaving only side tiles.
//    The library says so when it happens: it reports the sharpest tangent break
//    it did see and the smaller angle to pass. Note the asymmetry —
//    `brush::Pattern::cornerAngleDeg` defaults to 35 while
//    `Border::cornerAngleDeg` defaults to 30, so one polygon needs the same
//    fact stated twice, in two differently-shaped APIs. Both are 12 here.
//
// 2. `background()` IS UNDER THE FILL, AND THAT IS EASY TO MISREAD AS A BUG.
//    It is the CSS box-shadow slot. The natural sentence for a texture is
//    "give this node a hatch background", and doing that to a node with an
//    opaque fill renders NOTHING. The slot meaning "over the surface, under the
//    children" is `overlay()`. The mistake in reverse is just as quiet: a
//    `background` drop shadow under a low-alpha fill shows straight THROUGH it,
//    turning a gilt band mud-olive.
//
// 3. A RE-DESCRIBE THROWS AWAY EVERY `brush::Pattern` TILE CACHE. That cache
//    lives in the brush VALUE, so a freshly constructed brush starts empty and
//    re-bakes every corner and side tile. It only bites where something
//    re-describes repeatedly — here, the counter running 12 → 20 — which is why
//    the static subtrees sit behind `memo(0, …)`. Time the window your motion
//    happens in, not only the frame you ship.
//
// One deliberate cost: the bezel and the rosette are `Cache::Texture` nodes
// that ROTATE, so each frame resamples a large baked texture. That is the price
// of the slowly turning ring and it is paid on purpose. `bakeScale` is NOT
// used — it softens gilt hairlines, and the hairlines are the subject.
// -----------------------------------------------------------------------------
