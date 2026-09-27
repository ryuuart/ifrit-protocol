/** @file
 * cosmati — the Great Pavement at Westminster: opus sectile in quarried
 * stone and glass, set out by its own construction and laid in order.
 *
 * THE PAVEMENT, as the Roman crew under Odoricus laid it before the high
 * altar in 1268, a square 25 Roman feet a side:
 *   * a broad border of Purbeck, the framework's own dark stone, carrying
 *     the date in brass letters round its outer strip, twenty roundels
 *     about its corners and a panel at the middle of each side;
 *   * a square turned on its point inside it, the four triangles it leaves
 *     each holding a great roundel over a course of squares on their
 *     points;
 *   * inside the turned square the QUINCUNX, the tradition's governing
 *     figure: an onyx roundel lettered round with what the floor claims
 *     to be, four roundels about it — a circle, a hexagon, a heptagon and
 *     an octagon — and one guilloche band that loops each of the five and
 *     crosses over itself between them, on a ground of triangles in three
 *     stones about white marble.
 * The stones are the pavement's spolia, named in the words file: purple
 * porphyry, green lapis lacedaemonius, yellow limestone, white marble,
 * onyx, and opaque glass in red, turquoise and cobalt.
 *
 * THE FLOOR LAYS ITSELF IN THE ORDER IT WAS LAID: the Purbeck matrix, the
 * roundels set into it from the centre outward, the tesserae filling the
 * fields between, the band run round the quincunx, and the brass set
 * letter by letter; after that a low light crosses the polished stone for
 * as long as the plate is watched.
 */

// TAGS: Patterns/Tiling

#include <choreograph/Easing.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/typography/TextPath.h>
#include <sigildraw/Color.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/kit/Legend.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/kit/LineTables.h>

#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Stone.h"
#include "Latten.h"

#include "Construction.h"

namespace draw = sigil::draw;
namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;
namespace path = sigil::geometry::path;
using namespace sigil::compose;
using sigil::material::hexColor;
using namespace sigil::motion;
using namespace sigil::weave::literals;
using namespace std::chrono_literals;

