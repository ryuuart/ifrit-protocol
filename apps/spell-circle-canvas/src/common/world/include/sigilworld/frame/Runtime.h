#pragma once

/** @file
 * The seam a pass executes through, as a VALUE. `Runtime` holds an
 * `Executor` — the one operation a pass is performed by — so the same
 * Frame, the same passes and the same declarations run on whichever
 * executor the caller carries. One executor ships here, the CPU one; a
 * feature that owns a device supplies its own as a value, which keeps
 * every dependency pointing down and duplicates no file.
 *
 * What an executor must reproduce: a geometry pass paints the bodies its
 * realisation leaves it into the first image it writes, from the view's
 * camera and under the view's lights; a compute pass cooks its chain
 * into the point set it writes; a post pass applies its op to the images
 * it reads and writes the result. A pass carrying a body runs that body
 * instead, given the view and the targets.
 */

#include <sigilcore/reconcile/Erased.h>
#include <sigilworld/frame/Pass.h>
#include <sigilworld/frame/Targets.h>
#include <sigilworld/frame/View.h>

#include <utility>

namespace sigil::world {

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
};

/** The executor a frame runs on, carried as a comparable value. */
class Runtime : public core::Erased<Executor> {
 public:
  using core::Erased<Executor>::Erased;
  Runtime() = default;
  Runtime(core::Erased<Executor> erased)  // NOLINT: a Runtime IS its value
      : core::Erased<Executor>(std::move(erased)) {}

  /** The built-in executor: everything on the CPU, painting into raster
   *  surfaces. Every call returns the same value, so two frames that did
   *  not name a runtime compare equal on it. */
  static Runtime cpu();
};

}  // namespace sigil::world
