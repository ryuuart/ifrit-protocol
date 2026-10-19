#pragma once

/** @file
 * @ingroup sketch-scene3d
 *
 * A kind that can be told the device runtime its sessions draw a world
 * frame through.
 */

#include <sigilsketch/core/Kind.h>

namespace sigil::world {
class Runtime;
}

namespace sigil::sketch {

/** A KIND THAT OPENS ON A DEVICE RUNTIME IT CAN BE TOLD, rather than on
 *  the one the process installed.
 *
 *  A scene3d kind draws every frame through one, a scene2d kind bakes its
 *  world frames on one, and a kind a host makes of its own — a Python
 *  sketch — does whichever of the two its body does. Each answers here,
 *  so `onRuntime` reaches every kind that draws through a runtime, and a
 *  kind that draws through none is simply not one of these. */
class OpensOnRuntime {
 public:
  virtual ~OpensOnRuntime() = default;
  /** This kind, its sessions opening on @p runtime: an empty one is no
   *  device, so a kind that needs one is then unavailable. */
  [[nodiscard]] virtual Kind opensOn(const world::Runtime& runtime) const = 0;
};

}  // namespace sigil::sketch
