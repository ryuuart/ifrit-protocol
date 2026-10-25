/** @file
 * Grimoire: the one live application, and the one headless renderer.
 *
 *   Grimoire [--no-gpu]                      the Welcome screen
 *   Grimoire --sketch <name>                 the app, on that one
 *   Grimoire --list [--kind scene2d|scene3d] [--gpu]
 *                                              the registry, one per line
 *   Grimoire --catalog [<file.cpp>] [--gpu]  the browser's rows, one JSON
 *                                              object per line
 *   Grimoire --python-info                  Python ABI as JSON
 *   Grimoire --workspace <directory>        a saved folder selection
 *   Grimoire --examples                     the bundled catalogue
 *   Grimoire --compare <dir-a> <dir-b>       compare two plate sweeps
 *   Grimoire --headless [<outdir>] [--gpu] [--sketch <name>]
 *              [--kind <k>] [--at <s>]
 *              [--ledger]
 *                                              plates, and the timing table;
 *                                              into sketch_plates/ unless a
 *                                              directory is named
 *   Grimoire --video <out.mp4> [--video-frames <n>] [--fps <n>]
 *              [--video-size <WxH>] [--video-bitrate <bits>]
 *              [--sketch <name>] [--kind <k>] [--gpu]
 *                                              the vertical video montage
 *   Grimoire <file.cpp> --frame <png> [--at <s>] [--scale <n>]
 *              [--frames <count>] [--fps <n>] [--gpu]
 *              [--deterministic | --no-deterministic]
 *                                              a file, photographed; the
 *                                              pair names the clock policy
 *                                              its session is opened for:
 *                                              Advance (the default) or
 *                                              the wall's
 *   Grimoire <file.cpp>                      the app, on that file
 *   Grimoire <stem>/<stem>.cpp               …either way, a sketch that
 *                                              is a directory, by its entry
 *   Grimoire --window-bench [<sec>] [--window-size <WxH>]
 *              [--window-scale <n>] [--sketch <name> ...] [--kind <k>]
 *                                              the window's own frame rate
 *   Grimoire --thumbnails [--sketch <name>] [--kind scene2d|scene3d]
 *              [--thumbnail-budget <sec>] [--thumbnail-heavy]
 *                                              render missing/stale stills
 *   Grimoire --publish [<name>] …             the window's frames, offered
 *                                              to other applications
 *   Grimoire --shot <png> …                  the whole window, browser and
 *                                              inspector included, once the
 *                                              sketch on screen is live
 *   … [--at <sec>] in a window run             the canvas held at that
 *                                              moment
 *   … [--assets <dir>]                         where res:// mounts
 *   … [--state <dir>]                          where this run keeps builds,
 *                                              thumbnails, recorded device
 *                                              programs and settings
 *   … [--pipeline-names]                       each device program named
 *                                              on stderr as it is built or
 *                                              replayed
 *   … [--inspect[=<port>]]                     the protocol's endpoint on
 *                                              loopback, on the port named
 *                                              or any free one; the window
 *                                              mounts it unasked, a sweep
 *                                              only when asked, and every
 *                                              other lane refuses it
 *   Grimoire --headless --inspect[=<port>] [--state <dir>]
 *                                              no window and no plates: a
 *                                              host a client drives over
 *                                              the protocol until it is
 *                                              interrupted
 *   … [--repl]                                 the evaluate door: the
 *                                              window, or a headless
 *                                              --inspect host, runs any
 *                                              code a client on this
 *                                              machine sends inside the
 *                                              running sketch
 *   … --python-executable <path> --python-abi <abi>
 *                                              the Python environment
 *
 * `--sketch` takes a case-insensitive substring and answers to a
 * sketch's filed name or its file stem, which is the loop for visual
 * iteration.
 *
 * `--publish` OFFERS THE LIVE WINDOW'S FRAMES to other applications on
 * this machine, under a name they subscribe to — the one given, or the
 * stem of the sketch the run opens on. The window turns it on and off
 * while it runs; every other lane here renders without a window and
 * refuses the flag rather than accepting one that would do nothing.
 *
 * A `.cpp` PATH IS TAKEN WHEREVER IT STANDS. The file joins the app's
 * list under its own stem and opens there, and it is compiled and
 * watched exactly as a sketch in this repository is. `--assets` names
 * the directory that mounts at `res://`; without it a sketch reads
 * `assets/` beside its own file.
 */

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilsketch/core/Crash.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/core/State.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/plate/Compare.h>
#include <sigilsketch/python/Python.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <unistd.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QMutex>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QSettings>
#include <QtCore/QSysInfo>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

