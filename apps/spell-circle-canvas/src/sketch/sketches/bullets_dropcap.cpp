/** @file
 * bullets_dropcap — the furniture a page of set text carries, and how
 * little of it is a mechanism.
 *
 * A DROP CAP is an exclusion: the initial is an ordinary keyed element,
 * absolutely placed, and the body is an ordinary text leaf that flows
 * around that key — the same exclusion a photograph in a column gets,
 * resolved in the same pass. A letter can stand alone, or stand inside an
 * illuminated ornament whose silhouette the opening lines follow. How deep
 * a plain cap goes is its own type's SIZE; no ratio of the body size is
 * written anywhere.
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
 * A LIST is an indent with the marker standing in the room it opens:
 * `indent.start` holds EVERY line in, the first one included, and the
 * marker is a leaf placed beside the text at the block's own start. The
 * room belongs to the marker alone — a first line pulled back out of the
 * indent would begin exactly where the marker stands and print through
 * it. The numbering is the caller's to format, which is
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
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
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
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kBody{0.82f, 0.83f, 0.86f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};

const char* kPassage =
    "hen the measure changes the opening keeps its treatment, because "
    "the run was stated in the text's own terms and not in a range of "
    "characters somebody counted once. That is the whole difference "
    "between a nested style and a restyle by hand.";

weave::TextStyle serif(float size, SkColor4f color, float track = 0) {
  static const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle(
      {.face = face, .size = size, .color = color, .track = track});
}

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture, .padding = 14})
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

/** A caller-built initial: the star is both the ornament that paints and
 *  the silhouette the opening lines subtract. */
Element illuminated(const char* key, std::optional<kit::NestedStyle> nested) {
  Element ornament =
      box()
          .width(58)
          .height(64)
          .shape(sigil::geometry::shapes::star(8, 0.48f, 0.12f))
          .fill(Fill::color(kFigure))
          .child(text(u8"W", serif(27, kGround)).absolute().left(15).top(14));
  kit::DroppedCap made =
      kit::dropCap(std::move(ornament), toU8(kPassage), serif(11.5f, kBody),
                   key, kMargin, std::move(nested));
  return box()
      .child(std::move(made.initial))
      .child(std::move(made.body).width(Dim(kCell - 28)));
}

}  // namespace

struct BulletsDropCap final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const weave::TextStyle smallCaps = serif(11.5f, kFigure, 1.1f);

    // Two levels: two calls, the second inset by its own hang. A level is
    // not a mechanism here either.
    const std::vector<std::u8string> outer = {
        u8"The marker is set at the block's own start.",
        u8"Every line of the item begins one hang in, the first one "
        u8"included, so the marker has the room to itself."};
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

    ctx.composer.render(sketch::kit::page(
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
                        "vocabulary could already name, and a list is "
                        "an indent with the marker standing in the room "
                        "it opened")},
        kit::cells(
            {.cells =
                 {cell("kit::dropCap(\"W\", capType, rest, bodyType)",
                       "the initial is a keyed leaf placed absolutely "
                       "and the body flowAround()s it \xc2\xb7 no "
                       "drop-cap facility underneath",
                       dropped("cap-plain", {})),
                  cell("dropCap(ornament, rest, bodyType)",
                       "the star is the painted initial AND the "
                       "silhouette subtracted from each horizontal "
                       "line \xc2\xb7 type enters its notches",
                       illuminated("cap-ornament",
                                   kit::NestedStyle{
                                       .until = kit::NestedStyle::Until::Words,
                                       .count = 6,
                                       .style = smallCaps})),
                  cell("\xe2\x80\xa6"
                       ", NestedStyle{Delimiter, \"once.\"}",
                       "from the start THROUGH the first occurrence, "
                       "inclusive \xc2\xb7 an anchored non-greedy regex "
                       "with the mark literal-quoted",
                       dropped("cap-delim",
                               kit::NestedStyle{
                                   .until = kit::NestedStyle::Until::Delimiter,
                                   .delimiter = u8"once.",
                                   .style = smallCaps})),
                  cell("kit::bullets(items, markers, style, hang, "
                       "measure)",
                       "two levels, two calls \xc2\xb7 every line of an "
                       "item stands one hang in, the first included, "
                       "and the marker keeps the room the indent "
                       "opened",
                       std::move(list))},
             .gap = 14})));
  }
};

SIGIL_SKETCH(BulletsDropCap, "Kit \xc2\xb7 API",
             "one initial dropped into a paragraph three ways \xe2\x80\x94 "
             "plain, with a word-counted opening and with one that ends at "
             "a delimiter \xe2\x80\x94 beside a two-level hanging list")
