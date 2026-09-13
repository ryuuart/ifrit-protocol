#pragma once

/** @file
 * @ingroup layout
 *
 * `Block` — a block's setting as a PARTIAL: every field optional, so a call
 * site states the one thing it changes and says nothing about the rest.
 * Beside it the merges (`merge` folds one partial into another, `overlay`
 * resolves one against a whole `ParagraphStyle`) and the style a partial
 * alone names (`toParagraphStyle`). `Block` is to `ParagraphStyle` what
 * `Type` is to `TextStyle`.
 */

#include <optional>
#include <string>

#include "sigilweave/layout/Breaking.h"
#include "sigilweave/layout/Justification.h"
#include "sigilweave/layout/LayoutOptions.h"
#include "sigilweave/layout/Mojikumi.h"
#include "sigilweave/layout/ParagraphStyle.h"
#include "sigilweave/layout/TabStops.h"
#include "sigilweave/paragraph/Hyphenation.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {

/** WHAT A BLOCK INHERITS, EVERY FIELD OPTIONAL — A PARTIAL.
 *
 *  It carries the settings a block takes from the passage it stands in
 *  when it says nothing of its own: the pitch and where its room goes,
 *  the alignment, the justification and its last line, the hyphenation,
 *  the tab stops, the first- and last-line indents, the widow and orphan
 *  counts, balanced ragging, the breaking strategy, the writing mode, the
 *  locale the lines break under, and the line tables a house sets CJK
 *  text by. What
 *  a block keeps to itself — its air before and after, its reservation,
 *  its keeps with the next block, its initial letter, its every-line
 *  insets — stays on the whole `ParagraphStyle`, as a margin is a box's
 *  own and not its children's.
 *
 *  A field left unset is the field inherited; a field stated is the
 *  block's own. `overlay()` is one step of that, onto a whole style, and
 *  `toParagraphStyle()` is what a partial names with nothing above it: the
 *  layout's own answer for every field it leaves unset. */
struct Block {
  std::optional<Leading> leading;
  std::optional<bool> halfLeading;
  std::optional<TextAlignment> alignment;
  std::optional<JustificationOptions> justification;
  std::optional<HyphenationOptions> hyphenation;
  std::optional<TabStopOptions> tabStops;
  /** Added to the start inset on the block's first line, px; negative
   *  hangs the first line out (IndentOptions::firstLine). */
  std::optional<float> firstLineIndent;
  /** The same on the block's last line (IndentOptions::lastLine). */
  std::optional<float> lastLineIndent;
  std::optional<int> widowLines;
  std::optional<int> orphanLines;
  std::optional<bool> balanceRaggedLines;
  /** The direction the lines run in (Paragraph::setWritingMode). */
  std::optional<WritingMode> writingMode;
  /** The BCP-47 locale the line breaker segments under
   *  (Paragraph::setLineBreakLocale). */
  std::optional<std::string> lineBreakLocale;
  /** Greedy or optimal breaking (ParagraphLayoutOptions::lineBreakStrategy). */
  std::optional<LineBreakStrategy> lineBreak;
  /** How a paragraph-final line sits under justification, and whether it
   *  stretches to the measure (JustificationOptions::lastLineAlignment and
   *  justifyLastLine) — stated apart so a passage can name the last line
   *  without restating the rest of its justification. */
  std::optional<TextAlignment> lastLineAlignment;
  std::optional<bool> justifyLastLine;
  /** The house's own line-edge prohibitions, hanging allowances, full-width
   *  gaps and tsume (ParagraphLayoutOptions::kinsoku, hanging, mojikumi and
   *  tsume). */
  std::optional<KinsokuTable> kinsoku;
  std::optional<HangingTable> hanging;
  std::optional<MojikumiTable> mojikumi;
  std::optional<float> tsume;

  bool operator==(const Block&) const = default;

  /** Whether it states NOTHING — the partial that changes no field. */
  [[nodiscard]] bool empty() const {
    return !leading && !halfLeading && !alignment && !justification &&
           !hyphenation && !tabStops && !firstLineIndent && !lastLineIndent &&
           !widowLines && !orphanLines && !balanceRaggedLines && !writingMode &&
           !lineBreakLocale && !lineBreak && !lastLineAlignment &&
           !justifyLastLine && !kinsoku && !hanging && !mojikumi && !tsume;
  }
};

/** FOLDS `over` INTO `into` — a pure field copy: every field `over` sets
 *  replaces `into`'s, every field it leaves unset leaves `into`'s alone.
 *  How two partials written about one block become one. */
Block& merge(Block& into, const Block& over);

/** THE SET FIELDS OF `over` APPLIED TO A WHOLE STYLE — one step of a
 *  cascade whose base is a `ParagraphStyle`. The writing mode and the
 *  locale are not the style's to hold and pass through untouched; a
 *  consumer reads them from the partial. */
[[nodiscard]] ParagraphStyle overlay(ParagraphStyle base, const Block& over);

/** The `ParagraphStyle` a partial names with nothing above it: the
 *  layout's own answer for every field it leaves unset. */
[[nodiscard]] ParagraphStyle toParagraphStyle(const Block& block);

/** THE LAYOUT-WIDE FIELDS OF @p options THE BLOCK IN FORCE SETS — the
 *  alignment, the breaking strategy, the hyphenation, the justification
 *  and its last line, the tab stops and the line tables — each where the
 *  partial states it, the rest as the options already hold them. The
 *  per-block fields are `overlay`'s and `toParagraphStyle`'s; the writing
 *  mode and the locale are the Paragraph's and a consumer sets them
 *  there. */
void apply(ParagraphLayoutOptions& options, const Block& block);

}  // namespace sigil::weave
