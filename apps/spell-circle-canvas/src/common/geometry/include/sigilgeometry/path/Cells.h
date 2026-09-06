#pragma once
/** @file
 * A RECTANGLE OF CELLS THAT STEPS, and the two buffers that make a step
 * mean something.
 *
 * Every cellular automaton, every reaction-diffusion, every heat or fire
 * or erosion sheet is the same substrate: a grid of values, and a rule
 * that says what a cell becomes from what it and its neighbours ARE. The
 * one thing such a thing must get right is that the whole grid steps at
 * once — a rule that reads cells its own pass has already written is a
 * different and usually wrong automaton — and that is what a second
 * buffer is for.
 *
 * THE RULES ARE NOT HERE, and that is deliberate. A fire, a slime mould
 * and Conway's life share this buffer and share nothing else; a
 * catalogue of rules would be a catalogue of somebody else's pictures.
 * What is here is the buffer, the boundary behaviour, and the swap.
 *
 * No drawing either: a caller reads the cells and paints them however it
 * paints anything.
 */
#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace sigil::geometry::path {

/** WHAT A READ OUTSIDE THE RECTANGLE ANSWERS. The choice changes the
 *  picture completely — a wrapped sheet has no edge and a clamped one
 *  has a wall — so it is a property of the sheet rather than a decision
 *  each rule makes for itself. */
enum class Boundary : uint8_t {
  /** The nearest cell inside. An edge behaves as if it continued. */
  Clamp,
  /** The cell that many steps in from the other side. The sheet is a
   *  torus and there is no edge at all. */
  Wrap,
  /** The sheet's `outside` value. A wall of a stated value. */
  Constant,
};

/** A RECTANGLE OF CELLS, WITH A SPARE. */
template <typename T>
class Cells {
 public:
  Cells() = default;
  Cells(int width, int height, T fill = {})
      : m_width(std::max(width, 0)),
        m_height(std::max(height, 0)),
        m_front((size_t)std::max(width, 0) * (size_t)std::max(height, 0), fill),
        m_back(m_front.size(), fill),
        m_outside(fill) {}

  [[nodiscard]] int width() const { return m_width; }
  [[nodiscard]] int height() const { return m_height; }
  [[nodiscard]] size_t size() const { return m_front.size(); }
  [[nodiscard]] bool empty() const { return m_front.empty(); }

  /** The rule for a read outside, and the value `Boundary::Constant`
   *  answers with. */
  [[nodiscard]] Boundary boundary() const { return m_boundary; }
  void setBoundary(Boundary boundary) { m_boundary = boundary; }
  [[nodiscard]] const T& outside() const { return m_outside; }
  void setOutside(T value) { m_outside = std::move(value); }

  /** THE CELL AT (x, y), which must be inside. */
  [[nodiscard]] const T& at(int x, int y) const {
    return m_front[(size_t)y * (size_t)m_width + (size_t)x];
  }
  T& at(int x, int y) {
    return m_front[(size_t)y * (size_t)m_width + (size_t)x];
  }

  /** THE CELL AT (x, y) THROUGH THE EDGE RULE, which a rule reading its
   *  neighbours uses so that no rule spells the boundary itself. */
  [[nodiscard]] const T& read(int x, int y) const {
    switch (m_boundary) {
      case Boundary::Clamp:
        x = std::clamp(x, 0, m_width - 1);
        y = std::clamp(y, 0, m_height - 1);
        break;
      case Boundary::Wrap:
        x = ((x % m_width) + m_width) % m_width;
        y = ((y % m_height) + m_height) % m_height;
        break;
      case Boundary::Constant:
        if (x < 0 || y < 0 || x >= m_width || y >= m_height) return m_outside;
        break;
    }
    return m_front[(size_t)y * (size_t)m_width + (size_t)x];
  }

  /** The cells in row-major order, for a caller that walks them all —
   *  filling them, painting them, measuring them. */
  [[nodiscard]] std::span<const T> values() const { return m_front; }
  [[nodiscard]] std::span<T> values() { return m_front; }

  void fill(const T& value) {
    std::fill(m_front.begin(), m_front.end(), value);
  }

  /** ONE STEP: `rule(*this, x, y)` for every cell, written into the
   *  spare, and the two swapped at the end.
   *
   *  The rule is handed the sheet as it WAS and answers what the cell
   *  becomes. Nothing it can read has been written by this pass, so the
   *  order the cells are walked in cannot change the answer — which is
   *  the whole of what a second buffer buys, and the one thing a rule
   *  written against a single buffer gets wrong. */
  template <typename Rule>
  void step(Rule&& rule) {
    if (m_front.empty()) return;
    for (int y = 0; y < m_height; ++y)
      for (int x = 0; x < m_width; ++x)
        m_back[(size_t)y * (size_t)m_width + (size_t)x] = rule(*this, x, y);
    m_front.swap(m_back);
  }

  /** Content equality, cell for cell, including the boundary rule — two
   *  sheets that answer differently outside are not the same sheet. */
  friend bool operator==(const Cells& a, const Cells& b) {
    return a.m_width == b.m_width && a.m_height == b.m_height &&
           a.m_boundary == b.m_boundary && a.m_outside == b.m_outside &&
           a.m_front == b.m_front;
  }

 private:
  int m_width = 0;
  int m_height = 0;
  std::vector<T> m_front;
  std::vector<T> m_back;
  Boundary m_boundary = Boundary::Clamp;
  T m_outside{};
};

}  // namespace sigil::geometry::path
