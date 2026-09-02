#pragma once

/** @file
 * @ingroup layout
 *
 * What a caller tells the layout stage, grouped by the stage that reads it:
 * alignment and the choice of breaker, the line metrics that override the
 * font's, hyphenation, justification elasticity, the Knuth-Plass
 * tolerances, the overflow ellipsis and line clamp, tab stops, how a frame
 * seats what it holds, and the tangent snapping text on a path draws with.
 *
 * The paragraph controls sit here too, because a BLOCK — the text between
 * two mandatory breaks, the paragraph a reader sees — is styled by telling
 * the layout stage about it rather than by carrying anything on the text:
 * ParagraphStyle is one block's setting, ParagraphStyleSet names them, and
 * ParagraphLayoutOptions::blocks lists them in block order. Every field is
 * defaulted and every nested group is inert unless its stage runs. Settings
 * that belong to the geometry stay on the geometry
 * (ExclusionFlow::setMinIntervalWidth, for instance).
 */

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sigilweave/paragraph/Hyphenation.h"

namespace sigil::weave {

/** Specifies how text is aligned inside each available line interval. */
enum class TextAlignment : uint8_t { kStart, kCenter, kEnd, kJustify };

/** Selects the fast greedy breaker or optimal Knuth-Plass line breaking. */
enum class LineBreakStrategy : uint8_t { kGreedy, kKnuthPlass };

/** Overrides the paragraph's font-derived line metrics when non-zero. */
struct LineMetricsOptions {
  float height = 0;  ///< line height, px; 0 keeps the font-derived value
  float ascent = 0;  ///< baseline offset below the line top, px; 0 keeps
                     ///< the font-derived value
  bool operator==(const LineMetricsOptions&) const = default;
};

/** Where a word may be broken, and which of those breaks a line may take.
 *
 * The two halves are decided at different stages and that is the whole of
 * the split. Where a break MAY fall is segmentation: a soft hyphen already
 * in the text, plus whatever `patterns` finds inside a word under
 * `limits`, and all of that is a fact about the text that the whole layout
 * shares. Which of those opportunities a line actually TAKES is a break
 * decision — the three fields under `limits` — so a block may state its
 * own and the breaker reads the block's.
 */
struct HyphenationOptions {
  /// False removes the break opportunity, not just the hyphen glyph: the
  /// halves either side of a soft hyphen fuse into one unbreakable word
  /// during segmentation, so the word wraps or overflows whole, and
  /// `patterns` is not consulted at all. Reaching the paragraph is what
  /// makes that happen — see Paragraph::setSoftHyphenBreaks, which
  /// layoutParagraph sets from here.
  bool enabled = true;
  /// Added as squared demerits by Knuth-Plass to discourage repeated
  /// discretionary hyphen breaks.
  float penalty = 50.0f;

  /// Where inside a word a break may fall, beyond the soft hyphens the
  /// author typed. Null leaves discretionary hyphens the only opportunity,
  /// which is what a text that says nothing gets. The kit ships Liang
  /// pattern sets (kit/Hyphenation.h); a caller's own implementation is a
  /// peer of them. Compared by identity, because two hyphenators that are
  /// not the same object cannot be shown to answer the same way.
  const Hyphenator* patterns = nullptr;

  /// Which of a word's break points become opportunities at all — a fact
  /// about the word, so it is settled during segmentation and the whole
  /// layout shares it (paragraph/Hyphenation.h).
  HyphenationLimits limits;

  /// Most lines in a row that may end in a hyphen; 0 lifts the limit.
  int consecutiveLimit = 0;
  /// A ragged line whose last word ends within this distance of the measure
  /// is left ragged rather than broken, px. 0 lifts the limit.
  float zone = 0;
  /// Whether the last word of a block may be broken.
  bool lastWordOfBlock = true;

