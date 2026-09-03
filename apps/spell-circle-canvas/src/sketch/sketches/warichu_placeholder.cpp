/** @file
 * warichu_placeholder — the aside a text sets small and DOUBLED inside
 * the line it interrupts, and the reserved box it stands in.
 *
 * A warichu is not set beside its base and not set under it: it is two
 * short lines occupying one inline slot of the base's own line. Two
 * questions have to be answered before the slot can be reserved, and
 * `weave::warichuSplit` answers both: WHERE the note is cut, and WHAT
 * ROOM the two lines then need.
 *
 * The cut is the break opportunity that leaves the two lines CLOSEST IN
 * ADVANCE, because two lines of one length is what makes the note read as
 * one object rather than as a line with something under it. It is a WORD
 * index — the note's first word on the second line — so the caller cuts
 * its own text at that word's start and sets the halves. The note's size
 * is its own style's, as everything beside a base is: nothing here halves
 * anything.
 *
 * The room is then an ordinary inline slot: `advance` is the wider of the
 * two lines and `band` the depth they stack into, which is exactly what
 * `RichText::slot` reserves and what the breakers treat as one unbreakable
 * word. The child laid into it draws the two lines.
 *
 * EDIT THESE FIRST
 *   kNote — the aside, whose own length decides where it is cut.
 *   kNoteSize — the note's type size, which is the whole of what makes it
 *     small: nothing derives it from the base.
 *   kDrop — how far the slot's bottom sits below the base's baseline.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/layout/Beside.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cstdio>
#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 424};
constexpr float kCell = 254;
constexpr float kPicture = 210;

constexpr float kNoteSize = 8;  // the note's own size, and nothing else's
constexpr float kBaseSize = 13;
constexpr float kDrop = 3;  // the slot's bottom, below the base's baseline

const char* kNote = "which a text sets small and doubled inside the line";

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBody{0.86f, 0.87f, 0.90f, 1};
constexpr SkColor4f kFigure{0.90f, 0.83f, 0.68f, 1};
constexpr SkColor4f kSlot{0.16f, 0.17f, 0.20f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle serif(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** UTF-16 back to UTF-8 for the two halves of a Latin note. The note is
 *  the caller's own text, so the caller knows what is in it. */
std::u8string narrow(std::u16string_view utf16) {
  std::u8string out;
  out.reserve(utf16.size());
  for (char16_t unit : utf16)
    if (unit < 0x80) out.push_back(static_cast<char8_t>(unit));
  return out;
}

std::string line(const char* format, auto... args) {
  char buffer[160];
  std::snprintf(buffer, sizeof buffer, format, args...);
  return buffer;
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .width(Dim(kCell))
                       .height(Dim(kPicture))
                       .clip()
                       .fill(Fill::color(kCellGround))
                       .padding(12)
                       .child(std::move(body)));
}

}  // namespace

struct WarichuPlaceholder final : sketch::Sketch {
  weave::WarichuSplit split;
  std::u8string first, second;
  std::string report[3];
  float oneLine = 0;

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    // The note as a paragraph of its own, which is what the split is asked
    // about: its size, its face and its language are the note's, and the
    // base has no say in any of them.
    weave::Paragraph note =
        weave::ParagraphBuilder(serif(kNoteSize, kFigure))
            .addText(toU8(kNote))
            .build();
    split = weave::warichuSplit(*ctx.fonts, note);

    const std::u16string& text = note.text();
    const uint32_t cut =
        split.cutWord < note.words().size()
            ? note.words()[split.cutWord].textBegin
            : static_cast<uint32_t>(text.size());
    first = narrow(std::u16string_view(text).substr(0, cut));
    second = narrow(std::u16string_view(text).substr(cut));

