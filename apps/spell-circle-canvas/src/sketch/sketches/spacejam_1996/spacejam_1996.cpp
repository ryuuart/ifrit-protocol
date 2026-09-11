// The Space Jam website assembled from illustrated navigation and a table
// layout.

#include "Artwork.h"

struct SpaceJam1996 : sketch::Sketch {
  using Ix = sj::Ix;

  // The load simulator's state: one Output per asset, in [0,1].
  ch::Output<float> got[sj::kAssetCount];
  double gotBytes[sj::kAssetCount] = {};
  double sinceDone = 0.0;
  uint32_t arrivedMask = 0;
  bool needRender = true;

  /** THE LAYOUT'S VERDICT against the browser's own numbers. Every row is
   *  COMPUTED from the two it reports, so a page that stops resolving the
   *  grid cannot keep claiming it does. */
  measure::Table verdict;

  // Everything the browser would have cached as a decoded GIF: each nav
  // image baked ONCE from its element tree via snapshot(), then replayed
  // under a hard scanline clip. Baking is what makes the reveal affordable —
  // sixteen assets, each redrawn on every frame it is still arriving — and
  // the clip is hand-written because a picture has no directional-wipe
  // control of its own.
  sk_sp<SkPicture> pic[sj::kAssetCount];
  float artW[sj::kAssetCount] = {};
  float artH[sj::kAssetCount] = {};

  Pattern stars;
  mskia::Paint starsMat;
  /** THE LIVE BALL'S MATERIAL, built once and held. Its shader steps its
   *  own uTime at the GIF's frame rate, so the value is the same one every
   *  describe — and describe runs again on every arrival. */
  mskia::Paint fastballMat;
  // <TABLE WIDTH=500 CELLSPACING=2 CELLPADDING=1>, at this sketch's scale.
  // The columns and rows are the ones the children claim.
  Table table{.columns = 5,
              .rows = 5,
              .width = sj::S(500),
              .spacing = sj::S(2),
              .padding = sj::S(1)};

  // ---- the reveal --------------------------------------------------------
  Element revealed(int i, bool inFlight) const {
    const sk_sp<SkPicture> p = pic[i];
    const float h = artH[i];
    const ch::Output<float>* g = &got[i];
    // ARRIVED IS THE PICTURE ITSELF. A recorded picture's identity is its
    // own, so the leaf compares equal between describes and the node goes
    // static; the program below exists only for the hard scanline edge of
    // a partial image, which nothing in the picture can express.
    if (!inFlight) return picture(p, SkSize::Make(artW[i], artH[i]));
    // KEYLESS: the scanline edge is read off the arrival's live fraction.
    Element e = custom([p, h, g](SkCanvas& canvas, const PaintContext& ctx) {
                  const float frac = g->value();
                  if (frac <= 0.0f || !p) return;
                  // No interlacing on any of the sixteen (every image
                  // descriptor's flag is zero), so this is the older, simpler
                  // behaviour: a complete top band and nothing below, the
                  // edge hard and landing on a 1996 scanline.
                  const float rows =
                      std::floor(h / sj::kScale * frac) * sj::kScale;
                  if (rows <= 0.0f) return;
                  canvas.save();
                  canvas.clipRect(SkRect::MakeWH(ctx.size.width(), rows));
                  canvas.drawPicture(p.get());
                  canvas.restore();
                })
                    .width(Dim(artW[i]))
                    .height(Dim(artH[i]));
    if (inFlight) e.cache(Cache::None);
    return e;
  }

