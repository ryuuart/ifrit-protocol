/** @file
 * black_watch — the Government sett, woven from its thread counts.
 *
 * A TARTAN IS A PROGRAM. The register's threadcount says what colour each
 * thread is, the loom's three lines say which thread is on top at every
 * crossing, and the cloth follows with not one cell authored. The card
 * mounts that cloth the way a pattern card does: the sett as the one-
 * dimensional object it is, the cloth woven beneath it pick by pick, the
 * weaver's draft, the third colours a crossing makes, the five shade
 * cards one count is dyed in, the four clan labels the Cockburn
 * collection put on one cloth, Campbell of Argyll beside it, and the
 * arithmetic that proves the reconstruction, computed where it is shown.
 */
// TAGS: Patterns/Tiling

#include <choreograph/Easing.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmeasure/check/Check.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/kit/LineTables.h>

#include <ranges>

#include "Card.h"
#include "Fringe.h"
#include "Tartan.h"

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace measure = sigil::measure;
namespace weave = sigil::weave;
namespace data = sigil::data;
namespace shapes = sigil::geometry::shapes;
namespace shapers = sigil::geometry::shapers;
namespace ease = sigil::motion::ease;
using namespace sigil::compose;
using namespace sigil::motion;
using namespace sigil::weave::literals;

namespace {

constexpr SkSize kCanvas{1600, 1536};
/** Pixels per thread on the cloth, the sett bar and the swatches — a whole
 *  number, because a thread narrower than a pixel or magnified by a
 *  fraction over the twill's four-thread period is a moiré generator. */
constexpr float kThread = 2;
/** The cloth panel in threads: two setts across, a sett and a half down. */
constexpr int kEnds = 504, kPicks = 378;
/** Pixels per thread in the draft, and the window of the sett it shows:
 *  ends 61 to 92, the one run short enough to square that still crosses
 *  all three colours, so both kinds of blend cell stand in it. */
constexpr float kDraftCell = 9;
constexpr int kDraftFirst = 60, kDraftEnds = 32;
/** The provenance swatches, and where in the cloth each one is cut: the
 *  Government crop holds a blue square, a green one and the black bars
 *  between; Campbell's two overchecks stand 208 threads apart, so its
 *  crop takes the warp from the yellow and the weft from the white, the
 *  one place the two are seen crossing. */
constexpr SkSize kSwatch{180, 110};
constexpr SkPoint kGovernmentCrop{34, 40}, kArgyllCrop{150, 358};

// ---------------------------------------------------------------------------
// The loop, as positions in one eight-second phase: the warp beams on, the
// picks beat in, the arithmetic proves itself, and the cloth is dyed in
// each shade card in turn before it holds in the first.

constexpr float kCycle = 8;
constexpr float kBeamEnd = 0.7f / kCycle;
constexpr float kWeaveEnd = 4.1f / kCycle;
constexpr float kProveEnd = 4.9f / kCycle;
/** The shade card the cloth wears from each moment, and how long each
 *  card takes to fade over the one before. The last entry returns to the
 *  first card for the hold the still is taken in. */
struct Turn {
  int card;
  float start;
};
constexpr std::array<Turn, 6> kTurns{{{0, kWeaveEnd},
                                      {1, 0.6125f},
                                      {2, 0.6625f},
                                      {3, 0.7125f},
                                      {4, 0.7625f},
                                      {0, 0.8125f}}};
constexpr float kFade = 0.025f;

/** THE LOOM'S BEAT. The cloth is woven an inch at a time — 42 picks, the
 *  sett's own density — and each inch is two strokes: the shuttle is
 *  thrown across the open shed, then the reed beats the pick home and the
 *  fell drops by one inch. Nine inches make the panel's picks. */
constexpr int kBeats = 9;
constexpr float kThrow = 0.6f;

/** Where the fell stands, as a fraction of the panel, at a moment of the
 *  weave: still while the shuttle flies, then a hard eased drop. */
float beaten(float weave) {
  if (weave >= 1) return 1;
  const float scaled = std::max(weave, 0.0f) * kBeats;
  const float inch = std::floor(scaled);
  const float stroke = std::clamp((scaled - inch - kThrow) / (1 - kThrow), 0.0f, 1.0f);
  return (inch + sigil::motion::ease::outCubic(stroke)) / kBeats;
}

/** Where the shuttle is across the shed: out and back on alternate
 *  inches, eased at both boxes, and waiting at its box while the reed
 *  beats. */
float thrown(float weave) {
  const float scaled = std::clamp(weave, 0.0f, 0.9999f) * kBeats;
  const int inch = (int)scaled;
  const float flight = std::clamp((scaled - (float)inch) / kThrow, 0.0f, 1.0f);
  const float across = ease::smoothstep(flight);
  return inch % 2 == 0 ? across : 1 - across;
}

/** A layer filling the box it stands in. */
Element layer(material::Paint paint) {
  return box().cover().fill(std::move(paint));
}

/** A heading over what it announces. */
Element titled(Utf8 heading, std::initializer_list<Children> body) {
  return box().column().gap(12).children({document::h2(std::move(heading))})
      .children(body);
}

/** One check of the card: what it is called, and the check itself, whose
 *  label is the evidence it was judged on. */
struct Proof {
  std::string name;
  measure::Check check;
};

}  // namespace

