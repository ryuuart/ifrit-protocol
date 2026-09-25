/** @file
 * A value that differs per child, in Python: the place a child stands
 * at, `stagger()` and `cues()`, and the schedule a run of units resolves
 * a stagger into — `Timing`, `Schedule` and the `Beat` it reads back.
 *
 * Python holds one staggered class whose values are plain numbers. A
 * tween field that is a length of time reads those numbers as seconds,
 * and one that is a number reads them as they are, so the same
 * `stagger(0.04)` is a delay ladder on `delay` and a value ladder on
 * `to`.
 */

#include <pybind11/stl.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/motion/Registration.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

using Seconds = motion::Staggered<double>;

/** @p value's parts rebuilt over another value type through @p convert:
 *  the one bridge between the number Python holds and the duration or
 *  float a tween field takes. */
template <class To, class From, class Convert>
motion::Staggered<To> restated(const motion::Staggered<From>& value,
                               Convert convert) {
  using Form = typename motion::Staggered<From>::Form;
  const auto& options = value.options();
  const motion::StaggerOptions<To> restatedOptions{
      options.from,           options.grid, options.axis,
      options.ease,           convert(options.start),
      options.reverse,        options.seed, options.rankBy};
  switch (value.form()) {
    case Form::Value:
      return motion::Staggered<To>(convert(value.firstValue()));
    case Form::Each:
      return motion::Staggered<To>::each(convert(value.firstValue()),
                                         restatedOptions);
    case Form::Range:
      return motion::Staggered<To>::range(convert(value.firstValue()),
                                          convert(value.lastValue()),
                                          restatedOptions);
    case Form::Table: {
      std::vector<To> entries;
      for (const From& entry : value.table()) entries.push_back(convert(entry));
      return motion::Staggered<To>::table(std::move(entries));
    }
  }
  return {};
}

double secondsOf(py::handle value) {
  return py::cast<motion::Duration>(value).count();
}

/** The origin a stagger is dealt from: a named place, or a child's
 *  index. */
motion::StaggerOrigin origin(py::handle value) {
  if (py::isinstance<motion::StaggerFrom>(value))
    return py::cast<motion::StaggerFrom>(value);
  return py::cast<size_t>(value);
}

motion::StaggerOptions<double> staggerOptions(
    py::handle from, std::array<uint32_t, 2> grid, motion::StaggerAxis axis,
    py::handle ease, py::handle start, bool reverse, uint32_t seed,
    std::vector<float> rankBy) {
  motion::StaggerOptions<double> options;
  options.from = origin(from);
  options.grid = grid;
  options.axis = axis;
  if (!ease.is_none()) options.ease = motionEase(ease);
  options.start = secondsOf(start);
  options.reverse = reverse;
  options.seed = seed;
  options.rankBy = std::move(rankBy);
  return options;
}

}  // namespace

motion::Staggered<float> staggeredNumber(py::handle value) {
  if (py::isinstance<Seconds>(value)) return py::cast<Seconds>(value);
  return py::cast<float>(value);
}

motion::Staggered<motion::Duration> staggeredDuration(py::handle value) {
  if (py::isinstance<Seconds>(value))
    return restated<motion::Duration>(
        py::cast<Seconds>(value),
        [](double seconds) { return motion::Duration(seconds); });
  return py::cast<motion::Duration>(value);
}

py::object staggeredReading(const motion::Staggered<float>& value) {
  if (!value.isStaggered()) return py::float_(value.value());
  return py::cast(Seconds(value));
}

py::object staggeredReading(const motion::Staggered<motion::Duration>& value) {
  if (!value.isStaggered()) return py::float_(value.value().count());
  return py::cast(restated<double>(
      value, [](motion::Duration time) { return time.count(); }));
}

