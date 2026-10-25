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
#include <print>
#include <string>
#include <string_view>
#include <utility>

#include <WindowChrome.h>

#include "Conditions.h"
#include "GrimoireView.h"
#include "SketchCatalog.h"

namespace sketch = sigil::sketch;

bool startWindowBench(QGuiApplication& application, QQuickWindow& window,
                      QObject& view, const WindowBench& options,
                      std::vector<int> selection) {
  if (selection.empty()) {
    std::println(stderr, "--window-bench: nothing selected");
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
    /** When the entry on screen was asked for, when the window first
     *  presented its session, and the longest frame of its warm-up: what
     *  opening it cost, kept apart from the stretch it is measured over. */
    double asked = 0.0;
    double opened = -1.0;
    double slowestWarmMilliseconds = 0.0;
  };
  auto run = std::make_shared<Run>(
      Run{std::move(selection), 0, sketch::BenchCadence(times),
          std::chrono::steady_clock::now(), 0.0, 0, 0, 0.0, -1.0, 0.0});
  run->cadence.select(0.0);
  view.setProperty("sketchIndex", run->selection[0]);
  // THE DISPLAY IS LIT, AND STAYS LIT FOR THE WHOLE RUN. A sweep of the
  // registry outlasts the idle time after which the display sleeps, and a
  // window on a sleeping display is not exposed: every sketch from that
  // moment until someone touches the machine would be stood down as
  // presenting nothing. Held by the timer's closure, so it goes when the
  // run does.
  auto displayAwake = std::make_shared<DisplayAwakeHold>(
      "Grimoire measures each sketch in its window");
  // AND THE PROCESS STAYS USER-FACING FOR THE WHOLE RUN, wherever the
  // window stands. A window wholly covered by another application stops
  // declaring the process user-facing, and about half a minute later the
  // system moves every thread of it to the background tier: a sweep
  // nobody is watching reads its first rows at their own rate and every
  // row after them several times slower, with work that is still small.
  // A measurement's frames are wanted whatever covers the window, which
  // is what a window whose frames other applications watch declares.
  WindowChrome::keepRendering(&window);

  auto* timer = new QTimer(&application);
  timer->setInterval(8);
  QObject::connect(
      timer, &QTimer::timeout, &application,
      [&window, &view, options, run, timer, displayAwake] {
        // Each tick asks the item for a frame. The window drives itself
        // once it is presenting, and the ask costs nothing when it is
        // already going — but a window a compositor has stopped giving
        // frames to would otherwise be measured as a sketch that stopped
        // drawing, which is a different finding entirely.
        if (auto* item = qobject_cast<QQuickItem*>(&view)) item->update();
        const double now = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - run->began)
                               .count();
        const int index = run->selection[run->at];
        const auto& entries = sketch::registry();
        const std::filesystem::path wanted =
            index < (int)entries.size()
                ? sketch::sourceOf(SketchCatalog::sketchDirectory,
                                   entries[index].key)
                : SketchCatalog::externals.at(index - entries.size());
        const std::string key = wanted.stem().string();

        QMutexLocker lock(&GrimoireView::hostMutex);
        sketch::Host* host = GrimoireView::host;
        // THE SESSION ON SCREEN IS THIS ENTRY'S, AND A FRAME OF IT HAS
        // BEEN PRESENTED. The file a session was opened from is what
        // names it, which is the same key the resident set holds it by.
        const bool presenting = host && host->live() &&
                                host->sketchPath() == wanted &&
                                host->presentedFrames() > 0;
        if (presenting && run->opened < 0.0) run->opened = now;
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
            run->slowestWarmMilliseconds =
                sigil::measure::Milliseconds(
                    host->frameTimes().work().quantile(1.0))
                    .count();
            host->resetMetrics();
            run->framesAtBegin = host->presentedFrames();
            // The device's stages are timed over the measured stretch
            // alone, so the row's breakdown is the frames its rate is.
            if (sketch::Session* session = host->session())
              session->timeDevice(true);
          }
          return;
        }

        if (step == sketch::BenchCadence::Step::Read) {
          const QVariantMap metrics = view.property("metrics").toMap();
          const std::string_view runtime = host ? host->kind() : "?";
          const SkSize canvas = host ? host->canvasSize() : SkSize::Make(0, 0);
          const double work = host ? sigil::measure::Milliseconds(
                                         host->frameTimes().work().mean())
                                         .count()
                                   : 0.0;
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
          if (host && !host->errorLog().empty()) {
            std::println("WINDOW {} FAILED resource or runtime error", key);
            std::println(stderr, "{}", host->errorLog());
            ++run->stoodDown;
          } else if (frames < 2) {
            std::println("WINDOW {} SKIPPED presented {} frames in {:.1f}s",
                         key, frames, stretch);
            ++run->stoodDown;
          } else {
            // THE DEVICE'S SHARE, stage by stage, where the session has
            // stages on a device: the last field, so a reader of the
            // fields before it reads the row as it always did.
            std::string device;
            if (sketch::Session* session = host->session()) {
              device = session->deviceTimes();
              session->timeDevice(false);
            }
            if (!device.empty()) device = " gpu=" + device;
            std::println(
                "WINDOW {} window={}x{}@{:g} canvas={}x{} kind={} fps={:.1f} "
                "work={:.2f}ms p99={:.2f}ms draw={:.2f}ms submit={:.2f}ms "
                "headroom={:.1f} open={:.2f}s warmMax={:.1f}ms power={} "
                "gpuBusy={}%{}",
                key, window.width(), window.height(), window.devicePixelRatio(),
                (int)canvas.width(), (int)canvas.height(), runtime, fps, work,
                sigil::measure::Milliseconds(
                    host->frameTimes().work().quantile(0.99))
                    .count(),
                sigil::measure::Milliseconds(host->drawTimes().mean()).count(),
                metrics.value(QStringLiteral("submitMilliseconds")).toDouble(),
                host->frameTimes().headroomFps(),
                run->opened >= 0.0 ? run->opened - run->asked : -1.0,
                run->slowestWarmMilliseconds, powerSource(), gpuBusyPercent(),
                device);
          }
        } else {
          // Step::Skip — the window never presented this entry's session.
          // A window the system no longer exposes is asked for no frames
          // at all, which is the machine's state and not the sketch's, so
          // the line says which of the two it was.
          std::println("WINDOW {} SKIPPED no frame presented in {:.1f}s{}", key,
                       run->cadence.elapsed(now),
                       window.isExposed()
                           ? ""
                           : " (the window is not exposed: the display "
                             "asleep, the window minimised or on another "
                             "space)");
          ++run->stoodDown;
        }
        std::fflush(stdout);

        if (++run->at >= run->selection.size()) {
          timer->stop();
          // Every selected sketch must present clean frames. Keep the
          // successful rows, but fail the run if one could not be measured
          // or rendered with a resource or runtime error.
          QCoreApplication::exit(run->stoodDown > 0 ? 1 : 0);
          return;
        }
        view.setProperty("sketchIndex", run->selection[run->at]);
        run->cadence.select(now);
        run->asked = now;
        run->opened = -1.0;
        run->slowestWarmMilliseconds = 0.0;
      });
  timer->start();
  return true;
}

/** The registry entries `--window-bench` will present: the whole table,
 *  or what `--kind` narrows it to, or the entries @p named by `--sketch`
 *  in the order they were named. A sketch this machine cannot run is
 *  named as stood down and left out, exactly as the sweep passes over
 *  it — a skip is not a failure and not a measurement. */
std::vector<int> windowBenchSelection(const std::vector<int>& named,
                                      const std::string& kind) {
  std::vector<int> wanted;
  if (named.empty()) {
    wanted = sketch::selection(-1, kind);
  } else {
    for (int index : named)
      for (int kept : sketch::selection(index, kind)) wanted.push_back(kept);
  }
  std::vector<int> presented;
  const auto& entries = sketch::registry();
  for (int index : wanted) {
    std::string why;
    if (!entries[index].available(&why)) {
      std::println("WINDOW {} SKIPPED {}", entries[index].key, why);
      continue;
    }
    presented.push_back(index);
  }
  std::fflush(stdout);
  return presented;
}
