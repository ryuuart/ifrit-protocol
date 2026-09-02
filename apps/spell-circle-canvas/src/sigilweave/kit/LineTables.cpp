/** @file
 * The stock prohibition and hanging tables: which characters may not stand
 * at a line's edge, and how far one may stand outside it.
 */

#include "sigilweave/kit/LineTables.h"

namespace sigil::weave::kit {

namespace kinsoku {

KinsokuTable japanese() {
  KinsokuTable table;
  // May not OPEN a line: the closing brackets, the sentence marks, the
  // small kana and the sound marks, the prolonged sound mark, and the
  // repeat marks — every one of which reads as an error standing alone at
  // the head of a column.
  table.notLineStart =
      u"、。，．｡､・･：；？"
      u"！’”）〕］｝〉》」』"
      u"】―‐／＼ぁぃぅぇぉっ"
      u"ゃゅょゎァィゥェォッャ"
      u"ュョヮヵヶー゛゜ゝゞヽ"
      u"ヾ…‥";
  // May not CLOSE one: the opening brackets, which would otherwise end a
  // line with nothing to open.
  table.notLineEnd =
      u"‘“（〔［｛〈《「『【"
      u"｢";
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
