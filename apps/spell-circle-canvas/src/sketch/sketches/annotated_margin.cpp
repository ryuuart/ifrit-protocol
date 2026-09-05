/** @file
 * annotated_margin — things that stand BESIDE a text and reserve nothing:
 * a label per word, a note per line in the gutter, a rule cut to what a
 * block occupies, and a marker riding a cascade.
 */

// THE OTHER HALF OF ANNOTATION. A reading that reserves is part of the
// text (ruby_kenten shows that half); everything here is a SIBLING that
// reads the finished layout and stands next to it. Nothing on this page
// moves the text it annotates, and nothing on it is placed by a
// coordinate — every position is one unit's own rect, read off the
// glyphs rather than measured again.
//
// What is on the page:
//
//   · WORD LABELS — one small caption under every word of the first
//     phrase, keyed by that word's own text range. They follow the wrap:
//     narrow the measure and each label goes with its word.
//   · MARGINALIA — one note per LINE, in the gutter, with a leader from
//     the note to the line's own left edge.
//   · THE RULE — a hairline cut to the extent the block's lines actually
//     occupy, which is narrower than the box they sit in.
//   · THE PLAYHEAD — a marker riding an fx() cascade over the same text,
//     placed from the beat rather than from a coordinate, so it agrees
//     with the letters whatever the cascade turns out to be.
//
// EDIT THESE FIRST
//   kMeasure  — the measure the passage is set to. Change it and every
//               label and every note follows the new wrap.
//   kGutter   — how far the marginalia stand off the text.

#include <sigilcompose/kit/Annotations.h>
#include <sigilcompose/kit/Instruments.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
using namespace std::chrono_literals;
namespace motion = sigil::motion;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1100, 760};

namespace margin {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMeasure = 430;
constexpr float kGutter = 26;
constexpr float kNoteMeasure = 190;
constexpr float kTextLeft = 300;
constexpr float kTextTop = 150;

const SkColor4f kPaper{0.972f, 0.968f, 0.960f, 1};
const SkColor4f kInk{0.106f, 0.114f, 0.129f, 1};
const SkColor4f kFaint{0.106f, 0.114f, 0.129f, 0.32f};
const SkColor4f kMark{0.192f, 0.404f, 0.545f, 1};
const SkColor4f kHot{0.780f, 0.286f, 0.176f, 1};

sk_sp<SkTypeface> serif() {
  return weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
}
sk_sp<SkTypeface> grotesque() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
}

weave::TextStyle body(float size = 19.0f) {
  return weave::textStyle({.face = serif(), .size = size, .color = kInk});
}
weave::TextStyle note(float size = 8.5f, SkColor4f colour = kFaint,
                      float track = 0.4f) {
  return weave::textStyle(
      {.face = grotesque(), .size = size, .color = colour, .track = track});
}

/** The cascade the playhead rides, and the ms its master must span for it
 *  to run at those numbers: `Track::spanMs` is the same arithmetic
 *  `Composer::cascadeSpanMs` reads back off the mounted track, computed
 *  here from the word count before any node exists. */
const motion::Spread kRoll{.eachMs = 90, .durationMs = 420};
const float kRollSpan = kRoll.spanMs(12);  // the line below is twelve words

constexpr const char8_t* kPassage =
    u8"Marginalia stand beside a text and reserve nothing. Each note here "
    u8"is placed from the line it names, so a change of measure carries "
    u8"every one of them to wherever its line went.";

}  // namespace margin