  /** A table cell: the `<br>` blocks as an 18 px line box each, then the
   *  image. Its MEASURED size is what the table algorithm reads, so the
   *  br-count reaches the layout the same way it does in a browser. */
  /** One <TD>: the <br> block above the image inside the cell, the image
   *  itself, and the claim on the grid — said on the child, so a cell and
   *  its occupancy cannot drift apart. */
  Element cell(const sj::Slot& s) const {
    const bool inFlight =
        s.asset >= 0 && (arrivedMask & (1u << (unsigned)s.asset)) == 0;
    Element c = box().column().alignSelf(Align::Start).shrink(0);
    if (s.asset < 0) c.width(Dim(0)).height(Dim(0));
    if (s.brs > 0)
      c.child(box().width(Dim(0)).height(Dim(sj::S(18) * (float)s.brs)));
    if (s.asset >= 0) c.child(revealed(s.asset, inFlight));
    c.cells(s.col, s.row, s.colspan, s.rowspan).cellAlign(s.across, s.down);
    return c;
  }

  // ---- the page ----------------------------------------------------------
  Element describe(sketch::SketchContext& ctx) {
    using namespace sj;

    // 1. the starfield, genuinely full bleed: the body background paints the
    //    whole viewport including under the 8 px margin, so stars run to all
    //    four edges. Black until the tile's LAST byte lands, then the whole
    //    lattice at once — a tiled background cannot wipe (each repeat would
    //    reveal its own band and comb the page every 111 px), so a hard cut
    //    is the defensible model.
    //
    //    Spelled with bind()'s affine chain rather than a second Output:
    //    v*1000 - 999, clamped, is zero for every byte but the last. The
    //    step is the shaping, and it lives at the property instead of in
    //    the tick loop.
    Element field = box()
                        .inset(0)
                        .fill(starsMat)
                        .opacity(motion::bind(&got[kStars])
                                     .scale(1000.0f)
                                     .offset(-999.0f)
                                     .clamp(0.0f, 1.0f))
                        .key("starfield");

    // 2. the Fast Break row — the only left-aligned thing on the page, and
    //    the only thing that moves. Restored from the HTML comment the live
    //    page keeps it in.
    const bool ballIn = (arrivedMask & (1u << kFastbreak)) != 0;
    Element fastRow =
        stack()
            .left(Dim(S(70)))
            .top(Dim(S(86)))
            .width(Dim(S(500)))
            .height(Dim(S(46)))
            .child(revealed(kFast, (arrivedMask & (1u << kFast)) == 0)
                       .left(Dim(S(3)))
                       .top(Dim(S(17.5f))))
            .child(revealed(kBreak, (arrivedMask & (1u << kBreak)) == 0)
                       .left(Dim(S(93)))
                       .top(Dim(S(17.5f))));
    if (ballIn) {
      // Fully arrived: the live element, whose material steps its uTime at
      // 10 Hz — the GIF's own frame rate, six frames, forever.
      fastRow.child(rect(S(53), S(3), S(40), S(40))
                        .shape(shapes::circle())
                        .fill(fastballMat)
                        .key("fastbreak"));
    } else {
      // Still arriving: a partially-downloaded animated GIF shows its first
      // frame and does not animate. Same picture path as everything else.
      fastRow.child(revealed(kFastbreak, true).left(Dim(S(53))).top(Dim(S(3))));
    }

    // 3. the planet table. Nothing below is hand-placed: `Table`
    //    runs the auto-layout rule over the children's measured sizes and
    //    the cells they claim.
    Element grid = layout(table)
                       .left(Dim(S(70)))
                       .top(Dim(S(168)))
                       .width(Dim(S(500)))
                       .height(Dim(S(435)))
                       .key("table");
    for (const Slot& slot : kSlotTable) grid.child(cell(slot));

    // 4. the © line — the ONLY live text on the page. <font size="-1"> is
    //    HTML size 2 of 7 -> 13.33 px computed, hard-wrapped by the author's
    //    own <br> into two centred lines.
    auto small = ty(serif(), S(13.33f), kBodyText);
    Element colophon =
        box()
            .left(Dim(0))
            .top(Dim(S(757)))
            .width(Dim(S(640)))
            .column()
            .alignItems(Align::Center)
            .child(
                text(U("SPACE JAM, characters, names, and all related"), small))
            .child(
                text(U("indicia are trademarks of Warner Bros. \xc2\xa9 1996"),
                     small));

    (void)ctx;
    return stack()
        .child(std::move(field))
        // the ad-slot table: 488x60 of server-side includes that no longer
        // resolve. Left empty on purpose — 60 px of stars, and the reason
        // the page has a bald strip at the top.
        .child(std::move(fastRow))
        .child(std::move(grid))
        .child(std::move(colophon))
        .child(verdict.failures() > 0 ? failureCard() : box());
  }

