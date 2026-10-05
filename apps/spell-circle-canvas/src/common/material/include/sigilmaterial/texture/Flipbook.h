#pragma once

/** @file
 * @ingroup material-texture
 *
 * THE FLIPBOOK: a sheet of frames laid out in a grid, played as a
 * material — the way an effects tool hands over a flame, a burst or a
 * puff of smoke. A sheet carries no word about its grid, so the material
 * is told it; which frame stands is a pure function of the frame's clock,
 * or of a frame or a progress a sketch binds, and of the values it was
 * made with.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmotion/values/Animatable.h>

#include <optional>
#include <string>

namespace sigil::material {

/** How a sheet plays. */
struct FlipbookOptions {
  /** The grid, read row by row from the top left. */
  int columns = 8;
  int rows = 8;
  /** How many of the cells are frames; zero is every cell. */
  int frames = 0;
  /** How fast the clock moves through them. */
  float framesPerSecond = 30;
  /** Past the last frame: back to the first, or held on the last. */
  bool loop = true;
  /** Between two frames, mix the two cells by how far between them the
   *  clock stands, rather than showing the first until the second
   *  replaces it. */
  bool blendFrames = false;
  /** The frame the clock starts from — an offset that lets two copies of
   *  one effect stand out of step. */
  float startFrame = 0;
  /** THE FRAME SHOWN, bound in place of the clock: a number of frames,
   *  fractional between two, which a sketch or an instance drives. */
  std::optional<motion::Animatable<float>> frame;
  /** …or how far through the whole run, from zero at the first frame to
   *  one past the last, bound in place of the clock. `frame` wins where
   *  both are bound. */
  std::optional<motion::Animatable<float>> progress;
  /** THE FRAME SHOWN BY EACH INSTANCE: the name of a scalar every
   *  instance of instanced geometry carries — an age, a cell — read as a
   *  number of frames, so each copy of a stamp shows its own. It wins over
   *  `frame`, `progress` and the clock wherever the body is instanced and
   *  its instances carry the name; anywhere else they decide as before. */
  std::string frameAttribute;
  /** A SECOND SHEET OF THE SAME GRID whose red and green say how each
   *  pixel moves from its frame to the next — along u and along v, in the
   *  units of one cell, each encoded from zero to one around one half. With
   *  `blendFrames`, each of the two reads is pushed along that motion
   *  before they are mixed: the first forward by how far between the
   *  frames the clock stands, the second back by the rest. It is data, and
   *  is read as data whatever it says. */
  std::optional<Texture> motionVectors;
  /** What the motion is multiplied by: the tools that write motion
   *  vectors scale them differently, so it is set by eye. */
  float motionStrength = 1;
};

/** @p sheet played as a flipbook, as a material: a base, a layer, a mask
 *  or a surface channel, on a page or on a body. Each cell covers the
 *  region it paints — the whole of a mesh's uv square, the whole of a
 *  node's box. The sheet is read with no smaller copies unless it says
 *  otherwise, since a smaller copy of a sheet mixes neighbouring cells. */
Material flipbook(Texture sheet, FlipbookOptions options = {});

}  // namespace sigil::material
