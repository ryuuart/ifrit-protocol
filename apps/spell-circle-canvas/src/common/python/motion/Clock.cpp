/** @file
 * The frame clock, a ticker Python can own outright, and the master
 * timeline inside one: the phrases a motion is written out of, the
 * steppables a frame runs, and the derived outputs.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

TickerHandle::TickerHandle()
    : m_owner(std::make_shared<motion::Ticker>()),
      m_held(std::make_shared<std::vector<std::shared_ptr<const void>>>()),
      m_thread(std::this_thread::get_id()) {}

TickerHandle::TickerHandle(
    std::function<motion::Ticker&()> access,
    std::function<void(std::shared_ptr<const void>)> retain,
    CallbackLifetime* callbacks)
    : m_access(std::move(access)),
      m_retain(std::move(retain)),
      m_callbacks(callbacks),
      m_thread(std::this_thread::get_id()) {}

motion::Ticker& TickerHandle::get() const {
  // A borrowed ticker is checked by the host that lent it: the access
  // it was made with reports the closed session and the foreign thread
  // in the host's own words, and a second check here would answer the
  // first of those questions with the wrong one.
  if (!m_owner) {
    if (!m_access) throw std::runtime_error("This ticker has no clock behind it");
    return m_access();
  }
  if (m_thread != std::this_thread::get_id())
    throw std::runtime_error(
        "A ticker is touched only on the thread that made it");
  return *m_owner;
}

void TickerHandle::retain(std::shared_ptr<const void> value) const {
  if (!value) return;
  if (m_held)
    m_held->push_back(std::move(value));
  else if (m_retain)
    m_retain(std::move(value));
}

TimelineHandle::TimelineHandle(TickerHandle ticker)
    : m_ticker(std::move(ticker)) {}

choreograph::Timeline& TimelineHandle::get() const {
  return m_ticker.get().timeline();
}

namespace {

/**
 * ONE STEP OF A MOTION, written as a value. Choreograph builds a motion
 * through an options object holding references into the timeline, and
 * that object must not outlive the call that made it, so a whole
 * sequence is handed over at once as a list of these instead.
 *
 * `durationSeconds` is how long a ramp or a hold lasts, `timeSeconds`
 * the moment a hold runs until counted from the start of the sequence,
 * and each kind reads the ones that mean something to it.
 */
struct Phrase {
  enum class Kind : std::uint8_t { RampTo, Hold, HoldUntil, SetValue };
  Kind kind = Kind::Hold;
  float value = 0;
  double durationSeconds = 0;
  double timeSeconds = 0;
  choreograph::EaseFn ease;
};

/** @p value as animation seconds, which are finite and never negative:
 *  time in this library only goes forward. @p what opens the message. */
double animationSeconds(double value, const char* what) {
  if (!std::isfinite(value) || value < 0)
    throw py::value_error(std::string(what) +
                          " must be finite, nonnegative seconds.");
  return value;
}

/** How many of the arguments on offer @p function names, at most
 *  @p maximum of them. */
int callbackArity(const py::function& function, int maximum) {
  return py::module_::import("sigil._callbacks")
      .attr("arity")(function, maximum)
      .cast<int>();
}

/** Whether a steppable that answered @p result still needs frames.
 *  Saying nothing about being finished is taken as never finished. */
bool continueTick(py::object result) {
  return result.is_none() || result.cast<bool>();
}

/** @p function retained so the ticker can call it back: against the
 *  host lifetime a borrowed ticker names, and, on a ticker of Python's
 *  own, against whatever scope is already in force, which outside a
 *  host is ordinary shared ownership. */
std::shared_ptr<PythonCallback> retainForTicker(const TickerHandle& ticker,
                                                py::function function) {
  if (CallbackLifetime* lifetime = ticker.callbacks()) {
    const CallbackScope scope(*lifetime);
    return retainCallback(std::move(function));
  }
  return retainCallback(std::move(function));
}

void addSteppable(const TickerHandle& handle, py::function function) {
  auto& ticker = handle.get();
  const int arity = callbackArity(function, 2);
  auto retained = retainForTicker(handle, std::move(function));
  ticker.add([retained, arity](double dt, double elapsed) {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      auto callback = retained->get();
      if (arity == 0) return continueTick(callback());
      if (arity == 1) return continueTick(callback(dt));
      return continueTick(callback(dt, elapsed));
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  });
}

