#pragma once

/** @file
 * Internals shared across the layout translation units: the interval
 * sequence and placement the greedy (LineBreak.cpp) and Knuth-Plass
 * (KnuthPlass.cpp) breakers both drive, and the glue arithmetic both
 * must agree on. Never leaves the layout directory.
 */

#include <include/core/SkTextBlob.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/Flow.h"
#include "sigilweave/layout/ParagraphLayout.h"
#include "sigilweave/paragraph/Paragraph.h"

namespace sigil::weave {
namespace detail {

struct FlatInterval {
  LineInterval interval;
  int sourceLineIndex = 0;
  /// Position in the flattened sequence. Carried on the interval rather
  /// than threaded through the placement calls, so the number a run
  /// reports and the number the breakers used are the same number.
  int index = 0;
};

// ONE BLOCK OF A TEXT: the words between two mandatory breaks, and the
// setting they are laid out under. The style's optionals are resolved into
// `options` once — a copy of the layout's own with this block's overrides
// applied — so the breakers, the glue arithmetic and the placement all read
// one flat value and none of them resolves an optional in a loop.
struct Block {
  // The block's place in the TEXT, which a frame resuming part-way through
  // a story still counts from the story's start.
  int index = 0;
  uint32_t firstWord = 0;
  uint32_t endWord = 0;
  ParagraphLayoutOptions options;
  ParagraphStyle style;
  float pitch = 0;     // band depth (a vertical flow's column width)
  float ascent = 0;    // baseline offset below the band's near edge
  float lead = 0;      // air before this block's first band
  float gridStep = 0;  // > 0: bands land on multiples of this
  // ROOM ADDED AFTER EACH WORD by the mojikumi table and by tsume, px,
  // indexed from the TEXT'S first word so a block reads its own range out
  // of one array. Empty when no table and no tsume were asked for, which
  // is what every layout that says nothing about full-width spacing gets.
  std::span<const float> mojikumiAfter;
};

// The room a mojikumi table and a tsume setting put after each word of the
// text, px, resolved once per layout into `room` (cleared first) and read
// by both breakers and by placement through the block's own span. Empty
// when neither was asked for.
void resolveMojikumi(const Paragraph& paragraph,
                     const ParagraphLayoutOptions& options,
                     std::vector<float>& room);

// Flattens a FlowGeometry's lines into one ordered sequence of intervals,
// fetched lazily. Both breakers consume geometry exclusively through this,
// so break decisions and placement always agree on interval numbering.
//
// It also carries THE BAND CURSOR — how far down (or across) the flow the
// next band's near edge sits. Bands used to stand at `index · pitch`, which
// only holds while one pitch serves the whole text; a text whose blocks
// lead differently, or which puts air between them, stacks at distances
// only the accumulating side knows. openBlock() is where a block hands over
// its pitch, its air and its indents, and everything after it is asked for
// under those until the next block opens.
class IntervalSequence {
 public:
  IntervalSequence(FlowGeometry& geometry, float lineHeight, float ascent,
                   float minimumWidth = 0)
      : m_geometry(geometry),
        m_pitch(lineHeight),
        m_ascent(ascent),
        m_minimumWidth(minimumWidth) {}

  /** Moves the cursor to where the flow's FIRST band begins — the
   * first-baseline rule, expressed as the one place a whole passage moves
   * from. Call before any interval is asked for. */
  void seatFirstBand(float bandStart) { m_bandCursor = bandStart; }

  /** Opens a block: from here on bands are `pitch` deep with their baseline
   * `ascent` below the near edge, the first of them starting `lead` past
   * where the last one ended, and every interval inset by `indent`. A
   * positive `gridStep` snaps each band's near edge up to a multiple of it,
   * which is what makes two blocks of different type share one rhythm. */
  void openBlock(int blockIndex, float pitch, float ascent, float lead,
                 float gridStep, const IndentOptions& indent) {
    m_blockIndex = blockIndex;
    m_lineInBlock = 0;
    m_bandCursor += lead;
    m_pitch = pitch;
    m_ascent = ascent;
    m_gridStep = gridStep;
    m_indent = indent;
    m_blocked = true;
  }

