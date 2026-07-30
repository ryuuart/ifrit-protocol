#pragma once

#include "sigilmotion/Animation.h"

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
   * `wiggle(&penTip, …)` and every `Output*`-typed consumer (a pool
   * write, `spans::range`, `Effect::uniform`, a world lane) just work.
   * This closes the corpus's #1 measured gap — the shadow cells sketches
   * re-copied by hand every tick (ROADMAP §43.3).
   *
   * **THE HONEST LIMIT, at the call site because it belongs here:**
   * `derive()` remaps a schedule's VALUE. That equals a TIME remap
   * exactly when the schedule is affine in time — `phase = k·t` gives
   * `0.5·phase(t) = phase(t/2)`; a non-linear phase gives a value remap
   * that is not a retime. Compose does not retime time; it remaps
   * schedules (and the corpus's schedules are overwhelmingly `t*k` or
   * `fmod(t*k, 1)`, which is why this serves the demand).
   *
   * THE STEPPING CONTRACT: derivations run in a SECOND PHASE, after the
   * timeline and after every steppable, so **a derivation never reads a
   * stale source** — registration order does not matter, unlike a
   * hand-rolled shadow copy in a steppable, which is one frame late
   * whenever it is registered before its source's writer.
   *
   * ONE LEVEL ONLY, enforced loudly: within the second phase there is no
   * topological order, so an Output may not be both a derivation's
   * destination and a derivation's source. A registration that would
   * chain two (`derive(&c, bind(&b))` after `derive(&b, …)`, in either
   * order), write one destination twice, or self-derive is REFUSED with
   * a warning and returns false — the silent one-frame lag it would
   * otherwise hide is exactly the class this contract exists to close.
   *
   * The chain is applied once at registration, so `dst` is correct
   * before the first tick. Derivations are pure in their source and are
   * never retired; they do NOT hold active() true — when nothing else is
   * active the source cannot move, so neither can the derived value.
   *
   * @return true if registered; false (with a warning) when refused.
   */
  bool derive(choreograph::Output<float> *dst, const Bound &chain);

  /** Steps the timeline and steppables by `deltaSeconds`, then the
   *  derivations (see derive() for the two-phase contract); returns
   *  active(). Zero deltas (paused clock) still report activity. */
  bool tick(double deltaSeconds);

  /** True while the timeline has motions or steppables remain. */
  bool active() const;

  /**
   * Total time this Ticker has been stepped, in seconds.
   *
   * `add()` hands a steppable only `dt`, and a steppable is where
   * Outputs get written — so every author who needed the CLOCK rather
   * than the delta captured a `double t = 0` by value and accumulated it
   * themselves. Thirty-six files in this repo do exactly that, in two
   * populations that never shared a line of code, and the accumulator is
   * identical in all of them.
   *
   * That was never a missing helper. `FrameClock::elapsed()` exists,
   * `PaintContext::elapsedSeconds` exists, and `Sketch::update` is
   * handed elapsed; the one place that could not reach it was the one
   * place that needed it. So this is the getter, not a `phase()` or a
   * `breath()` over it — a helper wrapping an unreachable clock would
   * only have produced a thirty-seventh copy.
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
