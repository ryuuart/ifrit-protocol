/** @file
 * keeps_and_frames — the lines that refuse to be parted, and what
 * becomes of the room left over.
 *
 * Every KEEP is a statement about a FRAME BOUNDARY: a widow stands at the
 * head of the next frame, an orphan at the foot of this one, and a
 * kept-together block straddles the join. They are settled where the
 * boundary is — the fill runs, and lines the block may not leave behind
 * are taken back out of it and reported as overflow, which is how they
 * reach the next frame of the chain. No break is re-decided and nothing
 * is weighed against spacing, so both breakers obey them identically. A
 * keep never empties a frame: a retraction that would leave the fill with
 * nothing is dropped, or the chain would never advance.
 *
 * `widowLines` is the one that asks about a frame this fill cannot see,
 * so it counts the carried lines at the measure THIS frame's last line
 * was set in — exact for a chain of equal frames, and wrong for a chain
 * that changes width.
 *
 * The frame options are the other half, and they are about ROOM rather
 * than about breaking. `firstBaseline` seats the first line by its own
 * ascent, its cap height, its x-height, its whole pitch, or a stated
 * offset, and every later baseline follows at its block's pitch — so it
 * moves the whole passage rather than its first line. `distribute` says
 * what becomes of the room left over: nothing, half above and half below,
 * all above, or spread BETWEEN the lines as extra leading, which is what
 * a magazine column does to reach its foot. The last two cells stand a
 * frame of stated height under the first and the last of those: the room
 * a distribution divides is the frame's own depth, so a leaf that states
 * one has room to spend, and kJustify opens the gaps between the lines
 * until the last one lands on the foot.
 *
 * EDIT THESE FIRST
 *   kWidows, kOrphans — the two keep counts.
 *   kFrame — the size of each frame in the chains, px.
 *   kSeat — the extra offset added on top of whatever a seating measured.
 */

// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 604};
constexpr float kChainCell = 342;
constexpr float kOptionCell = 254;
constexpr SkSize kFrame = {158, 176};  // one frame of a chain
constexpr float kOptionPicture = 176;

constexpr int kWidows = 2;   // fewest lines at the head of a frame
constexpr int kOrphans = 2;  // fewest at the foot of one
constexpr float kSeat = 0;   // added on top of a measured seating

constexpr SkColor4f kBody{0.84f, 0.85f, 0.88f, 1};

weave::TextStyle serif(float size, SkColor4f color) {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** Three blocks: a lead, a body long enough to straddle the join, and a
 *  closing block, so a keep has a boundary to argue with. */
weave::Story article(bool longBody, weave::KeepOptions bodyKeep,
                     weave::KeepOptions closeKeep) {
  weave::Story story(
      weave::rich(serif(11.5f, kBody))
          .add(u8"THE FIRST BLOCK\n",
               serif(11.5f, sketch::kit::theme().palette.figure))
          .add(longBody ? u8"A widow stands at the head of the next frame "
                          u8"and an orphan at the foot of this one, so "
                          u8"both are settled where the boundary is "
                          u8"rather than while the lines are being "
                          u8"chosen, and the rest is overflow.\n"
                        : u8"A widow stands at the head of the next "
                          u8"frame.\n")
          .add(u8"The last block closes the story."));
  weave::ParagraphStyle lead;
  lead.spaceAfter = 5;
  weave::ParagraphStyle body;
  body.spaceAfter = 5;
  body.keep = bodyKeep;
  weave::ParagraphStyle close;
  close.keep = closeKeep;
  story.paragraphs({lead, body, close});
  return story;
}

/** One two-frame chain, drawn as two plates side by side. */
Element chain(const std::string& tag, const weave::Story& story) {
  const auto plate = [&](const std::string& key, bool threaded) {
    Element leaf = frame(story)
                       .key(key)
                       .width(kFrame.width() - 20)
                       .height(kFrame.height() - 20);
    if (threaded) leaf.thread(tag + "-2");
    return box()
        .width(kFrame.width())
        .height(kFrame.height())
        .clip()
        .fill(Fill::color(sketch::kit::theme().palette.cellGround))
        .padding(10)
        .children({std::move(leaf)});
  };
  return box()
      .row()
      .gap(kChainCell - 2 * kFrame.width())
      .children({plate(tag + "-1", true), plate(tag + "-2", false)});
}

Element optionPlate(Element body) {
  return box()
      .width(kOptionCell)
      .height(kOptionPicture)
      .clip()
      .fill(Fill::color(sketch::kit::theme().palette.cellGround))
      .padding(12)
      .children({std::move(body)});
}

}  // namespace

