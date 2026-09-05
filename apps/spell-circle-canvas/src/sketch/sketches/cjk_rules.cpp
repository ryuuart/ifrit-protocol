/** @file
 * cjk_rules — the four tables a Japanese column is set under, on one
 * passage, one at a time.
 *
 * None of them is a rule the engine holds. WHICH characters may not stand
 * at a line's edge, HOW FAR a mark may hang outside the measure, and HOW
 * MUCH room stands between two full-width characters are decisions, and
 * decisions are the caller's — so each arrives as DATA the layout asks
 * for and has no opinion about. `weave::kit` ships stock tables and a
 * house's own table is a peer of them.
 *
 * A TAILORING COMES FIRST. Segmentation runs under a locale, and a locale
 * that names its line-break rules — the strict Japanese ones a printed
 * page is set under — already refuses most of the boundaries a kinsoku
 * table would forbid. The table is what a house adds ON TOP: one more
 * character it refuses to open a line with. That is why the locale cell
 * stands beside the table cell rather than under it — and why the two of
 * them, and the kinsoku cell, are the same picture as the reference on
 * this passage. The segmentation had already refused every boundary they
 * would have.
 *
 * `hanging` is burasagari here: the sentence marks alone, at a line's END,
 * as a fraction of their own advance — so a column squares optically
 * rather than on its advances. It is the LINE EDGE and has nothing to do
 * with a hanging indent.
 *
 * `mojikumi` reads the class of the character BEFORE a gap and the class
 * of the one after it, and nearly every entry of a real table is NEGATIVE:
 * an opening bracket carries its ink in its right half and a closing one
 * in its left, so two back to back leave a full em of white between two
 * marks that are each half air. `tsume` closes the gap after every
 * full-width character the table gives no class of its own, on top of
 * that. Both apply where two characters meet across a break opportunity.
 *
 * EDIT THESE FIRST
 *   kSize — the body size, px.
 *   kBracketRoom — the em fraction a closing/opening pair closes up by.
 *   kTsume — the em fraction every other full-width gap closes up by.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/LineTables.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cstddef>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 362};
constexpr float kCell = 163;
constexpr float kPicture = 148;

constexpr float kSize = 13;            // the body size, px
constexpr float kBracketRoom = -0.5f;  // an em fraction: brackets close up
constexpr float kTsume = -0.12f;       // …and every other full-width gap

constexpr SkColor4f kBody{0.88f, 0.88f, 0.90f, 1};

/** The passage's own register: a mincho face tagged `ja`, so the face's
 *  Japanese behaviour is what shapes it. */
weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Hiragino Mincho ProN", "Yu Mincho", "Songti SC", "Noto Serif CJK JP"});
  weave::TextStyle style = weave::textStyle({.size = kSize, .color = kBody});
  style.shaping.typeface = face;
  style.shaping.languageTag = "ja";
  return style;
}

/** A table of the one pair that matters most: a closing mark followed by
 *  an opening one, whose two half-air cells otherwise leave a full em of
 *  white between them. */
weave::MojikumiTable brackets(float room) {
  weave::MojikumiTable table;
  table.members[static_cast<size_t>(weave::MojikumiClass::kOpening)] = u"（「";
  table.members[static_cast<size_t>(weave::MojikumiClass::kClosing)] = u"）」";
  table.members[static_cast<size_t>(weave::MojikumiClass::kFullStop)] = u"。";
  table.members[static_cast<size_t>(weave::MojikumiClass::kComma)] = u"、";
  table.room[static_cast<size_t>(weave::MojikumiClass::kClosing)]
            [static_cast<size_t>(weave::MojikumiClass::kOpening)] = room;
  table.room[static_cast<size_t>(weave::MojikumiClass::kFullStop)]
            [static_cast<size_t>(weave::MojikumiClass::kOpening)] = room;
  // A closing mark carries its ink in its left half, so the gap after it
  // is air whatever follows.
  table.room[static_cast<size_t>(weave::MojikumiClass::kClosing)]
            [static_cast<size_t>(weave::MojikumiClass::kIdeograph)] = room;
  return table;
}

/** The one passage, set the same way in every cell: it carries brackets,
 *  a reading mark and two sentence marks, so each table has something to
 *  act on. */
Element column() {
  return text(
             u8"「組版」「行送り」の禁則は、行頭に句読点を置かない。"
             u8"約物の空きは詰め、行末には句点をぶら下げる。",
             body())
      .width(Dim(kCell - 24))
      .height(Dim(kPicture - 24))
      .writingMode(weave::WritingMode::kVerticalRL);
}

Element cell(const char* call, const char* note, Element body) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well({.width = kCell, .height = kPicture, .padding = 12})
          .child(std::move(body)));
}

}  // namespace

struct CjkRules final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("THE JAPANESE TABLES \xc2\xb7 kinsoku, hanging, "
                       "mojikumi, tsume, lineBreakLocale"),
         .subtitle = toU8("dials \xc2\xb7 the body size (13 px) \xc2\xb7 "
                          "the bracket room and the tsume, as em "
                          "fractions \xc2\xb7 the locale the "
                          "segmentation runs under"),
         .footer = toU8("every one of these is DATA the layout asks for "
                        "and holds no opinion about \xe2\x80\x94 which "
                        "marks a house forbids, hangs or closes up is a "
                        "decision, and a caller's own table is a peer "
                        "of the stock one")},
        kit::cells(
            {.cells = {cell("writingMode(kVerticalRL)",
                            "the passage with no table at all \xc2\xb7 the "
                            "reference every other cell is read against",
                            column()),
                       cell("kinsoku(kit::kinsoku::japanese())",
                            "the closing marks and non-starters may not "
                            "OPEN a column \xc2\xb7 identical to the "
                            "reference, because the segmentation had already "
                            "refused those boundaries",
                            column().kinsoku(sigil::weave::kit::kinsoku::japanese())),
                       cell("hanging(kit::hanging::japanese())",
                            "burasagari \xc2\xb7 the sentence marks alone, "
                            "at a column's END \xc2\xb7 no column of this "
                            "setting closes on one, so nothing hangs",
                            column().hanging(sigil::weave::kit::hanging::japanese())),
                       cell("mojikumi(brackets(-0.5))",
                            "half an em taken out of the gap between a "
                            "closing mark and an opening one \xc2\xb7 two "
                            "half-air cells set closer",
                            column().mojikumi(brackets(kBracketRoom))),
                       cell("mojikumi({}, tsume = -0.12)",
                            "every full-width gap the table gives no class "
                            "closed up on top of that \xc2\xb7 the whole "
                            "column shortens",
                            column().mojikumi(brackets(kBracketRoom), kTsume)),
                       cell("lineBreakLocale(\"ja\")",
                            "the tailoring the segmentation runs under "
                            "\xc2\xb7 the default already breaks this "
                            "passage the same way, which is the point of "
                            "the two cells before it",
                            column().lineBreakLocale("ja"))},
             .gap = 10})));
  }
};

SIGIL_SKETCH(CjkRules, "Kit \xc2\xb7 API",
             "one vertical passage set under each of the Japanese line "
             "tables in turn \xe2\x80\x94 the prohibitions, the hanging "
             "marks, the bracket room, the tsume and the locale")
