/** @file
 * `penrose_paving`, registered into the bench binary under its own stem
 * exactly as the sketch library registers it into a host, so the pane
 * bench draws the registry's scene rather than a copy of it. The
 * registration macro chooses its form at include time, so the key is
 * named before the sketch is included.
 */

#define SIGIL_SKETCH_STATIC "penrose_paving"

#include "penrose_paving/penrose_paving.cpp"
