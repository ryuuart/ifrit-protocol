/** @file
 * A story crossing a frame boundary, with and without widow protection.
 * Frame options are a separate comparison: they place already broken
 * lines within a fixed depth. Every paired setting uses identical copy.
 */
// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr SkColor4f kBody{0.85f, 0.86f, 0.89f, 1};
constexpr SkColor4f kContinuation{0.55f, 0.77f, 0.94f, 1};
weave::TextStyle serif(float size, SkColor4f color = kBody) {
  return weave::textStyle(
      {.face = weave::ports::face(
           {"Iowan Old Style", "Georgia", "Times New Roman", "serif"}),
       .size = size,
       .color = color,
       .track = 0});
}

weave::Story article(bool shortBody, weave::KeepOptions keep,
                     bool newFrame = false) {
  weave::Story story(
      weave::rich(serif(14))
          .add(u8"THE FIRST BLOCK\n",
               serif(14, sketch::kit::theme().palette.figure))
          .add(shortBody
                   ? u8"A widow stands at the head of the next frame.\n"
                   : u8"A widow stands at the head of the next frame and an "
                     u8"orphan at the foot of this one, so both are settled "
                     u8"where the boundary is rather than while the lines are "
                     u8"being chosen, and the rest is overflow.\n",
               serif(14, kContinuation))
          .add(u8"The last block closes the story."));
  weave::ParagraphStyle lead;
  lead.spaceAfter = 6;
  weave::ParagraphStyle body;
  body.spaceAfter = 6;
  body.keep = keep;
  weave::ParagraphStyle close;
  close.keep.startInNextFrame = newFrame;
  story.paragraphs({lead, body, close});
  return story;
}

Element chain(const std::string& key, const weave::Story& story,
              float height = 214) {
  const auto leaf = [&](int index) {
    Element body = frame(story)
                       .key(key + std::to_string(index))
                       .width(168)
                       .height(height - 24);
    if (index == 1) body.thread(key + "2");
    return box().column().gap(10).children(
        {document::caption(index == 1 ? "FRAME 1" : "FRAME 2"),
         sketch::kit::well({.width = 192, .height = height, .padding = 12})
             .children({std::move(body)})});
  };
  return box()
      .row()
      .gap(24)
      .width(501)
      .justifyContent(Justify::Center)
      .children({leaf(1), leaf(2)});
}

Element seating(const char* key, weave::FrameOptions::FirstBaseline first,
                weave::FrameOptions::Distribute distribute) {
  const auto& look = sketch::kit::theme();
  return sketch::kit::well({.width = 241.5f, .height = 160, .padding = 18})
      .children({box().width(205.5f).height(124).children(
          {box().absolute().left(0).top(0).width(205.5f).height(1).fill(
               Fill::color(look.palette.figure)),
           box().absolute().left(0).top(124).width(205.5f).height(1).fill(
               Fill::color(look.palette.rule)),
           frame(weave::Story("One passage, one measure. Only the position of "
                              "the lines changes.",
                              serif(14)))
               .key(key)
               .width(205.5f)
               .height(124)
               .textFirstBaseline(first)
               .distribute(distribute)})});
}
}  // namespace

struct KeepsAndFrames {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 1180}, .captureAt = 0.05});
    using Seat = weave::FrameOptions::FirstBaseline;
    using Spend = weave::FrameOptions::Distribute;
    ctx.composer.render(sketch::kit::page(
        {.title = "Across the frame boundary",
         .subtitle = "The blue paragraph is the one that continues · equal "
                     "frames and identical copy within each comparison",
         .footer = "A keep retracts lines into overflow. It never empties a "
                   "frame, so the story can always advance."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "FILL TO THE FOOT",
                             .control = "No keep constraints",
                             .figure = chain("free-", article(false, {})),
                             .note =
                                 "The frame takes every line that fits, even "
                                 "when the next frame receives only one."},
                            {.title = "CARRY A PAIR",
                             .control = "widowLines = 2 · orphanLines = 2",
                             .figure = chain(
                                 "kept-", article(false, {.widowLines = 2,
                                                          .orphanLines = 2})),
                             .note = "The boundary retreats so the continued "
                                     "paragraph has company on both sides."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "CONTINUE IN PLACE",
                        .control = "A shorter story · ordinary closing block",
                        .figure = chain("close-free-", article(true, {}), 176),
                        .note = "The closing block uses the remaining room."},
                       {.title = "START A NEW FRAME",
                        .control = "Closing block: startInNextFrame = true",
                        .figure =
                            chain("close-new-", article(true, {}, true), 176),
                        .note = "The same block moves to frame two even though "
                                "there is room before it."}},
                  .measure = 1020,
                  .gap = 18}),
             document::eyebrow("WHERE THE LINES SIT · one 124 px text frame"),
             sketch::kit::comparison(
                 {.cases =
                      {{.title = "ASCENT",
                        .control = "First baseline: ascent",
                        .figure =
                            seating("ascent", Seat::kAscent, Spend::kStart),
                        .note = "The first line clears its ascent below the "
                                "top rule."},
                       {.title = "CAP HEIGHT",
                        .control = "First baseline: cap height",
                        .figure =
                            seating("cap", Seat::kCapHeight, Spend::kStart),
                        .note = "Capitals meet the top rule; the passage "
                                "shifts as one."},
                       {.title = "LEAVE THE ROOM",
                        .control = "Distribution: start",
                        .figure =
                            seating("start", Seat::kAscent, Spend::kStart),
                        .note = "Extra depth remains below the passage."},
                       {.title = "SPEND THE ROOM",
                        .control = "Distribution: justify",
                        .figure =
                            seating("justify", Seat::kAscent, Spend::kJustify),
                        .note =
                            "Extra depth is shared between the same lines."}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(KeepsAndFrames, "Kit · API",
             "controlled two-frame comparisons of widow protection and forced "
             "starts, followed by the same passage under four frame placements")
