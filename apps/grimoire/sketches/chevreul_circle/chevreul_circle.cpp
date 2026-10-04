// Michel-Eugène Chevreul's first chromatic circle, Plate V of "Des couleurs
// et de leurs applications aux arts industriels à l'aide des cercles
// chromatiques" (J.-B. Baillière et fils, Paris, 1864), engraved by Digeon
// and printed by Lamoureux.
//
// The plate, from the centre out: a paper medallion carrying the title in
// six lines of display type; seventy-two blades of flat colour, the
// "couleurs franches", each a five-degree sector with a sliver of paper
// between it and its neighbour; and a grey limb, one cell per blade, that
// names the twelve principal scales and numbers the five intermediates
// between each pair of them 1 to 5. ROUGE stands at the foot of the plate
// and the numbering runs anticlockwise, through ORANGÉ on the right, JAUNE,
// VERT at the head, BLEU and VIOLET on the left. The limb is lettered from
// inside: every label stands with its head toward the centre, so the scale
// reads upright at the foot and upside down at the head.
//
// Every blade's colour and its limb label stand in data/colours.csv, one row
// per sector; every word of the engraving stands in data/content.json.

// TAGS: Materials/Color

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigildata/table/Table.h>
#include <sigilgeometry/kit/Divisions.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace field = sigil::material::field;
using sigil::material::Paint;
using namespace sigil::compose;
using sigil::material::hexColor;

namespace {

// Laid paper, the faint tone the inked copper leaves inside the platemark,
// and the engraver's greys.
const auto kPaper = hexColor(0xF3EEE2);
const auto kPlate = hexColor(0xEEE8DA);
const auto kInk = hexColor(0x2E2B27);
const auto kEngraving = hexColor(0x6E6A63);
const auto kLimb = hexColor(0xD9D5CB);
const auto kHairline = hexColor(0x8F8A80);
const auto kMedallion = hexColor(0xEAE6DC);

constexpr float kWidth = 1200, kHeight = 1500;

// The platemark, and the head and foot rules the running head and the
// imprint stand between.
constexpr float kPlateLeft = 94, kPlateTop = 150;
constexpr float kPlateWidth = 1012, kPlateHeight = 1195;
constexpr float kBandDepth = 22;

// The circle, in units of the limb's outer edge. Angles are the plate's own:
// zero at ROUGE, straight down, increasing anticlockwise as the sector
// number does, five degrees to a sector.
const path::PolarFrame kCircle{.centre = {600, 722},
                               .radius = 496,
                               .zero = path::Zero::North,
                               .sense = path::Sense::CCW,
                               .originDeg = 180};
constexpr float kLimbInner = 0.953f;
constexpr float kBladeOuter = 0.946f;
constexpr float kMedallionOuter = 0.362f;
constexpr float kMedallionRing = 0.344f;
constexpr float kSectorDegrees = 5.0f;
// The paper left between two blades, in degrees: a fixed share of the
// pitch, so the gap widens toward the rim as the blades do.
constexpr float kBladeGap = 0.9f;

/** One of the seventy-two sectors: its limb label and its colour. */
struct Sector {
  std::string label;
  sigil::material::Color colour;
};

/** A limb label: a named scale is set as one word to a line. */
std::string stacked(std::string label) {
  for (char& letter : label)
    if (letter == ' ') letter = '\n';
  return label;
}

sigil::weave::Face face(
    std::initializer_list<const char*> families,
    sigil::weave::FaceStyle style = sigil::weave::FaceStyle{}) {
  return weave::ports::face(families, style);
}

struct ChevreulCircle {
  std::vector<Sector> sectors;
  sketch::kit::Document words;
  Paint paperGrain;