  /** Returns a flattened interval, fetching source lines on demand. */
  const FlatInterval* intervalAt(size_t intervalIndex) {
    while (intervalIndex >= m_flatIntervals.size() && !m_geometryExhausted)
      fetchLine();
    return intervalIndex < m_flatIntervals.size()
               ? &m_flatIntervals[intervalIndex]
               : nullptr;
  }

  /** Returns whether every source line has the same single measure. A
   * geometry that says so still varies once blocks indent differently or
   * lead differently, so the answer is the geometry's AND this layout's. */
  bool uniform() const { return m_geometry.uniformIntervals() && m_uniform; }
  /** States that this layout's blocks do not vary the measure. */
  void setUniformBlocks(bool uniform) { m_uniform = uniform; }

  /** Returns every interval fetched so far, in index order. */
  const std::vector<FlatInterval>& flattened() const { return m_flatIntervals; }

  /** Returns the first index past every interval of `index`'s source line,
   * walking only what is already fetched — the position a new block starts
   * at, since a block never shares a band with the one before it. */
  size_t pastSourceLine(size_t index) const {
    if (index >= m_flatIntervals.size()) return index + 1;
    const int line = m_flatIntervals[index].sourceLineIndex;
    size_t next = index + 1;
    while (next < m_flatIntervals.size() &&
           m_flatIntervals[next].sourceLineIndex == line)
      ++next;
    return next;
  }

  /** How far the bands have reached along the stacking axis — the depth the
   * placed text occupies, which is what a frame distributes the rest of. */
  float bandCursor() const { return m_bandCursor; }

 private:
  /** Insets one source line's intervals: the near indent moves where the
   * pen enters the FIRST of them, the far indent shortens the LAST. A line
   * an exclusion cut into three is inset at its outermost ends and nowhere
   * in the middle, which is what a reader sees as an indent. */
  static void applyIndent(std::vector<LineInterval>& intervals, float near,
                          float far) {
    if (intervals.empty() || (near == 0 && far == 0)) return;
    LineInterval& first = intervals.front();
    if (near != 0 && !first.contour.valid()) {
      first.origin += SkVector{first.direction.x() * near,
                               first.direction.y() * near};
      first.length = std::max(0.0f, first.length - near);
    }
    LineInterval& last = intervals.back();
    if (far != 0 && !last.contour.valid())
      last.length = std::max(0.0f, last.length - far);
  }

  /** Fetches and flattens the next source line into the interval cache. */
  void fetchLine() {
    m_sourceLineIntervals.clear();
    float bandStart = m_bandCursor;
    if (m_gridStep > 0) {
      constexpr float kOnGrid = 1e-3f;  // a band already on the grid stays
      bandStart =
          std::ceil((bandStart - kOnGrid) / m_gridStep) * m_gridStep;
    }
    const LineRequest request{m_nextLineIndex, bandStart, m_pitch, m_ascent,
                              m_blockIndex,    m_lineInBlock};
    if (!m_geometry.lineIntervals(request, m_sourceLineIntervals)) {
      m_geometryExhausted = true;
      return;
    }
    if (m_blocked)
      applyIndent(m_sourceLineIntervals,
                  m_indent.start +
                      (m_lineInBlock == 0 ? m_indent.firstLine : 0.0f),
                  m_indent.end);
    for (const LineInterval& interval : m_sourceLineIntervals)
      if (interval.length >= m_minimumWidth)
        m_flatIntervals.push_back({interval, m_nextLineIndex,
                                   static_cast<int>(m_flatIntervals.size())});
    m_bandCursor = bandStart + m_pitch;
    m_nextLineIndex++;
    m_lineInBlock++;
  }

