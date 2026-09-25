// nightingale_coxcomb.cpp — "Diagram of the Causes of Mortality in the
// Army in the East", Florence Nightingale, engraved 1858.
//
// REFERENCE
//   The two-wheel overlapping-sector polar-area plate ("the wedges";
//   later: coxcomb / rose diagram) from Notes on Matters Affecting the
//   Health … of the British Army (1858), printed as a lithograph in
//   Martineau & Nightingale, England and Her Soldiers (Smith, Elder &
//   Co., 1859). Printer's imprint: "Harrison & Sons, St. Martin's Lane."
//   Physical plate 19 x 35 cm.
//
//   Scan studied: David Rumsey Map Collection / Internet Archive item
//   dr_diagram-of-the-causes-of-mortality-in-the-army-in-the-east-10563002.
//   The blue-ink bounding boxes of both wheels fit ONE radius constant:
//   the two wheels share one scale.
//
//   Data: Nightingale's own published figures via HistData::Nightingale
//   (R), rate = 12 * 1000 * deaths / army = "annual rate of mortality
//   per 1000". Radius law r = k*sqrt(rate): at a fixed 30 deg wedge only
//   a square-root radius makes AREA proportional to the number of dead.
//
// WHAT THE PLATE DOES THAT REPRODUCTIONS USUALLY GET WRONG
//   1. Month labels are NOT on a common label ring. Each hugs its OWN
//      wedge's rim, with a floor so the tiny spring months clear the hub.
//      That scalloped label ring is most of what makes the plate read as
//      engraved rather than plotted.
//   2. The lower-half labels are NOT flipped. Glyph-up points radially
//      OUTWARD everywhere, so DECEMBER, JANUARY, FEBRUARY and the left
//      wheel's "1856" come out upside down.
//   3. The two campaign annotations (BULGARIA, CRIMEA) run ALONG their
//      spoke, not round the rim like the months.
//
// THE SHEET AS AN OBJECT
//   It is a folded plate bound into a book, so the paper carries what a
//   bound sheet carries: a raking light across it, the gutter's valley at
//   the fold, the pressed field inside the plate mark, foxing, and the
//   running head of the page behind it showing through. The tints were
//   printed from their own stones and the key outlines from another, so
//   the key sits a hair off the tints, as a registered lithograph does.
//
// The capture at 13.6 s is the settled plate. Earlier moments show the
// argument being made: 2.2 s is diagram 1 growing clockwise out of July
// 1854; from 9.4 s two brass index needles read each year round.

// TAGS: Data/Charts

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ground.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/scale/Scale.h>
#include <sigildata/table/Table.h>
#include <sigildraw/Color.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/schedule/Spread.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Transition.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Chart.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/layout/ParagraphStyle.h>
#include <sigilweave/style/Length.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace data = sigil::data;
namespace document = sigil::compose::document;
namespace draw = sigil::draw;
namespace field = sigil::material::field;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace patterns = sigil::material::pattern;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace sigil::motion;
using namespace sigil::weave::literals;
using namespace std::chrono_literals;
using sigil::material::skia::Paint;
namespace ch = choreograph;

