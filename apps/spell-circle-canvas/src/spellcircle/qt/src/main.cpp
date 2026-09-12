#ifdef __APPLE__
#include "AppNap.h"
#endif
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <thread>

#include "Models.h"
#include "spdlog/spdlog.h"

int main(int argc, char* argv[]) {
#ifdef __APPLE__
  AppNap::disable();
#endif

  QGuiApplication application(argc, argv);
  QGuiApplication::setOrganizationDomain("sigil.dev");
  QGuiApplication::setApplicationName("SpellCircle");

  // Windows request an alpha channel so the native vibrancy background
  // installed by Ui.WindowChrome shows through transparent QML windows.
  QQuickWindow::setDefaultAlphaBuffer(true);

  spdlog::info("App started");

  boost::asio::io_context networkContext;
  auto networkWork = boost::asio::make_work_guard(networkContext);
  std::thread networkThread([&networkContext] { networkContext.run(); });
  int result = 0;
  {
    Models models(networkContext.get_executor());
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"models", QVariant::fromValue(&models)}});

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("SpellCircle.App", "Main");
    models.networkManager()->start();

#ifdef __APPLE__
    for (QObject* rootObject : engine.rootObjects()) {
      auto* window = qobject_cast<QQuickWindow*>(rootObject);
      if (!window) continue;

      // Keep the render-side texture and Syphon server alive across ordinary
      // visibility changes, and keep the render loop active when the native
      // window is merely covered by another application.
      window->setPersistentGraphics(true);
      window->setPersistentSceneGraph(true);
      if (!AppNap::keepRenderingWhileOccluded(window))
        spdlog::warn("Could not enable rendering while the window is occluded");
    }
#endif

    result = QGuiApplication::exec();
    models.networkManager()->stop();
  }
  // Adapters are destroyed before their executor. Releasing the work guard
  // lets cancellation completions drain without stopping unrelated work.
  networkWork.reset();
  networkThread.join();
  return result;
}