struct AnnotatedMargin final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // MID-CASCADE. The playhead is the point of the lower strip, and a
    // meter photographed after its schedule has closed is nine full bars
    // saying nothing; this falls a little past half way through the roll.
    sketch::kit::stage(
        ctx,
        {.size = kSceneSize, .captureAt = 1.0, .background = margin::kPaper});
    ctx.composer.render(describe(ctx));
    // THE ANNOTATIONS ARE A READ-BACK: they resolve from the layout the
    // last draw left standing, so the page is described once for the text
    // and again for the things standing beside it. That second describe is
    // the whole cost of a sibling annotation, and it is why a reading that
    // must never lag is part of the text instead.
    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext& ctx) override {
    ctx.composer.render(describe(ctx));
  }

  Element describe(sketch::SketchContext& ctx) {
    namespace m = margin;
    namespace ch = choreograph;
    const Composer& composer = ctx.composer;

    Element page =
        box()
            .fill(Fill::color(m::kPaper))
            .child(box()
                       .absolute()
                       .inset(52, 44, 0, 0)
                       .column()
                       .gap(6)
                       .child(text(toU8("BESIDE THE TEXT"),
                                   m::note(12, m::kInk, 4.0f)))
                       .child(text(toU8("one element per unit, placed from "
                                        "the unit's own rect"),
                                   m::note(10, m::kFaint, 0.3f))))
            // The passage itself: one leaf, keyed, and annotated by
            // nothing — everything below reads it from outside.
            .child(text(m::kPassage, m::body())
                       .key("passage")
                       .absolute()
                       .left(Dim(m::kTextLeft))
                       .top(Dim(m::kTextTop))
                       .width(Dim(m::kMeasure))
                       .paragraph({.leading = weave::Leading::multiple(1.55f)}))
            // The same text again, lower, under a cascade — the playhead
            // below rides its beats.
            .child(text(toU8("A marker placed from a beat agrees with the "
                             "letters by construction."),
                        m::body(17))
                       .key("cascade")
                       .absolute()
                       .left(Dim(m::kTextLeft))
                       .top(Dim(m::kH - 210))
                       .width(Dim(m::kMeasure))
                       .fx({.effect = fx::rise(14),
                            .stagger = m::kRoll,
                            .over = weave::unit::Word,
                            .progress = animate(
                                motion::from(0.0f).to(1.0f),
                                {std::chrono::milliseconds((int)m::kRollSpan),
                                 &ch::easeNone, 200ms})}));

    // ── The label under every word of the opening phrase ────────────────
    page.child(kit::annotate(composer, "passage", weave::sel::words(0, 6),
                             weave::unit::Word,
                             {.side = kit::Beside::Side::After, .gap = 5.0f},
                             [&](const TextUnit& unit) {
                               // The label says what the unit IS — its range
                               // and the line it landed on — because a label
                               // that only repeated the word would be showing
                               // nothing the word does not already show.
                               return text(
                                   toU8(std::to_string(unit.range.start) +
                                        "\xe2\x80\x93" +
                                        std::to_string(unit.range.end)),
                                   m::note(7.5f, m::kMark, 0.2f));
                             })
                   .absolute()
                   .inset(0, 0, 0, 0));

    // ── One note per line, in the gutter, with a leader ──────────────────
    page.child(
        kit::annotate(composer, "passage", weave::sel::each(weave::unit::Line),
                      weave::unit::Line,
                      {.side = kit::Beside::Side::Start,
                       .gap = m::kGutter,
                       .measure = m::kNoteMeasure},
                      [&](const TextUnit& unit) {
                        return box()
                            .width(Dim(m::kNoteMeasure))
                            .justify(Justify::End)
                            .row()
                            .gap(8)
                            .child(text(
                                toU8("line " + std::to_string(unit.lineIndex) +
                                     " \xc2\xb7 baseline " +
                                     std::to_string((int)unit.axis)),
                                m::note()))
                            .child(box()
                                       .width(Dim(m::kGutter - 6))
                                       .height(Dim(1.0f))
                                       .fill(Fill::color(m::kFaint)));
                      })
            .absolute()
            .inset(0, 0, 0, 0));

    // ── A rule cut to what the block occupies ───────────────────────────
    page.child(kit::rules(composer, "passage",
                          weave::sel::each(weave::unit::Line),
                          {.where = kit::BlockRule::Where::Below,
                           .thickness = 1.0f,
                           .gap = 14.0f,
                           .colour = m::kMark})
                   .absolute()
                   .inset(0, 0, 0, 0));

    // ── The playhead, riding the cascade ────────────────────────────────
    page.child(kit::trackMeter(composer, "cascade", 0, m::kHot,
                               {m::kHot.fR, m::kHot.fG, m::kHot.fB, 0.12f},
                               {.where = kit::MeterPlacement::Where::Under,
                                .thickness = 3.0f,
                                .gap = 5.0f,
                                .trim = 2.0f})
                   .absolute()
                   .inset(0, 0, 0, 0));

    return page.child(
        text(toU8("a sibling annotation reserves nothing and lags a frame; "
                  "a reading that must never lag is part of the text"),
             m::note(10, m::kFaint, 0.2f))
            .absolute()
            .inset(52, m::kH - 34, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(AnnotatedMargin, "annotated_margin", "Catalog \xc2\xb7 Type",
                "labels, marginalia and rules read off a text's units")
