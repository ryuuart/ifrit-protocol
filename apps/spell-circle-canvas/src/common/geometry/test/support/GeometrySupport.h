#pragma once

/** @file
 * Helpers shared by more than one geometry test binary: an OBJ cube with
 * its material, and a two-triangle quad with a known winding.
 */

#include <vector>

#include "sigilgeometry/mesh/Mesh.h"

// Helpers shared by more than one geometry test binary. Each binary is one
// translation unit over one subsystem, so anything two of them read lives
// here rather than in either of them.

namespace sigil::geometry::test {

constexpr const char* kCubeObj = R"(mtllib cube.mtl
o Cube
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
usemtl scarlet
f 1 2 3 4
f 5 8 7 6
f 1 5 6 2
f 2 6 7 3
f 3 7 8 4
f 4 8 5 1
)";

constexpr const char* kCubeMtl = R"(newmtl scarlet
Kd 1 0 0
)";

/** Two triangles splitting a 100x100 square along the (-,-) to (+,+)
 *  diagonal. Front faces are wound counter-clockwise in this y-up world, and
 *  both triangles here are, so neither is dropped by backface culling.
 *
 *  Triangle 0 covers the half below the diagonal and triangle 1 the half
 *  above it; in the rendered image, with y increasing DOWNWARD on the
 *  canvas, those are the lower-right and upper-left halves respectively.
 *  Tests that sample a pixel per triangle depend on that mapping. */
inline mesh::Mesh splitQuad() {
  mesh::Mesh m;
  m.positions = {{-50, -50, 0}, {50, -50, 0}, {50, 50, 0}, {-50, 50, 0}};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

}  // namespace sigil::geometry::test
