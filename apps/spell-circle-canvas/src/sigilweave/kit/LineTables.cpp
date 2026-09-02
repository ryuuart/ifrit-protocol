/** @file
 * The stock prohibition and hanging tables: which characters may not stand
 * at a line's edge, and how far one may stand outside it.
 */

#include "sigilweave/kit/LineTables.h"

#include "sigilweave/unicode/Unicode.h"

namespace sigil::weave::kit {

namespace {

/** Appends a code point to a prohibition set when it is set in a
 *  FULL-WIDTH CELL and one UTF-16 unit spells it.
 *
 *  A prohibition set is scanned one code unit at a time, so a character
 *  outside the basic plane cannot be named in one; every mark a line-edge
 *  convention is about is inside it, a full-width character outside it
 *  being an ideograph, which no such convention names. The full-width test
 *  is what makes the set the punctuation of the ideographic grid — the
 *  marks that occupy a cell beside the kanji — and leaves ASCII
 *  punctuation to the segmentation, which already knows it. */
void appendIfSetInAFullWidthCell(char32_t codePoint, std::u16string& out) {
  if (codePoint > 0xFFFF) return;
  if (!unicode::isFullWidth(codePoint)) return;
  out.push_back(static_cast<char16_t>(codePoint));
}

}  // namespace

namespace kinsoku {

KinsokuTable japanese() {
  // DERIVED, not typed: which characters may not stand at a line's edge is
  // a property Unicode carries as each character's line-break class, and a
  // set typed out by hand is that property transcribed once and then left
  // behind. The derivation runs on first use and the classes do not change
  // under a running process.
  static const KinsokuTable table = [] {
    KinsokuTable derived;
    for (const char32_t codePoint : unicode::lineStartProhibited())
      appendIfSetInAFullWidthCell(codePoint, derived.notLineStart);
    for (const char32_t codePoint : unicode::lineEndProhibited())
      appendIfSetInAFullWidthCell(codePoint, derived.notLineEnd);
    return derived;
  }();
  return table;
}

}  // namespace kinsoku

namespace hanging {

HangingTable latin() {
  HangingTable table;
  table.entries = {
      // The quotes carry almost no ink below their own height, so they hang
      // nearly whole.
      {u'“', 0.85f, 0.85f}, {u'”', 0.85f, 0.85f},
      {u'‘', 0.9f, 0.9f},   {u'’', 0.9f, 0.9f},
      {u'"', 0.8f, 0.8f},        {u'\'', 0.85f, 0.85f},
      // The dashes and the hyphen are all ink and hang about half.
      {u'—', 0.4f, 0.4f},   {u'–', 0.45f, 0.45f},
      {u'-', 0.55f, 0.55f},
      // The stops hang least: a full stop pulled far out reads as a gap in
      // the margin rather than as a squared one.
      {u'.', 0.0f, 0.35f},       {u',', 0.0f, 0.35f},
      {u';', 0.0f, 0.25f},       {u':', 0.0f, 0.25f},
      // A bracket hangs on the side it opens or closes and nowhere else.
      {u'(', 0.3f, 0.0f},        {u')', 0.0f, 0.3f},
      {u'[', 0.3f, 0.0f},        {u']', 0.0f, 0.3f},
  };
  return table;
}

HangingTable japanese() {
  HangingTable table;
  // Burasagari proper: the sentence marks alone, at the foot of a column
  // or the end of a line, and nothing at the head.
  table.entries = {
      {u'、', 0.0f, 1.0f}, {u'。', 0.0f, 1.0f},
      {u'，', 0.0f, 1.0f}, {u'．', 0.0f, 1.0f},
      {u'｡', 0.0f, 1.0f}, {u'､', 0.0f, 1.0f},
  };
  return table;
}

}  // namespace hanging

}  // namespace sigil::weave::kit
