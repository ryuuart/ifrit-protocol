// The Black Watch sett woven from its thread counts and twill rule.

// TAGS: Patterns/Tiling

#include "Tartan.h"

struct BlackWatch : sketch::Sketch {
  // --- the data -----------------------------------------------------------
  std::vector<Run> bwRuns, caRuns;
  std::vector<uint8_t> S, A;
  Verdict v;
  std::vector<int> runStart;

  // --- the loom -----------------------------------------------------------
  choreograph::Output<float> loom{0};
  double clock = 0;

  // --- baked material ------------------------------------------------------
  // Held as members: the shared bake IS the identity (Pattern.h), and a fresh
  // Pattern per describe would re-render its tile on every render().
  Pattern warpPattern;                  // the 1-D sequence, 252 bands
  std::array<Pattern, 12> pickPattern;  // (colour, twill phase)
  Pattern threadGrid;                   // the interlacement grooves
  Paint warpMat, gridMat, boardMat, yarnGrain, drawGrid, swatchMat;
  std::vector<Paint> pickMat;  // 12, resolved once
  std::shared_ptr<instancing::Atlas> pickAtlas;
  std::shared_ptr<instancing::Pool> pickPool;
  std::vector<Paint> clothMat;  // 5 palettes, whole cloth
  Paint argyllMat;
  std::array<Paint, 9> blendMat;
  std::shared_ptr<const sigil::image::ImageAsset> drawdownAsset;
  measure::Table verdict;
  std::shared_ptr<weave::Paragraph> quote;

  // The drawdown window: threads 60..91 of the sett. The 18-thread black of
  // unit A sits between the last blue and the first green, so no shorter
  // window carries all three colours at once; this one runs K2 B6 K18 G6 and
  // therefore shows both kinds of blend cell as well as three solids.
  static constexpr int kDrawOrigin = 60, kDrawN = 32;
  static constexpr float kDrawCell = 9;

  // =========================================================================

