#pragma once

/** @file
 * SigilCompose feeds — a streaming collection: rows arrive at the tail,
 * the oldest fall off the head, and a window of the newest is on screen.
 * A log, a chat transcript, a tape of readings, a subtitle track. It is
 * built entirely from kernel composition: a ring of values, windowed to
 * the last N, laid out as a clipped column. There is no new machinery
 * here, only elements.
 *
 * The load-bearing rule: rows are keyed by a MONOTONIC SEQUENCE ID, never
 * by array index. An append therefore shifts nothing — every surviving row
 * keeps its instance and its cached picture, and reconciliation costs one
 * mount for the new tail plus one unmount for the scrolled-out head. Index
 * keys would renumber every row on every append, so each would compare
 * unequal, re-patch, and drop its cache; the cost would grow with the
 * window instead of staying constant.
 *
 * Volatility follows from that: scrolled-back rows are static and cache,
 * and the only live values are the ones the row factory binds. To fade out
 * old rows, put a gradient mask over the WHOLE feed (a Material drawn with
 * kDstIn, or a ground-coloured gradient laid over the top edge) rather than
 * animating per-row opacity, which would make every faded row volatile.
 *
 * What a row IS belongs to the caller: `feed()` takes a factory from the
 * row value to an Element, and whatever that factory declares — a mount
 * transition, an `fx()` track that settles — is the row's entrance, delayed
 * per row by `Options::entrance`.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcore/reconcile/Env.h>

#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::feed {

/** One row as the ring holds it: the caller's value under the sequence id
 *  that will key it for as long as it is on screen. */
template <class T>
struct Row {
  uint64_t seq = 0;
  T value{};

  bool operator==(const Row&) const = default;
};

/** Append-only retention with monotonic sequence ids: the newest
 *  `capacity` rows, each remembering when it arrived.
 *
 *  Sequence ids start at 1 and never repeat, including across `clear()` —
 *  a cleared feed that fills again mounts fresh rows rather than matching
 *  the ones it just dropped. */
template <class T>
  requires std::copyable<T> && std::equality_comparable<T>
class Ring {
 public:
  explicit Ring(size_t capacity = 512) : m_capacity(capacity) {}

  uint64_t append(T value) {
    const uint64_t seq = m_next++;
    m_rows.push_back({seq, std::move(value)});
    if (m_rows.size() > m_capacity) m_rows.pop_front();
    return seq;
  }
  void clear() { m_rows.clear(); }

  /** Oldest first. */
  const std::deque<Row<T>>& rows() const { return m_rows; }
  size_t size() const { return m_rows.size(); }
  bool empty() const { return m_rows.empty(); }
  size_t capacity() const { return m_capacity; }
  /** The id the next append will take. */
  uint64_t nextSeq() const { return m_next; }

 private:
  size_t m_capacity;
  uint64_t m_next = 1;
  std::deque<Row<T>> m_rows;
};

/** The key one row carries — `feed()` writes it, and a caller who builds
 *  the column by hand has to write the same one for the rows to match
 *  across describes. It overrides whatever key the row factory set: the
 *  identity of a row is its place in the sequence, not anything about its
 *  content. */
inline std::string rowKey(uint64_t seq) { return "row#" + std::to_string(seq); }

/** How a feed lays its rows out. Comparable, so it can BE a value a
 *  component compares or inherits. */
struct Options {
  /** THE WINDOW. Only the newest `visible` rows are built at all, so a ring
   *  of thousands costs a column of this many nodes and the rest are not
   *  mounted, laid out or painted. */
  size_t visible = 24;
  /** Between rows, along the column. */
  float gap = 2.0f;
  /** The entrance cascade for rows that mount, in the schedule vocabulary
   *  the glyph engine and `staggerChildren()` already speak. `eachMs` is
   *  the delay step and `from` is where the cascade starts; the fields
   *  that describe a per-unit remap inside one element (`durationMs`,
   *  `amountMs`, `distribution`, `inner`) belong to `fx()` tracks and a
   *  feed does not read them — a ROW is the beat here.
   *
   *  It delays only rows that actually mount, which is what makes it usable
   *  on a live feed: the first describe cascades the whole window, and each
   *  later append is the only new mount in its patch, so it enters at once
   *  instead of waiting out a full window's worth of steps. Rows already on
   *  screen never re-enter — an append does not re-cascade them.
   *
   *  Zero (the default) mounts every row immediately. */
  motion::Spread entrance{.eachMs = 0, .durationMs = 0};

  bool operator==(const Options&) const = default;
};

/** THE FEED: the ring's newest `visible` rows, each built by @p row and
 *  keyed by its sequence id, in a clipped column.
 *
 *  Re-describe on every append — reconciliation prices it at one mount (the
 *  tail), one unmount (the scrolled-out head), zero patches on the rows in
 *  between.
 *
 *  The returned column is an ordinary Element: give it a size, a fill, a
 *  `grow(1)`, or append something after the rows (a caret, a "…more"
 *  affordance) with `.child()`. */
template <class T, class RowFn>
  requires std::invocable<RowFn, const T&>
[[nodiscard]] Element feed(const Ring<T>& ring, const Options& options,
                           RowFn&& row) {
  Element column = box().column().gap(options.gap).clip();
  if (options.entrance.eachMs > 0)
    column.staggerChildren(
        std::chrono::milliseconds(std::lroundf(options.entrance.eachMs)),
        options.entrance.from);
  const std::deque<Row<T>>& rows = ring.rows();
  const size_t n = rows.size();
  const size_t first = n > options.visible ? n - options.visible : 0;
  for (size_t i = first; i < n; ++i) {
    Element built = row(rows[i].value);
    built.key(rowKey(rows[i].seq));
    column.child(std::move(built));
  }
  return column;
}