namespace {

// ---------------------------------------------------------------------------
// THE SHEET'S GRID. The canvas keeps the plate's 35:19. Both hubs stand on
// one row, the titles and captions on two rows above them, and the legend
// and the imprint close the sheet on a shared bottom line inside the
// plate mark.

constexpr SkSize kCanvas{1900, 1032};
constexpr float kPlateMark = 22.0f;  // inset of the copper's impression
constexpr float kAxis = 950.0f;      // the fold, and the titles' centre
constexpr float kTitleRow = 56.0f;
constexpr float kCaptionRow = 94.0f;
constexpr float kHubRow = 396.0f;
constexpr float kLegendLeft = 170.0f;
constexpr float kLegendTop = 616.0f;
constexpr float kLegendMeasure = 720.0f;
constexpr float kBottomLine = 980.0f;  // the last line the sheet sets
constexpr float kRightMargin = kPlateMark + 40.0f;

// ONE SCALE FOR BOTH WHEELS, and the plate's whole argument: a wedge's AREA
// is its death rate, so the radius goes as the root of the rate. A rate of
// 1 per 1000 stands 17 px off the centre; both diagrams read against that
// one rule, which is what lets the second year be compared with the first.
const data::Scale kRadius{
    .domain = {0, 1}, .range = {0, 17}, .transform = data::Transform::Sqrt};

float radiusOf(float rate) { return (float)kRadius(rate); }

// The paper sliver between two blue wedges, in degrees of the 30 deg pitch.
constexpr float kBlueGapDegrees = 0.9f;
// How far a hairline radial runs, as a fraction of the wheel's rim: on the
// lithograph they live inside the central cluster and never cross the blue.
constexpr float kSpokeReach = 0.24f;
// How far the key stone printed off the tint stones, px.
constexpr SkVector kRegistration{0.9f, -0.6f};

// The plate's coordinate convention: 0 deg is 12 o'clock and bearings run
// clockwise, because the year starts at the top and reads round to the
// right. Radius 1, so a node's own box scales it.
constexpr path::PolarFrame kPlate{.centre = {0, 0},
                                  .radius = 1.0f,
                                  .zero = path::Zero::North,
                                  .sense = path::Sense::CW};

// ---------------------------------------------------------------------------
// The data, read from the files beside this sketch. The rows are in WHEEL
// order: the engraver's seam is the June/July boundary at 12 o'clock, so a
// wheel runs July..June even though the report year runs April..March.

struct Month {
  std::string label;  // the outer line
  std::string year;   // the inner line, where the plate letters one
  float disease, wounds, other;  // annual rate per 1000
  float largest() const { return std::max({disease, wounds, other}); }
};

std::vector<Month> readWheel(const data::Table& table, double wheel) {
  const auto which = table.column<double>("wheel");
  const auto label = table.column<std::string>("label");
  const auto disease = table.column<double>("disease");
  const auto wounds = table.column<double>("wounds");
  const auto other = table.column<double>("other");
  std::vector<Month> months;
  for (size_t row = 0; row < which.size(); ++row) {
    if (which[row] != wheel) continue;
    const size_t lineBreak = label[row].find('\n');
    months.push_back(
        {label[row].substr(0, lineBreak),
         lineBreak == std::string::npos ? std::string()
                                        : label[row].substr(lineBreak + 1),
         (float)disease[row], (float)wounds[row], (float)other[row]});
  }
  return months;
}

/** ONE OF THE TWO DIAGRAMS: its months, where its hub stands, how its
 *  labels clear the hub, and when each part of it arrives (seconds). */
struct Diagram {
  std::string name;  // the class every part of it is set under
  std::vector<Month> months;
  SkPoint hub;
  float labelFloor, labelGap, labelLineStep;
  float spokes, wedges, wedgeStep, labels, needle, needleEnd;

  /** The wheel is drawn at its largest rate, so its box is that radius on
   *  the one scale and the rate domain follows. */
  float largestRate() const {
    float largest = 0;
    for (const Month& month : months) largest = std::max(largest, month.largest());
    return largest;
  }
  float rim() const { return radiusOf(largestRate()); }
  /** The box the wheel is inscribed in, in the sheet's space. */
  SkRect box(float radius) const {
    return path::PolarFrame{.centre = hub, .radius = radius}.box();
  }
};

/** THE SPOKE a line is drawn or lettered along: one tick of a
 *  one-division ladder at @p bearing, from @p inner to @p outer of the
 *  node's own radius — a straight baseline AND a comparable value. */
shapes::TicksShape spoke(float bearing, float inner, float outer) {
  return shapes::ticks({.divisions = 1, .from = bearing, .mark = {inner, outer}},
                       kPlate);
}

/** THE MONTHS' BASELINE: a clockwise ring from 12 o'clock, so a run's
 *  arc-length fraction IS its bearing over 360, and glyph-up points
 *  radially outward — the engraver's one convention on this sheet. */
const shapes::Circle kRimBaseline{.startIndex = 0};

/** The paint of a tint stone: the paper-side wash and the ink dot laid
 *  over it — a fine stipple for the tint, a coarse sparse one so the ink
 *  density wanders, and a luminance grain for the stone's own unevenness.
 *  The stipple's tile is larger than the eye's patch, so its repeat never
 *  reads as a motif. */
Paint tintStone(material::Color wash, material::Color ink, int fine,
                int coarse, uint32_t seed, Pattern& stipple, Pattern& blot) {
  stipple = patterns::speckle(128, fine * 10, 0.25f, 0.66f, {ink});
  stipple.seed(seed);
  blot = patterns::speckle(320, coarse * 8, 1.8f, 5.0f,
                           {material::withAlpha(ink, 0.12f)});
  blot.seed(seed * 7 + 3);
  return Paint::blend(
      {{Paint::solid(wash), SkBlendMode::kSrc},
       {stipple.material(), SkBlendMode::kSrcOver},
       {blot.material(), SkBlendMode::kSrcOver},
       {Paint::recipe(field::grain(0.010f, 3, (float)seed)),
        SkBlendMode::kSoftLight}});
}

}  // namespace

