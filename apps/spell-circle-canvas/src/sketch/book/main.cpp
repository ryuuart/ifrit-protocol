/** @file
 * Sketchbook: the one live application, and the one headless renderer.
 *
 *   Sketchbook [--no-gpu]                      the app, on the last sketch
 *   Sketchbook --sketch <name>                 the app, on that one
 *   Sketchbook --list [--kind canvas|set|draw] the registry, one per line
 *   Sketchbook --catalog [<file.cpp>]          the browser's rows, one JSON
 *                                              object per line
 *   Sketchbook --compare <dir-a> <dir-b>       two sweeps' plates, differenced
 *   Sketchbook --headless <outdir> [--gpu] [--sketch <name>] [--kind <k>]
 *              [--ledger] [--no-promotion | --promotion] [--composites]
 *              [--capture-at <s>]
 *              [--timing-json <path>]          plates, and the timing table
 *   Sketchbook --video <out.mp4> [--video-frames <n>] [--fps <n>]
 *              [--video-size <WxH>] [--video-bitrate <bits>]
 *              [--sketch <name>] [--kind <k>] [--gpu]
 *                                              the vertical video montage
 *   Sketchbook <file.cpp> [--frame <png>] [--bench] [--gpu]
 *                                              a file, live or measured
 *   Sketchbook <file.cpp>                      the app, on that file
 *   Sketchbook <stem>/<stem>.cpp               …either way, a sketch that
 *                                              is a directory, by its entry
 *   Sketchbook --window-bench [<sec>] [--window-size <WxH>]
 *              [--window-scale <n>] [--sketch <name>] [--kind <k>]
 *                                              the window's own frame rate
 *   Sketchbook --thumbnails [--sketch <name>] [--kind canvas|set|draw]
 *              [--thumbnail-budget <sec>] [--thumbnail-heavy]
 *                                              render missing/stale stills
 *   … [--assets <dir>]                         where res:// mounts
 *   … [--thumbnails-dir <dir>]                 the app's own thumbnail store
 *
 * `--sketch` takes a case-insensitive substring and answers to a
 * sketch's filed name or its file stem, which is the loop for visual
 * iteration.
 *
 * A HEADLESS SWEEP RENDERS WITH AUTOMATIC TEXTURE PROMOTION OFF, because
 * a plate is judged on byte identity and a cost-driven bake depends on
 * how busy the machine is. `--no-promotion` names that default so it
 * holds on a backend that would otherwise decide for itself.
 * `--promotion` opens the sessions EAGER instead: every node the
 * runtime is allowed to bake is baked from its first frame, whatever it
 * costs, so the set of nodes exercised is the scene's and identical on
 * every machine. Such a run is judged by distance from a plate, never
 * by hash. `--composites` writes a second picture beside each plate,
 * `counts_<sketch>.png`, whose grey level is how many cached rasters
 * were blitted over that pixel — the number a per-composite rounding
 * bound has to be multiplied by before it bounds a picture.
 *
 * A `.cpp` PATH IS TAKEN WHEREVER IT STANDS. The file joins the app's
 * list under its own stem and opens there, and it is compiled and
 * watched exactly as a sketch in this repository is. `--assets` names
 * the directory that mounts at `res://`; without it a sketch reads
 * `assets/` beside its own file.
 */

#include <sigilmaterial/stock/Stock.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Compare.h>
#include <unistd.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Arguments.h"
#include "FrameLane.h"
#include "SketchCatalog.h"
#include "SketchbookView.h"
#include "Startup.h"
#include "SweepLane.h"
#include "ThumbnailWarm.h"
#include "VideoLane.h"
#include "WindowBench.h"

namespace sketch = sigil::sketch;

