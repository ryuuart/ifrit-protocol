/** @file
 * ruby_kenten — readings set beside the type: mono, group and jukugo ruby
 * over a column, emphasis dots beside a phrase, and a base whose reading
 * splits with it across a column break.
 */

// A READING IS PART OF THE TEXT. Every reading on this page is one
// Annotation on the text leaf it reads — a selector, a unit, the reading
// itself and the type it is set in — and the band it needs is in the
// base's strut BEFORE the base is broken. Nothing here reads a finished
// layout and draws over it; the base is laid out once, with the room
// already open.
//
// The four columns, and the one thing each is for:
//
//   · MONO — one reading per character (weave::Unit::Cluster). The pitch
//     of the column opens by the reading's own line height, which is why
//     the bare column beside it is narrower.
//   · GROUP — one reading per word (weave::Unit::Word), centred on the whole
//     compound rather than distributed over its characters.
//   · JUKUGO — the compound annotated per cluster with the readings its
//     characters take, which is the same verb with a different unit and
//     a longer list.
//   · SPLIT — a compound long enough that the column breaks inside it.
//     Its reading splits with it, in proportion to the base's advance
//     either side, because the units it is placed from report on both
//     columns.
//
// Kenten runs down the fifth column: one sesame beside each character of
// a phrase, reserving nothing — emphasis marks sit in the room the line
// already has, which is why the marked phrase does not open the pitch.
//
// EDIT THESE FIRST
//   kBodySize / kRubySize — the two type sizes. The ruby's size is its
//                           OWN: there is no fraction of the base
//                           anywhere in the library or in this file.
//   kSplitHeight          — how deep the split column is, which is what
//                           decides where the base breaks.

// TAGS: Typography/CJK

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

#include "tategaki/VerticalSpecimen.h"

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace furigana {
using namespace vertical;
using namespace vertical::paper;

constexpr float kH = kSceneSize.fHeight;
constexpr float kBodySize = 26;
constexpr float kRubySize = 11;
constexpr float kKentenSize = 9;
constexpr float kColumnW = 70;  // the pitch a reserved band opens into
constexpr float kColumnH = 250;
constexpr float kSplitHeight = 120;

/** The one voice every column on this sheet is captioned in: the unit
 *  named, then what it does, both above the setting they describe and
 *  centred over it. Latin, set horizontally: a plate about a vertical
 *  convention that labelled itself vertically would be arguing its case
 *  in the same breath as showing it. */
inline kit::Caption voice() {
  return {.where = kit::Caption::Where::Above,
          .gap = 13,
          .noteGap = 7,
          .noteMeasure = 112.0f,
          .align = Align::Center};
}

/** THE TWO CLASSES A CAPTION IS SET IN — the name in the seal red, the
 *  remark a size down in the faded one. @p noteSize is the remark's,
 *  which the wide caption under the split setting states larger. */
inline weave::StyleSheet voiceClasses(float noteSize) {
  return weave::StyleSheet{{"label", labelType(9.5f, kAka, 1.6f)},
                           {"caption", labelType(noteSize, kUsu, 0.2f)}};
}

/** A captioned column: the caption over it, the specimen under it. */
inline Element column(const char* caption, const char* note, Element specimen) {
  return kit::cell(voice(), caption, note, std::move(specimen))
      .styleSheet(voiceClasses(8.5f));
}

}  // namespace furigana

