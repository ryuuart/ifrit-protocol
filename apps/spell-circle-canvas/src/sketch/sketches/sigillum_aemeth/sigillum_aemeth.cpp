// sigillum aemeth: scene assembly and animation.

// TAGS: Geometry/Diagrams, Patterns/Ornament

#include "SigillumAemeth.h"

auto SigillumAemeth::describe(sketch::SketchContext&) -> Element {
  // THE SETTLE. The rim is the FRAME and the two inner systems turn against
  // it off ONE Output — the seven-fold body one way, the five-fold heart the
  // other. Relative motion is what the figure is about: since gcd(40,7) =
  // gcd(40,5) = gcd(7,5) = 1 the three agree in exactly one direction, and
  // arriving there is the ending. Turning the rim as well would add a third
  // full-plate layer resampled every frame and show no relative motion that
  // is not already on screen.
  return box().inset(0).children(
      {box()
           .rect(path::centred({kCx, kCy}, {2 * kRR, 2 * kRR}))
           .children({waxGround().cache(Cache::Texture), circumferenceRules(),
                      circumferenceCells(),
                      // the whole seven-fold system turns as one body —
                      // heptagon, its two letter bands, the heptagram woven on
                      // its vertices, and everything the heptagram's points
                      // contain
                      box()
                          .inset(0)
                          .transformOrigin(0.5f, 0.5f)
                          .rotate(bind(&settle).target(0.0f, -360.0f / 7.0f))
                          .opacity(animate(from(0.0f).to(1.0f),
                                           ramp(tInner * 1000, 900)))
                          .cache(Cache::Texture)
                          .children({angles(), heptagonNames(), heptagram(),
                                     inner()}),
                      innerRings(),
                      pentagram()
                          .rotate(bind(&settle).target(0.0f, 72.0f))
                          .transformOrigin(0.5f, 0.5f),
                      centreCross(), slot("solver")}),
       margin(), consolePanel(), colophon()});
}

auto SigillumAemeth::setup(sketch::SketchContext& ctx) -> void {
  // The plate is the finished object, and this piece arrives in stages: the
  // last Name is solved at tSolve + 7 tSolveEach, the birds' square lands
  // 2.6 s after tBirds, and the rings start turning at tSpin. Only the
  // window between the two shows every part of it at once.
  sketch::kit::stage(ctx, {.size = SkSize::Make(kW, kH),
                           .captureAt = 14.0,
                           .background = kVitrine});

  // ONE FALLBACK CHAIN PER LETTERING SYSTEM, resolved through the
  // library's own walk: the first installed family wins, and a machine
  // with none of them gets the default face AT THE WEIGHT ASKED FOR
  // rather than silently at Normal.
  faceSerif = sketch::kit::houseFace(sketch::kit::Voice::Book);
  faceItalic = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                      SkFontStyle::kItalic_Slant);
  faceMono = sketch::kit::houseFace(sketch::kit::Voice::Terminal);
  faceSeal = sigil::weave::ports::face({"Herculanum", "Optima", "Baskerville"});
  faceRing = sigil::weave::ports::face(
      {"Trattatello", "Hoefler Text", "Baskerville"}, SkFontStyle::Italic());
  faceQuill = sketch::kit::houseFace(sketch::kit::Voice::Book, 400,
                                     SkFontStyle::kItalic_Slant);
  faceDisplay = sigil::weave::ports::face(
      {"Luminari", "Herculanum", "Optima", "Baskerville"});

  waxGrain = Paint::recipe(field::grain(1.6f, 4, 1582.0f, 0.34f));
  waxSpeck = patterns::speckle(520, 18, 1.4f, 4.4f,
                               {skia::toColor(hexColor(0x6a4a20, 0.10f))});
  waxSpeck.seed(1582);

  doc = ctx.assets.json(ctx.local("data/content.json"));

  // solve, then draw what the solver said
  for (int n = 0; n < 7; ++n)
    solved[(size_t)n] = walkFrom(kNames[(size_t)n].start);
  visited.fill(false);
  for (const Solved& s : solved)
    for (int c : s.cells) visited[(size_t)(c - 1)] = true;
  usedCells = 0;
  for (bool v : visited) usedCells += v ? 1 : 0;
  weave = buildWeave(rHept);
  runChecks();

  // THE FIGURES THE MARGIN'S SENTENCES NAME, under the names the document
  // calls them by: the walk's own counts and the cells it never reached.
  std::string un, glyphs;
  for (int i = 0; i < 40; ++i)
    if (!visited[(size_t)i]) {
      un += (un.empty() ? "" : "·") + std::to_string(i + 1);
      glyphs += kRing[(size_t)i].glyph;
    }
  figures = {{"used", std::to_string(usedCells)},
             {"unvisited", std::to_string(40 - usedCells)},
             {"cells", un},
             {"glyphs", glyphs}};

  // ONE Output: three systems turn off it and stop together at 0.
  ctx.ticker.add([this](double dt) {
    clockT += dt;
    const double loop = 26.0;
    const double t = std::fmod(clockT, loop);
    // the rings turn from tSpin, decelerating into the one alignment
    float s = 0.0f;
    if (t > (double)tSpin && t < 24.0) {
      const double u = (t - (double)tSpin) / (24.0 - (double)tSpin);
      // ease out from 1 turn to none
      s = (float)((1.0 - u) * (1.0 - u) * (1.0 - u));
    }
    settle = s;
  });

  ctx.composer.render(describe(ctx));
}

auto SigillumAemeth::update(double, sketch::SketchContext& ctx) -> void {
  const double t = std::fmod(clockT, 26.0);
  // -1 before the walk and again once the plate turns; 7 = every Name
  // solved, all of them held as faint ghosts while the birds arrive.
  int phase = -1;
  if (t >= (double)tSolve && t < (double)tSpin - 0.4)
    phase = std::min(7, (int)((t - (double)tSolve) / (double)tSolveEach));
  if (phase != solvePhase) {
    solvePhase = phase;
    ctx.composer.renderSlot("solver", solverOverlay());
  }
}

SIGIL_SKETCH(SigillumAemeth, "Study · Esoteric",
             "Dee's Sigillum Dei Aemeth (1582) — solved from the "
             "angels' own jump rule, 33 of 40 cells")
