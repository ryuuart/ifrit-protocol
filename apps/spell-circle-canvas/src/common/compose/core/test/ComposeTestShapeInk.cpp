// A node whose ink is the shape it declares is sized to that shape: the
// layer its opacity opens stands over the stroke, not over the page-sized
// box the stroke's coordinates came from, its filter reads the stroke
// grown by the filter's reach, and a mark laid against the box keeps the
// box.

#include <include/core/SkPathBuilder.h>
#include <include/effects/SkImageFilters.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Hatches.h>
#include <sigilcompose/brush/Lines.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/brush/Stamps.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Shapers.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "support/BakeTestSupport.h"

namespace {

constexpr int kPageWidth = 600, kPageHeight = 400;

/** EVERY BOUNDED LAYER A DRAW OPENS, in device pixels, and nothing drawn.
 *  A held picture is replayed through it so the layers recorded inside one
 *  are read as well. */
class LayerLog final : public SkNoDrawCanvas {
 public:
  LayerLog(int width, int height) : SkNoDrawCanvas(width, height) {}
  std::vector<SkRect> layers;

 protected:
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    if (rec.fBounds)
      layers.push_back(this->getTotalMatrix().mapRect(*rec.fBounds));
    return kNoLayer_SaveLayerStrategy;
  }
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }
};

/** A MARK THAT SAYS NOTHING ABOUT WHERE IT STANDS and draws nothing. */
struct MarkOnTheBox {
  bool operator==(const MarkOnTheBox&) const = default;
  void paint(sigil::draw::Pen&, const PaintContext&) const {}
};

/** The spine every case strokes, in page coordinates: one bent run whose
 *  bounds are (100, 100) to (160, 120). */
Shape spine() {
  return Shape(skiaShape([] {
    SkPathBuilder builder;
    builder.moveTo(100, 100);
    builder.lineTo(130, 120);
    builder.lineTo(160, 104);
    return builder.detach();
  }));
}

/** The keyline's width, and how far its stated bleed reaches past the
 *  spine: an outside keyline mitred at the default limit of four widths. */
constexpr float kWidth = 6;
constexpr float kReach = kWidth * 4;

/** The blur every case but the wide ones runs. */
constexpr float kSigma = 4;

/** The stroke laid on a box the size of the page, faded and, for a
 *  positive @p sigma, blurred by it, as a height map lays its marks;
 *  @p onTheBox adds a mark laid on the box. */
Element strokeOnThePage(bool onTheBox, float sigma = kSigma) {
  Element stroke = box()
                       .absolute()
                       .left(0)
                       .top(0)
                       .width(kPageWidth)
                       .height(kPageHeight)
                       .shape(spine())
                       .stroke(material::Material(material::Color{1, 1, 1, 1}),
                               {.width = kWidth})
                       .opacity(0.5f)
                       .cache(Cache::None);
  if (sigma > 0)
    stroke.filter(
        material::skia::filter(SkImageFilters::Blur(sigma, sigma, nullptr)));
  if (onTheBox) stroke.foreground(MarkOnTheBox{});
  Element page = box().width(kPageWidth).height(kPageHeight);
  page.children({std::move(stroke)});
  return page;
}

std::vector<SkRect> layersOf(Element page) {
  Host host(kPageWidth, kPageHeight);
  host.composer.render(std::move(page));
  LayerLog log(kPageWidth, kPageHeight);
  // Several frames, so a layer recorded into a held picture is read on
  // the frames that replay it as well as on the one that records it.
  for (int frame = 0; frame < 3; ++frame) host.composer.draw(log);
  return log.layers;
}

std::vector<SkColor> drawnOnce(Element page) {
  Host host(kPageWidth, kPageHeight);
  host.composer.render(std::move(page));
  host.frame();
  return surfaceOf(host, kPageWidth, kPageHeight);
}

}  // namespace

