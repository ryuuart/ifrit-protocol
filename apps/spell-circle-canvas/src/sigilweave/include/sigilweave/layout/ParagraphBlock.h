#pragma once

/** @file
 * @ingroup weave-layout
 *
 * `ParagraphBlock` — a block's setting as a PARTIAL: every field optional, so a
 * call site states the one thing it changes and says nothing about the rest.
 * Beside it the merges (`merge` folds one partial into another, `overlay`
 * resolves one against a whole `ParagraphStyle`) and the style a partial
 * alone names (`toParagraphStyle`). `ParagraphBlock` is to `ParagraphStyle`
 * what `Type` is to `TextStyle`.
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
#include "sigilweave/style/Keyword.h"

namespace sigil::weave {

/** ONE FIELD OF A BLOCK PARTIAL, named so a keyword can be said about
 *  it. Every one of them inherits, as every field of a text style does,
 *  so `initial` is the keyword that says something new. */
enum class ParagraphField : uint8_t {
  Leading,
  HalfLeading,
  Alignment,
  Justification,
  Hyphenation,
  TabStops,
  FirstLineIndent,
  LastLineIndent,
  WidowLines,
  OrphanLines,
  BalanceRaggedLines,
  WritingMode,
  LineBreakLocale,
  LineBreak,
  LastLineAlignment,
  JustifyLastLine,
  Kinsoku,
  Hanging,
  Mojikumi,
  Tsume,
  kCount
};

/** WHAT A BLOCK INHERITS, EVERY FIELD OPTIONAL — A PARTIAL: a field left
 *  unset is the field inherited, a field stated is the block's own,
 *  `overlay` is one step of that onto a whole style, and
 *  `toParagraphStyle` is what a partial names with nothing above it.
 *  @trap What a block keeps to ITSELF — its air before and after, its
 *  reservation, its keeps with the next block, its initial letter, its
 *  every-line insets — is not here but on the whole `ParagraphStyle`. */
struct ParagraphBlock {
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

  /** The fields written as `inherit`, `initial` or `unset` rather than
   *  as a value, resolved by `overlay` against the block in force above. */
  KeywordTable<ParagraphField> keywords;

  bool operator==(const ParagraphBlock&) const = default;

  /** Whether it states NOTHING — the partial that changes no field. */
  [[nodiscard]] bool empty() const {
    return !leading && !halfLeading && !alignment && !justification &&
           !hyphenation && !tabStops && !firstLineIndent && !lastLineIndent &&
           !widowLines && !orphanLines && !balanceRaggedLines && !writingMode &&
           !lineBreakLocale && !lineBreak && !lastLineAlignment &&
           !justifyLastLine && !kinsoku && !hanging && !mojikumi && !tsume &&
           keywords.empty();
  }
};

/** FOLDS `over` INTO `into` — a pure field copy: every field `over` sets
 *  replaces `into`'s, every field it leaves unset leaves `into`'s alone.
 *  How two partials written about one block become one.
 *  @trap The keywords ACCUMULATE and are not applied: a merge has no
 *  block in force to resolve `inherit` against. `overlay` is where a
 *  keyword lands. */
ParagraphBlock& merge(ParagraphBlock& into, const ParagraphBlock& over);

/** `over` RESOLVED AGAINST `base` — one step of a cascade between two
 *  partials, `over` winning where it states a field, and every field it
 *  wrote as a keyword taking `base`'s value (`inherit`, `unset`) or none
 *  at all (`initial`). The answer states no keywords. */
[[nodiscard]] ParagraphBlock overlay(const ParagraphBlock& base,
                                     const ParagraphBlock& over);

/** THE SET FIELDS OF `over` APPLIED TO A WHOLE STYLE — one step of a
 *  cascade whose base is a `ParagraphStyle`. The writing mode and the
 *  locale are not the style's to hold and pass through untouched; a
 *  consumer reads them from the partial. */
[[nodiscard]] ParagraphStyle overlay(ParagraphStyle base,
                                     const ParagraphBlock& over);

/** The `ParagraphStyle` a partial names with nothing above it: the
 *  layout's own answer for every field it leaves unset. */
[[nodiscard]] ParagraphStyle toParagraphStyle(const ParagraphBlock& block);

/** THE LAYOUT-WIDE FIELDS OF @p options THE BLOCK IN FORCE SETS — the
 *  alignment, the breaking strategy, the hyphenation, the justification
 *  and its last line, the tab stops and the line tables — each where the
 *  partial states it.
 *  @silent the field is per-block, which is `overlay`'s, or the writing
 *  mode or the locale, which are the Paragraph's. */
void apply(ParagraphLayoutOptions& options, const ParagraphBlock& block);

}  // namespace sigil::weave
