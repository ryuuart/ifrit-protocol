// slitscan 2001: scene assembly and animation.

// TAGS: Media/Video

#include "SlitScan2001.h"

auto SlitScan2001::describe(sketch::SketchContext& ctx) -> Element {
  using namespace slit;
  return box()
      .column()
      .width(kCanvasW)
      .height(kCanvasH)
      .padding(kPad)
      .gap(20)
      .fill(kInk)
      .children({header(),
                 box().row().gap(28).height(kBodyH).children(
                     {box().column().width(kLeftW).shrink(0).gap(24).children(
                          {filmFrame(), rigStrip()}),
                      sidebar()})});
}

// ===========================================================================
// The rig elevation. ONE SCALE: 3.0 px = 1 inch, throughout.
//
// Drawn as one custom() leaf -- custom() measures ZERO on the main axis, so
// it carries explicit width/height -- with every brushed mark going through
// decorations::paintOn(), which is how the whole vocabulary reaches
// hand-built geometry.

/** A COMPOSE DECORATION ON A PATH THE PEN DREW: the pen's canvas is the
 *  door out of p5's vocabulary, and the node's own paint context — which
 *  a pen program names when it reads it — is what the decoration needs to
 *  know where it stands. */
static void dressed(Pen& pen, const PaintContext& node, SkPath outline,
                    const Decoration& dress) {
  PaintContext ctx = node;
  ctx.outline = outline;
  decorations::paintOn(*pen.canvas(), ctx, std::move(outline), dress);
}