namespace {

constexpr SkSize kCanvas{1240, 800};
/** The pavement is square because the Great Pavement is; it stands on the
 *  canvas's left margin with the apparatus in the column beside it. */
constexpr float kSide = 720;
constexpr float kMargin = 40;
constexpr float kColumnLeft = kMargin + kSide + 48;
constexpr float kColumnWidth = kCanvas.fWidth - kColumnLeft - kMargin;

// THE SETTING-OUT, in pavement pixels from its top-left corner.
/** The outer strip the brass letters are set in. */
constexpr float kLetterStrip = 30;
/** The inner square's edge: the border runs between it and the strip. */
constexpr float kBorder = 100;
constexpr float kBorderMiddle = (kLetterStrip + kBorder) * 0.5f;
constexpr float kCentre = kSide * 0.5f;
/** The border's roundels and the panel at the middle of each side: five
 *  roundels about each corner at one pitch, the panel standing off the
 *  nearest by the gap the roundels keep between themselves. */
constexpr float kBorderRoundel = 25;
constexpr float kPanelLength = 176;
constexpr float kPanelDepth = 50;
constexpr float kBorderPitch =
    (kCentre - kPanelLength * 0.5f - kBorderMiddle + kBorderRoundel) / 3;
/** The quincunx: the onyx roundel and its lettered ring, the loop the band
 *  makes about them, and the four orbiting roundels with theirs. */
constexpr float kOnyx = 50;
constexpr float kLetterRing = 68;
constexpr float kCentreLoop = 76;
constexpr float kOrbit = 150;
constexpr float kSatellite = 34;
constexpr float kSatelliteLoop = 42;
/** The band's width, marble edge to marble edge, and the joint a piece of
 *  stone is set apart from the next by. */
constexpr float kBand = 13;
constexpr float kFillet = 2.5f;
/** The great roundels in the four corners the turned square leaves: each
 *  stands on the incentre of its triangle, whose legs are half the inner
 *  square. */
constexpr float kInnerHalf = kCentre - kBorder;
constexpr float kCornerInset = kInnerHalf * (2 - 1.41421356f) * 0.5f;
constexpr float kCornerRoundel = 54;

// THE LAYING, in seconds of the scene.
constexpr float kRoundelsAt = 0.35f;
constexpr float kFieldsAt = 1.7f;
constexpr float kBandFrom = 2.1f, kBandTo = 3.9f;
constexpr float kLettersFrom = 3.6f, kLettersTo = 5.4f;
/** The low light: one crossing of the floor every eight seconds, placed so
 *  the still finds it a little short of the middle. */
constexpr float kLightPeriod = 8;
constexpr float kLightPhase = 2.4f;

constexpr float kTurn = 6.2831853f;
/** The low window's light, warm on the stone. */
constexpr material::Color kDaylight{1, 0.93f, 0.80f, 1};

// ---------------------------------------------------------------------------
// The stones

/** A QUARRY: what the stone is called, where it came from, and the two
 *  tones its bed runs between, as the words file names them. */
struct Quarry {
  std::string name, source;
  material::Color hi, lo;
};

/** A quarry as the words file names it. Its tones are CSS colour text
 *  ("#74494A", or "#74494A80" with an alpha), and only the pen library
 *  reads a colour's text, so they are read through it. */
Quarry quarryOf(const sketch::kit::Document& words, std::string_view key) {
  const auto& stone = words["quarries"][key];
  return {std::string(stone["name"].string()), std::string(stone["source"].string()),
          draw::parseColor(stone["hi"].string("#808080")),
          draw::parseColor(stone["lo"].string("#606060"))};
}

struct Quarries {
  Quarry porphyry, serpentine, giallo, marble, onyx, purbeck, red, turquoise,
      cobalt;
};

/** A CUT STONE: the quarry's two tones on a bed at @p bedAngle degrees,
 *  veined with luminance grain — which shades a coloured stone rather than
 *  shifting its hue, so the floor never reads as rainbow terrazzo — and
 *  flecked with its own colours. @p seed makes two pieces of one quarry
 *  two pieces; @p veining is how hard the grain reads and @p bedLength
 *  how far apart its beds lie, and the framework asks for fainter grain
 *  on longer beds than the tesserae so the Purbeck reads as one slab. */
material::Material cut(const Quarry& quarry, float bedAngle, float seed = 0,
                       float veining = 0.35f, float bedLength = 52) {
  return cosmati::stone({.hi = quarry.hi,
                        .lo = quarry.lo,
                        .bedAngle = bedAngle,
                        .bedLength = bedLength,
                        .grainContrast = veining,
                        .speckle = 0.30f,
                        .speckleCell = 9.0f,
                        .speckleAlpha = 0.27f,
                        .seed = seed});
}

// ---------------------------------------------------------------------------
// Pieces of the construction

/** A closed outline through @p corners, as a held path in the coordinates
 *  of the node that wears it. */
HeldPath outline(std::initializer_list<glm::vec2> corners) {
  return heldPath(path::toPath(path::Polyline{.points = corners, .closed = true}));
}

/** A BED OF TESSERAE: the region a field is cut to, its courses of stone
 *  over mortar, and the marble fillet that edges it. */
struct Field {
  Shape region;
  std::vector<std::pair<cosmati::Course, material::Material>> courses;
};

Element field(const Field& bed, const Quarry& marble) {
  Element laid = stack()
                     .cover()
                     .shape(bed.region)
                     .overflow(Overflow::Clip)
                     .fill(Fill::var("mortar"))
                     .foreground(stroke(kFillet, cut(marble, 40),
                                        PathFormat::Align::Inner));
  for (const auto& [course, stone] : bed.courses)
    laid.children({box().cover().shape(course).fill(stone)});
  return laid;
}

/** ONE PIECE OF A ROUNDEL: an outline over the roundel's box, the stone it
 *  is cut from, and how far in from the box it stands. */
struct Piece {
  Shape outline;
  material::Material stone;
  float inset = 0;
};

/** A ROUNDEL: a disc (or a polygon) of one stone with its pieces set in it,
 *  edged in marble, entering as a stone set into the bed — it drops the
 *  last fraction and settles. The pieces are static once it has entered,
 *  so it is baked once and the entrance rides over the texture. */
Element roundel(std::string key, SkPoint at, float radius, Shape outline,
                material::Material disc, std::vector<Piece> pieces,
                const Quarry& marble, float enters) {
  const std::chrono::milliseconds delay{(int)(enters * 1000)};
  Element set = stack()
                    .key(std::move(key))
                    .width(radius * 2)
                    .height(radius * 2)
                    .centerAt(at)
                    .cache(Cache::Texture)
                    .opacity(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 320ms, .delay = delay, .ease = sigil::motion::ease::outQuad}))
                    .scale(sigil::motion::animate({.from = 1.07f, .to = 1.0f, .duration = 560ms, .delay = delay, .ease = sigil::motion::ease::outCubic}));
  set.children({box().cover().shape(outline).fill(std::move(disc)).foreground(
      stroke(kFillet, cut(marble, 70), PathFormat::Align::Inner))});
  for (Piece& piece : pieces)
    set.children({box()
                       .inset(piece.inset)
                       .shape(piece.outline)
                       .fill(std::move(piece.stone))
                       .stroke(stroke(0.8f, Fill::var("mortar")))});
  return set;
}

