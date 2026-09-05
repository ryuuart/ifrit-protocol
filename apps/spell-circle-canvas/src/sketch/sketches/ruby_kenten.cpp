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
//   · MONO — one reading per character (unit::Cluster). The pitch of the
//     column opens by the reading's own line height, which is why the
//     bare column beside it is narrower.
//   · GROUP — one reading per word (unit::Word), centred on the whole
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

#include <shared/VerticalSpecimen.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize = vertical::kSceneSize;

namespace furigana {
using namespace vertical;
using namespace vertical::paper;

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kBodySize = 26;
constexpr float kRubySize = 11;
constexpr float kKentenSize = 9;
constexpr float kColumnW = 70;  // the pitch a reserved band opens into
constexpr float kColumnH = 250;
constexpr float kSplitHeight = 120;

/** Latin captions, set horizontally: a plate about a vertical convention
 *  that labelled itself vertically would be arguing its case in the same
 *  breath as showing it. */
inline weave::TextStyle label(float size, SkColor4f colour, float track = 0) {
  return weave::textStyle({.size = size, .color = colour, .track = track});
}

/** The one voice every column on this sheet is captioned in: the unit
 *  named, then what it does, both above the setting they describe and
 *  centred over it. */
inline kit::Caption voice() {
  return {.where = kit::Caption::Where::Above,
          .label = label(9.5f, kAka, 1.6f),
          .note = label(8.5f, kUsu, 0.2f),
          .gap = 13,
          .noteGap = 7,
          .noteMeasure = 112.0f,
          .align = Align::Center};
}

/** A captioned column: the caption over it, the specimen under it. */
inline Element column(const char* caption, const char* note, Element specimen) {
  return kit::cell(voice(), toU8(caption), toU8(note), std::move(specimen));
}

}  // namespace furigana