struct BlackWatch {
  sketch::kit::Document doc;
  CardColours colours;
  Sett watch, argyll;
  std::vector<uint8_t> threads;
  std::vector<ShadeCard> cards;
  Verdict verdict;
  std::vector<Proof> proofs;

  /** Held because the bake is a pattern's identity: one minted inside a
   *  describe would re-render its tile on every render. */
  Pattern warpOnBeam, argyllCloth, drawdown, grooves, draftGrid;
  std::vector<Pattern> cloths;  // one per shade card
  std::array<Pattern, 9> blends;
  /** One wound card per shade card, a wrap of yarn per shade. */
  std::vector<std::array<Pattern, 3>> wraps;
  material::Paint board, yarn;

  sigil::motion::Animatable<float> loom = sigil::motion::animatable(0.0f);

  // =========================================================================

  void weaveEverything() {
    threads = watch.threads();
    const Shades& modern = cards.front().shades;
    for (const ShadeCard& card : cards)
      cloths.push_back(
          material::pattern::clothTile(cloth(threads, card.shades, 0.17f), kThread));
    // The warp on the beam is the cloth before a single pick is woven: an
    // interlacing that never lifts the weft shows the ends alone.
    warpOnBeam = material::pattern::clothTile(
        {.warp = threads,
         .weft = {0},
         .shades = {modern.begin(), modern.end()},
         .weave = material::pattern::Weave{.over = 1, .under = 0, .advance = 0}},
        kThread);
    argyllCloth =
        material::pattern::clothTile(cloth(argyll.threads(), modern, 0.17f), kThread);
    const std::vector<uint8_t> window(
        threads.begin() + kDraftFirst,
        threads.begin() + kDraftFirst + kDraftEnds);
    drawdown = material::pattern::clothTile(cloth(window, modern, 0.22f), kDraftCell);
    // The blend table is the same generator at a one-thread sett each way,
    // so if a cell disagrees with the cloth then one of the two is wrong.
    for (int weftShade = 0; weftShade < 3; ++weftShade)
      for (int warpShade = 0; warpShade < 3; ++warpShade)
        blends[(size_t)(weftShade * 3 + warpShade)] =
            material::pattern::clothTile({.warp = {0},
                                 .weft = {1},
                                 .shades = {modern[(size_t)warpShade],
                                            modern[(size_t)weftShade]},
                                 .weave = kTwill},
                                8);
    // The grooves between yarns: a hairline at every end and pick, which
    // the rib alone does not draw, kept on the threads' pixel grid.
    grooves = material::pattern::gridLines(kThread, 1, {0, 0, 0, 0.12f})
                  .filter(SkFilterMode::kNearest);
    draftGrid =
        material::pattern::gridLines(kDraftCell, 0.7f, faded(colours.rule, 0.6f))
            .filter(SkFilterMode::kNearest);
    // The board is one recipe, paint and tooth together; the yarn's tooth
    // keeps frequency · stretch · 2^(octaves−1) under 0.4, past which its
    // y axis aliases into hash noise.
    // A mount board reads as one even card: a fine tooth the eye takes
    // as paper and almost no wear, since any slow blotch on a light card
    // reads as marble rather than as board.
    board = material::Paint::recipe(
        material::kit::board({.paint = colours.ground,
                              .tooth = 0.05f,
                              .toothScale = 0.06f,
                              .wear = 0.004f,
                              .wearScale = 0.004f,
                              .seed = 7.0f}));
    yarn = material::Paint::recipe(
        material::field::grain(0.09f, 3, 3.0f, 0.75f));
    // A shade card is yarn wound round a board: each turn a lit crown and
    // a shadowed valley where it presses on the next.
    for (const ShadeCard& card : cards) {
      std::array<Pattern, 3> wound;
      for (int shade : {K, B, G}) {
        const material::Color dyed = card.shades[(size_t)shade];
        wound[(size_t)shade] =
            material::pattern::sequence(
                {{1, material::mixToward(dyed, {1, 1, 1, 1}, 0.14f, 1)},
                 {1.5f, dyed},
                 {0.5f, material::mixToward(dyed, {0, 0, 0, 1}, 0.45f, 1)}},
                0, material::pattern::Axis::V)
                .filter(SkFilterMode::kNearest);
      }
      wraps.push_back(std::move(wound));
    }
  }

