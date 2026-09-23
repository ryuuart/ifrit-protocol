/** @file
 * coverage_boundary — what a decoration dresses when the visible thing
 * is not the node's shape.
 *
 * `decorationOutline()` names the outline a node's marks are drawn across.
 * `Auto` is the node's own shape, `Glyphs` is the contours a text
 * placement produced, and `Coverage` is the silhouette of WHAT THE NODE
 * DREW — read off its rendered layer rather than off any description of
 * it. That is the only one that can answer for an image with an alpha
 * cut-out, for a subtree whose picture is the union of its children, or
 * for anything else clipped or masked into a shape nobody wrote down.
 *
 * Three consequences of tracing a raster are on this sheet, and they are
 * the whole bargain. The boundary is a STAIRCASE, because it is built
 * from whole pixels. The step size is the NODE'S OWN, because the trace
 * rasterises a fixed number of pixels on the longer side however large
 * the node is. And paint below HALF COVERAGE is not a silhouette, so the
 * same cut-out at 30% alpha traces to nothing at all — whereupon the
 * boundary falls back to the node's own shape, because the marks are
 * what dress a boundary and are never in it, so a node that drew nothing
 * would otherwise have no outline to wear at all.
 *
 * One style value dresses all of them, unchanged.
 *
 * EDIT THESE FIRST
 *   kWash — the alpha the fourth cell's cut-out is drawn at, under the
 *     half a pixel must be covered to join a boundary.
 *   kGlow — the outer glow's blur extent, px.
 */

// TAGS: Materials/Compositing

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <memory>
#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 232;
constexpr float kArt = 176;  // the cut-out's box inside a cell

constexpr float kWash = 0.30f;  // the faint cut-out's alpha, under 0.5
constexpr float kGlow = 11;     // the outer glow's blur extent, px

constexpr material::Color kFigure{0.86f, 0.79f, 0.62f, 1};
constexpr material::Color kHalo{0.36f, 0.72f, 1.00f, 0.95f};

/** The cut-out: a six-pointed star with a hole punched clean through it,
 *  on nothing at all. Its box is a rectangle, its silhouette is neither a
 *  rectangle nor simply connected, and `alpha` is how much of a pixel its
 *  paint covers — the one number a coverage trace asks about. */
std::shared_ptr<const sigil::image::ImageAsset> cutOut(float alpha) {
  constexpr int kSide = 176;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSide, kSide));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f({kFigure.r, kFigure.g, kFigure.b, alpha});
  canvas->drawPath(shapes::star(6, 0.46f, 0.14f).path({kSide, kSide}), paint);
  SkPaint punch;
  punch.setAntiAlias(true);
  punch.setBlendMode(SkBlendMode::kClear);
  canvas->drawPath(
      shapes::circle()
          .path({kSide * 0.30f, kSide * 0.30f})
          .makeTransform(SkMatrix::Translate(kSide * 0.35f, kSide * 0.35f)),
      punch);
  return std::make_shared<const sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(surface->makeImageSnapshot()));
}

/** One style value, worn by every cell that wears one: a halo under the
 *  outline and a recessed band inside it. Neither knows what outline it
 *  will be handed. */
LayerStyle halo() {
  return {.under = {styles::OuterGlow{kHalo, kGlow, 1.0f}},
          .over = {styles::InnerShadow{{0, 0, 0, 0.55f}, {0, 2}, 5}}};
}

/** THE TWO CUT-OUTS THIS SHEET SHOWS, baked once and held together for
 *  the sketch's life: an image is compared by POINTER, so a fresh bake per
 *  cell would make every cell's node unequal to every other's. Held on the
 *  sketch and not in a static, since this file is a dylib a reload
 *  unloads. */
struct CutOuts {
  std::shared_ptr<const sigil::image::ImageAsset> solid = cutOut(1.0f);
  std::shared_ptr<const sigil::image::ImageAsset> faint = cutOut(kWash);
};