  void build() {
    bwRuns = concatRuns({kSettA, kSettB, kSettC, kSettB});
    caRuns = concatRuns({kArgA, kArgB, kArgC, kArgD});
    S = patterns::threadcount(bwRuns);
    A = patterns::threadcount(caRuns);
    {
      int cur = 0;
      for (const Run& r : bwRuns) {
        runStart.push_back(cur);
        cur += r.threads;
      }
    }
    v = verify(bwRuns, S, A);

    const Shades modern = shadesOf(kPalettes[0]);

    // 1. THE WARP GROUND — one element for 252 threads. A tile exactly one
    //    sett wide; the bands are half-open, from the same cursor rule the
    //    expansion used.
    {
      const std::vector<Run> runs = bwRuns;
      warpPattern = Pattern::tile(
          {kPx * (float)S.size(), 8},
          // copying the captures can fail only on allocation
          // NOLINTNEXTLINE(bugprone-exception-escape)
          [runs, modern](SkCanvas& c, SkSize sz, uint32_t) {
            SkPaint p;
            float cur = 0;
            for (const Run& r : runs) {
              p.setColor4f(modern[(size_t)r.shade], nullptr);
              c.drawRect(
                  SkRect::MakeXYWH(cur, 0, kPx * (float)r.threads, sz.height()),
                  p);
              cur += kPx * (float)r.threads;
            }
          });
      warpMat = warpPattern.material();
    }

    // 2. THE WEFT — the defining property of a twill is that the
    //    interlacement advances ONE THREAD PER PICK, and a Pattern tile cannot
    //    be panned. Saved only by the phase being mod 4: the weft shows
    //    where (x - y) mod 4 is in {2, 3}, i.e. a 4-px-on / 4-px-off stripe
    //    whose phase is ((i + 2) mod 4) * 2 px. Three colours by four phases
    //    is twelve patterns, built once, and every one of the 378 picks
    //    selects one of them. A weave with any other modulus would have had
    //    nothing to select from.
    for (int c = 0; c < 3; ++c)
      for (int ph = 0; ph < 4; ++ph) {
        const SkColor4f col = modern[(size_t)c];
        const float off = (float)ph * kPx;
        pickPattern[(size_t)c * 4 + (size_t)ph] = Pattern::tile(
            {4 * kPx, kPx}, [col, off](SkCanvas& cv, SkSize sz, uint32_t) {
              SkPaint p;
              p.setColor4f(col, nullptr);
              // the float, plus its wrap copy so the tile stays seamless
              cv.drawRect(SkRect::MakeXYWH(off, 0, 2 * kPx, sz.height()), p);
              cv.drawRect(
                  SkRect::MakeXYWH(off - sz.width(), 0, 2 * kPx, sz.height()),
                  p);
            });
      }
    pickMat.clear();
    for (Pattern& p : pickPattern) pickMat.push_back(p.material());

    // The twelve pick tiles as an ATLAS of twelve cells, each one full-width
    // strip of cloth, and a POOL of one frame per pick. kNearest, because a
    // thread is a whole number of pixels and any filtering across a stripe
    // boundary is a blur of the interlacement this card exists to show.
    pickAtlas = std::make_shared<instancing::Atlas>(2.0f);
    pickAtlas->filter(SkFilterMode::kNearest);
    std::array<int, 12> frame{};
    for (int c = 0; c < 3; ++c)
      for (int ph = 0; ph < 4; ++ph) {
        const size_t k = (size_t)c * 4 + (size_t)ph;
        frame[k] = pickAtlas->cell(box().fill(pickMat[k]), {kClothW, kPx});
      }
    auto pool = std::make_shared<instancing::Pool>();
    pool->resize((size_t)kPicks);
    {
      // A stamp is anchored at the sprite's CENTRE, so pick i sits half a
      // thread below its own top edge.
      auto pos = pool->positions();
      auto fr = pool->frames();
      auto alpha = pool->alphas();  // the opt-in fade lane
      for (int i = 0; i < kPicks; ++i) {
        const int col = (int)S[(size_t)(i % (int)S.size())];
        const int phase = (i + 2) % 4;
        pos[(size_t)i] = {kClothW * 0.5f, ((float)i + 0.5f) * kPx};
        fr[(size_t)i] = frame[(size_t)col * 4 + (size_t)phase];
        alpha[(size_t)i] = 0.0f;
      }
      pool->commit();
    }
    pickPool = pool;

    // 3. THE INTERLACEMENT SHADOW — in real cloth the thread that is UNDER at
    //    a cell is shaded by the one on top, and that is why the twill rib is
    //    legible even inside a block of ONE colour. Read it off the register's
    //    swatch: its solid green square is not flat, it is ribbed. So the
    //    overlay is not a grid, it is the DRAFT again, at very low alpha —
    //    one 4x4-thread tile (the twill's own period), weft-up cells darkened,
    //    plus a hairline at each pick and end boundary for the yarn grooves.
    //    One element, multiplied over the whole panel.
    threadGrid = Pattern::tile({4 * kPx, 4 * kPx}, [](SkCanvas& c, SkSize,
                                                      uint32_t) {
      SkPaint p;
      p.setColor4f({0, 0, 0, 0.17f}, nullptr);
      for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
          if (!warpUp(x, y))
            c.drawRect(
                SkRect::MakeXYWH((float)x * kPx, (float)y * kPx, kPx, kPx), p);
      p.setColor4f({0, 0, 0, 0.16f}, nullptr);
      for (int i = 0; i < 4; ++i)
        c.drawRect(SkRect::MakeXYWH(0, (float)i * kPx, 4 * kPx, 1), p);
      p.setColor4f({0, 0, 0, 0.09f}, nullptr);
      for (int i = 0; i < 4; ++i)
        c.drawRect(SkRect::MakeXYWH((float)i * kPx, 0, 1, 4 * kPx), p);
    });
    gridMat = threadGrid.material();
    drawGrid =
        Pattern(patterns::gridLines(kDrawCell, 0.7f,
                                    skia::toColor(hexColor(0x8A8478, 0.6f))))
            .material();

    // 4. WHOLE-CLOTH BAKES — one per palette family, 252 x 252 at one pixel
    //    per thread, magnified x2 with kNearest. Same 252 threads, same
    //    draft, five legitimate cloths: the palette turn costs five images
    //    and changes nothing structural, which IS the colour argument.
    clothMat.clear();
    for (const Palette& p : kPalettes)
      clothMat.push_back(imageMat(
          bakeCloth(S, shadesOf(p), 0, 0, (int)S.size(), (int)S.size()), kPx));

    // The provenance swatches are CROPS, never a scaled-down sett: 90 x 55
    // threads at the same 2 px, taken at the same offset in all four. Origin
    // (34, 40) puts a blue square, a green square, the black bars between
    // them and both kinds of blend cell inside 180 x 110 px.
    swatchMat = imageMat(bakeCloth(S, modern, 34, 40, 90, 55), kPx);
    // Campbell's two overchecks are 208 threads apart, so no single square
    // crop contains both. This one takes its warp from the yellow band and
    // its weft from the white one — a real region of the cloth, and the only
    // place the two overchecks are seen crossing.
    argyllMat = imageMat(bakeCloth(A, modern, 150, 358, 90, 55), kPx);

    // 5. THE BLEND TABLE and THE DRAWDOWN — the same generator at different
    //    scales, never authored separately.
    for (int wp = 0; wp < 3; ++wp)
      for (int wf = 0; wf < 3; ++wf)
        blendMat[(size_t)wf * 3 + (size_t)wp] =
            imageMat(bakeBlend(modern[(size_t)wp], modern[(size_t)wf], 6), 8);
    drawdownAsset = std::make_shared<const sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(bakeCloth(
            S, modern, kDrawOrigin, kDrawOrigin, kDrawN, kDrawN, 0.22f)));

    // 6. Card tooth and yarn tooth. grain() is the LUMINANCE field, and an
    //    opaque manila board is where its header says it belongs. Both keep
    //    frequency * stretch * 2^(octaves-1) under 0.4, or the y axis aliases
    //    into hash noise with no diagnostic.
    boardMat = Paint::recipe(matkit::board({.paint = skia::toColor(kCard),
                                            .tooth = 0.10f,
                                            .toothScale = 0.045f,
                                            .wear = 0.05f,
                                            .wearScale = 0.004f,
                                            .seed = 7.0f}));
    yarnGrain = Paint::recipe(field::grain(0.09f, 3, 3.0f, 0.75f));