  FlowGeometry& m_geometry;
  float m_pitch;
  float m_ascent;
  float m_minimumWidth;
  float m_bandCursor = 0;
  float m_gridStep = 0;
  int m_blockIndex = 0;
  int m_lineInBlock = 0;
  IndentOptions m_indent;
  bool m_blocked = false;
  bool m_uniform = true;
  std::vector<FlatInterval> m_flatIntervals;
  std::vector<LineInterval> m_sourceLineIntervals;
  int m_nextLineIndex = 0;
  bool m_geometryExhausted = false;
};

// ── The break store ────────────────────────────────────────────────────
//
// WHERE A BLOCK'S LINES ENDED, KEPT SO THE NEXT FRAME NEED NOT DECIDE IT
// AGAIN. Break decisions are the expensive half of laying a paragraph out
// and they are a function of four things: which words, in what setting, at
// what measure, from which word. A moving text changes one of those at a
// time — a measure that animates, a string that churns — so the answers to
// the ones it did not change are still the answers.
//
// It is consulted only under ParagraphLayoutOptions::live and only for a
// block set in a UNIFORM measure, because those are the two conditions
// under which "the measure" is the whole of the geometry a break decision
// reads. A settled layout, or one over a contour or an exclusion, decides
// every break itself exactly as it always has.
//
// THE BOUND IS STATED AND SMALL: one store per thread, holding the most
// recently answered blocks up to kBreakStoreEntries, each one a break list
// as long as the block has lines. A width animating across a range of
// pixels therefore keeps the pixels it has most recently crossed and
// forgets the rest, and a story of many blocks keeps the blocks it is
// still setting.
struct BreakKey {
  uint64_t paragraph = 0;  // which paragraph, told apart for its lifetime
  uint64_t words = 0;      // what the paragraph's word list is
  uint32_t firstWord = 0;  // where this block's fill began
  uint32_t endWord = 0;
  int32_t measure = 0;   // the uniform measure, whole pixels below it
  uint64_t setting = 0;  // everything about the setting a break reads
  bool operator==(const BreakKey&) const = default;
};

// One line's end: the word it ends at, and the interval the line occupied.
using BreakList = std::vector<std::pair<uint32_t, uint32_t>>;

inline constexpr size_t kBreakStoreEntries = 32;

class BreakStore {
 public:
  /** The break list answered for `key`, or null. */
  const BreakList* find(const BreakKey& key) {
    for (size_t index = 0; index < m_entries.size(); ++index)
      if (m_entries[index].first == key) {
        promote(index);
        return &m_entries.front().second;
      }
    return nullptr;
  }
  /** Records `breaks` under `key`, evicting the least recently answered. */
  void store(const BreakKey& key, const BreakList& breaks) {
    for (size_t index = 0; index < m_entries.size(); ++index)
      if (m_entries[index].first == key) {
        m_entries[index].second = breaks;
        promote(index);
        return;
      }
    if (m_entries.size() >= kBreakStoreEntries) m_entries.pop_back();
    m_entries.insert(m_entries.begin(), {key, breaks});
  }

