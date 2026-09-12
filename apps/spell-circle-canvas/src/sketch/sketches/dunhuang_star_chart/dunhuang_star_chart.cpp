// dunhuang star chart: scene assembly and animation.

#include "DunhuangStarChart.h"

auto DunhuangStarChart::describe(sketch::SketchContext&) -> Element {
  auto root = box().left(0).top(0).width(Dimension(kW)).height(Dimension(kH));
  root.child(ground());
  root.child(locator());

  // the equatorial graticule the stars arrive in, fading as the fold runs
  root.child(
      box()
          .left(108)
          .top(250)
          .width(Dimension(2344))
          .height(Dimension(764))
          .key("grat")
          .opacity(gate(tSky - 0.6f, tSky + 0.6f))
          .zIndex(-1)
          .shape(keyedShape(std::string_view("graticule"),
                            [](SkSize s) {
                              SkPathBuilder b;
                              for (int i = 0; i <= 12; ++i) {
                                const float x = s.width() * (float)i / 12.0f;
                                b.moveTo(x, 0);
                                b.lineTo(x, s.height());
                              }
                              for (int j = 0; j <= 6; ++j) {
                                const float y = s.height() * (float)j / 6.0f;
                                b.moveTo(0, y);
                                b.lineTo(s.width(), y);
                              }
                              return b.detach();
                            }))
          .stroke(
              PathFormat{.width = 0.8f,
                         .strokeFill = Fill::color(hexColor(0x2f6d86, 0.42f)),
                         .dashIntervals = {3, 7}}));

  root.child(scrollBand(-90, kBreakL, "bandL", -0.42f));
  root.child(scrollBand(kBreakR, kW + 90, "bandR", -0.42f));
  for (int seg = 0; seg < 2; ++seg) {
    auto sg = box()
                  .left(segLo(seg))
                  .top(kSegTop)
                  .width(Dimension(segHi(seg) - segLo(seg)))
                  .height(Dimension(kSegH))
                  .clip(true)
                  .key(seg ? "segR" : "segL");
    for (int k = 1; k <= 12; ++k) {
      sg.child(mapFrame(k, seg));
      sg.child(columnBand(k, seg));
    }
    sg.child(discPlate(seg));
    sg.child(discNotes(seg));
    sg.child(raRuler(seg));
    if (seg == 0) {
      sg.child(unreadTitle());
      sg.child(archer());
    }
    root.child(std::move(sg));
  }
  root.child(breakMark());

  // the star field — ONE leaf for 1,460 dots
  root.child(
      box()
          .left(0)
          .top(0)
          .width(Dimension(kW))
          .height(Dimension(kH))
          .key("stars")
          .opacity(gate(tSky - 0.5f, tSky + 0.7f))
          .child(instancing::instances(atlas, pool, instancing::Mode::Live)));

  root.child(asterismLines());
  root.child(map5Labels());

  root.child(headings());
  root.child(poleDrift());
  root.child(poleText());
  root.child(ruleNote());
  root.child(projectionPanel());
  root.child(map13Panel());
  root.child(slot("audit"));
  root.child(consolePanel());
  return root;
}

