#pragma once

/** @file
 * SigilCompose console — a streaming log/terminal view, built entirely from
 * kernel composition: a ring of text lines, windowed to the last N, laid out
 * as a clipped column. There is no new machinery here, only elements.
 *
 * The load-bearing rule: lines are keyed by a MONOTONIC SEQUENCE ID, never
 * by array index. An append therefore shifts nothing — every surviving line
 * keeps its instance and its cached picture, and reconciliation costs one
 * mount for the new tail plus one unmount for the scrolled-out head. Index
 * keys would renumber every line on every append, so each would compare
 * unequal, re-patch, and drop its cache; the cost would grow with the window
 * instead of staying constant.
 *
 * Volatility follows from that: scrollback lines are static and cache, and
 * the only live values are the ones the caller binds — a blinking cursor's
 * opacity, a glow on the panel. To fade out old lines, put a gradient mask
 * over the whole console (a Material drawn with kDstIn) rather than animating
 * per-line opacity, which would make every faded line volatile.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Studio.h"
#include "sigilcompose/Util.h"

#include <deque>
#include <string>
#include <vector>

namespace sigil::compose::console {

struct Line {
  uint64_t seq = 0;
  std::u8string text;
  /** Optional per-line style override; index into Style::palette (SIZE_MAX
   *  = Style::text). Lets log levels tint without a style per line. */
  size_t paletteIndex = SIZE_MAX;
};

/** Append-only scrollback with monotonic sequence ids. */
class LineRing {
public:
  explicit LineRing(size_t capacity = 512) : m_capacity(capacity) {}

  uint64_t append(std::u8string text, size_t paletteIndex = SIZE_MAX) {
    const uint64_t seq = m_next++;
    m_lines.push_back({seq, std::move(text), paletteIndex});
    if (m_lines.size() > m_capacity)
      m_lines.pop_front();
    return seq;
  }
  void clear() { m_lines.clear(); }
  const std::deque<Line> &lines() const { return m_lines; }
  uint64_t nextSeq() const { return m_next; }

private:
  size_t m_capacity;
  uint64_t m_next = 1;
  std::deque<Line> m_lines;
};

struct Style {
  sigil::weave::TextStyle text;                  // default line style
  std::vector<sigil::weave::TextStyle> palette;  // Line::paletteIndex targets
  float gap = 2.0f;
  size_t visibleLines = 24; // the viewport window (virtualization)
  /** Cursor block after the tail line (alpha 0 = none); bind the blink via
   *  cursorOpacity — the ONLY volatile node in the whole console. */
  SkColor4f cursorColor = {0, 0, 0, 0};
  float cursorWidth = 8.0f, cursorHeight = 14.0f;
  const choreograph::Output<float> *cursorOpacity = nullptr;

  /** Comparable, so a `Style` can BE an inherited value: `env::` bindings
   *  are keyed by C++ type, and the key this component uses is its own
   *  props type — which is how a console gets themed from above without
   *  the library shipping a palette layer (see `console(const LineRing&)`).
   *  Exact and structural, like every other value the reconciler compares:
   *  the binding pointer by identity, colours bitwise. */
  bool operator==(const Style &) const = default;
};

/** ONE console row — the exact Element `console()` builds for each line,
 *  including the `con#<seq>` key the constant-cost append rests on.
 *
 *  Exposed so a caller can build the column itself when the rows need
 *  something `console()` does not give them. An entrance is the usual
 *  reason: `staggerChildren()` delays the mount transitions its children
 *  DECLARE, and a plain `text()` declares none, so a typed-out console has
 *  to declare the transition on each row:
 *
 *      auto panel = box().column().gap(st.gap).clip().staggerChildren(40ms);
 *      for (const console::Line &l : ring.lines())
 *        panel.child(console::line(l, st).opacity(
 *            animate(from(0.0f).to(1.0f), {180ms, &choreograph::easeNone})));
 *
 *  Windowing is the caller's in that spelling: `console()` shows the last
 *  `Style::visibleLines` and the loop above shows the whole ring. */
inline Element line(const Line &l, const Style &style) {
  const sigil::weave::TextStyle &ts = l.paletteIndex < style.palette.size()
                                          ? style.palette[l.paletteIndex]
                                          : style.text;
  return text(l.text, ts).key("con#" + std::to_string(l.seq));
}

/** The console: the ring's last visibleLines as seq-keyed rows. Re-render
 *  on every append — reconciliation prices it at one mount (the tail), one
 *  unmount (the scrolled head), zero patches on surviving lines. */
