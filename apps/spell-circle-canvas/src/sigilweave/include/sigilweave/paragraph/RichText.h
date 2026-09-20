#pragma once

/** @file
 * @ingroup weave-document
 *
 * MIXED TEXT AS ONE VALUE: `RichText`, a passage described as runs and
 * the styles they are set in, and `rich()`, which starts one. There is no
 * markup language: a run of text carries a style, or the name of one, and
 * anything else a passage needs is asked for by SELECTION after the fact.
 * `Paragraph` is the other end of this, for the passage too custom for a
 * value; appending a rich text's runs in order is the whole conversion.
 */

#include <include/core/SkSize.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"
#include "sigilweave/style/TypeSheet.h"

namespace sigil::weave {

/** MIXED-STYLE TEXT AS A COMPARABLE VALUE: runs appended in order, each
 *  set in the base, in a whole style of its own, in a partial over the
 *  base, or in a class looked up by NAME through the `TypeSheet` that
 *  `RichText::styles` supplies. Nothing is inserted between runs, so the
 *  spaces are the author's. Two values describing the same runs in the
 *  same styles are EQUAL, which is what lets a caller rebuilding its text
 *  every frame shape nothing when the text did not change.
 *  @trap A name the sheet does not register resolves to the base rather
 *  than failing, so a misspelling draws in the default style. */
class RichText {
 public:
  /** One run of text and the style it is set in — or one INLINE SLOT, which
   *  is a run whose content is the single object-replacement character the
   *  flow anchors a reserved box at. */
  struct Run {
    std::u8string utf8;
    TextStyle style;        ///< resolved: its own, its class's, or the base
    std::string styleName;  ///< the name it was written with, if any
    /** THE PARTIAL THE RUN WAS WRITTEN WITH — its own, or the copy its
     *  class resolved to out of the sheet, and empty for a run written
     *  with a whole style or with none. It is the only form a run can be
     *  re-resolved from when a host supplies a base the passage never
     *  had. */
    std::optional<Type> over;
    /** Whether `style` is the run's OWN WHOLE style, stated outright
     *  rather than resolved out of the base: true for a run written with
     *  a `TextStyle`, false for a partial, a class name or nothing, which
     *  all inherit. */
    bool total = false;
    /** Non-empty on a SLOT run: the name whatever fills the reserved box
     *  answers to. */
    std::string slotName;
    SkSize slotSize = {0, 0};    ///< the box the breakers reserve
    float slotBaselineDrop = 0;  ///< the box's bottom, below the baseline
    bool operator==(const Run&) const = default;
  };

  /** Starts a value that states no base — see `hasBase()`. */
  RichText() = default;
  /** Starts a value whose unstyled runs — and unregistered names — are set
   *  in @p baseStyle. */
  explicit RichText(TextStyle baseStyle)
      : m_base(std::move(baseStyle)), m_hasBase(true) {}

  /** Appends a run in the base style. */
  RichText& add(std::u8string_view utf8);
  /** Appends a run in its own WHOLE style, which inherits nothing. */
  RichText& add(std::u8string_view utf8, TextStyle style);
  /** Appends a run in the base style changed by @p partial — the fields it
   *  names, and the base's everywhere else. */
  RichText& add(std::u8string_view utf8, Type partial);
  /** Appends a run in the class registered under @p styleName. */
  RichText& add(std::u8string_view utf8, std::string_view styleName);
  /** The same four, from UTF-8 held as `char`. */
  RichText& add(std::string_view utf8);
  RichText& add(std::string_view utf8, TextStyle style);
  RichText& add(std::string_view utf8, Type partial);
  RichText& add(std::string_view utf8, std::string_view styleName);

  /** Reserves an INLINE SLOT: @p size px of blank space woven into the
   *  flow as ONE UNBREAKABLE WORD, under the name whatever is placed there
   *  answers to. @p baselineDrop is how far the box's BOTTOM sits below
   *  the baseline, 0 standing it on the baseline like an inline image.
   *  @trap @p size is LOGICAL, so a vertical passage reports the rectangle
   *  the other way round, and @p baselineDrop applies only horizontally. */
  RichText& slot(std::string name, SkSize size, float baselineDrop = 0);
  /** Supplies the style sheet names resolve through, and re-resolves every
   *  named run already added. */
  RichText& styles(TypeSheet sheet);

  /** The style unstyled runs and unregistered names are set in. */
  [[nodiscard]] const TextStyle& base() const { return m_base; }
  /** Whether a base was ever NAMED — false for `rich()`, true for
   *  `rich(style)` however default that style is. What a host asks before
   *  it supplies a base of its own. */
  [[nodiscard]] bool hasBase() const { return m_hasBase; }
  /** The runs, in the order they were added. */
  [[nodiscard]] std::span<const Run> runs() const { return m_runs; }
  [[nodiscard]] bool empty() const { return m_runs.empty(); }
  /** Whether a style sheet is in play — false until `styles()` gives one.
   *  This is what a host offering an ambient registry asks before it
   *  supplies its own, so a sheet the author named is never replaced. */
  [[nodiscard]] bool hasStyles() const { return m_hasStyles; }

  /** Equal when the base — whether one was named at all included — the
   *  runs, their resolved styles, the partials and the names they were
   *  written with all match.
   *  @trap The style SHEET is deliberately not compared: a name is
   *  resolved as it is added, so two values that resolved alike describe
   *  the same passage however they got there. */
  bool operator==(const RichText& other) const {
    return m_hasBase == other.m_hasBase && m_base == other.m_base &&
           m_runs == other.m_runs;
  }

 private:
  TextStyle m_base;
  std::vector<Run> m_runs;
  TypeSheet m_styles;
  bool m_hasBase = false;
  bool m_hasStyles = false;
};

/** Starts a mixed-text value that states no base — see RichText. */
[[nodiscard]] RichText rich();
/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(TextStyle base);

}  // namespace sigil::weave