void addFixedSteppable(const TickerHandle& handle, double hz,
                       py::function function, int maxCatchUp,
                       std::shared_ptr<choreograph::Output<float>> alphaOut,
                       std::shared_ptr<motion::Ticker::FixedStatus> statusOut) {
  if (!std::isfinite(hz) || hz <= 0 || maxCatchUp <= 0)
    throw py::value_error(
        "Fixed-step rate and catch-up limit must be positive");
  auto& ticker = handle.get();
  callbackArity(function, 0);
  auto retained = retainForTicker(handle, std::move(function));
  ticker.addFixed(
      hz,
      [retained] {
        const py::gil_scoped_acquire lock;
        const CallbackBoundary boundary;
        try {
          return continueTick(retained->get()());
        } catch (const py::error_already_set& error) {
          throw std::runtime_error(error.what());
        }
      },
      maxCatchUp, alphaOut.get(), statusOut.get());
  handle.retain(std::move(alphaOut));
  handle.retain(std::move(statusOut));
}

bool deriveOutput(const TickerHandle& handle,
                  std::shared_ptr<choreograph::Output<float>> destination,
                  const motion::Bound& chain) {
  if (!destination) throw py::type_error("A derived output must be an Output");
  if (!handle.get().derive(destination.get(), chain)) return false;
  handle.retain(std::move(destination));
  if (chain.owner()) handle.retain(chain.owner());
  return true;
}

/** Writes @p phrases onto the motion @p options is building, in order. */
void playPhrases(choreograph::MotionOptions<float>& options,
                 const std::vector<Phrase>& phrases) {
  for (const Phrase& phrase : phrases) {
    switch (phrase.kind) {
      case Phrase::Kind::RampTo:
        options.rampTo(phrase.value, phrase.durationSeconds, phrase.ease);
        break;
      case Phrase::Kind::Hold:
        options.hold(phrase.durationSeconds);
        break;
      case Phrase::Kind::HoldUntil:
        options.holdUntil(phrase.timeSeconds);
        break;
      case Phrase::Kind::SetValue:
        options.set(phrase.value);
        break;
    }
  }
}

/** A callback the timeline stores, handed the output's value when the
 *  Python side names a parameter for it. Choreograph's own motion
 *  callback carries nothing, so the value is read off @p output at the
 *  moment of the call. */
