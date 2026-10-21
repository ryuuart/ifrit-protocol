// A node whose ink is the shape it declares is sized to that shape: the
// layer its opacity and its filter open stands over the stroke, not over
// the page-sized box the stroke's coordinates came from, it filters the
// same picture there, and a mark laid against the box keeps the box.

#include <include/core/SkPathBuilder.h>
#include <include/effects/SkImageFilters.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

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

/** The stroke laid on a box the size of the page, faded and blurred, as a
 *  height map lays its marks; @p onTheBox adds a mark laid on the box. */
Element strokeOnThePage(bool onTheBox) {
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
          .filter(material::skia::filter(SkImageFilters::Blur(4, 4, nullptr)));
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
  const std::vector<SkRect> layers = layersOf(strokeOnThePage(false));
  ASSERT_FALSE(layers.empty())
      << "the faded, blurred stroke opened no bounded layer at all";
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
