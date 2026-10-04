/** @file
 * The flags anchor: the exact header surface a sketch sees, compiled
 * inside the real target graph — so this translation unit's entry in the
 * compilation database IS the ground truth for how to compile a sketch.
 * The build lifts that command into the flags file the live host hands
 * the compiler, and nothing about sketch compilation is maintained by
 * hand. It also guarantees both preludes compile even when no sketch is
 * open.
 */

#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/set/Set.h>

namespace sigil::sketch {

/** Referenced by nothing; exists so this is a real translation unit. */
void anchor() {}

}  // namespace sigil::sketch
