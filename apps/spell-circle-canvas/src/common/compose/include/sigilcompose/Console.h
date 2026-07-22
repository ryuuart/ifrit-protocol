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
};

/** The console: the ring's last visibleLines as seq-keyed rows. Re-render
 *  on every append — reconciliation prices it at one mount (the tail), one
 *  unmount (the scrolled head), zero patches on surviving lines. */
inline Element console(const LineRing &ring, const Style &style) {
  auto panel = box().column().gap(style.gap).clip();
  const std::deque<Line> &lines = ring.lines();
  const size_t n = lines.size();
  const size_t from = n > style.visibleLines ? n - style.visibleLines : 0;
  for (size_t i = from; i < n; ++i) {
    const Line &line = lines[i];
    const sigil::weave::TextStyle &ts =
        line.paletteIndex < style.palette.size()
            ? style.palette[line.paletteIndex]
            : style.text;
    panel.child(
        text(line.text, ts).key("con#" + std::to_string(line.seq)));
  }
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
 *  Two of the seven are reproduced pixel-for-pixel by migration
 *  (`chaucer_astrolabe`, `minard_1869`). `psx_doom_fire` and
 *  `ScenesConsole.h` are a titled panel around ONE ring, which is a
 *  different thing and should stay hand-built; `thunder_fulu` needs a
 *  content-height column (see `ringExtent`).
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
   *  equally (grow(1)). `minard_1869` pins 480 px, `chaucer_astrolabe` and
   *  `sigillum_aemeth` share.
   *
   *  There is no third "size each ring to its content" mode, and that is a
   *  limit worth stating rather than papering over: `thunder_fulu`'s column
   *  plate stacks three content-height rings, and reproducing that would
   *  need a mode this has no migration to verify against. That plate keeps
   *  its longhand until someone migrates it and can measure the result. */
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
