// slitscan 2001: scene assembly and animation.

#include "SlitScan2001.h"

auto SlitScan2001::describe(sketch::SketchContext& ctx) -> Element {
  using namespace slit;
  return box()
      .column()
      .width(Dim(kCanvasW))
      .height(Dim(kCanvasH))
      .padding(kPad)
      .gap(20)
      .fill(kInk)
      .child(header())
      .child(box()
                 .row()
                 .gap(28)
                 .height(Dim(kBodyH))
                 .child(box()
                            .column()
                            .width(Dim(kLeftW))
                            .shrink(0)
                            .gap(24)
                            .child(filmFrame())
                            .child(rigStrip()))
                 .child(sidebar()));
}

// ===========================================================================
// The rig elevation. ONE SCALE: 3.0 px = 1 inch, throughout.
//
// Drawn as one custom() leaf -- custom() measures ZERO on the main axis, so
// it carries explicit width/height -- with every brushed mark going through
// decorations::paintOn(), which is how the whole vocabulary reaches
// hand-built geometry.

void SlitScan2001::drawRig(SkCanvas& c, const PaintContext& ctx) {
  using namespace slit;
  const float S = kRigPxPerIn;
  const float plateX = 548.0f;  // the slit; z = 0 for focus
  const float trackY = 194.0f;
  const float plateCY = 116.0f;
  const float plateH = kPlateIn * S;  // 216 px
  const float benchT = 232.0f, benchB = 244.0f;

  SkPaint p;
  p.setAntiAlias(true);

  // The bench, as a drafting section: a solid with 45-degree hatching.
  SkPathBuilder benchB2;
  benchB2.addRect(SkRect::MakeLTRB(4, benchT, kElevW - 6, benchB));
  const SkPath benchPath = benchB2.detach();
  p.setColor4f(kSolid);
  c.drawPath(benchPath, p);
  decorations::paintOn(
      c, ctx, benchPath,
      lines::presets::hatch(Fill::color(al(kAmber, 0.20f)), 6.0f, 1.0f, 45.0f));
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(al(kAmber, 0.55f));
  c.drawPath(benchPath, p);
  p.setStyle(SkPaint::kFill_Style);

  // THE TRACK: 14 ft = 168 in = 504 px [C85], from z = 180 in down to
  // z = 12 in -- which is where note 2 becomes a picture. A double rule,
  // with the worm gear as a micro-pitch lattice along it [NO].
  const float trackL = plateX - kZ0In * S;     // z = 180 in, x = 8
  const float trackR = trackL + kTrackIn * S;  // z = 12 in,  x = 512
  p.setColor4f(kAmber);
  c.drawRect(SkRect::MakeLTRB(trackL, trackY - 1, trackR, trackY), p);
  c.drawRect(SkRect::MakeLTRB(trackL, trackY + 5, trackR, trackY + 6), p);
  p.setColor4f(al(kAmber, 0.5f));
  // the loop walks a distance; the accumulated float is the position
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float x = trackL; x <= trackR; x += 4.0f)
    c.drawRect(SkRect::MakeLTRB(x, trackY + 1, x + 1.7f, trackY + 5), p);
  // …and the 36 px of reach the published track does NOT cover.
  p.setColor4f(al(kRed, 0.85f));
  // the loop walks a distance; the accumulated float is the position
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float x = trackR; x < plateX - 2; x += 5.0f)
    c.drawRect(SkRect::MakeLTRB(x, trackY + 2, x + 2.0f, trackY + 4), p);

  // THE PLATE: 6 ft = 216 px tall, rotatable [C85], turning live.
  c.save();
  c.translate(plateX, plateCY);
  c.rotate(plateDeg * 0.4f);
  const SkRect plate = SkRect::MakeLTRB(-9, -plateH * 0.5f, 9, plateH * 0.5f);
  p.setColor4f(kSolid);
  c.drawRect(plate, p);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(kAmber);
  c.drawRect(plate, p);
  p.setStyle(SkPaint::kFill_Style);
  const float slitH = kSlitHIn * S;  // 4 ft of the 6 ft plate [NO] vs [C85]
  p.setColor4f(al(kCold, 0.26f));
  c.drawRect(SkRect::MakeLTRB(-5.0f, -slitH * 0.5f, 5.0f, slitH * 0.5f), p);
  p.setColor4f(kCold);
  c.drawRect(SkRect::MakeLTRB(-0.9f, -slitH * 0.5f, 0.9f, slitH * 0.5f), p);
  c.restore();

  // THE LOG TICKS. z halves eight times across the sweep, and because
  // u = f*X0/z the SAME eight steps are equal intervals in the film frame.
  // Drawing them on the track is the clearest statement of why a corridor
  // shot at constant speed reads as acceleration.
  {
    SkPaint q;
    q.setAntiAlias(true);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.9f);
    SkFont f7(monoFace(), 6.8f);
    SkPaint qt;
    qt.setAntiAlias(true);
    // EIGHT EQUAL STEPS IN ln z means the ticks CROWD at the near end, and
    // the labels cannot all be set: over 180 -> 1.5 in the last four ticks
    // fall 17.3 / 8.8 / 4.4 px apart while a five-character label measures
    // about 20 px, so 11.68 / 5.898 / 2.977 / 1.5 would print on top of one
    // another and the near end of the rule would read as a blot. Every tick
    // is still drawn — a ruler whose crowded end is tick-only is what a
    // drafting plate does — and a label is set only when it clears the last
    // one set.
    float lastRight = -1e9f;
    for (int i = 0; i <= 7; ++i) {
      const float zz = kZ0In * std::pow(kR, -(float)i / 7.0f);
      const float x = plateX - zz * S;
      const float a2 = 0.30f + 0.09f * (float)i;
      q.setColor4f(al(kAmber, a2));
      c.drawLine(x, trackY - 8, x, trackY - 2, q);
      qt.setColor4f(al(kAmber, a2));
      const std::string lab = kit::formatted("%.4g", zz);
      const float tw =
          f7.measureText(lab.c_str(), lab.size(), SkTextEncoding::kUTF8);
      const float left = x - tw * 0.5f;
      if (left < lastRight + 3.0f || left + tw > plateX - 2.0f) continue;
      c.drawString(lab.c_str(), left, trackY - 11, f7, qt);
      lastRight = left + tw;
    }
  }

  // The twelve-foot panel, edge on, immediately behind the plate: [T68]'s
  // 0.09 mm of depth at 1.5 in means the artwork must be pressed against
  // the glass or the near end of every streak is a blur of it. [C85] says
  // the plate "stood in front of" the panel and says nothing about the gap.
  p.setColor4f(al(kCold, 0.5f));
  c.drawRect(SkRect::MakeLTRB(plateX + 12, plateCY - kSlitHIn * S * 0.5f,
                              plateX + 15, plateCY + kSlitHIn * S * 0.5f),
             p);

  // THE CARRIAGE at z(tau) = z0 * 120^(-tau) -- log, so it visibly
  // decelerates in FILM terms as it closes, which is the whole reason the
  // corridor reads as acceleration.
  const float z = kZ0In * std::pow(kR, -(float)tau);
  const float camX = plateX - z * S;
  p.setColor4f(kSolid);
  const SkRect body = SkRect::MakeXYWH(camX - 30, trackY - 32, 42, 26);
  c.drawRect(body, p);
  p.setStyle(SkPaint::kStroke_Style);
  p.setColor4f(kAmber);
  c.drawRect(body, p);
  p.setStyle(SkPaint::kFill_Style);
  c.drawCircle(camX - 17, trackY - 38, 7, p);                       // the mag
  c.drawRect(SkRect::MakeXYWH(camX + 12, trackY - 24, 10, 11), p);  // lens
  p.setColor4f(al(kAmber, 0.85f));
  c.drawRect(SkRect::MakeXYWH(camX - 28, trackY - 6, 38, 5), p);  // carriage

  // The light path, 1 px, opacity falling as it lengthens.
  const float beamA = std::clamp(0.9f - 0.62f * (float)tau, 0.1f, 0.9f);
  p.setColor4f(al(kCold, beamA));
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  c.drawLine(camX + 22, trackY - 19, plateX, plateCY + slitH * 0.5f, p);
  c.drawLine(camX + 22, trackY - 19, plateX, plateCY - slitH * 0.5f, p);
  p.setStyle(SkPaint::kFill_Style);

  SkFont f75(monoFace(), 7.5f);
  SkPaint tp;
  tp.setAntiAlias(true);

  auto leader = [&](float ax, float bx, float y, const char* label,
                    SkColor4f col) {
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(col);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.8f);
    c.drawLine(ax, y, bx, y, q);
    c.drawLine(ax, y - 4, ax, y + 4, q);
    c.drawLine(bx, y - 4, bx, y + 4, q);
    q.setStyle(SkPaint::kFill_Style);
    SkPathBuilder h;
    h.moveTo(ax, y).lineTo(ax + 6, y - 2.4f).lineTo(ax + 6, y + 2.4f).close();
    h.moveTo(bx, y).lineTo(bx - 6, y - 2.4f).lineTo(bx - 6, y + 2.4f).close();
    c.drawPath(h.detach(), q);
    tp.setColor4f(col);
    const float tw =
        f75.measureText(label, std::strlen(label), SkTextEncoding::kUTF8);
    c.drawString(label, (ax + bx) * 0.5f - tw * 0.5f, y - 3.5f, f75, tp);
  };
  leader(trackL, plateX, trackY + 20.0f,
         "15 ft = 180 in = 540 px  FOCUS REACH [T68]", kAmber);
  leader(trackL, trackR, trackY + 34.0f, "14 ft = 168 in = 504 px  TRACK [C85]",
         kAmber);

  // The 1.5 in near focus: 4.5 px, dimensioned anyway. The whole erratum of
  // note 2 is visible as a dimension you can barely see -- and as the
  // 36 px of red dashes the published track never reaches.
  {
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(kRed);
    q.setStyle(SkPaint::kStroke_Style);
    q.setStrokeWidth(0.9f);
    const float ax = plateX - kZ1In * S, bx = plateX;
    c.drawLine(ax, trackY - 50, bx, trackY - 50, q);
    c.drawCircle((ax + bx) * 0.5f, trackY - 50, 11, q);
    c.drawLine((ax + bx) * 0.5f - 11, trackY - 50, 466, trackY - 22, q);
    // HALOED. The carriage travels the whole rail over tau, so this caption
    // is printed through by the camera body at some phase of every sweep —
    // and there is no clear band to move it to: above is the clock block,
    // below is the leader pair, the worm-gear rail and the scale note. A
    // 2 px knockout in the panel colour is what a drafting plate does when
    // a note has to cross the drawing.
    // kit::drawHaloed is exactly this: a knockout pass in the ground
    // colour, then the ink. Immediate mode, because this caption lives
    // inside a paint program and cannot reach weave::PaintStyle::addUnderlay
    // — which is the reason the kit ships a canvas spelling at all.
    const kit::Halo halo{.colour = kPanelBg, .width = 2.2f};
    const char* r1 = "1½ in = 4.5 px [T68] — AND [C85]’s TRACK";
    const char* r2 = "STOPS 12 in SHORT: THE RED DASHES.";
    tp.setColor4f(kRed);
    // The BLOCK form: every halo, then every ink. Haloing line by line
    // instead, the second line's knockout eats the first line's descenders.
    kit::drawHaloed(c, {{r1, {304, trackY - 26}}, {r2, {304, trackY - 16}}},
                    f75, tp, halo);
  }

  // The machine's own clock, which is the reason this study is called
  // "time as an axis of the image". Arithmetic over published numbers.
  {
    SkFont f8(monoFace(), 8.0f);
    SkPaint q;
    q.setAntiAlias(true);
    q.setColor4f(al(kCold, 0.92f));
    c.drawString("2 EXPOSURES / FRAME × 45–60 s  [T68][C85]", 304, 44, f8, q);
    q.setColor4f(kType2);
    c.drawString("= 90–120 s PER FILM FRAME  ·  ×24 fps", 304, 56, f8, q);
    c.drawString("MACHINE : SCREEN  =  2160 – 2880 : 1", 304, 68, f8, q);
    c.drawString("[FS]’s 4 min/frame GIVES 5760 : 1", 304, 80, f8, q);
    c.drawString("ONE 36-HOUR TAKE [C85] = 45–60 s OF SCREEN", 304, 92, f8, q);
    q.setColor4f(kAmber);
    c.drawString("ONE SECOND OF THE STAR GATE COST", 304, 110, f8, q);
    c.drawString("36 TO 96 MINUTES OF MACHINE TIME.", 304, 122, f8, q);
    q.setColor4f(kTick);
    c.drawString("SIX MONTHS OF SLIT-SCAN WORK [C85], OF WHICH", 304, 140, f8,
                 q);
    c.drawString("ABOUT A QUARTER REACHED THE FILM [FS].", 304, 152, f8, q);
  }

  // Two lines, not one: the caption column runs out of room at the plate,
  // and set as a single run the tail of it is drawn UNDER the glass panel
  // and never read.
  tp.setColor4f(al(kAmber, 0.9f));
  c.drawString("6 ft PLATE [C85], 4 ft OPENING [NO] —", 292, 10, f75, tp);
  c.drawString("PROBABLY NOT A CONTRADICTION, AND NOTHING SAYS SO", 292, 21,
               f75, tp);
  tp.setColor4f(al(kCold, 0.85f));
  c.drawString("SLIT · X0 = 49.2 in OFF AXIS, w = 0.586 in, 82 : 1", 292, 32,
               f75, tp);
  tp.setColor4f(kTick);
  c.drawString(
      "ONE SCALE: 3.0 px = 1 INCH.  180 in = 540 px · 168 in = 504 px "
      "· 72 in = 216 px · 144 in = 432 px  ·  THE z TICKS ARE EIGHT",
      6, 258, f75, tp);
  c.drawString(
      "EQUAL STEPS IN ln z, WHICH ARE EIGHT EQUAL STEPS IN THE FILM "
      "FRAME  ·  WORM-GEAR DOLLY [NO] · SELSYN FOLLOW-FOCUS [T68]",
      6, 270, f75, tp);
}