  /** Every row's verdict is computed from the two values it reports, so a
   *  row reading PASS cannot disagree with the figure beside it. The
   *  agreement of the two setts is a claim about the cloth rather than
   *  about this reconstruction, so it is a finding and never counted
   *  against the run. */
  void prove() {
    verdict = verify(watch, argyll);
    std::string units;
    for (int ends : verdict.unitEnds)
      units += kit::formatted("%s%d", units.empty() ? "" : " + ", ends);
    const int perceived = verdict.solids + verdict.blends;
    proofs = {
        {"SETT CLOSES", measure::check(units + " ends", watch.publishedEnds,
                                       verdict.total)},
        {"REFLECTIVE",
         measure::check(kit::formatted("mirrors at thread %d, %d, gap",
                                       verdict.mirrors.empty() ? -1 : verdict.mirrors[0],
                                       verdict.mirrors.size() > 1 ? verdict.mirrors[1]
                                                            : -1),
                        verdict.total / 2, verdict.mirrorGap)},
        {"2/2 BALANCE",
         measure::check(kit::formatted("longest float, warp %d, weft",
                                       verdict.maxWarpFloat),
                        2, verdict.maxWeftFloat)},
        {"THREAD RATIO",
         measure::check(kit::formatted("K %d : B %d : G %d, blue is a third",
                                       verdict.counts[K], verdict.counts[B], verdict.counts[G]),
                        verdict.counts[B] * 3 == verdict.total)},
        {"COLOUR LAW",
         measure::check(
             kit::formatted("n = %d → n(n+1)/2 perceived", verdict.solids),
             verdict.solids * (verdict.solids + 1) / 2, perceived)},
        {"EXACT COVER",
         measure::check(kit::formatted("%d samples, gaps + overlaps",
                                       verdict.samples),
                        0, verdict.uncovered + verdict.doubled)},
        {"CAMPBELL ARGYLL",
         measure::check(
             kit::formatted("n = %d → %d perceived, ends", verdict.argyllSolids,
                            verdict.argyllSolids + verdict.argyllBlends),
             argyll.publishedEnds, verdict.argyllTotal)},
        {"UNIT DRIFT",
         measure::finding(measure::check("max |BW − CA| over A B C D, %", 0.0,
                                         verdict.unitDrift, 1.0))},
        {"TWILL ANGLE", measure::reading("42 epi = 42 ppi, degrees", 45)},
        {"SETT WIDTH",
         measure::reading(
             kit::formatted("%d ends / 42 epi, mm", verdict.total),
             std::round((double)verdict.total / 42.0 * 25.4 * 10.0) / 10.0)},
    };
    const int ends = verdict.total;
    doc.figures({
        {"ends", kit::formatted("%d", ends)},
        {"half", kit::formatted("%d", ends / 2)},
        {"cells", kit::formatted("%d,%03d", ends * ends / 1000,
                                 ends * ends % 1000)},
        {"first", kit::formatted("%d", kDraftFirst + 1)},
        {"last", kit::formatted("%d", kDraftFirst + kDraftEnds)},
        {"window", spelled(threads, kDraftFirst, kDraftEnds)},
        {"cell", kit::formatted("%.0f", kDraftCell)},
        {"solids", kit::formatted("%d", verdict.solids)},
        {"blends", kit::formatted("%d", verdict.blends)},
        {"perceived", kit::formatted("%d", perceived)},
        {"argyll", kit::formatted("%d", verdict.argyllTotal)},
        {"drift", kit::formatted("%.2f", verdict.unitDrift)},
    });
  }

  /** Where the mirror axes fall across the cloth, in pixels: each pivot
   *  of each repeat the panel shows, the first repeat's first. */
  std::vector<float> mirrorPositions() const {
    std::vector<float> positions;
    for (int repeat = 0; repeat < 2; ++repeat)
      for (int mirror : verdict.mirrors) {
        const float position =
            (float)(mirror + repeat * verdict.total) * kThread;
        if (position <= kEnds * kThread) positions.push_back(position);
      }
    return positions;
  }

  // =========================================================================
  // The masthead: the registration over a rule, the specimen ticket in
  // the corner.

  Element masthead() const {
    const data::Json& words = doc["masthead"];
    return box().row().justifyContent(Justify::SpaceBetween).children(
        {box().column().gap(14).children(
             // The title is struck into the board: a hairline of light
             // along the lower lip of every glyph's bite.
             {document::h1(doc.phrase(words["title"]))
                  .decorationOutline(Boundary::Glyphs)
                  .background(styles::dropShadow({1, 1, 1, 0.8f}, {0, 1.2f}, 0.4f))
                  .foreground(styles::InnerShadow{{0, 0, 0, 0.55f}, {0, 1.5f}, 1.5f}),
              document::lead(doc.phrase(words["registration"]))}),
         box().row().gap(13).width(440).styleClass("ticket").children(
             {kit::line({.thickness = 1, .column = true,
                         .fill = Fill::var("rule")}),
              box().column().gap(4.5f).children(
                  {each(words["ticket"].array(),
                        [this](const data::Json& line) {
                          return document::caption(doc.phrase(line));
                        })})})});
  }

  // =========================================================================
  // The sett bar: the threadcount as the one-dimensional object it is,
  // warp-aligned with the cloth under it — it IS the warp on the beam.