inline Element console(const LineRing &ring, const Style &style);

/** The same console, styled by whoever composed it rather than by whoever
 *  wrote this call — bind `env::Provide<console::Style>` upstream and
 *  nothing has to be threaded through the containers in between. Falls back
 *  to a default-constructed `Style` when nothing is bound.
 *
 *  The environment key is this component's own props type. There is no
 *  library-wide theme value to inherit instead: a design-token layer would
 *  have to decide what a token IS for every component, and this library
 *  deliberately leaves that to the composition. */
inline Element console(const LineRing &ring) {
  return console(ring, env::inheritedOr(Style{}));
}

inline Element console(const LineRing &ring, const Style &style) {
  auto panel = box().column().gap(style.gap).clip();
  const std::deque<Line> &lines = ring.lines();
  const size_t n = lines.size();
  const size_t from = n > style.visibleLines ? n - style.visibleLines : 0;
  for (size_t i = from; i < n; ++i)
    panel.child(line(lines[i], style)); // console::line — the row, exposed
  if (style.cursorColor.fA > 0) {
    Element cursor = box().width(style.cursorWidth)
                         .height(style.cursorHeight)
                         .fill(Fill::color(style.cursorColor))
                         .key("con#cursor");
    if (style.cursorOpacity)
      cursor.opacity(style.cursorOpacity);
    panel.child(std::move(cursor));
  }
  return panel;
}

/** How tall a console of `lines` rows is at this `style` — the number to
 *  size a panel against instead of hand-tuning one:
 *
 *      const float rows = console::height(logStyle(), fonts);
 *      const float h = 2 * padY + 3 * rows + 4 * gap + 2 * dividerWidth;
 *
 *  **It measures, it does not compute.** A probe ring of `lines` rows goes
 *  through the real `console()` and the real `compose::measure()`, so the
 *  answer includes `Style::gap` between the rows, the cursor row when
 *  `cursorColor` is opaque, and whatever SigilWeave's line metrics and
 *  Yoga's pixel grid do to a text node. Doing the arithmetic instead —
 *  `metrics(style.text, fonts).lineHeight * lines + gap * (lines - 1)` —
 *  under-reports by close to a whole row on a long console, because each
 *  row is rounded up to the pixel grid before the gaps are added and the
 *  arithmetic rounds nothing.
 *
 *  Three properties worth knowing:
 *
 *  - **It clamps to the window.** The probe runs through `console()`, which
 *    shows only the last `visibleLines`, so asking for 400 rows on a
 *    12-line window returns the 12-line height. The console is virtualized
 *    and its height is bounded by its style.
 *  - **Rows are measured at `Style::text`.** `Line::paletteIndex` selects
 *    another `TextStyle`, and a palette entry at a different SIZE makes its
 *    row a different height. For a palette that varies colour only — which
 *    is what `monoStyle` builds — this is exact; for one that varies size
 *    it is a lower bound.
 *  - **A row that WRAPS is taller.** The probe measures unconstrained, so
 *    each row is one line box. A real row long enough to wrap at the
 *    console's final width takes two, which is what `clip()` is for.
 *
 *  The `box()` shell is the `snapshot()`/`measure()` sizing rule: both size
 *  by the root's CHILDREN and ignore the root's own dimensions. It changes
 *  nothing today — `console()` returns a panel that sets neither a width
 *  nor a height — and it keeps the measurement honest if that ever
 *  changes. */
inline float height(const Style &style, size_t lines,
                    sigil::weave::FontContext &fonts) {
  LineRing probe(lines > 0 ? lines : 1);
  for (size_t i = 0; i < lines; ++i)
    probe.append(u8"H");
  return compose::measure(box().child(console(probe, style)), fonts).height();
}

/** The height of the style's OWN window — `height(style, style.visibleLines,
 *  fonts)`, which is what an author laying out a console panel wants. */
inline float height(const Style &style, sigil::weave::FontContext &fonts) {
  return height(style, style.visibleLines, fonts);
}

// ---------------------------------------------------------------------------
// Ready-made chrome around the rings.

/** A monospaced style: one face, one size, one base colour, and N palette
 *  entries that differ from it only in colour — which is the palette shape
 *  `height()` can measure exactly.
 *
 *  The palette entries mean **nothing to this function**, deliberately.
 *  Callers do not agree on what the slots are (one reads them as
 *  {dim, heading, pass, number, fail}, another as
 *  {dim, pass, fail, measured, heading}), so there is no fixed slot enum
 *  here and `Line::paletteIndex` is just an index. */