#include "Arguments.h"
#include "CatalogRowMap.h"
#include "Conditions.h"
#include "FrameLane.h"
#include "GrimoireView.h"
#include "Inspection.h"
#include "PipelineStore.h"
#include "PipelineWarm.h"
#include "PythonEnvironment.h"
#include "ServeLane.h"
#include "SketchActions.h"
#include "SketchCatalog.h"
#include "Startup.h"
#include "SweepLane.h"
#include "ThumbnailWarm.h"
#include "VideoLane.h"
#include "WindowBench.h"
#include "WindowCapture.h"
#include "WindowEvaluation.h"
#include "WindowInput.h"
#include "Workspace.h"

namespace sketch = sigil::sketch;

namespace {

/** How often the window's event loop answers the protocol: within a frame
 *  or two of a request, and nothing when no client is attached. */
constexpr int kInspectionPumpMilliseconds = 30;

/** THE SESSION THE WINDOW SHOWS, as `host.describe` lists it, read under
 *  the lock the window's render thread draws under. */
Inspection::Sessions windowSessions() {
  return [] {
    std::vector<OpenSession> open;
    QMutexLocker lock(&GrimoireView::hostMutex);
    const sketch::Host* host = GrimoireView::host;
    if (!host || !host->live()) return open;
    OpenSession session;
    session.sketch = host->sketchPath().string();
    session.kind = std::string(host->kind());
    session.width = host->canvasSize().width();
    session.height = host->canvasSize().height();
    session.moment = host->captureSeconds();
    open.push_back(std::move(session));
    return open;
  };
}

}  // namespace

