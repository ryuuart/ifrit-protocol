/** @file
 * Rendering the selection headless, one plate each.
 */

#include "SweepLane.h"

#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/plate/Graphite.h>
#include <sigilsketch/plate/Sweep.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <cstdio>
#include <exception>
#include <optional>
#include <print>

#include "Arguments.h"
#include "Inspection.h"
#include "PipelineStore.h"
#include "PipelineWarm.h"
#include "Startup.h"

namespace sketch = sigil::sketch;

int runSweep(const Arguments& args, int chosen,
             std::future<sigil::material::WarmupResult>& materialWarmup) {
  sketch::SweepOptions options = args.sweepOptions;
  options.only = chosen;
  options.kind = args.kind;
  options.gpu = args.gpu;
  // `--at` names the moment for a sweep as it does for a still: every
  // plate is taken there instead of at its sketch's own moment.
  options.at = args.capture.at;
  // `--gpu` BRINGS THE ONE DEVICE UP, whatever the selection holds.
  // A scene3d sketch is rendered by the runtime installed on it; a scene2d
  // sketch is photographed on a Graphite surface allocated from that same
  // device's context, so there is one device in the process and not
  // two that cannot read each other's textures. WITHOUT the flag no device comes
  // up and every scene3d sketch is listed unavailable and skipped, saying
  // why.
  // A SWEEP ON THE DEVICE DRAWS THE PROGRAMS AN OPEN WINDOW DRAWS, with
  // nobody waiting on any of them, so what it built is worth writing
  // down for a machine that has nothing written down yet. The
  // declaration has to stand before the first Graphite context does and
  // after the bodies it names are compiled, which is why the material
  // warm-up is joined here rather than after the device comes up.
  //
  // ONLY WHERE A DEVICE WAS ASKED FOR. A sweep with no `--gpu` builds no
  // device program at all, so declaring the bodies, walking every stock
  // recipe's program and installing a reporter would be a warm-up for a
  // store this lane can neither read nor write.
  finishMaterialWarmup(materialWarmup);
  if (args.gpu)
    openPipelineWarmup(pipelines::storeDirectory(), args.namePipelines);
  if (args.gpu && !useDevice()) return 1;
  const sigil::skia::GraphiteContext* graphite = sketch::deviceGraphite();
  if (graphite && graphite->context()) {
    recordPipelinesForAColdStore(*graphite->context());
    // Each sketch's programs are written down under the key a window
    // opens it by.
    options.opening = [](const sketch::Entry& entry) {
      pipelinesDrawnFor(entry.key);
    };
  }
  SharedWebEngineScope sharedWebEngine;
  // A SWEEP HAS A GUEST TOO, and it has a hundred of them in one
  // process: without the reporter a faulting sketch takes the run down
  // with a bare signal, and the only thing left saying which sketch it
  // was is whatever the one before it happened to print. There is no
  // one file to name here — the sweep names the entry it is on.
  sketch::installCrashReporter({});
  // `--inspect` ON A SWEEP answers `host` and `registry` between sketches,
  // on the thread the sweep draws on; with no client attached a dispatch
  // runs no handler, and nothing a plate holds depends on it.
  std::optional<Inspection> inspection;
  if (args.inspectPort) {
    inspection.emplace(*args.inspectPort,
                       sketch::CatalogSources{SIGIL_SKETCH_DIR, {}, {}});
    options.betweenSketches = [&inspection] { inspection->advance(); };
  }
  int result = 1;
  try {
    result = sketch::sweep(options, fonts(), assets());
  } catch (const std::exception& error) {
    std::println(stderr, "sketch sweep failed: {}", error.what());
  }
  inspection.reset();
  sharedWebEngine.shutdown();
  releaseDevice();
  // Last, so a program built on the way out is in the set too.
  finishPipelineWarmup();
  return result;
}