struct KeepsAndFrames final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "KEEPS AND FRAME OPTIONS · KeepOptions, "
                  "Element::firstBaseline, Element::distribute",
         .subtitle = "dials · the widow and orphan counts "
                     "(2 and 2) · startInNextFrame · "
                     "the seating rule · what becomes of the "
                     "room left over",
         .footer = "a keep never empties a frame — a "
                   "retraction that would leave the fill with "
                   "nothing is dropped, because the text would "
                   "arrive at the next frame in exactly the state "
                   "that emptied this one and the chain would "
                   "never advance"},
        kit::cells(
            {.cells = {chains(), options()}, .column = true, .gap = 18})));
  }

  /** The three chains: the same story, the same two frames, one keep
   *  changed. */
  Element chains() {
    return kit::cells(
        {.cells = {sketch::kit::caption(
                       kChainCell, "KeepOptions{} · free",
                       "the cut falls where the fill reached the foot "
                       "of frame one · the reference",
                       chain("free", article(true, {}, {}))),
                   sketch::kit::caption(
                       kChainCell, "keep{.widowLines = 2, .orphanLines = 2}",
                       "no single line may stand alone at either side "
                       "of the join · the lines that would have "
                       "are taken back out and reported as overflow",
                       chain("keep", article(true,
                                             {.widowLines = kWidows,
                                              .orphanLines = kOrphans},
                                             {}))),
                   sketch::kit::caption(
                       kChainCell, "keep{.startInNextFrame = true}",
                       "on the LAST block, over a SHORTER body "
                       "· it starts frame two though frame "
                       "one still has room for it",
                       chain("start",
                             article(false, {}, {.startInNextFrame = true})))},
         .gap = 16});
  }

  /** ONE FRAME OPTION: the same passage in the same box, seated on the
   *  rule the cell names and spending its leftover room as it says. */
  struct Option {
    const char* key;
    const char* call;
    const char* note;
    const char* words;
    weave::FrameOptions::FirstBaseline seat =
        weave::FrameOptions::FirstBaseline::kAscent;
    weave::FrameOptions::Distribute spend =
        weave::FrameOptions::Distribute::kStart;
  };

  /** The four frame options: two seatings and two distributions, each on
   *  the same short passage in the same box. */
  Element options() {
    static constexpr Option kOptions[] = {
        {"seat-ascent", "firstBaseline(kAscent)",
         "the first line's own ascent · what a leaf that says nothing "
         "gets, and the reference for the cell beside it",
         "Seated on the first line's own ascent, which is what a leaf "
         "that says nothing gets."},
        {"seat-cap", "firstBaseline(kCapHeight)",
         "the cap top lands on the box's own top · every later baseline "
         "follows at its block's pitch, so the passage moves as one",
         "Seated on the first line's CAP HEIGHT, so two leaves of "
         "different type start their text at one height.",
         weave::FrameOptions::FirstBaseline::kCapHeight},
        {"dist-start", "distribute(kStart)",
         "the leftover room stays past the last line · the frame carries "
         "a stated height, so there IS room left over here",
         "The lines stack from the top and the remainder is air "
         "underneath, which is the default and costs nothing to say."},
        {"dist-justify", "distribute(kJustify)",
         "the same room spread between the lines as extra leading · the "
         "gaps open evenly and the last line lands on the frame's foot",
         "The remainder is spread BETWEEN the lines as extra leading, "
         "which is how a column of a magazine reaches its foot.",
         weave::FrameOptions::FirstBaseline::kAscent,
         weave::FrameOptions::Distribute::kJustify},
    };
    return kit::cells(
        {.cells =
             each(kOptions,
                  [](const Option& one) {
                    return sketch::kit::caption(
                        kOptionCell, one.call, one.note,
                        optionPlate(
                            frame(weave::Story(one.words, serif(11.5f, kBody)))
                                .key(one.key)
                                .width(kOptionCell - 24)
                                .height(kOptionPicture - 24)
                                .firstBaseline(one.seat, kSeat)
                                .distribute(one.spend)));
                  }),
         .gap = 14});
  }
};

SIGIL_SKETCH(KeepsAndFrames, "Kit · API",
             "one story cut across two frames three times, free and under "
             "two keeps, and one passage seated two ways with its leftover "
             "room left alone and then spread between its lines")