// ===========================================================================

struct NightingaleCoxcomb {
  Diagram first, second;
  std::vector<std::string> legend;
  std::map<std::string, material::Color> palette;

  // Held so their identity, and the stipple's bake, survive re-describes.
  std::array<Pattern, 6> stipples;
  Pattern foxing;
  Paint disease, wounds, other;

  material::Color colour(const std::string& name) const {
    const auto found = palette.find(name);
    return found == palette.end() ? material::Color{0, 0, 0, 1} : found->second;
  }
  /** The palette's @p name at @p share of its own strength. */
  material::Color colour(const std::string& name, float share) const {
    material::Color faded = colour(name);
    faded.a *= share;
    return faded;
  }

  // ------------------------------------------------------------------
  /** HOW THE SHEET IS SET. The tokens are the palette file's; the type is
   *  three hands — an engraved inline roman for the title, an engraver's
   *  copperplate capital for every label, a roundhand for the legend and
   *  the imprint — and the root states the one size the scale is in rems
   *  of. The display lines carry a stroke under their fill in the ink,
   *  because the plate's lettering is heavier than any installed face. */
  StyleSheet sheet() const {
    const std::string engraved = "Academy Engraved LET, Bodoni 72, serif";
    const std::string capitals = "Copperplate, Helvetica Neue, sans-serif";
    const std::string roundhand = "Snell Roundhand, Apple Chancery, cursive";
    Rule root = rule(":root");
    for (const auto& [name, value] : palette) root.var(name, value);
    return StyleSheet{
        root.fontFamily(capitals).fontSize(20).ink(var("ink")),
        rule("h1")
            .fontFamily(engraved)
            .fontSize(1.8_rem)
            .letterSpacing(0.8)
            .textStroke(1.1f, Fill::currentInk()),
        rule("h2, caption")
            .fontWeight(700)
            .letterSpacing(0.4)
            .textStroke(0.9f, Fill::currentInk()),
        rule("h2").fontSize(1.35_rem),
        rule("caption").fontSize(1.05_rem),
        rule(".numeral").fontWeight(700).fontSize(1.2_rem),
        rule(".first label").fontSize(1_rem).letterSpacing(0.4).textStroke(
            0.35f, Fill::currentInk()),
        // The small wheel's lettering is cut through whatever line it
        // crosses, as an engraver breaks a rule for a word.
        rule(".second label")
            .fontSize(0.6_rem)
            .letterSpacing(0)
            .textStroke(4.0f, Fill::var("paper")),
        rule(".campaign").fontSize(0.8_rem).letterSpacing(1.9),
        rule(".verso")
            .fontFamily(engraved)
            .fontSize(1.1_rem)
            .letterSpacing(1.5)
            .ink(var("verso")),
        rule("paragraph")
            .fontFamily(roundhand)
            .fontSize(1.3_rem)
            .lineHeight(weave::Leading::absolute(30.5f))
            .textWrap(TextWrap::Pretty)
            .textIndent(-22)
            .paddingLeft(22),
        rule("footer").fontFamily(roundhand).fontSize(1_rem).ink(
            var("ink-soft")),
        rule(".spoke").ink(var("ink-soft")),
        rule(".tint .disease").fill(disease),
        rule(".tint .wounds").fill(wounds),
        rule(".tint .other").fill(other),
        rule(".key").ink(var("ink")),
        rule(".flash").ink(var("flash")),
        rule(".needle").ink(var("brass")),
        rule(".leader").ink(var("ink")),
    };
  }

