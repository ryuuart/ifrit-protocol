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

  /**
   * Registers a FIXED-TIMESTEP steppable: `fn()` is called zero or more
   * times per frame so that it advances at exactly @p hz, whatever the
   * host is drawing at. Returns whether it still needs frames, like
   * add().
   *
   *     ticker.addFixed(27.0, [this] { stepFire(); return true; });
   *
   * Every simulation-shaped study reinvented this and its
   * spiral-of-death clamp: a cellular automaton at 27 Hz behind the DOOM
   * PlayStation titles, particles at 24. The library had already
   * declared choppiness for shaders — `Material::quantizeTime(hz)` — and
   * nothing at all for logic.
   *
   * The step count comes from TOTAL ELAPSED TIME rather than from a
   * running accumulator: `want = floor(total * hz)`, run `want - ran`.
   * An accumulator compared against a step slips one comparison over a
   * long pre-roll, which made the same capture land on either side of a
   * step boundary depending on the draw rate — a study measured
   * byte-identical output at 60/30/20 fps and a one-step slip at 15 and
   * 10, and correctly diagnosed it as float accumulation rather than the
   * clamp. From total time it is exact at any draw rate.
   *
   * @p maxCatchUp bounds how many steps one frame may run. Without it, a
   * hitch longer than the step makes the next frame run the backlog,
   * which takes longer, which grows the backlog — the spiral. Dropping
   * simulated time is the correct failure: the sim runs slow for one
   * frame instead of locking the process.
   *
   * @p alphaOut, if given, receives the leftover fraction of a step after
   * the frame's stepping — the standard render interpolant. A fixed sim
   * drawn straight from its own state judders whenever the draw rate is
   * not a multiple of `hz`; drawing `lerp(previous, current, alpha)`
   * removes it. The accumulator lived inside this function and there was
   * no way to read it, which is the whole reason for the parameter: a
   * verlet body's state is literally the pair (x*, x), so the integrator
   * is already holding both ends of the interpolation and the library was
   * hiding the only scalar missing. Bindable like any Output, so
   * `bind(alphaOut)` reaches a property directly.
   */
  /** What one frame's fixed stepping did. When `clamped` is true the
   *  simulation DROPPED time, so anything measured on that frame — a
   *  constraint residual, a convergence rate — is meaningless and must
   *  not be reported. A study had to infer that from a step count. */
  struct FixedStatus {
    int stepsRun = 0;
    bool clamped = false;
  };

  void addFixed(double hz, std::function<bool()> fn, int maxCatchUp = 8,
                choreograph::Output<float> *alphaOut = nullptr,
                FixedStatus *statusOut = nullptr);

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