void SlitScan2001::drawRig(Pen& pen, const PaintContext& ctx) {
  using namespace slit;
  const float S = kRigPxPerIn;
  const float plateX = 548.0f;  // the slit; z = 0 for focus
  const float trackY = 194.0f;
  const float plateCY = 116.0f;
  const float plateH = kPlateIn * S;  // 216 px
  const float benchT = 232.0f, benchB = 244.0f;

  pen.textFont(monoFace());

  // The bench, as a drafting section: a solid with 45-degree hatching. The
  // hatch is a compose decoration over the pen's own canvas, which is the
  // door the pen keeps open for the rest of this repository's vocabulary.
  const SkRect bench = SkRect::MakeLTRB(4, benchT, kElevW - 6, benchB);
  pen.fill(kSolid);
  pen.stroke(al(kAmber, 0.55f));
  pen.strokeWeight(1.0f);
  pen.rect(bench.x(), bench.y(), bench.width(), bench.height());
  SkPathBuilder hatched;
  hatched.addRect(bench);
  dressed(
      pen, ctx, hatched.detach(),
      lines::presets::hatch(Fill::color(al(kAmber, 0.20f)), 6.0f, 1.0f, 45.0f));

  // THE TRACK: 14 ft = 168 in = 504 px [C85], from z = 180 in down to
  // z = 12 in -- which is where note 2 becomes a picture. A double rule,
  // with the worm gear as a micro-pitch lattice along it [NO].
  const float trackL = plateX - kZ0In * S;     // z = 180 in, x = 8
  const float trackR = trackL + kTrackIn * S;  // z = 12 in,  x = 512
  pen.noStroke();
  pen.fill(kAmber);
  pen.rect(trackL, trackY - 1, trackR - trackL, 1);
  pen.rect(trackL, trackY + 5, trackR - trackL, 1);
  pen.fill(al(kAmber, 0.5f));
  // the loops walk a distance; the accumulated float is the position
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float x = trackL; x <= trackR; x += 4.0f)
    pen.rect(x, trackY + 1, 1.7f, 4);
  // …and the 36 px of reach the published track does NOT cover.
  pen.fill(al(kRed, 0.85f));
  // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
  for (float x = trackR; x < plateX - 2; x += 5.0f)
    pen.rect(x, trackY + 2, 2.0f, 2);

  // THE PLATE: 6 ft = 216 px tall, rotatable [C85], turning live.
  const float slitH = kSlitHIn * S;  // 4 ft of the 6 ft plate [NO] vs [C85]
  pen.push();
  pen.translate(plateX, plateCY);
  pen.rotate(plateDeg * 0.4f);
  pen.fill(kSolid);
  pen.stroke(kAmber);
  pen.strokeWeight(1.0f);
  pen.rect(-9, -plateH * 0.5f, 18, plateH);
  pen.noStroke();
  pen.fill(al(kCold, 0.26f));
  pen.rect(-5.0f, -slitH * 0.5f, 10, slitH);
  pen.fill(kCold);
  pen.rect(-0.9f, -slitH * 0.5f, 1.8f, slitH);
  pen.pop();

  // THE LOG TICKS. z halves eight times across the sweep, and because
  // u = f*X0/z the SAME eight steps are equal intervals in the film frame.
  // Drawing them on the track is the clearest statement of why a corridor
  // shot at constant speed reads as acceleration.
  //
  // EIGHT EQUAL STEPS IN ln z means the ticks CROWD at the near end, and
  // the labels cannot all be set: over 180 -> 1.5 in the last four ticks
  // fall 17.3 / 8.8 / 4.4 px apart while a five-character label measures
  // about 20 px, so 11.68 / 5.898 / 2.977 / 1.5 would print on top of one
  // another and the near end of the rule would read as a blot. Every tick
  // is still drawn — a ruler whose crowded end is tick-only is what a
  // drafting plate does — and a label is set only when it clears the last
  // one set.
  pen.textSize(6.8f);
  pen.strokeWeight(0.9f);
  float lastRight = -1e9f;
  for (int i = 0; i <= 7; ++i) {
    const float zz = kZ0In * std::pow(kR, -(float)i / 7.0f);
    const float x = plateX - zz * S;
    const float a2 = 0.30f + 0.09f * (float)i;
    pen.stroke(al(kAmber, a2));
    pen.line(x, trackY - 8, x, trackY - 2);
    const std::string lab = kit::formatted("%.4g", zz);
    const float left = x - pen.textWidth(lab) * 0.5f;
    if (left < lastRight + 3.0f || left + pen.textWidth(lab) > plateX - 2.0f)
      continue;
    pen.fill(al(kAmber, a2));
    pen.text(lab, left, trackY - 11);
    lastRight = left + pen.textWidth(lab);
  }
  pen.noStroke();

  // The twelve-foot panel, edge on, immediately behind the plate: [T68]'s
  // 0.09 mm of depth at 1.5 in means the artwork must be pressed against
  // the glass or the near end of every streak is a blur of it. [C85] says
  // the plate "stood in front of" the panel and says nothing about the gap.
  pen.fill(al(kCold, 0.5f));
  pen.rect(plateX + 12, plateCY - slitH * 0.5f, 3, slitH);

  // THE CARRIAGE at z(tau) = z0 * 120^(-tau) -- log, so it visibly
  // decelerates in FILM terms as it closes, which is the whole reason the
  // corridor reads as acceleration.
  const float z = kZ0In * std::pow(kR, -(float)tau);
  const float camX = plateX - z * S;
  pen.fill(kSolid);
  pen.stroke(kAmber);
  pen.rect(camX - 30, trackY - 32, 42, 26);
  pen.noStroke();
  pen.fill(kAmber);
  pen.circle(camX - 17, trackY - 38, 14);    // the mag
  pen.rect(camX + 12, trackY - 24, 10, 11);  // the lens
  pen.fill(al(kAmber, 0.85f));
  pen.rect(camX - 28, trackY - 6, 38, 5);  // the carriage

  // The light path, 1 px, opacity falling as it lengthens.
  pen.noFill();
  pen.strokeWeight(1.0f);
  pen.stroke(al(kCold, std::clamp(0.9f - 0.62f * (float)tau, 0.1f, 0.9f)));
  pen.line(camX + 22, trackY - 19, plateX, plateCY + slitH * 0.5f);
  pen.line(camX + 22, trackY - 19, plateX, plateCY - slitH * 0.5f);

  // A DIMENSION, as a drafting plate sets one: a run between two witness
  // ticks, an arrowhead at each end, and the measure over the middle.
  pen.textSize(7.5f);
  auto leader = [&pen](float ax, float bx, float y, const char* label,
                       SkColor4f col) {
    pen.stroke(col);
    pen.noFill();
    pen.strokeWeight(0.8f);
    pen.line(ax, y, bx, y);
    pen.line(ax, y - 4, ax, y + 4);
    pen.line(bx, y - 4, bx, y + 4);
    pen.noStroke();
    pen.fill(col);
    pen.triangle(ax, y, ax + 6, y - 2.4f, ax + 6, y + 2.4f);
    pen.triangle(bx, y, bx - 6, y - 2.4f, bx - 6, y + 2.4f);
    pen.text(label, (ax + bx) * 0.5f - pen.textWidth(label) * 0.5f, y - 3.5f);
  };
  leader(trackL, plateX, trackY + 20.0f,
         "15 ft = 180 in = 540 px  FOCUS REACH [T68]", kAmber);
  leader(trackL, trackR, trackY + 34.0f, "14 ft = 168 in = 504 px  TRACK [C85]",
         kAmber);

  // The 1.5 in near focus: 4.5 px, dimensioned anyway. The whole erratum of
  // note 2 is visible as a dimension you can barely see -- and as the
  // 36 px of red dashes the published track never reaches.
  {
    const float ax = plateX - kZ1In * S, bx = plateX;
    pen.noFill();
    pen.stroke(kRed);
    pen.strokeWeight(0.9f);
    pen.line(ax, trackY - 50, bx, trackY - 50);
    pen.circle((ax + bx) * 0.5f, trackY - 50, 22);
    pen.line((ax + bx) * 0.5f - 11, trackY - 50, 466, trackY - 22);
    // HALOED. The carriage travels the whole rail over tau, so this caption
    // is printed through by the camera body at some phase of every sweep —
    // and there is no clear band to move it to: above is the clock block,
    // below is the leader pair, the worm-gear rail and the scale note. A
    // 2 px knockout in the panel colour is what a drafting plate does when
    // a note has to cross the drawing, and kit::drawHaloed is exactly that
    // pass — a knockout in the ground colour, then the ink. The BLOCK form:
    // every halo, then every ink, because haloing line by line lets the
    // second line's knockout eat the first line's descenders.
    SkFont f75(monoFace(), 7.5f);
    SkPaint ink;
    ink.setAntiAlias(true);
    ink.setColor4f(kRed);
    kit::drawHaloed(
        *pen.canvas(),
        {{"1½ in = 4.5 px [T68] — AND [C85]’s TRACK", {304, trackY - 26}},
         {"STOPS 12 in SHORT: THE RED DASHES.", {304, trackY - 16}}},
        f75, ink, {.colour = kPanelBg, .width = 2.2f});
  }

  // The machine's own clock, which is the reason this study is called
  // "time as an axis of the image". Arithmetic over published numbers, set
  // as a block: every line at 304, one leading apart, in the ink its own
  // claim is printed in.
  pen.noStroke();
  pen.textSize(8.0f);
  struct Told {
    const char* words;
    float y;
    SkColor4f ink;
  };
  static const std::array<Told, 9> clock{
      {{"2 EXPOSURES / FRAME × 45–60 s  [T68][C85]", 44, al(kCold, 0.92f)},
       {"= 90–120 s PER FILM FRAME  ·  ×24 fps", 56, kType2},
       {"MACHINE : SCREEN  =  2160 – 2880 : 1", 68, kType2},
       {"[FS]’s 4 min/frame GIVES 5760 : 1", 80, kType2},
       {"ONE 36-HOUR TAKE [C85] = 45–60 s OF SCREEN", 92, kType2},
       {"ONE SECOND OF THE STAR GATE COST", 110, kAmber},
       {"36 TO 96 MINUTES OF MACHINE TIME.", 122, kAmber},
       {"SIX MONTHS OF SLIT-SCAN WORK [C85], OF WHICH", 140, kTick},
       {"ABOUT A QUARTER REACHED THE FILM [FS].", 152, kTick}}};
  for (const Told& line : clock) {
    pen.fill(line.ink);
    pen.text(line.words, 304, line.y);
  }

  // Two lines, not one: the caption column runs out of room at the plate,
  // and set as a single run the tail of it is drawn UNDER the glass panel
  // and never read.
  pen.textSize(7.5f);
  static const std::array<Told, 3> notes{
      {{"6 ft PLATE [C85], 4 ft OPENING [NO] —", 10, al(kAmber, 0.9f)},
       {"PROBABLY NOT A CONTRADICTION, AND NOTHING SAYS SO", 21,
        al(kAmber, 0.9f)},
       {"SLIT · X0 = 49.2 in OFF AXIS, w = 0.586 in, 82 : 1", 32,
        al(kCold, 0.85f)}}};
  for (const Told& line : notes) {
    pen.fill(line.ink);
    pen.text(line.words, 292, line.y);
  }
  pen.fill(kTick);
  pen.text(
      "ONE SCALE: 3.0 px = 1 INCH.  180 in = 540 px · 168 in = 504 px "
      "· 72 in = 216 px · 144 in = 432 px  ·  THE z TICKS ARE EIGHT",
      6, 258);
  pen.text(
      "EQUAL STEPS IN ln z, WHICH ARE EIGHT EQUAL STEPS IN THE FILM "
      "FRAME  ·  WORM-GEAR DOLLY [NO] · SELSYN FOLLOW-FOCUS [T68]",
      6, 270);
}