    buildVerifyTable();
    buildQuote();
  }

  /** THE VERIFICATION, as one table. Every row's verdict is COMPUTED from
   *  the two values it reports, so a row that reads PASS cannot disagree
   *  with the arithmetic beside it, and the panel reads the verdict as a
   *  value rather than sniffing it out of a formatted string. The two rows
   *  that are measurements with nothing to judge are
   *  readings; the one that is a statement about the SETTS rather than
   *  about this reconstruction — that Black Watch and Campbell of Argyll
   *  agree unit for unit — is a finding. */
  void buildVerifyTable() {
    verdict = {};
    verdict
        .add(measure::check(
            kit::formatted("SETT CLOSES      %d + %d + %d + %d ends", v.unitA,
                           v.unitB, v.unitC, v.unitB),
            kPublishedEnds, v.total))
        .add(measure::check(
            kit::formatted("REFLECTIVE       mirrors at thread %d, %d, gap",
                           !v.mirrors.empty() ? v.mirrors[0] : -1,
                           v.mirrors.size() > 1 ? v.mirrors[1] : -1),
            v.total / 2, v.mirrorGap))
        .add(measure::check(
            kit::formatted("2/2 BALANCE      max warp float %d, max weft float",
                           v.maxWarpFloat),
            2, v.maxWeftFloat))
        .add(measure::check(
            kit::formatted(
                "THREAD RATIO     K %d : B %d : G %d, blue is the third",
                v.counts[K], v.counts[B], v.counts[G]),
            v.blueIsThird))
        .add(measure::check(
            kit::formatted(
                "COLOUR LAW       n = %d \xe2\x86\x92 n(n+1)/2 perceived",
                v.solids),
            v.solids * (v.solids + 1) / 2, v.perceived))
        .add(measure::check("EXACT COVER      uncovered", 0, v.uncovered))
        .add(measure::check("                 doubled", 0, v.doubled))
        .add(measure::reading("                 samples", v.samples))
        .add(measure::check(
            kit::formatted(
                "CAMPBELL ARGYLL  n = %d \xe2\x86\x92 %d perceived, ends",
                v.argyllSolids, v.argyllPerceived),
            kPublishedArgyll, v.argyllTotal))
        // The two setts are the same design at two scales — a claim about
        // the CLOTH, not about this file, so its verdict is printed and
        // never counted against the run.
        .add(measure::finding(measure::check(
            "UNIT DRIFT       max |BW \xe2\x88\x92 CA| over A B C D, %", 0.0,
            (double)(v.unitDrift * 100.0f), 1.0)))
        .add(measure::reading("TWILL ANGLE      42 epi = 42 ppi, degrees",
                              std::atan2(1.0, 1.0) * 180.0 / 3.14159265358979))
        .add(measure::reading(
            kit::formatted("SETT WIDTH       %d ends / 42 epi, mm", v.total),
            (double)((float)v.total / 42.0f * 25.4f)));
  }

  void buildQuote() {
    // One genuinely justified paragraph: Knuth-Plass, hyphenation on, a real
    // 320 px measure. SigilWeave breaks at SOFT HYPHENS only, so the
    // discretionaries are typed in (U+00AD) the way a compositor would set
    // them — HyphenationOptions has no automatic dictionary behind it.
    weave::TextStyle body =
        weave::textStyle({.face = serif(), .size = 13, .color = kInk});
    body.shaping.languageTag = "en-GB";
    weave::TextStyle attrib =
        weave::textStyle({.face = serifIt(), .size = 11, .color = kInk2});
    weave::ParagraphBuilder b(body);
    b.addText(
        u8"“The per­fect bal­ance of the weav­ing "
        u8"— ex­actly as many weft shots per inch as there "
        u8"are warp ends, to give a 45-de­gree twill an­gle "
        u8"— is of ut­most im­por­tance in "
        u8"pro­duc­ing a true Tar­tan, as each of the "
        u8"col­or blocks must be squared in its prop­er "
        u8"suc­ces­sion.”  ");
    b.pushStyle(attrib);
    b.addText(
        u8"— Harriet C. Douglas, Scotch Tartan Setts, "
        u8"Shuttle-Craft Guild, 1949");
    quote = std::make_shared<weave::Paragraph>(b.build());
  }

  // =========================================================================
  // The cloth. One striped ground, 378 phase-shifted picks, and nothing else.

  Element theCloth() {
    Element panel = at(kClothX, kClothY, kClothW, kClothH)
                        .clip(true)
                        .background(styles::dropShadow(
                            hexColor(0x3E3A33, 0.55f), {3, 4}, 10))
                        .fill(kWell);

    // the warp on the beam: the whole design, in one dimension
    panel.child(at(0, 0, kClothW, kClothH).fill(warpMat));

    // the picks. 378 strips that differ only in WHICH of twelve pick
    // materials they wear and how far along the beat they have beaten in —
    // which is one instanced leaf: an Atlas of the twelve (colour, phase)
    // tiles, and a Pool of 378 frames whose per-instance ALPHA lane is
    // written from the loom. One draw, no layout, and a fade that rewrites
    // one float per pick instead of running 378 bindings.
    panel.child(at(0, 0, kClothW, kClothH)
                    .child(instancing::instances(pickAtlas, pickPool,
                                                 instancing::Mode::Live)));

    // the shutter: the warp beams on left-to-right. There is no wipe verb, and
    // scaleX on the ground itself would squash the bands rather than reveal
    // them, so the reveal is a retreating card-coloured blind laid over it.
    panel.child(
        at(0, 0, kClothW, kClothH)
            .fill(kWell)
            .transformOrigin(1.0f, 0.5f)
            .scaleX(
                bind(&loom).source(0.0f, kBeamEnd).invert().clamp(0.0f, 1.0f)));

    // the five cloths. Pixel-identical to the woven picks at palette 0, so
    // the hand-off at the end of the weaving beat is invisible; then each
    // family fades over the last, and Modern comes back for the hold.
    struct Turn {
      int pal;
      float a, b;
    };
    const Turn turns[] = {{0, 0.5050f, kWeaveEnd}, {1, 0.6125f, 0.6375f},
                          {2, 0.6625f, 0.6875f},   {3, 0.7125f, 0.7375f},
                          {4, 0.7625f, 0.7875f},   {0, 0.8125f, kTurnEnd}};
    for (const Turn& t : turns)
      panel.child(at(0, 0, kClothW, kClothH)
                      .fill(clothMat[(size_t)t.pal])
                      .opacity(bind(&loom).source(t.a, t.b).clamp(0.0f, 1.0f)));

    // the surface, above the weave: rib shadow, then yarn tooth. These are
    // siblings rather than decorations, and the reason is narrow.
    // Element::overlay() paints UNDER children; foreground(), which is above
    // them, takes a Decoration, and none of the decoration primitives means
    // "flood the outline with this Material through this blend mode". A raw
    // PaintProgram would do it, but a node carrying one never prunes, whereas
    // a sibling element with .blend() does. Both grain layers carry
    // Cache::Texture, because a static SkSL Material caches its SHADER and not
    // its PIXELS — without the bake, this full-canvas fractal is re-evaluated
    // per pixel per frame, and it is by far the most expensive thing here.
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(gridMat)
                    .blend(SkBlendMode::kMultiply)
                    .opacity(0.9f));
    panel.child(at(0, 0, kClothW, kClothH)
                    .fill(yarnGrain)
                    .blend(SkBlendMode::kOverlay)
                    .cache(Cache::Texture)
                    .opacity(0.14f));

    // the mirror axes, flashed once while the arithmetic proves itself
    for (int rep = 0; rep < 2; ++rep)
      for (int m : v.mirrors) {
        const float x = (float)(m + rep * v.total) * kPx;
        if (x > kClothW) continue;
        panel.child(at(x - 0.5f, 0, 1, kClothH)
                        .fill(kRed)
                        .opacity(bind(&loom)
                                     .source(kWeaveEnd, kProveEnd)
                                     .map(plateau(0.25f))
                                     .scale(0.8f)));
      }

    // the fell line: the shuttle's current pick, riding the leading edge
    panel.child(at(0, 0, kClothW, 2)
                    .fill(kRed)
                    .translateY(bind(&loom)
                                    .source(kBeamEnd, kWeaveEnd)
                                    .target(0.0f, kClothH)
                                    .clamp(0.0f, kClothH))
                    .opacity(bind(&loom)
                                 .source(kBeamEnd - 0.01f, kWeaveEnd + 0.01f)
                                 .map(plateau(0.03f))));
    return panel;
  }

  // =========================================================================
  // The sett bar: the thread count as the one-dimensional object it is,
  // warp-aligned with the cloth under it.

  Element theSettBar() {
    Element g = box();
    g.child(at(kClothX, kBarY, kClothW, kBarH)
                .fill(warpMat)
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Outer)));

    // run numerals, staggered over two rows so a 4 px band still gets one
    for (size_t r = 0; r < bwRuns.size(); ++r) {
      const float cx =
          kClothX +
          ((float)runStart[r] + (float)bwRuns[r].threads * 0.5f) * kPx;
      g.child(
          centred(std::to_string(bwRuns[r].threads), cx - 14,
                  kBarY + kBarH + 3 + (r % 2 ? 10.0f : 0.0f), 28)
              .font({.size = 8, .color = r % 2 ? kInk2 : kInk, .track = 0.4f}));
    }

    // the two pivots, snapping in when the arithmetic proves
    for (int rep = 0; rep < 2; ++rep)
      for (size_t i = 0; i < v.mirrors.size(); ++i) {
        const float x = kClothX + (float)(v.mirrors[i] + rep * v.total) * kPx;
        if (x > kClothX + kClothW) continue;
        g.child(at(x - 6, kBarY - 11, 12, 10)
                    .shape(shapes::polygon(3, 180))
                    .fill(kRed)
                    .transformOrigin(0.5f, 1.0f)
                    .scale(bind(&loom)
                               .source(kWeaveEnd, kWeaveEnd + 0.035f)
                               .map(backOut())));
        if (rep == 0)
          g.child(
              centred(i == 0 ? "PIVOT  B/9" : "PIVOT  B/3", x - 45, kBarY - 26,
                      90)
                  .font({.size = 8, .color = kRed, .track = 0.8f})
                  .opacity(bind(&loom)
                               .source(kWeaveEnd + 0.01f, kWeaveEnd + 0.045f)
                               .clamp(0.0f, 1.0f)));
      }

    // the register's own phrase, out of the way of the pivot flags
    g.child(label("“THE DNA OF A TARTAN.”   — SCOTTISH REGISTER OF TARTANS, "
                  "ON THE THREADCOUNT",
                  660, kBarY - 26, 420)
                .font({.size = 8.5f, .color = kRed, .track = 0.4f}));

    // the count itself, set as one mono run
    std::string count;
    static const char kCode[] = "KBGYW";
    for (const auto& run : bwRuns)
      count += kit::formatted("%c%d ", kCode[run.shade], run.threads);
    g.child(label(count, kClothX, kBarY + kBarH + 27, kClothW)
                .font({.size = 11.5f, .color = kInk, .track = 0.3f}));
    g.child(label("HALF-SETT, REGISTER NOTATION:  B/9 K2 B6 K2 B6 K18 G18 K6 "
                  "G18 K18 B18 K2 B/3   =  126  =  252 / 2",
                  kClothX, kBarY + kBarH + 47, kClothW)
                .font({.size = 8.5f, .track = 0.5f}));
    return g;
  }

  // =========================================================================
  // The weaver's draft: threading across the top, tie-up in the corner,
  // treadling down the right, drawdown filling the body. Four centuries old,
  // and still the clearest notation anyone has for a stored program.

  Element theDraft() {
    const float c = kDrawCell;
    const float x0 = kColX, y0 = 172;
    const float bodyY = y0 + 4 * c + 20;     // 228
    const float tieX = x0 + kDrawN * c + 8;  // 1400
    Element g = box();

    g.child(label("THE DRAFT  ·  4 SHAFTS, STRAIGHT DRAW, 2/2 BALANCED TWILL, "
                  "TROMP AS WRIT",
                  kColX, 140, kColW + 40)
                .styleClass("heading"));

    auto cell = [&](float x, float y, bool on) {
      return at(x, y, c, c)
          .fill(on ? kInk : kCard)
          .foreground(
              stroke(0.6f, Fill::color(kRule), PathFormat::Align::Inner));
    };

    // threading — shaft (j mod 4), shaft 1 at the top
    for (int j = 0; j < kDrawN; ++j)
      for (int s = 0; s < 4; ++s)
        g.child(cell(x0 + (float)j * c, y0 + (float)s * c,
                     ((kDrawOrigin + j) % 4) == s));
    // tie-up — treadle t lifts shafts t and t+1
    for (int t = 0; t < 4; ++t)
      for (int s = 0; s < 4; ++s)
        g.child(cell(tieX + (float)t * c, y0 + (float)s * c,
                     s == t || s == (t + 1) % 4));
    // treadling — as-drawn-in, and it highlights in sync with the fell line
    for (int i = 0; i < kDrawN; ++i) {
      const int t = (kDrawOrigin + i) % 4;
      for (int tt = 0; tt < 4; ++tt)
        g.child(cell(tieX + (float)tt * c, bodyY + (float)i * c, tt == t));
      const float a =
          kBeamEnd + (kWeaveEnd - kBeamEnd) * (float)i / (float)kDrawN;
      const float b =
          kBeamEnd + (kWeaveEnd - kBeamEnd) * (float)(i + 1) / (float)kDrawN;
      g.child(at(tieX - 3, bodyY + (float)i * c, 4 * c + 6, c)
                  .fill(hexColor(0x9A3324, 0.30f))
                  .opacity(bind(&loom).source(a, b).map(plateau(0.35f))));
    }
    // drawdown — the cloth itself, at kDrawCell px per thread, kNearest
    g.child(
        at(x0, bodyY, (float)kDrawN * c, (float)kDrawN * c)
            .child(image(drawdownAsset)
                       .inset(0)
                       .sampling(SkSamplingOptions(SkFilterMode::kNearest)))
            .child(
                at(0, 0, (float)kDrawN * c, (float)kDrawN * c).fill(drawGrid))
            .foreground(
                stroke(1, Fill::color(kInk), PathFormat::Align::Outer)));

    g.child(label(kit::formatted(
                      "ENDS %d-%d OF THE SETT (K2 B6 K18 G6), SQUARED AGAINST "
                      "THEMSELVES  ·  %d PX / THREAD",
                      kDrawOrigin + 1, kDrawOrigin + kDrawN, (int)c),
                  x0, bodyY + (float)kDrawN * c + 6, kColW + 40)
                .font({.size = 8, .track = 0.4f}));
    // shaft numbers down the left of the threading block
    for (int s = 0; s < 4; ++s)
      g.child(centred(std::to_string(s + 1), x0 - 15, y0 + (float)s * c - 1, 13)
                  .font({.size = 7}));
    g.child(centred("THREADING", x0 + 60, y0 - 11, 120).styleClass("tag"));
    g.child(centred("TIE-UP", tieX - 8, y0 - 11, 60).styleClass("tag"));
    g.child(centred("TREADLING", tieX - 14, bodyY - 12, 72).styleClass("tag"));
    g.child(centred("DRAWDOWN", x0 + 60, bodyY - 12, 120).styleClass("tag"));
    return g;
  }

  // =========================================================================

  Element theBlendTable() {
    const float y0 = 584, cell = 46, gap = 6, gx = kColX + 20;
    Element g = box();
    g.child(
        label("THE THIRD COLOURS  ·  WARP ACROSS, WEFT DOWN", kColX, 556, kColW)
            .styleClass("heading"));
    static const char* kName[3] = {"K", "B", "G"};
    const SkSize module{cell, cell};
    const SkSize gaps{gap, gap};
    for (int i = 0; i < 3; ++i) {
      const SkRect head = arrange::cellRect({i, i}, module, gaps, {gx, y0});
      g.child(centred(kName[i], head.fLeft, y0 - 14, cell)
                  .font({.size = 9, .track = 0.8f}));
      g.child(centred(kName[i], kColX, head.fTop + cell / 2 - 7, 16)
                  .font({.size = 9, .track = 0.8f}));
    }
    for (int wf = 0; wf < 3; ++wf)
      for (int wp = 0; wp < 3; ++wp) {
        const SkRect box = arrange::cellRect({wp, wf}, module, gaps, {gx, y0});
        g.child(at(box.fLeft, box.fTop, cell, cell)
                    .fill(blendMat[(size_t)wf * 3 + (size_t)wp])
                    .foreground(stroke(1, Fill::color(wp == wf ? kRule : kInk),
                                       PathFormat::Align::Outer)));
      }
    const float tx =
        arrange::cellRect({3, 0}, module, gaps, {gx, y0}).fLeft + 12;
    g.child(label("EACH CELL IS 6 × 6 THREADS AT 8 PX.", tx, y0 - 2, 200)
                .font({.size = 8, .track = 0.3f}));
    g.child(label("THE BLENDS ARE WOVEN, NOT MIXED.", tx, y0 + 10, 200)
                .font({.size = 8, .track = 0.3f}));
    g.child(label(kit::formatted(
                      "n = %d  →  %d solid + %d blend  =  %d  =  n(n+1)/2",
                      v.solids, v.solids, v.blends, v.perceived),
                  tx, y0 + 30, 210)
                .font({.size = 8.5f, .color = kRed, .track = 0.2f}));
    g.child(label("A tartan has six colours from\nthree threads because the "
                  "blend\nis spatial: at 42 ends per inch\nthe eye does the "
                  "mixing, not\nthe dyer.",
                  tx, y0 + 52, 200)
                .font({.face = serifIt(), .size = 11}));
    return g;
  }

  // =========================================================================

  Element thePaletteStrip() {
    const float y0 = 790, rowH = 34;
    Element g = box();
    g.child(label("ONE THREAD COUNT, FIVE SHADE CARDS  ·  SCOTTISH REGISTER "
                  "OF TARTANS",
                  kColX, 762, kColW + 40)
                .styleClass("heading"));
    // which family the cloth is wearing, right now
    const float spans[5][2] = {{kWeaveEnd, kProveEnd},
                               {0.6375f, 0.6625f},
                               {0.6875f, 0.7125f},
                               {0.7375f, 0.7625f},
                               {0.7875f, 0.8125f}};
    static const char* code[3] = {"K", "B", "G"};
    for (int r = 0; r < 5; ++r) {
      const float y = y0 + (float)r * rowH;
      const Palette& p = kPalettes[(size_t)r];
      g.child(label(p.name, kColX + 10, y + 1, 140)
                  .font({.size = 7.5f, .color = kInk, .track = 0.4f}));
      const uint32_t shade[3] = {p.k, p.b, p.g};
      for (int i = 0; i < 3; ++i) {
        const float x = kColX + 150 + (float)i * 94;
        g.child(at(x, y, 86, 14).fill(hexColor(shade[i])));
        g.child(
            label(kit::formatted("%s #%06X", code[i], shade[i]), x, y + 16, 86)
                .font({.size = 7, .track = 0.2f}));
      }
      auto mark = [&](float a, float b) {
        return at(kColX, y - 2, 5, 22)
            .fill(kRed)
            .opacity(bind(&loom).source(a, b).map(plateau(0.12f)));
      };
      g.child(mark(spans[r][0] - 0.012f, spans[r][1] + 0.012f));
      if (r == 0)
        g.child(mark(0.8125f, 1.03f));  // and back to Modern for the hold
    }
    return g;
  }

  // =========================================================================
  // The argument. Four identical cloths under four clan names, and then the
  // cloth Wilsons actually sold as Campbell of Argyll: the same sett with two
  // centre-lines recoloured.

  Element theProvenance() {
    const float y0 = 1052, sw = 180, sh = 110, gap = 16;
    static const char* kNames[4] = {"Campbell Argyll", "Grant", "Munro",
                                    "Sutherland"};
    Element g = box();
    g.child(label("THE COCKBURN COLLECTION, 1810–15  ·  ONE CLOTH, FOUR "
                  "LABELS",
                  kClothX, 1030, 700)
                .styleClass("heading"));
    for (int i = 0; i < 4; ++i) {
      const float x =
          arrange::cellRect({i, 0}, {sw, sh}, {gap, 0}, {kClothX, 0}).fLeft;
      // the SAME crop of the SAME cloth, four times over
      g.child(at(x, y0, sw, sh)
                  .clip(true)
                  .background(
                      styles::dropShadow(hexColor(0x3E3A33, 0.45f), {2, 3}, 7))
                  .fill(swatchMat)
                  .foreground(
                      stroke(1, Fill::color(kRule), PathFormat::Align::Outer))
                  .child(at(0, 0, sw, sh)
                             .fill(gridMat)
                             .blend(SkBlendMode::kMultiply)
                             .opacity(0.85f)));
      g.child(centred(kNames[i], x, y0 + sh + 6, sw)
                  .styleClass("name")
                  .ink(kInk)
                  .opacity(bind(&loom)
                               .source(0.63f + (float)i * 0.022f,
                                       0.66f + (float)i * 0.022f)
                               .clamp(0.0f, 1.0f)));
    }
    g.child(label("“The Cockburn Collection (1810-15) includes four specimens "
                  "of the Government tartan labelled;",
                  kClothX, y0 + sh + 28, 800)
                .styleClass("quote"));
    g.child(label("‘Campbell Argyll’, ‘Grant’, ‘Munro’ and ‘Sutherland’.”   "
                  "— Scottish Register of Tartans, registration note, SRT 277",
                  kClothX, y0 + sh + 43, 800)
                .styleClass("quote"));

    // ...and the cloth that carries one of those names honestly
    const float ax =
        arrange::cellRect({4, 0}, {sw, sh}, {gap, 0}, {kClothX, 0}).fLeft + 12;
    g.child(at(ax, y0, sw, sh)
                .clip(true)
                .background(
                    styles::dropShadow(hexColor(0x3E3A33, 0.45f), {2, 3}, 7))
                .fill(argyllMat)
                .foreground(
                    stroke(1.5f, Fill::color(kRed), PathFormat::Align::Outer))
                .child(at(0, 0, sw, sh)
                           .fill(gridMat)
                           .blend(SkBlendMode::kMultiply)
                           .opacity(0.85f)));
    g.child(centred("Campbell of Argyll", ax, y0 + sh + 6, sw)
                .styleClass("name")
                .ink(kRed));
    g.child(label("416 ends: the Government sett", ax, y0 + sh + 28, sw + 30)
                .styleClass("note"));
    g.child(label("with its two black centre-lines", ax, y0 + sh + 39, sw + 30)
                .styleClass("note"));
    g.child(label("recoloured YELLOW and WHITE.", ax, y0 + sh + 50, sw + 30)
                .styleClass("note")
                .ink(kRed));
    return g;
  }

  // =========================================================================
  // Structure against numbers: the two setts, each normalised to its own
  // total, unit for unit. They agree to under one percent — a tartan's
  // identity is its structure; its numbers are a matter of authority.

  Element theComparison() {
    const float y0 = 1276, barW = 880, barH = 26, x0 = kClothX + 140;
    Element g = box();
    g.child(label("THE SAME SETT, TWICE  ·  EACH NORMALISED TO ITS OWN TOTAL, "
                  "UNIT FOR UNIT",
                  kClothX, 1242, 900)
                .styleClass("heading"));

    struct Bar {
      const char* name;
      const std::vector<Run>* runs;
      int total;
      float y;
    };
    const Bar bars[2] = {
        {"BLACK WATCH        252", &bwRuns, v.total, y0},
        {"CAMPBELL ARGYLL   416", &caRuns, v.argyllTotal, y0 + barH + 8}};
    const Shades modern = shadesOf(kPalettes[0]);
    for (const Bar& b : bars) {
      g.child(label(b.name, kClothX, b.y + 8, 140)
                  .font({.size = 8.5f, .color = kInk, .track = 0.4f}));
      float cur = 0;
      for (const Run& r : *b.runs) {
        const float w = barW * (float)r.threads / (float)b.total;
        Element seg = at(x0 + cur, b.y, w, barH).fill(modern[(size_t)r.shade]);
        if (r.shade == Y || r.shade == W)
          seg.foreground(
              stroke(1.5f, Fill::color(kRed), PathFormat::Align::Outer));
        g.child(std::move(seg));
        cur += w;
      }
      g.child(at(x0, b.y, barW, barH)
                  .foreground(
                      stroke(1, Fill::color(kRule), PathFormat::Align::Outer)));
    }
    // the unit boundaries, dropped through both bars
    float cum = 0;
    const int units[4] = {v.unitA, v.unitB, v.unitC, v.unitB};
    static const char* uname[4] = {"A", "B", "C", "B"};
    for (int u = 0; u < 4; ++u) {
      const float cx =
          x0 + barW * (cum + (float)units[u] * 0.5f) / (float)v.total;
      g.child(centred(uname[u], cx - 12, y0 - 15, 24)
                  .font({.size = 8.5f, .color = kRed, .track = 0.6f}));
      cum += (float)units[u];
      if (u < 3)
        g.child(at(x0 + barW * cum / (float)v.total - 0.5f, y0 - 3, 1,
                   2 * barH + 14)
                    .fill(hexColor(0x9A3324, 0.8f)));
    }
    g.child(
        label(kit::formatted("UNIT FRACTIONS AGREE TO %.2f %%  ·  IDENTICAL "
                             "STRUCTURE, RUN FOR RUN  ·  DIFFERENT NUMBERS  ·  "
                             "THE OVERCHECKS ARE RINGED",
                             v.unitDrift * 100.0f),
              x0, y0 + 2 * barH + 14, 900)
            .font({.size = 8.5f, .color = kRed, .track = 0.3f}));
    return g;
  }

  // =========================================================================

  Element theVerification() {
    const float x0 = 1060, y0 = 1052, lh = 13.6f;
    const size_t rows = verdict.rows.size();
    Element g = box();
    g.child(label("VERIFIED AT STARTUP, NOT ASSERTED", x0, 1030, kColW)
                .styleClass("heading"));
    g.child(at(x0 - 12, y0 - 8, 472, (float)rows * lh + 14)
                .fill(hexColor(0xDCD4C4, 0.8f))
                .foreground(
                    stroke(1, Fill::color(kRule), PathFormat::Align::Inner)));
    // The words are the run's own — the label it was made under, the figure
    // it came to, and the verdict computed from the two. The mark before
    // each row carries that verdict as colour, so a row that failed is
    // legible before it is read. A finding that fails is the two setts'
    // agreement drifting rather than a defect here, and it is marked in the
    // same red because the run counts it apart, not the card.
    std::vector<sketch::kit::Row> lines;
    lines.reserve(rows);
    for (const measure::Check& c : verdict.rows) {
      std::string verdictWord;
      if (c.judged()) verdictWord = c.pass ? "PASS" : "FAIL want " + c.expected;
      lines.push_back({.cells = {U(c.label), U(c.actual), U(verdictWord)},
                       .swatch = Fill::color(
                           !c.judged() ? kRule : (c.pass ? kInk : kRed))});
    }
    g.child(at(x0, y0, 450, (float)rows * lh)
                .opacity(bind(&loom)
                             .source(kWeaveEnd,
                                     kWeaveEnd + (float)rows * 0.0092f + 0.011f)
                             .clamp(0.0f, 1.0f))
                .child(sketch::kit::table(
                    std::move(lines), {.columns = {{322}, {58, true}, {}}})));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    // The card's sheet and its classes stand for everything described below
    // it, so a kit component four levels down is set in the card's ink
    // without being handed it; the root states the voice every line inherits.
    const sketch::kit::Provide look(sheet(), classes());
    Element root = stack()
                       .width(Dimension(kCanvasW))
                       .height(Dimension(kCanvasH))
                       .font({.face = mono()})
                       .ink(kInk2);

    // the board: one recipe, paint and tooth together
    root.child(
        at(0, 0, kCanvasW, kCanvasH).fill(boardMat).cache(Cache::Texture));
    root.child(at(24, 24, kCanvasW - 48, kCanvasH - 48)
                   .foreground(stroke(1, Fill::color(kRule),
                                      PathFormat::Align::Inner)));

    // 1. the heading
    root.child(
        label("BLACK WATCH (GOVERNMENT)", kClothX, 46, 900)
            .font({.face = sansB(), .size = 34, .color = kInk, .track = 4.6f}));
    root.child(label("STA 207  ·  STWR 207  ·  SRT 277  ·  TARTAN DATE "
                     "01/01/1739  ·  CATEGORY MILITARY  ·  DESIGNER UNKNOWN",
                     kClothX, 96, 1200)
                   .font({.size = 10.5f, .track = 1.1f}));
    root.child(rule(kClothX, 122, kCanvasW - 2 * kClothX, 1, kRule));

    // the specimen ticket — the physical facts, in the header's dead corner
    {
      static const char* spec[] = {
          "WORSTED WOOL  ·  2/2 BALANCED TWILL  ·  STRAIGHT DRAW",
          "252 ENDS AT 42 EPI  =  6.00 IN SETT  =  152.4 MM",
          "KILTING CLOTH 10 / 13 / 16 OZ  ·  REGIMENTAL WORSTED ~21 OZ",
          "SAMPLE: SCOTTISH TARTANS AUTHORITY, SCARLETT COLLECTION",
          "OLDEST DATED: COCKBURN COLLECTION, MITCHELL LIBRARY, GLASGOW",
      };
      for (int i = 0; i < 5; ++i)
        root.child(label(spec[i], kColX, 48 + (float)i * 12.5f, kColW + 40)
                       .font({.size = 7.5f, .track = 0.35f}));
      root.child(rule(kColX - 14, 46, 1, 62, kRule));
    }

    root.child(theSettBar());
    root.child(theCloth());
    root.child(theDraft());
    root.child(theBlendTable());
    root.child(thePaletteStrip());
    root.child(theProvenance());
    root.child(theVerification());
    root.child(theComparison());

    // the justified paragraph, at a real 320 px measure
    {
      weave::ParagraphLayoutOptions o;
      o.alignment = weave::TextAlignment::kJustify;
      o.lineBreakStrategy = weave::LineBreakStrategy::kKnuthPlass;
      o.hyphenation.enabled = true;
      o.hyphenation.penalty = 45.0f;
      o.justification.spaceStretch = 0.55f;
      o.justification.spaceShrink = 0.30f;
      o.justification.lastLineAlignment = weave::TextAlignment::kStart;
      o.knuthPlass.tolerance = 6000.0f;
      o.lineMetrics.height = 16.0f;
      root.child(at(kColX, 1246, 320, 150)
                     .child(text(quote, o).width(Dimension(320))));
    }

    root.child(rule(kClothX, 1408, kCanvasW - 2 * kClothX, 1, kRule));
    root.child(
        label("SETT AFTER H. C. DOUGLAS, SCOTCH TARTAN SETTS, SHUTTLE-CRAFT "
              "GUILD, 1949  ·  252 ENDS  ·  2/2 TWILL, STRAIGHT DRAW  ·  "
              "PALETTE: SCOTTISH REGISTER OF TARTANS COLOUR SHADES  ·  63,504 "
              "CELLS, NONE AUTHORED",
              kClothX, 1414, kCanvasW - 2 * kClothX)
            .font({.size = 8.5f, .track = 0.7f}));
    return root;
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    build();
    // The still belongs to the MODERN hold, and has to be declared, because
    // an undeclared capture lands mid-cycle. The loop weaves, proves, then
    // turns the five shade families over one another (see `turns`), so most
    // of its 8 s shows a cloth that is a correct Black Watch in some OTHER
    // registered palette — brown and olive under a title reading GOVERNMENT,
    // which reads as a broken blend layer and is not one. 7.2 s is loom 0.90,
    // inside the final Modern hold (0.85 -> 1.0).
    sketch::kit::stage(ctx, {.size = SkSize::Make(kCanvasW, kCanvasH),
                             .captureAt = 7.2,
                             .background = kCard});
    ctx.ticker.add([this](double dt) {
      clock += dt;
      loom = (float)(std::fmod(clock, (double)kCycle) / (double)kCycle);
      // THE BEAT-IN, one float per pick: pick i comes up over its own
      // 0.0035 of the cycle, the same window each of the 378 leaves used to
      // hold its own binding for.
      const float span = kWeaveEnd - kBeamEnd;
      auto alpha = pickPool->alphas();
      for (int i = 0; i < kPicks; ++i) {
        const float w0 = kBeamEnd + span * ((float)i / (float)kPicks);
        alpha[(size_t)i] =
            std::clamp((loom.value() - w0) / 0.0035f, 0.0f, 1.0f);
      }
      return true;
    });
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    BlackWatch, "Study \xc2\xb7 Pattern",
    "The Government sett \xe2\x80\x94 24 integers and a mod-4 rule, 63,504 "
    "emergent cells")