// The artwork panel, face on, at the same 3.0 px = 1 inch.
void SlitScan2001::drawArtworkPanel(SkCanvas& c, const PaintContext& ctx) {
  using namespace slit;
  const Shot& s = shotAt(shot);
  const Strip& S = strips[(size_t)s.cell];
  if (!S.image) return;
  const float pw = kPanelIn * kRigPxPerIn;  // 432 px, exact
  const float ph = kSlitHIn * kRigPxPerIn;  // 144 px -- 3:1, undistorted
  const float top = 40.0f;

  SkPaint p;
  p.setAntiAlias(false);
  c.save();
  c.clipRect(SkRect::MakeXYWH(0, top, pw, ph));
  // The panel crawls left by one slit-width per film frame -- pinned by
  // [GE]'s unwrap, not chosen.
  const float ox = -artOffset * pw;
  for (int k = 0; k < 2; ++k) {
    c.save();
    c.translate(ox + (float)k * pw, top);
    c.scale(pw / (float)S.w, ph / (float)S.h);
    c.drawImage(S.image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), &p);
    c.restore();
  }
  c.restore();

  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(1.0f);
  p.setColor4f(al(kAmber, 0.7f));
  c.drawRect(SkRect::MakeXYWH(0, top, pw, ph), p);

  // The slit's window over the panel, with a cased leader back to the rig.
  const float sw = std::max(2.5f, kSlitWIn * kRigPxPerIn);
  const float sx = pw * 0.5f;
  p.setColor4f(kCold);
  p.setStrokeWidth(1.2f);
  c.drawRect(SkRect::MakeXYWH(sx - sw * 0.5f, top - 5, sw, ph + 10), p);
  SkPathBuilder lb;
  lb.moveTo(sx, top + ph + 8);
  lb.lineTo(sx, top + ph + 16);
  lb.lineTo(-38, top + ph + 16);
  decorations::paintOn(
      c, ctx, lb.detach(),
      lines::presets::cased(1.2f, Fill::color(al(kCold, 0.5f)), 3.0f));

  SkFont f76(monoFace(), 7.6f);
  SkPaint tp;
  tp.setAntiAlias(true);
  tp.setColor4f(al(kCold, 0.9f));
  c.drawString(s.art, 0, top - 24, f76, tp);
  tp.setColor4f(kType2);
  c.drawString("THE 12-FOOT PANEL, FACE ON, AT 3.0 px = 1 INCH", 0, top - 12,
               f76, tp);
  tp.setColor4f(kTick);
  c.drawString(
      "12′ MECHANISED BACKLIT GLASS PANEL · TRANSPARENCIES + "
      "CELLULOID GELS · HIGH-CONTRAST",
      0, top + ph + 26, f76, tp);
  c.drawString(
      "NEGATIVES OF OP-ART, ARCHITECTURAL DRAWINGS, CIRCUIT "
      "PRINTS [C85]",
      0, top + ph + 37, f76, tp);
  tp.setColor4f(al(kAmber, 0.95f));
  c.drawString(
      "ADVANCE 0.586 in/FRAME = EXACTLY ONE SLIT WIDTH — PINNED BY "
      "[GE]’S UNWRAP,",
      0, top + ph + 55, f76, tp);
  c.drawString("NOT CHOSEN: MORE WOULD GAP, LESS WOULD REPEAT.", 0,
               top + ph + 66, f76, tp);
  tp.setColor4f(al(kCold, 0.8f));
  c.drawString(
      kit::formatted("ROUND TRIP OVER 96 FRAMES: %d COLUMNS BACK, r = %0.5f, "
                     "%0.3f%% OF PIXELS WRONG",
                     rtCols, rtCorr, rtMismatch * 100.0f)
          .c_str(),
      0, top + ph + 84, f76, tp);
  if (s.cell == 2) {
    tp.setColor4f(kRed);
    c.drawString(
        "THIS STRIP CARRIES [GE]’S WHITE-STRIPE DEFECT — FOUND IN "
        "BOTH SH 27 AND SH 29",
        0, top + ph + 14, f76, tp);
  }
}