  /** THE BLADES: sector n, centred on n × 5°, cut short of its neighbours
   *  by the paper gap and running from the medallion to the limb. */
  Element blades() const {
    const float half = (kSectorDegrees - kBladeGap) * 0.5f;
    return box().inset(0).children(
        each(sectors, [half](const Sector& sector, size_t number) {
          const float centre =
              kCircle.screenDegrees(kSectorDegrees * (float)number);
          return kit::disc(kCircle, kBladeOuter)
              .shape(shapes::sector(centre - half, 2 * half,
                                    kMedallionOuter / kBladeOuter))
              .fill(Fill::color(sector.colour));
        }));
  }

  /** THE LIMB: a grey band ruled at both edges and between every cell,
   *  each cell lettered with its sector's label, head toward the centre. */
  Element limb() const {
    const float middle = (1.0f + kLimbInner) * 0.5f;
    const float cellWidth = 2.0f * 3.14159265f * kCircle.radius * middle / 72;
    return box().inset(0).children(
        {kit::disc(kCircle)
             .shape(shapes::annulus(kLimbInner))
             .fill(Fill::color(kLimb)),
         kit::ring(kCircle.centre, kCircle.radius,
                   stroke(1.0f, Fill::color(kHairline))),
         kit::ring(kCircle.centre, kCircle.radius * kLimbInner,
                   stroke(0.8f, Fill::color(kHairline))),
         kit::disc(kCircle)
             .shape(shapes::ticks({.divisions = 72,
                                   .from = kSectorDegrees * 0.5f,
                                   .mark = {kLimbInner, 1.0f}},
                                  kCircle))
             .fill(Fill::none())
             .stroke(stroke(0.6f, Fill::color(kHairline))),
         box().inset(0).children(each(
             sectors, [middle, cellWidth](const Sector& sector, size_t number) {
               const float degrees = kSectorDegrees * (float)number;
               const bool named = number % 6 == 0;
               return text(stacked(sector.label))
                   .styleClass(named ? "scale" : "numeral")
                   .paragraph({.alignment = weave::TextAlignment::kCenter})
                   .width(cellWidth)
                   .centerAt(kCircle.at(degrees, middle))
                   .transformOrigin(pct(50), pct(50))
                   .rotate(kCircle.screenDegrees(degrees) - 90.0f);
             }))});
  }

  /** THE MEDALLION: paper inside an engraved grey ring, and the title in
   *  six lines, each in its own display face. */
  Element medallion() const {
    const float diameter = 2 * kCircle.radius * kMedallionRing;
    // The two display lines are shaded letters: each stroke carries a
    // grey shadow cut below and to the right of it.
    const auto shaded =
        sigil::material::from(kInk).effects(sigil::material::Filter::shadow(
            hexColor(0xA39E94), {.offset = {1.4f, 1.4f}}));
    return box().inset(0).children(
        {kit::dot(kCircle.centre, kCircle.radius * kMedallionOuter,
                  Fill::color(kEngraving))
             .opacity(0.45f),
         kit::dot(kCircle.centre, kCircle.radius * kMedallionRing,
                  Fill::color(kMedallion))
             .stroke(stroke(0.8f, Fill::color(kHairline))),
         box()
             .column()
             .alignItems(Align::Center)
             .gap(7)
             .width(diameter)
             .centerAt(kCircle.centre)
             .children(
                 {text(words.phrase("ordinal"))
                      .styleClass("ordinal")
                      .span(weave::selectors::text(u8"er"),
                            SpanStyle().font(
                                {.size = 11.0f, .baselineShift = 7.0f})),
                  text(words.phrase("circle")).styleClass("circle").ink(shaded),
                  text(words.phrase("of")).styleClass("small"),
                  text(words.phrase("author"))
                      .styleClass("author")
                      .ink(shaded)
                      .span(weave::selectors::text(u8"r"),
                            SpanStyle().font(
                                {.size = 16.0f, .baselineShift = 10.0f})),
                  text(words.phrase("containing")).styleClass("small"),
                  text(words.phrase("colours")).styleClass("colours"),
                  kit::line({.length = Dimension(96), .thickness = 1.4f})
                      .margin(Edges{.top = 6})})});
  }