  // ------------------------------------------------------------------
  /** THE PAPER: a grained cream, sparse foxing and the corners darkened,
   *  then the sheet as a bound object — the field inside the plate mark
   *  pressed smooth by the stone, the gutter's valley at the fold, and a
   *  light raking across from the upper left. Static, so it is one bake. */
  Element paper() const {
    const float gutter = 180.0f;
    return stack()
        .inset(0)
        .fill(kit::grained(colour("paper"), 0.05f, 0.011f))
        .children({
            box().inset(0).fill(foxing.material()),
            box().inset(0).fill(kit::vignette(kCanvas, colour("umber"), 0.62f)),
            box()
                .inset(kPlateMark + 2)
                .fill(Fill::color(material::withAlpha(colour("paper"), 0.28f))),
            box().inset(0).fill(linearGradient(
                {0, 0}, {kCanvas.width(), kCanvas.height()},
                {colour("raking-light"), colour("raking-light", 0),
                 colour("raking-shade")},
                {0.0f, 0.45f, 1.0f})),
            box()
                .rect(SkRect::MakeXYWH(kAxis - gutter / 2, 0, gutter,
                                       kCanvas.height()))
                .fill(linearGradient(
                    {0, 0}, {gutter, 0},
                    {colour("gutter-shadow", 0), colour("gutter-shadow", 0.45f),
                     colour("gutter-shadow"), colour("gutter-light"),
                     colour("gutter-shadow", 0.35f), colour("gutter-shadow", 0)},
                    {0.0f, 0.38f, 0.485f, 0.515f, 0.6f, 1.0f})),
            // The plate mark: the copper's impression, a shadowed edge
            // and a lit one beside it.
            box().inset(kPlateMark).stroke(stroke(1.0f, Fill::var("plate-shadow"))),
            box()
                .inset(kPlateMark + 2)
                .stroke(stroke(1.0f, Fill::var("plate-light"))),
            // The running head of the page behind, read through the sheet
            // and so reversed.
            text("ENGLAND AND HER SOLDIERS.")
                .styleClass("verso")
                .scaleX(-1)
                .centerAt({775, 300}),
        })
        .cache(Cache::Texture)
        .key("paper");
  }

  // ------------------------------------------------------------------
  /** THE TITLE BLOCK on the sheet's two top rows: each diagram's number
   *  over its caption, centred over its own hub, and the title over the
   *  subtitle on the fold, closed by a double rule. The pen writes the
   *  title and subtitle; the captions arrive with their wheels. */
  Element titles() const {
    const auto writing = [](float startMs, float spanMs, float durationMs) {
      return Track{.effect = textFx::typeOn(),
                   .stagger = {.eachMs = 0, .amountMs = spanMs, .durationMs = 40},
                   .progress = animate(from(0.0f).to(1.0f),
                                       ramp(startMs, durationMs, ch::easeNone))};
    };
    const auto arrive = [](float startMs) {
      return animate(from(0.0f).to(1.0f), ramp(startMs, 320));
    };
    const auto numbered = [&](const Diagram& diagram, const char* number,
                              const char* caption, float startMs) {
      return box().cover().children({
          text(number).styleClass("numeral").centerAt(
              {diagram.hub.x(), kTitleRow}).opacity(arrive(startMs)),
          document::caption(caption)
              .centerAt({diagram.hub.x(), kCaptionRow})
              .opacity(arrive(startMs + 90)),
      });
    };
    // An echo takes its colour as a value and not as the ink in force, so
    // the doubled pass under each display line reads the palette's ink.
    return box().cover().children({
        document::h1("DIAGRAM of the CAUSES of MORTALITY")
            .textFx(writing(0, 620, 700))
            .layerStyle(LayerStyle::echo({0.8f, 0.5f},
                                         material::withAlpha(colour("ink"), 0.8f)))
            .centerAt({kAxis, kTitleRow}),
        document::h2("in the ARMY in the EAST.")
            .textFx(writing(900, 340, 400))
            .layerStyle(LayerStyle::echo({0.6f, 0.4f},
                                         material::withAlpha(colour("ink"), 0.7f)))
            .centerAt({kAxis, kCaptionRow}),
        kit::line({.length = Dimension(368),
                   .pair = kit::Line::Companion{.thickness = 1.0f, .gap = 3.0f}})
            .at({kAxis - 184, kCaptionRow + 18})
            .transformOrigin(pct(0), pct(50))
            .scaleX(animate(from(0.0f).to(1.0f),
                            ramp(1120, 420, ch::easeOutQuint))),
        numbered(first, "1.", "APRIL 1854 to MARCH 1855.", first.spokes * 1000 - 150),
        numbered(second, "2.", "APRIL 1855 to MARCH 1856.", second.spokes * 1000 - 150),
    });
  }