// The measured 1/rho profile against the analytic C/u, on log-log axes.
// The frame is normalised so an exact 1/u law is the box diagonal, which
// makes any departure from p = 1 a visible bow rather than a number.
void SlitScan2001::drawMeasuredPoints(SkCanvas& c, const PaintContext& ctx) {
  using namespace slit;
  if (profN <= 0) return;
  const float W = ctx.size.width(), H = ctx.size.height();
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(al(kCold, 0.92f));
  // THE MEASURED SAMPLES. Every ray levelled by its own fitted intercept,
  // then averaged: on these axes an exact 1/u law is the diagonal, so the
  // visible wiggle IS the +-1-stamp quantisation the sampling panel names.
  for (int i = 0; i < profN; i += 2) {
    const float x = profX[(size_t)i] * W;
    const float y = std::clamp(profY[(size_t)i], 0.0f, 1.0f) * H;
    c.drawCircle(x, y, 1.5f, p);
  }
  SkFont f(monoFace(), 6.4f);
  SkPaint tp;
  tp.setAntiAlias(true);
  tp.setColor4f(al(kTick, 0.95f));
  c.drawString("log u  8 → 520 px", 3, H - 3, f, tp);
  c.drawString("log 1/E", 3, 9, f, tp);
  tp.setColor4f(al(kAmber, 0.95f));
  c.drawString("ANALYTIC C/u", W - 66, 9, f, tp);
  tp.setColor4f(al(kCold, 0.95f));
  c.drawString("MEASURED", W - 50, H - 3, f, tp);
}

