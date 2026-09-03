/** @file
 * coverage_boundary — what a decoration dresses when the visible thing
 * is not the node's shape.
 *
 * `boundary()` names the outline a node's marks are drawn across.
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

#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

#include <memory>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 200;
constexpr float kPicture = 200;
constexpr float kArt = 132;  // the cut-out's box inside a cell

constexpr float kWash = 0.30f;  // the faint cut-out's alpha, under 0.5
constexpr float kGlow = 11;     // the outer glow's blur extent, px

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFigure{0.86f, 0.79f, 0.62f, 1};
constexpr SkColor4f kHalo{0.36f, 0.72f, 1.00f, 0.95f};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

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
  paint.setColor4f({kFigure.fR, kFigure.fG, kFigure.fB, alpha});
  canvas->drawPath(shapes::star(6, 0.46f, 0.14f).path({kSide, kSide}), paint);
  SkPaint punch;
  punch.setAntiAlias(true);
  punch.setBlendMode(SkBlendMode::kClear);
  canvas->drawPath(
      shapes::circle().path({kSide * 0.30f, kSide * 0.30f})
          .makeTransform(SkMatrix::Translate(kSide * 0.35f, kSide * 0.35f)),
      punch);
  return std::make_shared<const sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(surface->makeImageSnapshot()));
}

/** One style value, worn by every cell that wears one: a halo under the
 *  outline and a recessed band inside it. Neither knows what outline it
 *  will be handed. */
LayerStyle halo() {
  LayerStyle style;
  style.under.push_back(styles::OuterGlow{kHalo, kGlow, 1.0f});
  style.over.push_back(styles::InnerShadow{{0, 0, 0, 0.55f}, {0, 2}, 5});
  return style;
}

Element art(float alpha = 1.0f) {
  static const std::shared_ptr<const sigil::image::ImageAsset> solid =
      cutOut(1.0f);
  static const std::shared_ptr<const sigil::image::ImageAsset> faint =
      cutOut(kWash);
  return image(alpha < 1.0f ? faint : solid)
      .width(Dim(kArt))
      .height(Dim(kArt));
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .width(Dim(kCell))
                       .height(Dim(kPicture))
                       .clip()
                       .fill(Fill::color(kCellGround))
                       .child(std::move(body).absolute().inset(
                           (kCell - kArt) / 2, (kPicture - kArt) / 2,
                           (kCell - kArt) / 2, (kPicture - kArt) / 2)));
}

}  // namespace

struct CoverageBoundary final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // The union of three discs: a silhouette that exists only once the
    // children have been drawn, so no shape() could have named it.
    const auto disc = [](float x, float y, float d) {
      return box()
          .absolute()
          .left(Dim(x))
          .top(Dim(y))
          .width(Dim(d))
          .height(Dim(d))
          .shape(shapes::circle())
          .fill(Fill::color(kFigure));
    };

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("COVERAGE BOUNDARY \xc2\xb7 "
                           "Element::boundary(Boundary::Coverage)"),
             .subtitle = toU8("dials \xc2\xb7 the boundary \xc2\xb7 the "
                              "cut-out's alpha (0.30, under the half a "
                              "pixel must be covered to join) \xc2\xb7 the "
                              "glow's blur (11 px) \xc2\xb7 one style value "
                              "for every cell"),
             .footer = toU8("Coverage costs a raster and a trace whenever "
                            "the node's layer is invalidated, and the "
                            "node's OWN decorations are never in it "
                            "\xe2\x80\x94 a mark that dressed itself would "
                            "have no fixed point"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("image(cutOut)",
                           "the source \xc2\xb7 an opaque star with a hole "
                           "punched through it, and a rectangle of nothing "
                           "around both",
                           art()),
                      cell("\xe2\x80\xa6" ".style(halo)",
                           "Boundary::Auto is the node's own shape \xc2\xb7 "
                           "the halo hugs the BOX, which is what the "
                           "picture is not",
                           art().style(halo())),
                      cell("\xe2\x80\xa6" ".boundary(Coverage).style(halo)",
                           "the same style on the traced silhouette \xc2\xb7 "
                           "a staircase of whole pixels, which is what "
                           "reading a raster gives",
                           art().boundary(Boundary::Coverage)
                               .style(halo())),
                      cell("the same cut-out at 30% alpha",
                           "under half a pixel covered is not a silhouette "
                           "\xc2\xb7 the trace comes back EMPTY, and an "
                           "empty trace keeps the node's own shape",
                           art(kWash)
                               .boundary(Boundary::Coverage)
                               .style(halo())),
                      cell("children only \xc2\xb7 boundary(Coverage)",
                           "the content and the CHILDREN are in the trace "
                           "\xc2\xb7 three discs, one outline, and no "
                           "shape() that could have said it",
                           box()
                               .boundary(Boundary::Coverage)
                               .style(halo())
                               .child(disc(6, 22, 62))
                               .child(disc(44, 4, 70))
                               .child(disc(30, 60, 76)))},
                 .gap = 12}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(CoverageBoundary, "Kit \xc2\xb7 API",
             "one layer style handed the node's box, then the silhouette of "
             "what the node actually drew, and finally a wash too faint to "
             "have one")
