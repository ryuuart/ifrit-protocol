/** @file
 * The frame-rate lane over the real window, and the entries it presents.
 */

#include "WindowBench.h"

#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>
#include <sigilsketch/live/BenchCadence.h>
#include <sigilsketch/live/Host.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>

#include "SketchCatalog.h"
#include "SketchbookView.h"

namespace sketch = sigil::sketch;

bool startWindowBench(QGuiApplication& application, QQuickWindow& window,
                      QObject& view, const WindowBench& options,
                      std::vector<int> selection) {
  if (selection.empty()) {
    std::fprintf(stderr, "--window-bench: nothing selected\n");
    return false;
  }
  sketch::BenchCadence::Times times;
  times.warmupSeconds = kWindowBenchWarmupSeconds;
  times.measureSeconds = options.seconds;
  times.ceilingSeconds = kWindowBenchCeilingSeconds;
  struct Run {
    std::vector<int> selection;
    size_t at = 0;
    sketch::BenchCadence cadence;
    std::chrono::steady_clock::time_point began;
    double measureBegan = 0.0;
    unsigned long long framesAtBegin = 0;
    int stoodDown = 0;
  };
  auto run = std::make_shared<Run>(
      Run{std::move(selection), 0, sketch::BenchCadence(times),
          std::chrono::steady_clock::now(), 0.0, 0, 0});
  run->cadence.select(0.0);
  view.setProperty("sketchIndex", run->selection[0]);

  auto* timer = new QTimer(&application);
  timer->setInterval(8);
  QObject::connect(
      timer, &QTimer::timeout, &application,
      [&window, &view, options, run, timer] {
        // Each tick asks the item for a frame. The window drives itself
        // once it is presenting, and the ask costs nothing when it is
        // already going — but a window a compositor has stopped giving
        // frames to would otherwise be measured as a sketch that stopped
        // drawing, which is a different finding entirely.
        if (auto* item = qobject_cast<QQuickItem*>(&view)) item->update();
        const double now = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - run->began)
                               .count();
        const sketch::Entry& entry =
            sketch::registry()[run->selection[run->at]];
        const std::filesystem::path wanted =
            sketch::sourceOf(SketchCatalog::sketchDir, entry.key);

        QMutexLocker lock(&SketchbookView::hostMutex);
        sketch::Host* host = SketchbookView::host;
        // THE SESSION ON SCREEN IS THIS ENTRY'S, AND A FRAME OF IT HAS
        // BEEN PRESENTED. The file a session was opened from is what
        // names it, which is the same key the resident set holds it by.
        const bool presenting = host && host->live() &&
                                host->sketchPath() == wanted &&
                                host->presentedFrames() > 0;
        const sketch::BenchCadence::Step step =
            run->cadence.advance(now, presenting);
        if (step == sketch::BenchCadence::Step::Wait) return;
        if (step == sketch::BenchCadence::Step::Begin) {
          run->measureBegan = now;
          run->framesAtBegin = 0;
          // The session can go between the warm-up and here — a frame it
          // could not draw drops it — and a stretch measured over no
          // session is stood down below rather than read.
          if (host) {
            host->resetMetrics();
            run->framesAtBegin = host->presentedFrames();
          }
          return;
        }

        if (step == sketch::BenchCadence::Step::Read) {
          const QVariantMap metrics = view.property("metrics").toMap();
          const sketch::Kind kind = entry.kind();
          const std::string_view runtime = kind ? kind->runtime() : "?";
          const SkSize canvas = host ? host->canvasSize() : SkSize::Make(0, 0);
          const double work = host ? host->workMsAverage() : 0.0;
          // THE SAME SESSION AT BOTH ENDS OF THE STRETCH, and the frames
          // it put on screen in between.
          const unsigned long long frames =
              presenting && host->presentedFrames() >= run->framesAtBegin
                  ? host->presentedFrames() - run->framesAtBegin
                  : 0;
          // THE RATE IS THE WHOLE STRETCH: the frames that reached the
          // screen over the time they took. The host's own readout is a
          // rolling one, short enough to answer a reader watching it
          // change; a row is one number about a stated stretch, and a
          // hitch inside that stretch weighs what it actually was.
          const double stretch = now - run->measureBegan;
          const double fps = stretch > 0 ? (double)frames / stretch : 0.0;
          // KEYED BY THE STEM, not by the filed name: the line is one
          // whitespace-separated record, and a filed name carries spaces
          // — "aero desktop" would be read as the name "aero" followed
          // by a field nobody wrote. The stem cannot contain a space and
          // is what --sketch already takes.
          //
          // A stretch that ended with all but no frames in it is not a
          // rate: it is stood down with what it did, rather than
          // printed as a rate of nearly zero.
          if (frames < 2) {
            std::printf("WINDOW %s SKIPPED presented %llu frames in %.1fs\n",
                        entry.key, frames, stretch);
            ++run->stoodDown;
          } else {
            std::printf(
                "WINDOW %s window=%dx%d@%g canvas=%dx%d kind=%.*s fps=%.1f "
                "work=%.2fms p99=%.2fms draw=%.2fms submit=%.2fms "
                "headroom=%.1f\n",
                entry.key, window.width(), window.height(),
                window.devicePixelRatio(), (int)canvas.width(),
                (int)canvas.height(), (int)runtime.size(), runtime.data(), fps,
                work, host->workMsP99(), host->drawMsAverage(),
                metrics.value(QStringLiteral("submitMs")).toDouble(),
                work > 0 ? 1000.0 / work : 0.0);
          }
        } else {
          // Step::Skip — the window never presented this entry's session.
          std::printf("WINDOW %s SKIPPED no frame presented in %.1fs\n",
                      entry.key, run->cadence.elapsed(now));
          ++run->stoodDown;
        }
        std::fflush(stdout);

        if (++run->at >= run->selection.size()) {
          timer->stop();
          // A stand-down is a sketch this sweep could not measure, and a
          // sweep that could not measure one did not do what it was
          // asked: the rows it did take stand, and the run says so.
          QCoreApplication::exit(run->stoodDown > 0 ? 1 : 0);
          return;
        }
        view.setProperty("sketchIndex", run->selection[run->at]);
        run->cadence.select(now);
      });
  timer->start();
  return true;
}

/** The registry entries `--window-bench` will present: the whole table,
 *  or what `--sketch` and `--kind` narrow it to. A sketch this machine
 *  cannot run is named as stood down and left out, exactly as the sweep
 *  passes over it — a skip is not a failure and not a measurement. */
std::vector<int> windowBenchSelection(int only, const std::string& kind) {
  std::vector<int> presented;
  const auto& entries = sketch::registry();
  for (int index : sketch::selection(only, kind)) {
    std::string why;
    if (!entries[index].available(&why)) {
      std::printf("WINDOW %s SKIPPED %s\n", entries[index].key, why.c_str());
      continue;
    }
    presented.push_back(index);
  }
  std::fflush(stdout);
  return presented;
}