TEST(ComposePaintBounds, AShapedLeafIsBoundedByItsShape) {
  // Painted live, so the fade's layer is opened on the canvas itself.
  Element page = strokeOnThePage(false, /*sigma=*/0);
  page.cache(Cache::None);
  const std::vector<SkRect> layers = layersOf(std::move(page));
  ASSERT_FALSE(layers.empty())
      << "the faded stroke opened no bounded layer at all";
  const SkRect ink =
      SkRect::MakeLTRB(100 - kReach, 100 - kReach, 160 + kReach, 120 + kReach);
  for (const SkRect& layer : layers)
    EXPECT_EQ(layer, ink)
        << "a layer stood at (" << layer.left() << ", " << layer.top()
        << ") to (" << layer.right() << ", " << layer.bottom()
        << ") where the stroke's ink is its spine grown by the keyline's "
           "reach, ("
        << ink.left() << ", " << ink.top() << ") to (" << ink.right() << ", "
        << ink.bottom() << ")";
}

TEST(ComposePaintBounds, AFilterOverAShapedLeafFiltersItsInk) {
  // The stroke's own blur filters the stroke's ink grown by the blur's
  // reach, on the filter grid: a layer the stroke's size, never the
  // page's, at every width of blur.
  for (const float sigma : {kSigma, 12.0f, 24.0f, 48.0f}) {
    const std::vector<SkRect> layers = layersOf(strokeOnThePage(false, sigma));
    ASSERT_FALSE(layers.empty())
        << "the faded, blurred stroke opened no bounded layer at all";
    const SkRect ink = SkRect::MakeLTRB(100 - kReach, 100 - kReach,
                                        160 + kReach, 120 + kReach);
    // The ink grown by three deviations, then out by one cell of the grid.
    const float reach = 3 * sigma + 64;
    const SkRect allowed = ink.makeOutset(reach, reach);
    for (const SkRect& layer : layers) {
      EXPECT_TRUE(layer.contains(ink))
          << "blur " << sigma << ": a layer stood at (" << layer.left() << ", "
          << layer.top() << ") to (" << layer.right() << ", " << layer.bottom()
          << ") and cut the stroke's ink";
      EXPECT_TRUE(allowed.contains(layer))
          << "blur " << sigma << ": a layer stood at (" << layer.left() << ", "
          << layer.top() << ") to (" << layer.right() << ", " << layer.bottom()
          << "), past the stroke's ink grown by the "
          << "blur's reach";
    }
  }
}

TEST(ComposePaintBounds, AMarkLaidOnTheBoxKeepsTheBox) {
  const std::vector<SkRect> layers = layersOf(strokeOnThePage(true));
  ASSERT_FALSE(layers.empty())
      << "the faded, blurred stroke opened no bounded layer at all";
  for (const SkRect& layer : layers)
    EXPECT_TRUE(layer.contains(SkRect::MakeWH(kPageWidth, kPageHeight)))
        << "a layer stood at (" << layer.left() << ", " << layer.top()
        << ") to (" << layer.right() << ", " << layer.bottom()
        << ") although a mark that says nothing of where it stands may "
           "draw anywhere on the page-sized box";
}

/** The stroke under a wide blur standing on the page above it rather
 *  than on the stroke; @p onTheBox adds a mark laid on the stroke's box. */
Element blurredAbove(bool onTheBox, float sigma) {
  Element stroke = box()
                       .absolute()
                       .left(0)
                       .top(0)
                       .width(kPageWidth)
                       .height(kPageHeight)
                       .shape(spine())
                       .stroke(material::Material(material::Color{1, 1, 1, 1}),
                               {.width = kWidth});
  if (onTheBox) stroke.foreground(MarkOnTheBox{});
  Element sheet = box()
                      .absolute()
                      .left(0)
                      .top(0)
                      .width(kPageWidth)
                      .height(kPageHeight)
                      .filter(material::skia::filter(
                          SkImageFilters::Blur(sigma, sigma, nullptr)));
  sheet.children({std::move(stroke)});
  Element page = box().width(kPageWidth).height(kPageHeight);
  page.children({std::move(sheet)});
  return page;
}