  // ---- setup -------------------------------------------------------------
  void bakeArt(sketch::SketchContext& ctx) {
    using namespace sj;
    sigil::weave::FontContext& f = *ctx.fonts;
    struct Job {
      int ix;
      Element tree;
      float w, h;
    };
    std::vector<Job> jobs;
    jobs.push_back(
        {kFast, wordmark(f, "FAST", S(50), S(11), true), S(50), S(11)});
    jobs.push_back(
        {kBreak, wordmark(f, "BREAK", S(50), S(11), false), S(50), S(11)});
    jobs.push_back({kFastbreak,
                    rect(0, 0, S(40), S(40))
                        .left(Dim(0))
                        .top(Dim(0))
                        .shape(shapes::circle())
                        .fill(ballMaterial(false, C5(0xFF6B29), C5(0xC64210),
                                           C5(0x521800), 0.050f)),
                    S(40), S(40)});
    jobs.push_back({kPressbox, artPressBox(f), S(131), S(56)});
    jobs.push_back({kJamcentral, artJamCentral(f), S(55), S(67)});
    jobs.push_back({kBball, artBball(f), S(62), S(62)});
    jobs.push_back({kLunartunes, artLunarTunes(f), S(95), S(77)});
    jobs.push_back({kLineup, artLineup(f), S(63), S(52)});
    jobs.push_back({kJamlogo, artLogo(f), S(272), S(165)});
    jobs.push_back({kJump, artJump(f), S(58), S(52)});
    jobs.push_back({kJunior, artJunior(f), S(49), S(57)});
    jobs.push_back({kStudiostore, artStudioStore(f), S(94), S(72)});
    jobs.push_back({kSouvenirs, artSouvenirs(f), S(83), S(83)});
    jobs.push_back({kSitemap, artSitemap(f), S(104), S(67)});
    jobs.push_back({kBehind, artBehind(f), S(67), S(63)});

    for (Job& j : jobs) {
      artW[j.ix] = j.w;
      artH[j.ix] = j.h;
      pic[j.ix] =
          snapshot(box().width(Dim(j.w)).height(Dim(j.h)).clip(true).child(
                       std::move(j.tree)),
                   f, {j.w, j.h});
    }
    artW[kStars] = artH[kStars] = 0;
  }

