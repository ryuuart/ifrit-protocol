#pragma once

/** @file
 * The study harness: one 3D scene, stepped to a declared moment on the
 * CPU and photographed. A study is what "what does this look like" is
 * answered with, and what the plate ledger's 3D tier hashes.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilworld/element/Element.h>

#include <functional>
#include <span>
#include <string>

namespace sigil::world::testing {

/** ONE STUDY: what it is called, the plate it renders into, the moment
 *  it is photographed at, and the tree it describes at a given scene
 *  time.
 *
 *  `describe` is a pure function of the scene time and nothing else,
 *  which is what makes a plate reproducible: the harness steps from zero
 *  at a fixed 1/60 and photographs the declared moment, so the image
 *  depends on the declaration and never on how fast the machine ran. */
struct Study {
  std::string name;
  SkISize canvas{900, 640};
  float captureSeconds = 1.0f;
  SkColor4f background{0.04f, 0.045f, 0.06f, 1.0f};
  /** The viewpoint, unless the tree declares one of its own. */
  Camera camera;
  std::function<Element(float seconds)> describe;
};

/** Steps @p study to its declared moment and draws the last frame
 *  through the CPU mesh runtime. */
SkBitmap render(const Study& study);

/** `render()`, written to `<outDir>/study_<name>.png`. */
bool capture(const Study& study, const std::string& outDir);

/** THE HARNESS ENTRY, spelled the way the gallery's headless mode is:
 *
 *      world_studies --headless <outdir> [--study <name>]
 *      world_studies --headless <outdir> --list-studies
 *
 *  `--study` takes a case-insensitive substring and renders just that
 *  one, which is the loop for visual iteration; `--list-studies` prints
 *  the registry one name per line, which is what the plate ledger reads.
 */
int runStudies(std::span<const Study> studies, int argc, char* argv[]);

}  // namespace sigil::world::testing
