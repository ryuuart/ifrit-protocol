/** @file
 * horizontal_flow — a line meeting a silhouette, and an ornamented initial
 * opening a paragraph.
 *
 * The left passage crosses a star in the middle of its measure. Each
 * horizontal band receives the intervals on both sides of the silhouette,
 * then becomes a full line again below it. The right passage begins with a
 * caller-built ORNAMENT — an element with a key and a silhouette — and the
 * paragraph flows around that key, so the same line-flow rule follows its
 * outline. (A plain letter needs none of this: `initialLetter` sizes and
 * seats one from the block's own pitch.)
 *
 * EDIT THESE FIRST
 *   kShapeSize — how much of the left passage the central shape interrupts.
 *   kDropWidth / kDropHeight — how many opening lines the ornament occupies.
 *   kWrapMargin — the clearance between either silhouette and the type.
 */

// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Cells.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1080, 620};
constexpr float kShapeSize = 152;
constexpr float kDropWidth = 96;
constexpr float kDropHeight = 118;
constexpr float kWrapMargin = 9;

constexpr SkColor4f kPaper{0.94f, 0.91f, 0.84f, 1};
constexpr SkColor4f kPanel{0.975f, 0.955f, 0.91f, 1};
constexpr SkColor4f kInk{0.12f, 0.11f, 0.10f, 1};
constexpr SkColor4f kQuiet{0.42f, 0.37f, 0.31f, 1};
constexpr SkColor4f kCinnabar{0.67f, 0.16f, 0.11f, 1};
constexpr SkColor4f kGold{0.78f, 0.55f, 0.16f, 1};

constexpr float kMeasure = 430;

/** THE SHEET'S LOOK, as the theme every component below reads.
 *
 *  Its two faces are a grotesque and a BOOK face: a theme's second face
 *  is whichever one the sheet's subject is set in, and this sheet's
 *  subject is a paragraph. Both panels stand as wells on the paper, and
 *  each carries its caption over its passage rather than around it,
 *  because what a caption names here is the whole passage under it. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme paper = sketch::kit::houseTheme();
  paper.palette.ground = kPaper;
  paper.palette.cellGround = kPanel;
  paper.palette.ink = kInk;
  paper.palette.ash = kQuiet;
  paper.palette.figure = kCinnabar;
  paper.type.sans = weave::ports::face(
      {"Avenir Next", "Helvetica Neue", "DejaVu Sans", "sans-serif"});
  paper.type.mono = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  paper.type.title = {24, 3.2f};
  paper.type.subtitle = {13, 0.5f};
  paper.type.captionLabel = {11, 1.8f};
  paper.type.captionNote = {11};
  paper.spacing.captionGap = 18;
  paper.spacing.captionNoteGap = 5;
  paper.spacing.wellPadding = 22;
  paper.captionWhere = sigil::compose::kit::Caption::Where::Above;
  return paper;
}

Element shapePassage() {
  Element medallion =
      kit::at(kit::centred(), 139, 74, kShapeSize, kShapeSize)
          .key("central-star")
          .shape(shapes::star(10, 0.62f, 0.08f))
          .fill(Fill::color(kGold))
          .children({text(u8"FLOW").font(
              sketch::kit::theme().font({.size = 14, .track = 1.2f}))});

  Element passage =
      document::paragraph(
          u8"A horizontal line begins at the left edge of its measure. "
          u8"When it reaches the ornament, the available band becomes two "
          u8"intervals: words fill the room on the left and continue in "
          u8"the room on the right. Near each point the interval changes "
          u8"with the silhouette instead of following its box. Once the "
          u8"shape has passed, the paragraph recovers its full measure and "
          u8"continues without a special text mode.")
          .key("shape-passage")
          .width(kMeasure)
          .flowAround("central-star", kWrapMargin)
          .zIndex(1);

  return box().width(kMeasure).height(350).children(
      {std::move(medallion), std::move(passage)});
}

Element droppedPassage() {
  Element ornament =
      kit::at(kit::centred(), 0.0f, 0.0f, kDropWidth, kDropHeight)
          .key("illuminated-h")
          .shape(shapes::rounded(shapes::star(8, 0.58f, 0.12f), 5))
          .fill(Fill::color(kCinnabar))
          .children({text(u8"H").font({.size = 50, .color = kPaper})});

  return box().width(kMeasure).height(350).children(
      {std::move(ornament),
       document::paragraph(
           u8"orizontal setting needs no drop-cap mechanism when the "
           u8"initial is an ornament. The ornament is an element with "
           u8"a key and a silhouette, while this paragraph is an "
           u8"ordinary text leaf flowing around that key. The opening "
           u8"lines take the changing room beside the points; the "
           u8"later lines return to the whole measure. A photograph, "
           u8"seal, flourish, or illustrated letter uses exactly the "
           u8"same relationship.")
           .key("drop-passage")
           .width(kMeasure)
           .flowAround("illuminated-h", kWrapMargin)});
}

Element panel(float left, const char* title, const char* note, Element body) {
  return sketch::kit::well(
             {.width = Dimension(476), .height = Dimension(438)},
             sketch::kit::caption(kMeasure, title, note, std::move(body)))
      .at({left, 126});
}

}  // namespace

struct HorizontalFlow {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    const sketch::kit::Theme& sheet = sketch::kit::theme();
    sketch::kit::stage(ctx,
                       {.size = SkSize::Make(kCanvas.width(), kCanvas.height()),
                        .captureAt = 0.05,
                        .background = sheet.palette.ground});

    // The root: the book face at the passages' size, in the sheet's ink,
    // with the theme's registers stated for the two lines that name one.
    ctx.composer.render(
        box()
            .font({.face = sheet.type.mono, .size = 15})
            .ink(sheet.palette.ink)
            .styleSheet(sheet.styleSheet())
            .children(
                {document::h1(u8"HORIZONTAL TEXT FLOW").at({42, 34}),
                 document::lead(
                     u8"one exclusion rule · a shape in the measure · an "
                     u8"ornament at the opening")
                     .at({43, 76}),
                 panel(42, "FLOW AROUND A SHAPE",
                       "the line divides left and right, then becomes whole",
                       shapePassage()),
                 panel(562, "ORNAMENTED DROP CAP",
                       "a caller-built element supplies the painted outline",
                       droppedPassage())}));
  }
};

SIGIL_SKETCH(HorizontalFlow, "Catalog · Type",
             "horizontal text flowing around a central silhouette and an "
             "ornamented drop cap")