 private:
  void promote(size_t index) {
    if (index == 0) return;
    std::rotate(m_entries.begin(), m_entries.begin() + (long)index,
                m_entries.begin() + (long)index + 1);
  }
  std::vector<std::pair<BreakKey, BreakList>> m_entries;
};

/** The store this thread answers from. */
BreakStore& breakStore();

/** Everything about a block's setting that a BREAK DECISION reads, folded
 *  into one number. Two blocks whose settings differ in any of it are two
 *  different questions; two whose settings agree in all of it have one
 *  answer between them. Placement settings are deliberately absent: where
 *  a line ends does not depend on how it is then seated in its interval. */
uint64_t breakSetting(const Block& block);

/** The whole pixel below a measure — the width break decisions are kept
 *  under, so a width that animates decides them once per pixel it crosses
 *  and a line is never broken for a measure wider than it is set in. */
int32_t quantisedMeasure(float measure);

/** Places one line per entry of `breaks`, in the interval each names. */
void placeBreaks(FontContext& fontContext, Paragraph& paragraph,
                 IntervalSequence& intervalSequence, const Block& block,
                 const BreakList& breaks, ParagraphLayout& result,
                 size_t& lastIntervalUsed, uint32_t& overflowWord);

// Natural (unjustified) width of a half-open word range on one line: content
// widths plus inter-word glue, the last word's trailing space excluded.
float naturalWidth(const std::vector<Word>& words, uint32_t firstWordIndex,
                   uint32_t endWordIndex);

// Whether tab stops are configured at all (ParagraphLayoutOptions::tabStops).
// Defined in the header (as is glueAfter below) rather than in one of the
// breaker translation units: both are called from the innermost loops of
// both breakers, and both must stay inlinable there.
inline bool tabStopsActive(const ParagraphLayoutOptions& options) {
  return !options.tabStops.stops.empty() || options.tabStops.interval > 0;
}

// The glue width after one word with the pen at `penPosition` (relative to
// the line interval's start): the distance to the next tab stop for tab
// gaps, the measured whitespace otherwise. Both breakers and placement
// resolve stops through this one function so they always agree on widths.
// The stop a tab at `penPosition` advances to. `stop` is null for a stop
// the repeating interval synthesized, because a repeat states a position
// and nothing else — no alignment, no leader. Answers by pointer rather
// than by value: a TabStop carries its leader string, and both breakers
// resolve stops in their innermost loops.
struct ResolvedTabStop {
  float position = 0;
  const TabStop* stop = nullptr;
};

inline bool tabStopAhead(float penPosition,
                         const ParagraphLayoutOptions& options,
                         ResolvedTabStop& resolved) {
  constexpr float kMinTabAdvance = 0.5f;  // a stop the pen already reached
                                          // is not "the next" stop
  for (const TabStop& stop : options.tabStops.stops)
    if (stop.position >= penPosition + kMinTabAdvance) {
      resolved = {stop.position, &stop};
      return true;
    }
  if (options.tabStops.interval > 0) {
    const float base = options.tabStops.stops.empty()
                           ? 0.0f
                           : options.tabStops.stops.back().position;
    const float distance = std::max(penPosition - base, 0.0f);
    const float repeats =
        std::floor(distance / options.tabStops.interval) + 1.0f;
    float position = base + repeats * options.tabStops.interval;
    if (position < penPosition + kMinTabAdvance)
      position += options.tabStops.interval;
    resolved = {position, nullptr};
    return true;
  }
  return false;
}

// The room after one word from the mojikumi table and tsume, or zero.
inline float mojikumiAfter(const Block& block, uint32_t wordIndex) {
  return wordIndex < block.mojikumiAfter.size()
             ? block.mojikumiAfter[wordIndex]
             : 0.0f;
}

inline float glueAfter(const Word& word, float penPosition,
                       const ParagraphLayoutOptions& options) {
  if (!word.tabAfter || !tabStopsActive(options)) return word.spaceWidth;
  ResolvedTabStop resolved;
  if (!tabStopAhead(penPosition, options, resolved))
    return word.spaceWidth;  // stops exhausted: tab degrades to a space
  // The distance to the stop, which is where the text after the tab STARTS.
  // A stop that aligns its cell some other way pulls the text back from
  // there, and the breakers take the start rule as the width they fit
  // against: every other alignment renders the same cell NEARER the stop,
  // so a line that fits under this one fits under those.
  return resolved.position - penPosition;
}

// Places a half-open word range into `interval` with the given alignment,
// appending PositionedRuns to `out`. Pure arithmetic over cached ShapedWords:
// straight horizontal intervals reuse each word's shared blob; rotated and
// contour intervals bake per-glyph RSXforms. When `hyphenBreakTaken`, the
// line ends at a soft hyphen and the word's hyphen glyph is rendered.
void placeWords(FontContext& fontContext, const Paragraph& paragraph,
                uint32_t firstWordIndex, uint32_t endWordIndex,
                const FlatInterval& interval, TextAlignment alignment,
                bool lastLine, bool hyphenBreakTaken,
                const ParagraphLayoutOptions& options, ParagraphLayout& out,
                std::span<const float> mojikumiAfter = {});

// Whether a non-final break before `endWordIndex` lands on a soft hyphen.
inline bool hyphenTakenAt(const std::vector<Word>& words, uint32_t endWordIndex,
                          bool lineIsFinal,
                          const ParagraphLayoutOptions& options) {
  return options.hyphenation.enabled && !lineIsFinal && endWordIndex > 0 &&
         endWordIndex <= words.size() && words[endWordIndex - 1].hyphenBreak &&
         words[endWordIndex - 1].hyphenGlyph &&
         !words[endWordIndex - 1].mandatoryBreakAfter;
}

// Knuth-Plass entry, one BLOCK at a time: breaks + places every word of it
// that fits, appending to `result`. Takes the paragraph (not just its words)
// so it can pull lazy shaping along its own frontier. Defined in
// KnuthPlass.cpp.
void knuthPlassBlock(FontContext& fontContext, Paragraph& paragraph,
                     IntervalSequence& intervalSequence, const Block& block,
                     size_t firstInterval, ParagraphLayout& result,
                     size_t& lastIntervalUsed, uint32_t& overflowWord,
                     bool& outOfBudget);

}  // namespace detail
}  // namespace sigil::weave
