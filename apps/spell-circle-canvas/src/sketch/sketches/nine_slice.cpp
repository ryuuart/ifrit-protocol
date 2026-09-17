/** @file
 * One generated carved texture, divided into nine regions.
 * The source guides identify the fixed corner bands. Two comparisons isolate
 * source density and the drawing path; a live destination shows the edge and
 * centre bands stretching. Slice and skia::draw::drawLattice both decompose
 * the image into rectangles supported by raster and device backends.
 */

// TAGS: Geometry/Layout, Materials/Compositing

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilskia/draw/Direct.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;

namespace {

constexpr SkSize kSceneSize = {1100, 830};
/** The panels every comparison is drawn in — one size, so what differs
 *  between two cells is the one thing the cell is about. */
constexpr float kPanelW = 250, kPanelH = 96;
/** The frame is drawn at twice the size it is used at, so its bands stay
 *  sharp on a dense display; every consumer repeats that factor as the
 *  slice's density. */
constexpr float kFrameDensity = 2.0f;

constexpr SkColor4f kInk{0.86f, 0.88f, 0.94f, 1};
constexpr SkColor4f kAsh{0.60f, 0.64f, 0.73f, 1};
constexpr SkColor4f kRule{0.22f, 0.23f, 0.30f, 1};
constexpr SkColor4f kQuest{0.169f, 0.110f, 0.043f, 1};

/** THIS SHEET'S LOOK, and its one voice: the call over the panel, what
 *  it did under it. The page's ground is a shade off the canvas's, which
 *  is black, so the margin around the sheet reads as a border. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette = {.ground = {0.055f, 0.055f, 0.075f, 1},
                  .ink = kInk,
                  .ash = kAsh,
                  .rule = kRule};
  look.type.title = {.size = 26, .track = 3};
  look.type.captionLabel = {.size = 12.5f, .track = 0.4f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 34;
  look.spacing.marginBottom = 22;
  look.spacing.captionGap = 7;
  return look;
}

/** The panel every cell shows: the frame stretched over a box of one
 *  size, with room inside it for a line of type. 24 clears the carved
 *  corner bosses, which reach 0.215 of the 96-unit band in from the
 *  edge. */
Element panel(Slice frame, Utf8 caption, SkColor4f ink) {
  return kit::centred()
      .width(kPanelW)
      .height(kPanelH)
      .background(std::move(frame))
      .padding(24)

      .children(
          {text(std::move(caption)).font({.size = 17, .track = 0}).ink(ink)});
}

/** THE DIRECT DOOR, in a leaf of its own: `skia::draw::drawLattice`
 *  against the same divs the `Slice` beside it declares. Nothing here
 *  goes through a decoration, which is the point — the call is what a
 *  program of one's own reaches for, and it paints the same rects. */
Element directLattice(std::shared_ptr<sigil::image::ImageAsset> asset) {
  return kit::centred()
      .width(kPanelW)
      .height(kPanelH)

      // The asset is the only captured input to this keyed draw.
      .children(
          {custom("lattice.direct",
                  [asset = std::move(asset)](SkCanvas& canvas,
                                             const PaintContext& ctx) {
                    const sk_sp<SkImage> image =
                        asset ? asset->frameAt(0).image : nullptr;
                    if (!image) return;
                    const int side = image->width();
                    const std::vector<int> xs{side / 3, side * 2 / 3};
                    const std::vector<int> ys{side / 3, side * 2 / 3};
                    sigil::skia::draw::drawLattice(
                        canvas, image, xs, ys,
                        SkRect::MakeWH(ctx.size.width(), ctx.size.height()),
                        SkFilterMode::kLinear);
                  })
               .cover(),
           text(u8"DIRECT").font({.size = 17, .track = 0}).ink(kQuest)});
}

struct NineSlice {
  std::shared_ptr<sigil::image::ImageAsset> oak, crimson;
  /** The trap's row compares two DRAW PATHS, so both of its cells wear a
   *  texture drawn at the size it is used at: the native call has no
   *  density of its own, and a pair that also differed in weight would
   *  be comparing two things at once. */
  std::shared_ptr<sigil::image::ImageAsset> azurePlain;
  float stretch = 0.0f;

  static std::shared_ptr<sigil::image::ImageAsset> generate(
      const Palette& pal, float density = kFrameDensity) {
    // The intermediate canvas: draw the carved frame once, wrap the
    // snapshot, stretch it everywhere below.
    return std::make_shared<sigil::image::ImageAsset>(
        sigil::image::ImageAsset::wrap(
            makeCarvedFrame(pal, (int)(96 * density))));
  }

