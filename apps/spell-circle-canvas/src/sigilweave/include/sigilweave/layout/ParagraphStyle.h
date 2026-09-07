#pragma once

/** @file
 * @ingroup layout
 *
 * ONE BLOCK'S SETTING — the paragraph controls a reader sees as a
 * paragraph: its pitch, its indents, the keeps that hold its lines
 * together, and the registry that names a set of them.
 */

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "sigilweave/layout/Breaking.h"
#include "sigilweave/layout/InitialLetter.h"
#include "sigilweave/layout/Justification.h"
#include "sigilweave/layout/Overflow.h"
#include "sigilweave/layout/TabStops.h"

namespace sigil::weave {

/**
 * How far apart a block's lines stand — its PITCH, which in a vertical
 * setting is the width of its columns.
 *
 * `face` takes the first span's own line height, which is what a text that
 * says nothing has always used. `multiple` scales that. `absolute` states
 * it outright in px. `grid` states a rhythm rather than a pitch: the
 * block's own height rounds UP to a multiple of it, so blocks set on the
 * same grid share one rhythm however differently their faces are cut.
 */
struct Leading {
  enum class Kind : uint8_t { kFace, kMultiple, kAbsolute, kGrid };
  Kind kind = Kind::kFace;
  float value = 0;  ///< the factor, the px pitch, or the px grid step

  /** The first span's own single-spaced line height. */
  [[nodiscard]] static Leading face() { return {}; }
  /** `factor` times the face's own line height. */
  [[nodiscard]] static Leading multiple(float factor) {
    return {Kind::kMultiple, factor};
  }
  /** Exactly `px`, whatever the face reports. */
  [[nodiscard]] static Leading absolute(float px) {
    return {Kind::kAbsolute, px};
  }
  /** The face's own height rounded up to a multiple of `px`. */
  [[nodiscard]] static Leading grid(float px) { return {Kind::kGrid, px}; }

  bool operator==(const Leading&) const = default;
};

/**
 * Where a block's lines start and end across the measure.
 *
 * `start` and `end` inset every line of the block from the two ends of
 * whatever interval the geometry offered — the near end being the one the
 * pen enters, so a line and a column read them the same way round.
 * `firstLine` and `lastLine` are added to `start` on the block's first and
 * last line only; a NEGATIVE `firstLine` is the hanging indent a bullet or
 * a number hangs into.
 *
 * An indent is arithmetic on the interval the geometry handed back, so it
 * composes with exclusions and columns without either knowing about it: a
 * line broken into three intervals by a shape is inset at its outermost
 * ends and nowhere in the middle.
 */
struct IndentOptions {
  float start = 0;      ///< px inset at the end the pen enters, every line
  float end = 0;        ///< px inset at the far end, every line
  float firstLine = 0;  ///< added to `start` on the block's first line
  float lastLine = 0;   ///< added to `start` on the block's last line
  bool operator==(const IndentOptions&) const = default;
};

/**
 * Which of a block's lines refuse to be parted from each other.
 *
 * Every one of these is a statement about a FRAME BOUNDARY — a widow
 * stands at the head of the next frame, an orphan at the foot of this one,
 * a kept-together pair straddles the join — so they are settled where the
 * boundary is: the fill runs, and lines the block may not leave behind are
 * taken back out of it and reported as overflow, which is how they reach
 * the next frame of the chain. No break is re-decided and nothing is
 * weighed against spacing, so BOTH BREAKERS obey these identically.
 *
 * A keep never empties a frame. A retraction that would leave the fill
 * with nothing is dropped: the text would arrive at the next frame in
 * exactly the state that emptied this one, and the chain would never
 * advance.
 *
 * `widowLines` is the one that asks about a frame this fill cannot see, so
 * it counts the carried lines at the measure THIS frame's last line was
 * set in. A chain of equal frames — the ordinary one — counts exactly; a
 * chain that changes width counts the carried lines at the wrong measure.
 */
struct KeepOptions {
  /// Fewest lines of the block that may stand alone at the START of a
  /// frame or column (a widow); 0 leaves the breaker free.
  int widowLines = 0;
  /// Fewest lines that may stand alone at the END of one (an orphan).
  int orphanLines = 0;
  /// The block ends where the next one begins: no frame or column boundary
  /// between them.
  bool withNext = false;
  /// Every line of the block lands in one frame or column.
  bool allLinesTogether = false;
  /// The block starts a new frame however much room is left in this one.
  bool startInNextFrame = false;
  bool operator==(const KeepOptions&) const = default;
};

/**
 * ONE BLOCK'S SETTING — the paragraph controls, as one comparable value.
 *
 * Everything above the overrides is the block's own and has no
 * layout-wide counterpart. The four optionals below are the layout-wide
 * settings of the same names: present, the block is set that way; absent,
 * the layout's own answer stands. That is what makes a text with no block
 * styles lay out exactly as one that never heard of them.
 *
 * SPACE BEFORE AND AFTER DO NOT COLLAPSE AND ARE NOT SUPPRESSED. The gap
 * between two blocks is the LARGER of the first's `spaceAfter` and the
 * second's `spaceBefore`, everywhere, including at the head of a frame.
 * One rule, no exceptions to hold in the head.
 */
struct ParagraphStyle {
  Leading leading;  ///< the block's pitch
  /// Where the room a leading opened goes: all of it ABOVE the line (the
  /// setting convention, and the default), or half above and half below,
  /// which sits the type optically centred in its own band.
  bool halfLeading = false;
  float spaceBefore = 0;  ///< px of air before the block
  float spaceAfter = 0;   ///< px of air after it
  /// Added to whatever the whole layout reserved (see ReservedBand).
  ReservedBand reserved;
  IndentOptions indent;
  KeepOptions keep;
  /// Set the block in the NARROWEST MEASURE THAT STILL TAKES THE SAME
  /// NUMBER OF LINES, which is what a ragged heading of three lines wants:
  /// the lines then have nowhere to be long, and that is an even rag. The
  /// optimizing breaker searches for that measure by bisection and breaks
  /// against it; placement still sets the lines in the measure the geometry
  /// gave, so a centred block stays centred on the real one. Ignored by the
  /// greedy breaker, which takes the first break that fits.
  bool balanceRaggedLines = false;
  /// The block's OPENING SET LARGE — sized so its reference metric spans
  /// the lines it is given, seated on the baseline it sinks to, with the
  /// lines under it wrapping the notch it cuts (InitialLetter.h). Zero
  /// lines, the default, declares none and costs nothing.
  InitialLetter initial;