  bool operator==(const HyphenationOptions&) const = default;
};

/** Controls spacing when TextAlignment::kJustify is selected.
 *
 * A justified line is fitted in three passes, each spending only what the
 * one before it could not: the WORD GAPS move first, from their desired
 * width towards the near limit; then LETTER SPACING is added between the
 * glyphs; then the glyphs themselves are SCALED across. Shrinking runs the
 * same order. A pass whose limits equal its desired value contributes
 * nothing and costs nothing — which is why a caller who sets none of them
 * gets word spacing alone, as this stage has always done.
 */
struct JustificationOptions {
  /// Paragraph-final and hard-break-final lines use this alignment unless
  /// `justifyLastLine` requests full justification.
  TextAlignment lastLineAlignment = TextAlignment::kStart;
  bool justifyLastLine = false;  ///< stretch final lines to full measure too

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f;  ///< per-gap cap, fraction of fontSize

  /// The width a justified word gap is AIMED at, as a multiple of the
  /// shaped space width; the elasticity below is measured from it, so the
  /// gap may run from `wordSpacing · (1 - spaceShrink)` to
  /// `wordSpacing · (1 + spaceStretch)`.
  float wordSpacing = 1.0f;
  float spaceStretch = 0.5f;   ///< maximum stretch, as a fraction
  float spaceShrink = 0.333f;  ///< maximum shrink, as a fraction

  /// Letter spacing the second pass may add or remove, as fractions of the
  /// em. `letterSpacing` is applied to every justified line whatever its
  /// fit; the two limits bound what the pass may add on top. All three zero
  /// leaves the pass out.
  float letterSpacing = 0;
  float letterSpacingMinimum = 0;
  float letterSpacingMaximum = 0;

  /// Horizontal glyph scale the third pass may reach for. `glyphScale` is
  /// applied to every justified line; the limits bound the pass. All three
  /// at 1 leaves it out. Scaling letters is the last thing a page should
  /// do and the defaults never do it.
  float glyphScale = 1.0f;
  float glyphScaleMinimum = 1.0f;
  float glyphScaleMaximum = 1.0f;

  /// A line holding ONE word has no gaps to spend: `kAlign` leaves it at
  /// the block's alignment, `kJustify` stretches it across the measure with
  /// letter spacing alone.
  enum class SingleWord : uint8_t { kAlign, kJustify };
  SingleWord singleWord = SingleWord::kAlign;