  // ------------------------------------------------------------------
  /** THE WEDGES OF ONE STONE. A month owns a band of the sweep and a wedge
   *  is that band grown out to the radius its rate stands at, so nothing
   *  here turns a rate into a pixel. The three causes are painted per month
   *  BIGGEST FIRST, so every band shows its own colour with no stacking
   *  arithmetic, and declaration order is that painter's order.
   *
   *  The TINT stone lays the three washes; the KEY stone outlines the red
   *  and black wedges only — on the plate the blue tint simply stops, and a
   *  hairline round every wedge is what makes a sheet read as a modern
   *  vector chart. Each wedge grows out of the hub on its month's beat and
   *  is baked once: only its scale moves. */
  Element stone(const Diagram& diagram, bool key) const {
    struct Wedge {
      sketch::kit::Datum datum;
      std::string cause;
      int month;
    };
    std::vector<Wedge> wedges;
    for (int month = 0; month < 12; ++month) {
      const Month& rates = diagram.months[month];
      std::array<Wedge, 3> causes{{{{(double)month, rates.disease}, "disease", month},
                                   {{(double)month, rates.wounds}, "wounds", month},
                                   {{(double)month, rates.other}, "other", month}}};
      std::ranges::sort(causes, std::greater{},
                        [](const Wedge& wedge) { return wedge.datum.y; });
      for (const Wedge& wedge : causes)
        if (wedge.datum.y > 0 && !(key && wedge.cause == "disease"))
          wedges.push_back(wedge);
    }
    const auto part = [wedges, diagram, key](std::size_t index) {
      const Wedge& wedge = wedges[index];
      Element shape = box().styleClass(wedge.cause);
      if (key) shape.stroke(stroke(1.0f));
      const float delay =
          (diagram.wedges + diagram.wedgeStep * (float)wedge.month) * 1000.0f;
      return std::move(shape)
          .scale(animate(from(0.002f).to(1.0f),
                         ramp(delay, 620.0f, ch::easeOutExpo)))
          .cache(Cache::Texture);
    };
    std::vector<sketch::kit::Datum> data;
    for (const Wedge& wedge : wedges) data.push_back(wedge.datum);
    Element plate =
        sketch::kit::plot(
            diagram.name + (key ? "-key" : "-tint"),
            {.x = {.transform = data::Transform::Band,
                   .steps = 12,
                   .padding = kBlueGapDegrees / 30.0f},
             .y = {.domain = {0, diagram.largestRate()},
                   .transform = data::Transform::Sqrt},
             .polar = sketch::kit::Polar{.sweep = {-90, 270}}},
            {sketch::kit::bands(data, {.x = &sketch::kit::Datum::x,
                                       .y = &sketch::kit::Datum::y,
                                       .part = part,
                                       .styleClass = key ? "key" : "tint"})})
            .rect(diagram.box(diagram.rim()));
    if (key) plate.translateX(kRegistration.x()).translateY(kRegistration.y());
    return plate;
  }