TEST(ComposePaintBounds, AShapedLeafFiltersTheSamePictureUnderAWideBlur) {
  // A wide blur standing on the page above the stroke filters the page's
  // extent whatever size the stroke's own layers are, so the stroke at its
  // ink and at its box gives one picture under it.
  for (const float sigma : {12.0f, 24.0f, 48.0f}) {
    size_t differing = 0;
    const std::vector<SkColor> aboveAtTheInk =
        drawnOnce(blurredAbove(false, sigma));
    const std::vector<SkColor> aboveAtTheBox =
        drawnOnce(blurredAbove(true, sigma));
    const int worst = worstDrift(aboveAtTheBox, aboveAtTheInk, &differing);
    EXPECT_EQ(worst, 0) << differing << " pixels moved, worst " << worst
                        << " code values, under a blur of " << sigma
                        << " standing above the stroke, at its ink and at "
                           "its box";
  }
}

TEST(ComposePaintBounds, AShapedLeafFiltersTheSamePictureAtItsInk) {
  // The layer at the ink and the layer at the box hold the same stroke and
  // run the same blur over it, so the two pictures are the same pixels.
  const std::vector<SkColor> atTheInk = drawnOnce(strokeOnThePage(false));
  const std::vector<SkColor> atTheBox = drawnOnce(strokeOnThePage(true));
  size_t differing = 0;
  const int worst = worstDrift(atTheBox, atTheInk, &differing);
  EXPECT_EQ(worst, 0) << differing << " pixels moved, worst " << worst
                      << " code values, between the stroke filtered in a "
                         "layer at its ink and in one at its box";
}

namespace {

/** A star whose ink begins a fraction of a unit into its page-sized box, so
 *  a bake sized to the ink starts between the texels a bake of the box is
 *  laid on. */
Shape offsetStar() {
  return Shape(skiaShape([] {
    SkPathBuilder builder;
    builder.moveTo(140.3f, 80.3f);
    builder.lineTo(152.7f, 121.1f);
    builder.lineTo(190.6f, 124.9f);
    builder.lineTo(158.9f, 146.2f);
    builder.lineTo(171.4f, 186.8f);
    builder.lineTo(140.3f, 160.4f);
    builder.lineTo(109.2f, 186.8f);
    builder.lineTo(121.7f, 146.2f);
    builder.lineTo(90.1f, 124.9f);
    builder.lineTo(127.9f, 121.1f);
    builder.close();
    return builder.detach();
  }));
}

/** The star filled, faded and kept as a texture on a box the size of the
 *  page; @p onTheBox adds a mark laid on the box. */
Element bakedStarOnThePage(bool onTheBox) {
  Element star =
      box()
          .absolute()
          .left(0)
          .top(0)
          .width(kPageWidth)
          .height(kPageHeight)
          .shape(offsetStar())
          .fill(material::Material(material::Color{1, 0.8f, 0.2f, 1}))
          .opacity(0.7f)
          .cache(Cache::Texture);
  if (onTheBox) star.foreground(MarkOnTheBox{});
  Element page = box().width(kPageWidth).height(kPageHeight);
  page.children({std::move(star)});
  return page;
}

/** The page drawn at @p density bake pixels a unit, over a view @p scale
 *  times the page. */
std::vector<SkColor> bakedAt(Element page, float density, float scale) {
  const int width = (int)std::ceil(kPageWidth * scale);
  const int height = (int)std::ceil(kPageHeight * scale);
  Host host(width, height);
  host.composer.setBakeDensity(density);
  host.composer.render(std::move(page));
  for (int frame = 0; frame < 2; ++frame) {
    SkCanvas* canvas = host.surface->getCanvas();
    canvas->clear(SK_ColorBLACK);
    canvas->save();
    canvas->scale(scale, scale);
    host.composer.draw(*canvas);
    canvas->restore();
  }
  return surfaceOf(host, width, height);
}

}  // namespace

TEST(ComposePaintBounds, AShapedLeafBakesThePictureItsBoxBakes) {
  // A texture bake is blitted back with linear sampling and a device bake
  // runs its effects over the surface it is given, so the texels a bake
  // holds depend on where its surface starts: a surface laid from the ink
  // rather than the box puts every texel a fraction off and softens every
  // edge it holds. Sized to its ink or to its box, the star bakes the same
  // picture, whatever the density and whatever the view.
  for (const float density : {1.0f, 2.0f, 3.0f}) {
    for (const float scale : {1.0f, 2.0f}) {
      const std::vector<SkColor> atTheInk =
          bakedAt(bakedStarOnThePage(false), density, scale);
      const std::vector<SkColor> atTheBox =
          bakedAt(bakedStarOnThePage(true), density, scale);
      size_t differing = 0;
      const int worst = worstDrift(atTheBox, atTheInk, &differing);
      EXPECT_EQ(worst, 0) << differing << " pixels moved, worst " << worst
                          << " code values, between the star baked at its "
                             "ink and at its box, at "
                          << density << " bake pixels a unit and a view "
                          << scale << " times the page";
    }
  }
}