// an uncaught exception ends the app with its message
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
  // Set before any QStandardPaths lookup — the headless warm command
  // resolves its cache directory before a QGuiApplication exists, and the
  // location is named for this app. Static setters, so no instance is
  // needed yet.
  QCoreApplication::setOrganizationDomain(QStringLiteral("sigil.dev"));
  QCoreApplication::setApplicationName(QStringLiteral("Grimoire"));

  std::optional<Arguments> parsed = parseArguments(argc, argv);
  if (!parsed) return 2;
  Arguments& args = *parsed;

  // ONE ROOT FOR EVERYTHING THIS RUN WRITES FOR A LATER ONE, named before
  // anything reads its settings or opens a store: the builds, the
  // thumbnails and the recorded device programs each take a directory
  // under it, and the settings and recents move from the platform's own
  // store into a file there, so a run that names a root reads nothing an
  // earlier run left.
  if (!args.stateDirectory.empty()) {
    std::error_code error;
    std::filesystem::create_directories(args.stateDirectory, error);
    if (error) {
      std::println(stderr, "--state: cannot use {}: {}",
                   args.stateDirectory.string(), error.message());
      return 2;
    }
    const std::filesystem::path root =
        std::filesystem::weakly_canonical(args.stateDirectory);
    sketch::setStateDirectory(root);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QString::fromStdString((root / "settings").string()));
  }

  // The launcher's handshake and environment selection precede every registry
  // probe: checking an optional Python module can initialize the interpreter.
  const auto pythonAbi = sketch::python::interpreterAbi();
  if (args.pythonInfo) {
    const auto [major, minor] = sketch::python::interpreterVersion();
    const QJsonObject info{
        {QStringLiteral("implementation"), QStringLiteral("cpython")},
        {QStringLiteral("version"), QJsonArray{major, minor}},
        {QStringLiteral("soabi"),
         QString::fromUtf8(pythonAbi.data(), (qsizetype)pythonAbi.size())},
        {QStringLiteral("machine"), QSysInfo::currentCpuArchitecture()},
        {QStringLiteral("pointer_bits"), (int)(sizeof(void*) * 8)},
    };
    std::println(
        "{}", QJsonDocument(info).toJson(QJsonDocument::Compact).constData());
    return 0;
  }

  const bool windowRun =
      !args.list && !args.catalog && !args.headless && !args.warmThumbnails &&
      args.compareOptions.first.empty() &&
      args.storyOptions.outputPath.empty() && args.capture.outputPath.empty();
  const bool remembersWindow =
      windowRun && args.shotPath.empty() && args.windowBench.seconds <= 0;
  QSettings workspaceSettings;
  grimoire::WorkspaceHistory history(workspaceSettings);
  std::vector<std::filesystem::path> workspaceSources;
  if (!args.workspace.empty()) {
    std::error_code error;
    if (!std::filesystem::is_directory(args.workspace, error)) {
      std::println(stderr, "workspace directory is unavailable: {}",
                   args.workspace.string());
      return 2;
    }
    args.workspace = std::filesystem::weakly_canonical(args.workspace);
    workspaceSources = grimoire::workspaceFiles(args.workspace);
    if (args.sketchFile.empty() && args.selected.empty())
      args.sketchFile = grimoire::workspaceEntry(
          workspaceSources, history.forRoot(args.workspace));
  }
  if (!args.sketchFile.empty()) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(args.sketchFile, error)) {
      std::println(stderr, "sketch file is unavailable: {}",
                   args.sketchFile.string());
      return 2;
    }
    args.sketchFile = std::filesystem::weakly_canonical(args.sketchFile);
    if (std::find(workspaceSources.begin(), workspaceSources.end(),
                  args.sketchFile) == workspaceSources.end()) {
      if (args.workspace.empty()) {
        workspaceSources.push_back(args.sketchFile);
      } else {
        std::println(stderr, "sketch is not an entry in this workspace: {}",
                     args.sketchFile.string());
        return 2;
      }
    }
  }

  // Environment selection precedes catalogue probes and thumbnail workers.
  // A picker supplies an already prepared interpreter; direct file opens and
  // restored workspaces resolve theirs here before any Python module runs.
  const bool examples =
      args.workspace.empty() && args.sketchFile.empty() &&
      (args.noRestore || !args.selected.empty() || args.headless ||
       args.warmThumbnails || !args.storyOptions.outputPath.empty() ||
       args.windowBench.seconds > 0);
  const bool pythonExamples =
      examples &&
      (args.selected.empty() ||
       sketch::sourceOf(SIGIL_SKETCH_DIR, args.selected).extension() == ".py");
  const bool hasPython =
      pythonExamples ||
      std::any_of(workspaceSources.begin(), workspaceSources.end(),
                  [](const auto& path) { return path.extension() == ".py"; });
  if (args.pythonExecutable.empty() && hasPython && !args.catalog &&
      !args.list) {
    const auto source =
        pythonExamples
            ? std::filesystem::path(SIGIL_SKETCH_DIR)
            : (args.workspace.empty() ? args.sketchFile : args.workspace);
    QProcess prepare;
    auto prepareEnvironment = QProcessEnvironment::systemEnvironment();
    prepareEnvironment.remove(QStringLiteral("PYTHONEXECUTABLE"));
    prepareEnvironment.remove(QStringLiteral("__PYVENV_LAUNCHER__"));
    prepare.setProcessEnvironment(prepareEnvironment);
    prepare.start(QStringLiteral(SIGIL_PYTHON_EXECUTABLE),
                  grimoire::pythonEnvironmentArguments(source));
    QString problem;
    grimoire::PythonEnvironment environment;
    if (!prepare.waitForStarted()) {
      problem = prepare.errorString();
    } else if (!prepare.waitForFinished(180000)) {
      prepare.kill();
      prepare.waitForFinished();
      problem = QStringLiteral("Preparing the Python environment timed out.");
    } else if (prepare.exitStatus() != QProcess::NormalExit ||
               prepare.exitCode() != 0) {
      problem = QString::fromUtf8(prepare.readAllStandardError()).trimmed();
      if (problem.isEmpty())
        problem = QStringLiteral("Could not prepare the Python environment.");
    } else {
      (void)grimoire::readPythonEnvironment(prepare.readAllStandardOutput(),
                                            environment, problem);
    }
    if (!problem.isEmpty()) {
      std::println(stderr, "{}", problem.toUtf8().constData());
      if (!windowRun) return 1;
      SketchActions::startupError = problem;
      args.workspace.clear();
      args.sketchFile.clear();
      workspaceSources.clear();
      args.noRestore = false;
      args.selected.clear();
    } else {
      args.pythonExecutable = environment.executable;
      args.pythonAbi = environment.abi;
    }
  }
  if (!args.pythonExecutable.empty()) {
    if (args.pythonAbi != pythonAbi) {
      std::println(stderr,
                   "--python-abi mismatch: executable declares \"{}\", "
                   "Grimoire requires \"{}\"",
                   args.pythonAbi, pythonAbi);
      return 2;
    }
    try {
      sketch::python::configureInterpreter(args.pythonExecutable);
    } catch (const std::exception& error) {
      std::println(stderr, "--python-executable: {}", error.what());
      return 2;
    }
  }

  // Every way out of a rendering run lets the device go first: released during
  // static destruction, its textures and pipelines want locks that no longer
  // exist.
  struct DeviceScope {
    ~DeviceScope() { releaseDevice(); }
  } deviceScope;

  // PUBLISHING IS THE LIVE WINDOW'S. What travels is the texture a
  // frame was drawn into, and every lane below that answers before the
  // window opens draws somewhere else or does not draw at all — so the
  // flag is refused where it could only be ignored.
  if (args.publish &&
      (args.headless || args.list || args.catalog || args.warmThumbnails ||
       !args.compareOptions.first.empty() ||
       !args.storyOptions.outputPath.empty() ||
       !args.capture.outputPath.empty())) {
    std::println(stderr,
                 "--publish: what is offered is a live window's frames, "
                 "and this run opens no window");
    return 2;
  }

  // THE PROTOCOL SERVES A HOST A CLIENT DRIVES, so a run that ends once its
  // output is written has nothing to serve: `--inspect` is refused there,
  // since a client attaching to it would find the run already gone. A
  // sweep mounts it between its sketches; the window mounts it unasked.
  // THE EVALUATE DOOR STANDS ON A SESSION a client reaches: the window's,
  // or the one `--headless --inspect` serves. A sweep and every run that
  // ends once its output is written hold none for it to open onto.
  if (args.repl &&
      (args.list || args.catalog || args.warmThumbnails ||
       !args.compareOptions.first.empty() ||
       !args.storyOptions.outputPath.empty() ||
       !args.capture.outputPath.empty() ||
       (args.headless && (!args.inspectPort || args.headlessDirectoryNamed ||
                          !args.selected.empty() || !args.kind.empty())))) {
    std::println(stderr,
                 "--repl: evaluates text inside a session a client drives — "
                 "the window's, or the one `--headless --inspect` serves — "
                 "and this run holds none");
    return 2;
  }

  if (args.inspectPort && (args.list || args.catalog || args.warmThumbnails ||
                           !args.compareOptions.first.empty() ||
                           !args.storyOptions.outputPath.empty() ||
                           !args.capture.outputPath.empty())) {
    std::println(stderr,
                 "--inspect: this run ends once its output is written, and "
                 "a client has nothing to drive; `--headless --inspect` "
                 "serves a host with no window");
    return 2;
  }

  // NOTHING IS OPENED FOR A COMPARISON: it reads two directories of
  // finished plates, so it wants no fonts, no assets, no device and no
  // registry — and it answers before any of them is built.
  if (!args.compareOptions.first.empty())
    return sketch::printComparison(sketch::compare(args.compareOptions));

  const int chosen = args.selected.empty() ? -1 : sketch::find(args.selected);
  if (!args.selected.empty() && chosen < 0) {
    std::println(stderr,
                 "no sketch matches \"{}\"; known sketches:", args.selected);
    const auto& entries = sketch::registry();
    for (int i = 0; i < (int)entries.size(); ++i)
      std::println(stderr, "  {:2}  {:<24} {}", i, entries[i].name,
                   entries[i].category);
    return 1;
  }
  // The further sketches a repeated `--sketch` named, found the same way,
  // for the window's lane to present after the first.
  std::vector<int> named;
  if (chosen >= 0) named.push_back(chosen);
  for (const std::string& further : args.alsoSelected) {
    const int index = sketch::find(further);
    if (index < 0) {
      std::println(stderr, "no sketch matches \"{}\"", further);
      return 1;
    }
    named.push_back(index);
  }

  if (args.list) {
    // THE REGISTRY'S ROWS FOR A READER, spelled the way a plate names
    // them. A script reads the same rows as values instead: in Python, or
    // as the catalog's JSON objects below.
    //
    // A SKETCH THIS MACHINE CANNOT RUN IS STILL LISTED, greyed, with what
    // it is missing after a tab. Dropping it from the listing would say
    // the same thing by saying nothing, which reads as a sketch that was
    // deleted.
    //
    // A SCENE3D SKETCH IS AVAILABLE ONLY WHERE A DEVICE IS, so `--gpu`
    // brings the one device up first and a listing without it greys every
    // scene3d sketch.
    if (args.gpu && !useDevice()) return 1;
    const bool toTerminal = isatty(fileno(stdout)) != 0;
    for (const sketch::RegistryRow& row : sketch::registryRows(args.kind)) {
      if (row.available) {
        std::println("{}", row.name);
        continue;
      }
      std::println("{}{}\tunavailable: {}{}", toTerminal ? "\x1b[2m" : "",
                   row.name, row.reason, toTerminal ? "\x1b[0m" : "");
    }
    releaseDevice();
    return 0;
  }

  if (args.catalog) {
    // THE BROWSER'S ROWS WITHOUT A WINDOW: what the catalog knows about
    // every sketch before one is opened, the registry first and a file
    // this run was pointed at after it, one JSON object per line. They are
    // `sketch::catalog`'s rows as the browser maps them, so this is also
    // how another process reads the registry as values: every entry's
    // filed name, stem, runtime and availability on this machine. A
    // scene3d sketch is available only where a device is, so `--gpu` brings the
    // one device up first, as it does for the listing above.
    if (args.gpu && !useDevice()) return 1;
    int coreArgc = 1;
    const QCoreApplication core(coreArgc, argv);
    SketchCatalog::sketchDirectory = SIGIL_SKETCH_DIR;
    SketchCatalog::thumbnailDirectory.clear();  // no still is rendered here
    SketchCatalog::externals = workspaceSources;
    SketchCatalog::workspaceRoot = args.workspace;
    const SketchCatalog rows;
    for (const QVariant& row : rows.sketches())
      std::println("{}", grimoire::catalogRowJson(row.toMap()).constData());
    releaseDevice();
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
        chosen, args.kind, thumbnailStoreDirectory(), args.thumbnailBudget,
        args.thumbnailHeavy, fonts(), assets());
    sharedWebEngine.shutdown();
    return result;
  }

  if (!args.storyOptions.outputPath.empty() &&
      args.storyOptions.framesPerSketch > 0)
    return runVideo(args, chosen, materialWarmup);

  // `--headless --inspect` WITH NOTHING TO SWEEP SERVES: no directory, no
  // sketch and no kind named, so what it is for is a client.
  if (args.headless && args.inspectPort && !args.headlessDirectoryNamed &&
      args.selected.empty() && args.kind.empty())
    return runServe(args, flagsFileNear(executableDirectory(argv[0])),
                    materialWarmup);

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
  options.pythonLoader = &sketch::python::load;
  // A STILL IS TAKEN UNDER A CLIENT'S CLOCK BY DEFAULT. A capture exists
  // to be looked at or diffed, and a sketch that draws its own bake time
  // into its own plate differs from itself between two runs — so a pixel
  // sweep reports it as changed by a patch that changed nothing. Its
  // session is opened for the Advance policy, which pins those numbers;
  // the live host keeps the wall's clock and its real numbers, which is
  // where they are wanted.
  options.clock = args.clockPolicy.value_or(
      !args.capture.outputPath.empty() ? sigil::motion::ClockPolicy::Advance
                                       : sigil::motion::ClockPolicy::Wall);
  // WHAT MOUNTS AT res:// unless `--assets` says otherwise: for a sketch
  // of this repository the demo assets root, and for a file anywhere else
  // on disk the assets beside it, which the host defaults to when this is
  // left empty — what makes a directory outside this checkout a place to
  // work. The sketches folder mounts at sketch:// either way, so a sketch
  // this binary carries reaches its own files.
  options.assetsDirectory = args.assetsOverride;
  if (options.assetsDirectory.empty() && !fileGiven)
    options.assetsDirectory = SIGIL_SKETCH_ASSET_DIR;
  options.sketchesDirectory = sketchDirectory;
  options.flagsFile = flagsFileNear(executableDirectory(argv[0]));

  if (!args.capture.outputPath.empty()) {
    if (args.sketchFile.empty() || !std::filesystem::exists(args.sketchFile)) {
      std::println(stderr,
                   "usage: Grimoire <sketch.cpp|sketch.py> [--frame <out.png>] "
                   "[--at <sec>] [--scale <n>]\n"
                   "         [--frames <count>] [--fps <n>] [--gpu] "
                   "[--deterministic | --no-deterministic]");
      return 2;
    }
    if (args.sketchFile.extension() != ".py" &&
        !std::filesystem::exists(options.flagsFile)) {
      std::println(stderr, "missing {} (rebuild Grimoire)",
                   options.flagsFile.string());
      return 2;
    }
    options.sketchPath = std::filesystem::absolute(args.sketchFile);
    // Installed before the guest can ever run: without it, a fault
    // inside a sketch is a bare signal with nothing printed.
    sketch::installCrashReporter(options.sketchPath);
    finishMaterialWarmup(materialWarmup);
    // `--gpu` PUTS THIS RUN ON THE DEVICE: a scene3d sketch draws its frame
    // there, and a scene2d sketch is photographed on a Graphite surface. A
    // sketch whose kind needs the device executor brings it up before its
    // session opens, and a device that will not come up fails the run, because
    // a capture that asked for the device and quietly gave the CPU's picture
    // puts two different pictures under one name.
    if (args.gpu) {
      options.prepareSession = [&](const sketch::Kind& kind) {
#ifdef __APPLE__
        if (!kind->needsDevice()) return;
#else
        // Geometry owns the Vulkan device Graphite adopts on this platform.
        (void)kind;
#endif
        if (!useDevice())
          throw std::runtime_error(
              "the sketch's device executor could not open");
      };
    }
    SharedWebEngineScope sharedWebEngine;
    // Sessions release their retained GPU resources before their context.
    std::unique_ptr<sigil::core::hardware::GpuDevice> canvasDevice;
    std::unique_ptr<sigil::skia::GraphiteContext> canvasGraphite;
    int result = 0;
    {
      sketch::Host host(std::move(options), fonts());
      if (!awaitFirstBuild(host)) result = 1;
#ifdef __APPLE__
      if (!result && args.gpu && !host.needsDevice()) {
        auto made = sigil::core::hardware::GpuDevice::createOwned();
        if (made) canvasDevice = std::move(*made);
        if (!canvasDevice) {
          std::println(stderr, "capture: no canvas GPU device ({})",
                       made.error().message);
          result = 1;
        } else {
          canvasGraphite = sigil::skia::GraphiteContext::create(*canvasDevice);
          if (!canvasGraphite) {
            std::println(stderr, "capture: the device has no Graphite context");
            result = 1;
          }
        }
      }
#endif
      if (!result)
        result = runFrames(host, args.capture, args.gpu, canvasGraphite.get());
    }
    // The session goes before the device does: it holds textures and
    // pipelines the device made, and releasing the device first takes
    // their teardown into static destruction.
    sharedWebEngine.shutdown();
    releaseDevice();
    return result;
  }

  // ---- the app ---------------------------------------------------------
  // THE LIVE HOST DRAWS SCENE3D SKETCHES ON THE DEVICE, which is the only
  // place one is drawn. Unlike the sweep's flag, a device that will not
  // come up is not fatal here: the scene2d sketches still run, and every
  // scene3d sketch is listed unavailable with the reason.
  // The window's own frame-rate lane opens the window at a stated size
  // and a stated device pixel ratio, which Qt can only be told before the
  // application exists. Qt's factor MULTIPLIES the display's own ratio, so
  // the factor asked for is the stated ratio over the ratio of the display
  // a new window opens on: a ratio of 2 is a factor of 1 on a Retina panel
  // and of 2 on an external display of one pixel per point.
  if (args.windowBench.scale > 0.0) {
    const double native = mainDisplayPixelRatio();
    char factor[32];
    std::snprintf(factor, sizeof factor, "%g",
                  native > 0.0 ? args.windowBench.scale / native
                               : args.windowBench.scale);
    setenv("QT_SCALE_FACTOR", factor, 1);
  }
  if (args.noGpu || !useDevice())
    std::println(stderr,
                 "[grimoire] no device: every scene3d sketch is listed "
                 "unavailable");
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
  SketchCatalog::workspaceRoot = args.workspace;
  SketchActions::workspaceRoot = args.workspace;
  SketchActions::startsAtWelcome =
      windowRun && !args.noRestore && args.workspace.empty() &&
      args.sketchFile.empty() && args.selected.empty() &&
      args.windowBench.seconds <= 0;
  SketchActions::pythonExecutable = args.pythonExecutable;
  SketchActions::pythonAbi = QString::fromStdString(args.pythonAbi);
  SketchActions::rememberSelections = remembersWindow;
  // WHERE THE BROWSER'S THUMBNAILS COME FROM: this app's own store, filled
  // on demand by a background worker and by the `--thumbnails` warm
  // command. The worker renders with the process's own font context and
  // asset store, on the CPU, so it shares no graphics context with the
  // live canvas.
  SketchCatalog::thumbnailDirectory = thumbnailStoreDirectory();
  SketchCatalog::thumbnailBudget = args.thumbnailBudget;
  SketchCatalog::thumbnailHeavy = args.thumbnailHeavy;
  SketchCatalog::thumbnailFonts = &fonts();
  SketchCatalog::thumbnailAssets = &assets();
  GrimoireView::fonts = &fonts();
  // `--at` in a window run holds the canvas there; a frame-rate run keeps
  // the wall's clock it measures.
  if (args.windowBench.seconds <= 0.0) GrimoireView::heldAt = args.capture.at;
  GrimoireView::assetsDirectory = options.assetsDirectory;
  GrimoireView::sketchesDirectory = options.sketchesDirectory;
  GrimoireView::flagsFile = options.flagsFile;
  // WHAT A SUBSCRIBER BINDS TO is the run's name and not the sketch on
  // screen: a name that followed the selection would drop every client
  // the moment something else was looked at. Without one, the sketch
  // this run opens on names it, and a run that opens on the browser
  // publishes under the application's own name.
  if (args.publish) {
    GrimoireView::publishAtStart = true;
    GrimoireView::publishName = !args.publishName.empty() ? args.publishName
                                : !args.sketchFile.empty()
                                    ? args.sketchFile.stem().string()
                                    : "Grimoire";
  }
  // A FILE ON THE COMMAND LINE OPENS THE WINDOW ON THAT FILE. The
  // registry is the compiled-in table and settles the first time it is
  // read, so the file joins a session-local list the app's own listing
  // reads after it, under its own stem — the dylib a hot-loaded sketch
  // exports carries neither key nor name.
  int openAt = chosen;
  SketchCatalog::externals = workspaceSources;
  if (fileGiven) {
    const auto selectedFile = std::find(
        workspaceSources.begin(), workspaceSources.end(), args.sketchFile);
    openAt = (int)sketch::registry().size() +
             (int)std::distance(workspaceSources.begin(), selectedFile);
  }
  // WHAT THE CANVAS OPENS ON, AND WHEN. Left alone, the window comes up
  // on the browser and fills in the thumbnails nothing has drawn yet,
  // opening this sketch once that is done — the machine is the fill's
  // for exactly as long as nothing is being presented. A run that named
  // a sketch, or that is here to photograph or measure one, is not
  // browsing: it opens at once and no fill starts.
  SketchCatalog::opensAt =
      SketchActions::startsAtWelcome
          ? -1
          : (openAt >= 0 || !args.workspace.empty() ? openAt : 0);
  SketchCatalog::opensWithoutFill =
      SketchActions::startsAtWelcome || !args.shotPath.empty() ||
      args.windowBench.seconds > 0.0 || fileGiven || !args.workspace.empty() ||
      chosen >= 0;
  // A FRAME-RATE SWEEP MEASURES THE FRAMES AND NOTHING BESIDE THEM. The
  // browser photographs each sketch it opens for its own store, and the
  // repaint and readback of that still are on the render thread and
  // inside a frame; here they would fall in the middle of a stretch
  // whose whole subject is how long a frame takes. So the store is out
  // of reach for the run, and the sessions
  // the window would otherwise keep warm behind the one on screen go as
  // the next one opens rather than in the middle of measuring it.
  if (args.windowBench.seconds > 0.0) {
    SketchCatalog::thumbnailDirectory.clear();
    GrimoireView::oneSessionAtATime = true;
  }
  sketch::installCrashReporter(args.sketchFile.empty() ? sketchDirectory
                                                       : args.sketchFile);

  QGuiApplication application(argc, argv);
  if (remembersWindow && (!args.workspace.empty() || fileGiven))
    history.remember({args.workspace, args.sketchFile});

  // THE PROTOCOL, ON BY DEFAULT: an endpoint on loopback answering `host`
  // and `registry`, dispatched from this thread's event loop. With no
  // client attached a dispatch runs no handler, so the window draws what
  // it draws without one.
  Inspection inspection(
      args.inspectPort.value_or(0),
      sketch::CatalogSources{sketchDirectory, workspaceSources, args.workspace},
      windowSessions());
  QTimer inspectionPump;
  QObject::connect(&inspectionPump, &QTimer::timeout,
                   [&inspection] { inspection.advance(); });
  if (inspection.listening()) inspectionPump.start(kInspectionPumpMilliseconds);
  // POINTER AND KEY INPUT FROM A CLIENT, reaching the sketch on screen as
  // the window's own pointer and keyboard do.
  if (inspection.listening()) mountWindowInput(inspection.dispatcher());
  // THE EVALUATE DOOR, only when asked: whatever a client on this machine
  // sends runs inside the sketch on screen. Declared before the engine,
  // so it goes after the render thread that answers it has stopped.
  std::optional<WindowEvaluation> evaluation;
  if (args.repl && inspection.listening()) {
    evaluation.emplace(inspection.dispatcher(), inspectionStateRoot());
    std::println(stderr,
                 "[grimoire] --repl: session.evaluate runs any code a client "
                 "on this machine sends inside the running sketch");
  }

  finishMaterialWarmup(materialWarmup);
  // AND THE SECOND COMPILE, DECLARED BEFORE ANYTHING CAN DRAW. The
  // warm-up above turns each stock recipe's SkSL into a program; a
  // device builds a further program per distinct DRAW, out of the whole
  // inlined paint tree, and the thread that records the draw waits for
  // it. Which of those a run needed can be written down and stood up at
  // the next launch — but only for a paint tree whose runtime effects
  // were declared before the context was created, which is here: the
  // window's context is made on the render thread, after this.
  openPipelineWarmup(pipelines::storeDirectory(), args.namePipelines);
  // A SCENE3D SKETCH'S SURFACE PROGRAMS, kept beside them: each one a run
  // compiled is read back by the next instead of compiled again on the
  // thread that draws while the scene3d sketch opens.
  sigil::material::slang::useProgramStore(pipelines::storeDirectory() /
                                          "surface programs");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Sigil.Grimoire", "Main");

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
      std::println(stderr, "--window-bench: no window to present in");
      return 1;
    }
    window->resize(args.windowBench.width, args.windowBench.height);
    // A WINDOW THE DISPLAY CANNOT HOLD is sized by the system, not by the
    // ask: it is clamped to the screen, or left reaching past it. Either
    // way the row reports the window that presented, and this says why it
    // may differ — the room the display has, in the units the ask is in,
    // since the ratio asked for scales every size the window reads.
    if (const QScreen* screen = window->screen()) {
      const QSize room = screen->availableGeometry().size();
      if (args.windowBench.width > room.width() ||
          args.windowBench.height > room.height())
        std::println(stderr,
                     "[grimoire] --window-bench: a window {}x{} at ratio {:g} "
                     "does not fit the display, which has room for {}x{} at "
                     "that ratio; the system sizes it and the rows report "
                     "the window it gave",
                     args.windowBench.width, args.windowBench.height,
                     window->devicePixelRatio(), room.width(), room.height());
    }
    window->raise();
    window->requestActivate();
    if (!startWindowBench(application, *window, *view, args.windowBench,
                          fileGiven ? std::vector<int>{openAt}
                                    : windowBenchSelection(named, args.kind)))
      return 1;
  }

  if (!args.shotPath.empty()) {
    if (!window || !view) {
      std::println(stderr, "--shot: no window to grab");
      return 1;
    }
    // Synchronize the live host before each forced frame. A file may still
    // be compiling, so warm-up waits for its session before capturing.
    sigil::qt::captureWindow(
        *window, QString::fromStdString(args.shotPath),
        {.prepareFrame =
             [view] {
               if (auto* item = qobject_cast<QQuickItem*>(view)) item->update();
             },
         .ready =
             [] {
               if (SketchCatalog::opensAt < 0) return true;
               QMutexLocker lock(&GrimoireView::hostMutex);
               return GrimoireView::host && GrimoireView::host->live();
             }});
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
    QMutexLocker lock(&GrimoireView::hostMutex);
    GrimoireView::sessions.clear();
    GrimoireView::host = nullptr;
  }
  sharedWebEngine.shutdown();
  releaseDevice();
  // Last, because the set written down is everything this run's draws
  // needed: the canvas's own context goes with the item above, and a
  // program it built on the way out belongs in the file as much as one
  // built on the way in.
  finishPipelineWarmup();
  return status;
}