  Element settBar() const {
    const data::Json& words = doc["sett"];
    const float width = kEnds * kThread;
    Element bar =
        box()
            .height(34)
            .fill(warpOnBeam.material())
            .stroke(stroke(1, Fill::var("rule"), PathFormat::Align::Outer))
            .children({document::caption(doc.phrase(words["register"]))
                           .styleClass("proof")
                           .right(0)
                           .top(-26)});
    // The pivots snap in when the arithmetic proves them; the first
    // repeat's are named.
    const std::vector<float> pivots = mirrorPositions();
    for (size_t index = 0; index < pivots.size(); ++index) {
      bar.children({box()
                        .left(pivots[index] - 6)
                        .top(-11)
                        .width(12)
                        .height(10)
                        .shape(shapes::polygon(3, 180))
                        .fill(Fill::var("proof"))
                        .transformOrigin(pct(50), pct(100))
                        .scale(sigil::motion::bind(loom, {.from = {kWeaveEnd, kWeaveEnd + 0.035f}, .clampFrom = true, .ease = ease::outBack()}))});
      if (index < verdict.mirrors.size())
        bar.children(
            {box()
                 .left(pivots[index] - 45)
                 .top(-26)
                 .width(90)
                 .opacity(sigil::motion::bind(loom, {.from = {kWeaveEnd + 0.01f, kWeaveEnd + 0.045f}, .clampFrom = true}))
                 .children(
                     {document::caption(doc.phrase(words["pivots"][index]))
                          .styleClass("proof")
                          .textAlign(weave::TextAlignment::kCenter)
                          .width(pct(100))})});
    }
    // The numerals of one repeat, each centred under its own band.
    const std::vector<Run> runs = watch.runs();
    return box().column().width(width).marginTop(26).children(
        {std::move(bar),
         box().row().marginTop(3).styleClass("runs").children(
             {each(runs,
                   [](const Run& run) {
                     return box()
                         .width(kThread * (float)run.threads)
                         .flexShrink(0)
                         .alignItems(Align::Center)
                         .children({document::caption(
                             std::to_string(run.threads))});
                   })}),
         document::caption(spelled(threads, 0, threads.size()))
             .styleClass("count")
             .marginTop(4),
         document::caption(doc.phrase(words["notation"])).marginTop(6)});
  }

  // =========================================================================
  // The cloth: the warp beams on left to right, the picks beat in top to
  // bottom, the mirror axes flash while the arithmetic proves, and the
  // cloth is dyed in each shade card in turn.

  Element clothPanel() const {
    const float width = kEnds * kThread, height = kPicks * kThread;
    Element panel =
        box()
            .width(width)
            .height(height)
            .marginTop(14)
            .overflow(Overflow::Clip)
            .background(styles::dropShadow(faded(colours.shadow, 0.55f),
                                           {3, 4}, 10))
            .fill(Fill::var("well"))
            .children({
                layer(cloths.front().material()),
                // The warp alone covers the woven cloth and withdraws
                // downward, so the cloth appears pick by pick above the
                // fell. The warp is the same at every height, so squashing
                // it only shortens it.
                layer(warpOnBeam.material())
                    .transformOrigin(pct(50), pct(100))
                    .scaleY(sigil::motion::bind(loom, {.from = {kBeamEnd, kWeaveEnd}, .clampFrom = true, .ease = beaten, .to = {1.0f, 0.0f}})),
                // Before that the warp itself is beamed on: a blind in the
                // well's colour withdraws to the right.
                box()
                    .cover()
                    .fill(Fill::var("well"))
                    .transformOrigin(pct(100), pct(50))
                    .scaleX(sigil::motion::bind(loom, {.from = {0, kBeamEnd}, .clampFrom = true, .to = {1.0f, 0.0f}})),
            });
    for (size_t turn = 1; turn < kTurns.size(); ++turn)
      panel.children(
          {layer(cloths[(size_t)kTurns[turn].card].material())
               .opacity(sigil::motion::bind(loom, {.from = {kTurns[turn].start, kTurns[turn].start + kFade}, .clampFrom = true}))});
    panel.children({layer(grooves.material())
                        .blendMode(material::BlendMode::Multiply)
                        .opacity(0.9f)
                        .cache(Cache::Texture),
                    layer(yarn)
                        .blendMode(material::BlendMode::Overlay)
                        .opacity(0.14f)
                        .cache(Cache::Texture),
                    // Light rakes the cloth from the upper left, as it
                    // falls on a card pinned by a window: the nap catches
                    // it near and gives it up far.
                    box()
                        .cover()
                        .fill(material::Paint::radialGradient(
                            {0.12f * width, 0.02f * height}, 0.85f * width,
                            {{0, {1, 1, 1, 0.13f}},
                             {0.4f, {1, 1, 1, 0}},
                             {0.65f, {0, 0, 0, 0}},
                             {1, {0, 0, 0, 0.32f}}},
                            {.units = material::GradientUnits::Pixels}))
                        .cache(Cache::Texture)});
    // The mirror axes flash while the arithmetic proves itself.
    panel.children({each(mirrorPositions(), [this](float position) {
      return box()
          .left(position - 0.5f)
          .top(0)
          .width(1)
          .height(pct(100))
          .fill(Fill::var("proof"))
          .opacity(sigil::motion::bind(loom, {.from = {kWeaveEnd, kProveEnd}, .envelope = sigil::motion::envelope::trapezoid(0, 0.25f, 0.75f, 1), .to = {0.0f, 0.8f}}));
    })});
    // The fell, the edge of the cloth the reed beats each inch home to,
    // and the shuttle flying the open shed along it between beats.
    const auto weaving = [this] {
      return sigil::motion::bind(loom, {.from = {kBeamEnd - 0.01f, kWeaveEnd + 0.01f}, .envelope = sigil::motion::envelope::trapezoid(0, 0.03f, 0.97f, 1)});
    };
    const auto atFell = [this, height](float lift) {
      return sigil::motion::bind(loom, {.from = {kBeamEnd, kWeaveEnd}, .clampFrom = true, .ease = beaten, .to = {lift, height + lift}});
    };
    panel.children(
        {box()
             .left(0)
             .top(0)
             .width(pct(100))
             .height(2)
             .fill(Fill::var("proof"))
             .translateY(atFell(0))
             .opacity(weaving()),
         box()
             .left(0)
             .top(0)
             .width(58)
             .height(11)
             .shape(shapes::svg("M0 5.5 C9 0 49 0 58 5.5 C49 11 9 11 0 5.5 Z"))
             .fill(material::Paint::linearGradient(
                 {0, 0}, {0, 11},
                 {{0, colourOf("#E0B878")},
                  {0.45f, colourOf("#B98A4E")},
                  {1, colourOf("#6E4A26")}},
                 {.units = material::GradientUnits::Pixels}))
             .stroke(stroke(0.8f, Fill::var("ink"), PathFormat::Align::Inner))
             .background(
                 styles::dropShadow(faded(colours.shadow, 0.6f), {1, 3}, 3))
             .children({box()
                            .left(17)
                            .top(3.5f)
                            .width(24)
                            .height(4)
                            .borderRadius(2)
                            .fill(cards.front().shades[B])})
             .translateX(sigil::motion::bind(loom, {.from = {kBeamEnd, kWeaveEnd}, .clampFrom = true, .ease = thrown, .to = {-58.0f, width}}))
             .translateY(atFell(-5.5f))
             .opacity(weaving())});
    return panel;
  }