namespace {

/** A closed triangle in page coordinates whose bounds are (100, 100) to
 *  (160, 140). */
Shape triangle() {
  return Shape(skiaShape([] {
    SkPathBuilder builder;
    builder.moveTo(100, 100);
    builder.lineTo(160, 104);
    builder.lineTo(130, 140);
    builder.close();
    return builder.detach();
  }));
}

const SkRect kTriangleBounds = SkRect::MakeLTRB(100, 100, 160, 140);

/** The triangle filled and faded by @p opacity on a box the size of the
 *  page, wearing @p mark over its fill, painted live so the fade's layer
 *  is opened on the canvas itself. */
Element markedOnThePage(const Decoration& mark, float opacity = 0.5f) {
  Element shaped = box()
                       .absolute()
                       .left(0)
                       .top(0)
                       .width(kPageWidth)
                       .height(kPageHeight)
                       .shape(triangle())
                       .fill(material::Material(material::Color{1, 1, 1, 1}))
                       .opacity(opacity)
                       .cache(Cache::None);
  shaped.foreground(mark);
  Element page = box().width(kPageWidth).height(kPageHeight);
  page.children({std::move(shaped)});
  page.cache(Cache::None);
  return page;
}

/** A small square of art for the stamped and swept brushes. */
Element stampArt() {
  return box().width(4).height(4).fill(
      material::Material(material::Color{1, 0.5f, 0.2f, 1}));
}

/** A plain stroke whose corners and ends stay inside its half width. */
PathFormat roundStroke(float width = 4) {
  PathFormat stroke = brush::solid(width, Fill::color({1, 1, 1, 1}));
  stroke.join = geometry::path::Join::Round;
  return stroke;
}

struct MarkCase {
  std::string name;
  Decoration mark;
  bool staysOnOutline;
};

std::vector<MarkCase> markCases() {
  std::vector<MarkCase> cases;
  const auto add = [&](std::string name, Decoration mark, bool onOutline) {
    cases.push_back({std::move(name), std::move(mark), onOutline});
  };
  add("a round-joined stroke", roundStroke(), true);
  PathFormat outer = roundStroke();
  outer.align = PathFormat::Align::Outer;
  add("a round-joined outer stroke", outer, true);
  PathFormat inner =
      brush::solid(4, Fill::color({1, 1, 1, 1}), PathFormat::Align::Inner);
  add("a mitred inner stroke", inner, true);
  add("a mitred stroke", brush::solid(4, Fill::color({1, 1, 1, 1})), true);
  PathFormat square = roundStroke();
  square.cap = geometry::path::Cap::Square;
  add("a square-capped stroke", square, true);
  PathFormat stamped = roundStroke();
  stamped.stampPath = geometry::path::fromSk([] {
    SkPathBuilder spike;
    spike.moveTo(0, -10);
    spike.lineTo(3, 10);
    spike.lineTo(-3, 10);
    spike.close();
    return spike.detach();
  }());
  stamped.stampAdvance = 12;
  add("a stamped stroke", stamped, false);
  add("a shadow", shadow(material::Color{0, 0, 0, 1}, {3, 4}, 2), true);
  add("a wash",
      decorations::wash(material::Material(material::Color{0, 0, 1, 1})), true);
  Border border = decorations::border(2, Fill::color({1, 1, 1, 1}), 1);
  border.join = geometry::path::Join::Round;
  add("a round-joined border", border, true);
  border.inset = -2;
  add("a border set outside the outline", border, false);
  add("a brush", Brush{}.layer(roundStroke()), true);
  add("a brush over a mitred stroke",
      Brush{}.layer(brush::solid(4, Fill::color({1, 1, 1, 1}))), true);
  add("layers", brush::layers({roundStroke(), roundStroke(2)}), true);
  add("a restyled stroke",
      brush::restyle(geometry::shapers::Rounded{6}, roundStroke()), true);
  add("a stroke on the top edges",
      onEdges(geometry::path::Edge::Top, roundStroke()), true);
  add("a stroke on an inward copy", inset(2, roundStroke()), true);
  add("a stroke on an outward copy", inset(-2, roundStroke()), false);
  add("a line", lines::Line{}, true);
  lines::Line mitredLine;
  mitredLine.join = geometry::path::Join::Miter;
  add("a mitred line", mitredLine, true);
  add("rails",
      lines::rails({{.across = -3, .width = 2}, {.across = 3, .width = 2}}),
      true);
  add("mitred rails",
      lines::rails(
          {{.across = -3, .width = 2, .join = geometry::path::Join::Miter},
           {.across = 3, .width = 2, .join = geometry::path::Join::Miter}}),
      true);
  add("square-ended rails",
      lines::rails(
          {{.across = -3, .width = 2, .cap = geometry::path::Cap::Square},
           {.across = 3, .width = 2, .cap = geometry::path::Cap::Square}}),
      true);
  add("a hatch", lines::Hatch{}, true);
  add("a radial hatch", lines::RadialHatch{}, true);
  add("a layered brush", LayeredBrush{{StrokeLayer{.width = 2}}}, true);
  add("a bevel pair", styles::BevelPair{}, true);
  add("a bevel", kit::Bevel{}, true);
  kit::Bevel outside;
  outside.inner = kit::BevelInner{.gap = -2};
  add("a bevel whose inner ring stands outside", outside, false);
  add("a relief",
      relief(material::Material(material::Color{0.5f, 0.5f, 0.5f, 1})), true);
  add("a scatter",
      brush::Scatter{.art = stampArt(), .spacing = 12, .bleedPx = 6}, true);
  add("a pattern", brush::Pattern{.side = stampArt(), .bleedPx = 6}, true);
  brush::Art art = brush::artAlong(stampArt(), 6);
  art.bleedPx = 6;
  add("swept art", art, true);
  add("brackets", styles::brackets(material::Color{1, 1, 1, 1}), false);
  add("a tick rail", styles::tickRail(material::Color{1, 1, 1, 1}), false);
  add("a slice", Slice{}, false);
  add("a contour walk", ContourWalk{}, false);
  ContourWalk declared;
  declared.stamp = stampArt();
  declared.bleedPx = 6;
  add("a contour walk declaring its reserve", declared, true);
  return cases;
}

}  // namespace