  /** CLAIM the grid the table resolved against the browser's, once at
   *  startup, so the layout is verified against the reference render
   *  instead of taken on trust. The literals are what headless Chrome
   *  reports for the same page, and every row's verdict is COMPUTED from
   *  the two numbers it carries.
   *
   *  The input is built from the same `kSlotTable` the children are, so
   *  what is claimed is the layout that was drawn. */
  void checkGrid() {
    using namespace sj;
    LayoutInput in;
    in.container = {S(500), S(435)};
    for (const Slot& s : kSlotTable) {
      in.childSizes.push_back(
          s.asset < 0
              ? SkSize{0, 0}
              : SkSize{artW[s.asset], artH[s.asset] + S(18) * (float)s.brs});
      in.childCells.push_back({.column = s.col,
                               .row = s.row,
                               .columns = s.colspan,
                               .rows = s.rowspan,
                               .across = s.across,
                               .down = s.down,
                               .declared = true});
    }
    const Table::Grid grid = table.solve(in);
    verdict = {};
    verdict.add(measure::heading("TABLE-AUTO AGAINST HEADLESS CHROME"));
    // The surplus a table-auto scheme distributes lands on fractional
    // pixels, so the columns agree to a hundredth and the rows — which are
    // whole content heights — agree exactly.
    const double chromeCols[5] = {71.42, 97.70, 122.33, 78.95, 107.59};
    for (size_t i = 0; i < grid.columnWidths.size() && i < 5; ++i)
      verdict.add(measure::check(
          kit::formatted("column %zu content width, page px", i), chromeCols[i],
          (double)grid.columnWidths[i] / kScale, 0.15));
    const double chromeRows[5] = {0, 113, 88, 73, 139};
    for (size_t i = 0; i < grid.rowHeights.size() && i < 5; ++i)
      verdict.add(measure::check(kit::formatted("row %zu height, page px", i),
                                 chromeRows[i],
                                 (double)grid.rowHeights[i] / kScale, 0.01));

    // ...and the twelve images, which is what actually has to land. The
    // table origin is (70, 168) on the page; each row prints the
    // scheme-placed rect against the headless-Chrome
    // getBoundingClientRect() probe for the same image.
    const float refX[14] = {0,      115.13f, 283.78f, 384.92f, 465.70f,
                            77.20f, 183.41f, 509.00f, 84.20f,  466.20f,
                            0,      155.77f, 259.28f, 382.42f};
    const float refY[14] = {0,       230.50f, 198.00f, 175.00f, 211.00f,
                            328.00f, 292.00f, 328.00f, 400.00f, 420.00f,
                            0,       461.00f, 533.00f, 499.00f};
    const std::vector<SkRect> rects = table.place(in);
    verdict.add(measure::heading("THE TWELVE IMAGES, PLACED"));
    for (size_t i = 0; i < std::size(kSlotTable); ++i) {
      const Slot& s = kSlotTable[i];
      if (s.asset < 0) continue;
      const float px = 70.0f + rects[i].left() / kScale;
      // the <br> block sits above the image inside the cell
      const float py = 168.0f + rects[i].top() / kScale + 18.0f * (float)s.brs;
      const float dx = px - refX[i], dy = py - refY[i];
      // One claim per image on the FARTHER of its two axes: an x that
      // agrees and a y that is thirty pixels out must not average into a
      // verdict that reads well.
      verdict.add(measure::check(
          kit::formatted("%s  at (%.2f, %.2f), px from the browser",
                         kManifest[(size_t)s.asset].name, (double)px,
                         (double)py),
          0.0, (double)std::max(std::abs(dx), std::abs(dy)), 0.15));
    }
  }

  /** THE CLAIMS THAT DID NOT HOLD, painted over the page — and only when
   *  there are any. The page is the artefact and carries no drafting
   *  chrome, so a layout that agrees with the browser shows the page and
   *  nothing else. */
  Element failureCard() const {
    using namespace sj;
    sketch::kit::Theme look;
    look.palette.ash = C5(0xFFFFFF);
    look.palette.figure = C5(0xFFFF00);
    look.type.captionNote = {S(7.5f), 0.1f};
    look.type.captionLabel = {S(7.5f), 0.1f, true};
    look.spacing.rowGap = S(3);
    std::vector<sketch::kit::Row> rows;
    for (const measure::Check& c : verdict.rows) {
      if (!c.judged() || c.pass) continue;
      rows.push_back(
          {{toU8(c.label), toU8(c.actual), toU8("want " + c.expected)},
           Fill::color(C5(0xFF0000))});
    }
    sketch::kit::Provide bound(look);
    return box()
        .left(S(40))
        .top(S(120))
        .width(S(560))
        .height(S(30) + S(13) * (float)rows.size())
        .fill(Fill::color(C5(0x000080)))
        .foreground(
            stroke(S(2), Fill::color(C5(0xFF0000)), PathFormat::Align::Inner))
        .column()
        .padding(S(12))
        .gap(S(8))
        .child(text(
            toU8("THE TABLE DOES NOT RESOLVE THE BROWSER'S GRID"),
            weave::textStyle(
                {.face = display(), .size = S(11), .color = C5(0xFFFF00)})))
        .child(sketch::kit::table(std::move(rows),
                                  {.columns = {{S(230)}, {S(46), true}, {}},
                                   .gap = S(6),
                                   .swatchSide = S(5)}));
  }

