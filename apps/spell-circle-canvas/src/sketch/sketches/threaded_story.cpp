/** @file
 * threaded_story — one story through a chain of frames: a rectangle, a
 * frame cut round a silhouette, and a two-column set, with the cut moving
 * as the first frame's measure moves and no marker until the last.
 */

// ONE STORY, FOUR FRAMES, ONE CHAIN. The story is declared once — content
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
//     (kit::columns writes them). Every frame but the last stops where
//     its geometry stops and hands the remainder on, drawing no marker,
//     which is what makes the cut invisible. THE LAST FRAME THREADS
//     NOWHERE, so what it cannot hold has nowhere to go: kit::columns'
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

#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1180, 700};

namespace story {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 56;
constexpr float kFrameH = 190;
constexpr float kNarrow = 210;
constexpr float kWide = 320;
constexpr float kColumnGutter = 24;

const SkColor4f kPaper{0.949f, 0.945f, 0.933f, 1};
const SkColor4f kInk{0.098f, 0.106f, 0.118f, 1};
const SkColor4f kFaint{0.098f, 0.106f, 0.118f, 0.30f};
const SkColor4f kMark{0.643f, 0.310f, 0.157f, 1};
const SkColor4f kDisc{0.643f, 0.310f, 0.157f, 0.16f};

sk_sp<SkTypeface> serif() {
  return weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
}
sk_sp<SkTypeface> grotesque() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
}

weave::TextStyle body(float size = 13.0f) {
  return weave::textStyle({.face = serif(), .size = size, .color = kInk});
}
weave::TextStyle lead(float size = 15.0f) {
  return weave::textStyle({.face = serif(), .size = size, .color = kInk});
}
weave::TextStyle label(float size = 8.5f, SkColor4f colour = kFaint,
                       float track = 1.4f) {
  return weave::textStyle(
      {.face = grotesque(), .size = size, .color = colour, .track = track});
}

/** The one voice both chains are captioned in: the measure named over the
 *  chain, what it does to the cut under the name, both above the frames
 *  they describe. */
kit::Caption voice() {
  return {.where = kit::Caption::Where::Above,
          .label = label(9.5f, kMark, 2.4f),
          .note = label(9.0f, kFaint, 0.2f),
          .gap = 12,
          .noteGap = 8,
          .noteMeasure = 300.0f};
}

/** The story, declared once. Its blocks are numbered from its own start,
 *  so the third block is set the same way whichever frame it lands in. */
weave::Story article() {
  weave::Story built(
      weave::rich(body())
          .add(u8"WHERE A STORY IS CUT\n", lead(16))
          .add(u8"A frame is not a paragraph and not a column: it is a "
               u8"piece of geometry a story is filled into, and a story is "
               u8"filled into as many of them as it is given. The first "
               u8"frame begins at the first word. Every frame after it "
               u8"begins wherever the one before it ran out of room, which "
               u8"is a word index and nothing more — the same number the "
               u8"pass before it reported as its remainder.\n")
          .add(u8"That is the whole mechanism, and everything a reader "
               u8"recognises about a threaded text follows from it. Narrow "
               u8"the first frame and the cut moves later in the story, so "
               u8"every frame after it holds different words; widen it and "
               u8"the cut moves back. Nothing re-shapes: the story is one "
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

struct ThreadedStory final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
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
    return box()
        .column()
        .gap(12)
        .child(box()
                   .fill(Fill::color({1, 1, 1, 0.6f}))
                   .corners({3})
                   .padding(14)
                   .width(Dim(measure + 28))
                   .height(Dim(s::kFrameH))
                   .clip()
                   .child(box()
                              .key(stone)
                              .absolute()
                              .left(Dim(measure * 0.45f))
                              .top(Dim(58.0f))
                              .width(Dim(74.0f))
                              .height(Dim(74.0f))
                              .corners({37})
                              .fill(Fill::color(s::kDisc)))
                   .child(frame(article)
                              .key(head)
                              .thread(column + "0")
                              .width(Dim(measure))
                              .height(Dim(s::kFrameH - 28))
                              .flowAround(stone, 9.0f)))
        .child(box()
                   .fill(Fill::color({1, 1, 1, 0.6f}))
                   .corners({3})
                   .padding(14)
                   .width(Dim(measure + 28))
                   .height(Dim(s::kFrameH + 96))
                   .child(kit::columns(article, 2, s::kColumnGutter, measure,
                                       s::kFrameH + 68, column,
                                       u8"\u2026")));
  }

  Element describe() {
    namespace s = story;
    const weave::Story article = s::article();

    const auto captioned = [&](const char* name, const char* note,
                               Element built) {
      return kit::cell(s::voice(), toU8(name), toU8(note), std::move(built));
    };

    return box()
        .fill(Fill::color(s::kPaper))
        .child(box()
                   .absolute()
                   .inset(s::kMargin, s::kMargin - 14, 0, 0)
                   .column()
                   .gap(5)
                   .child(text(toU8("ONE STORY, THREE FRAMES, TWICE"),
                               s::label(11, s::kInk, 3.4f)))
                   .child(text(toU8("the cut is a word index \xe2\x80\x94 the "
                                    "remainder the frame before reported \xe2"
                                    "\x80\x94 so a narrower first frame moves "
                                    "it, and the columns begin elsewhere"),
                               s::label(9.5f, s::kFaint, 0.3f))
                              .width(Dim(700.0f))))
        .child(box()
                   .absolute()
                   .inset(s::kMargin, s::kMargin + 56, 0, 0)
                   .row()
                   .gap(44)
                   .child(captioned("NARROW FIRST FRAME",
                                    "less fits before the columns, so they "
                                    "start earlier in the story",
                                    chain("narrow", s::kNarrow, article)))
                   .child(captioned("WIDE FIRST FRAME",
                                    "more fits before them, and the same two "
                                    "columns begin further in",
                                    chain("wide", s::kWide, article))))
        .child(text(toU8("a Western column is a FRAME; the vertical writing "
                         "mode keeps the word for the thing it already meant"),
                    s::label(9.5f, s::kFaint, 0.2f))
                   .absolute()
                   .inset(s::kMargin, s::kH - 32, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(ThreadedStory, "threaded_story", "Catalog \xc2\xb7 Type",
                "one story through a chain of frames, the cut moving")