  /** THE TWELVE HAIRLINE RADIALS. A radial exists only where both
   *  neighbouring months have ink, so each runs out to the smaller of its
   *  two months' rims, and never past the central cluster. One node each,
   *  so each draws itself on its own beat. */
  Element radials(const Diagram& diagram) const {
    const float rim = diagram.rim();
    const auto rimOf = [&](int month) {
      return radiusOf(std::max(diagram.months[(month + 12) % 12].largest(), 0.1f));
    };
    Element wheel = box().rect(diagram.box(rim)).styleClass("spoke");
    for (int month = 0; month < 12; ++month) {
      const float length =
          std::min({rimOf(month - 1), rimOf(month), rim * kSpokeReach}) * 0.98f;
      if (length < 4.0f) continue;
      wheel.children({box()
                          .inset(0)
                          .shape(spoke((float)month * 30.0f, 0.0f, length / rim))
                          .stroke(spans::upTo(animate(
                                      from(0.0f).to(1.0f),
                                      ramp(diagram.spokes * 1000.0f + month * 16.0f,
                                           220.0f))),
                                  stroke(0.7f))});
    }
    return wheel;
  }

  /** THE INDEX NEEDLE that reads a wheel round once, clockwise from its
   *  seam, and the flash it rings out of each month's rim as it passes. */
  Element needle(const Diagram& diagram) const {
    const float rim = diagram.rim();
    const auto at = [](float seconds) {
      return std::chrono::milliseconds((int)(seconds * 1000.0f));
    };
    const float start = diagram.needle, end = diagram.needleEnd;
    const float sweep = end - start;
    Element reading = box().rect(diagram.box(rim));
    for (int month = 0; month < 12; ++month) {
      const float passes = start + sweep * ((float)month * 30.0f + 15.0f) / 360.0f;
      const float halfWidth = sweep * 13.0f / 360.0f;
      const float rimOfMonth = radiusOf(std::max(diagram.months[month].largest(), 1.0f)) + 10.0f;
      reading.children(
          {box()
               .rect(path::PolarFrame{.centre = {rim, rim}, .radius = rimOfMonth}.box())
               .styleClass("flash")
               .shape(shapes::arc((float)month * 30.0f - 90.0f + 1.0f, 28.0f))
               .stroke(stroke(2.4f))
               .opacity(animate(through({{0ms, 0.0f},
                                         {at(passes - halfWidth), 0.0f},
                                         {at(passes), 1.0f},
                                         {at(passes + halfWidth), 0.0f}}),
                                ch::easeNone))});
    }
    reading.children(
        {box()
             .inset(0)
             .styleClass("needle")
             .shape(spoke(0.0f, 0.0f, 1.0f))
             .stroke(stroke(1.4f))
             // A shadow takes its colour as a value, so the glow names the
             // palette's brass rather than the needle's ink.
             .background(shadow(colour("brass-glow"), {0, 0}, 9))
             .transformOrigin(pct(50), pct(50))
             .rotate(animate(through({{0ms, 0.0f}, {at(start), 0.0f}, {at(end), 360.0f}}),
                             ch::easeNone))
             .opacity(animate(through({{0ms, 0.0f},
                                       {at(start), 0.0f},
                                       {at(start + 0.15f), 1.0f},
                                       {at(end), 1.0f},
                                       {at(end + 0.45f), 0.0f}}),
                              ch::easeNone))});
    return reading;
  }

  // ------------------------------------------------------------------
  /** ONE LABEL ON ITS OWN RING: a leaf the ring's diameter across,
   *  standing on the hub, its run shaped once and laid along the rim. */
  static Element onRing(Text label, const Diagram& diagram, float bearing,
                        float radius, float delayMs) {
    return std::move(label)
        .width(2 * radius)
        .height(2 * radius)
        .centerAt(diagram.hub)
        .textOnPath({.path = kRimBaseline,
                     .at = bearing / 360.0f,
                     .align = TextPath::Align::Center,
                     .autoFlip = false,
                     .orient = TextPath::Orient::Tangent})
        .opacity(animate(from(0.0f).to(1.0f), ramp(delayMs, 260.0f)));
  }

