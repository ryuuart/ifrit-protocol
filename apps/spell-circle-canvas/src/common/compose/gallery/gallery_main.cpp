// ComposeGallery entry point. Two ways to run it:
//
//   (no arguments)   the Qt Quick app — a QML sidebar over the scene
//                    registry with a live metrics panel.
//   --headless [outdir] [--gpu] [--scene <name|index>]
//                    a per-scene FPS table plus PNG captures; --gpu sweeps
//                    on a Graphite Metal surface instead of raster.
//
// `--scene` narrows either mode to a single entry, which is how one scene is
// iterated on without paying for the whole registry. `--shot <png>` captures
// the APP window rather than a scene, so the sidebar, folders and metrics
// panel can be looked at.

#include "GalleryScenes.h"

#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

int main(int argc, char *argv[]) {
  if (argc >= 2 && std::string(argv[1]) == "--headless") {
    std::string outDir = "compose_gallery_out";
    std::string only;
    bool gpu = false;
    bool noPromotion = false;
    // --capture-at S: take every still at scene time S, overriding both the
    // derived frame and any Scene::captureSeconds. Sweeping at two different
    // times and diffing the output tells you which scenes are still in motion
    // at the moment the gallery photographs them: a settled scene renders
    // identically at both times, a moving one does not. Only the scene's
    // author can then say whether the moment it was caught at is the one
    // worth showing, but without this flag the question cannot be asked
    // per-scene without a recompile.
    double captureAt = -1.0;
    // --ledger: byte-identity plates only — skip the benchmark phases and
    // step every scene straight to its deterministic capture frame. This
    // is what scripts/plate_ledger.py drives, in parallel, per scene.
    bool ledger = false;
    // --timing-json <path>: also write one JSON line per scene with the
    // steady-state sample numbers — the machine-readable lane the GPU
    // 60 FPS gate (scripts/plate_ledger.py --fps-gate) parses. Stdout is
    // unchanged; refused under --ledger (no benchmark phases, no timing).
    std::string timingJson;
    for (int i = 2; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--gpu")
        gpu = true;
      else if (arg == "--no-promotion") // force auto promotion off (the A/B)
        noPromotion = true;
      else if (arg == "--ledger")
        ledger = true;
      else if (arg == "--list-scenes") { // machine-readable registry list, one
        // per line, spelled the way a capture names it (registryName)
        for (int s = 0; s < compose_gallery::kGallerySceneCount; ++s)
          std::printf("%s\n", compose_gallery::registryName(s));
        return 0;
      } else if (arg == "--scene" && i + 1 < argc)
        only = argv[++i];
      else if (arg == "--capture-at" && i + 1 < argc)
        captureAt = std::atof(argv[++i]);
      else if (arg == "--timing-json" && i + 1 < argc)
        timingJson = argv[++i];
      else
        outDir = arg;
    }
    int sceneIndex = -1;
    if (!only.empty()) {
      sceneIndex = compose_gallery::findScene(only);
      if (sceneIndex < 0) {
        std::fprintf(stderr, "no scene matches \"%s\"; known scenes:\n",
                     only.c_str());
        // sceneInfo(), not kScenes[]: the registry runs past the catalog
        // into the studies, and indexing the catalog array with a study's
        // index reads off the end of it.
        for (int i = 0; i < compose_gallery::kGallerySceneCount; ++i) {
          const compose_gallery::SceneInfo info = compose_gallery::sceneInfo(i);
          std::fprintf(stderr, "  %2d  %-20s %s\n", i, info.name,
                       info.category);
        }
        return 1;
      }
    }
    return compose_gallery::runHeadless(outDir, gpu, sceneIndex, noPromotion,
                                        captureAt, ledger, timingJson);
  }

  // `--shot <png> [--scene <name|index>]`: bring the real window up, let it
  // settle, grab it, quit. --headless captures the SCENE; this captures the
  // APP, which is the only way to look at the sidebar, the folders and the
  // metrics panel without a person sitting in front of it.
  std::string shotPath, shotScene;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--shot" && i + 1 < argc)
      shotPath = argv[++i];
    else if (arg == "--scene" && i + 1 < argc)
      shotScene = argv[++i];
  }

  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("ComposeGallery");

  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
  engine.loadFromModule("Compose.Gallery", "Main");

  if (!shotPath.empty()) {
    const QObjectList &roots = engine.rootObjects();
    auto *window = roots.isEmpty()
                       ? nullptr
                       : qobject_cast<QQuickWindow *>(roots.first());
    if (!window) {
      std::fprintf(stderr, "--shot: no window to grab\n");
      return 1;
    }
    // Find the compose view by the properties it has, not by position:
    // findChild() returns the first child of any type, which is a layout.
    QObject *view = nullptr;
    for (QObject *child : window->findChildren<QObject *>()) {
      if (child->property("sceneIndex").isValid() &&
          child->property("scenes").isValid()) {
        view = child;
        break;
      }
    }
    if (!view) {
      std::fprintf(stderr, "--shot: no compose view in the window\n");
      return 1;
    }
    if (!shotScene.empty()) {
      const int index = compose_gallery::findScene(shotScene);
      if (index < 0) {
        std::fprintf(stderr, "no scene matches \"%s\"\n", shotScene.c_str());
        return 1;
      }
      view->setProperty("sceneIndex", index);
    }
    // Drive real frames rather than waiting for them. An unfocused window
    // gets no render loop from the compositor, so a single delayed grab
    // catches a scene that has not started — grabWindow() is what makes the
    // thing run. Marking the item dirty first is the part that is easy to
    // miss: without it the grab re-renders the EXISTING scene-graph node and
    // never calls synchronize(), so the metrics panel keeps showing the
    // values it had before any scene was activated.
    auto *warm = new QTimer(&application);
    auto framesLeft = std::make_shared<int>(90);
    warm->setInterval(16);
    QObject::connect(warm, &QTimer::timeout, &application,
                     [window, view, shotPath, warm, framesLeft] {
                       if (auto *item = qobject_cast<QQuickItem *>(view))
                         item->update();
                       if (--*framesLeft > 0) {
                         window->grabWindow();
                         return;
                       }
                       warm->stop();
                       const QImage image = window->grabWindow();
                       if (image.isNull() ||
                           !image.save(QString::fromStdString(shotPath),
                                       "PNG")) {
                         std::fprintf(stderr, "--shot: grab failed\n");
                         QCoreApplication::exit(1);
                         return;
                       }
                       std::printf("wrote %s (%dx%d)\n", shotPath.c_str(),
                                   image.width(), image.height());
                       QCoreApplication::quit();
                     });
    warm->start();
  }
  return application.exec();
}
