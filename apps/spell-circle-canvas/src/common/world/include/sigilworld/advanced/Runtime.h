#pragma once

/** @file
 * @ingroup world-frame
 * The seam a pass executes through, as a VALUE. `Runtime` holds an
 * `Executor` — the one operation a pass is performed by — so the frame,
 * the ordering and the retained scene name no device and link none. The
 * executor that draws is the device's, which the feature that owns a
 * device supplies as a value; a frame carrying no runtime runs none of
 * its passes and says so.
 */

#include <sigilcore/comparable/Erased.h>
#include <sigilworld/advanced/View.h>
#include <sigilworld/frame/Pass.h>

#include <utility>

namespace sigil::world {

/** The resources a frame's passes write and read, declared with the
 *  renderer they are surfaces of, in `advanced/Skia.h`. */
class Targets;

/** The one operation a pass is performed by. An implementation owns
 *  whatever device it needs; the arguments carry none, so a frame
 *  declared for one executor runs on any of them. */
class Executor {
 public:
  Executor() = default;
  Executor(const Executor&) = default;
  Executor(Executor&&) = default;
  Executor& operator=(const Executor&) = default;
  Executor& operator=(Executor&&) = default;
  virtual ~Executor() = default;

  /** Perform @p work — the pass and what the ordering decided about it
   *  — over @p view, into @p targets. */
  virtual void execute(const PassWork& work, const View& view,
                       Targets& targets) const = 0;

  /** Called once before a frame's steps. An executor holding resources
   *  of its own sizes them to the targets' extent here, and makes what
   *  the frame before wrote into what this frame's `previous()` names —
   *  which cannot wait until the frame ends, because between the last
   *  pass and the next frame is exactly when the picture is presented
   *  and a readback is taken. */
  virtual void beginFrame(Targets&) const {}
  /** Called once after the steps, and after everything the frame read
   *  back has been taken. An executor holding resources of its own lets
   *  go of what the frame stopped needing here. */
  virtual void endFrame(Targets&) const {}
};

/** The executor a frame runs on, carried as a comparable value. An
 *  empty one is no executor at all: a frame carrying it describes and
 *  extracts, and runs none of its passes. */
class Runtime : public core::Erased<Executor> {
 public:
  using core::Erased<Executor>::Erased;
  Runtime() = default;
  /** A runtime standing for @p erased, which IS the executor it holds. */
  Runtime(core::Erased<Executor> erased)  // NOLINT: a Runtime IS its value
      : core::Erased<Executor>(std::move(erased)) {}
};

}  // namespace sigil::world