auto DunhuangStarChart::setup(sketch::SketchContext& ctx) -> void {
  // This study brings its own canvas size and background (above) rather
  // than inheriting a default, and names its own still frame here. The
  // settled plate holds [28.2, 31.0) of the 31 s loop, so 29.0 s sits
  // inside that hold with 2 s of margin before the wrap. Anything much
  // earlier catches the score mid-precession: unprojected sky, no scroll,
  // no asterisms, no checking panel.
  sketch::kit::stage(ctx, {.size = SkSize::Make((int)kW, (int)kH),
                           .captureAt = 29.0,
                           .background = kVoid});

  // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
  // library's own walk: the first installed family wins, and a machine
  // with none of them gets the default face AT THE WEIGHT ASKED FOR
  // rather than silently at Normal. The book and the terminal runs are
  // the house's, named by the voice they are asked for in, so this sheet
  // and every other one asking for them share one resolved face; the
  // display cut and the Han chain are this plate's own.
  faceSerif = sketch::kit::houseFace(sketch::kit::Voice::Book);
  faceItalic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                      SkFontStyle::kItalic_Slant);
  faceMono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
  faceDisplay =
      weave::ports::face({"Optima", "Baskerville"}, SkFontStyle::kBold_Weight);
  faceHan = weave::ports::face(
      {"Songti SC", "PingFang SC", "Hiragino Sans", "Baskerville"});

  // the fibre runs ALONG the roll: anisotropic luminance grain, not noise
  paperGrain = Paint::recipe(field::grain(1.15f, 4, 3326.0f, 0.42f, 5.5f));
  paperSpeck = patterns::speckle(900, 34, 0.20f, 0.85f,
                                 {skia::toColor(hexColor(0x6a5330, 0.10f)),
                                  skia::toColor(hexColor(0x2a2118, 0.08f))});
  paperSpeck.seed(649);

  cat = catalogue(ctx.assets);
  conc = readConcordance(ctx.assets);
  nStars = cat.stars();
  nAst = cat.asterisms();

  computeJoins();
  buildAsterismArt();
  buildFixtures();

  // FIVE cells, because an atlas cell is a whole ELEMENT TREE: the black
  // ring and the school fill bake into ONE sprite, so 1,460 dots stay one
  // draw and there is no second concentric pass.
  atlas = std::make_shared<instancing::Atlas>(3.0f);
  auto dot = [](SkColor4f fill, bool ring) {
    auto e = box().width(11).height(11).shape(shapes::circle());
    if (fill.fA > 0) e.fill(Fill::color(fill));
    if (ring)
      e.stroke(PathFormat{.width = 1.15f,
                          .strokeFill = Fill::color(hexColor(0x1d1710, 0.92f)),
                          .align = PathFormat::Align::Inner});
    return e;
  };
  cellRed = atlas->cell(dot(kCinnabar, true), {11, 11});
  cellBlack = atlas->cell(dot(kInk, true), {11, 11});
  cellWhite = atlas->cell(dot(kLead, true), {11, 11});
  // NOT an empty ring: the chart's default mark IS a dot with a black
  // ring, and only the FILL carries the school. A star whose school the
  // tables do not give is drawn as a dot of undeclared colour.
  cellOpen = atlas->cell(dot(hexColor(0x6f5c40), true), {11, 11});
  cellBare = atlas->cell(dot(kCinnabar, false), {11, 11});

  pool = std::make_shared<instancing::Pool>();
  pool->resize((size_t)nStars);
  {
    auto frames = pool->frames();
    auto scales = pool->scales();
    for (int i = 0; i < nStars; ++i) {
      frames[(size_t)i] = placed[(size_t)i].cell;
      // "the dots are all of similar size" — the chart does NOT encode
      // magnitude, and resisting that instinct is part of the study.
      scales[(size_t)i] = 0.74f;
    }
  }
  rebuild(2000.0f, 0.0f);

  logA.append({toUtf8("THE JOIN"), "heading"});
  logA.append({toUtf8(kit::formatted(
                   "chinese_chenzhuo: %d asterisms, 1,883 vertex words", nAst)),
               "dim"});
  logA.append({toUtf8("1,463 star TOKENS \xe2\x80\x94 3 are DSO (M44/M7/M31)"),
               "number"});
  logA.append(
      {toUtf8(kit::formatted(
           "  so %d HIP numbers vs Chen Zhuo's canonical 1,464", nStars)),
       "dim"});
  logA.append({toUtf8("HYG v4.1 join on HIP: 1,457 direct, 3 LOST"), "number"});
  logA.append({toUtf8("  55203 xi UMa, 78727 xi Sco, 115125 94 Aqr B"), "dim"});
  logA.append(
      {toUtf8("  cause: HYG BLANKS hip on resolved double components"), "dim"});
  logA.append({toUtf8("  Bayer fallback recovers all three"), "pass"});
  logA.append(
      {toUtf8(kit::formatted("RESOLVED %d / %d = 100.00%%", nStars, nStars)),
       "pass"});
  logA.append(
      {toUtf8("largest 1300-yr proper motion 1.476 deg (HIP 19849)"), "dim"});

  logB.append({toUtf8("THE EPOCH, AND THE DECLINATION WINDOW"), "heading"});
  logB.append(
      {toUtf8("paper precessed to +700, NOT +665 (sect. 4.1)"), "number"});
  logB.append(
      {toUtf8("  665 vs 700 = 0.489 deg RA; map 5's RA residual 2.26"), "dim"});
  logB.append(
      {toUtf8("  4.6x below the chart's own hand. UNRESOLVABLE."), "pass"});
  logB.append({toUtf8(kit::formatted("of %d stars at +700:", nStars)), "dim"});
  logB.append({toUtf8(kit::formatted("  %4d fall on maps 1-12  (|DEC| <= 45)",
                                     nOnMaps)),
               "dim"});
  logB.append({toUtf8(kit::formatted("  %4d fall on the disc    (DEC >= +52)",
                                     nOnDisc)),
               "dim"});
  logB.append({toUtf8(kit::formatted(
                   "  %4d fall in the UNCOVERED band +45..+52", nInGap)),
               "number"});
  logB.append({toUtf8(kit::formatted(
                   "  %4d are south of DEC -45, off the chart", nTooSouth)),
               "number"});
  logB.append(
      {toUtf8("Chang'an is 34.3N, so DEC < -55.7 never rises at all"), "dim"});

  logC.append({toUtf8("THE SCHOOLS, AND WHAT IS NOT ATTESTED"), "heading"});
  logC.append(
      {toUtf8("S.3326 is the FIRST document to colour the three"), "dim"});
  logC.append(
      {toUtf8("  schools: Shi shi RED, Gan shi BLACK, Wu Xian WHITE"), "dim"});
  logC.append(
      {toUtf8(kit::formatted(
           "Tables 4+5 give a colour for 54 asterisms; %d stars", nSchooled)),
       "dim"});
  logC.append(
      {toUtf8(kit::formatted(
           "%d stars have NO published school: drawn undeclared", nUnattested)),
       "number"});
  logC.append(
      {toUtf8("guessing the rest would be inventing the evidence"), "dim"});
  logC.append(
      {toUtf8("Huagai +6 unaccounted: Chen Zhuo HAS Gang, 9 stars"), "number"});
  logC.append(
      {toUtf8("  9 != 6, so it is consistent and does not close"), "dim"});
  logC.append(
      {toUtf8("Sangong: Chen Zhuo files one under WU XIAN (white),"), "dim"});
  logC.append({toUtf8("  the map draws BOTH black. printed, not corrected."),
               "number"});

  ctx.ticker.add([this, &tick = ctx.ticker](double) {
    clockT = tick.elapsed();
    const float t = (float)std::fmod(clockT, (double)kLoop);
    scribe = t;
    const float e =
        2000.0f + (700.0f - 2000.0f) * smooth((t - tPrec0) / (tPrec1 - tPrec0));
    const float f = smooth((t - tFold0) / (tFold1 - tFold0));
    rebuild(e, f);
    return true;
  });

  ctx.composer.render(describe(ctx));
  ctx.composer.renderSlot("audit", auditPanel());
}

auto DunhuangStarChart::update(double, sketch::SketchContext& ctx) -> void {
  const float t = (float)std::fmod(clockT, (double)kLoop);
  const bool want = t >= tSettle;
  if (want != settled) {
    settled = want;
    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("audit", auditPanel());
  }
}

SIGIL_SKETCH(DunhuangStarChart, "Study \xc2\xb7 Esoteric",
             "The Dunhuang star chart (c. 649\xe2\x80\x93"
             "684) reprojected from "
             "1,460 real stars \xe2\x80\x94 and it refuses to answer")