void bindMotionSchedule(py::module_& root) {
  auto module = root.def_submodule("motion");

  auto place = py::class_<motion::Place>(
      module, "Place",
      "Where a child stands among its siblings: its index and how many "
      "there are. One child alone is Place(0, 1).");
  place
      .def(py::init([](size_t index, size_t count) {
             return motion::Place{index, count};
           }),
           py::arg("index") = 0, py::arg("count") = 1)
      .def_readwrite("index", &motion::Place::index)
      .def_readwrite("count", &motion::Place::count)
      .def("copy", [](const motion::Place& value) { return value; })
      .def(
          "__eq__",
          [](const motion::Place& left, const motion::Place& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(place);

  py::enum_<motion::StaggerFrom>(module, "StaggerFrom",
                                 "Which sibling takes a stagger's first value.")
      .value("First", motion::StaggerFrom::First)
      .value("Center", motion::StaggerFrom::Center)
      .value("Last", motion::StaggerFrom::Last)
      .value("Edges", motion::StaggerFrom::Edges)
      .value("Random", motion::StaggerFrom::Random);
  py::enum_<motion::StaggerAxis>(
      module, "StaggerAxis",
      "Which distance a grid stagger reads: straight-line, or along one axis.")
      .value("Both", motion::StaggerAxis::Both)
      .value("Columns", motion::StaggerAxis::Columns)
      .value("Rows", motion::StaggerAxis::Rows);

  auto staggered = py::class_<Seconds>(
      module, "Staggered",
      "A value that may differ per child: a plain number, a step from one "
      "sibling to the next, a spread from the first child's value to the "
      "last's, or a table. A field that is a length of time reads its "
      "numbers as seconds.");
  py::enum_<Seconds::Form>(staggered, "Form")
      .value("Value", Seconds::Form::Value)
      .value("Each", Seconds::Form::Each)
      .value("Range", Seconds::Form::Range)
      .value("Table", Seconds::Form::Table);
  staggered
      .def(py::init([](py::handle value) { return Seconds(secondsOf(value)); }),
           py::arg("value"))
      .def("at", &Seconds::at, py::arg("place"),
           "The value for the child at this place.")
      .def("value", &Seconds::value,
           "The value for a child alone, placed among no siblings.")
      .def("isStaggered", &Seconds::isStaggered)
      .def_property_readonly("form", &Seconds::form)
      .def("copy", [](const Seconds& value) { return value; })
      .def(
          "__eq__",
          [](const Seconds& left, const Seconds& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(staggered);

  module.def(
      "stagger",
      [](py::handle each, py::handle from, std::array<uint32_t, 2> grid,
         motion::StaggerAxis axis, py::handle ease, py::handle start,
         bool reverse, uint32_t seed, std::vector<float> rankBy) {
        auto options = staggerOptions(from, grid, axis, ease, start, reverse,
                                      seed, std::move(rankBy));
        if (!py::isinstance<py::str>(each) && py::isinstance<py::sequence>(each)) {
          const auto ends = py::cast<py::sequence>(each);
          if (py::len(ends) != 2)
            throw py::value_error(
                "A spread stagger is the first child's value and the last's.");
          return Seconds::range(secondsOf(ends[0]), secondsOf(ends[1]),
                                std::move(options));
        }
        return Seconds::each(secondsOf(each), std::move(options));
      },
      py::arg("each"), py::kw_only(),
      py::arg("from_") = motion::StaggerFrom::First,
      py::arg("grid") = std::array<uint32_t, 2>{0, 0},
      py::arg("axis") = motion::StaggerAxis::Both, py::arg("ease") = py::none(),
      py::arg("start") = 0.0, py::arg("reverse") = false, py::arg("seed") = 0,
      py::arg("rankBy") = std::vector<float>{},
      "A value that steps from one sibling to the next — stagger(0.04) is "
      "0, 0.04, 0.08… — or, given a first and a last value, one spread "
      "across the siblings.");
  module.def(
      "cues",
      [](py::handle starts) {
        std::vector<double> entries;
        for (auto item : py::iter(starts)) entries.push_back(secondsOf(item));
        return Seconds::table(std::move(entries));
      },
      py::arg("starts"),
      "A table of starts cut against a recording, read by index; a child "
      "past the end starts at the last entry.");

  auto beat = py::class_<motion::Beat>(
      module, "Beat", "One beat of a resolved schedule, read back.");
  beat.def_readonly("unitIndex", &motion::Beat::unitIndex)
      .def_readonly("start", &motion::Beat::start)
      .def_readonly("localProgress", &motion::Beat::localProgress)
      .def_readonly("running", &motion::Beat::running)
      .def("copy", [](const motion::Beat& value) { return value; })
      .def(
          "__eq__",
          [](const motion::Beat& left, const motion::Beat& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(beat);

  auto timing = py::class_<motion::Timing>(
      module, "Timing",
      "How a run of units shares one progress: each unit's delay, its own "
      "motion's length, the loop period, and a second stagger inside every "
      "beat.");
  timing
      .def(py::init([](py::handle delay, motion::Duration duration,
                       motion::Duration loop, py::handle within) {
             motion::Timing value;
             value.delay = staggeredDuration(delay);
             value.duration = duration;
             value.loop = loop;
             if (!within.is_none()) value.within = staggeredDuration(within);
             return value;
           }),
           py::kw_only(),
           py::arg("delay") = Seconds::each(0.03, {}), py::arg("duration") = 0.45,
           py::arg("loop") = 0.0, py::arg("within") = py::none())
      .def_property(
          "delay",
          [](const motion::Timing& value) { return staggeredReading(value.delay); },
          [](motion::Timing& value, py::handle delay) {
            value.delay = staggeredDuration(delay);
          })
      .def_readwrite("duration", &motion::Timing::duration)
      .def_readwrite("loop", &motion::Timing::loop)
      .def_property(
          "within",
          [](const motion::Timing& value) -> py::object {
            if (!value.within) return py::none();
            return staggeredReading(*value.within);
          },
          [](motion::Timing& value, py::handle within) {
            if (within.is_none())
              value.within.reset();
            else
              value.within = staggeredDuration(within);
          })
      .def("span", &motion::Timing::span, py::arg("count"),
           py::arg("innerCount") = 1,
           "The span one master progress maps onto for this many units.")
      .def("copy", [](const motion::Timing& value) { return value; })
      .def(
          "__eq__",
          [](const motion::Timing& left, const motion::Timing& right) {
            return left == right;
          },
          py::arg("other"));
  copyProtocol(timing);

  auto schedule = py::class_<motion::Schedule>(
      module, "Schedule",
      "A timing resolved for a frame's unit counts: the delay ladder, the "
      "beat length and the span one master progress maps onto.");
  schedule
      .def(py::init<const motion::Timing&, uint32_t, uint32_t>(),
           py::arg("timing"), py::arg("outerCount"), py::arg("innerCount") = 1)
      .def("total", &motion::Schedule::total)
      .def("start", &motion::Schedule::start, py::arg("outerUnit"),
           py::arg("innerUnit") = 0)
      .def("localProgress", &motion::Schedule::localProgress,
           py::arg("master"), py::arg("outerUnit"), py::arg("innerUnit") = 0)
      .def("beat", &motion::Schedule::beat, py::arg("master"),
           py::arg("outerUnit"), py::arg("innerUnit") = 0)
      .def("copy", [](const motion::Schedule& value) { return value; });
  copyProtocol(schedule);
}

}  // namespace sigil::python
