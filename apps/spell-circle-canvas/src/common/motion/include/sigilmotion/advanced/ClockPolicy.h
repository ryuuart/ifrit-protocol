#pragma once

/** @file
 * @ingroup motion-clock
 *
 * Who moves an engine's clock: the wall, a caller's stated steps, or
 * nobody. The host's word — a sketch never spells it.
 */

#include <cstdint>

namespace sigil::motion {

/** WHO MOVES THE CLOCK. The one choice a run is made repeatable by: a
 *  host that draws under anything but `Wall` draws frames whose time is a
 *  function of what it was told, never of how fast the machine ran. */
enum class ClockPolicy : uint8_t {
  /** The wall clock: a frame moves by the time that passed since the
   *  last, held and time-scaled. What a person watching a window sees. */
  Wall,
  /** The caller's clock: a frame the engine takes on its own moves
   *  nothing, and only a stated step does. */
  Advance,
  /** Nobody's: no frame moves, a stated step included. */
  Pause,
  /** The wall clock, except that a frame drawn while something the run
   *  asked for is still arriving moves nothing. */
  PauseWhileLoading,
};

}  // namespace sigil::motion
