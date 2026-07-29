#pragma once

/** @file
 * SigilCompose console — the streaming log/terminal component (REVIEW.md
 * §6.2), and the cleanest proof that the retained spine already composes
 * into a virtualized append-only view: this header is PURE composition over
 * the kernel (no new machinery), and appends cost O(1) reconciliation.
 *
 * The load-bearing rule (the Alacritty ring model): lines are keyed by a
 * MONOTONIC SEQUENCE ID, never by array index. An append therefore shifts
 * nothing — every retained line keeps its instance and its cached picture;
 * only the new tail mounts (and the scrolled-out head unmounts). Index keys
 * would shift on every append and bust every line's cache, turning O(1)
 * into O(visible).
 *
 * Volatility stays where it belongs: scrollback lines are static and cache;
 * the only live things are whatever YOU bind — a blinking cursor's opacity,
 * a glow on the panel. Fade-out of old lines is best done with a gradient
 * mask overlay (Material + kDstIn) over the console, which touches no line.
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
   *  are keyed by C++ type and the key a library component uses is its
   *  own props type — which is how the console gets themed without the
   *  library shipping a palette layer (see `console(const LineRing&)`
   *  and Compose.h "env"). Exact and structural, like every other value
   *  the reconciler compares: the binding pointer by identity, colours
   *  bitwise. */
  bool operator==(const Style &) const = default;
};

/** ONE console row — the exact Element `console()` builds for each line,
 *  including the `con#<seq>` key the whole O(1) append story rests on.
 *
 *  Exposed because `console()` builds its rows internally, and a row built
 *  internally cannot be given an entrance: `staggerChildren()` delays the
 *  `animate()` mount transitions its children DECLARE, and a plain `text()`
 *  declares none, so "the console types out on mount" was inexpressible
 *  through the component (ROADMAP §14 — this is the entry's own second
 *  option, and the smaller one; `Style::entrance` would have been a
 *  decision about what an entrance IS).
 *
 *      auto panel = box().column().gap(st.gap).clip().staggerChildren(40ms);
 *      for (const console::Line &l : ring.lines())
 *        panel.child(console::line(l, st).opacity(
 *            animate(from(0.0f).to(1.0f), {180ms, &choreograph::easeNone})));
 *
 *  The window is yours in that spelling — `console()` shows the last
 *  `Style::visibleLines`, three lines of arithmetic you now own. */
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
 *  wrote this call — `env::Provide<console::Style>` upstream, nothing
 *  threaded through the four containers in between. Falls back to a
 *  default-constructed `Style` when nothing is bound, exactly like a React
 *  context's default value.
 *
 *  This is the first library component on the env channel, and it is the
 *  worked example of the rule: the KEY is the component's own existing
 *  props type. There is no `compose::Theme` and there will not be one —
 *  a design-token layer is the header archive/EXTRACT.md §4.7 refused. */
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

/** How tall a console of `lines` rows is at this `style` — the number
 *  `Style::visibleLines` always implied and never gave (ROADMAP §21).
 *
 *  Filed by `dunhuang_star_chart`, which stacks three `LineRing`s with
 *  hairline dividers inside ONE fixed-height panel and had to hand-tune
 *  that height against 9.2 px mono × 12 lines. With this the panel is
 *  arithmetic over an answer:
 *
 *      const float rows = console::height(logStyle(), fonts);
 *      const float h = 2 * padY + 3 * rows + 4 * gap + 2 * dividerWidth;
 *
 *  **It measures, it does not compute.** A probe ring of `lines` rows goes
 *  through the real `console()` and the real `compose::measure()`, so the
 *  answer includes `Style::gap` between the rows, the cursor row when
 *  `cursorColor` is opaque, and whatever SigilWeave's line metrics and
 *  Yoga's pixel grid do to a text node. The arithmetic spelling —
 *  `metrics(style.text, fonts).lineHeight * lines + gap * (lines - 1)` —
 *  is not merely less tidy, it is WRONG by most of a row: at the study's
 *  9.2 px mono × 12 it answers 121.4 where the laid-out console is 131,
 *  because each row is ceil()ed to the pixel grid before the gaps are
 *  added and the arithmetic ceils nothing (measured as this function's
 *  positive control).
 *
 *  Three properties worth knowing:
 *
 *  - **It clamps to the window.** The probe runs through `console()`, which
 *    shows only the last `visibleLines`, so `height(style, 400, fonts)` on
 *    a 12-line window is the 12-line height. That is the point: the console
 *    is virtualized and its height is bounded by its style.
 *  - **Rows are measured at `Style::text`.** `Line::paletteIndex` selects
 *    another `TextStyle`, and a palette entry at a different SIZE makes its
 *    row a different height. Every plate in the corpus varies palette
 *    entries by COLOUR only (`monoStyle` enforces exactly that), so this is
 *    the honest answer for them and a lower bound for anyone who does
 *    otherwise.
 *  - **A row that WRAPS is taller.** The probe measures unconstrained, so
 *    each row is one line box. A real row long enough to wrap at the
 *    console's final width takes two, which is what `clip()` is for.
 *
 *  The `box()` shell is the `snapshot()`/`measure()` sizing rule
 *  (Instances.h, Brushes.h): those size by the root's CHILDREN, not the
 *  root's own dims. It does not bite here — `console()` returns a panel
 *  that never sets its own width or height — and the shell keeps it that
 *  way if that ever changes. The pin asserts both spellings agree rather
 *  than assuming it — see
 *  `ComposeConsole.VisibleLinesHasAHeightAndThreeRingsFitOnePanel`. */
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
// The verification plate — the chrome around the rings.

