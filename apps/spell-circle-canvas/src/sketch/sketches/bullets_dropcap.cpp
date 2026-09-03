/** @file
 * bullets_dropcap — the furniture a page of set text carries, and how
 * little of it is a mechanism.
 *
 * A DROP CAP is an exclusion: the initial is an ordinary text leaf with a
 * key, absolutely placed, and the body is an ordinary text leaf that
 * flows around that key — the same exclusion a photograph in a column
 * gets, resolved in the same pass. How deep the cap goes is its own
 * type's SIZE, because the body flows around the box the initial
 * occupies; a cap three lines deep is a cap set three lines deep, and no
 * ratio of the body size is written anywhere.
 *
 * A NESTED STYLE is the opening of a paragraph set differently from the
 * rest of it. What makes it nested rather than a hand-cut restyle is that
 * the author says WHERE IT STOPS in the text's own terms — so many words,
 * so many characters, or through a delimiter — and the text decides where
 * that falls. `nestedRun` answers a plain selector and `spanStyle` does
 * the work; the delimiter form is an anchored non-greedy regular
 * expression with the mark literal-quoted, so a dash that is also a regex
 * operator means itself. Edit the copy and the run re-resolves.
 *
 * A LIST is a hanging indent with the marker set in the room the hang
 * opens: `indent.start` holds every line in and a negative
 * `indent.firstLine` pulls the first one back out, so the marker and the
 * opening word begin together at the block's start and the hang shows on
 * the lines after them. The numbering is the caller's to format, which is
 * why `markers` is strings: roman, lettered, restarting and hierarchical
 * schemes are data, and this is the shape they are drawn in. Two levels
 * is two calls.
 *
 * EDIT THESE FIRST
 *   kCapSize — the initial's type size, which IS the cap's depth.
 *   kMargin — how far the body stands off the initial, px.
 *   kHang — the indent a marker hangs in, px, per level.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilsketch/canvas/Sketch.h>
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

constexpr SkSize kCanvas = {1100, 496};
constexpr float kCell = 250;
constexpr float kPicture = 252;

constexpr float kCapSize = 46;  // the initial's size, which IS its depth
constexpr float kMargin = 7;    // the body's stand-off from the initial
constexpr float kHang = 16;     // the indent a marker hangs in, px

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBody{0.82f, 0.83f, 0.86f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

const char* kPassage =
    "hen the measure changes the opening keeps its treatment, because "
    "the run was stated in the text's own terms and not in a range of "
    "characters somebody counted once. That is the whole difference "
    "between a nested style and a restyle by hand.";

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle serif(float size, SkColor4f color, float track = 0) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle(
      {.face = face, .size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .width(Dim(kCell))
                       .height(Dim(kPicture))
                       .clip()
                       .fill(Fill::color(kCellGround))
                       .padding(14)
                       .child(std::move(body)));
}

/** One dropped cap over the same passage; `nested`, when given, sets the
 *  opening of the BODY — the cap is a leaf of its own, so the run is
 *  stated over what follows it. */
Element dropped(const char* key, std::optional<kit::NestedStyle> nested) {
  kit::DroppedCap made =
      kit::dropCap(u8"W", serif(kCapSize, kFigure), toU8(kPassage),
                   serif(11.5f, kBody), key, kMargin, std::move(nested));
  return box()
      .child(std::move(made.initial))
      .child(std::move(made.body).width(Dim(kCell - 28)));
}

}  // namespace

struct BulletsDropCap final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    const weave::TextStyle smallCaps = serif(11.5f, kFigure, 1.1f);

    // Two levels: two calls, the second inset by its own hang. A level is
    // not a mechanism here either.
    const std::vector<std::u8string> outer = {
        u8"The marker is set at the block's own start.",
        u8"The item's text begins one hang in, and every line after the "
        u8"first keeps that indent."};
    const std::vector<std::u8string> outerMarks = {u8"1.", u8"2."};
    const std::vector<std::u8string> inner = {
        u8"A number is the caller's to format.",
        u8"Roman, lettered, restarting, hierarchical \xe2\x80\x94 all data."};
    const std::vector<std::u8string> innerMarks = {u8"\xe2\x80\x94",
                                                  u8"\xe2\x80\x94"};

    Element list =
        box()
            .column()
            .gap(9)
            .child(kit::bullets(outer, outerMarks, serif(11, kBody), kHang,
                                kCell - 28 - kHang))
            .child(kit::bullets(inner, innerMarks, serif(10.5f, kAsh), kHang,
                                kCell - 28 - kHang * 2)
                       .margin(kHang, 0, 0, 0));

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("BULLETS AND THE DROPPED CAP \xc2\xb7 kit::"
                           "dropCap, kit::NestedStyle, kit::bullets"),
             .subtitle = toU8("dials \xc2\xb7 the cap's type size (46 px, "
                              "which IS its depth) \xc2\xb7 the body's "
                              "stand-off (7 px) \xc2\xb7 where the nested "
                              "run stops \xc2\xb7 the hang (16 px per "
                              "level)"),
             .footer = toU8("none of these is a mechanism: the cap is an "
                            "exclusion the body flows around, the nested "
                            "style is a span restyle over a selector the "
                            "vocabulary could already name, and a list is a "
                            "hanging indent"),
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
                     {cell("kit::dropCap(\"W\", capType, rest, bodyType)",
                           "the initial is a keyed leaf placed absolutely "
                           "and the body flowAround()s it \xc2\xb7 no "
                           "drop-cap facility underneath",
                           dropped("cap-plain", {})),
                      cell("\xe2\x80\xa6" ", NestedStyle{Words, 6}",
                           "the first six of the paragraph's own "
                           "line-break words, restyled \xc2\xb7 an edit that "
                           "adds a word before them moves the run",
                           dropped("cap-words",
                                   kit::NestedStyle{
                                       .until = kit::NestedStyle::Until::Words,
                                       .count = 6,
                                       .style = smallCaps})),
                      cell("\xe2\x80\xa6" ", NestedStyle{Delimiter, \"once.\"}",
                           "from the start THROUGH the first occurrence, "
                           "inclusive \xc2\xb7 an anchored non-greedy regex "
                           "with the mark literal-quoted",
                           dropped("cap-delim",
                                   kit::NestedStyle{
                                       .until =
                                           kit::NestedStyle::Until::Delimiter,
                                       .delimiter = u8"once.",
                                       .style = smallCaps})),
                      cell("kit::bullets(items, markers, style, hang, "
                           "measure)",
                           "two levels, two calls \xc2\xb7 the hang shows "
                           "on the lines AFTER the first, which the "
                           "negative firstLine pulls back to the marker",
                           std::move(list))},
                 .gap = 14}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(BulletsDropCap, "Kit \xc2\xb7 API",
             "one initial dropped into a paragraph three ways \xe2\x80\x94 "
             "plain, with a word-counted opening and with one that ends at "
             "a delimiter \xe2\x80\x94 beside a two-level hanging list")
