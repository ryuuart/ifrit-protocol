/** @file
 * The engine in Python: one class for an engine Python owns and one a
 * host lends, the animations, timelines and timers it runs, the
 * playback every one of them answers to, and the host's side — the
 * frame that moves it, under a policy and a budget.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/advanced/ClockPolicy.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

namespace {
/** Every value in @p targets, made live in place — so the Python objects
 *  themselves move — and copied into one run sharing their cells. */
template <class T>
std::vector<motion::Animatable<T>> liveRun(const py::sequence& targets) {
  std::vector<motion::Animatable<T>> run;
  run.reserve(py::len(targets));
  for (py::handle item : targets) {
    auto& target = py::cast<motion::Animatable<T>&>(item);
    motion::detail::makeLive(target);
    run.push_back(target);
  }
  return run;
}

motion::Tween<material::Color> colorTween(py::handle value) {
  if (!py::isinstance<motion::Tween<material::Color>>(value))
    throw py::type_error("A motion on a colour needs a ColorTween.");
  return py::cast<motion::Tween<material::Color>>(value);
}
}  // namespace

EngineHandle::EngineHandle(motion::EngineOptions options)
    : m_owner(std::make_shared<motion::Engine>(options)),
      m_thread(std::this_thread::get_id()) {}

EngineHandle::EngineHandle(std::function<motion::Engine&()> access,
                           CallbackLifetime* callbacks)
    : m_access(std::move(access)),
      m_callbacks(callbacks),
      m_thread(std::this_thread::get_id()) {}

motion::Engine& EngineHandle::get() const {
  // A borrowed engine is checked by the host that lent it: the access it
  // was made with reports the closed session and the foreign thread in
  // the host's own words, and a second check here would answer the first
  // of those questions with the wrong one.
  if (!m_owner) {
    if (!m_access)
      throw std::runtime_error("This engine has no clock behind it");
    return m_access();
  }
  if (m_thread != std::this_thread::get_id())
    throw std::runtime_error(
        "An engine is touched only on the thread that made it");
  return *m_owner;
}

motion::Engine& EngineHandle::ownedEngine(const char* refusal) const {
  if (!m_owner) {
    // The session is asked first, so a closed one is reported as closed
    // rather than as a call its host would have refused anyway.
    (void)get();
    throw std::runtime_error(refusal);
  }
  return get();
}