  /** The cloth as a sample laid on the board: the woven panel and the
   *  warp running on past its last pick into a knotted fringe. Nothing
   *  pins it — a cutting with its ends left free is laid, not cornered. */
  Element mountedCloth() const {
    const float width = kEnds * kThread;
    constexpr float kHidden = 4, kHang = 30, kClear = 22;
    const Fringe fringe = tassels(kEnds, kHidden, kHang, kThread);
    // The fringe stands behind the panel, so the knots show just below
    // its edge and the tassels hang free onto the board, each lifted off
    // it by its own small shadow.
    Element mount =
        box().column().width(width).paddingBottom(kHang + kClear).children(
            {box()
                 .left(0)
                 .top(14 + kPicks * kThread - kHidden)
                 .width(width)
                 .height(fringe.height)
                 .shape(shapes::svg(fringe.outline.c_str()))
                 .fill(warpOnBeam.material())
                 .background(styles::dropShadow(faded(colours.shadow, 0.5f),
                                                {0.6f, 1.4f}, 1.2f))
                 .transformOrigin(pct(0), pct(50))
                 .scaleX(sigil::motion::bind(loom, {.from = {0, kBeamEnd}, .clampFrom = true})),
             clothPanel()});
    return mount;
  }

  // =========================================================================
  // The weaver's draft, drawn from the loom's three lines — threading
  // across the top, tie-up in the corner, treadling down the right — and
  // the drawdown the cloth generator weaves from them. If the two ever
  // disagree, one of them is wrong.

  Element draftBlock(int columns, int rows,
                     const std::function<bool(int, int)>& lifted) const {
    return box()
        .row()
        .flexWrap()
        .width((float)columns * kDraftCell)
        .stroke(stroke(1, Fill::var("ink"), PathFormat::Align::Outer))
        .children({each(std::views::iota(0, columns * rows),
                        [&](int index) {
                          return box().styleClass(
                              lifted(index % columns, index / columns)
                                  ? "cell lifted"
                                  : "cell");
                        }),
                   layer(draftGrid.material())});
  }