  /** THE PLATEMARK, its running head over a rule, and the imprint under
   *  one: engraver left, publisher centred, printer right. */
  Element platemark() const {
    const auto rule = [](float top) {
      return kit::at(kPlateLeft + 14, top, kPlateWidth - 28, 1)
          .fill(Fill::color(kHairline));
    };
    const float footTop = kPlateTop + kPlateHeight - kBandDepth;
    return box().inset(0).children(
        {kit::at(kPlateLeft, kPlateTop, kPlateWidth, kPlateHeight)
             .fill(Fill::color(kPlate))
             .stroke(stroke(1.0f, Fill::color(hexColor(0xC9C2B2)))),
         rule(kPlateTop + kBandDepth), rule(footTop),
         kit::at(kPlateLeft + 16, kPlateTop + 6, 300, 14)
             .children({text(words.phrase("runningHead")).styleClass("head")}),
         kit::at(kPlateLeft + 16, footTop + 5, kPlateWidth - 32, 14)
             .row()
             .justifyContent(Justify::SpaceBetween)
             .children({text(words.phrase("engraver")).styleClass("credit"),
                        text(words.phrase("publisher")).styleClass("imprint"),
                        text(words.phrase("printer")).styleClass("credit")})});
  }

  StyleSheet engraving() const {
    const auto didot = face({"Didot", "Bodoni 72", "Baskerville"});
    const auto italic =
        face({"Didot", "Baskerville"},
             sigil::weave::FaceStyle{.slant = sigil::weave::FaceSlant::Italic});
    return StyleSheet{
        rule(".numeral").font({.face = didot, .size = 10.5f}),
        rule(".scale").font({.face = didot, .size = 6.6f, .track = 0.3f}),
        rule(".ordinal").font({.face = didot, .size = 17}),
        rule(".circle").font(
            {.face = face({"Copperplate", "Gill Sans"},
                          sigil::weave::FaceStyle{.weight = 700}),
             .size = 19,
             .track = 1.4f}),
        rule(".small").font({.face = didot, .size = 12, .track = 1.5f}),
        rule(".author").font(
            {.face = face({"SuperClarendon", "Rockwell"},
                          sigil::weave::FaceStyle{.weight = 700}),
             .size = 29,
             .track = 1.5f}),
        rule(".colours")
            .font({.face = face({"Avenir Next Condensed", "Gill Sans"}),
                   .size = 20,
                   .track = 1}),
        rule(".head").font({.face = didot, .size = 10, .track = 0.6f}),
        rule(".credit").font({.face = italic, .size = 10}),
        rule(".imprint").font({.face = didot, .size = 11, .track = 0.3f})};
  }

  void setup(sketch::SketchContext& context) {
    sketch::kit::stage(
        context,
        {.size = {kWidth, kHeight}, .captureAt = 0.05, .background = kPaper});
    words = sketch::kit::Document(context, "data/content.json");
    if (const auto table = context.assets.hub().load<sigil::data::Table>(
            context.local("data/colours.csv"))) {
      const auto label = table->column<std::string>("label");
      const auto colour = table->column<std::string>("colour");
      for (size_t row = 0; row < label.size(); ++row)
        sectors.push_back(
            {label[row], sigil::material::parseColor(colour[row])});
    }
    paperGrain = Paint::recipe(field::grain(0.013f, 4, 11.0f, 0.32f));

    context.composer.render(
        box()
            .inset(0)
            .ink(kInk)
            .applyStyleSheet(engraving())
            .children({platemark(), blades(), limb(), medallion(),
                       box()
                           .inset(0)
                           .fill(sigil::material::skia::base(paperGrain))
                           .blendMode(sigil::material::BlendMode::Multiply)
                           .opacity(0.07f)
                           .cache(Cache::Texture)}));
  }
};

}  // namespace

SIGIL_SKETCH(ChevreulCircle, "Study · Science",
             "Chevreul's 1er cercle chromatique, Plate V, 1864 — seventy-two "
             "couleurs franches on a numbered limb")
