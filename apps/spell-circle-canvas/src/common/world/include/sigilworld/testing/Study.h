#pragma once

/** @file
 * The study harness: one 3D scene, stepped to a declared moment on the
 * CPU and photographed. A study is what "what does this look like" is
 * answered with, and what the plate ledger's 3D tier hashes.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/frame/Runtime.h>

#include <functional>
#include <span>
#include <string>

namespace sigil::world::testing {

/** ONE STUDY: what it is called, the plate it renders into, the moment
 *  it is photographed at, and the frame it describes at a given scene
 *  time.
 *
 *  `describe` is a pure function of the scene time and nothing else,
 *  which is what makes a plate reproducible: the harness steps from zero
 *  at a fixed 1/60 and photographs the declared moment, so the image
 *  depends on the declaration and never on how fast the machine ran.
 *
 *  A study about the scene returns an Element and a Frame is made of it;
 *  a study about the passes returns the Frame itself. The harness writes
 *  the plate's size and this viewpoint into whichever it was handed, so
 *  a study states its subject and nothing about where it lands. */
struct Study {
  std::string name;
  SkISize canvas{900, 640};
  float captureSeconds = 1.0f;
  SkColor4f background{0.04f, 0.045f, 0.06f, 1.0f};
  /** The viewpoint, unless the tree declares one of its own. */
  Camera camera;
  std::function<Frame(float seconds)> describe;
};

/** Steps @p study to its declared moment and photographs the last frame.
 *
 *  An EMPTY @p runtime leaves each frame carrying its own, which is the
 *  CPU executor, and a study that declared no passes is drawn straight
 *  from what extract left. A runtime that was GIVEN is put on every
 *  frame, and a study that declared no passes is wrapped in one geometry
 *  pass that clears to its background — because an executor is only
 *  reached through passes, and a study about the scene must be able to
 *  say what it looks like on a device too. */
SkBitmap render(const Study& study, const Runtime& runtime = {});

/** `render()`, written to `<outDir>/study_<name>.png`. */
bool capture(const Study& study, const std::string& outDir,
             const Runtime& runtime = {});

/** THE HARNESS ENTRY, spelled the way the gallery's headless mode is:
 *
 *      world_studies --headless <outdir> [--study <name>] [--gpu]
 *      world_studies --headless <outdir> --list-studies
 *
 *  `--study` takes a case-insensitive substring and renders just that
 *  one, which is the loop for visual iteration; `--list-studies` prints
 *  the registry one name per line, which is what the plate ledger reads.
 *
 *  `--gpu` asks @p device for a runtime and renders every study through
 *  it. A caller that supplies no factory, or a factory that answers with
 *  an empty runtime because the machine has no device, reports that and
 *  fails rather than quietly rendering the CPU's answer under a flag
 *  that asked for the device's.
 */
int runStudies(std::span<const Study> studies, int argc, char* argv[],
               const std::function<Runtime(std::string* error)>& device = {});

}  // namespace sigil::world::testing