namespace {

constexpr const char* kHostMoves =
    "A host moves the engine it lends; a body starts motions on it and "
    "reads it";

/** How many of the arguments on offer @p function names, at most
 *  @p maximum of them. */
int callbackArity(const py::function& function, int maximum) {
  return py::module_::import("sigil._callbacks")
      .attr("arity")(function, maximum)
      .cast<int>();
}

/** @p function retained so the engine can call it back: against the host
 *  lifetime a borrowed engine names, and, on an engine of Python's own,
 *  against whatever scope is already in force, which outside a host is
 *  ordinary shared ownership. */
std::shared_ptr<PythonCallback> retainForEngine(const EngineHandle& engine,
                                                py::function function) {
  if (CallbackLifetime* lifetime = engine.callbacks()) {
    const CallbackScope scope(*lifetime);
    return retainCallback(std::move(function));
  }
  return retainCallback(std::move(function));
}

/** A callback that takes nothing and answers nothing, called with the
 *  interpreter lock held and a Python error turned into a native one. */
std::function<void()> notification(std::shared_ptr<PythonCallback> retained) {
  return [retained = std::move(retained)] {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      retained->get()();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

/** A timer's update: handed as many of the frame's delta and the timer's
 *  elapsed time, in seconds, as it names, and running on until it
 *  answers False. Saying nothing is taken as running on. */
std::function<bool(motion::Duration, motion::Duration)> timerUpdate(
    const EngineHandle& engine, py::function function) {
  const int arity = callbackArity(function, 2);
  auto retained = retainForEngine(engine, std::move(function));
  return [retained, arity](motion::Duration delta, motion::Duration elapsed) {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      auto callback = retained->get();
      py::object result;
      if (arity == 0)
        result = callback();
      else if (arity == 1)
        result = callback(delta.count());
      else
        result = callback(delta.count(), elapsed.count());
      return result.is_none() || result.cast<bool>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

motion::Duration timelineTime(motion::Duration value, const char* what) {
  if (!std::isfinite(value.count()))
    throw py::value_error(std::string(what) + " must be finite seconds.");
  return value;
}

/** The verbs every playback answers, bound on @p type so each one hands
 *  back the Python object it was called on: pybind11 finds that object
 *  only when the answer is spelled as the class it was made as, so a
 *  chain keeps the class it started from. The object is the one called,
 *  so the answer keeps nothing alive — a playback that kept itself would
 *  never be released. */
template <class Kind, class... Options>
void bindPlaybackVerbs(py::class_<Kind, Options...>& type) {
  constexpr auto fluent = py::return_value_policy::reference;
  const auto verb = [&](const char* name,
                        motion::Playback& (motion::Playback::*member)(),
                        const char* doc) {
    type.def(
        name,
        [member](Kind& self) -> Kind& {
          (self.*member)();
          return self;
        },
        fluent, doc);
  };
  verb("play", &motion::Playback::play,
       "Runs from where it stands; a completed playback starts again.");
  verb("pause", &motion::Playback::pause, "Holds where it stands.");
  verb("resume", &motion::Playback::resume,
       "Runs again from where it was paused.");
  verb("restart", &motion::Playback::restart, "Back to the start, running.");
  verb("reverse", &motion::Playback::reverse,
       "Runs the other way from where it stands.");
  verb("alternate", &motion::Playback::alternate,
       "Flips direction at the end of every pass from now on.");
  verb("complete", &motion::Playback::complete,
       "Jumps to the end: the targets take their final values.");
  verb("cancel", &motion::Playback::cancel,
       "Stops where it stands; the targets keep the values they hold.");
  verb("revert", &motion::Playback::revert,
       "Stops and puts every target back where it was before it started.");
  type.def(
      "seek",
      [](Kind& self, motion::Duration time) -> Kind& {
        self.seek(timelineTime(time, "A seek's time"));
        return self;
      },
      py::arg("time"), fluent,
      "Moves to this time from its start and shows it there.");
  type.def(
      "onComplete",
      [](Kind& self, py::function callback) -> Kind& {
        // The playback is not told which engine runs it, so the callable
        // is held against the scope in force, which a host opens around
        // every body it runs.
        self.onComplete(notification(retainCallback(std::move(callback))));
        return self;
      },
      py::arg("callback"), fluent, "Called once, when it completes.");
}

}  // namespace

void bindMotionClock(py::module_& root) {
  auto module = root.def_submodule("motion");

  py::enum_<motion::ClockPolicy>(
      module, "ClockPolicy",
      "Who moves an engine's clock: the wall, a caller's stated steps, "
      "nobody, or the wall except while something asked for is arriving.")
      .value("Wall", motion::ClockPolicy::Wall)
      .value("Advance", motion::ClockPolicy::Advance)
      .value("Pause", motion::ClockPolicy::Pause)
      .value("PauseWhileLoading", motion::ClockPolicy::PauseWhileLoading);

  auto options = bindRecord<motion::EngineOptions>(
      module, "EngineOptions", "Unknown EngineOptions field: ");
  options.doc() =
      "What an engine is built with: a fixed step every frame it takes on "
      "its own moves (zero moves it by the wall clock), the most one wall "
      "frame moves, and engine time per wall time.";
  options.def_readwrite("fixedStep", &motion::EngineOptions::fixedStep)
      .def_readwrite("maxWallStep", &motion::EngineOptions::maxWallStep)
      .def_readwrite("speed", &motion::EngineOptions::speed);

  auto position = py::class_<motion::Position>(
      module, "Position",
      "Where a timeline item starts, relative to what is already on it.");
  py::enum_<motion::Position::Kind>(position, "Kind")
      .value("At", motion::Position::Kind::At)
      .value("AfterEnd", motion::Position::Kind::AfterEnd)
      .value("AfterPrevious", motion::Position::Kind::AfterPrevious)
      .value("WithPrevious", motion::Position::Kind::WithPrevious)
      .value("AtLabel", motion::Position::Kind::AtLabel);
  position.def_readonly("kind", &motion::Position::kind)
      .def_readonly("offset", &motion::Position::offset)
      .def_readonly("label", &motion::Position::label)
      .def("copy", [](const motion::Position& value) { return value; });
  copyProtocol(position);
  module.def(
      "at",
      [](motion::Duration time) {
        return motion::at(timelineTime(time, "A timeline position"));
      },
      py::arg("time"), "At this time from the timeline's start.");
  module.def(
      "afterEnd",
      [](motion::Duration offset) {
        return motion::afterEnd(timelineTime(offset, "An offset"));
      },
      py::arg("offset") = 0.0,
      "After everything on the timeline has ended, plus the offset.");
  module.def(
      "afterPrevious",
      [](motion::Duration offset) {
        return motion::afterPrevious(timelineTime(offset, "An offset"));
      },
      py::arg("offset") = 0.0,
      "When the item added just before ends, plus the offset.");
  module.def(
      "withPrevious",
      [](motion::Duration offset) {
        return motion::withPrevious(timelineTime(offset, "An offset"));
      },
      py::arg("offset") = 0.0,
      "When the item added just before starts, plus the offset.");
  module.def(
      "atLabel",
      [](std::string name, motion::Duration offset) {
        return motion::atLabel(std::move(name),
                               timelineTime(offset, "An offset"));
      },
      py::arg("name"), py::arg("offset") = 0.0,
      "At the named label, plus the offset.");

  constexpr auto fluent = py::return_value_policy::reference;
  py::class_<motion::Playback> playback(
      module, "Playback",
      "What an animation, a timeline and a timer answer to. Copies control "
      "the same playback, and the engine runs it whether or not a handle "
      "is kept.");
  bindPlaybackVerbs(playback);
  playback.def("isRunning", &motion::Playback::isRunning)
      .def("isPaused", &motion::Playback::isPaused)
      .def("isCompleted", &motion::Playback::isCompleted)
      .def("currentTime", &motion::Playback::currentTime,
           "Time from its start, delay included, in seconds.")
      .def("progress", &motion::Playback::progress);
  py::class_<motion::Animation, motion::Playback> animation(
      module, "Animation", "One tween running on one live value.");
  bindPlaybackVerbs(animation);
  py::class_<motion::Timer, motion::Playback> timer(
      module, "Timer",
      "A callback the engine runs every frame, at a fixed rate, or "
      "throttled.");
  bindPlaybackVerbs(timer);
  timer
      .def("betweenSteps", &motion::Timer::betweenSteps,
           "Under a step rate: the fraction of a step left over after this "
           "frame's stepping.")
      .def("droppedTime", &motion::Timer::droppedTime,
           "Under a step rate: whether this frame dropped time because the "
           "backlog passed the catch-up limit.")
      .def("stepsThisFrame", &motion::Timer::stepsThisFrame);
  py::class_<motion::Timeline, motion::Playback> timeline(
      module, "Timeline",
      "Tweens and calls placed in time, played as one. An item with no "
      "position starts after everything already on it has ended.");
  bindPlaybackVerbs(timeline);
  timeline
      .def(
          "add",
          [](motion::Timeline& self, motion::Animatable<float>& target,
             py::handle tween,
             const motion::Position& when) -> motion::Timeline& {
            return self.add(target, motionTween(tween), when);
          },
          py::arg("target"), py::arg("tween"),
          py::arg("position") = motion::afterEnd(), fluent)
      .def(
          "add",
          [](motion::Timeline& self,
             motion::Animatable<material::Color>& target, py::handle tween,
             const motion::Position& when) -> motion::Timeline& {
            return self.add(target, colorTween(tween), when);
          },
          py::arg("target"), py::arg("tween"),
          py::arg("position") = motion::afterEnd(), fluent)
      .def(
          "add",
          [](motion::Timeline& self, const py::list& targets, py::handle tween,
             const motion::Position& when) -> motion::Timeline& {
            std::vector<motion::Animatable<float>> run =
                liveRun<float>(targets);
            return self.add(run, motionTween(tween), when);
          },
          py::arg("targets"), py::arg("tween"),
          py::arg("position") = motion::afterEnd(), fluent)
      .def(
          "call",
          [](motion::Timeline& self, py::function callback,
             const motion::Position& when) -> motion::Timeline& {
            return self.call(notification(retainCallback(std::move(callback))),
                             when);
          },
          py::arg("callback"), py::arg("position") = motion::afterEnd(), fluent)
      .def(
          "label",
          [](motion::Timeline& self, std::string name,
             const motion::Position& when) -> motion::Timeline& {
            return self.label(std::move(name), when);
          },
          py::arg("name"), py::arg("position") = motion::afterEnd(), fluent);

  py::class_<EngineHandle>(
      module, "Engine",
      "The one clock a sketch's animations, timelines and timers run on. "
      "One made here is Python's own and moved from Python; the one a "
      "sketch is handed is the host's, which moves it.")
      .def(py::init<motion::EngineOptions>(),
           py::arg("options") = motion::EngineOptions{})
      .def(
          "animate",
          [](const EngineHandle& handle, motion::Animatable<float>& target,
             py::handle tween) {
            return handle.get().animate(target, motionTween(tween));
          },
          py::arg("target"), py::arg("tween"),
          "Runs the tween on the target, a live value — made live from the "
          "value it holds if it is not one.")
      .def(
          "animate",
          [](const EngineHandle& handle,
             motion::Animatable<material::Color>& target, py::handle tween) {
            return handle.get().animate(target, colorTween(tween));
          },
          py::arg("target"), py::arg("tween"),
          "Runs a colour tween on a colour value, on the line Material mixes "
          "a colour along.")
      .def(
          "animate",
          [](const EngineHandle& handle, const py::list& targets,
             py::handle tween) {
            std::vector<motion::Animatable<float>> run =
                liveRun<float>(targets);
            return handle.get().animate(run, motionTween(tween));
          },
          py::arg("targets"), py::arg("tween"),
          "Runs the tween on every one of the targets as siblings: a field "
          "written as stagger() or cues() takes each target's own value from "
          "its place in the list. The timeline handed back controls the run.")
      .def(
          "timeline",
          [](const EngineHandle& handle) { return handle.get().timeline(); },
          "A timeline, running from now.")
      .def(
          "timer",
          [](const EngineHandle& handle, py::function callback, double stepRate,
             double frameRate, int catchUp, motion::Duration duration,
             motion::Duration delay) {
            if (!std::isfinite(stepRate) || stepRate < 0 ||
                !std::isfinite(frameRate) || frameRate < 0 || catchUp <= 0)
              throw py::value_error(
                  "A timer's rates must be finite and nonnegative, and its "
                  "catch-up limit positive.");
            auto& engine = handle.get();
            return engine.timer(
                timerUpdate(handle, std::move(callback)),
                motion::TimerOptions{
                    stepRate, frameRate, catchUp,
                    timelineTime(duration, "A timer's duration"),
                    timelineTime(delay, "A timer's delay")});
          },
          py::arg("callback"), py::kw_only(), py::arg("stepRate") = 0.0,
          py::arg("frameRate") = 0.0, py::arg("catchUp") = 8,
          py::arg("duration") = 0.0, py::arg("delay") = 0.0,
          "A callback the engine runs every frame, or at the rate asked "
          "for, until it is cancelled or answers False. It is handed as "
          "many of the frame's delta and the timer's elapsed time, in "
          "seconds, as it names.")
      .def(
          "elapsed",
          [](const EngineHandle& handle) { return handle.get().elapsed(); },
          "Engine time so far, in seconds.")
      .def(
          "isRunning",
          [](const EngineHandle& handle) { return handle.get().isRunning(); },
          "Whether anything is declared to move.")
      .def(
          "advance",
          [](const EngineHandle& handle, std::optional<motion::Duration> to) {
            auto& engine = handle.ownedEngine(kHostMoves);
            if (to) return engine.advance(timelineTime(*to, "A frame's time"));
            return engine.advance();
          },
          py::arg("to") = py::none(),
          "One frame. With no time, the fixed step or the wall clock; with "
          "one, to that absolute engine time. Answers how far it moved.")
      .def(
          "advanceWall",
          [](const EngineHandle& handle, double wallSeconds) {
            if (!std::isfinite(wallSeconds))
              throw py::value_error("A wall reading must be finite seconds.");
            return handle.ownedEngine(kHostMoves).advanceWall(wallSeconds);
          },
          py::arg("wallSeconds"), "One frame at a stated wall reading.")
      .def(
          "setPolicy",
          [](const EngineHandle& handle, motion::ClockPolicy policy,
             std::optional<motion::Duration> budget) {
            handle.ownedEngine(kHostMoves).setPolicy(policy, budget);
          },
          py::arg("policy"), py::arg("budget") = py::none())
      .def("policy",
           [](const EngineHandle& handle) { return handle.get().policy(); })
      .def("isWall",
           [](const EngineHandle& handle) { return handle.get().isWall(); })
      .def(
          "setArriving",
          [](const EngineHandle& handle, bool arriving) {
            handle.ownedEngine(kHostMoves).setArriving(arriving);
          },
          py::arg("arriving"))
      .def(
          "setHeld",
          [](const EngineHandle& handle, bool held) {
            handle.ownedEngine(kHostMoves).setHeld(held);
          },
          py::arg("held"))
      .def("isHeld",
           [](const EngineHandle& handle) { return handle.get().isHeld(); })
      .def(
          "setSpeed",
          [](const EngineHandle& handle, double speed) {
            if (!std::isfinite(speed) || speed < 0)
              throw py::value_error(
                  "An engine's speed must be finite and "
                  "nonnegative.");
            handle.ownedEngine(kHostMoves).setSpeed(speed);
          },
          py::arg("speed"))
      .def("speed",
           [](const EngineHandle& handle) { return handle.get().speed(); })
      .def("isPaused",
           [](const EngineHandle& handle) { return handle.get().isPaused(); })
      .def("frames",
           [](const EngineHandle& handle) { return handle.get().frames(); })
      .def("budgetRemaining",
           [](const EngineHandle& handle) {
             return handle.get().budgetRemaining();
           })
      .def("isBudgetExpired",
           [](const EngineHandle& handle) {
             return handle.get().isBudgetExpired();
           })
      .def("restart", [](const EngineHandle& handle) {
        handle.ownedEngine(kHostMoves).restart();
      });
}

}  // namespace sigil::python
