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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

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
constexpr SkColor4f kLead{0.90f, 0.83f, 0.68f, 1};

weave::TextStyle serif(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** Three blocks: a lead, a body long enough to straddle the join, and a
 *  closing block, so a keep has a boundary to argue with. */
Story article(bool longBody, weave::KeepOptions bodyKeep,
              weave::KeepOptions closeKeep) {
  Story story(rich(serif(11.5f, kBody))
                  .add(u8"THE FIRST BLOCK\n", serif(11.5f, kLead))
                  .add(longBody
                           ? u8"A widow stands at the head of the next frame "
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
Element chain(const std::string& tag, const Story& story) {
  const auto plate = [&](const std::string& key, bool threaded) {
    Element leaf = frame(story)
                       .key(key)
                       .width(Dim(kFrame.width() - 20))
                       .height(Dim(kFrame.height() - 20));
    if (threaded) leaf.thread(tag + "-2");
    return box()
        .width(Dim(kFrame.width()))
        .height(Dim(kFrame.height()))
        .clip()
        .fill(Fill::color(sketch::kit::theme().palette.cellGround))
        .padding(10)
        .child(std::move(leaf));
  };
  return box()
      .row()
      .gap(kChainCell - 2 * kFrame.width())
      .child(plate(tag + "-1", true))
      .child(plate(tag + "-2", false));
}

Element optionPlate(Element body) {
  return box()
      .width(Dim(kOptionCell))
      .height(Dim(kOptionPicture))
      .clip()
      .fill(Fill::color(sketch::kit::theme().palette.cellGround))
      .padding(12)
      .child(std::move(body));
}

}  // namespace

struct KeepsAndFrames final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("KEEPS AND FRAME OPTIONS \xc2\xb7 KeepOptions, "
                       "Element::firstBaseline, Element::distribute"),
         .subtitle = toU8("dials \xc2\xb7 the widow and orphan counts "
                          "(2 and 2) \xc2\xb7 startInNextFrame \xc2\xb7 "
                          "the seating rule \xc2\xb7 what becomes of the "
                          "room left over"),
         .footer = toU8("a keep never empties a frame \xe2\x80\x94 a "
                        "retraction that would leave the fill with "
                        "nothing is dropped, because the text would "
                        "arrive at the next frame in exactly the state "
                        "that emptied this one and the chain would "
                        "never advance")},
        kit::cells(
            {.cells = {chains(), options()}, .column = true, .gap = 18})));
  }

  /** The three chains: the same story, the same two frames, one keep
   *  changed. */
  Element chains() {
    return kit::cells(
        {.cells = {sketch::kit::caption(
                       kChainCell, toU8("KeepOptions{} \xc2\xb7 free"),
                       toU8("the cut falls where the fill reached the foot "
                            "of frame one \xc2\xb7 the reference"),
                       chain("free", article(true, {}, {}))),
                   sketch::kit::caption(
                       kChainCell,
                       toU8("keep{.widowLines = 2, .orphanLines = 2}"),
                       toU8("no single line may stand alone at either side "
                            "of the join \xc2\xb7 the lines that would have "
                            "are taken back out and reported as overflow"),
                       chain("keep", article(true,
                                             {.widowLines = kWidows,
                                              .orphanLines = kOrphans},
                                             {}))),
                   sketch::kit::caption(
                       kChainCell, toU8("keep{.startInNextFrame = true}"),
                       toU8("on the LAST block, over a SHORTER body "
                            "\xc2\xb7 it starts frame two though frame "
                            "one still has room for it"),
                       chain("start",
                             article(false, {}, {.startInNextFrame = true})))},
         .gap = 16});
  }

  /** The four frame options: two seatings and two distributions, each on
   *  the same short passage in the same box. */
  Element options() {
    const auto passage = [](const char* text) { return toU8(text); };
    Element seated =
        text(passage("Seated on the first line's own ascent, which is what "
                     "a leaf that says nothing gets."),
             serif(11.5f, kBody))
            .width(Dim(kOptionCell - 24))
            .firstBaseline(weave::FrameOptions::FirstBaseline::kAscent, kSeat);
    Element capped =
        text(passage("Seated on the first line's CAP HEIGHT, so two leaves "
                     "of different type start their text at one height."),
             serif(11.5f, kBody))
            .width(Dim(kOptionCell - 24))
            .firstBaseline(weave::FrameOptions::FirstBaseline::kCapHeight,
                           kSeat);
    Element stacked =
        frame(Story(passage("The lines stack from the top and the remainder "
                            "is air underneath, which is the default and "
                            "costs nothing to say."),
                    serif(11.5f, kBody)))
            .key("dist-start")
            .width(Dim(kOptionCell - 24))
            .height(Dim(kOptionPicture - 24))
            .distribute(weave::FrameOptions::Distribute::kStart);
    Element justified =
        frame(Story(passage("The remainder is spread BETWEEN the lines as "
                            "extra leading, which is how a column of a "
                            "magazine reaches its foot."),
                    serif(11.5f, kBody)))
            .key("dist-justify")
            .width(Dim(kOptionCell - 24))
            .height(Dim(kOptionPicture - 24))
            .distribute(weave::FrameOptions::Distribute::kJustify);

    return kit::cells(
        {.cells = {sketch::kit::caption(
                       kOptionCell, toU8("firstBaseline(kAscent)"),
                       toU8("the first line's own ascent \xc2\xb7 what a "
                            "leaf that says nothing gets, and the "
                            "reference for the cell beside it"),
                       optionPlate(std::move(seated))),
                   sketch::kit::caption(
                       kOptionCell, toU8("firstBaseline(kCapHeight)"),
                       toU8("the cap top lands on the box's own top "
                            "\xc2\xb7 every later baseline follows at its "
                            "block's pitch, so the passage moves as one"),
                       optionPlate(std::move(capped))),
                   sketch::kit::caption(
                       kOptionCell, toU8("distribute(kStart)"),
                       toU8("the leftover room stays past the last line "
                            "\xc2\xb7 the frame carries a stated height, "
                            "so there IS room left over here"),
                       optionPlate(std::move(stacked))),
                   sketch::kit::caption(
                       kOptionCell, toU8("distribute(kJustify)"),
                       toU8("the same room spread between the lines as "
                            "extra leading \xc2\xb7 the gaps open evenly "
                            "and the last line lands on the frame's "
                            "foot"),
                       optionPlate(std::move(justified)))},
         .gap = 14});
  }
};

SIGIL_SKETCH(KeepsAndFrames, "Kit \xc2\xb7 API",
             "one story cut across two frames three times, free and under "
             "two keeps, and one passage seated two ways with its leftover "
             "room left alone and then spread between its lines")