  bool operator==(const JustificationOptions&) const = default;
};

/** Advanced tuning used only by LineBreakStrategy::kKnuthPlass. */
struct KnuthPlassOptions {
  /// Maximum TeX-style badness before the breaker uses its forced-fit path.
  float tolerance = 4000.0f;
  /// Intervals narrower than this are ignored so the algorithm never has to
  /// force a word into exclusion-shape slivers.
  float minimumIntervalWidth = 0.0f;
  bool operator==(const KnuthPlassOptions&) const = default;
};

/** Controls how overflowing text is represented. */
struct OverflowOptions {
  /// Empty disables the marker. Straight flows render it — at a line's end
  /// or at a column's foot, set the way the text it cut was set — while a
  /// contour flow reports its overflow without one, having no end to put a
  /// marker at.
  std::u16string ellipsis;
  /// > 0: the layout uses at most this many of the geometry's lines
  /// (CSS line-clamp — COLUMNS in a vertical flow); remaining text reports
  /// as overflow and `ellipsis` (when set) lands on the clamped line.
  /// Works with every breaker and geometry — the limit wraps the
  /// FlowGeometry, so exclusion flows and Knuth-Plass need no special
  /// handling.
  int maxLines = 0;
  bool operator==(const OverflowOptions&) const = default;
};

/** One tab stop: where the pen goes, what it aligns there, and what fills
 * the gap behind it.
 *
 * `kStart` puts the text after the stop, `kEnd` ends it there, `kCenter`
 * straddles it, and `kCharacter` lines the FIRST `alignOn` in the following
 * text up on it — which is the decimal column a table of figures wants, and
 * falls back to `kEnd` for a cell that holds no such character.
 *
 * `leader` is set repeatedly across the gap the stop opened, clipped to it,
 * in the style of the text ahead of the tab: a run of dots between a
 * heading and its page number is one string here rather than typed content.
 */
struct TabStop {
  enum class Align : uint8_t { kStart, kCenter, kEnd, kCharacter };
  float position = 0;                 ///< px from the interval's start
  Align align = Align::kStart;        ///< what the stop pins there
  char16_t alignOn = u'.';            ///< kCharacter: the character pinned
  std::u16string leader;              ///< repeated across the gap; may be empty
  bool operator==(const TabStop&) const = default;
};

/** Tab-character handling for straight horizontal flows.
 *
 * A word whose trailing whitespace contains a tab advances the pen to the
 * next stop instead of its measured glue: first through `stops` (ascending,
 * px from each line interval's start), then repeating every `interval` px
 * past the last explicit stop. With no stop ahead (or no configuration at
 * all — the default) tabs keep their shaped space-equivalent width.
 *
 * Both breakers resolve stops identically: greedy fits against tab-resolved
 * widths as it goes, and Knuth-Plass scores every candidate line at its
 * tab-resolved width. Stops are line-local — alignment other than kStart
 * shifts the resolved line as a whole. Tab gaps are rigid under
 * justification, and gaps at or before a line's last tab never stretch or
 * shrink (the following stop would swallow the adjustment and unpin the
 * column); only the gaps past the last tab absorb slack.
 * Scope: straight horizontal intervals, LTR lines.
 */
struct TabStopOptions {
  std::vector<TabStop> stops;  ///< explicit stops, ascending
  float interval = 0;          ///< repeat spacing past the last explicit
                               ///< stop; 0 disables repetition
  bool operator==(const TabStopOptions&) const = default;
};

/** Rendering-only controls that do not affect line breaking. */
struct PathTextOptions {
  /// Animated path tangents snap to this many directions to avoid creating
  /// a fresh glyph-atlas strike for every tiny rotation change. Zero
  /// preserves exact rotations for static artwork.
  int tangentRotationSteps = 512;
  bool operator==(const PathTextOptions&) const = default;
};

/**
 * How a frame seats the lines it holds — the two decisions a frame makes
 * that no line makes for itself.
 *
 * WHERE THE FIRST BASELINE SITS is otherwise the first line's own ascent,
 * so two frames of different type start their text at different heights;
 * naming a cap height, an x-height or a fixed offset instead pins the first
 * line to something the page can be ruled against.
 *
 * WHAT BECOMES OF THE ROOM LEFT OVER is otherwise nothing: the lines stack
 * from the top and the remainder is air underneath. Centring or seating the
 * text against the far edge translates the whole block; justifying it
 * spreads the remainder BETWEEN the lines, as extra leading, which is what
 * a column of a magazine does to reach its foot.
 *
 * Both need to know how deep the frame is, which a geometry knows and the
 * layout does not, so `extent` states it: 0 leaves both decisions alone.
 * Neither applies to a flow whose intervals ride a contour — a loop has no
 * near edge to measure from.
 */
struct FrameOptions {
  enum class FirstBaseline : uint8_t {
    kAscent,     ///< the first line's own ascent
    kCapHeight,  ///< the first line's cap height
    kXHeight,    ///< the first line's x-height
    kLeading,    ///< the first line's whole pitch
    kFixed,      ///< exactly `firstBaselineOffset`
  };
  enum class Distribute : uint8_t {
    kStart,    ///< the leftover room stays past the last line
    kCenter,   ///< half before the first line, half past the last
    kEnd,      ///< all of it before the first line
    kJustify,  ///< spread between the lines as extra leading
  };

