/** @file
 * Encoding the selection into one vertical montage.
 */

#include "VideoLane.h"

#include <sigilsketch/core/Crash.h>
#include <sigilsketch/plate/Story.h>

#include <cstdio>

#include "Arguments.h"
#include "Startup.h"

namespace sketch = sigil::sketch;

int runVideo(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup) {
  sketch::StoryOptions options = args.storyOptions;
  options.only = chosen;
  options.kind = args.kind;
  // `--gpu` FIRST, exactly as the sweep tests it: a montage of a set
  // needs the device its materials run in, and a run that did not ask
  // for one must not bring it up as a side effect of the test. A
  // selection that holds a set and did not ask is REFUSED rather than
  // encoded on the CPU mesh executor: a set is lit by the device
  // renderer, so the cut under that sketch's name would be a picture
  // no recipe ran in.
  if (selectionNeedsDevice(chosen, args.kind)) {
    if (!args.gpu) {
      std::fprintf(stderr,
                   "--video: this selection holds a set, which is lit on "
                   "the device; pass --gpu or narrow the selection with "
                   "--kind\n");
      return 2;
    }
    if (!useDevice()) return 1;
  }
  SharedWebEngineScope sharedWebEngine;
  sketch::installCrashReporter({});
  finishMaterialWarmup(materialWarmup);
  const int result = sketch::story(options, fonts(), assets());
  sharedWebEngine.shutdown();
  releaseDevice();
  return result;
}