/** How tall a feed of @p rows rows is at these options — the number to size
 *  a panel against instead of hand-tuning one:
 *
 *      const float well = feed::height(logOptions(), fonts);
 *      const float h = 2 * padY + 3 * well + 4 * gap + 2 * dividerWidth;
 *
 *  **It measures, it does not compute.** A probe ring of @p rows rows goes
 *  through the real `feed()` and the real `compose::measure()`, so the
 *  answer includes `Options::gap` between the rows and whatever SigilWeave's
 *  line metrics and Yoga's pixel grid do to the row element. Doing the
 *  arithmetic instead — a line height times a row count plus the gaps —
 *  under-reports by close to a whole row on a long feed, because each row is
 *  rounded up to the pixel grid before the gaps are added and the arithmetic
 *  rounds nothing.
 *
 *  Three properties worth knowing:
 *
 *  - **It clamps to the window.** The probe runs through `feed()`, which
 *    builds only the newest `Options::visible` rows, so asking for 400 rows
 *    on a 12-row window returns the 12-row height. A feed is windowed and
 *    its height is bounded by its options.
 *  - **Every probe row is @p probeRow.** A feed whose rows differ in HEIGHT
 *    — a style at another size, an entrance that changes the box — gets a
 *    lower bound from a probe built at the ordinary row.
 *  - **A row that WRAPS is taller.** The probe measures unconstrained, so
 *    each row is one line box. A real row long enough to wrap at the feed's
 *    final width takes two, which is what the column's clip is for.
 *
 *  The `box()` shell is the `snapshot()`/`measure()` sizing rule: both size
 *  by the root's CHILDREN and ignore the root's own dimensions. It changes
 *  nothing today — `feed()` returns a column that sets neither a width nor a
 *  height — and it keeps the measurement honest if that ever changes. */
template <class RowFn>
  requires std::invocable<RowFn>
[[nodiscard]] float height(const Options& options, size_t rows,
                           RowFn&& probeRow, sigil::weave::FontContext& fonts) {
  Ring<uint64_t> probe(rows > 0 ? rows : 1);
  for (size_t i = 0; i < rows; ++i) probe.append(i);
  Element column =
      feed(probe, options, [&](const uint64_t&) { return probeRow(); });
  return compose::measure(box().child(std::move(column)), fonts).height();
}

// ---------------------------------------------------------------------------
// Rows of text — the shape a log, a transcript or a tape takes.

/** A text row: the line, and the NAME of the style it is set in. The name
 *  is resolved against a `weave::StyleSet` at build time, so an unregistered
 *  name — including the default empty one — takes the set's base style. */
struct TextRow {
  std::u8string text;
  std::string style;

  bool operator==(const TextRow&) const = default;
};

using TextRing = Ring<TextRow>;

/** A text feed's whole appearance: the layout and the styles its rows name.
 *
 *  Comparable, so it can BE an inherited value: `env::` bindings are keyed
 *  by C++ type, and the key this component uses is its own props type —
 *  which is how a feed gets themed from above without the library shipping
 *  a palette layer (see `feed(const TextRing&)`). Exact and structural,
 *  like every other value the reconciler compares. */
struct TextOptions {
  Options window;
  /** Row style by name. The base entry sets every row that names nothing. */
  sigil::weave::StyleSet styles;

  bool operator==(const TextOptions&) const = default;
};

/** ONE text row as an element — the exact thing `feed(const TextRing&, …)`
 *  builds for each row, minus the key `feed()` puts on it.
 *
 *  Exposed so a caller can build the column by hand when the rows need
 *  something the options do not carry, keying each row with `rowKey()`. */
[[nodiscard]] inline Element textRow(const TextRow& row,
                                     const sigil::weave::StyleSet& styles) {
  return text(row.text, styles[row.style]);
}

/** The text feed: rows set in the styles they name. */
[[nodiscard]] inline Element feed(const TextRing& ring,
                                  const TextOptions& options) {
  return feed(ring, options.window,
              [&](const TextRow& row) { return textRow(row, options.styles); });
}

/** The same feed, styled by whoever composed it rather than by whoever
 *  wrote this call — bind `env::Provide<feed::TextOptions>` upstream and
 *  nothing has to be threaded through the containers in between. Falls back
 *  to default-constructed options when nothing is bound.
 *
 *  The environment key is this component's own props type. There is no
 *  library-wide theme value to inherit instead: a design-token layer would
 *  have to decide what a token IS for every component, and this library
 *  deliberately leaves that to the composition. */
[[nodiscard]] inline Element feed(const TextRing& ring) {
  return feed(ring, core::env::inheritedOr(TextOptions{}));
}

/** How tall a text feed of @p rows rows is — `height()` with the base style
 *  as the probe row. Exact for a style set whose entries differ only in
 *  colour, a lower bound for one that varies size. */
[[nodiscard]] inline float height(const TextOptions& options, size_t rows,
                                  sigil::weave::FontContext& fonts) {
  return height(
      options.window, rows, [&] { return text(u8"H", options.styles.base()); },
      fonts);
}

/** The height of the options' OWN window — `height(options,
 *  options.window.visible, fonts)`, which is what an author laying out a
 *  feed panel wants. */
[[nodiscard]] inline float height(const TextOptions& options,
                                  sigil::weave::FontContext& fonts) {
  return height(options, options.window.visible, fonts);
}

}  // namespace sigil::compose::feed