  Element describe() {
    const sketch::kit::Provide look(sheetTheme());
    const float breathW = kPanelW + 66 * stretch;
    const float breathH = kPanelH + 26 * stretch;

    Element sourceImage = box().width(192).height(192).children(
        {image(oak).cover(),
         box().left(64).top(0).width(1).height(192).fill(Fill::color(kInk)),
         box().left(128).top(0).width(1).height(192).fill(Fill::color(kInk)),
         box().left(0).top(64).width(192).height(1).fill(Fill::color(kInk)),
         box().left(0).top(128).width(192).height(1).fill(Fill::color(kInk))});
    Element source = box().column().gap(18).width(288).children(
        {document::label("ONE SOURCE TEXTURE"),
         text("192 × 192 px · 2× density").styleClass("readout"),
         sketch::kit::well({.width = 288,
                            .height = 244,
                            .content = sketch::kit::Well::Content{}},
                           std::move(sourceImage)),
         text("The four corners retain their size. Edge bands stretch along "
              "one axis; the centre stretches along both.")
             .width(288),
         text("The guides mark thirds of the source image. Density converts "
              "those source pixels into layout units.")
             .width(288)});
    Element density = sketch::kit::comparison(
        {.cases = {{.title = "DECLARED DENSITY",
                    .control = "Slice::density = 2",
                    .figure = panel(carvedFrameSlice(oak, kFrameDensity),
                                    "BEGIN QUEST", kQuest),
                    .note =
                        "The generated bands land at their intended weight."},
                   {.title = "SOURCE PIXELS AS UNITS",
                    .control = "Slice::density = 1",
                    .figure =
                        panel(carvedFrameSlice(oak, 1), "BEGIN QUEST", kQuest),
                    .note = "The same bands occupy twice the layout space."}},
         .measure = 700,
         .gap = 28});
    Element paths = sketch::kit::comparison(
        {.cases =
             {{.title = "COMPOSE DECORATION",
               .control = "Slice",
               .figure =
                   panel(carvedFrameSlice(azurePlain, 1), "DECOMPOSED", kQuest),
               .note = "The decoration divides the texture into rectangles."},
              {.title = "DIRECT CANVAS",
               .control = "skia::draw::drawLattice",
               .figure = directLattice(azurePlain),
               .note = "The same rectangles through the explicit draw call."}},
         .measure = 700,
         .gap = 28});
    Element stretchPreview = sketch::kit::well(
        {.width = 500, .height = 160, .content = sketch::kit::Well::Content{}},
        panel(carvedFrameSlice(crimson, kFrameDensity), "RESIZE THE MIDDLE",
              kQuest)
            .width(breathW)
            .height(breathH));
    return sketch::kit::page(
        {.title = "A frame that can change size",
         .subtitle = "Nine regions, one texture. Separate source density from "
                     "the draw path and the changing destination.",
         .footer = "Both draw paths use rectangles supported by raster and "
                   "device backends."},
        box().column().gap(28).children(
            {box().row().gap(32).children(
                 {std::move(source),
                  box().column().gap(30).width(700).children(
                      {std::move(density), std::move(paths)})}),
             sketch::kit::sectionHeader(
                 {.label = "LIVE RESIZE",
                  .note = "Corner bands keep their dimensions"}),
             box().row().gap(32).children(
                 {std::move(stretchPreview),
                  box().column().gap(14).width(488).children(
                      {text(kit::formatted("%.0f × %.0f layout units", breathW,
                                           breathH))
                           .styleClass("readout"),
                       text("The destination is laid out again every frame. "
                            "Watch the straight bands grow while the carved "
                            "corners retain their shape.")
                           .width(360)})})}));
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
    oak = generate(oakPalette());
    crimson = generate(crimsonPalette());
    azurePlain = generate(azurePalette(), 1.0f);
    stretch = 0.0f;
    ctx.composer.render(describe());
  }

  /** THE WHOLE TREE, EVERY FRAME, and deliberately: what moves here is a
   *  panel's SIZE, so the frame is re-sliced and everything below it
   *  re-laid out. That is the describe path — a bound Output animates a
   *  value the layout already settled, and this changes what the layout
   *  settles. The reconciler diffs the rest, which is the point of
   *  watching the stretch rather than assuming it. */
  void update(double elapsed, sketch::SketchContext& ctx) {
    stretch = 0.5f + 0.5f * (float)std::sin(elapsed * 1.4);
    ctx.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(NineSlice, "nine slice", "Kit · API",
                "one frame texture over every size — the lattice, "
                "the density it declares, and the native op that draws "
                "nothing on a device")