std::function<void()> motionCallback(
    const TickerHandle& ticker, py::function function,
    std::shared_ptr<choreograph::Output<float>> output) {
  const int arity = callbackArity(function, 1);
  auto retained = retainForTicker(ticker, std::move(function));
  return [retained, arity, output] {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      auto callback = retained->get();
      if (arity == 0)
        callback();
      else
        callback(output->value());
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

/** A callback the timeline stores for its own two reports, which carry
 *  nothing at all. */
std::function<void()> timelineCallback(const TickerHandle& ticker,
                                       py::function function) {
  auto retained = retainForTicker(ticker, std::move(function));
  return [retained] {
    const py::gil_scoped_acquire lock;
    const CallbackBoundary boundary;
    try {
      retained->get()();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

void applyMotion(const TimelineHandle& handle,
                 std::shared_ptr<choreograph::Output<float>> output,
                 const std::vector<Phrase>& phrases, bool append,
                 std::optional<bool> removeOnFinish,
                 std::optional<double> playbackSpeed,
                 std::optional<double> startTime,
                 std::optional<py::function> onStart,
                 std::optional<py::function> onUpdate,
                 std::optional<py::function> onFinish) {
  if (!output) throw py::type_error("A motion needs an Output to write into");
  auto& timeline = handle.get();
  // The options object holds references into the timeline and into the
  // motion it just made, so it is written and dropped inside this call.
  auto options = append ? timeline.append(output.get())
                        : timeline.apply(output.get());
  playPhrases(options, phrases);
  if (removeOnFinish) options.removeOnFinish(*removeOnFinish);
  if (playbackSpeed) options.playbackSpeed(*playbackSpeed);
  if (startTime)
    options.setStartTime(animationSeconds(*startTime, "A motion's start time"));
  if (onStart)
    options.startFn(
        motionCallback(handle.ticker(), std::move(*onStart), output));
  if (onUpdate)
    options.updateFn(
        motionCallback(handle.ticker(), std::move(*onUpdate), output));
  if (onFinish)
    options.finishFn(
        motionCallback(handle.ticker(), std::move(*onFinish), output));
  // The timeline writes through a bare pointer, so the cell outlives
  // the Python name that was handed over.
  handle.ticker().retain(std::move(output));
}

}  // namespace

void bindMotionClock(py::module_& module) {
  auto clocks = submodule(module, "motion");

  bindRecord<motion::FrameClockOptions>(clocks, "FrameClockOptions",
                                        "Unknown FrameClockOptions field: ")
      .def_readwrite("maxDelta", &motion::FrameClockOptions::maxDelta);

  py::class_<motion::FrameClock>(clocks, "FrameClock")
      .def(py::init<motion::FrameClockOptions>(),
           py::arg("options") = motion::FrameClockOptions{})
      .def(
          "tick",
          [](motion::FrameClock& clock, std::optional<double> nowSeconds) {
            return nowSeconds ? clock.tick(*nowSeconds) : clock.tick();
          },
          py::arg("nowSeconds") = py::none(),
          "The scaled, clamped delta since the previous tick, which is "
          "zero on the first one. Without a reading the clock takes its "
          "own from the monotonic system clock.")
      .def("advance", &motion::FrameClock::advance, py::arg("deltaSeconds"))
      .def("setPaused", &motion::FrameClock::setPaused, py::arg("paused"))
      .def("paused", &motion::FrameClock::paused)
      .def("setTimeScale", &motion::FrameClock::setTimeScale, py::arg("scale"))
      .def("timeScale", &motion::FrameClock::timeScale)
      .def("elapsed", &motion::FrameClock::elapsed);

  // ONE STEP OF A SEQUENCE, AND NOTHING ELSE READS IT. The four
  // factories below are the only way to make one, and a list of them is
  // the only way a motion is written, because the fluent object
  // choreograph would otherwise hand back holds references into a
  // timeline that removes a motion the moment it finishes.
  copyProtocol(py::class_<Phrase>(clocks, "Phrase")
                   .def("copy", [](const Phrase& phrase) { return phrase; }));
  clocks.def(
      "rampTo",
      [](float value, double duration, py::handle ease) {
        return Phrase{.kind = Phrase::Kind::RampTo,
                      .value = value,
                      .durationSeconds =
                          animationSeconds(duration, "A ramp's duration"),
                      .ease = motionEase(ease)};
      },
      py::arg("value"), py::arg("duration"), py::arg("ease") = py::none());
  clocks.def(
      "hold",
      [](double duration) {
        return Phrase{.kind = Phrase::Kind::Hold,
                      .durationSeconds =
                          animationSeconds(duration, "A hold's duration")};
      },
      py::arg("duration"));
  clocks.def(
      "holdUntil",
      [](double time) {
        return Phrase{
            .kind = Phrase::Kind::HoldUntil,
            .timeSeconds = animationSeconds(time, "A hold's end")};
      },
      py::arg("time"));
  // `set` is what choreograph calls this and what a Python author would
  // shadow by naming a variable after it, so the value it sets is in
  // the name instead.
  clocks.def(
      "setValue",
      [](float value) {
        return Phrase{.kind = Phrase::Kind::SetValue, .value = value};
      },
      py::arg("value"));

  py::class_<TimelineHandle>(clocks, "Timeline")
      .def(
          "apply",
          [](const TimelineHandle& handle,
             std::shared_ptr<choreograph::Output<float>> output,
             const std::vector<Phrase>& phrases,
             std::optional<bool> removeOnFinish,
             std::optional<double> playbackSpeed,
             std::optional<double> startTime,
             std::optional<py::function> onStart,
             std::optional<py::function> onUpdate,
             std::optional<py::function> onFinish) {
            applyMotion(handle, std::move(output), phrases, false,
                        removeOnFinish, playbackSpeed, startTime,
                        std::move(onStart), std::move(onUpdate),
                        std::move(onFinish));
          },
          py::arg("output"), py::arg("phrases") = std::vector<Phrase>{},
          py::kw_only(), py::arg("removeOnFinish") = py::none(),
          py::arg("playbackSpeed") = py::none(),
          py::arg("startTime") = py::none(), py::arg("onStart") = py::none(),
          py::arg("onUpdate") = py::none(), py::arg("onFinish") = py::none(),
          "Writes a motion over this output, replacing whatever was "
          "driving it.")
      .def(
          "append",
          [](const TimelineHandle& handle,
             std::shared_ptr<choreograph::Output<float>> output,
             const std::vector<Phrase>& phrases,
             std::optional<bool> removeOnFinish,
             std::optional<double> playbackSpeed,
             std::optional<double> startTime,
             std::optional<py::function> onStart,
             std::optional<py::function> onUpdate,
             std::optional<py::function> onFinish) {
            applyMotion(handle, std::move(output), phrases, true,
                        removeOnFinish, playbackSpeed, startTime,
                        std::move(onStart), std::move(onUpdate),
                        std::move(onFinish));
          },
          py::arg("output"), py::arg("phrases") = std::vector<Phrase>{},
          py::kw_only(), py::arg("removeOnFinish") = py::none(),
          py::arg("playbackSpeed") = py::none(),
          py::arg("startTime") = py::none(), py::arg("onStart") = py::none(),
          py::arg("onUpdate") = py::none(), py::arg("onFinish") = py::none(),
          "Adds to the end of the motion already driving this output, "
          "and writes a new one where there is none.")
      .def(
          "cue",
          [](const TimelineHandle& handle, py::function function,
             double delay) {
            const double when = animationSeconds(delay, "A cue's delay");
            auto& timeline = handle.get();
            timeline.cue(timelineCallback(handle.ticker(), std::move(function)),
                         when);
          },
          py::arg("function"), py::arg("delay"))
      .def("size",
           [](const TimelineHandle& handle) { return handle.get().size(); })
      .def("__len__",
           [](const TimelineHandle& handle) { return handle.get().size(); })
      .def("empty",
           [](const TimelineHandle& handle) { return handle.get().empty(); })
      .def("clear",
           [](const TimelineHandle& handle) { handle.get().clear(); })
      .def("timeUntilFinish",
           [](const TimelineHandle& handle) {
             return handle.get().timeUntilFinish();
           })
      .def("duration",
           [](const TimelineHandle& handle) {
             return handle.get().getDuration();
           })
      .def(
          "setDefaultRemoveOnFinish",
          [](const TimelineHandle& handle, bool doRemove) {
            handle.get().setDefaultRemoveOnFinish(doRemove);
          },
          py::arg("doRemove"))
      .def(
          "setFinishFn",
          [](const TimelineHandle& handle, py::function function) {
            auto& timeline = handle.get();
            timeline.setFinishFn(
                timelineCallback(handle.ticker(), std::move(function)));
          },
          py::arg("function"))
      .def(
          "setClearedFn",
          [](const TimelineHandle& handle, py::function function) {
            auto& timeline = handle.get();
            timeline.setClearedFn(
                timelineCallback(handle.ticker(), std::move(function)));
          },
          py::arg("function"));

  py::class_<TickerHandle>(clocks, "Ticker")
      .def(py::init<>())
      .def(
          "tick",
          [](const TickerHandle& handle, double deltaSeconds) {
            if (!handle.owned())
              throw std::runtime_error(
                  "A host steps the ticker it lends; a body registers on it "
                  "and reads it");
            if (!std::isfinite(deltaSeconds))
              throw py::value_error("A tick's delta must be finite seconds.");
            return handle.get().tick(deltaSeconds);
          },
          py::arg("deltaSeconds"))
      .def("add", &addSteppable, py::arg("function"))
      .def("addFixed", &addFixedSteppable, py::arg("hz"), py::arg("function"),
           py::arg("maxCatchUp") = 8, py::arg("alphaOut") = nullptr,
           py::arg("statusOut") = nullptr)
      .def("derive", &deriveOutput, py::arg("destination"), py::arg("chain"),
           "Whether the derivation was registered. One level only: a "
           "chain of two, a cell written twice and a cell derived from "
           "itself are refused, and the refusal is answered False.")
      .def("timeline",
           [](const TickerHandle& handle) { return TimelineHandle(handle); })
      .def("active",
           [](const TickerHandle& handle) { return handle.get().active(); })
      .def("elapsed",
           [](const TickerHandle& handle) { return handle.get().elapsed(); });
}

}  // namespace sigil::python