struct RubyKenten {
  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 0.4,
                             .background = furigana::kKinari});
    ctx.composer.render(describe());
  }

  /** A vertical column of the body type, at the plate's own measure. */
  Element passage(Utf8 utf8, float height = furigana::kColumnH) {
    namespace f = furigana;
    return text(std::move(utf8), f::body(f::kBodySize, f::kSumi))
        .width(f::kColumnW)
        .height(height)
        .block({.writingMode = weave::WritingMode::kVerticalRL});
  }

  weave::Type rubyType() {
    namespace f = furigana;
    return f::bodyType(f::kRubySize, f::kSumi);
  }

  Element describe() {
    namespace f = furigana;
    const weave::Type marks = f::bodyType(f::kKentenSize, f::kAka);

    // MONO — one reading per character.
    Element mono =
        passage(
            u8"日本語の"
            u8"書物。")
            .textAnnotation(kit::ruby(
                weave::selectors::text(u8"日本語"), weave::Unit::Cluster,
                {u8"に", u8"ほ", u8"ご"}, rubyType(), 1.0f));

    // GROUP — one reading over the whole compound.
    Element group = passage(
                        u8"日本語の"
                        u8"書物。")
                        .textAnnotation(kit::ruby(
                            weave::selectors::text(u8"書物"), weave::Unit::Word,
                            {u8"しょ"
                             u8"もつ"},
                            rubyType(), 1.0f));

    // JUKUGO — the compound per cluster, each character its own reading.
    Element jukugo =
        passage(
            u8"国語辞典"
            u8"を引く。")
            .textAnnotation(kit::ruby(
                weave::selectors::text(u8"国語辞典"), weave::Unit::Cluster,
                {u8"こく", u8"ご", u8"じ", u8"てん"}, rubyType(), 1.0f));

    // SPLIT — a column short enough that the base breaks inside the
    // compound; its reading breaks with it.
    Element split =
        passage(
            u8"序文のあと"
            u8"に国語辞典"
            u8"が立つ。",
            f::kSplitHeight)
            .width(f::kColumnW * 2.2f)
            .textAnnotation(kit::ruby(weave::selectors::text(u8"国語辞典"),
                                      weave::Unit::Word,
                                      {u8"こくごじ"
                                       u8"てん"},
                                      rubyType(), 1.0f));

    // KENTEN — one sesame beside each character, reserving nothing.
    Element kenten =
        passage(
            u8"ここだけ"
            u8"は見逃す"
            u8"な。")
            .textAnnotation(kit::kenten(weave::selectors::text(u8"見逃す"),
                                        marks, u8"﹅", 1.0f));

    // The wide caption under the split setting states its remark a size
    // larger than a column's, so it carries a sheet of its own.
    Element splitCell =
        kit::cell({.where = kit::Caption::Where::Above,
                   .gap = 13,
                   .noteGap = 7,
                   .noteMeasure = 300.0f},
                  "SPLIT · ACROSS A COLUMN BREAK",
                  "the base breaks inside the compound, so its "
                  "reading breaks with it, in proportion to the "
                  "base's advance either side",
                  std::move(split))
            .styleSheet(f::voiceClasses(9.0f))
            .left(52)
            .top(320);

    return box()
        .fill(linearGradient({0, 0}, {0, f::kH}, {f::kKinariLift, f::kKinari}))
        .font({.size = 10, .track = 0.2f})
        .ink(f::kUsu)
        .children(
            {box().left(52).top(44).column().gap(4).children(
                 {text("ルビと"
                       "傍点",
                       f::body(30, f::kSumi)),
                  box().height(6),
                  document::eyebrow("A READING IS PART OF THE TEXT")
                      .font({.size = 11, .color = f::kAi, .track = 3.0f}),
                  document::lead("the band it needs is in the base's strut "
                                 "before the base is broken, so the column "
                                 "pitch opens once\nand the reading is "
                                 "placed on the result")
                      .font({.size = 10.5f})
                      .width(430.0f)}),
             box()
                 .right(46)
                 .top(158)
                 .row()
                 .gap(20)
                 .justifyContent(Justify::End)
                 .children({f::column("KENTEN · CLUSTER",
                                      "one sesame a character, reserving "
                                      "nothing",
                                      std::move(kenten)),
                            f::column("JUKUGO · CLUSTER",
                                      "the compound per character, each its "
                                      "own reading",
                                      std::move(jukugo)),
                            f::column("GROUP · WORD",
                                      "one reading over the whole compound",
                                      std::move(group)),
                            f::column("MONO · CLUSTER",
                                      "one reading a character; the pitch "
                                      "opens to hold it",
                                      std::move(mono))}),
             std::move(splitCell),
             document::paragraph(
                 "mono · group · jukugo are the UNIT "
                 "and nothing else — the reading's size is "
                 "its own type's, never a fraction of the base's")
                 .left(52)
                 .bottom(34)});
  }
};

}  // namespace

SIGIL_SKETCH_AS(RubyKenten, "ruby_kenten", "Catalog · Type",
                "readings beside the type — mono, group, jukugo, "
                "kenten")