struct RubyKenten final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 0.4,
                             .background = furigana::kKinari});
    ctx.composer.render(describe());
  }

  /** A vertical column of the body type, at the plate's own measure. */
  Element passage(std::u8string utf8, float height = furigana::kColumnH) {
    namespace f = furigana;
    return text(std::move(utf8), f::body(f::kBodySize, f::kSumi))
        .width(Dim(f::kColumnW))
        .height(Dim(height))
        .writingMode(weave::WritingMode::kVerticalRL);
  }

  weave::TextStyle rubyType() {
    namespace f = furigana;
    return f::body(f::kRubySize, f::kSumi);
  }

  Element describe() {
    namespace f = furigana;
    const weave::TextStyle marks = f::body(f::kKentenSize, f::kAka);

    // MONO — one reading per character.
    Element mono =
        passage(
            u8"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae"
            u8"\xe6\x9b\xb8\xe7\x89\xa9\xe3\x80\x82")
            .annotate(kit::ruby(
                sel::text(u8"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"),
                unit::Cluster,
                {u8"\xe3\x81\xab", u8"\xe3\x81\xbb", u8"\xe3\x81\x94"},
                rubyType(), 1.0f));

    // GROUP — one reading over the whole compound.
    Element group = passage(
                        u8"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe3\x81\xae"
                        u8"\xe6\x9b\xb8\xe7\x89\xa9\xe3\x80\x82")
                        .annotate(kit::ruby(
                            sel::text(u8"\xe6\x9b\xb8\xe7\x89\xa9"), unit::Word,
                            {u8"\xe3\x81\x97\xe3\x82\x87"
                             u8"\xe3\x82\x82\xe3\x81\xa4"},
                            rubyType(), 1.0f));

    // JUKUGO — the compound per cluster, each character its own reading.
    Element jukugo =
        passage(
            u8"\xe5\x9b\xbd\xe8\xaa\x9e\xe8\xbe\x9e\xe5\x85\xb8"
            u8"\xe3\x82\x92\xe5\xbc\x95\xe3\x81\x8f\xe3\x80\x82")
            .annotate(kit::ruby(
                sel::text(u8"\xe5\x9b\xbd\xe8\xaa\x9e\xe8\xbe\x9e\xe5\x85\xb8"),
                unit::Cluster,
                {u8"\xe3\x81\x93\xe3\x81\x8f", u8"\xe3\x81\x94",
                 u8"\xe3\x81\x98", u8"\xe3\x81\xa6\xe3\x82\x93"},
                rubyType(), 1.0f));

    // SPLIT — a column short enough that the base breaks inside the
    // compound; its reading breaks with it.
    Element split =
        passage(
            u8"\xe5\xba\x8f\xe6\x96\x87\xe3\x81\xae\xe3\x81\x82\xe3\x81\xa8"
            u8"\xe3\x81\xab\xe5\x9b\xbd\xe8\xaa\x9e\xe8\xbe\x9e\xe5\x85\xb8"
            u8"\xe3\x81\x8c\xe7\xab\x8b\xe3\x81\xa4\xe3\x80\x82",
            f::kSplitHeight)
            .width(Dim(f::kColumnW * 2.2f))
            .annotate(kit::ruby(
                sel::text(u8"\xe5\x9b\xbd\xe8\xaa\x9e\xe8\xbe\x9e\xe5\x85\xb8"),
                unit::Word,
                {u8"\xe3\x81\x93\xe3\x81\x8f\xe3\x81\x94\xe3\x81\x98"
                 u8"\xe3\x81\xa6\xe3\x82\x93"},
                rubyType(), 1.0f));

    // KENTEN — one sesame beside each character, reserving nothing.
    Element kenten =
        passage(
            u8"\xe3\x81\x93\xe3\x81\x93\xe3\x81\xa0\xe3\x81\x91"
            u8"\xe3\x81\xaf\xe8\xa6\x8b\xe9\x80\x83\xe3\x81\x99"
            u8"\xe3\x81\xaa\xe3\x80\x82")
            .annotate(
                kit::kenten(sel::text(u8"\xe8\xa6\x8b\xe9\x80\x83\xe3\x81\x99"),
                            marks, u8"\xef\xb9\x85", 1.0f));

    return box()
        .fill(linearGradient({0, 0}, {0, f::kH}, {f::kKinariLift, f::kKinari}))
        .child(box()
                   .absolute()
                   .inset(52, 44, 0, 0)
                   .column()
                   .gap(4)
                   .child(text(toU8("\xe3\x83\xab\xe3\x83\x93\xe3\x81\xa8"
                                    "\xe5\x82\x8d\xe7\x82\xb9"),
                               f::body(30, f::kSumi)))
                   .child(box().height(6))
                   .child(text(toU8("A READING IS PART OF THE TEXT"),
                               f::label(11, f::kAi, 3.0f)))
                   .child(text(toU8("the band it needs is in the base's strut "
                                    "before the base is broken, so the column "
                                    "pitch opens once\nand the reading is "
                                    "placed on the result"),
                               f::label(10.5f, f::kUsu, 0.2f))
                              .width(Dim(430.0f))))
        .child(box()
                   .absolute()
                   .inset(0, 158, 46, 0)
                   .row()
                   .gap(20)
                   .justify(Justify::End)
                   .child(f::column("KENTEN \xc2\xb7 CLUSTER",
                                    "one sesame a character, reserving "
                                    "nothing",
                                    std::move(kenten)))
                   .child(f::column("JUKUGO \xc2\xb7 CLUSTER",
                                    "the compound per character, each its "
                                    "own reading",
                                    std::move(jukugo)))
                   .child(f::column("GROUP \xc2\xb7 WORD",
                                    "one reading over the whole compound",
                                    std::move(group)))
                   .child(f::column("MONO \xc2\xb7 CLUSTER",
                                    "one reading a character; the pitch "
                                    "opens to hold it",
                                    std::move(mono))))
        .child(kit::cell({.where = kit::Caption::Where::Above,
                          .label = f::label(9.5f, f::kAka, 1.6f),
                          .note = f::label(9.0f, f::kUsu, 0.2f),
                          .gap = 13,
                          .noteGap = 7,
                          .noteMeasure = 300.0f},
                         toU8("SPLIT \xc2\xb7 ACROSS A COLUMN BREAK"),
                         toU8("the base breaks inside the compound, so its "
                              "reading breaks with it, in proportion to the "
                              "base's advance either side"),
                         std::move(split))
                   .absolute()
                   .inset(52, 320, 0, 0))
        .child(text(toU8("mono \xc2\xb7 group \xc2\xb7 jukugo are the UNIT "
                         "and nothing else \xe2\x80\x94 the reading's size is "
                         "its own type's, never a fraction of the base's"),
                    f::label(10, f::kUsu, 0.2f))
                   .absolute()
                   .inset(52, f::kH - 34, 0, 0));
  }
};

}  // namespace

SIGIL_SKETCH_AS(RubyKenten, "ruby_kenten", "Catalog \xc2\xb7 Type",
                "readings beside the type \xe2\x80\x94 mono, group, jukugo, "
                "kenten")
