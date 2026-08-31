#pragma once

/** @file
 * One running sketch: the seam a host drives, whatever the sketch draws
 * through.
 */

#include <sigilsketch/core/CanvasSpec.h>

#include <span>
#include <string>
#include <vector>

class SkCanvas;

namespace sigil::sketch {

/** What one LANE of a frame cost, named by the runtime that spent it. */
struct Lane {
  const char* name = "";
  double ms = 0;
};

/** How a frame split at the seam the performance question always asks
 *  about first: `updateMs` is everything the sketch's own body did,
 *  `drawMs` is what its runtime then did with what the body described,
 *  and `totalMs` is both. */
struct Timing {
  double totalMs = 0;
  double updateMs = 0;
  double drawMs = 0;
};

/** ONE RUNNING SKETCH — a body, the runtime it draws through, and the
 *  state that runtime keeps between frames.
 *
 *  A Kind opens one; a host steps it and it draws. Everything a host
 *  needs in order to letterbox, clear, photograph and time a sketch is
 *  here, and nothing about WHICH runtime is: a host that lists,
 *  captures and benchmarks sketches never learns whether it is holding a
 *  drawn tree or a lit set. */
class Session {
 public:
  virtual ~Session() = default;

  /** The canvas the body declared. Read it AFTER opening: a sketch
   *  declares its size from inside its own setup. A body may declare a
   *  new one mid-run, so a host re-reads it after every frame. */
  [[nodiscard]] virtual const CanvasSpec& canvas() const = 0;

  /** Advance the scene by @p dt seconds and draw the result into
   *  @p canvas. A negative @p dt asks for wall time.
   *
   *  Advancing and drawing are ONE call because in some runtimes the
   *  drawing is part of what advances the scene — a picture recorded, a
   *  texture baked, a volatile leaf run — so a sweep that stepped
   *  without painting would reach different pixels than the host it is
   *  standing in for. */
  virtual void frame(SkCanvas& canvas, double dt) = 0;

  /** Draw the state the last frame left, WITHOUT advancing it — what a
   *  host repaints with when only its window changed. */
  virtual void repaint(SkCanvas& canvas) = 0;

  /** THE STILL this sketch is photographed as, into a canvas the host
   *  has already sized and scaled.
   *
   *  It is a seam rather than a repaint because the two runtimes make a
   *  plate differently, and both ways are load-bearing. A runtime whose
   *  drawing is resolution-independent RE-RENDERS here — a texture bake
   *  re-runs at the capture scale rather than being upsampled — so it
   *  draws one more frame at the larger size. A runtime whose plate IS
   *  the frame it just finished presents that. Either way the result is
   *  a function of the declared moment and of nothing a machine
   *  decides. */
  virtual void still(SkCanvas& canvas) = 0;

  /** How much larger than its declared canvas a still of this sketch is
   *  worth taking. A host clamps it against a pixel ceiling: sketches
   *  differ widely in canvas width, and doubling an already-wide one
   *  produces a plate too large to actually look at. */
  [[nodiscard]] virtual float oversample() const { return 1.0f; }

  /** Run the body's declaration again — what a host does when a file the
   *  sketch reads changed underneath it. A runtime whose body declares
   *  nothing up front has nothing to do. */
  virtual void redeclare() {}

  /** How the last frame split. Zero until the first one lands. */
  [[nodiscard]] virtual Timing timing() const = 0;

  /** What the last frame spent, lane by lane, in the runtime's own
   *  words. Empty for a runtime that keeps no such breakdown. */
  [[nodiscard]] virtual std::span<const Lane> lanes() const { return {}; }

  /** One line of live counters for a status bar — what the runtime
   *  currently holds rather than what it just spent. Empty for a
   *  runtime with none. */
  [[nodiscard]] virtual std::string counters() const { return {}; }

  /** Hold off (or restore) any re-baking the runtime does BY MEASURED
   *  COST rather than by declaration. It is the one behaviour a
   *  byte-identity sweep must turn off: load can tip a measured cost
   *  either way, so with it on a plate's bytes depend on how busy the
   *  machine was. A runtime that promotes nothing ignores this. */
  virtual void setAutoPromotion(bool on) { (void)on; }

  /** Attribute per-node cost on the frames that follow. It costs
   *  something to collect, so a host turns it on for one frame rather
   *  than for a measured run. */
  virtual void setProfiling(bool on) { (void)on; }

  /** The most expensive things the last profiled frame did, at most
   *  @p limit of them, already written out in the runtime's own words —
   *  what a failed frame-time verdict prints so an author has a next
   *  move. Empty for a runtime that cannot attribute cost. */
  [[nodiscard]] virtual std::vector<std::string> costs(size_t limit) const {
    (void)limit;
    return {};
  }

  /** Whether this runtime has a viewpoint a host can take hold of —
   *  what a host reads to decide whether to offer the control at all. */
  [[nodiscard]] virtual bool hasViewpoint() const { return false; }

  /** Move it: yaw and pitch in degrees about the scene's centre, and a
   *  distance from it. A runtime with no viewpoint ignores it. */
  virtual void viewpoint(float yawDeg, float pitchDeg, float distance) {
    (void)yawDeg;
    (void)pitchDeg;
    (void)distance;
  }
};

}  // namespace sigil::sketch
