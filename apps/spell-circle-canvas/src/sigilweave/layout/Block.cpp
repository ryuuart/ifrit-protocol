/** @file
 * The block partial's merges: the pure field copy that folds two partials
 * into one, the resolving overlay onto a whole style, and the style a
 * partial alone names.
 */

#include "sigilweave/layout/Block.h"

namespace sigil::weave {

Block& merge(Block& into, const Block& over) {
  if (over.leading) into.leading = over.leading;
  if (over.halfLeading) into.halfLeading = over.halfLeading;
  if (over.alignment) into.alignment = over.alignment;
  if (over.justification) into.justification = over.justification;
  if (over.hyphenation) into.hyphenation = over.hyphenation;
  if (over.tabStops) into.tabStops = over.tabStops;
  if (over.firstLineIndent) into.firstLineIndent = over.firstLineIndent;
  if (over.lastLineIndent) into.lastLineIndent = over.lastLineIndent;
  if (over.widowLines) into.widowLines = over.widowLines;
  if (over.orphanLines) into.orphanLines = over.orphanLines;
  if (over.balanceRaggedLines)
    into.balanceRaggedLines = over.balanceRaggedLines;
  if (over.writingMode) into.writingMode = over.writingMode;
  if (over.lineBreakLocale) into.lineBreakLocale = over.lineBreakLocale;
  if (over.lineBreak) into.lineBreak = over.lineBreak;
  if (over.lastLineAlignment) into.lastLineAlignment = over.lastLineAlignment;
  if (over.justifyLastLine) into.justifyLastLine = over.justifyLastLine;
  if (over.kinsoku) into.kinsoku = over.kinsoku;
  if (over.hanging) into.hanging = over.hanging;
  if (over.mojikumi) into.mojikumi = over.mojikumi;
  if (over.tsume) into.tsume = over.tsume;
  return into;
}

ParagraphStyle overlay(ParagraphStyle base, const Block& over) {
  if (over.leading) base.leading = *over.leading;
  if (over.halfLeading) base.halfLeading = *over.halfLeading;
  if (over.alignment) base.alignment = over.alignment;
  if (over.justification) base.justification = over.justification;
  if (over.hyphenation) base.hyphenation = over.hyphenation;
  if (over.tabStops) base.tabStops = over.tabStops;
  if (over.firstLineIndent) base.indent.firstLine = *over.firstLineIndent;
  if (over.lastLineIndent) base.indent.lastLine = *over.lastLineIndent;
  if (over.widowLines) base.keep.widowLines = *over.widowLines;
  if (over.orphanLines) base.keep.orphanLines = *over.orphanLines;
  if (over.balanceRaggedLines)
    base.balanceRaggedLines = *over.balanceRaggedLines;
  return base;
}

ParagraphStyle toParagraphStyle(const Block& block) {
  return overlay(ParagraphStyle{}, block);
}

void apply(ParagraphLayoutOptions& options, const Block& block) {
  if (block.alignment) options.alignment = *block.alignment;
  if (block.lineBreak) options.lineBreakStrategy = *block.lineBreak;
  if (block.hyphenation) options.hyphenation = *block.hyphenation;
  if (block.justification) options.justification = *block.justification;
  if (block.lastLineAlignment)
    options.justification.lastLineAlignment = *block.lastLineAlignment;
  if (block.justifyLastLine)
    options.justification.justifyLastLine = *block.justifyLastLine;
  if (block.tabStops) options.tabStops = *block.tabStops;
  if (block.kinsoku) options.kinsoku = *block.kinsoku;
  if (block.hanging) options.hanging = *block.hanging;
  if (block.mojikumi) options.mojikumi = *block.mojikumi;
  if (block.tsume) options.tsume = *block.tsume;
}

}  // namespace sigil::weave