  Element draft() const {
    const data::Json& words = doc["draft"];
    const auto tagged = [this](const data::Json& tag, Element block) {
      return box().column().alignItems(Align::Center).gap(4).children(
          {document::caption(doc.phrase(tag)).styleClass("tag"),
           std::move(block)});
    };
    Element threading = draftBlock(kDraftEnds, 4, [](int end, int shaft) {
      return (kDraftFirst + end) % 4 == shaft;
    });
    threading.children({box().left(-15).top(0).column().children(
        {each(std::views::iota(1, 5), [](int shaft) {
          return box().height(kDraftCell).justifyContent(Justify::Center)
              .children({document::caption(std::to_string(shaft))
                             .styleClass("tag")});
        })})});
    Element treadling = draftBlock(4, kDraftEnds, [](int treadle, int pick) {
      return (kDraftFirst + pick) % 4 == treadle;
    });
    // The treadle underfoot, stepping down in time with the fell.
    treadling.children(
        {box()
             .left(-3)
             .top(0)
             .width(4 * kDraftCell + 6)
             .height(kDraftCell)
             .fill(faded(colours.proof, 0.3f))
             .translateY(sigil::motion::bind(loom, {.from = {kBeamEnd, kWeaveEnd}, .clampFrom = true, .quantize = kDraftEnds, .to = {0.0f, (kDraftEnds - 1) * kDraftCell}}))
             .opacity(sigil::motion::bind(loom, {.from = {kBeamEnd, kWeaveEnd}, .envelope = sigil::motion::envelope::trapezoid(0, 0.01f, 0.99f, 1)}))});
    const float side = (float)kDraftEnds * kDraftCell;
    return titled(
        doc.phrase(words["heading"]),
        {box().row().gap(8).alignItems(Align::End).marginLeft(15).children(
             {tagged(words["tags"][0], std::move(threading)),
              tagged(words["tags"][1],
                     draftBlock(4, 4, [](int treadle, int shaft) {
                       return shaft == treadle || shaft == (treadle + 1) % 4;
                     }))}),
         box().row().gap(8).alignItems(Align::Start).marginLeft(15).children(
             {tagged(words["tags"][3],
                     box()
                         .width(side)
                         .height(side)
                         .fill(drawdown.material())
                         .stroke(stroke(1, Fill::var("ink"),
                                        PathFormat::Align::Outer))
                         .children({layer(draftGrid.material())})),
              tagged(words["tags"][2], std::move(treadling))}),
         document::caption(doc.phrase(words["caption"]))});
  }

  // =========================================================================
  // The third colours: three threads crossed make three solids and three
  // blends, and the blend is woven, never mixed.

  Element blendTable() const {
    const data::Json& words = doc["blends"];
    constexpr float kCell = 46, kGap = 6;
    const auto code = [](int shade, float width, float height) {
      return box().width(width).height(height).alignItems(Align::Center)
          .justifyContent(Justify::Center).children(
              {document::caption(std::string(1, kShadeCodes[(size_t)shade]))
                   .styleClass("code")});
    };
    Element table = box().column().gap(kGap).children(
        {box().row().gap(kGap).marginLeft(16 + kGap).children(
            {each(std::views::iota(0, 3),
                  [&](int warp) { return code(warp, kCell, 14); })})});
    for (int weftShade = 0; weftShade < 3; ++weftShade)
      table.children({box().row().gap(kGap).children(
          {code(weftShade, 16, kCell),
           each(std::views::iota(0, 3), [&](int warpShade) {
             return box()
                 .width(kCell)
                 .height(kCell)
                 .fill(blends[(size_t)(weftShade * 3 + warpShade)].material())
                 .stroke(stroke(
                     1,
                     Fill::var(std::string_view(warpShade == weftShade ? "rule" : "ink")),
                     PathFormat::Align::Outer));
           })})});
    return titled(
        doc.phrase(words["heading"]),
        {box().row().gap(20).children(
            {std::move(table),
             box().column().gap(3).width(220).marginTop(14).children(
                 {each(words["notes"].array(),
                       [this](const data::Json& note) {
                         return document::caption(doc.phrase(note));
                       }),
                  document::caption(doc.phrase(words["law"]))
                      .styleClass("proof")
                      .marginTop(10),
                  document::paragraph(doc.phrase(words["reading"]))
                      .styleClass("reading")
                      .marginTop(6)})})});
  }

  // =========================================================================
  // One count, five shade cards: the colour is a variable, the count the
  // cloth's identity. The mark says which card the cloth wears now.

  Element shadeCards() const {
    std::vector<Element> rows;
    for (size_t index = 0; index < cards.size(); ++index) {
      const ShadeCard& card = cards[index];
      std::vector<SurfacePaint> swatches;
      std::vector<Utf8> labels;
      for (int shade : {K, B, G}) {
        swatches.push_back(wraps[index][(size_t)shade].material());
        labels.push_back(kit::formatted("%c %s", kShadeCodes[(size_t)shade],
                                        hexOf(card.shades[(size_t)shade])
                                            .c_str()));
      }
      Element row = box().row().paddingLeft(12).children(
          {document::caption(card.name).styleClass("card-name").width(146),
           sketch::kit::swatchStrip({.swatches = std::move(swatches),
                                     .labels = std::move(labels),
                                     .width = Dimension(82),
                                     .height = Dimension(14),
                                     .gap = 8})
               .styleClass("shades")});
      for (size_t turn = 0; turn < kTurns.size(); ++turn) {
        if ((size_t)kTurns[turn].card != index) continue;
        const float until = turn + 1 < kTurns.size()
                                ? kTurns[turn + 1].start + kFade
                                : 1.03f;
        row.children({box()
                          .left(0)
                          .top(-3)
                          .width(5)
                          .height(22)
                          .fill(Fill::var("proof"))
                          .opacity(sigil::motion::bind(loom, {.from = {kTurns[turn].start, until}, .envelope = sigil::motion::envelope::trapezoid(0, 0.12f, 0.88f, 1)}))});
      }
      rows.push_back(std::move(row));
    }
    return titled(doc.phrase(doc["palettes"]["heading"]),
                  {box().column().gap(14).children({rows})});
  }

