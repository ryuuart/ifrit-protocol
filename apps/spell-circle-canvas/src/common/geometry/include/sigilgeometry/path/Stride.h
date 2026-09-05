#pragma once
/** @file
 * An even-spacing walk assembled from pieces that arrive one at a time.
 *
 * `subdivide` and `resample` both hold the whole curve before they start.
 * A stylus does not: the next piece of the stroke exists only when the
 * device reports it, and a walk that restarted its spacing at every piece
 * would lay marks at the device's rate rather than at the spacing asked
 * for. This is that walk with its debt written down.
 */
namespace sigil::geometry::path {

/** AN EVEN-SPACING WALK IN PROGRESS: how far it has travelled and the
 *  distance the next landing is owed at, so that a walk fed one piece at
 *  a time lands exactly where one walk over the joined pieces would.
 *
 *  A piece is handed over as its LENGTH, and the walk answers with the
 *  fractions of that piece it lands at — the caller owns the geometry and
 *  interpolates whatever rides on it, which is why nothing here is a
 *  point. Two callers with the same lengths and the same spacing get the
 *  same landings whatever they are carrying. */
class Stride {
 public:
  /** One landing: how far along the piece just handed over it fell, in
   *  [0, 1], and the arc length from the start of the whole walk. */
  struct Step {
    float fraction = 0;
    float distance = 0;
  };

  /** Walks `length` more, calling `land` with each landing in order.
   *
   *  The first landing of a walk is one spacing in, not at zero: the
   *  place a walk starts is the caller's to lay down, and a walk that
   *  emitted it would lay it twice wherever two pieces meet. A piece of
   *  no length moves nothing and lands nothing. */
  template <class Land>
  void advance(float length, float spacing, Land&& land) {
    if (!(length > 0) || !(spacing > 0)) return;
    if (m_pending <= m_travelled) m_pending = m_travelled + spacing;
    while (m_pending <= m_travelled + length) {
      land(Step{.fraction = (m_pending - m_travelled) / length,
                .distance = m_pending});
      m_pending += spacing;
    }
    m_travelled += length;
  }

  /** The arc length of everything handed over so far. */
  [[nodiscard]] float travelled() const { return m_travelled; }
  /** The arc length the next landing is owed at. Behind `travelled()`
   *  before the first piece and after a spacing has changed, which the
   *  next `advance` reads as "one spacing from here". */
  [[nodiscard]] float pending() const { return m_pending; }

  /** Back to the start: a new walk, owing nothing. */
  void restart() {
    m_travelled = 0;
    m_pending = 0;
  }

 private:
  float m_travelled = 0;
  float m_pending = 0;
};

}  // namespace sigil::geometry::path