  /** A CAMPAIGN ANNOTATION, run along its spoke and centred @p radius
   *  from the hub. */
  static Element alongSpoke(const Diagram& diagram, const char* words,
                            float bearing, float radius, float delayMs) {
    const float reach = 120.0f;
    const float box = radius + reach;
    return text(words)
        .styleClass("campaign")
        .width(2 * box)
        .height(2 * box)
        .centerAt(diagram.hub)
        .textOnPath({.path = spoke(bearing, (radius - reach) / box,
                                   (radius + reach) / box),
                     .at = 0.5f,
                     .align = TextPath::Align::Center,
                     .autoFlip = false,
                     .orient = TextPath::Orient::Tangent})
        .opacity(animate(from(0.0f).to(1.0f), ramp(delayMs, 260.0f)));
  }

  /** THE MONTHS, each hugging its own wedge's rim. The floor is not
   *  decoration: twelve labels must fit the circumference they sit on, so
   *  the ring cannot close tighter than twelve widest labels — which is
   *  why the plate sets the small wheel in a smaller size. */
  Element months(const Diagram& diagram) const {
    Element ring = box().cover();
    for (int month = 0; month < 12; ++month) {
      const Month& rates = diagram.months[month];
      const float base = std::max(radiusOf(std::max(rates.largest(), 0.5f)) +
                                      diagram.labelGap,
                                  diagram.labelFloor);
      const float bearing = (float)month * 30.0f + 15.0f;
      const float delay = (diagram.labels + (float)month * 0.028f) * 1000.0f;
      const bool twoLines = !rates.year.empty();
      ring.children({onRing(document::label(rates.label), diagram, bearing,
                            base + (twoLines ? diagram.labelLineStep : 0.0f),
                            delay)});
      if (twoLines)
        ring.children({onRing(document::label(rates.year), diagram, bearing,
                              base, delay + 60.0f)});
    }
    return ring;
  }

  /** ONE DIAGRAM, back to front: the hairline radials, the tint stone,
   *  the key stone over it, the lettering, and the needle that reads it. */
  Element wheel(const Diagram& diagram) const {
    return box().cover().styleClass(diagram.name).children({
        radials(diagram),
        stone(diagram, false),
        stone(diagram, true),
        months(diagram),
        needle(diagram),
    });
  }

