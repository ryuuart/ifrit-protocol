#pragma once

#include <choreograph/Choreograph.h>

#include <functional>
#include <vector>

namespace sigil::motion {

/**
 * The backing ticking engine for animation: owns a master
 * choreograph::Timeline and steps it (plus any registered steppables)
 * from per-frame deltas, reporting whether anything is still animating
 * so hosts can stay event-driven — render when active() or content is
 * dirty, sleep otherwise.
 *
 * Choreograph supplies the vocabulary (Phrase/Sequence/Motion/Output —
 * see <choreograph/Choreograph.h>); the Ticker only drives it:
 *
 *   ch::Output<float> opacity = 0.0f;
 *   ticker.timeline().apply(&opacity).then<ch::RampTo>(1.0f, 0.4f);
 *   ...
 *   bool animating = ticker.tick(clock.tick());
 *
 * Not thread-safe; one Ticker per animation domain (typically per
 * canvas/composer), all touched from that domain's thread.
 */
class Ticker {
public:
  Ticker();

  /** The master timeline. Finished motions are removed automatically so
   *  active() naturally settles to false. */
  choreograph::Timeline &timeline() { return m_timeline; }

  /**
   * Registers an additional steppable — `fn(dt)` returns whether it
   * still needs frames. Steppables returning false are dropped. Use for
   * non-Choreograph per-frame effects (glyph choreography, physics).
   */
  void add(std::function<bool(double)> steppable);

  /** Steps the timeline and steppables by `deltaSeconds`; returns
   *  active(). Zero deltas (paused clock) still report activity. */
  bool tick(double deltaSeconds);

  /** True while the timeline has motions or steppables remain. */
  bool active() const;

private:
  choreograph::Timeline m_timeline;
  std::vector<std::function<bool(double)>> m_steppables;
};

} // namespace sigil::motion
