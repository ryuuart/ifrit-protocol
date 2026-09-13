/** @file
 * rich_slot_reserve — an element woven into a line, and room kept beside
 * every line before anything is broken.
 *
 * `weave::RichText::slot` reserves px of blank space in the flow and names the
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

// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 163;
constexpr float kPicture = 200;

constexpr SkSize kChip = {34, 16};  // the inline slot
constexpr SkSize kTall = {40, 26};  // …and one taller than the type
constexpr float kDrop = 4;          // the drop that centres a pill
constexpr float kBand = 14;         // room beside every line, px

constexpr SkColor4f kBody{0.84f, 0.85f, 0.88f, 1};
constexpr SkColor4f kChipFill{0.86f, 0.52f, 0.34f, 1};
constexpr SkColor4f kBandTint{0.16f, 0.20f, 0.24f, 1};

/** THE VOICE ALL SIX CELLS ARE SET IN, stated once over the run of them:
 *  the sheet is about where a line breaks, so nothing but where the room
 *  went differs between the cells. The tracking is stated because the
 *  page's running register tracks its remarks and a passage is not
 *  tracked, and the face because a passage here is not set in the sheet's
 *  own text face. */
weave::Type bodyVoice() {
  return {.face = weave::ports::face(
              {"Helvetica Neue", "Helvetica", "Arial", "sans-serif"}),
          .size = 12,
          .color = kBody,
          .track = 0};
}

/** The paragraph the reserve cells all set, so the only difference
 *  between them is where the room went. */
const char* kPassage =
    "Room beside a line is a layout input: it stands in the strut before "
    "the passage is broken, so nothing chases anything afterwards.";

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, call, note,
      sketch::kit::well({.width = kCell, .height = kPicture, .padding = 12})
          .children({std::move(body)}));
}

/** One passage with an inline slot in the middle of it. */
Element slotted(SkSize size, float drop, SkColor4f fill) {
  return text(weave::rich()
                  .add(u8"A reserved box is one unbreakable word, so a "
                       u8"line never breaks inside ")
                  .slot("chip", size, drop)
                  .add(u8" and it keeps its whole advance however narrow "
                       u8"the measure gets."))
      .width(kCell - 24)
      .children({box().key("chip").fill(Fill::color(fill))});
}

/** The same passage under one reserved band, on a tinted plate so the
 *  line pitch is visible as a pitch. */
Element banded(weave::ReservedBand band) {
  return text(kPassage)
      .width(kCell - 24)
      .fill(Fill::color(kBandTint))
      .reserve(band);
}

}  // namespace

struct RichSlotReserve final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "SLOTS AND RESERVED ROOM · "
                  "weave::RichText::slot, Element::reserve",
         .subtitle = "dials · the slot's size (34×16, "
                     "then 40×26) · its baseline drop "
                     "(0, then 4) · the band reserved beside "
                     "every line (14 px)",
         .footer = "a text slot is not a mount slot: these names "
                   "live in one rich-text value and are matched "
                   "against this node's own children, so two "
                   "captions may both reserve an \"icon\" and "
                   "neither is reachable by renderSlot"},
        kit::cells(
            {.cells = {cell("weave::rich(…).slot(\"chip\", {34, 16})",
                            "the box stands ON the baseline, like an inline "
                            "image · the child is keyed \"chip\" and "
                            "lands wherever the placeholder does",
                            slotted(kChip, 0, kChipFill)),
                       cell("…"
                            ", baselineDrop = 4",
                            "the box's BOTTOM dropped below the baseline by "
                            "about the face's descent · a pill centred "
                            "on the x-height",
                            slotted(kChip, kDrop, kChipFill)),
                       cell("slot(\"chip\", {40, 26})",
                            "taller than the type · the strut takes "
                            "how far it reaches either side of the baseline, "
                            "so every line of the BLOCK opens by that much",
                            slotted(kTall, kDrop, kChipFill)),
                       cell("no reserve",
                            "the reference pitch · the plate is filled "
                            "so the block's own height is legible",
                            banded({})),
                       cell("reserve({.before = 14})",
                            "room ABOVE every line, and the baseline moved "
                            "down inside the band · where a reading "
                            "goes",
                            banded({.before = kBand})),
                       cell("reserve({.after = 14})",
                            "room BELOW every line · the pitch opens "
                            "by the same amount and the type does not move "
                            "inside it",
                            banded({.after = kBand}))},
             .gap = 10})
            .font(bodyVoice())));
  }
};

SIGIL_SKETCH(RichSlotReserve, "Kit · API",
             "an element woven into a wrapping line at two baseline drops "
             "and one that opens the line, and one passage under a band "
             "reserved above its lines and below them")