// ===========================================================================

void SlitScan2001::setup(sketch::SketchContext& ctx) {
  using namespace slit;
  // tau lands on 0.60 here, which is the carriage two thirds down its
  // fourteen feet, mid-exposure — what the +0.60 phase offset is for.
  sketch::kit::stage(ctx, {.size = SkSize::Make(kCanvasW, kCanvasH),
                           .captureAt = 6.0,
                           .background = kInk});
  transfer = transferCurve();

  // ---- bake the artwork ONCE. These are static images made at setup and
  // never mutated afterwards, so they are plain baked SkImages; a live
  // pixel buffer would only be needed if something wrote into them later.
  const double b0 = (double)std::clock() / CLOCKS_PER_SEC;
  gridPat = patterns::gridLines(37.0f, 23.0f, 2.0f, toColor(kWhite));
  // patterns::speckle's tile IS the repeat, so it has to be large or the
  // speckle reads as a visibly repeating stamp.
  spekPat = patterns::speckle(492.0f, 84, 5.0f, 21.0f, {toColor(kWhite)});
  if (ctx.fonts) {
    strips[0] =
        bakeStrip(artOpArt(), *ctx.fonts, (int)kCellW, (int)kCellH, 0.30f, -1);
    strips[1] =
        bakeStrip(artArch(), *ctx.fonts, (int)kCellW, (int)kCellH, 0.30f, -1);
    strips[2] = bakeStrip(artCircuit(gridPat, spekPat), *ctx.fonts, (int)kCellW,
                          (int)kCellH, 0.42f, 903);
  }
  bakeMs = ctx.measured(((double)std::clock() / CLOCKS_PER_SEC - b0) * 1000.0);

  // ---- ONE atlas, THREE cells, ONE bake: one cell per artwork strip, and
  // nothing else. The crawl across a strip is addressed per stamp through
  // Pool::texWindows(), so it costs no extra cells and no re-bake.
  atlas = std::make_shared<instancing::Atlas>(1.0f);
  atlas->filter(SkFilterMode::kNearest);  // 1-bit artwork
  for (int i = 0; i < 3; ++i) {
    const Strip& S = strips[(size_t)i];
    atlas->cell(box().fill(Paint::image(
                    S.image, SkTileMode::kClamp, SkTileMode::kClamp,
                    SkMatrix::Scale(kCellW / (float)std::max(S.w, 1),
                                    kCellH / (float)std::max(S.h, 1)),
                    SkSamplingOptions())),
                {kCellW, kCellH});
  }
  if (ctx.fonts && atlas->ensureBaked(*ctx.fonts) && atlas->image()) {
    sheetW = atlas->image()->width();
    sheetH = atlas->image()->height();
  }
  // A uniform white slit, for the measurement and for the sampling strips:
  // there the subject is the SPACING, not the artwork.
  flatAtlas = std::make_shared<instancing::Atlas>(1.0f);
  flatAtlas->filter(SkFilterMode::kNearest);
  flatAtlas->cell(box().fill(Fill::color(kWhite)), {kCellW, kCellH});

  wallA = std::make_shared<instancing::Pool>();
  wallB = std::make_shared<instancing::Pool>();
  monA = std::make_shared<instancing::Pool>();
  monB = std::make_shared<instancing::Pool>();
  rebuildWalls();

  // ---- the sampling strips: SIX REAL WALLS, rendered small. Two spacing
  // rules against three K. Each is an instances() leaf over its own pool,
  // so the argument is drawn by the same code that draws the picture.
  const int Ks[3] = {12, 48, kK};
  for (int r = 0; r < 2; ++r)
    for (int k = 0; k < 3; ++k) {
      auto pool = std::make_shared<instancing::Pool>();
      WallSpec w;
      w.vp = {0.0f, 10.0f};
      w.gel = kCold;
      w.uFar = 98.0f / kR;  // uNear lands exactly at the strip's right edge
      w.gain = 46.0f;
      w.artwork = false;
      w.K = Ks[k];
      w.logSpaced = (r == 1);
      buildWall(*pool, w);
      s4[(size_t)r * 3 + (size_t)k] = pool;
    }

  // ---- the two verifications
  if (ctx.fonts) {
    measureExposure(*ctx.fonts);
    roundTrip(*ctx.fonts);
  }
  // ---- THE FILM CLOCK. 24 Hz because the film runs at 24 fps. The
  // interpolant drives the shutter bar and the sub-frame readout and
  // NOTHING in the picture: a projector holds a frame for its whole 1/24 s
  // and then replaces it, so tweening the frame would be wrong.
  ctx.ticker.addFixed(
      24.0,
      [this] {
        ++filmNo;
        artOffset = std::fmod(artOffset + kAdvanceIn / kPanelIn, 1.0f);
        plateDeg += shotAt(shotFor(filmNo)).dPhi;
        return true;
      },
      8, &frameAlpha, &fixedStatus);

  // ---- the demonstration clock: tau sweeps once per 3.0 s.
  ctx.ticker.add([this](double dt) {
    elapsed += dt;
    // Phase-offset so the recommended capture lands with the carriage
    // two thirds down its fourteen feet, mid-exposure.
    tau = std::fmod(elapsed / 3.0 + 0.60, 1.0);
    rebuildWalls();
    return true;
  });

  if (ctx.fonts) {
    const SkSize a1 = intrinsicSize(s1Quote(), *ctx.fonts, {kSideW, 4000});
    const SkSize a2 = intrinsicSize(s2Lens(), *ctx.fonts, {kSideW, 4000});
    const SkSize a3 = intrinsicSize(s3Law(), *ctx.fonts, {kSideW, 4000});
    const SkSize a4 = intrinsicSize(s4Sampling(), *ctx.fonts, {kSideW, 4000});
    // A layout self-check, because a flex column silently overlaps its
    // children rather than complaining when they do not fit.
    const float total = a1.height() + a2.height() + a3.height() + a4.height();
    std::fprintf(
        stderr,
        "[slitscan] sidebar %.0f + %.0f + %.0f + %.0f = %.0f / %.0f%s\n",
        a1.height(), a2.height(), a3.height(), a4.height(), total, kBodyH,
        total > kBodyH ? "   *** OVER BUDGET ***" : "");
  }
  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("readout", readoutEl());
  ctx.composer.renderSlot("expo", expoEl());
  ctx.composer.renderSlot("fit", fitEl());
  ctx.composer.renderSlot("ripple", rippleEl());
}

void SlitScan2001::update(double e, sketch::SketchContext& ctx) {
  if (fixedStatus.clamped) everClamped = true;
  // Shot cuts are HARD, and they land on a FILM FRAME (every 96th), not on
  // a wall-clock boundary. The film cuts.
  (void)e;
  const int s = shotFor(filmNo);
  if (s != shot) {
    shot = s;
    rebuildWalls();
    ctx.composer.render(describe(ctx));
  }
  // The live numbers go through slot()/renderSlot(), so the tree itself is
  // never re-described per frame.
  ctx.composer.renderSlot("readout", readoutEl());
  ctx.composer.renderSlot("expo", expoEl());
  ctx.composer.renderSlot("fit", fitEl());
}

SIGIL_SKETCH(SlitScan2001, "Study \xc2\xb7 Motion",
             "Trumbull's slit-scan machine (1966\xe2\x80\x93"
             "68) \xe2\x80\x94 a frame that is a time integral")
