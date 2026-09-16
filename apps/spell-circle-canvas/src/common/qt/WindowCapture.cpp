#include "WindowCapture.h"

#include <QCoreApplication>
#include <QImage>
#include <QQuickWindow>
#include <QTimer>
#include <algorithm>
#include <cstdio>
#include <utility>

namespace ifrit::qt {

void captureWindow(QQuickWindow& window, QString output,
                   WindowCaptureOptions options) {
  auto* timer = new QTimer(&window);
  timer->setInterval(std::max(1, options.intervalMs));
  const int warmupFrames = std::max(1, options.warmupFrames);
  const int readyFrames = std::max(0, options.maxReadyFrames);
  QObject::connect(
      timer, &QTimer::timeout, timer,
      [&window, output = std::move(output), options = std::move(options), timer,
       framesLeft = warmupFrames, readyFramesLeft = readyFrames]() mutable {
        if (options.prepareFrame) options.prepareFrame();
        if (options.ready && readyFramesLeft > 0 && !options.ready()) {
          --readyFramesLeft;
          window.grabWindow();
          return;
        }
        readyFramesLeft = 0;
        // Covered windows may not receive compositor frames. Grabbing
        // drives the same Qt Quick renderer used by the visible window.
        if (--framesLeft > 0) {
          window.grabWindow();
          return;
        }
        timer->stop();
        const QImage picture = window.grabWindow();
        if (picture.isNull() || !picture.save(output, "PNG")) {
          std::fprintf(stderr, "--shot: grab failed\n");
          QCoreApplication::exit(1);
          return;
        }
        std::printf("wrote %s (%dx%d)\n", output.toLocal8Bit().constData(),
                    picture.width(), picture.height());
        QCoreApplication::quit();
      });
  timer->start();
}

}  // namespace ifrit::qt
