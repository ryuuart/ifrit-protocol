// The Penrose paving outside the Mathematical Institute in Oxford, seen in
// plan: two granite rhombs, a fat one of 72° in Royal White and a thin one
// of 36° in Kobra grey, laid with hairline joints, each carrying two
// polished stainless steel arcs of half an edge's radius that link across
// the joints into the circles and ribbons running over the whole plaza.
//
// The tiling is drawn by hand the way Penrose's rhombs are usually drawn:
// start from a sun of ten thin half-rhombs around one point, cut every
// half-rhomb into smaller ones by the golden ratio, repeat, and pair the
// halves back into rhombs. A panel beside the plaque shows the same cut on
// one fat rhomb, generation by generation.

// TAGS: Patterns/Tiling

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Page.h>

#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <numbers>
#include <vector>

namespace field = sigil::material::field;
namespace material = sigil::material;
namespace materialkit = sigil::material::kit;
namespace shapes = sigil::geometry::shapes;
namespace sketch = sigil::sketch;
using material::skia::Paint;

using namespace sigil::compose;

namespace {

// --- the drawing ------------------------------------------------------------

constexpr float kWidth = 1600, kHeight = 1200;
constexpr glm::vec2 kCentre{kWidth / 2, kHeight / 2};  // the five-fold point
constexpr float kEdge = 78;      // a rhomb's side, px
constexpr int kGenerations = 6;  // cuts from the sun down to the paving
constexpr float kJoint = 1.2f;   // the saw-cut joint between two setts
constexpr float kBand = 9.8f;    // the steel insert's width
constexpr float kGolden = std::numbers::phi_v<float>;

// --- the stone and the steel --------------------------------------------------

const material::Color kWhiteLit = hexColor(0xD4D0C6);  // Royal White
const material::Color kWhiteShade = hexColor(0xBDB9AF);
const material::Color kGreyLit = hexColor(0x979A9E);  // Kobra grey
const material::Color kGreyShade = hexColor(0x7E8186);
const material::Color kJointMortar = hexColor(0x404346);
const material::Color kGroove = hexColor(0x4A4F53, 0.45f);
const material::Color kSteel = hexColor(0xE9ECED);
const material::Color kSteelCatch = hexColor(0xFFFFFF, 0.75f);
const material::Color kPanel = hexColor(0x121517, 0.88f);
const material::Color kPanelRule = hexColor(0x5E6163, 0.55f);
const material::Color kLettering = hexColor(0xDCE0E2);
const material::Color kLetteringQuiet = hexColor(0x9CA2A5);

// --- the construction ----------------------------------------------------------

/** HALF A RHOMB, cut along the diagonal between `first` and `second`. A fat
 *  half is 108° at its apex and 36° at the other two corners; a thin half
 *  is 36° at its apex and 72° at the other two. The two halves of one
 *  rhomb are mirror images, so they wind in opposite directions. */
struct Half {
  bool fat;
  glm::vec2 apex, first, second;
};

/** One cut: every half becomes two or three halves smaller by the golden
 *  ratio, covering exactly the same ground. A thin half gives one thin and
 *  one fat; a fat half gives two fat and one thin. */
std::vector<Half> subdivide(const std::vector<Half>& halves) {
  std::vector<Half> finer;
  for (const Half& half : halves) {
    if (half.fat) {
      const glm::vec2 alongApex =
          half.first + (half.apex - half.first) / kGolden;
      const glm::vec2 alongBase =
          half.first + (half.second - half.first) / kGolden;
      finer.push_back({true, alongBase, half.second, half.apex});
      finer.push_back({true, alongApex, alongBase, half.first});
      finer.push_back({false, alongBase, alongApex, half.apex});
    } else {
      const glm::vec2 cut = half.apex + (half.first - half.apex) / kGolden;
      finer.push_back({false, half.second, cut, half.first});
      finer.push_back({true, cut, half.second, half.apex});
    }
  }
  return finer;
}

/** The sun: ten thin halves meeting at their 36° apexes, alternately
 *  mirrored, with legs of @p radius. */
std::vector<Half> sun(glm::vec2 centre, float radius) {
  std::vector<Half> halves;
  for (int spoke = 0; spoke < 10; ++spoke) {
    auto rim = [&](int step) {
      const float angle = step * std::numbers::pi_v<float> / 10;
      return centre + radius * glm::vec2{std::cos(angle), std::sin(angle)};
    };
    glm::vec2 first = rim(2 * spoke - 1), second = rim(2 * spoke + 1);
    if (spoke % 2 == 0) std::swap(first, second);
    halves.push_back({false, centre, first, second});
  }
  return halves;
}

/** A sett: a whole rhomb, its corners in order round it. */
struct Rhomb {
  bool fat;
  glm::vec2 apex, first, far, second;
  glm::vec2 centre() const { return (first + second) / 2.0f; }
};

/** A half's whole rhomb: the far corner mirrors the apex through the
 *  diagonal's midpoint. */
Rhomb whole(const Half& half) {
  return {half.fat, half.apex, half.first, half.first + half.second - half.apex,
          half.second};
}

/** Winding of the half: its mirror partner has the other sign, so keeping
 *  one sign keeps each rhomb once. */
float winding(const Half& half) {
  const glm::vec2 one = half.first - half.apex, two = half.second - half.apex;
  return one.x * two.y - one.y * two.x;
}

/** The paving: the sun cut down to setts of side kEdge, one rhomb per
 *  mirrored pair, kept where it reaches the canvas. */
std::vector<Rhomb> paving() {
  std::vector<Half> halves =
      sun(kCentre, kEdge * std::pow(kGolden, (float)kGenerations));
  for (int cut = 0; cut < kGenerations; ++cut) halves = subdivide(halves);
  std::vector<Rhomb> setts;
  for (const Half& half : halves) {
    const glm::vec2 centre = (half.first + half.second) / 2.0f;
    const bool onCanvas = centre.x > -kEdge && centre.x < kWidth + kEdge &&
                          centre.y > -kEdge && centre.y < kHeight + kEdge;
    if (winding(half) > 0 && onCanvas) setts.push_back(whole(half));
  }
  return setts;
}

// --- drawing one sett -----------------------------------------------------------

float degreesTowards(glm::vec2 from, glm::vec2 to) {
  return std::atan2(to.y - from.y, to.x - from.x) * 180 /
         std::numbers::pi_v<float>;
}

float wrapDegrees(float degrees) {
  return std::remainder(degrees, 360.0f);
}

/** A RHOMB IS A PARALLELOGRAM, TURNED. Laid flat, its acute corner sits at
 *  the bottom left, one edge running right and the other rising at the
 *  corner's angle; the turn then lays that bottom edge along whichever of
 *  the corner's two edges has the other one counter-clockwise of it. */
Element rhomb(glm::vec2 centre, glm::vec2 corner, glm::vec2 one, glm::vec2 two,
              float cornerDegrees, float side) {
  const float radians = cornerDegrees * std::numbers::pi_v<float> / 180;
  const float width = side * (1 + std::cos(radians));
  const float height = side * std::sin(radians);
  const float towardsOne = degreesTowards(corner, one);
  const float towardsTwo = degreesTowards(corner, two);
  const float turn =
      wrapDegrees(towardsTwo - towardsOne) < 0 ? towardsOne : towardsTwo;
  return kit::at(box()
                     .shape(shapes::parallelogram(90 - cornerDegrees))
                     .rotate(turn),
                 centre.x - width / 2, centre.y - height / 2, width, height);
}

/** The acute-corner reading of a sett, which is what `rhomb` is drawn from:
 *  a fat sett's 72° corner is the end of its diagonal, a thin sett's 36°
 *  corner is its apex. */
Element rhombOf(const Rhomb& sett, float side) {
  return sett.fat ? rhomb(sett.centre(), sett.first, sett.apex, sett.far, 72,
                          side)
                  : rhomb(sett.centre(), sett.apex, sett.first, sett.second,
                          36, side);
}

/** Granite cut from one slab per sett: the stone's two tones, grain and
 *  speckle, seeded by the sett's place so no two neighbours match. */
Paint granite(bool fat, int place) {
  return Paint::recipe(materialkit::stone({
      .hi = fat ? kWhiteLit : kGreyLit,
      .lo = fat ? kWhiteShade : kGreyShade,
      .bedLength = 260,
      .bedDepth = 0.3f,
      .grainScale = fat ? 0.88f : 1.0f,
      .grainContrast = fat ? 0.44f : 0.37f,
      .speckle = 0.34f,
      .speckleCell = fat ? 4.0f : 6.5f,
      .speckleAlpha = 0.26f,
      .seed = (float)(place % 37),
  }));
}

/** THE STEEL ARC round one corner: radius half an edge, from the midpoint
 *  of one edge to the midpoint of the other, stopping short of the joints
 *  it meets there. A band set in a milled groove, with the light caught
 *  along its crown. */
Element arc(glm::vec2 corner, glm::vec2 one, glm::vec2 two) {
  const float radius = kEdge / 2;
  const float start = degreesTowards(corner, one);
  const float sweep = wrapDegrees(degreesTowards(corner, two) - start);
  const float clear = (kJoint / 2 + 0.9f) / radius * 180 /
                      std::numbers::pi_v<float> * (sweep < 0 ? -1 : 1);
  return kit::at(box()
                     .shape(shapes::arc(start + clear, sweep - 2 * clear))
                     .fill(Fill::none())
                     .stroke(stroke(kBand + 1.2f, Fill::color(kGroove)))
                     .stroke(stroke(kBand, Fill::color(kSteel)))
                     .stroke(stroke(kBand * 0.3f, Fill::color(kSteelCatch))),
                 corner.x - radius, corner.y - radius, kEdge, kEdge);
}

/** The steel over the whole plaza: both arcs of every sett sit on the
 *  corners at the ends of its diagonal. */
std::vector<Element> steel(const std::vector<Rhomb>& setts) {
  std::vector<Element> arcs;
  for (const Rhomb& sett : setts) {
    arcs.push_back(arc(sett.first, sett.apex, sett.far));
    arcs.push_back(arc(sett.second, sett.apex, sett.far));
  }
  return arcs;
}

// --- the deflation panel ---------------------------------------------------------

constexpr float kDiagramSide = 72;

/** One fat rhomb laid flat, cut @p cuts times, clipped to itself: the
 *  halves along its rim belong to rhombs that run past it. */
Element deflated(int cuts) {
  const float lean = kDiagramSide * std::cos(72 * std::numbers::pi_v<float> / 180);
  const float width = kDiagramSide + lean;
  const float height = kDiagramSide * std::sin(72 * std::numbers::pi_v<float> / 180);
  std::vector<Half> halves = {
      {true, {lean, 0}, {0, height}, {width, 0}},
      {true, {width - lean, height}, {width, 0}, {0, height}}};
  for (int cut = 0; cut < cuts; ++cut) halves = subdivide(halves);
  const float side = kDiagramSide / std::pow(kGolden, (float)cuts);
  return box()
      .width(width)
      .height(height)
      .shape(shapes::parallelogram(18))
      .overflow(Overflow::Clip)
      .children({positioned().inset(0).children({each(
          halves, [side](const Half& half) {
            return rhombOf(whole(half), side)
                .fill(Fill::color(half.fat ? kWhiteLit : kGreyShade))
                .foreground(decorations::border(0.8f, Fill::color(kJointMortar)));
          })})});
}

}  // namespace