TEST(ComposePaintBounds, EveryMarkStandsOnTheOutlineOrOnTheBox) {
  // A mark on the outline sizes a shaped node to the shape grown by its
  // bleed; a mark laid against the box, or one whose bleed does not hold
  // its whole reach from the outline, keeps the box.
  const SkRect page = SkRect::MakeWH(kPageWidth, kPageHeight);
  for (const MarkCase& markCase : markCases()) {
    SCOPED_TRACE(markCase.name);
    EXPECT_EQ(markCase.mark.staysOnOutline(), markCase.staysOnOutline)
        << markCase.name << " says the wrong thing about where it stands";
    const std::vector<SkRect> layers = layersOf(markedOnThePage(markCase.mark));
    ASSERT_FALSE(layers.empty())
        << "the faded triangle wearing " << markCase.name
        << " opened no bounded layer at all";
    const float bleed =
        markCase.mark.bleed({(float)kPageWidth, (float)kPageHeight});
    const SkRect ink = kTriangleBounds.makeOutset(bleed, bleed);
    for (const SkRect& layer : layers) {
      if (markCase.staysOnOutline)
        EXPECT_EQ(layer, ink)
            << "a layer stood at (" << layer.left() << ", " << layer.top()
            << ") to (" << layer.right() << ", " << layer.bottom()
            << ") where the triangle wearing " << markCase.name
            << " is its bounds grown by the mark's bleed, (" << ink.left()
            << ", " << ink.top() << ") to (" << ink.right() << ", "
            << ink.bottom() << ")";
      else
        EXPECT_TRUE(layer.contains(page))
            << "a layer stood at (" << layer.left() << ", " << layer.top()
            << ") to (" << layer.right() << ", " << layer.bottom()
            << ") although " << markCase.name
            << " may stand anywhere on the page-sized box";
    }
  }
}

