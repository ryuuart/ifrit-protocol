#pragma once

#include "sigilmotion/Animation.h"

#include <choreograph/Choreograph.h>

#include <functional>
#include <vector>

namespace sigil::motion {

/**
 * The ticking engine for animation: owns a master choreograph::Timeline
 * and steps it, plus any registered steppables, from per-frame deltas.
 * It reports whether anything is still animating, so a host can stay
 * event-driven — render while active() or while content is dirty, sleep
 * otherwise.
 *
 * Choreograph supplies the vocabulary (Phrase/Sequence/Motion/Output —
 * see <choreograph/Choreograph.h>); the Ticker only drives it:
 *
 *   ch::Output<float> opacity = 0.0f;
 *   ticker.timeline().apply(&opacity).then<ch::RampTo>(1.0f, 0.4f);
 *   ...
 *   bool animating = ticker.tick(clock.tick());
 *
 * Not thread-safe. Use one Ticker per animation domain and touch it only
 * from that domain's thread.
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
   * per-frame effects Choreograph does not express, such as physics.
   *
   * A steppable that always returns true keeps active() true forever,
   * and so keeps an event-driven host rendering forever. Retire it when
   * it has nothing left to do.
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
   * The step count comes from TOTAL ELAPSED TIME, not from a running
   * accumulator: `want = floor(total * hz)`, run `want - ran`. A float
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
   * draw rate is not a multiple of `hz`; drawing
   * `lerp(previous, current, alpha)` removes it. It is an ordinary
   * Output, so `bind(alphaOut)` reaches a property directly.
   */
  /** What one frame's fixed stepping did. When `clamped` is true the
   *  simulation DROPPED time, so anything measured on that frame — a
   *  constraint residual, a convergence rate — is meaningless and must
   *  not be reported. This flag is the only signal of that. */
  struct FixedStatus {
    int stepsRun = 0;
    bool clamped = false;
  };

  void addFixed(double hz, std::function<bool()> fn, int maxCatchUp = 8,
                choreograph::Output<float> *alphaOut = nullptr,
                FixedStatus *statusOut = nullptr);

  /**
   * A DERIVED OUTPUT: `dst` is recomputed every tick as `chain` applied
   * to its source Output's current value — the `bind()` shaping
   * vocabulary (`source`/`window`/`map`/`quantize`/the affine chain/
   * `wrap`/`wiggle`/`clamp`), verbatim, reaching an OUTPUT instead of a
   * property slot. The Ticker owns the write; the caller owns both
   * cells, exactly as with any bound Output.
   *
   *     ch::Output<float> phase, penTip, stepped, backwards;
   *     ticker.add([&](double) { phase = ...; return true; });
   *     ticker.derive(&penTip,    bind(&phase).offset(-0.008f).clamp(0, 1));
   *     ticker.derive(&stepped,   bind(&phase).quantize(8));
   *     ticker.derive(&backwards, bind(&phase).invert());
   *
   * A derived Output is an ordinary Output: `bind(&penTip)`,
   * `wiggle(&penTip, …)` and every `Output*`-typed consumer read it like
   * any other cell, with no knowledge that the Ticker owns the write.
   *
   * WHAT IT IS NOT: `derive()` remaps a schedule's VALUE, which equals a
   * remap of TIME only when the schedule is affine in time — for
   * `phase = k·t`, `0.5·phase(t)` is `phase(t/2)`, but for a non-linear
   * phase the two differ. If you need the source evaluated at another
   * time, retime the source, not its value.
   *
   * THE STEPPING CONTRACT: derivations run in a SECOND PHASE, after the
   * timeline and after every steppable, so **a derivation never reads a
   * stale source** and registration order does not matter. A hand-rolled
   * shadow copy inside a steppable does not have that property: it is one
   * frame late whenever it is registered before its source's writer.
   *
   * ONE LEVEL ONLY, refused loudly. Phase two has no topological order,
   * so an Output may not be both a derivation's destination and a
   * derivation's source. Chaining two derivations (in either
   * registration order), writing one destination twice, and deriving a
   * cell from itself are all REFUSED with a warning, returning false.
   * The silent one-frame lag such a registration would otherwise hide is
   * the exact failure this contract exists to prevent.
   *
   * The chain is applied once at registration, so `dst` is correct before
   * the first tick. Derivations are pure in their source and are never
   * retired, and they do NOT hold active() true: when nothing else is
   * active the source cannot move, so neither can the derived value.
   *
   * @return true if registered; false (with a warning) when refused.
   */
  bool derive(choreograph::Output<float> *dst, const Bound &chain);

  /** Steps the timeline and steppables by `deltaSeconds`, then the
   *  derivations (see derive() for the two-phase contract); returns
   *  active(). A zero delta — a paused clock — still steps everything
   *  and still reports activity. */
  bool tick(double deltaSeconds);

  /** True while the timeline holds motions or any steppable remains
   *  registered. Derivations never contribute. */
  bool active() const;

  /**
   * Total time this Ticker has been stepped, in seconds: the sum of the
   * deltas passed to tick(), not wall-clock time.
   *
   * A steppable is handed only `dt`, and a steppable is where Outputs get
   * written, so this is how one reaches the CLOCK rather than the delta
   * without accumulating a private copy of the same number:
   *
   *     ticker.add([this, &ticker](double) {
   *       phase = std::sin(ticker.elapsed() * 2.0);
   *       return true;
   *     });
   */
  double elapsed() const { return m_elapsed; }

private:
  /** One derived Output: the destination cell and the shaping applied to
   *  its source each tick. Stepped in phase two — see derive(). */
  struct Derivation {
    choreograph::Output<float> *dst = nullptr;
    BoundFloat map;
  };

  choreograph::Timeline m_timeline;
  std::vector<std::function<bool(double)>> m_steppables;
  std::vector<Derivation> m_derivations;
  double m_elapsed = 0.0;
};

} // namespace sigil::motion
