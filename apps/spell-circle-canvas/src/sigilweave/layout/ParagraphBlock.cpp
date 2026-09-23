/** @file
 * The block partial's merges: the pure field copy that folds two partials
 * into one, the overlay that resolves one against the block in force
 * above it, the overlay onto a whole style, and the style a partial alone
 * names.
 */

#include "sigilweave/layout/ParagraphBlock.h"

namespace sigil::weave {
namespace {

/** The field @p field of @p total taken from @p base or left unstated,
 *  which is what a keyword said about it means. Every field of a block
 *  inherits, so `unset` reads as `inherit` and `inherit` is the base's
 *  own; `initial` is a field nobody stated, which is the paragraph
 *  layout's own answer. */
void applyKeyword(ParagraphBlock& total, const ParagraphBlock& base,
                  ParagraphField field, Keyword keyword) {
  const ParagraphBlock initial;
  const ParagraphBlock& from = keyword == Keyword::Initial ? initial : base;
  switch (field) {
    case ParagraphField::Leading:
      total.leading = from.leading;
      return;
    case ParagraphField::HalfLeading:
      total.halfLeading = from.halfLeading;
      return;
    case ParagraphField::Alignment:
      total.alignment = from.alignment;
      return;
    case ParagraphField::Justification:
      total.justification = from.justification;
      return;
    case ParagraphField::Hyphenation:
      total.hyphenation = from.hyphenation;
      return;
    case ParagraphField::TabStops:
      total.tabStops = from.tabStops;
      return;
    case ParagraphField::FirstLineIndent:
      total.firstLineIndent = from.firstLineIndent;
      return;
    case ParagraphField::LastLineIndent:
      total.lastLineIndent = from.lastLineIndent;
      return;
    case ParagraphField::WidowLines:
      total.widowLines = from.widowLines;
      return;
    case ParagraphField::OrphanLines:
      total.orphanLines = from.orphanLines;
      return;
    case ParagraphField::BalanceRaggedLines:
      total.balanceRaggedLines = from.balanceRaggedLines;
      return;
    case ParagraphField::WritingMode:
      total.writingMode = from.writingMode;
      return;
    case ParagraphField::LineBreakLocale:
      total.lineBreakLocale = from.lineBreakLocale;
      return;
    case ParagraphField::LineBreak:
      total.lineBreak = from.lineBreak;
      return;
    case ParagraphField::LastLineAlignment:
      total.lastLineAlignment = from.lastLineAlignment;
      return;
    case ParagraphField::JustifyLastLine:
      total.justifyLastLine = from.justifyLastLine;
      return;
    case ParagraphField::Kinsoku:
      total.kinsoku = from.kinsoku;
      return;
    case ParagraphField::Hanging:
      total.hanging = from.hanging;
      return;
    case ParagraphField::Mojikumi:
      total.mojikumi = from.mojikumi;
      return;
    case ParagraphField::Tsume:
      total.tsume = from.tsume;
      return;
    case ParagraphField::kCount:
      return;
  }
}

}  // namespace

ParagraphBlock& merge(ParagraphBlock& into, const ParagraphBlock& over) {
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

ParagraphBlock overlay(const ParagraphBlock& base, const ParagraphBlock& over) {
  ParagraphBlock total = base;
  merge(total, over);
  // A field written as a keyword takes the base's value or none at all,
  // over whatever the merge copied: the two are ONE layer and the keyword
  // is the statement that wins. A resolved block states none of them.
  for (const KeywordTable<ParagraphField>::Entry& entry :
       over.keywords.entries())
    applyKeyword(total, base, entry.field, entry.keyword);
  total.keywords = {};
  return total;
}

ParagraphStyle overlay(ParagraphStyle base, const ParagraphBlock& over) {
  // A block handed straight to a whole style stands under NO ancestor.
  // `inherit` therefore comes to the field nobody stated, which is what
  // the style already carries, and resolving against an empty block is
  // exactly that. `initial` comes to the same thing HERE and not to the
  // layout's own default for that field: the door reads a partial and the
  // style beneath it is already a whole value, so there is nothing to
  // reset it to without a second table mapping every block field onto the
  // style's. State the field's value where a block must reset one.
  if (!over.keywords.empty())
    return overlay(base, overlay(ParagraphBlock{}, over));
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

ParagraphStyle toParagraphStyle(const ParagraphBlock& block) {
  return overlay(ParagraphStyle{}, block);
}

void apply(ParagraphLayoutOptions& options, const ParagraphBlock& block) {
  if (!block.keywords.empty()) {
    apply(options, overlay(ParagraphBlock{}, block));
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