TEST(ComposePaintBounds, AFadedShapeHoldsTheMitredTipsItsMarksLay) {
  // The triangle's sharpest corner turns by about 140 degrees, so a mitre
  // there stands about three half-widths past it, and the layer its fade
  // opens is sized from the marks' bleeds: every pixel the shape and its
  // mark paint unfaded is still painted faded.
  std::vector<std::pair<std::string, Decoration>> marks;
  marks.emplace_back("the stock stroke",
                     brush::solid(6, Fill::color({1, 1, 1, 1})));
  marks.emplace_back(
      "an outer stroke",
      brush::solid(4, Fill::color({1, 1, 1, 1}), PathFormat::Align::Outer));
  marks.emplace_back("a border",
                     decorations::border(6, Fill::color({1, 1, 1, 1})));
  marks.emplace_back("a layered brush",
                     LayeredBrush{{StrokeLayer{
                         .width = 6, .ink = Fill::color({1, 1, 1, 1})}}});
  lines::Line mitredLine;
  mitredLine.width = 6;
  mitredLine.join = geometry::path::Join::Miter;
  marks.emplace_back("a mitred line", mitredLine);
  marks.emplace_back("mitred rails",
                     lines::rails({{.across = -4,
                                    .width = 3,
                                    .fill = Fill::color({1, 1, 1, 1}),
                                    .join = geometry::path::Join::Miter},
                                   {.across = 4,
                                    .width = 3,
                                    .fill = Fill::color({1, 1, 1, 1}),
                                    .join = geometry::path::Join::Miter}}));
  const auto lostWhenFaded = [](const std::function<Element(float)>& page) {
    const std::vector<SkColor> whole = drawnOnce(page(1));
    const std::vector<SkColor> faded = drawnOnce(page(0.5f));
    // A pixel the antialiased edge barely touches may round to nothing at
    // half its strength, so only one the mark plainly paints is counted.
    size_t lost = 0;
    for (size_t i = 0; i < whole.size(); ++i)
      if (SkColorGetR(whole[i]) >= 16 && faded[i] == SK_ColorBLACK) ++lost;
    return lost;
  };
  for (const auto& [name, mark] : marks)
    EXPECT_EQ(lostWhenFaded([&mark](float opacity) {
                return markedOnThePage(mark, opacity);
              }),
              0u)
        << "the faded triangle wearing " << name
        << " lost pixels its unfaded paint draws";
  // A material's own stroke step, laid by its effects outside the shape.
  const size_t lost = lostWhenFaded([](float opacity) {
    Element shaped = box()
                         .absolute()
                         .left(0)
                         .top(0)
                         .width(kPageWidth)
                         .height(kPageHeight)
                         .shape(triangle())
                         .fill(material::from(material::Color{1, 1, 1, 1})
                                   .effects(material::Filter::stroke(
                                       {1, 1, 1, 1}, {.width = 6})))
                         .opacity(opacity)
                         .cache(Cache::None);
    Element page = box().width(kPageWidth).height(kPageHeight);
    page.children({std::move(shaped)});
    page.cache(Cache::None);
    return page;
  });
  EXPECT_EQ(lost, 0u) << "the faded triangle whose material strokes it lost "
                      << lost << " pixels its unfaded paint draws";
}

TEST(ComposePaintBounds, AShapedLeafCarryingBracketsKeepsItsBox) {
  const std::vector<SkRect> layers =
      layersOf(markedOnThePage(styles::brackets(material::Color{1, 1, 1, 1})));
  ASSERT_FALSE(layers.empty())
      << "the faded triangle wearing brackets opened no bounded layer";
  for (const SkRect& layer : layers)
    EXPECT_TRUE(layer.contains(SkRect::MakeWH(kPageWidth, kPageHeight)))
        << "a layer stood at (" << layer.left() << ", " << layer.top()
        << ") to (" << layer.right() << ", " << layer.bottom()
        << ") although brackets stand at the corners of the page-sized box";
}