  void setup(sketch::SketchContext& ctx) override {
    using namespace sj;
    // <body bgcolor="#000000">, literally
    // The still is taken mid-hold: the load finishes around 7.96 s of sketch
    // time and the reload wipes the page around 11.46 s, so 9.5 s is the one
    // window where every asset is present. Anything earlier catches the page
    // mid-load and misses the logotype, which is dead last in the byte
    // schedule — and the reference this study is diffed against is the
    // FINISHED page.
    sketch::kit::stage(ctx, {.size = SkSize::Make(S(640), S(800)),
                             .captureAt = 9.5,
                             .background = kPageBlack,
                             .nonlinearPicture = true});

    bakeArt(ctx);
    checkGrid();

    stars = Pattern::tile({S(111), S(111)}, starTile());
    starsMat = stars.material(*ctx.fonts);
    fastballMat =
        ballMaterial(true, C5(0xFF6B29), C5(0xC64210), C5(0x521800), 0.050f);

    // The 216-colour round, over the finished frame. It is a property of
    // the SCREEN, not of the artwork — which is exactly why it lives here
    // and the RGB555 snap lives in the materials.
    ctx.composer.setView(mskia::Effect::shader(viewEffect()));

    for (int i = 0; i < kAssetCount; ++i) {
      gotBytes[i] = 0;
      got[i] = 0.0f;
    }
    arrivedMask = 0;
    sinceDone = 0.0;

    // The transport. Fixed 120 Hz so the schedule is identical whatever the
    // host draws at (and whatever --fps a capture pre-rolls with).
    ctx.ticker.addFixed(
        120.0,
        [this] {
          stepLoad(1.0 / 120.0);
          return true;
        },
        16);

    ctx.composer.render(describe(ctx));
    needRender = false;
  }

  void stepLoad(double dt) {
    using namespace sj;
    const auto& m = kManifest;
    dt *= kSpeedup;

    uint32_t done = 0;
    int active[kSlots];
    int nActive = 0;
    for (int i = 0; i < kAssetCount; ++i) {
      if (gotBytes[i] >= (double)m[(size_t)i].bytes) {
        done |= 1u << (unsigned)i;
        continue;
      }
      if (nActive < kSlots) active[nActive++] = i;
    }
    if (nActive == 0) {
      sinceDone += dt / kSpeedup;  // back to sketch seconds
      // Idle time since the last byte. The subtrahend is the load's own
      // duration in sketch seconds, so a cycle — load plus hold — comes to
      // kReloadAt. Every counter goes back to zero, so the whole page
      // re-downloads: a cold reload, deliberately — a 1996 Reload
      // re-requested every asset, so everything blanks and re-arrives,
      // the spinning ball included. Do not add a warm cache here.
      if (sinceDone > kReloadAt - 8.0) {
        for (int i = 0; i < kAssetCount; ++i) {
          gotBytes[i] = 0;
          got[i] = 0.0f;
        }
        sinceDone = 0.0;
      }
    } else {
      const double share = kBandwidth * dt / (double)nActive;
      for (int k = 0; k < nActive; ++k) {
        const int i = active[k];
        gotBytes[i] = std::min((double)m[(size_t)i].bytes, gotBytes[i] + share);
        got[i] = (float)(gotBytes[i] / (double)m[(size_t)i].bytes);
      }
    }
    if (done != arrivedMask) {
      arrivedMask = done;
      needRender = true;
    }
  }

  void update(double, sketch::SketchContext& ctx) override {
    if (!needRender) return;
    needRender = false;
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(SpaceJam1996, "Study \xc2\xb7 Screens",
             "spacejam.com, still live \xe2\x80\x94 the page set by Table, "
             "each <TD> naming its own cells")
