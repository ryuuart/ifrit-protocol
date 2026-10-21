// A node whose ink is the shape it declares is sized to that shape: the
// layer its opacity and its filter open stands over the stroke, not over
// the page-sized box the stroke's coordinates came from, it filters the
// same picture there, and a mark laid against the box keeps the box.

#include <include/core/SkPathBuilder.h>
#include <include/effects/SkImageFilters.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

#include <cmath>
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
  Element stroke =
      box()
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
  const SkRect ink = SkRect::MakeLTRB(100 - kReach, 100 - kReach,
                                      160 + kReach, 120 + kReach);
  for (const SkRect& layer : layers)
    EXPECT_EQ(layer, ink)
        << "a layer stood at (" << layer.left() << ", " << layer.top()
        << ") to (" << layer.right() << ", " << layer.bottom()
        << ") where the stroke's ink is its spine grown by the keyline's "
           "reach, ("
        << ink.left() << ", " << ink.top() << ") to (" << ink.right() << ", "
        << ink.bottom() << ")";
}

TEST(ComposePaintBounds, AFilterOverAShapedLeafReadsTheBox) {
  // A blur is exact to the extent it is given and to no other, so the
  // stroke's own blur filters the extent its box gives it.
  const std::vector<SkRect> layers = layersOf(strokeOnThePage(false));
  ASSERT_FALSE(layers.empty())
      << "the faded, blurred stroke opened no bounded layer at all";
  for (const SkRect& layer : layers)
    EXPECT_TRUE(layer.contains(SkRect::MakeWH(kPageWidth, kPageHeight)))
        << "a layer stood at (" << layer.left() << ", " << layer.top()
        << ") to (" << layer.right() << ", " << layer.bottom()
        << ") where the blur over the stroke reads its page-sized box";
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
  // A wide blur is taken over a reduced copy of what it filters and spread
  // back, and its faint far edge reaches past the kernel's nominal reach:
  // the layer at the ink has to hold that edge as the layer at the box
  // does, whether the blur is the stroke's own or stands above it.
  for (const float sigma : {12.0f, 24.0f, 48.0f}) {
    const std::vector<SkColor> ownAtTheInk =
        drawnOnce(strokeOnThePage(false, sigma));
    const std::vector<SkColor> ownAtTheBox =
        drawnOnce(strokeOnThePage(true, sigma));
    size_t differing = 0;
    int worst = worstDrift(ownAtTheBox, ownAtTheInk, &differing);
    EXPECT_EQ(worst, 0) << differing << " pixels moved, worst " << worst
                        << " code values, under the stroke's own blur of "
                        << sigma << " at its ink and at its box";
    const std::vector<SkColor> aboveAtTheInk =
        drawnOnce(blurredAbove(false, sigma));
    const std::vector<SkColor> aboveAtTheBox =
        drawnOnce(blurredAbove(true, sigma));
    worst = worstDrift(aboveAtTheBox, aboveAtTheInk, &differing);
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
  Element star = box()
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
