#pragma once

/** @file
 * THE PROGRAMS THIS BACKEND DRAWS WITH, each compiled once for the
 * process, and the registration that lets a material's own body join
 * them. The mesh painter's is not among them: it draws no frame and
 * reads no material, so it stands with the mesh-render seam's device
 * executor in SigilGeometry.
 *
 * The scaffold is the text every surface is built on: a material's body
 * is appended to it and compiled when the library RUNS, because a body
 * exists only as a value in memory. The build compiles the scaffold on
 * its own, which is what makes a mistake in it a build failure.
 *
 * The compile itself, the reflected layout and the program handle are
 * SigilMaterial's Slang backend — <sigilmaterial/slang/SlangCompiler.h>.
 */

#include <sigilmaterial/slang/SlangCompiler.h>

namespace sigil::world::diligent {

/** THE SCAFFOLD ON ITS OWN, in each of its two builds. It is what a body
 *  whose material has no Slang body is drawn with — the colour the frame
 *  extracted, lit or not as the pass wants — and its unlit build is what
 *  a coverage or a variant re-draw uses. Empty when the scaffold failed
 *  to compile, which is reported once. */
const material::slang::Compiled& scaffold(bool lit);

/** THE SKY ITSELF, as one triangle over the whole target: the same
 *  scaffold text, compiled at its other pair of entry points. It is a
 *  pass rather than a body because a body would need a mesh, a placement
 *  and a depth, and a sky has none of the three. Empty when it failed to
 *  compile, which is reported once. */
const material::slang::Compiled& backdropProgram();

/** WHAT A POST PASS DOES, one program each. Each is a triangle covering
 *  the target and one fragment stage; none of them depends on a
 *  material, so all four are the same for every frame. */
struct PostPrograms {
  material::slang::Compiled copy;
  material::slang::Compiled blur;
  material::slang::Compiled levels;
  /** The second draw of a masked pass: the graded picture, reaching the
   *  one beneath it only where the coverage stands. */
  material::slang::Compiled masked;
};
const PostPrograms& postPrograms();

}  // namespace sigil::world::diligent