  FirstBaseline firstBaseline = FirstBaseline::kAscent;
  /// kFixed states the offset outright; every other mode adds this on top
  /// of what it measured.
  float firstBaselineOffset = 0;
  Distribute distribute = Distribute::kStart;
  /// kJustify: the most any one gap may grow, px. 0 lifts the limit.
  float maximumInterlineSpacing = 0;
  /// How deep the frame is along the axis its lines stack on, px. 0 means
  /// the caller did not say, and both decisions above are left alone.
  float extent = 0;

  bool operator==(const FrameOptions&) const = default;
};

/**
 * SPACE RESERVED BESIDE EVERY LINE, over and above the leading — the band
 * something set alongside the type occupies: a reading over a base, a row
 * of emphasis dots, a note in the gutter.
 *
 * It is a LAYOUT INPUT and that is the whole point of it. The band is
 * stated before the text is laid out, from the annotation's own metrics,
 * never from where the base's glyphs turned out to land — so the base is
 * broken and placed once, with the room already in its strut, and the
 * annotation is then placed on the result. Nothing chases anything.
 *
 * `before` is above a line and to the RIGHT of a column, `after` below a
 * line and to the LEFT of one: the sides each writing mode reads its
 * furniture on. Both open the pitch; `before` also moves the baseline down
 * inside the band, so the type stays where the reader expects it and the
 * room appears where the reading goes.
 */
struct ReservedBand {
  float before = 0;
  float after = 0;
  bool operator==(const ReservedBand&) const = default;
};

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
  /// Spend the optimizing breaker's slack on lines of even LENGTH rather
  /// than even spacing, which is what a ragged heading of three lines
  /// wants. Ignored by the greedy breaker.
  bool balanceRaggedLines = false;

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

/**
 * Groups the settings of paragraph layout by the stage that reads them.
 *
 * Every member is defaulted, and the common path sets only `alignment`.
 * Each nested group is inert unless its stage runs: `justification`
 * applies under kJustify, `knuthPlass` under kKnuthPlass, `tabStops` only
 * when a word carries a tab, `pathText` only when runs are transformed.
 *
 * The top-level `alignment`, `justification`, `hyphenation` and `tabStops`
 * are the WHOLE LAYOUT'S answer, and a block that states none of its own
 * is set by them. `blocks` overrides them block by block.
 */
struct ParagraphLayoutOptions {
  TextAlignment alignment = TextAlignment::kStart;  ///< per-interval placement
  /// Greedy is the fast default; Knuth-Plass trades speed for even spacing.
  LineBreakStrategy lineBreakStrategy = LineBreakStrategy::kGreedy;
  LineMetricsOptions lineMetrics;  ///< non-zero fields override font metrics
  HyphenationOptions hyphenation;  ///< where words may break, and which take
  JustificationOptions justification;  ///< only used under kJustify
  KnuthPlassOptions knuthPlass;        ///< only used under kKnuthPlass
  OverflowOptions overflow;            ///< ellipsis marker and line clamping
  TabStopOptions tabStops;   ///< empty/zero → tabs measure as shaped spaces
  PathTextOptions pathText;  ///< draw-time only, never affects breaking
  FrameOptions frame;        ///< first baseline and vertical distribution
  /// Room beside every line for what is set alongside the type; a block
  /// may reserve more.
  ReservedBand reserved;

  /// Which characters may not stand at a line's edge (kinsoku shori). A
  /// prohibition is settled during SEGMENTATION — the boundary is simply
  /// not opened — so no breaker knows the rule and both of them obey it.
  KinsokuTable kinsoku;
  /// How far a character may hang past the measure (optical margin
  /// alignment; burasagari down a column). Empty leaves every line squared
  /// on its advances, which is what a text that says nothing gets.
  HangingTable hanging;

  /// One entry per BLOCK — the text between two mandatory breaks — in
  /// block order. A block past the end of this list, and every block when
  /// it is empty, is set by the fields above alone.
  std::vector<ParagraphStyle> blocks;

  bool operator==(const ParagraphLayoutOptions&) const = default;
};

}  // namespace sigil::weave