  // =========================================================================
  // The argument: the same crop of the same cloth under four clan names,
  // and the cloth that carries one of those names honestly.

  Element swatch(const Pattern& cloth, SkPoint crop, SurfacePaint edge,
                 float edgeWidth) const {
    Pattern cut = cloth;
    cut.offset({-crop.x() * kThread, -crop.y() * kThread});
    // A specimen is a cutting, and cloth is cut with pinking shears so the
    // weave cannot run back from the edge.
    return box()
        .width(kSwatch.width())
        .height(kSwatch.height())
        .shape(shapes::shaped(shapes::chamfered(0),
                              shapers::Zigzag{.amplitude = 2, .wavelength = 7}))
        .overflow(Overflow::Clip)
        .background(
            styles::dropShadow(faded(colours.shadow, 0.45f), {2, 3}, 7))
        .fill(cut.material())
        .stroke(stroke(edgeWidth, std::move(edge), PathFormat::Align::Outer))
        .children({layer(grooves.material())
                       .blendMode(material::BlendMode::Multiply)
                       .opacity(0.85f)})
        .cache(Cache::Texture);
  }

  Element provenance() const {
    const data::Json& words = doc["provenance"];
    const auto labelled = [](Element picture, Element name) {
      return box().column().gap(6).alignItems(Align::Center).children(
          {std::move(picture), std::move(name)});
    };
    // Five cuttings in one row, the four the Cockburn Collection labelled
    // and the one that carries its name honestly; the words stand under
    // them at the row's two ends.
    return titled(
        doc.phrase(words["heading"]),
        {box().row().gap(16).children(
             {each(words["labels"].array(),
                   [&](const data::Json& label, size_t index) {
                     return labelled(
                         swatch(cloths.front(), kGovernmentCrop,
                                Fill::var("rule"), 1),
                         document::caption(doc.phrase(label))
                             .styleClass("name")
                             .opacity(sigil::motion::bind(loom, {.from = {0.63f + (float)index * 0.022f, 0.66f + (float)index * 0.022f}, .clampFrom = true})));
                   }),
              labelled(swatch(argyllCloth, kArgyllCrop, Fill::var("proof"),
                              1.5f),
                       document::caption(doc.phrase(words["honest"]))
                           .styleClass("name honest"))}),
         box().row().justifyContent(Justify::SpaceBetween).alignItems(Align::Start).children(
             {document::paragraph(doc.passage(words["quote"]))
                  .styleClass("quote")
                  .width(620),
              document::paragraph(doc.passage(words["note"]))
                  .styleClass("note")
                  .width(300)})});
  }

  // =========================================================================
  // Structure against numbers: the two setts, each normalised to its own
  // total, unit for unit. A tartan's identity is its structure; its
  // numbers are a matter of authority.

  Element comparison() const {
    const data::Json& words = doc["comparison"];
    constexpr float kBar = 846, kHeight = 26, kName = 130;
    const auto bar = [&](const Sett& sett, const data::Json& name) {
      const std::vector<Run> runs = sett.runs();
      const float total = (float)sett.threads().size();
      return box().row().alignItems(Align::Center).children(
          {box().row().width(kName).styleClass("bar-name").children(
               {document::caption(std::string(name.string())).width(100),
                document::caption(kit::formatted("%d", (int)total))}),
           box().row().width(kBar).height(kHeight).stroke(
               stroke(1, Fill::var("rule"), PathFormat::Align::Outer))
               .children({each(runs, [&](const Run& run) {
                 Element band =
                     box().width(kBar * (float)run.threads / total)
                         .flexShrink(0)
                         .fill(cards.front().shades[(size_t)run.shade]);
                 if (run.shade == Y || run.shade == W)
                   band.stroke(stroke(1.5f, Fill::var("proof"),
                                      PathFormat::Align::Outer));
                 return band;
               })})});
    };
    // The unit letters over both bars, and a line down through both at
    // every unit boundary.
    const float total = (float)verdict.total;
    Element bars = box().column().gap(8).children(
        {bar(watch, words["bars"][0]), bar(argyll, words["bars"][1])});
    float cumulative = 0;
    std::vector<Element> letters;
    for (size_t unit = 0; unit < watch.order.size(); ++unit) {
      const float share = kBar * (float)verdict.unitEnds[unit] / total;
      letters.push_back(box().width(share).alignItems(Align::Center).children(
          {document::caption(watch.order[unit]).styleClass("proof")}));
      cumulative += share;
      if (unit + 1 < watch.order.size())
        bars.children({box()
                           .left(kName + cumulative - 0.5f)
                           .top(-3)
                           .width(1)
                           .height(2 * kHeight + 14)
                           .fill(faded(colours.proof, 0.8f))});
    }
    return titled(
        doc.phrase(words["heading"]),
        {box().column().gap(6).children(
            {box().row().marginLeft(kName).children({letters}),
             std::move(bars),
             document::caption(doc.phrase(words["summary"]))
                 .styleClass("proof")
                 .marginLeft(kName)
                 .marginTop(4)})});
  }

