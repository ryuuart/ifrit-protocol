#pragma once

/** @file
 * @ingroup motion-clock
 *
 * The ticker: the motions a host started, stepped from frame deltas
 * beside any registered steppables, and the report of whether anything
 * still moves.
 */

#include <functional>
#include <memory>
#include <type_traits>
#include <vector>

#include "sigilcore/callable/Callable.h"

namespace sigil::motion {

namespace detail {
/** ONE MOTION the ticker steps: advanced by a frame's delta, answering
 *  whether it still runs. The values feature supplies the motions; the
 *  ticker only moves them, in the order they were started. */
class Stepped {
 public:
  virtual ~Stepped() = default;
  virtual bool advance(double deltaSeconds) = 0;
};
}  // namespace detail

/**
 * The ticking engine for animation: steps the motions a host started,
 * plus any registered steppables, from per-frame deltas. It reports
 * whether anything is still animating, so a host can stay event-driven —
 * render while active() or while content is dirty, sleep otherwise.
 *
 *   bool animating = ticker.tick(clock.tick());
 *
 * Not thread-safe. Use one Ticker per animation domain and touch it only
 * from that domain's thread.
 */
class Ticker {
 public:
  /** Starts stepping @p motion from the next tick on. A finished motion
   *  leaves the ticker, so active() settles to false by itself. */
  void run(std::shared_ptr<detail::Stepped> motion);

  /**
   * Registers an additional steppable. The frame's delta and the ticker's
   * total elapsed time are both offered and a steppable names the ones it
   * reads — `[] {…}`, `[](double deltaSeconds) {…}`,
   * `[](double deltaSeconds, double elapsed) {…}` — and it may answer whether it still needs frames. Steppables
   * answering false are dropped. Use for per-frame effects Choreograph does
   * not express, such as physics.
   *
   * A steppable that always returns true keeps active() true forever, and
   * so keeps an event-driven host rendering forever. A steppable that
   * ANSWERS NOTHING is one of those: saying nothing about being finished is
   * taken as never finished, so a void steppable keeps active() true for as
   * long as it is added. Retire it — answer false, or do not add it — when
   * it has nothing left to do.
   */
  template <class Fn>
    requires core::PrefixCallable<Fn, void(double, double)>
  void add(Fn steppable) {
    addStep([held = std::move(steppable)](double deltaSeconds, double elapsed) mutable {
      if constexpr (std::is_void_v<decltype(core::callPrefix(held, deltaSeconds,
                                                             elapsed))>) {
        core::callPrefix(held, deltaSeconds, elapsed);
        return true;
      } else {
        return (bool)core::callPrefix(held, deltaSeconds, elapsed);
      }
    });
  }

  /**
   * Registers a FIXED-TIMESTEP steppable: `steppable()` is called zero or more
   * times per frame so that it advances at exactly @p rate, whatever the
   * host is drawing at. It may answer whether it still needs frames, like
   * add(), and answering nothing means it always does.
   *
   *     ticker.addFixed(27.0, [this] { stepFire(); });
   *
   * The step count comes from TOTAL ELAPSED TIME, not from a running
   * accumulator: `want = floor(total * rate)`, run `want - ran`. A float
   * accumulator compared against a step size drifts over a long run, so
   * the same simulated moment lands on either side of a step boundary
   * depending on the host's draw rate. Counting from total time is exact
   * at any draw rate, which is what makes a captured frame reproducible.
   *
   * @p maxCatchUp bounds how many steps one frame may run. Without it, a
   * hitch longer than one step makes the next frame run the backlog,
   * which takes longer, which grows the backlog. Dropping simulated time
   * is the correct failure: the simulation runs slow for one frame
   * instead of locking up the process.
   *
   * @p alphaOut, if given, receives the leftover fraction of a step after
   * this frame's stepping — the standard render interpolant. A fixed-rate
   * simulation drawn straight from its own state judders whenever the
   * draw rate is not a multiple of `rate`; drawing
   * `lerp(previous, current, alpha)` removes it.
   */
  /** What one frame's fixed stepping did. When `clamped` is true the
   *  simulation DROPPED time, so anything measured on that frame — a
   *  constraint residual, a convergence rate — is meaningless and must
   *  not be reported. This flag is the only signal of that. */
  struct FixedStatus {
    int stepsRun = 0;
    bool clamped = false;
  };

  template <class Fn>
    requires core::PrefixCallable<Fn, void()>
  void addFixed(double rate, Fn steppable, int maxCatchUp = 8,
                float* alphaOut = nullptr,
                FixedStatus* statusOut = nullptr) {
    addFixedStep(
        rate,
        [step = std::move(steppable)]() mutable {
          if constexpr (std::is_void_v<decltype(core::callPrefix(step))>) {
            core::callPrefix(step);
            return true;
          } else {
            return (bool)core::callPrefix(step);
          }
        },
        maxCatchUp, alphaOut, statusOut);
  }

  /** Steps the motions, then the steppables, by `deltaSeconds`; returns
   *  active(). A zero delta — a paused clock — still steps everything
   *  and still reports activity. */
  bool tick(double deltaSeconds);

  /** True while a motion runs or any steppable remains registered.
   *
   *  THE DOMAIN-WIDE form of "is anything moving", and a DECLARATION like
   *  every other: it says machinery is registered, never that the numbers
   *  are changing — a wave held at one phase is active and moves nothing.
   *  `isLive()` is the same question about one value; whether values have
   *  provably held still is a fact about the values themselves and is
   *  answered by whatever caches them. */
  bool active() const;

  /**
   * Total time this Ticker has been stepped, in seconds: the sum of the
   * deltas passed to tick(), not wall-clock time.
   *
   * The same number a steppable is offered as its second parameter, for a
   * caller that reaches the clock from outside one:
   *
   *     ticker.add([this](double deltaSeconds, double elapsed) {
   *       phase = std::sin(elapsed * 2.0);
   *     });
   */
  double elapsed() const { return m_elapsed; }

 private:
  /** The erased steppable every `add` spelling lands on: the delta and the
   *  elapsed time, answering whether it still needs frames. */
  void addStep(std::function<bool(double, double)> steppable);
  void addFixedStep(double rate, std::function<bool()> steppable, int maxCatchUp,
                    float* alphaOut,
                    FixedStatus* statusOut);

  std::vector<std::shared_ptr<detail::Stepped>> m_motions;
  std::vector<std::function<bool(double, double)>> m_steppables;
  double m_elapsed = 0.0;
};

}  // namespace sigil::motion