  // ------------------------------------------------------------------
  /** THE DASHED LEADER that carries the reader from the end of the first
   *  year to the start of the second: from the March 1855 wedge's outer
   *  corner at nine o'clock on diagram 1, down under the gap between the
   *  wheels, to the April 1855 wedge's on diagram 2. */
  Element leader() const {
    const SkPoint yearEnds =
        kPlate.about(first.hub).px(270.0f, radiusOf(first.months[8].disease));
    const SkPoint yearBegins =
        kPlate.about(second.hub).px(270.0f, radiusOf(second.months[9].disease));
    const SkPoint knee{(yearEnds.x() + yearBegins.x()) * 0.5f, kHubRow + 138.0f};
    PathFormat dashed = stroke(1.1f);
    dashed.dashIntervals = {7.0f, 5.0f};
    return box()
        .inset(0)
        .styleClass("leader")
        .shape(heldPath(path::toPath(path::Polyline{
            .points = {{yearBegins.x(), yearBegins.y()},
                       {knee.x(), knee.y()},
                       {yearEnds.x(), yearEnds.y()}}})))
        .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                    ramp(6000, 620, ch::easeOutQuad))),
                dashed);
  }

  /** THE LEGEND in the engraver's roundhand: one passage of five
   *  sentences, each hung at the margin with its run-on lines indented,
   *  on one leading. A pen writes it line by line, each line starting
   *  a fifth of a second after the one above. */
  Element key() const {
    std::string words;
    for (const std::string& sentence : legend)
      words += (words.empty() ? "" : "\n") + sentence;
    const Spread pen = Spread{.eachMs = 200}.then({.amountMs = 620, .durationMs = 30});
    return document::paragraph(words)
        .at({kLegendLeft, kLegendTop})
        .width(kLegendMeasure)
        .textFx({.effect = textFx::typeOn(),
                 .stagger = pen,
                 .unit = weave::Unit::Line,
                 .innerUnit = weave::Unit::Cluster,
                 .progress = animate(from(0.0f).to(1.0f),
                                     ramp(6400, pen.spanMs(12, 70), ch::easeNone))});
  }

  // ------------------------------------------------------------------
  Element describe() const {
    return stack()
        .applyStyleSheet(sheet())
        .fill(Fill::var("paper"))
        .children({
            paper(),
            titles(),
            leader(),
            wheel(second),
            wheel(first),
            key(),
            document::footer("Harrison & Sons, St. Martin's Lane.")
                .right(kRightMargin)
                .bottom(kCanvas.height() - kBottomLine)
                .opacity(animate(from(0.0f).to(1.0f), ramp(8900, 600))),
            // The two campaign annotations along their spokes.
            alongSpoke(first, "BULGARIA", 358.0f, 150.0f, 3480),
            alongSpoke(first, "CRIMEA", 87.0f, 268.0f, 3560),
            // The left wheel's year marker, upside down at six o'clock —
            // the outward-up rule arriving at the bottom of the circle.
            box().cover().styleClass(second.name).children(
                {onRing(document::label("1856"), second, 180.0f, 134.0f, 5750)}),
        });
  }

  void setup(sketch::SketchContext& ctx) {
    // A colour is read from its CSS text by the p5 canvas library's parser,
    // the one such parser the libraries hold.
    if (const auto colours = ctx.assets.table(ctx.local("data/palette.csv"))) {
      const auto names = colours->column<std::string>("name");
      const auto values = colours->column<std::string>("colour");
      for (size_t row = 0; row < names.size(); ++row)
        palette[names[row]] = draw::parseColor(values[row]);
    }
    if (const auto deaths = ctx.assets.table(ctx.local("data/deaths.csv"))) {
      first = {.name = "first",
               .months = readWheel(*deaths, 1),
               .hub = {1397, kHubRow},
               .labelFloor = 172,
               .labelGap = 26,
               .labelLineStep = 24,
               .spokes = 1.15f,
               .wedges = 1.35f,
               .wedgeStep = 0.115f,
               .labels = 3.10f,
               .needle = 9.40f,
               .needleEnd = 11.40f};
      second = {.name = "second",
                .months = readWheel(*deaths, 2),
                .hub = {430, kHubRow},
                .labelFloor = 160,
                .labelGap = 14,
                .labelLineStep = 14,
                .spokes = 3.85f,
                .wedges = 4.05f,
                .wedgeStep = 0.100f,
                .labels = 5.45f,
                .needle = 11.50f,
                .needleEnd = 13.10f};
    }
    if (const auto sentences = ctx.assets.table(ctx.local("data/legend.csv")))
      for (const auto& sentence : sentences->column<std::string>("sentence"))
        legend.emplace_back(sentence);

    disease = tintStone(colour("disease-wash"), colour("disease-ink"), 1150, 14,
                        11, stipples[0], stipples[1]);
    wounds = tintStone(colour("wounds-wash"), colour("wounds-ink"), 900, 10, 23,
                       stipples[2], stipples[3]);
    other = tintStone(colour("other-wash"), colour("other-ink"), 900, 18, 37,
                      stipples[4], stipples[5]);
    foxing = patterns::speckle(190, 4, 1.5f, 6.5f, {colour("fox")});
    foxing.seed(91);

    // The still is the first clean instant after the second needle has
    // faded: every entrance has finished and the plate holds unchanged.
    sketch::kit::stage(ctx, {.size = kCanvas,
                             .captureAt = 13.6,
                             .background = colour("paper")});
    ctx.composer.render(describe());
  }

  void update(double, sketch::SketchContext&) {}
};

SIGIL_SKETCH(NightingaleCoxcomb, "Study · Science",
             "Nightingale's 1858 coxcomb — polar-area wedges from the real "
             "mortality table")