    oneLine = ctx.measure(box().child(text_(kNote))).width();
    report[0] = line("one line \xc2\xb7 advance %.1f px", oneLine);
    report[1] = line("split \xc2\xb7 advance %.1f \xc2\xb7 band %.1f",
                     split.advance, split.band);
    report[2] = line("cut at word %u \xc2\xb7 \"%s\"", split.cutWord,
                     reinterpret_cast<const char*>(second.c_str()));

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("WARICHU \xc2\xb7 weave::warichuSplit into a "
                           "reserved inline slot"),
             .subtitle = toU8("dials \xc2\xb7 the note's own size (8 px "
                              "against a 13 px base) \xc2\xb7 the slot's "
                              "baseline drop \xc2\xb7 the note's length, "
                              "which is what decides the cut"),
             .footer = toU8("the cut is the break opportunity that leaves "
                            "the two lines CLOSEST IN ADVANCE \xe2\x80\x94 "
                            "two lines of one length is what makes a note "
                            "read as one object rather than as a line with "
                            "something under it"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells({.cells = {oneLineCell(), splitCell(), verticalCell(),
                                  readoutCell()},
                        .gap = 14}))
            .absolute()
            .inset(0));
  }

  Element text_(const char* utf8) { return text(toU8(utf8), serif(kNoteSize, kFigure)); }

  /** The base sentence, with one inline slot in the middle of it. */
  Element based(SkSize slot, Element child, bool vertical = false) {
    Element leaf =
        text(rich(serif(kBaseSize, kBody))
                 .add(u8"A warichu ")
                 .slot("note", slot, kDrop)
                 .add(u8" interrupts the line it stands in, rather than "
                      u8"standing beside it."))
            .width(Dim(kCell - 24))
            // The pitch is the CALLER'S: a slot deeper than the type does
            // not open the line it lands on, so a base carrying a warichu
            // is set on a leading that already holds the band.
            .paragraph({.leading = weave::Leading::absolute(
                            kBaseSize + slot.height() + 4)})
            .child(box().key("note").fill(Fill::color(kSlot)).child(
                std::move(child)));
    if (vertical) {
      leaf.writingMode(weave::WritingMode::kVerticalRL)
          .width(Dim(kCell - 24))
          .height(Dim(kPicture - 24));
    }
    return leaf;
  }

  /** The note set as ONE line, which is what the slot holds when nothing
   *  splits it — and why a long aside interrupts so badly. */
  Element oneLineCell() {
    return cell("slot(\"note\", {one line, band})",
                "the aside set as a single line \xc2\xb7 it takes the base's "
                "whole measure and the line it interrupts has nowhere to go",
                based({oneLine, kNoteSize * 1.4f}, text_(kNote)));
  }

  /** The two lines, cut where the split said, stacked across the band it
   *  asked for. */
  Element splitCell() {
    return cell("warichuSplit(fonts, note)",
                "the same note in two lines of one length, in a slot the "
                "split sized \xc2\xb7 the base is set on a leading that "
                "already holds the band",
                based({split.advance, split.band}, stackedNote()));
  }

  /** The same slot in a vertical base: the two lines stack ACROSS the
   *  column, which is the setting the form comes from. */
  Element verticalCell() {
    return cell("\xe2\x80\xa6" " in a vertical base",
                "the two lines stack across the column \xc2\xb7 the slot is "
                "the same value and the writing mode is the base's",
                based({split.band, split.advance}, stackedNote(true), true));
  }

  /** THE SLOT'S CHILD IS A POSITIONED SUBTREE — the placeholder rect is
   *  its box and no flex layout runs inside it — so the two lines carry
   *  their own rects rather than stacking in a column. */
  Element stackedNote(bool vertical = false) {
    const float half = split.band * 0.5f;
    const auto row = [&](const std::u8string& text8, float along) {
      Element leaf = text(text8, serif(kNoteSize, kFigure)).absolute();
      if (vertical) {
        // The band is ACROSS the column in a vertical setting, so the two
        // lines stand side by side and each runs down the note's advance.
        leaf.left(Dim(along))
            .top(Dim(0.0f))
            .width(Dim(half))
            .height(Dim(split.advance))
            .writingMode(weave::WritingMode::kVerticalRL);
      } else {
        leaf.left(Dim(0.0f)).top(Dim(along)).width(Dim(split.advance));
      }
      return leaf;
    };
    return box().child(row(first, 0)).child(row(second, half));
  }

  /** What the split answered, printed. */
  Element readoutCell() {
    Element column = box().column().gap(8);
    for (const std::string& row : report)
      column.child(text(toU8(row), mono(10, kFigure)).width(Dim(kCell - 24)));
    return cell("WarichuSplit{advance, band, cutWord}",
                "what the split answered for this note at this size "
                "\xc2\xb7 the caller cuts its own text at that word's start",
                std::move(column));
  }
};

SIGIL_SKETCH(WarichuPlaceholder, "Kit \xc2\xb7 API",
             "one aside set as a single line and then as two of one length "
             "in the slot the split sized, in a horizontal base and in a "
             "vertical one, with the numbers printed")