  // =========================================================================

  /** Each check is a row: its name, its evidence, the figure it computed
   *  and the verdict. A reading has no verdict, and says so with a dash. */
  Element verification() const {
    std::vector<sketch::kit::Row> rows;
    for (const Proof& proof : proofs) {
      const measure::Check& check = proof.check;
      rows.push_back(
          {.cells = {proof.name, check.label, check.actual,
                     check.judged()
                         ? (check.pass ? "PASS" : "FAIL want " + check.expected)
                         : "—"},
           .swatch = Fill::var(!check.judged() ? "rule"
                               : check.pass    ? "ink"
                                               : "proof")});
    }
    const float reveal = (float)rows.size() * 0.0092f + 0.011f;
    return titled(
        doc.phrase(doc["verification"]["heading"]),
        {box()
             .padding(8, 12)
             .fill(faded(colours.well, 0.8f))
             .stroke(stroke(1, Fill::var("rule"), PathFormat::Align::Inner))
             .children({sketch::kit::table(
                            std::move(rows),
                            {.columns = {{.width = 104},
                                         {.width = 200},
                                         {.width = 56, .figure = true},
                                         {}}})
                            .opacity(sigil::motion::bind(loom, {.from = {kWeaveEnd, kWeaveEnd + reveal}, .clampFrom = true}))})})
        .flexGrow(1);
  }

  // =========================================================================

  Element describe() const {
    const data::Json& words = doc["masthead"];
    // Two columns on one grid: the cloth's width on the left, the rail's
    // on the right, and every row below the cloth keeps to the same two
    // edges — the specimens and the comparison to the cloth's, the
    // verification and the closing quotation to the rail's.
    constexpr float kRail = 440, kGutter = 24;
    return box()
        .width(kCanvas.width())
        .height(kCanvas.height())
        .applyStyleSheet(cardSheet(colours, kDraftCell))
        .padding(46, 64, 0, 64)
        .children({
            layer(board).cache(Cache::Texture),
            // The card lies under a window: light falls across it from the
            // upper left and the far corner sits in shade.
            box()
                .cover()
                .fill(material::Paint::radialGradient(
                    {0.1f * kCanvas.width(), 0}, 1.15f * kCanvas.width(),
                    {{0, {1, 1, 1, 0.14f}},
                     {0.35f, {1, 1, 1, 0}},
                     {0.6f, {0, 0, 0, 0}},
                     {1, {0, 0, 0, 0.11f}}},
                    {.units = material::GradientUnits::Pixels}))
                .cache(Cache::Texture),
            box().cover().inset(24).stroke(
                stroke(1, Fill::var("rule"), PathFormat::Align::Inner)),
            box().column().gap(26).children({
                box().column().children(
                    {masthead(),
                     kit::line({.fill = Fill::var("rule")}).marginTop(18)}),
                box().row().gap(kGutter).children(
                    {box().column().flexShrink(0).children(
                         {settBar(), mountedCloth()}),
                     box()
                         .column()
                         .width(kRail)
                         .flexShrink(0)
                         .gap(34)
                         .marginTop(26)
                         .children({draft(), blendTable(), shadeCards()})}),
                box()
                    .row()
                    .gap(kGutter)
                    .alignItems(Align::Start)
                    .children({provenance().flexGrow(1),
                               verification().width(kRail).flexGrow(0)}),
                box()
                    .row()
                    .gap(kGutter)
                    .alignItems(Align::Start)
                    .children({comparison().flexGrow(1),
                               document::paragraph(doc.passage("douglas"))
                                   .styleClass("douglas")
                                   .width(kRail)
                                   .marginTop(2)}),
            }),
            box().flexGrow(),
            kit::line({.fill = Fill::var("rule")}),
            document::footer(doc.phrase(words["colophon"]))
                .marginTop(6)
                .marginBottom(26),
        });
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) {
    doc = sketch::kit::Document(ctx, "data/content.json");
    const sketch::kit::Document setts(ctx, "data/setts.json");
    colours = readCard(doc["card"]);
    watch = readSett(setts["black watch"]);
    argyll = readSett(setts["campbell of argyll"]);
    cards = readShadeCards(setts["shades"]);
    weaveEverything();
    prove();
    // The still belongs to the hold in the first shade card, and is
    // declared: most of the loop shows a correct Black Watch in some other
    // registered card, which under a title reading GOVERNMENT looks like a
    // broken blend rather than the point.
    sketch::kit::stage(ctx, {.size = kCanvas,
                             .captureAt = 7.2,
                             .background = colours.ground});
    ctx.engine.add([this, &ticker = ctx.engine] {
      loom = phase(ticker.elapsed(), kCycle);
    });
    const sketch::kit::Provide look(cardTheme(colours));
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(BlackWatch, "Study · Pattern",
             "The Government sett — 24 integers and a mod-4 rule, 63,504 "
             "emergent cells")
