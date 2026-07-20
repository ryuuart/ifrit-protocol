// The flags anchor: this TU compiles the exact header surface a sketch
// sees, inside the real target graph — so its entry in
// compile_commands.json IS the ground truth for how to compile a
// sketch. ExtractSketchFlags.cmake lifts that command into
// sketch_flags.rsp at build time; nothing about sketch compilation is
// hand-maintained. (It also guarantees the sketch prelude always
// compiles, even if no sketch is open.)

#include <sigilsketch/Sketch.h>

namespace sigil::compose::sketch {

/** Referenced by nothing; exists so this TU is a real compile unit. */
void sketchAnchor() {}

} // namespace sigil::compose::sketch