struct PenrosePaving {
  /** The plaque's words, from `data/content.json` beside this file. */
  sketch::kit::Document words;
  std::vector<Rhomb> setts;

  Element plaza() const {
    return positioned().inset(0).children(
        {each(setts,
              [](const Rhomb& sett, size_t place) {
                return rhombOf(sett, kEdge)
                    .fill(granite(sett.fat, (int)place))
                    .foreground(
                        decorations::border(kJoint, Fill::color(kJointMortar)));
              }),
         steel(setts)});
  }

  /** Daylight over the plaza: traffic staining metres across that ignores
   *  the joints, and one broad falloff from the sunlit corner. */
  static std::vector<Element> weather() {
    return {box()
                .inset(0)
                .fill(Paint::recipe(field::grain(0.0042f, 2, 91, 0.62f, 1.15f)))
                .blendMode(SkBlendMode::kSoftLight)
                .opacity(0.5f),
            box()
                .inset(0)
                .blendMode(SkBlendMode::kMultiply)
                .fill(radialGradient({470, 280}, 1280,
                                     {hexColor(0xFFFFFF), hexColor(0xE8E8E6),
                                      hexColor(0xB4B6BA), hexColor(0x7A7D82)},
                                     {0.0f, 0.3f, 0.7f, 1.0f}))};
  }

