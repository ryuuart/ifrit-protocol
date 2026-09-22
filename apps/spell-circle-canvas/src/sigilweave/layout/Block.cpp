/** @file
 * The block partial's merges: the pure field copy that folds two partials
 * into one, the overlay that resolves one against the block in force
 * above it, the overlay onto a whole style, and the style a partial alone
 * names.
 */

#include "sigilweave/layout/Block.h"

namespace sigil::weave {
namespace {

/** The field @p field of @p total taken from @p base or left unstated,
 *  which is what a keyword said about it means. Every field of a block
 *  inherits, so `unset` reads as `inherit` and `inherit` is the base's
 *  own; `initial` is a field nobody stated, which is the paragraph
 *  layout's own answer. */
void applyKeyword(Block& total, const Block& base, BlockField field,
                  Keyword keyword) {
  const Block initial;
  const Block& from = keyword == Keyword::Initial ? initial : base;
  switch (field) {
    case BlockField::Leading:
      total.leading = from.leading;
      return;
    case BlockField::HalfLeading:
      total.halfLeading = from.halfLeading;
      return;
    case BlockField::Alignment:
      total.alignment = from.alignment;
      return;
    case BlockField::Justification:
      total.justification = from.justification;
      return;
    case BlockField::Hyphenation:
      total.hyphenation = from.hyphenation;
      return;
    case BlockField::TabStops:
      total.tabStops = from.tabStops;
      return;
    case BlockField::FirstLineIndent:
      total.firstLineIndent = from.firstLineIndent;
      return;
    case BlockField::LastLineIndent:
      total.lastLineIndent = from.lastLineIndent;
      return;
    case BlockField::WidowLines:
      total.widowLines = from.widowLines;
      return;
    case BlockField::OrphanLines:
      total.orphanLines = from.orphanLines;
      return;
    case BlockField::BalanceRaggedLines:
      total.balanceRaggedLines = from.balanceRaggedLines;
      return;
    case BlockField::WritingMode:
      total.writingMode = from.writingMode;
      return;
    case BlockField::LineBreakLocale:
      total.lineBreakLocale = from.lineBreakLocale;
      return;
    case BlockField::LineBreak:
      total.lineBreak = from.lineBreak;
      return;
    case BlockField::LastLineAlignment:
      total.lastLineAlignment = from.lastLineAlignment;
      return;
    case BlockField::JustifyLastLine:
      total.justifyLastLine = from.justifyLastLine;
      return;
    case BlockField::Kinsoku:
      total.kinsoku = from.kinsoku;
      return;
    case BlockField::Hanging:
      total.hanging = from.hanging;
      return;
    case BlockField::Mojikumi:
      total.mojikumi = from.mojikumi;
      return;
    case BlockField::Tsume:
      total.tsume = from.tsume;
      return;
    case BlockField::kCount:
      return;
  }
}

}  // namespace

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
  // The keywords ACCUMULATE and are not applied: a merge folds two
  // partials into one partial, and there is no block in force here to
  // resolve `inherit` against. `overlay` is where they land.
  into.keywords.overlay(over.keywords);
  return into;
}

Block overlay(const Block& base, const Block& over) {
  Block total = base;
  merge(total, over);
  // A field written as a keyword takes the base's value or none at all,
  // over whatever the merge copied: the two are ONE layer and the keyword
  // is the statement that wins. A resolved block states none of them.
  for (const KeywordTable<BlockField>::Entry& entry : over.keywords.entries())
    applyKeyword(total, base, entry.field, entry.keyword);
  total.keywords = {};
  return total;
}

ParagraphStyle overlay(ParagraphStyle base, const Block& over) {
  // A block handed straight to a whole style stands under NO ancestor.
  // `inherit` therefore comes to the field nobody stated, which is what
  // the style already carries, and resolving against an empty block is
  // exactly that. `initial` comes to the same thing HERE and not to the
  // layout's own default for that field: the door reads a partial and the
  // style beneath it is already a whole value, so there is nothing to
  // reset it to without a second table mapping every block field onto the
  // style's. State the field's value where a block must reset one.
  if (!over.keywords.empty()) return overlay(base, overlay(Block{}, over));
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
  if (!block.keywords.empty()) {
    apply(options, overlay(Block{}, block));
    return;
  }
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
