#pragma once

/** @file
 * @ingroup paragraph
 *
 * MIXED TEXT AS ONE VALUE: `RichText`, a passage described as runs and the
 * styles they are set in, and `rich()`, which starts one.
 *
 * A paragraph whose words are not all set the same way is a VALUE here,
 * not a document to be marked up. There is no markup language: a run of
 * text carries a style, or the name of one, and that is the whole
 * vocabulary. Whatever else a passage needs — a colour on the numbers, a
 * weight on one phrase — is asked for by SELECTION after the fact
 * (`query/Selector.h`), so the content stays content and the type
 * treatment stays in one place.
 *
 * `Paragraph` is the other end of this: a document built span by span, with
 * an edit log and a shaping cache, for the passage too custom for a value.
 * A rich text is what a caller who only wants to SAY what the text is
 * writes instead, and appending its runs to a paragraph in order is the
 * whole of turning one into the other.
 */

#include <include/core/SkSize.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/style/StyleSheet.h"
#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"

namespace sigil::weave {

/** MIXED-STYLE TEXT AS A COMPARABLE VALUE.
 *
 *      RichText passage = rich(base)
 *                             .add(u8"Signal ")
 *                             .add(u8"woven", accent)
 *                             .add(u8" through ")
 *                             .add(u8"noise", mono);
 *
 *  `add(utf8)` sets a run in the base style; `add(utf8, style)` sets it in
 *  its own, whole; `add(utf8, partial)` changes the base in the fields that
 *  partial names and leaves the rest (Type.h); `add(utf8, name)` sets it in
 *  a class looked up by NAME (see below). Runs are appended in order and
 *  concatenate into one passage — nothing is inserted between them, so the
 *  spaces are the author's.
 *
 *  WHY IT IS A VALUE. Two rich texts describing the same runs in the same
 *  styles are EQUAL, so a caller that rebuilds its text every frame can ask
 *  whether the text actually changed and shape nothing when it did not. A
 *  `Paragraph` cannot answer that question — it is a document with an
 *  identity and an edit history, and a freshly built one reads as new
 *  content however familiar its words are.
 *
 *  NAMES resolve through a `StyleSheet`, which `styles()` supplies. A name
 *  the sheet does not register resolves to the base handed to `rich()` —
 *  the base is this text's one default, and a misspelled name shows as
 *  content set in it rather than as content that did not draw. Resolution
 *  happens as the run is added, and again over every named run when
 *  `styles()` arrives, so the two may be written in either order and the
 *  finished value holds real styles rather than a reference to a registry
 *  that may since have gone. A host that offers a sheet AMBIENTLY to
 *  everything described in a scope asks `hasStyles()` and calls `styles()`
 *  itself when the answer is no; a sheet the value already carries always
 *  wins.
 *
 *  THE BASE ITSELF MAY BE LEFT UNSAID. `rich()` with no argument is a
 *  passage that states no default at all, which is not the same as one
 *  whose default happens to be a default-constructed style: the first
 *  expects to be given a base by whatever sets it, the second has named
 *  one. `hasBase()` is the difference, and every run keeps the partial it
 *  was written with in `Run::over` so a host that later supplies the base
 *  can resolve the runs against it rather than over a style they were never
 *  written against. */
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
     *  class resolved to out of the sheet. Empty when the run was written
     *  with a whole style, or with none at all. It is kept because a
     *  partial is the only form a run can be re-resolved from: a host that
     *  supplies a base the passage never had overlays this onto it, where
     *  the already-resolved `style` would carry the old base's fields. */
    std::optional<Type> over;
    /** Whether `style` is the run's OWN WHOLE style, stated outright rather
     *  than resolved out of the base. True for a run written with a
     *  `TextStyle`, false for one written with a partial, with a class
     *  name, or with nothing — which all inherit, and all carry an empty
     *  `over` unless a partial reached them. */
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

  /** Reserves an INLINE SLOT: `size` px of blank space woven into the flow,
   *  and the name whatever is placed in that space answers to.
   *
   *  The reserved box is ONE UNBREAKABLE WORD: a line never breaks inside
   *  it, and a box taller than the type opens the lines of its BLOCK. The
   *  room is in the strut before anything is broken, because bands are
   *  asked of the geometry before anyone knows which words land on them —
   *  a depth found afterwards would be a depth decided after the break it
   *  decides.
   *
   *  `baselineDrop` is how far the box's BOTTOM sits below the baseline —
   *  0 stands it on the baseline like an inline image, and about the face's
   *  descent centres a pill on the x-height.
   *
   *  The slot occupies one code point (U+FFFC), so it counts as a cluster,
   *  falls inside the ranges a selection names, and takes its turn in
   *  anything that steps over units exactly as a letter does. The names are
   *  this value's own and are matched in declaration order against the
   *  placeholder boxes the layout reports; nothing outside the passage can
   *  reach one. */
  RichText& slot(std::string name, SkSize size, float baselineDrop = 0);
  /** Supplies the style sheet names resolve through, and re-resolves every
   *  named run already added. */
  RichText& styles(StyleSheet sheet);

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
   *  written with all match: the question a caller asking "is this the same
   *  text?" needs answered.
   *
   *  The style SHEET is deliberately not compared: a name is resolved as it
   *  is added, so two values that resolved to the same styles describe the
   *  same passage however they got there, and an entry neither of them
   *  names cannot make them differ. Whether a base was NAMED is compared,
   *  because a passage waiting for one is not the passage that settled on
   *  the default. */
  bool operator==(const RichText& other) const {
    return m_hasBase == other.m_hasBase && m_base == other.m_base &&
           m_runs == other.m_runs;
  }

 private:
  TextStyle m_base;
  std::vector<Run> m_runs;
  StyleSheet m_styles;
  bool m_hasBase = false;
  bool m_hasStyles = false;
};

/** Starts a mixed-text value that states no base — see RichText. */
[[nodiscard]] RichText rich();
/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(TextStyle base);

}  // namespace sigil::weave