/** The mono style a proving plate builds by hand: one face, one size, one
 *  base colour, and N palette entries that differ only in colour.
 *
 *  Six studies and `gallery/ScenesConsole.h:117` all write the same block —
 *  `s.text = type(faceMono, sz, base); s.palette = {type(faceMono, sz, c0),
 *  type(faceMono, sz, c1), …};` — five to six times over.
 *
 *  The palette is a plain vector and its entries mean **nothing to this
 *  function**, deliberately. The six hand-built plates do not agree on what
 *  the slots are: `thunder_fulu` reads {dim, heading, pass, number, fail}
 *  and `minard_1869` reads {dim, pass, fail, measured, heading}. A fixed
 *  five-slot enum would be a decision, and it would already be wrong for
 *  one of the two studies that motivated this. */
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
 *  dividers between them, inside a padded box with a fill and an INNER
 *  stroke.
 *
 *  Seven independent hand-builds — `chaucer_astrolabe.cpp:2507`,
 *  `sigillum_aemeth.cpp:1719`, `thunder_fulu.cpp:1494`,
 *  `minard_1869.cpp:2531`, `psx_doom_fire.cpp:565`,
 *  `eva_magi_defense.cpp:1044`, and the control group's
 *  `gallery/ScenesConsole.h:205` — differing only in count, axis and
 *  colour. ~24 lines each.
 *
 *  Three of the seven are reproduced pixel-for-pixel by migration:
 *  `chaucer_astrolabe` and `minard_1869` (rows), and `thunder_fulu` (a
 *  column of three with dividers, verified at four phases — see
 *  `ringExtent` for why the column case works when it looked like it
 *  should not). `psx_doom_fire` and `ScenesConsole.h` are a titled panel
 *  around ONE ring, which is a different thing and should stay
 *  hand-built.
 *
 *  **It does not place itself.** That is the `Layouts.h` rule: a component
 *  that decides where a thing goes gets used zero times. Hand it a rect:
 *
 *      console::panel({.rings = {&logA, &logB, &logC, &logD},
 *                      .style = logStyle(), .paddingX = 14, .paddingY = 9,
 *                      .gap = 18,
 *                      .fill = Fill::color(hex(0xe4d9c0, 0.78f)),
 *                      .border = Fill::color(hex(0x241c15, 0.25f)),
 *                      .divider = Fill::color(hex(0x241c15, 0.18f))})
 *          .rect({64, 1420, kW - 64, 1576})
 *
 *  Does not cover a GRID of plates — `sigillum_aemeth.cpp:1761` nests two
 *  panels in a row, which is one line of composition and should stay one
 *  line of composition. */
struct Panel {
  /** In order along the axis. Null entries are skipped. */
  std::vector<const LineRing *> rings;
  /** Shared by every ring — a plate whose columns disagree about type is
   *  not a plate. */
  Style style;
  /** false (default) lays the rings out as a ROW of columns, which is what
   *  five of the seven plates do; true stacks them. */
  bool column = false;
  float paddingX = 10.0f, paddingY = 8.0f;
  float gap = 12.0f;
  /** The plate's own ground and its keyline. */
  Fill fill, border;
  float borderWidth = 1.0f;
  /** Inner (default) keeps the keyline inside the silhouette, which is what
   *  six of the seven plates want. It is a parameter because the seventh
   *  does not: `minard_1869` strokes Center, and a 1 px keyline straddling
   *  the edge instead of sitting inside it moves 10,796 pixels — which is
   *  how this field was found. */
  PathFormat::Align borderAlign = PathFormat::Align::Inner;
  /** Fill::none() (default) means no dividers. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** Fixed extent per ring across the stacking axis; 0 shares the space
   *  equally (grow(1)). `minard_1869` pins 480 px; everyone else shares.
   *
   *  This comment used to claim there had to be a third "size each ring to
   *  its content" mode, because `thunder_fulu`'s column plate stacks three
   *  content-height rings and grow(1) looked like it would redistribute
   *  them. **That was reasoning, and it was wrong.** Migrating the plate and
   *  measuring gives zero differing pixels at four phases (t = 8, 14, 22.1,
   *  30).
   *
   *  The reason is `shrink`, which defaults to 1. A console plate sizes
   *  `visibleLines` to fill its panel, so the rings' natural height always
   *  EXCEEDS the interior — and when they overflow, flex shrink distributes
   *  the deficit to exactly the sizes grow(1) distributes the surplus to.
   *  The two modes converge for every plate in the corpus.
   *
   *  Where they would genuinely differ is rings whose natural height is
   *  LESS than the interior: grow(1) then stretches them and pushes the
   *  later siblings down. No plate does that, so the mode stays unbuilt —
   *  now on evidence rather than on a guess. */
  float ringExtent = 0.0f;
};

inline Element panel(const Panel &p) {
  Element plate = box().fill(p.fill);
  if (p.border.kind != Fill::Kind::None)
    plate.stroke(util::stroke(p.borderWidth, p.border, p.borderAlign));

  // grow(1) so the padded interior fills the plate the caller sized; the
  // hand-built versions all spell that as a second absolute rect.
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
    // wrapper matters: as a flex child of the row the console also stretches
    // to the interior height, and inside a wrapper it would take its content
    // height instead. Five of the seven hand-built plates spell the direct
    // form, so the migration has to be a no-op.
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
