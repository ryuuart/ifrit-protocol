/** @file
 * nine slice — one generated frame texture, and the three things a
 * lattice has to get right.
 *
 * A carved frame is drawn once on an offscreen canvas at TWICE the size
 * it is used at, and every panel below wears that one image through a
 * `Slice`: the corner and edge bands hold their shape while the middle
 * stretches to whatever box the layout settled.
 *
 *   DENSITY  A texture generated oversized is sharp on a dense display
 *            and, drawn without saying so, twice as heavy as it was
 *            designed: a 64 px corner band lands 64 layout units wide
 *            where 32 were meant. `Slice::density` is the source's
 *            pixels per layout unit, and the two panels in the first row
 *            are the one image at 2 and at 1, so the difference between
 *            declaring it and not is the picture.
 *   THE TRAP Skia's own `drawImageLattice` is not implemented on every
 *            backend and draws NOTHING where it is not — including when
 *            a picture recorded elsewhere replays there. The second row
 *            draws the same frame twice at the same size: once through
 *            `Slice`, which decomposes the lattice into rects on every
 *            backend, and once through the native call in a `custom()`
 *            leaf. On a raster plate the two agree; on the device the
 *            right-hand cell is empty, which is why the decomposed path
 *            exists.
 *   STRETCH  The panel at the foot is re-laid out every frame, so the
 *            middle bands are watched stretching rather than assumed.
 *
 * The cells and the page are the specimen kit's, so the sheet's voice is
 * declared once.
 */

#include <include/core/SkSamplingOptions.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Ornament.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <iterator>
#include <memory>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
using namespace sigil::compose::kit::ornament;

namespace {

constexpr SkSize kSceneSize = {900, 640};
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

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

/** THIS SHEET'S LOOK, and its one voice: the call over the panel, what
 *  it did under it. The page's ground is a shade off the canvas's, which
 *  is black, so the margin around the sheet reads as a border. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look;
  look.palette = {.ground = {0.055f, 0.055f, 0.075f, 1},
                  .ink = kInk,
                  .ash = kAsh,
                  .rule = kRule};
  look.type.title = {.size = 26, .track = 3};
  look.type.subtitle = {.size = 12, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 1.2f};
  look.type.captionLabel = {.size = 12.5f, .track = 0.4f};
  look.type.captionNote = {.size = 11.0f, .track = 0.2f};
  look.spacing.marginX = 44;
  look.spacing.marginTop = 34;
  look.spacing.marginBottom = 22;
  look.spacing.captionGap = 7;
  return look;
}

/** The panel every cell shows: the frame stretched over a box of one
 *  size, with room inside it for a line of type. 24 clears the carved
 *  corner bosses, which reach 0.215 of the 96-unit band in from the
 *  edge. */
Element panel(Slice frame, std::u8string caption, SkColor4f ink) {
  return box()
      .width(Dim(kPanelW))
      .height(Dim(kPanelH))
      .background(std::move(frame))
      .padding(24)
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(text(std::move(caption), label(17, ink)));
}

/** THE NATIVE CALL, in a leaf of its own: Skia's `drawImageLattice`
 *  against the same divs the `Slice` beside it declares. Nothing here
 *  goes through the library's decomposition, which is the point. */
Element nativeLattice(std::shared_ptr<sigil::image::ImageAsset> asset) {
  return box()
      .width(Dim(kPanelW))
      .height(Dim(kPanelH))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(custom([asset = std::move(asset)](SkCanvas& canvas,
                                               const PaintContext& ctx) {
        const sk_sp<SkImage> image =
            asset ? asset->frameAt(0).image : nullptr;
        if (!image) return;
        const int side = image->width();
        const int xs[] = {side / 3, side * 2 / 3};
        const int ys[] = {side / 3, side * 2 / 3};
        const SkIRect bounds = SkIRect::MakeWH(side, image->height());
        // EVERY FIELD NAMED, because Lattice is a plain aggregate with no
        // default member initializers: a declaration followed by the four
        // assignments this call needs leaves the per-rectangle fill
        // arrays holding whatever was on the stack, and the recorder
        // dereferences them whenever they are not null.
        const SkCanvas::Lattice lattice{.fXDivs = xs,
                                        .fYDivs = ys,
                                        .fRectTypes = nullptr,
                                        .fXCount = (int)std::size(xs),
                                        .fYCount = (int)std::size(ys),
                                        .fBounds = &bounds,
                                        .fColors = nullptr};
        canvas.drawImageLattice(
            image.get(), lattice,
            SkRect::MakeWH(ctx.size.width(), ctx.size.height()),
            SkFilterMode::kLinear);
      }).absolute()
                 .inset(0))
      .child(text(u8"NATIVE", label(17, kQuest)));
}

struct NineSlice final : sketch::Sketch {
  std::shared_ptr<sigil::image::ImageAsset> oak, azure, crimson;
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

