/** @file
 * rich_slot_reserve — an element woven into a line, and room kept beside
 * every line before anything is broken.
 *
 * `RichText::slot` reserves px of blank space in the flow and names the
 * child laid out into it. The reserved box is ONE UNBREAKABLE WORD: a
 * line never breaks inside it however narrow the measure gets.
 * `baselineDrop` is how far the box's BOTTOM sits below the baseline — 0
 * stands it on the baseline like an inline image, and about the face's
 * descent centres a pill on the x-height. A box TALLER than the type
 * OPENS THE LINES OF ITS BLOCK: how far the box reaches either side of
 * the baseline is a fact about the strut, and the strut is the block's,
 * because a band is asked of the geometry before anyone knows which
 * words land on it. The third cell is that case — every line of the
 * passage stands on the opened pitch, not the one the slot happens to
 * sit in.
 *
 * The child is an ordinary subtree that animates, caches and hit-tests
 * like any other, and it re-lands wherever the placeholder lands when the
 * text reflows. It is a POSITIONED subtree: the placeholder rect is its
 * box, so no flex layout runs inside it. A TEXT SLOT IS NOT A MOUNT SLOT
 * — these names live in this rich-text value alone and are matched
 * against this node's own children's keys, so two captions may both
 * reserve a slot called "icon" without colliding, and neither is
 * reachable by `Composer::renderSlot`.
 *
 * `Element::reserve` is the other half: room beside every LINE, over and
 * above the leading. It is a layout INPUT — the room is in the strut
 * before anything is broken — so nothing chases anything afterwards.
 * `before` is above a line and to the right of a column, `after` below a
 * line and to the left, and `before` also moves the baseline down inside
 * the band, so the type stays where the reader expects it and the room
 * appears where the reading goes.
 *
 * EDIT THESE FIRST
 *   kChip — the inline slot's size, px.
 *   kDrop — the baseline drop that centres a pill on the x-height.
 *   kBand — the room reserved beside every line, px.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 163;
constexpr float kPicture = 200;

constexpr SkSize kChip = {34, 16};   // the inline slot
constexpr SkSize kTall = {40, 26};   // …and one taller than the type
constexpr float kDrop = 4;           // the drop that centres a pill
constexpr float kBand = 14;          // room beside every line, px

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBody{0.84f, 0.85f, 0.88f, 1};
constexpr SkColor4f kChipFill{0.86f, 0.52f, 0.34f, 1};
constexpr SkColor4f kBandTint{0.16f, 0.20f, 0.24f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"});
  return weave::textStyle({.face = face, .size = 12, .color = kBody});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** The paragraph the reserve cells all set, so the only difference
 *  between them is where the room went. */
const char* kPassage =
    "Room beside a line is a layout input: it stands in the strut before "
    "the passage is broken, so nothing chases anything afterwards.";

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .width(Dim(kCell))
                       .height(Dim(kPicture))
                       .clip()
                       .fill(Fill::color(kCellGround))
                       .padding(12)
                       .child(std::move(body)));
}

/** One passage with an inline slot in the middle of it. */
Element slotted(SkSize size, float drop, SkColor4f fill) {
  return text(rich(body())
                  .add(u8"A reserved box is one unbreakable word, so a "
                       u8"line never breaks inside ")
                  .slot("chip", size, drop)
                  .add(u8" and it keeps its whole advance however narrow "
                       u8"the measure gets."))
      .width(Dim(kCell - 24))
      .child(box().key("chip").fill(Fill::color(fill)));
}

/** The same passage under one reserved band, on a tinted plate so the
 *  line pitch is visible as a pitch. */
Element banded(weave::ReservedBand band) {
  return text(toU8(kPassage), body())
      .width(Dim(kCell - 24))
      .fill(Fill::color(kBandTint))
      .reserve(band);
}

}  // namespace

struct RichSlotReserve final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("SLOTS AND RESERVED ROOM \xc2\xb7 "
                           "RichText::slot, Element::reserve"),
             .subtitle = toU8("dials \xc2\xb7 the slot's size (34\xc3\x97" "16, "
                              "then 40\xc3\x97" "26) \xc2\xb7 its baseline drop "
                              "(0, then 4) \xc2\xb7 the band reserved beside "
                              "every line (14 px)"),
             .footer = toU8("a text slot is not a mount slot: these names "
                            "live in one rich-text value and are matched "
                            "against this node's own children, so two "
                            "captions may both reserve an \"icon\" and "
                            "neither is reachable by renderSlot"),
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
                     {cell("rich(body).slot(\"chip\", {34, 16})",
                           "the box stands ON the baseline, like an inline "
                           "image \xc2\xb7 the child is keyed \"chip\" and "
                           "lands wherever the placeholder does",
                           slotted(kChip, 0, kChipFill)),
                      cell("\xe2\x80\xa6" ", baselineDrop = 4",
                           "the box's BOTTOM dropped below the baseline by "
                           "about the face's descent \xc2\xb7 a pill centred "
                           "on the x-height",
                           slotted(kChip, kDrop, kChipFill)),
                      cell("slot(\"chip\", {40, 26})",
                           "taller than the type \xc2\xb7 the strut takes "
                           "how far it reaches either side of the baseline, "
                           "so every line of the BLOCK opens by that much",
                           slotted(kTall, kDrop, kChipFill)),
                      cell("no reserve",
                           "the reference pitch \xc2\xb7 the plate is filled "
                           "so the block's own height is legible",
                           banded({})),
                      cell("reserve({.before = 14})",
                           "room ABOVE every line, and the baseline moved "
                           "down inside the band \xc2\xb7 where a reading "
                           "goes",
                           banded({.before = kBand})),
                      cell("reserve({.after = 14})",
                           "room BELOW every line \xc2\xb7 the pitch opens "
                           "by the same amount and the type does not move "
                           "inside it",
                           banded({.after = kBand}))},
                 .gap = 10}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(RichSlotReserve, "Kit \xc2\xb7 API",
             "an element woven into a wrapping line at two baseline drops "
             "and one that opens the line, and one passage under a band "
             "reserved above its lines and below them")
