/** @file
 * Rendering the selection headless, one plate each.
 */

#include "SweepLane.h"

#include <sigilsketch/core/Crash.h>
#include <sigilsketch/plate/Sweep.h>

#include "Arguments.h"
#include "Startup.h"

namespace sketch = sigil::sketch;

int runSweep(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup) {
  sketch::SweepOptions options = args.sweepOptions;
  options.only = chosen;
  options.kind = args.kind;
  options.gpu = args.gpu;
  // `--gpu` BRINGS THE ONE DEVICE UP, whatever the selection holds.
  // A set is rendered by the runtime installed on it; a canvas is
  // photographed on a Graphite surface allocated from that same
  // device's context, so there is one device in the process and not
  // two that cannot read each other's textures. A canvas sketch's mesh
  // painter still stays on the CPU executor whatever the flag says: a
  // plate is hashed from that executor, and the two rasterise the same
  // picture but not the same bytes.
  if (args.gpu && !useDevice()) return 1;
  SharedWebEngineScope sharedWebEngine;
  // A SWEEP HAS A GUEST TOO, and it has a hundred of them in one
  // process: without the reporter a faulting sketch takes the run down
  // with a bare signal, and the only thing left saying which sketch it
  // was is whatever the one before it happened to print. There is no
  // one file to name here — the sweep names the entry it is on.
  sketch::installCrashReporter({});
  finishMaterialWarmup(materialWarmup);
  const int result = sketch::sweep(options, fonts(), assets());
  sharedWebEngine.shutdown();
  releaseDevice();
  return result;
}