Element art(const CutOuts& cut, float alpha = 1.0f) {
  return image(alpha < 1.0f ? cut.faint : cut.solid).width(kArt).height(kArt);
}

sketch::kit::ComparisonCase cell(const char* caseTitle, const char* call,
                                 const char* note, Element body) {
  // The well HOLDS the picture: it stands in the middle at its own size,
  // which is what halving the difference between the two was computing.
  return {.title = caseTitle,
          .control = call,
          .figure = sketch::kit::well({.width = kCell,
                                       .height = kPicture,
                                       .content = sketch::kit::Well::Content{}},
                                      std::move(body)),
          .note = note};
}

}  // namespace

struct CoverageBoundary {
  const CutOuts cut;

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // The union of three discs: a silhouette that exists only once the
    // children have been drawn, so no shape() could have named it.
    const auto disc = [](SkPoint centre, float radius) {
      return kit::dot({centre.x() * (kArt / 132), centre.y() * (kArt / 132)},
                      radius * (kArt / 132), Fill::color(kFigure));
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "Which edge receives the layer style?",
         .subtitle =
             "A rectangle, an alpha silhouette, and the limits of tracing",
         .footer = "Coverage is a raster-derived outline. An empty trace falls "
                   "back to the node’s own shape."},
        box().column().gap(28).children(
            {sketch::kit::sectionHeader(
                 {.label = "ONE IMAGE · TWO OUTLINES", .note = ""}),
             sketch::kit::comparison(
                 {.cases = {cell("SOURCE", "image(cutOut)",
                                 "An opaque star with a transparent hole.",
                                 art(cut)),
                            cell("THE BOX",
                                 "…"
                                 ".layerStyle(halo)",
                                 "Automatic outline: the image rectangle "
                                 "receives the layer style.",
                                 art(cut).layerStyle(halo())),
                            cell("THE DRAWN SILHOUETTE",
                                 "…"
                                 ".decorationOutline(Coverage)"
                                 ".layerStyle(halo)",
                                 "Coverage outline: the visible star and its "
                                 "hole receive the same layer style.",
                                 art(cut)
                                     .decorationOutline(Boundary::Coverage)
                                     .layerStyle(halo()))},
                  .measure = 1020,
                  .gap = 18}),
             box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(18)
                 .children(
                     {box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "WHEN THE OUTLINE IS NOT GIVEN",
                                .note = ""}),
                           sketch::kit::comparison(
                               {.cases = {cell("BELOW THE THRESHOLD",
                                               "the same cut-out at 30% alpha",
                                               "At 30% alpha no pixel reaches "
                                               "the tracing threshold; the box "
                                               "is the fallback.",
                                               art(cut, kWash)
                                                   .decorationOutline(
                                                       Boundary::Coverage)
                                                   .layerStyle(halo())),
                                          cell(
                                              "CHILDREN AS ONE OUTLINE",
                                              "children only · "
                                              "decorationOutline(Coverage)",
                                              "Three children make one "
                                              "silhouette after drawing. No "
                                              "explicit path describes it.",
                                              box()
                                                  .width(kArt)
                                                  .height(kArt)
                                                  .decorationOutline(
                                                      Boundary::Coverage)
                                                  .layerStyle(halo())
                                                  .children({disc({37, 53}, 31),
                                                             disc({79, 39}, 35),
                                                             disc({68, 98},
                                                                  38)}))},
                                .measure = 674,
                                .gap = 18})}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "WHAT IS TRACED", .note = ""}),
                           document::caption(
                               "Coverage reads the completed content, "
                               "including children. The node’s own halo and "
                               "shadow are excluded, so a decoration cannot "
                               "grow the outline it decorates.")
                               .width(328),
                           document::caption(
                               "The halo and recessed shadow are identical "
                               "throughout. Only the decoration outline "
                               "changes.")
                               .width(328)})})})));
  }
};

SIGIL_SKETCH(CoverageBoundary, "Kit · API",
             "one layer style handed the node's box, then the silhouette of "
             "what the node actually drew, and finally a wash too faint to "
             "have one")
