// dunhuang star chart: scene assembly and animation.
//
// EDIT THESE FIRST: the sentences are in data/content.json beside this file.
// Its keys are `title`, `provenance`, `claim`, `plate`, `scale`, `sources`,
// `locator`, `raRuler`, `poleTitle`/`pole`/`poleEpochs`/`poleSweep`/
// `poleMarks`, `ruleTitle`/`rule`, `disc`, `auditTitle`/`auditLead`/
// `auditHeads`/`auditFoot`/`auditNoLabel`, `projectionTitle`/
// `projectionPlots`/`projection`, `map13Title`/`map13`, `archer`, and the
// three console runs `logJoin`, `logEpoch` and `logSchools`. A line is the
// words alone, or `{"words": …, "class": …}` where the CONTENT decides the
// class; `{name}` in a line is the figure this study measured under that
// name.

// TAGS: Data/Astronomy

#include "DunhuangStarChart.h"

auto DunhuangStarChart::graticule() -> Element {
  return box()
      .rect(SkRect::MakeXYWH(108, 250, 2344, 764))
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
      .stroke(PathFormat{.width = 0.8f,
                         .strokeFill = Fill::color(hexColor(0x2f6d86, 0.42f)),
                         .dashIntervals = {3, 7}});
}

auto DunhuangStarChart::starField() -> Element {
  return box()
      .cover()
      .key("stars")
      .opacity(gate(tSky - 0.5f, tSky + 0.7f))
      .children({instancing::instances(atlas, pool, instancing::Mode::Live)});
}

auto DunhuangStarChart::segment(int seg) -> Element {
  const auto maps = std::views::iota(1, 13);
  return box()
      .rect(SkRect::MakeLTRB(segLo(seg), kSegTop, segHi(seg), kSegTop + kSegH))
      .overflow(Overflow::Clip)
      .key(seg ? "segR" : "segL")
      .children({each(maps, [this, seg](int k) { return mapFrame(k, seg); }),
                 each(maps, [this, seg](int k) { return columnBand(k, seg); }),
                 discPlate(seg), discNotes(seg), raRuler(seg),
                 seg ? box() : unreadTitle(), seg ? box() : archer()});
}

auto DunhuangStarChart::describe(sketch::SketchContext&) -> Element {
  // THE PLATE'S RUNNING VOICE AND ITS WHOLE CLASS TABLE STAND HERE, so every
  // leaf under them is set by name: the terminal face at the note's size in
  // the note's ink, which a class steps away from and nothing restates.
  return box()
      .width(100_pw)
      .height(100_ph)
      .font({.face = faceMono, .size = 9.0f, .color = hexColor(0x9a8a68)})
      .applyStyleSheet(voices())
      .children({ground(), locator(), graticule(),
                 scrollBand(-90, kBreakL, "bandL", -0.42f),
                 scrollBand(kBreakR, kW + 90, "bandR", -0.42f), segment(0),
                 segment(1), breakMark(), starField(), asterismLines(),
                 map5Labels(), headings(), poleDrift(), poleText(), ruleNote(),
                 projectionPanel(), map13Panel(), slot("audit"),
                 consolePanel()});
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
  paperSpeck =
      patterns::speckle(900, 34, 0.20f, 0.85f,
                        {hexColor(0x6a5330, 0.10f), hexColor(0x2a2118, 0.08f)});
  paperSpeck.seed(649);

  doc = sketch::kit::Document(ctx, "data/content.json");
  cat = catalogue(ctx);
  conc = readConcordance(ctx);
  nStars = cat.stars();
  nAst = cat.asterisms();

  computeJoins();
  buildAsterismArt();
  buildFixtures();

  // FIVE cells, because an atlas cell is a whole ELEMENT TREE: the black
  // ring and the school fill bake into ONE sprite, so 1,460 dots stay one
  // draw and there is no second concentric pass.
  atlas = std::make_shared<instancing::CellSheet>(3.0f);
  auto dot = [](sigil::material::Color fill, bool ring) {
    auto e = box().width(11).height(11).shape(shapes::circle());
    if (fill.a > 0) e.fill(Fill::color(fill));
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

  // THE FIGURES THE SENTENCES NAME, under the names the document calls them
  // by and at the width the columns of the console read at. Every number on
  // this plate is the join's own, and none of them is typed twice.
  doc.figures(
      {{"ast", kit::formatted("%d", nAst)},
       {"stars", kit::formatted("%d", nStars)},
       {"onMaps", kit::formatted("%4d", nOnMaps)},
       {"onDisc", kit::formatted("%4d", nOnDisc)},
       {"inGap", kit::formatted("%4d", nInGap)},
       {"tooSouth", kit::formatted("%4d", nTooSouth)},
       {"schooled", kit::formatted("%d", nSchooled)},
       {"unattested", kit::formatted("%d", nUnattested)},
       {"m5Sxc", kit::formatted("%d", m5Sxc)},
       {"m5Map", kit::formatted("%d", m5Map)},
       {"m5ChenZhuo", kit::formatted("%d", m5ChenZhuo)},
       {"poleOffMm", kit::formatted("%.1f", (90.0f - kDiscCenDec) / kPolPerMm)},
       {"mercDeg", kit::formatted("%.3f", depMerc.maxDeg)},
       {"mercMm", kit::formatted("%.2f", depMerc.mm)},
       {"mercRatio", kit::formatted("%.2f", depMerc.ratio)},
       {"mercSigma", kit::formatted("%.4f", depMerc.sigma)},
       {"mercPub", kit::formatted("%.2f", 0.002f / depMerc.sigma)},
       {"stereoDeg", kit::formatted("%.3f", depStereo.maxDeg)},
       {"stereoMm", kit::formatted("%.2f", depStereo.mm)},
       {"stereoRatio", kit::formatted("%.2f", depStereo.ratio)},
       {"stereoSigma", kit::formatted("%.4f", depStereo.sigma)},
       {"stereoPub", kit::formatted("%.2f", 0.013f / depStereo.sigma)}});

  // THE THREE CHECKING RUNS, read out of the document: one append per line,
  // and the feed's levels are the classes the file names.
  const std::pair<feed::TextRing*, const char*> runs[3] = {
      {&logA, "logJoin"}, {&logB, "logEpoch"}, {&logC, "logSchools"}};
  for (const auto& [ring, key] : runs)
    for (const sketch::kit::Document::Line& l : doc.run(key))
      ring->append({l.words, l.styleClass});

  ctx.ticker.add([this, &tick = ctx.ticker] {
    clockT = tick.elapsed();
    const float t = (float)std::fmod(clockT, (double)kLoop);
    scribe = t;
    const float e =
        2000.0f + (700.0f - 2000.0f) * smooth((t - tPrec0) / (tPrec1 - tPrec0));
    const float f = smooth((t - tFold0) / (tFold1 - tFold0));
    rebuild(e, f);
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

SIGIL_SKETCH(DunhuangStarChart, "Study · Esoteric",
             "The Dunhuang star chart (c. 649–684) reprojected from "
             "1,460 real stars — and it refuses to answer")
