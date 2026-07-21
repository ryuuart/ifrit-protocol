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

#include <deque>
#include <string>

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

} // namespace sigil::compose::console
