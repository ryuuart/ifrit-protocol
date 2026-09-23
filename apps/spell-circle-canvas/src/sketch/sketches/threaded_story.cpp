/** @file
 * threaded_story — one story through a chain of frames: a rectangle, a
 * frame cut round a silhouette, and a two-column set, with the cut moving
 * as the first frame's measure moves and no marker until the last.
 */

// ONE STORY, THREE FRAMES, ONE CHAIN. The story is declared once — content
// and block styles — and each frame fills from where the one before it
// stopped. Nothing on this page decides where the text breaks between
// frames: the first frame's measure does, and every later frame inherits
// whatever is left.
//
// The chain, in order:
//
//   · the first frame — a plain rectangle, flowing round a disc: an
//     exclusion shortens its lines exactly as it shortens any other
//     text's, and what it could not hold is what the columns get.
//   · the columns — two more frames side by side, threaded in order
//     (kit::textColumns writes them). Every frame but the last stops where
//     its geometry stops and hands the remainder on, drawing no marker,
//     which is what makes the cut invisible. THE LAST FRAME THREADS
//     NOWHERE, so what it cannot hold has nowhere to go: kit::textColumns'
//     last argument is the ellipsis that ends the chain, and it lands on
//     that column's last line only. A marker at every cut would read as
//     three separate texts rather than one story threaded through three
//     frames.
//
// THE CUT IS SHOWN BY SHOWING IT TWICE. The page carries the SAME chain
// at two measures, one narrow and one wide, and the words the columns
// begin with are different in the two — which is the whole claim, and a
// static plate can make it where a moving one only implies it.
//
// EDIT THESE FIRST
//   kNarrow / kWide — the two measures the first frame is set to. Their
//                     difference is how far the cut moves.
//   kFrameH         — how deep each frame is; the shallower they are, the
//                     more of the story reaches the columns.
//   kColumnGutter   — the gutter between the two column frames.

// TAGS: Typography/Paragraph, Motion/Transitions

#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1180, 760};

namespace story {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 56;
constexpr float kFrameH = 190;
constexpr float kNarrow = 210;
constexpr float kWide = 320;
constexpr float kColumnGutter = 24;

const material::Color kPaper{0.949f, 0.945f, 0.933f, 1};
const material::Color kInk{0.098f, 0.106f, 0.118f, 1};
const material::Color kFaint{0.098f, 0.106f, 0.118f, 0.65f};
const material::Color kMark{0.643f, 0.310f, 0.157f, 1};
const material::Color kDisc{0.643f, 0.310f, 0.157f, 0.16f};

sk_sp<SkTypeface> serif() {
  return weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
}
sk_sp<SkTypeface> grotesque() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
}

/** A caption line: a partial over what the cell inherits. */
weave::Type label(float size, material::Color colour, float track) {
  return {.face = grotesque(), .size = size, .color = colour, .track = track};
}

/** The one voice both chains are captioned in: the measure named over the
 *  chain, what it does to the cut under the name, both above the frames
 *  they describe. */
kit::Caption voice() {
  return {.where = kit::Caption::Where::Above,
          .gap = 12,
          .noteGap = 8,
          .noteMeasure = 300.0f};
}

/** THE TWO CLASSES THAT VOICE IS SET IN — the name in the mark colour, the
 *  remark a size under it in the faint one. */
sigil::compose::StyleSheet voiceClasses() {
  return sigil::compose::StyleSheet{
      sigil::compose::rule("label, .label").font(label(11.0f, kMark, 1.7f)),
      sigil::compose::rule("caption, .caption")
          .font(label(11.0f, kFaint, 0.1f))};
}

/** The story, declared once. Its blocks are numbered from its own start,
 *  so the third block is set the same way whichever frame it lands in. It
 *  states no base: a frame sets it in the font and ink in force where the
 *  frame stands. */
weave::Story article() {
  weave::Story built(
      weave::rich()
          .add(u8"WHERE A STORY IS CUT\n", weave::Type{.size = 16})
          .add(u8"A frame is not a paragraph and not a column: it is a "
               u8"piece of geometry a story is filled into, and a story is "
               u8"filled into as many of them as it is given. The first "
               u8"frame begins at the first word. Every frame after it "
               u8"begins wherever the one before it ran out of room, which "
               u8"is a word index and nothing more — the same number the "
               u8"pass before it reported as its remainder.\n")
          .add(u8"That is the whole mechanism, and everything a reader "
               u8"recognises about a threaded text follows from it. Narrow "
               u8"the first frame and the cut moves earlier in the story, so "
               u8"every frame after it holds different words; widen it and "
               u8"the cut moves further along. Nothing re-shapes: the story is "
               u8"one "
               u8"analysed, shaped text and each frame reads the same warm "
               u8"word list.\n")
          .add(u8"A frame's own geometry is its business. This one flows "
               u8"round a disc, and an exclusion shortens its lines exactly "
               u8"as it shortens any other text's, because a frame is an "
               u8"ordinary text leaf with an ordinary flow.\n")
          .add(u8"Western columns are not a geometry either. Three columns "
               u8"of one story are three frames side by side, threaded in "
               u8"order — which is why the vertical writing mode keeps the "
               u8"word column for the thing it already meant, a line turned "
               u8"a quarter turn. The last frame of a chain is the only one "
               u8"that may cut: every frame before it overflows by design, "
               u8"and a marker there would say the text ended when it was "
               u8"only continued."));
  weave::ParagraphStyle heading;
  heading.spaceAfter = 12;
  weave::ParagraphStyle para;
  para.indent.firstLine = 16;
  para.spaceAfter = 7;
  built.paragraphs({heading, para, para, para, para});
  return built;
}

}  // namespace story