  std::optional<TextAlignment> alignment;
  std::optional<JustificationOptions> justification;
  std::optional<HyphenationOptions> hyphenation;
  std::optional<TabStopOptions> tabStops;

  bool operator==(const ParagraphStyle&) const = default;
};

/**
 * Paragraph styles under names — the registry a document resolves
 * "heading" and "body" through.
 *
 * It answers every name: one it does not carry resolves to the base entry,
 * so a misspelling shows as a block set in the document's default rather
 * than as a block that did not lay out. `find` is the form that admits
 * absence. Order of registration is kept and compared, which is what lets
 * a set sit inside a larger comparable value and be diffed with it. Lookup
 * is a linear scan: a document names a handful of styles, and a scan of a
 * handful beats a hash of one.
 */
class ParagraphStyleSet {
 public:
  ParagraphStyleSet() = default;
  /** Starts a set whose unregistered names resolve to `base`. */
  explicit ParagraphStyleSet(ParagraphStyle base) : m_base(std::move(base)) {}

  /** Registers or replaces `name`. */
  ParagraphStyleSet& set(std::string name, ParagraphStyle style) {
    for (std::pair<std::string, ParagraphStyle>& entry : m_entries)
      if (entry.first == name) {
        entry.second = std::move(style);
        return *this;
      }
    m_entries.emplace_back(std::move(name), std::move(style));
    return *this;
  }
  /** The style registered under `name`, or the base entry. */
  [[nodiscard]] const ParagraphStyle& operator[](std::string_view name) const {
    const ParagraphStyle* found = find(name);
    return found ? *found : m_base;
  }
  /** The style registered under `name`, or null. */
  [[nodiscard]] const ParagraphStyle* find(std::string_view name) const {
    for (const std::pair<std::string, ParagraphStyle>& entry : m_entries)
      if (entry.first == name) return &entry.second;
    return nullptr;
  }
  /** The entries, in registration order. */
  [[nodiscard]] std::span<const std::pair<std::string, ParagraphStyle>>
  entries() const {
    return m_entries;
  }
  /** The style every unregistered name resolves to. */
  [[nodiscard]] const ParagraphStyle& base() const { return m_base; }

  bool operator==(const ParagraphStyleSet&) const = default;

 private:
  ParagraphStyle m_base;
  std::vector<std::pair<std::string, ParagraphStyle>> m_entries;
};

}  // namespace sigil::weave