/** THE BAND'S DRESS: a marble strip with a core of porphyry, and giallo
 *  tesserae set in the core at one pitch.
 *  @p along is where on the whole band this stretch starts, so a
 *  stretch drawn again over a crossing keeps the band's own pitch. */
Decoration bandDress(const Quarries& stone, float along = 0) {
  PathFormat tesserae = stroke(kBand - 5, cut(stone.giallo, 30, 3));
  tesserae.dashIntervals = {4.5f, 4.5f};
  tesserae.dashPhase = along;
  return brush::layers({stroke(kBand, cut(stone.marble, 60, 5)),
                        stroke(kBand - 4, cut(stone.porphyry, 20, 4)),
                        std::move(tesserae)});
}

}  // namespace

struct Cosmati {
  sketch::kit::Document words;
  Quarries stone;
  cosmati::Interlace band;
  /** The scene's one clock, in seconds; every beat is a window on it. */
  sigil::motion::Animatable<float> seconds = sigil::motion::animatable(0.0f);

  void setup(sketch::SketchContext& ctx) {
    words = sketch::kit::Document{ctx, "data/pavement.json"};
    stone = {quarryOf(words, "porphyry"), quarryOf(words, "serpentine"),
             quarryOf(words, "giallo"),   quarryOf(words, "marble"),
             quarryOf(words, "onyx"),     quarryOf(words, "purbeck"),
             quarryOf(words, "red"),      quarryOf(words, "turquoise"),
             quarryOf(words, "cobalt")};
    band = cosmati::quincunxInterlace({kCentre, kCentre}, kCentreLoop,
                                      kSatelliteLoop, kOrbit, -kTurn * 0.25f);
    sketch::kit::stage(ctx, {.size = kCanvas,
                             .captureAt = 6.0,
                             .background = draw::parseColor(words["ink"]["ground"].string())});
    sigil::motion::Engine& ticker = ctx.engine;
    ticker.add([this, &ticker] { seconds = (float)ticker.elapsed(); });
    ctx.composer.render(describe());
  }

  // =========================================================================
  // How the plate is set

  StyleSheet sheet() const {
    const auto& ink = words["ink"];
    const std::string inscriptional = "Optima, Gill Sans, Helvetica Neue, sans-serif";
    const std::string book = "Palatino, Book Antiqua, Baskerville, serif";
    return StyleSheet{
        rule(":root")
            .var("ink", draw::parseColor(ink["ink"].string()))
            .var("ash", draw::parseColor(ink["ash"].string()))
            .var("rule", draw::parseColor(ink["rule"].string()))
            .var("mortar", draw::parseColor(ink["mortar"].string()))
            .fontFamily(inscriptional)
            .fontSize(11)
            .ink(var("ash")),
        rule("eyebrow").fontSize(0.9_rem).letterSpacing(0.22_em).ink(var("rule")),
        rule("h1")
            .fontSize(2.6_rem)
            .fontWeight(600)
            .letterSpacing(0.16_em)
            .ink(var("ink")),
        rule("lead").fontSize(1.1_rem).letterSpacing(0.05_em),
        rule("paragraph")
            .fontFamily(book)
            .fontSize(1.2_rem)
            .letterSpacing(0)
            .ink(var("ink"))
            .lineHeight(weave::Leading::absolute(19))
            .textWrap(TextWrap::Pretty)
            .hyphens({.patterns = weave::kit::englishHyphenator()})
            .paragraph({.hanging = weave::kit::hanging::latin()}),
        rule("quote paragraph").fontStyle(FontStyle::Italic).ink(var("ash")),
        rule("caption").fontSize(0.9_rem).letterSpacing(0.04_em),
        rule("h2").fontSize(0.9_rem).letterSpacing(0.22_em).ink(var("rule")),
        rule(".key label")
            .fontFamily(inscriptional)
            .fontSize(1.05_rem).letterSpacing(0.03_em).ink(var("ink")),
        rule(".key caption")
            .fontFamily(book)
            .fontStyle(FontStyle::Italic)
            .fontSize(1.05_rem)
            .letterSpacing(0),
        // Brass letters, cut and set into the Purbeck: the inscriptional
        // capitals, widely spaced, in latten that one light crosses.
        rule(".inscription")
            .fontWeight(600)
            .fontSize(1.2_rem)
            .letterSpacing(0.34_em),
        rule(".ring").fontSize(0.74_rem).letterSpacing(0.12_em),
    };
  }