struct ThreadedStory {
  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(
        ctx,
        {.size = kSceneSize, .captureAt = 0.4, .background = story::kPaper});
    ctx.composer.render(describe());
  }

  /** ONE CHAIN at one measure: a frame round a disc, then two columns.
   *  `prefix` keys the chain, so the page may carry two of them. */
  Element chain(const char* prefix, float measure,
                const weave::Story& article) {
    namespace s = story;
    const std::string head = std::string(prefix) + "-head";
    const std::string column = std::string(prefix) + "-column";
    const std::string stone = std::string(prefix) + "-stone";
    // The paper a frame stands on: one well, at the frame's own measure.
    const auto plate = [measure](float height, bool clip) {
      return kit::well({.width = Dimension(measure + 28),
                        .height = Dimension(height),
                        .ground = Fill::color({1, 1, 1, 0.6f}),
                        .padding = 14,
                        .clip = clip,
                        .corners = 3});
    };
    return box().column().gap(12).children(
        {plate(s::kFrameH, true)
             .children({kit::dot({measure * 0.45f + 37.0f, 95.0f}, 37.0f,
                                 Fill::color(s::kDisc))
                            .key(stone),
                        frame(article)
                            .key(head)
                            .textThreadTo(column + "0")
                            .width(measure)
                            .height(s::kFrameH - 28)
                            .contentFlowAround(stone, 9.0f)}),
         plate(s::kFrameH + 96, false)
             .children(
                 {kit::textColumns(article, 2, s::kColumnGutter, measure,
                                   s::kFrameH + 68, column, u8"\u2026")})});
  }

  Element describe() {
    namespace s = story;
    const weave::Story article = s::article();

    const auto captioned = [&](const char* name, const char* note,
                               Element built) {
      return kit::cell(s::voice(), name, note, std::move(built))
          .applyStyleSheet(s::voiceClasses());
    };

    return box()
        .fill(Fill::color(s::kPaper))
        .font({.face = s::grotesque(), .size = 9.5f})
        .ink(s::kFaint)
        .children(
            {box()
                 .inset(s::kMargin - 14, 0, 0, s::kMargin)
                 .column()
                 .gap(5)
                 .children(
                     {document::h1("One story, three frames")
                          .font({.size = 26, .color = s::kInk, .track = 0.2f}),
                      document::lead(
                          "Change the first frame's width. The same story "
                          "continues from a different word in each column.")
                          .font({.size = 12, .track = 0.1f})
                          .width(620.0f)}),
             box()
                 .inset(s::kMargin + 72, 0, 0, s::kMargin)
                 .row()
                 .gap(44)
                 .font({.face = s::serif(), .size = 13})
                 .ink(s::kInk)
                 .children({captioned("NARROW FIRST FRAME",
                                      "Less fits before the columns.\n"
                                      "They continue earlier in the story.",
                                      chain("narrow", s::kNarrow, article)),
                            captioned("WIDE FIRST FRAME",
                                      "More fits before the columns.\n"
                                      "They continue later in the story.",
                                      chain("wide", s::kWide, article))}),
             box()
                 .left(850)
                 .top(s::kMargin + 72)
                 .width(245)
                 .gap(14)
                 .children({
                     document::h2("READ THE CONTINUATION")
                         .font({.size = 11, .color = s::kMark, .track = 1.4f}),
                     document::paragraph(
                         "Start at the top frame, then read down the left "
                         "column and down the right. Each frame receives "
                         "the words its predecessor could not hold.")
                         .font({.size = 12, .color = s::kInk}),
                     document::paragraph(
                         "Compare the first word below each upper frame. "
                         "The wider frame holds more of the opening, so "
                         "its continuation begins later.")
                         .font({.size = 12, .color = s::kInk}),
                     document::caption(
                         "Only the last column has an ellipsis. An "
                         "intermediate frame continues; it does not end "
                         "the story.")
                         .font({.size = 11, .color = s::kFaint}),
                 }),
             document::footer(
                 "a Western column is a FRAME; the vertical writing "
                 "mode keeps the word for the thing it already meant")
                 .font({.size = 9.5f, .track = 0.2f})
                 .inset(s::kH - 32, 0, 0, s::kMargin)});
  }
};

}  // namespace

SIGIL_SKETCH_AS(ThreadedStory, "threaded_story", "Catalog · Type",
                "one story through a chain of frames, the cut moving")