// an uncaught exception ends the app with its message
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
  // Set before any QStandardPaths lookup — the headless warm command
  // resolves its cache directory before a QGuiApplication exists, and the
  // location is named for this app. Static setters, so no instance is
  // needed yet.
  QCoreApplication::setOrganizationDomain(QStringLiteral("sigil.dev"));
  QCoreApplication::setApplicationName(QStringLiteral("Sketchbook"));

  // Every way out of main lets the device go first: released during static
  // destruction, the textures and pipelines it holds want locks that no
  // longer exist.
  struct DeviceScope {
    ~DeviceScope() { releaseDevice(); }
  } deviceScope;

  std::optional<Arguments> parsed = parseArguments(argc, argv);
  if (!parsed) return 2;
  Arguments& args = *parsed;

  // NOTHING IS OPENED FOR A COMPARISON: it reads two directories of
  // finished plates, so it wants no fonts, no assets, no device and no
  // registry — and it answers before any of them is built.
  if (!args.compareOptions.first.empty())
    return sketch::compare(args.compareOptions);

  const int chosen = args.selected.empty() ? -1 : sketch::find(args.selected);
  if (!args.selected.empty() && chosen < 0) {
    std::fprintf(stderr, "no sketch matches \"%s\"; known sketches:\n",
                 args.selected.c_str());
    const auto& entries = sketch::registry();
    for (int i = 0; i < (int)entries.size(); ++i)
      std::fprintf(stderr, "  %2d  %-24s %s\n", i, entries[i].name,
                   entries[i].category);
    return 1;
  }

  if (args.list) {
    // Spelled the way a plate names it, because a script that selects a
    // sketch here looks for its plate under the same name.
    //
    // A SKETCH THIS MACHINE CANNOT RUN IS STILL LISTED, with what it is
    // missing after a tab — one line, two consumers. A reader sees the
    // entry greyed and the reason beside it; a script splits on the tab
    // and knows not to ask for a plate it has just been told cannot
    // exist. Dropping it from the listing would say the same thing by
    // saying nothing, which reads as a sketch that was deleted.
    const bool toTerminal = isatty(fileno(stdout)) != 0;
    const auto& entries = sketch::registry();
    for (int index : sketch::selection(-1, args.kind)) {
      const sketch::Entry& entry = entries[index];
      std::string why;
      if (entry.available(&why)) {
        std::printf("%s\n", entry.name);
        continue;
      }
      std::printf("%s%s\tunavailable: %s%s\n", toTerminal ? "\x1b[2m" : "",
                  entry.name, why.c_str(), toTerminal ? "\x1b[0m" : "");
    }
    return 0;
  }

  if (args.catalog) {
    // THE BROWSER'S ROWS WITHOUT A WINDOW: what the catalog knows about
    // every sketch before one is opened, the registry first and a file
    // this run was pointed at after it, one JSON object per line. What a
    // script reads off them is what the browser reads: a compiled-in
    // sketch's row names the runtime it draws through, and a file opened
    // by path has none until it has been built.
    int coreArgc = 1;
    const QCoreApplication core(coreArgc, argv);
    SketchCatalog::sketchDirectory = SIGIL_SKETCH_DIR;
    SketchCatalog::thumbnailDirectory.clear();  // no still is rendered here
    if (!args.sketchFile.empty()) SketchCatalog::externals = {args.sketchFile};
    const SketchCatalog rows;
    for (const QVariant& row : rows.sketches())
      std::printf("%s\n",
                  QJsonDocument(QJsonObject::fromVariantMap(row.toMap()))
                      .toJson(QJsonDocument::Compact)
                      .constData());
    return 0;
  }

  std::future<sigil::material::WarmupResult> materialWarmup =
      std::async(std::launch::async, warmStockMaterials);

  // THE WARM COMMAND renders straight through the CPU still path, exactly
  // as the browser's lazy render does, and never brings a device up.
  if (args.warmThumbnails) {
    SketchCatalog::sketchDirectory = SIGIL_SKETCH_DIR;
    // A SKETCH THAT DRAWS A PAGE NEEDS THE ONE ENGINE HERE TOO. Without
    // it `sharedEngine()` answers null and such a sketch draws the card
    // that says why it could not — which would then be written to disk
    // under the sketch's own key, as if it were the picture.
    SharedWebEngineScope sharedWebEngine;
    sketch::installCrashReporter({});
    finishMaterialWarmup(materialWarmup);
    const int result = runThumbnails(
        chosen, args.kind, thumbnailStoreDirectory(args.thumbnailDirectory),
        args.thumbnailBudget, args.thumbnailHeavy, fonts(), assets());
    sharedWebEngine.shutdown();
    return result;
  }

  if (!args.storyOptions.outputPath.empty() &&
      args.storyOptions.framesPerSketch > 0)
    return runVideo(args, chosen, materialWarmup);

  if (args.headless) return runSweep(args, chosen, materialWarmup);

  // ---- one file, live or measured -------------------------------------
  const std::filesystem::path sketchDirectory = SIGIL_SKETCH_DIR;
  // Whether the file came from the command line or was derived from a
  // registry selection: only the first is a sketch this binary was not
  // built with, and only the first joins the app's list on its own.
  const bool fileGiven = !args.sketchFile.empty();
  if (!fileGiven && chosen >= 0)
    args.sketchFile =
        sketch::sourceOf(sketchDirectory, sketch::registry()[chosen].key);

  sketch::Host::Options options;
  // DETERMINISTIC BY DEFAULT WHEN CAPTURING. A capture exists to be
  // looked at or diffed, and a sketch that draws its own bake time into
  // its own plate differs from itself between two runs — so a pixel
  // sweep reports it as changed by a patch that changed nothing. The
  // live host keeps its real numbers, which is where they are wanted.
  options.deterministic = args.deterministic.value_or(
      !args.capture.outputPath.empty() && !args.capture.bench);
  // ASSETS STAND BESIDE THE SKETCH unless `--assets` says otherwise.
  // Leaving this empty is what asks the host for that default, and it is
  // the same answer for a sketch in this repository — whose assets stand
  // beside it too — as for a file anywhere else on disk, which is what
  // makes a directory outside this checkout a place to work.
  options.assetsDirectory = args.assetsOverride;
  options.flagsFile = flagsFileNear(executableDirectory(argv[0]));

  if (!args.capture.outputPath.empty() || args.capture.bench) {
    if (args.sketchFile.empty() || !std::filesystem::exists(args.sketchFile)) {
      std::fprintf(stderr,
                   "usage: Sketchbook <sketch.cpp> [--frame <out.png>] "
                   "[--at <sec>] [--scale <n>]\n"
                   "         [--frames <count>] [--fps <n>] [--bench] "
                   "[--bench-frames <n>]\n"
                   "         [--gpu] [--jitter-dt [amplitude]] "
                   "[--deterministic | --no-deterministic]\n");
      return 2;
    }
    if (!std::filesystem::exists(options.flagsFile)) {
      std::fprintf(stderr, "missing %s (rebuild Sketchbook)\n",
                   options.flagsFile.string().c_str());
      return 2;
    }
    options.sketchPath = std::filesystem::absolute(args.sketchFile);
    // Installed before the guest can ever run: without it, a fault
    // inside a sketch is a bare signal with nothing printed.
    sketch::installCrashReporter(options.sketchPath);
    // `--gpu` PUTS THIS RUN ON THE DEVICE, exactly as it does for a
    // sweep: a set draws its frame there, and a canvas sketch's mesh
    // painter rasterises there. Fatal when the device will not come up,
    // because a run that asked for the device and quietly gave the CPU's
    // picture puts two different pictures under one name — which is the
    // one thing a capture must never do.
    if (args.gpu && !useDevice()) return 1;
    SharedWebEngineScope sharedWebEngine;
    finishMaterialWarmup(materialWarmup);
    int result = 0;
    {
      sketch::Host host(std::move(options), fonts());
      result = args.capture.bench
                   ? runBench(host, args.capture, host.sketchPath())
                   : runFrames(host, args.capture);
    }
    // The session goes before the device does: it holds textures and
    // pipelines the device made, and releasing the device first takes
    // their teardown into static destruction.
    sharedWebEngine.shutdown();
    releaseDevice();
    return result;
  }

  // ---- the app ---------------------------------------------------------
  // THE LIVE HOST DRAWS SETS ON THE DEVICE. A device is what runs a
  // material's own body: the CPU mesh executor has no compiler, so every
  // surface reaches it as the colour the frame extracted, and a reader
  // looking at a lit set in this window would be looking at a picture no
  // recipe ever ran in. Unlike the sweep's flag, a device that will not
  // come up is not fatal here — there is no plate whose name would then
  // stand over two different pictures, only a window that says which
  // tier it is showing.
  // The window's own frame-rate lane opens the window at a stated size
  // and asks Qt for a stated scale, which it can only be told before the
  // application exists.
  if (args.windowBench.scale > 0.0) {
    char factor[32];
    std::snprintf(factor, sizeof factor, "%g", args.windowBench.scale);
    setenv("QT_SCALE_FACTOR", factor, 1);
  }
  if (args.noGpu || !useDevice())
    std::fprintf(stderr,
                 "[sketchbook] sets draw on the CPU mesh executor: a "
                 "surface reaches it as the colour extract read off it\n");
  SharedWebEngineScope sharedWebEngine;
  // The build directories of runs that were killed are the process's to
  // clear, not the first sketch's: swept while the window is coming up,
  // the first host built on the render thread walks no directories under
  // the lock the live canvas draws under. THE WALK IS CLAIMED HERE, on
  // this thread and before the async starts, so a first host opened
  // before the async has run finds it taken and walks nothing.
  std::future<void> buildDirectorySweep;
  if (sketch::Host::claimSweep())
    buildDirectorySweep = std::async(
        std::launch::async, &sketch::Host::sweepAbandonedBuildDirectories);
  SketchCatalog::sketchDirectory = sketchDirectory;
  // WHERE THE BROWSER'S THUMBNAILS COME FROM: this app's own store, filled
  // on demand by a background worker and by the `--thumbnails` warm
  // command. The worker renders with the process's own font context and
  // asset store, on the CPU, so it shares no graphics context with the
  // live canvas.
  SketchCatalog::thumbnailDirectory =
      thumbnailStoreDirectory(args.thumbnailDirectory);
  SketchCatalog::thumbnailBudget = args.thumbnailBudget;
  SketchCatalog::thumbnailHeavy = args.thumbnailHeavy;
  SketchCatalog::thumbnailFonts = &fonts();
  SketchCatalog::thumbnailAssets = &assets();
  SketchbookView::fonts = &fonts();
  SketchbookView::assetsDirectory = options.assetsDirectory;
  SketchbookView::flagsFile = options.flagsFile;
  // A FILE ON THE COMMAND LINE OPENS THE WINDOW ON THAT FILE. The
  // registry is the compiled-in table and settles the first time it is
  // read, so the file joins a session-local list the app's own listing
  // reads after it, under its own stem — the dylib a hot-loaded sketch
  // exports carries neither key nor name.
  int openAt = chosen;
  if (fileGiven) {
    SketchCatalog::externals.push_back(
        std::filesystem::absolute(args.sketchFile));
    openAt = (int)sketch::registry().size();
  }
  // WHAT THE CANVAS OPENS ON, AND WHEN. Left alone, the window comes up
  // on the browser and fills in the thumbnails nothing has drawn yet,
  // opening this sketch once that is done — the machine is the fill's
  // for exactly as long as nothing is being presented. A run that named
  // a sketch, or that is here to photograph or measure one, is not
  // browsing: it opens at once and no fill starts.
  SketchCatalog::opensAt = openAt >= 0 ? openAt : 0;
  SketchCatalog::opensWithoutFill = !args.shotPath.empty() ||
                                    args.windowBench.seconds > 0.0 ||
                                    fileGiven || chosen >= 0;
  // A FRAME-RATE SWEEP MEASURES THE FRAMES AND NOTHING BESIDE THEM. The
  // browser photographs each sketch it opens for its own store, on the
  // render thread and inside a frame; here that still would be taken in
  // the middle of a stretch whose whole subject is how long a frame
  // takes. So the store is out of reach for the run, and the sessions
  // the window would otherwise keep warm behind the one on screen go as
  // the next one opens rather than in the middle of measuring it.
  if (args.windowBench.seconds > 0.0) {
    SketchCatalog::thumbnailDirectory.clear();
    SketchbookView::oneSessionAtATime = true;
  }
  sketch::installCrashReporter(args.sketchFile.empty() ? sketchDirectory
                                                       : args.sketchFile);

  QGuiApplication application(argc, argv);

  finishMaterialWarmup(materialWarmup);

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Sigil.Sketchbook", "Main");

  const QObjectList& roots = engine.rootObjects();
  auto* window =
      roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
  QObject* view = nullptr;
  if (window)
    for (QObject* child : window->findChildren<QObject*>())
      if (child->property("sketchIndex").isValid() &&
          child->property("metrics").isValid()) {
        view = child;
        break;
      }

  if (args.windowBench.seconds > 0.0) {
    if (!window || !view) {
      std::fprintf(stderr, "--window-bench: no window to present in\n");
      return 1;
    }
    window->resize(args.windowBench.width, args.windowBench.height);
    window->raise();
    window->requestActivate();
    if (!startWindowBench(application, *window, *view, args.windowBench,
                          windowBenchSelection(chosen, args.kind)))
      return 1;
  }

  if (!args.shotPath.empty()) {
    if (!window || !view) {
      std::fprintf(stderr, "--shot: no window to grab\n");
      return 1;
    }
    // Drive real frames rather than waiting for them. An unfocused
    // window gets no render loop from the compositor, so a single
    // delayed grab catches a sketch that has not started — grabWindow()
    // is what makes the thing run. Marking the item dirty first is the
    // part that is easy to miss: without it the grab re-renders the
    // existing scene-graph node and never synchronizes, so the metrics
    // panel keeps showing what it had before anything was activated.
    auto* warm = new QTimer(&application);
    auto framesLeft = std::make_shared<int>(90);
    auto patience = std::make_shared<int>(900);
    warm->setInterval(16);
    QObject::connect(
        warm, &QTimer::timeout, &application,
        [window, view, shotPath = args.shotPath, warm, framesLeft, patience] {
          if (auto* item = qobject_cast<QQuickItem*>(view)) item->update();
          // A SKETCH THIS BINARY DOES NOT CARRY HAS TO BE BUILT TO BE
          // SEEN, which takes longer than the warm-up does. So the
          // warm-up does not begin until something is live: otherwise a
          // grab of a file opened by path is always a picture of a
          // window compiling. The patience is bounded, because a file
          // that will never compile still has to be photographed —
          // the error overlay is what there is to look at.
          bool live = false;
          {
            QMutexLocker lock(&SketchbookView::hostMutex);
            live = SketchbookView::host && SketchbookView::host->live();
          }
          if (!live && --*patience > 0) {
            window->grabWindow();
            return;
          }
          if (--*framesLeft > 0) {
            window->grabWindow();
            return;
          }
          warm->stop();
          const QImage image = window->grabWindow();
          if (image.isNull() ||
              !image.save(QString::fromStdString(shotPath), "PNG")) {
            std::fprintf(stderr, "--shot: grab failed\n");
            QCoreApplication::exit(1);
            return;
          }
          std::printf("wrote %s (%dx%d)\n", shotPath.c_str(), image.width(),
                      image.height());
          QCoreApplication::quit();
        });
    warm->start();
  }
  const int status = QGuiApplication::exec();
  // The device outlives every frame that used it and must go before the
  // process does — and before it, whatever still holds textures it made:
  // released after its own queue, those take their teardown into static
  // destruction, where the locks they want no longer exist.
  //
  // THE BACKGROUND STILL IS THE FIRST THING ENDED, because it is the one
  // thing still running: the QML engine is destroyed after this function
  // returns, so a worker left to its own destructor would be walking a
  // sketch while everything below is let go.
  for (QObject* root : engine.rootObjects())
    for (SketchCatalog* browser : root->findChildren<SketchCatalog*>())
      browser->stopThumbnails();
  {
    QMutexLocker lock(&SketchbookView::hostMutex);
    SketchbookView::sessions.clear();
    SketchbookView::host = nullptr;
  }
  sharedWebEngine.shutdown();
  releaseDevice();
  return status;
}