// The artwork panel, face on, at the same 3.0 px = 1 inch.
void SlitScan2001::drawArtworkPanel(Pen& pen, const PaintContext& ctx) {
  using namespace slit;
  const Shot& s = shotAt(shot);
  const Strip& S = strips[(size_t)s.cell];
  if (!S.image) return;
  const float pw = kPanelIn * kRigPxPerIn;  // 432 px, exact
  const float ph = kSlitHIn * kRigPxPerIn;  // 144 px -- 3:1, undistorted
  const float top = 40.0f;

  pen.textFont(monoFace());
  // The panel crawls left by one slit-width per film frame -- pinned by
  // [GE]'s unwrap, not chosen — so two copies stand side by side and the
  // window keeps whichever part of them is under the slit. 1-bit artwork,
  // so it is resampled with no smoothing at all.
  pen.push();
  pen.noSmooth();
  pen.clip([&pen, pw, ph, top] { pen.rect(0, top, pw, ph); });
  for (int k = 0; k < 2; ++k)
    pen.image(S.image, -artOffset * pw + (float)k * pw, top, pw, ph);
  pen.pop();
  pen.smooth();

  pen.noFill();
  pen.stroke(al(kAmber, 0.7f));
  pen.strokeWeight(1.0f);
  pen.rect(0, top, pw, ph);

  // The slit's window over the panel, with a cased leader back to the rig.
  const float sw = std::max(2.5f, kSlitWIn * kRigPxPerIn);
  const float sx = pw * 0.5f;
  pen.stroke(kCold);
  pen.strokeWeight(1.2f);
  pen.rect(sx - sw * 0.5f, top - 5, sw, ph + 10);
  SkPathBuilder lb;
  lb.moveTo(sx, top + ph + 8);
  lb.lineTo(sx, top + ph + 16);
  lb.lineTo(-38, top + ph + 16);
  dressed(pen, ctx, lb.detach(),
          lines::presets::cased(1.2f, Fill::color(al(kCold, 0.5f)), 3.0f));

  pen.noStroke();
  pen.textSize(7.6f);
  struct Told {
    std::string words;
    float y;
    SkColor4f ink;
  };
  std::vector<Told> notes{
      {s.art, top - 24, al(kCold, 0.9f)},
      {"THE 12-FOOT PANEL, FACE ON, AT 3.0 px = 1 INCH", top - 12, kType2},
      {"12′ MECHANISED BACKLIT GLASS PANEL · TRANSPARENCIES + "
       "CELLULOID GELS · HIGH-CONTRAST",
       top + ph + 26, kTick},
      {"NEGATIVES OF OP-ART, ARCHITECTURAL DRAWINGS, CIRCUIT "
       "PRINTS [C85]",
       top + ph + 37, kTick},
      {"ADVANCE 0.586 in/FRAME = EXACTLY ONE SLIT WIDTH — PINNED BY "
       "[GE]’S UNWRAP,",
       top + ph + 55, al(kAmber, 0.95f)},
      {"NOT CHOSEN: MORE WOULD GAP, LESS WOULD REPEAT.", top + ph + 66,
       al(kAmber, 0.95f)},
      {kit::formatted("ROUND TRIP OVER 96 FRAMES: %d COLUMNS BACK, r = %0.5f, "
                      "%0.3f%% OF PIXELS WRONG",
                      rtCols, rtCorr, rtMismatch * 100.0f),
       top + ph + 84, al(kCold, 0.8f)}};
  if (s.cell == 2)
    notes.push_back(
        {"THIS STRIP CARRIES [GE]’S WHITE-STRIPE DEFECT — FOUND IN "
         "BOTH SH 27 AND SH 29",
         top + ph + 14, kRed});
  for (const Told& line : notes) {
    pen.fill(line.ink);
    pen.text(line.words, 0, line.y);
  }
}