inline Style monoStyle(sk_sp<SkTypeface> face, float size, SkColor4f base,
                       std::vector<SkColor4f> palette) {
  Style s;
  s.text = studio::type({.face = face, .size = size, .color = base});
  s.palette.reserve(palette.size());
  for (SkColor4f c : palette)
    s.palette.push_back(
        studio::type({.face = face, .size = size, .color = c}));
  return s;
}

/** A bordered console plate: N LineRings laid on one axis with hairline
 *  dividers between them, inside a padded box with a fill and an inner
 *  stroke.
 *
 *  **It does not place itself.** A component that decides where it goes
 *  cannot be reused, so this returns an element and the caller positions
 *  it:
 *
 *      console::panel({.rings = {&logA, &logB, &logC, &logD},
 *                      .style = logStyle(), .paddingX = 14, .paddingY = 9,
 *                      .gap = 18,
 *                      .fill = Fill::color(hex(0xe4d9c0, 0.78f)),
 *                      .border = Fill::color(hex(0x241c15, 0.25f)),
 *                      .divider = Fill::color(hex(0x241c15, 0.18f))})
 *          .rect({64, 1420, kW - 64, 1576})
 *
 *  It covers one axis only. A titled panel, or a grid of panels, is a line
 *  of ordinary composition around this one — nest two `panel()`s in a row
 *  rather than looking for a mode here. */
struct Panel {
  /** In order along the axis. Null entries are skipped. */
  std::vector<const LineRing *> rings;
  /** Shared by every ring — one type treatment across the whole plate. */
  Style style;
  /** false (default) lays the rings out as a ROW of columns; true stacks
   *  them vertically. Dividers follow the axis either way. */
  bool column = false;
  float paddingX = 10.0f, paddingY = 8.0f;
  float gap = 12.0f;
  /** The plate's own ground and its keyline. */
  Fill fill, border;
  float borderWidth = 1.0f;
  /** Inner (default) keeps the keyline inside the silhouette; Center makes
   *  it straddle the edge, which moves every pixel along the border by half
   *  a stroke width. It is a parameter because the two are visibly
   *  different at a 1 px keyline, not because either is more correct. */
  PathFormat::Align borderAlign = PathFormat::Align::Inner;
  /** Fill::none() (default) means no dividers. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** Fixed extent per ring across the stacking axis; 0 shares the space
   *  equally with grow(1).
   *
   *  There is deliberately no third "size each ring to its content" mode.
   *  A console sizes itself from `visibleLines`, so a ring's natural extent
   *  normally EXCEEDS the padded interior — and in that case flex `shrink`
   *  (which defaults to 1) distributes the deficit across exactly the sizes
   *  grow(1) would distribute a surplus across, making the two
   *  indistinguishable. They diverge only for rings whose natural extent is
   *  SMALLER than the interior, where grow(1) stretches them and pushes
   *  later siblings along; give those a `ringExtent`. */
  float ringExtent = 0.0f;
};

inline Element panel(const Panel &p) {
  Element plate = box().fill(p.fill);
  if (p.border.kind != Fill::Kind::None)
    plate.stroke(util::stroke(p.borderWidth, p.border, p.borderAlign));

  // grow(1) so the padded interior fills whatever rect the caller gave the
  // plate, without the interior needing its own size.
  Element inner = box().padding(p.paddingX, p.paddingY).gap(p.gap).grow(1);
  if (p.column)
    inner.column();
  else
    inner.row();

  const bool dividers = p.divider.kind != Fill::Kind::None;
  bool first = true;
  for (const LineRing *ring : p.rings) {
    if (!ring)
      continue;
    if (!first && dividers)
      inner.child(p.column
                      ? box().height(p.dividerWidth).fill(p.divider)
                      : box().width(p.dividerWidth).fill(p.divider));
    first = false;
    // Shared-space rings go in DIRECTLY with grow(1) — no wrapper box. The
    // difference is not cosmetic: as a flex child of the row the console
    // also stretches to the interior height, where inside a wrapper it
    // would take its content height and leave the rest of the cell empty.
    if (p.ringExtent <= 0) {
      inner.child(console(*ring, p.style).grow(1));
      continue;
    }
    Element cell = box().child(console(*ring, p.style));
    if (p.column)
      cell.height(p.ringExtent);
    else
      cell.width(p.ringExtent);
    inner.child(std::move(cell));
  }
  plate.child(std::move(inner));
  return plate;
}

} // namespace sigil::compose::console