    Element density = kit::cells(
        {.cells =
             {sketch::kit::caption(kPanelW, u8"Slice::density = 2",
                        u8"192 px at its design width \xe2\x80\x94 a 16-unit "
                        u8"band",
                        panel(carvedFrameSlice(oak, kFrameDensity),
                              u8"BEGIN QUEST", kQuest)),
              sketch::kit::caption(kPanelW, u8"Slice::density = 1",
                        u8"the same image at face value \xe2\x80\x94 twice "
                        u8"as heavy",
                        panel(carvedFrameSlice(oak, 1.0f), u8"BEGIN QUEST",
                              kQuest))},
         .gap = 34,
         .divider = Fill::color(kRule)});

    Element trap = kit::cells(
        {.cells =
             {sketch::kit::caption(kPanelW, u8"Slice",
                        u8"decomposed into rects \xe2\x80\x94 every backend",
                        panel(carvedFrameSlice(azurePlain, 1.0f),
                              u8"DECOMPOSED", kQuest)),
              sketch::kit::caption(kPanelW, u8"canvas.drawImageLattice",
                        u8"the native op \xe2\x80\x94 blank on a device",
                        nativeLattice(azurePlain))},
         .gap = 34,
         .divider = Fill::color(kRule)});

    Element source = kit::cells(
        {.cells = {sketch::kit::caption(kPanelW, u8"the source",
                             u8"drawn once, offscreen, at 2\xc3\x97",
                             image(oak).width(Dim(96)).height(Dim(96))),
                   sketch::kit::caption(kPanelW, u8"re-laid out every frame",
                             u8"the box changes, the corners do not",
                             panel(carvedFrameSlice(crimson, kFrameDensity),
                                   u8"stretch me", kQuest)
                                 .width(Dim(breathW))
                                 .height(Dim(breathH)))},
         .gap = 34,
         .divider = Fill::color(kRule),
         .align = Align::Center});

    return sketch::kit::page(
        {.title = u8"NINE SLICE",
         .subtitle = u8"one generated texture over every size \xe2\x80\x94 "
                     u8"the density it declares, and the native op it does "
                     u8"not use",
         .footer = u8"Sketchbook \xc2\xb7 nine_slice"},
        kit::cells({.cells = {std::move(density), std::move(trap),
                              std::move(source)},
                    .column = true,
                    .gap = 22}));
  }

  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = SkColor4f{0, 0, 0, 1}});
    oak = generate(oakPalette());
    azure = generate(azurePalette());
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
  void update(double elapsed, sketch::SketchContext& ctx) override {
    stretch = 0.5f + 0.5f * (float)std::sin(elapsed * 1.4);
    ctx.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH_AS(NineSlice, "nine slice", "Kit \xc2\xb7 API",
                "one frame texture over every size \xe2\x80\x94 the lattice, "
                "the density it declares, and the native op that draws "
                "nothing on a device")