  static Element panel(float left, float top, float width, float height) {
    return kit::at(box()
                       .fill(Fill::color(kPanel))
                       .stroke(stroke(1, Fill::color(kPanelRule),
                                      PathFormat::Align::Inner))
                       .background(styles::dropShadow(hexColor(0x000000, 0.5f),
                                                      {0, 5}, 18)),
                   left, top, width, height);
  }

  Element plaque() const {
    int fat = 0;
    for (const Rhomb& sett : setts) fat += sett.fat;
    const int thin = (int)setts.size() - fat;
    const std::string construction = kit::formatted(
        "%d CUTS FROM A SUN OF TEN  ·  EDGE %.0f px  ·  %d SETTS  ·  "
        "FAT : THIN = %.3f  (φ = 1.618)",
        kGenerations, kEdge, (int)setts.size(), (double)fat / thin);
    return panel(56, 1054, 980, 118)
        .column()
        .padding(20)
        .gap(12)
        .children({text(words["plaque.title"])
                       .font({.size = 13, .color = kLettering, .track = 1.9f}),
                   text(words["plaque.place"]).font({.size = 11.5f,
                                                     .color = kLetteringQuiet,
                                                     .track = 1.5f}),
                   text(construction)});
  }

  static Element deflation(const sketch::kit::Document& words) {
    return panel(1060, 1022, 484, 150)
        .column()
        .padding(16)
        .gap(18)
        .children({text(words["inset.head"]),
                   box()
                       .row()
                       .gap(20)
                       .alignItems(Align::Center)
                       .children({each(4, [](size_t cuts) {
                         return deflated((int)cuts);
                       })})});
  }

  void setup(sketch::SketchContext& context) {
    sketch::kit::stage(context, {.size = {kWidth, kHeight},
                                 .captureAt = 0.05,
                                 .background = kJointMortar});
    words = sketch::kit::Document(context, "data/content.json");
    setts = paving();

    context.composer.render(
        stack()
            .fill(Fill::color(kJointMortar))
            // The plaza's lettering voice: small, tracked, cool grey.
            .font({.size = 10.5f, .color = hexColor(0x8E9295), .track = 1.0f})
            .children({plaza(), weather(),
                       // A shaded foot for the plaque and the panel to sit in.
                       kit::at(box().fill(linearGradient(
                                   {0, 0}, {0, 200},
                                   {hexColor(0x08090A, 0), hexColor(0x08090A, 0.6f)})),
                               0, kHeight - 200, kWidth, 200),
                       plaque(), deflation(words)}));
  }
};

SIGIL_SKETCH(PenrosePaving, "Study · Pattern",
             "Penrose's 2012 P3 paving, Oxford — granite rhombs with steel "
             "arcs, cut from a sun by golden-ratio deflation")