  /** Brass under the nave's one light, laid across the whole canvas so the
   *  sheen runs through every letter as one sheet. */
  static material::Material brass() {
    return cosmati::latten({.from = {0, 0},
                           .to = {kCanvas.fWidth, kCanvas.fHeight},
                           .level = 0.55f,
                           .sheen = 0.16f,
                           .patina = 0.25f});
  }

  // =========================================================================
  // The pavement, layer by layer

  /** THE MATRIX: the Purbeck slab everything is set into, with the marble
   *  fillets that mark the strip, the border and the inner square. */
  Element matrix() const {
    const auto fillet = [&](float inset) {
      return box().inset(inset).foreground(stroke(kFillet, cut(stone.marble, 50),
                                                  PathFormat::Align::Inner));
    };
    return stack()
        .cover()
        .key("matrix")
        .cache(Cache::Texture)
        .opacity(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 500ms, .ease = sigil::motion::ease::outQuad}))
        .fill(cut(stone.purbeck, 28, 0, 0.14f, 260))
        .children({fillet(0), fillet(kLetterStrip), fillet(kBorder)});
  }

  /** THE FIELDS OF TESSERAE between the roundels: triangles in three
   *  stones about white marble in the turned square, squares on their
   *  points in the four corners it leaves, and a finer course of squares
   *  in each border panel. A few pieces in each are worn out of the bed. */
  Element fields() const {
    // Every field stands clear of the lines it is set between by one
    // Purbeck strip, measured square to the line.
    const float clear = 12;
    const float lozenge = kInnerHalf - clear * 1.41421356f;
    const float side = 14, rise = side * 0.866f;
    const glm::vec2 across{side, 0}, down{side * 0.5f, rise};
    const auto triangles = [&](int offset, const Quarry& quarry, float bed) {
      return std::pair{cosmati::Course{.tessera = cosmati::upTriangle(side, rise),
                                       .across = across,
                                       .down = down,
                                       .every = 3,
                                       .offset = offset,
                                       .loss = 0.02f,
                                       .seed = (uint32_t)offset + 1},
                       cut(quarry, bed, (float)offset)};
    };
    Field turned{
        .region = outline({{kCentre, kCentre - lozenge},
                           {kCentre + lozenge, kCentre},
                           {kCentre, kCentre + lozenge},
                           {kCentre - lozenge, kCentre}}),
        .courses = {triangles(0, stone.porphyry, 18),
                    triangles(1, stone.serpentine, 34),
                    triangles(2, stone.giallo, 12),
                    {cosmati::Course{.tessera = cosmati::downTriangle(side, rise),
                                     .across = across,
                                     .down = down,
                                     .loss = 0.015f,
                                     .seed = 9},
                     cut(stone.marble, 52)}}};

    const auto squares = [&](float half, const Quarry& first,
                             const Quarry& second, uint32_t seed) {
      const glm::vec2 squareAcross{half, half}, squareDown{-half, half};
      return std::vector<std::pair<cosmati::Course, material::Material>>{
          {cosmati::Course{.tessera = cosmati::squareOnPoint(half),
                           .across = squareAcross,
                           .down = squareDown,
                           .every = 2,
                           .loss = 0.02f,
                           .seed = seed},
           cut(first, 30, (float)seed)},
          {cosmati::Course{.tessera = cosmati::squareOnPoint(half),
                           .across = squareAcross,
                           .down = squareDown,
                           .every = 2,
                           .offset = 1,
                           .loss = 0.02f,
                           .seed = seed + 1},
           cut(second, 60, (float)seed + 1)}};
    };
    // The four corners the turned square leaves, each a right triangle
    // held clear of the square and the turned square by a Purbeck strip.
    const float reach = kInnerHalf - 2 * clear - clear * 1.41421356f;
    const float near = kBorder + clear, far = kSide - kBorder - clear;
    const std::array<std::array<glm::vec2, 3>, 4> corners{{
        {{{near, near}, {near + reach, near}, {near, near + reach}}},
        {{{far, near}, {far, near + reach}, {far - reach, near}}},
        {{{far, far}, {far - reach, far}, {far, far - reach}}},
        {{{near, far}, {near, far - reach}, {near + reach, far}}},
    }};
    const std::array<std::pair<const Quarry*, const Quarry*>, 4> cornerStones{
        {{&stone.serpentine, &stone.marble},
         {&stone.porphyry, &stone.giallo},
         {&stone.serpentine, &stone.giallo},
         {&stone.porphyry, &stone.marble}}};

    Element laid = stack()
                       .cover()
                       .key("fields")
                       .cache(Cache::Texture)
                       .opacity(sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 800ms, .delay = std::chrono::milliseconds{(int)(kFieldsAt * 1000)}, .ease = sigil::motion::ease::outQuad}))
                       .children({field(turned, stone.marble)});
    for (size_t index = 0; index < corners.size(); ++index) {
      const auto& [a, b, c] = corners[index];
      laid.children({field({.region = outline({a, b, c}),
                            .courses = squares(7, *cornerStones[index].first,
                                               *cornerStones[index].second,
                                               (uint32_t)(20 + 2 * index))},
                           stone.marble)});
    }
    // The border panel at the middle of each side, a finer chequer of
    // porphyry and glass turned with its side.
    for (int sideIndex = 0; sideIndex < 4; ++sideIndex) {
      const float left = kCentre - kPanelLength * 0.5f;
      const float top = kBorderMiddle - kPanelDepth * 0.5f;
      laid.children(
          {box().cover().rotate(90.0f * (float)sideIndex).children(
              {field({.region = outline({{left, top},
                                         {left + kPanelLength, top},
                                         {left + kPanelLength, top + kPanelDepth},
                                         {left, top + kPanelDepth}}),
                      .courses = squares(5, sideIndex % 2 ? stone.red : stone.porphyry,
                                         stone.giallo, (uint32_t)(40 + 2 * sideIndex))},
                     stone.marble)})});
    }
    return laid;
  }

  /** THE ROUNDELS, from the centre outward: the onyx eye, the four that
   *  orbit it, the four great ones in the corners and the twenty of the
   *  border. No two are cut alike. */
  Element roundels() const {
    const SkPoint centre{kCentre, kCentre};
    std::vector<Element> set;
    set.push_back(roundel(
        "onyx", centre, kLetterRing, shapes::circle(), cut(stone.purbeck, 8, 11, 0.2f),
        {{shapes::circle(), cut(stone.onyx, 64, 12, 0.5f), kLetterRing - kOnyx}},
        stone.marble, kRoundelsAt));

    // The four orbiting roundels: a circle, a hexagon, a heptagon and an
    // octagon, each with a ring course of as many lozenges as it has sides
    // and an eye of glass.
    struct Orbiting {
      int sides;
      const Quarry *disc, *course, *eye;
    };
    const std::array<Orbiting, 4> orbiting{{{0, &stone.porphyry, &stone.giallo, &stone.turquoise},
                                            {6, &stone.serpentine, &stone.marble, &stone.red},
                                            {7, &stone.porphyry, &stone.marble, &stone.cobalt},
                                            {8, &stone.serpentine, &stone.giallo, &stone.red}}};
    for (size_t index = 0; index < orbiting.size(); ++index) {
      const Orbiting& cutting = orbiting[index];
      const float angle = -kTurn * 0.25f + kTurn * 0.25f * (float)index;
      const SkPoint at{kCentre + std::cos(angle) * kOrbit,
                       kCentre + std::sin(angle) * kOrbit};
      const Shape outer = cutting.sides == 0
                              ? Shape(shapes::circle())
                              : Shape(shapes::polygon(cutting.sides));
      set.push_back(roundel(
          "orbit." + std::to_string(index), at, kSatellite, outer,
          cut(*cutting.disc, 30.0f * (float)index, 20.0f + (float)index),
          {{cosmati::Rosette{.count = cutting.sides == 0 ? 12 : cutting.sides,
                             .inner = 0.38f,
                             .outer = 0.84f,
                             .phase = -kTurn * 0.25f},
            cut(*cutting.course, 50, 24.0f + (float)index)},
           {outer, cut(*cutting.eye, 20, 28.0f + (float)index), kSatellite * 0.68f}},
          stone.marble, kRoundelsAt + 0.2f + 0.12f * (float)index));
    }

    // The great roundels, each on the incentre of its corner: an
    // eight-pointed star, a ring of ten, a six-pointed star, a double ring.
    const float near = kBorder + kCornerInset, far = kSide - near;
    const std::array<SkPoint, 4> greatAt{{{near, near}, {far, near}, {far, far}, {near, far}}};
    const float inside = kCornerRoundel * 0.18f;
    const std::array<std::vector<Piece>, 4> greatPieces{{
        {{shapes::star(8, 0.52f, 0.12f), cut(stone.giallo, 30, 31), inside},
         {shapes::circle(), cut(stone.red, 10, 32), kCornerRoundel * 0.72f}},
        {{cosmati::Rosette{.count = 10, .inner = 0.34f, .outer = 0.82f},
          cut(stone.marble, 45, 33)},
         {shapes::circle(), cut(stone.porphyry, 10, 34), kCornerRoundel * 0.66f}},
        {{shapes::star(6, 0.56f, 0.08f), cut(stone.serpentine, 20, 35), inside},
         {shapes::polygon(6), cut(stone.giallo, 70, 36), kCornerRoundel * 0.7f}},
        {{cosmati::Rosette{.count = 16, .inner = 0.56f, .outer = 0.86f, .width = 0.7f},
          cut(stone.giallo, 15, 37)},
         {cosmati::Rosette{.count = 8, .inner = 0.18f, .outer = 0.5f, .phase = 0.39f},
          cut(stone.turquoise, 60, 38)}},
    }};
    const std::array<const Quarry*, 4> greatDisc{
        {&stone.porphyry, &stone.serpentine, &stone.porphyry, &stone.serpentine}};
    for (size_t index = 0; index < greatAt.size(); ++index)
      set.push_back(roundel("great." + std::to_string(index), greatAt[index],
                            kCornerRoundel, shapes::circle(),
                            cut(*greatDisc[index], 25.0f * (float)index, 40.0f + (float)index),
                            greatPieces[index], stone.marble,
                            kRoundelsAt + 0.7f + 0.12f * (float)index));

    // The border's twenty: one on each corner and two either side of it,
    // porphyry and green in turn, each with a glass eye.
    const auto borderPlace = [&](int index) {
      // Twenty places sunwise from the top-left corner: each side holds
      // its corner and the two after it, and the two before the next
      // corner.
      const int sideIndex = index / 5, place = index % 5;
      const float offsets[5] = {0, kBorderPitch, 2 * kBorderPitch,
                                kSide - 2 * kBorderMiddle - 2 * kBorderPitch,
                                kSide - 2 * kBorderMiddle - kBorderPitch};
      const float run = kBorderMiddle + offsets[place];
      const SkPoint top{run, kBorderMiddle};
      // Turn the top edge's place about the centre onto its side.
      const float angle = kTurn * 0.25f * (float)sideIndex;
      const float dx = top.fX - kCentre, dy = top.fY - kCentre;
      return SkPoint{kCentre + dx * std::cos(angle) - dy * std::sin(angle),
                     kCentre + dx * std::sin(angle) + dy * std::cos(angle)};
    };
    for (int index = 0; index < 20; ++index) {
      const bool green = index % 2 == 1;
      set.push_back(roundel(
          "border." + std::to_string(index), borderPlace(index), kBorderRoundel,
          shapes::circle(),
          cut(green ? stone.serpentine : stone.porphyry, 15.0f * (float)index,
              50.0f + (float)index),
          {{cosmati::Rosette{.count = green ? 10 : 8, .inner = 0.46f, .outer = 0.86f},
            cut(green ? stone.giallo : stone.marble, 40, 90.0f + (float)index)},
           {shapes::circle(), cut(green ? stone.red : stone.turquoise, 30, 80.0f + (float)index),
            kBorderRoundel * 0.7f}},
          stone.marble, kRoundelsAt + 1.0f + 0.03f * (float)index));
    }
    return stack().cover().key("roundels").children({std::move(set)});
  }

  /** THE GUILLOCHE, run once round the quincunx as the band is laid, then
   *  its outgoing stretch drawn again over each crossing, clipped to the
   *  patch where the two stretches overlap, so it passes over there. */
  Element guilloche() const {
    const auto laying = [&](float from, float to) {
      const float span = kBandTo - kBandFrom;
      return sigil::motion::bind(seconds, {.from = {kBandFrom + span * from / band.length, kBandFrom + span * to / band.length}, .clampFrom = true});
    };
    Element run = stack().cover().key("band").children(
        {box().cover().shape(heldPath(band.spine)).stroke(
            spans::upTo(laying(0, band.length)), bandDress(stone))});
    for (const cosmati::Interlace::Knot& knot : band.knots) {
      const SkPath patch = path::crossingPatch(
          knot.over, kBand, knot.under, kBand, {knot.at.x, knot.at.y}, kBand * 1.6f);
      run.children({box().cover().shape(heldPath(patch)).overflow(Overflow::Clip).children(
          {box().cover().shape(heldPath(knot.over)).stroke(
              spans::upTo(laying(knot.overFrom, knot.overTo)),
              bandDress(stone, knot.overFrom))})});
    }
    return run;
  }

  /** THE BRASS: the date round the border, a line to a side and read
   *  sunwise, and the ring round the onyx saying what the floor is. Each
   *  letter is pressed into its cut as it is set. */
  Element letters() const {
    const auto setting = [&](float from, float to) {
      return Track{.effect = textFx::enter({.fromScale = 1.5f, .fadeOver = 0.45f}),
                   .delay = sigil::motion::stagger(40ms), .duration = 260ms,
                   .progress = sigil::motion::bind(seconds, {.from = {from, to}, .clampFrom = true})};
    };
    const float beat = (kLettersTo - kLettersFrom) / 5;
    Element brassWork = stack().cover().key("brass").ink(brass(), PaintBox::Canvas);
    const auto sides = words["inscription"]["sides"].array();
    for (size_t index = 0; index < sides.size() && index < 4; ++index) {
      const float from = kLettersFrom + beat * (float)index;
      brassWork.children({box().cover().rotate(90.0f * (float)index).children(
          {text(words.phrase(sides[index]))
               .styleClass("inscription")
               .left(kLetterStrip)
               .width(kSide - 2 * kLetterStrip)
               .top(8)
               .textAlign(weave::TextAlignment::kCenter)
               .textFx(setting(from, from + beat * 1.4f))})});
    }
    // The ring's run stops short of its seam: a run on a closed baseline
    // cannot be fitted to the baseline's length, so its size is chosen to
    // come near.
    const float ringRadius = kOnyx + 7;
    brassWork.children(
        {text(words.phrase(words["inscription"]["ring"]))
             .styleClass("inscription ring")
             .width(ringRadius * 2)
             .height(ringRadius * 2)
             .centerAt({kCentre, kCentre})
             .textOnPath({.path = shapes::arc(-90.0f, 359.9f),
                          .at = 0.0f,
                          .align = TextPath::Align::Start})
             .textFx(setting(kLettersFrom + beat * 4, kLettersTo))});
    return brassWork;
  }

  /** THE LIGHT: a low window's light crossing the polished floor, added
   *  over the stone, and the dusk the pavement's corners fall into. */
  Element light() const {
    return stack().cover().key("light").hitTestable(false).children(
        {box()
             .left(-300)
             .top(-100)
             .width(300)
             .height(kSide + 200)
             .rotate(14.0f)
             .translateX(sigil::motion::bind(seconds, {.from = {kLightPhase, kLightPhase + kLightPeriod}, .envelope = sigil::motion::envelope::shaped(sigil::motion::ease::linear), .to = {0.0f, kSide + 600}}))
             .fill(material::Paint::linearGradient(
                 {0, 0}, {300, 0},
                 {{0.0f, material::withAlpha(kDaylight, 0)},
                  {0.35f, material::withAlpha(kDaylight, 0.09f)},
                  {0.5f, material::withAlpha(kDaylight, 0.28f)},
                  {0.65f, material::withAlpha(kDaylight, 0.09f)},
                  {1.0f, material::withAlpha(kDaylight, 0)}},
                 {.units = material::GradientUnits::Pixels}))
             .blendMode(material::BlendMode::PlusLighter),
         box().cover().fill(material::Paint::radialGradient(
             {kCentre, kCentre * 0.8f}, kSide * 0.78f,
             {{0.0f, {0, 0, 0, 0}},
              {0.55f, {0, 0, 0, 0}},
              {1.0f, {0, 0, 0, 0.42f}}},
             {.units = material::GradientUnits::Pixels}))});
  }

  Element pavement() const {
    return stack()
        .key("pavement")
        .left(kMargin)
        .top(kMargin)
        .width(kSide)
        .height(kSide)
        .overflow(Overflow::Clip)
        .background(styles::dropShadow({0, 0, 0, 0.75f}, {0, 10}, 26))
        .children({matrix(), fields(), roundels(), guilloche(), letters(), light()});
  }

  // =========================================================================
  // The apparatus beside it

  /** THE SETTING-OUT: the lines the floor is laid to, struck in hairlines
   *  at @p size — the square and its border, the square turned in it, and
   *  the circles every roundel and every loop of the band is cut to. */
  Element settingOut(float size) const {
    const float scale = size / kSide;
    const auto struck = [&](Element line) {
      return line.stroke(stroke(0.75f, Fill::var("rule")));
    };
    const auto circle = [&](SkPoint at, float radius) {
      return struck(box()
                        .width(radius * 2 * scale)
                        .height(radius * 2 * scale)
                        .centerAt({at.fX * scale, at.fY * scale})
                        .shape(shapes::circle()));
    };
    Element drawing = stack().width(size).height(size).children(
        {struck(box().cover()), struck(box().inset(kLetterStrip * scale)),
         struck(box().inset(kBorder * scale)),
         struck(box().inset(kBorder * scale).shape(shapes::polygon(4))),
         circle({kCentre, kCentre}, kCentreLoop), circle({kCentre, kCentre}, kOnyx)});
    for (int index = 0; index < 4; ++index) {
      const float angle = kTurn * 0.25f * (float)index;
      drawing.children({circle({kCentre + std::cos(angle) * kOrbit,
                                kCentre + std::sin(angle) * kOrbit},
                               kSatelliteLoop)});
      const float near = kBorder + kCornerInset, far = kSide - near;
      drawing.children({circle({index % 3 == 0 ? near : far, index < 2 ? near : far},
                               kCornerRoundel)});
    }
    return drawing;
  }

  Element apparatus() const {
    const auto& apparatus = words["apparatus"];
    std::vector<sketch::kit::LegendEntry> quarries;
    int order = 0;
    for (const Quarry* quarry :
         {&stone.porphyry, &stone.serpentine, &stone.giallo, &stone.marble, &stone.onyx,
          &stone.purbeck, &stone.red, &stone.turquoise, &stone.cobalt}) {
      quarries.push_back(
          {.label = quarry->name,
           .note = quarry->source,
           .mark = box()
                       .width(26)
                       .height(15)
                       .fill(cut(*quarry, 34, 60.0f + (float)order++))
                       .foreground(stroke(1.0f, Fill::var("ash"))),
           .opacity = sigil::motion::animate({.from = 0.0f, .to = 1.0f, .duration = 320ms}),
           .slide = sigil::motion::animate({.from = -12.0f, .to = 0.0f, .duration = 400ms})});
    }
    // The column is static once its key has been dealt, and its swatches
    // are stone evaluated per pixel, so it is baked once.
    // Every block of the rail stands on one rhythm: the column spends
    // what the blocks leave of the pavement's height as equal gaps between
    // them, so its first line starts on the pavement's top edge and the
    // key's last row closes on its bottom edge.
    return box()
        .key("apparatus")
        .cache(Cache::Texture)
        .column()
        .justifyContent(Justify::SpaceBetween)
        .left(kColumnLeft)
        .top(kMargin)
        .width(kColumnWidth)
        .height(kSide)
        .children(
            {box().column().children(
                 {document::eyebrow(words.phrase(apparatus["eyebrow"])),
                  document::h1(words.phrase(apparatus["title"])).marginTop(10),
                  document::lead(words.phrase(apparatus["lead"]))
                      .marginTop(6)}),
             kit::line(
                 {.length = Dimension(kColumnWidth),
                  .fill = material::Paint::linearGradient(
                      {0, 0}, {kColumnWidth, 0},
                      {draw::parseColor(words["ink"]["rule"].string()),
                       material::withAlpha(
                           draw::parseColor(words["ink"]["rule"].string()), 0)},
                      {.units = material::GradientUnits::Pixels})}),
             document::paragraph(words.phrase(apparatus["reading"])),
             // The quotation and what it is, one indented block.
             document::quote(
                 {document::paragraph(words.phrase(apparatus["quote"])),
                  document::caption(words.phrase(apparatus["attribution"]))})
                 .gap(6),
             box()
                 .row()
                 .gap(18)
                 .alignItems(Align::Center)
                 .children(
                     {settingOut(128).flexShrink(0),
                      box().column().flexShrink(1).children(
                          {document::h2(words.phrase(apparatus["settingOut"]))
                               .marginBottom(8),
                           document::caption(
                               words.phrase(apparatus["settingOutNote"]))})}),
             box().column().styleClass("key").children(
                 {document::h2(words.phrase(apparatus["key"])).marginBottom(12),
                  sketch::kit::legend({.entries = std::move(quarries),
                                       .gap = 6,
                                       .labelGap = 12})
                      .key("quarries")
                      .staggerChildren(60ms)})});
  }

  Element describe() const {
    return stack()
        .applyStyleSheet(sheet())
        .fill(material::Paint::radialGradient(
            {kMargin + kCentre, kMargin + kCentre}, kCanvas.fWidth * 0.8f,
            {hexColor(0x1C1814),
             draw::parseColor(words["ink"]["ground"].string())},
            {.units = material::GradientUnits::Pixels}))
        .children({pavement(), apparatus()});
  }
};

SIGIL_SKETCH_AS(Cosmati, "cosmati", "Study · Pattern",
                "the Great Pavement at Westminster — quincunx, guilloche and "
                "quarried stone, laid in order")