// The measured 1/rho profile against the analytic C/u, on log-log axes.
// The frame is normalised so an exact 1/u law is the box diagonal, which
// makes any departure from p = 1 a visible bow rather than a number.
void SlitScan2001::drawMeasuredPoints(Pen& pen) {
  using namespace slit;
  if (profN <= 0) return;
  const float W = pen.width, H = pen.height;
  // THE MEASURED SAMPLES. Every ray levelled by its own fitted intercept,
  // then averaged: on these axes an exact 1/u law is the diagonal, so the
  // visible wiggle IS the +-1-stamp quantisation the sampling panel names.
  pen.noStroke();
  pen.fill(al(kCold, 0.92f));
  for (int i = 0; i < profN; i += 2)
    pen.circle(profX[(size_t)i] * W,
               std::clamp(profY[(size_t)i], 0.0f, 1.0f) * H, 3.0f);
  pen.textFont(monoFace());
  pen.textSize(6.4f);
  pen.fill(al(kTick, 0.95f));
  pen.text("log u  8 → 520 px", 3, H - 3);
  pen.text("log 1/E", 3, 9);
  pen.fill(al(kAmber, 0.95f));
  pen.text("ANALYTIC C/u", W - 66, 9);
  pen.fill(al(kCold, 0.95f));
  pen.text("MEASURED", W - 50, H - 3);
}

// ===========================================================================

const data::Json& SlitScan2001::doc() const {
  static const data::Json none;
  return words ? *words : none;
}

void SlitScan2001::setup(sketch::SketchContext& ctx) {
  using namespace slit;
  words = ctx.assets.json(ctx.local("data/content.json"));
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

SIGIL_SKETCH(
    SlitScan2001, "Study · Motion",
    "Trumbull's slit-scan machine (1966–68) — a frame that is a time integral")
