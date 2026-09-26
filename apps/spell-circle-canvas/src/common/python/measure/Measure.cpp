/** @file
 * SigilMeasure's front door in Python: the stopwatch and a timed block;
 * the summary, quantile and histogram of a run in hand; the window,
 * smoothed reading and rate of a live stream; and the check table. Every
 * span of time crosses as seconds, and a run is any iterable, read
 * through an optional `key` the way `sorted()` reads one.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigilmeasure/Measure.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/measure/Registration.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

/** @p values read as numbers, each through @p key when one is given. */
std::vector<double> numbersOf(const py::iterable& values,
                              const std::optional<py::function>& key) {
  std::vector<double> numbers;
  for (const py::handle value : values)
    numbers.push_back(key ? (*key)(value).cast<double>()
                          : value.cast<double>());
  return numbers;
}

std::vector<double> copied(std::span<const double> values) {
  return {values.begin(), values.end()};
}

std::vector<double> seconds(std::span<const measure::Duration> spans) {
  std::vector<double> out;
  out.reserve(spans.size());
  for (const measure::Duration span : spans) out.push_back(span.count());
  return out;
}

void bindTime(py::module_& module) {
  py::class_<measure::Stopwatch>(
      module, "Stopwatch",
      "The time since it started, on the steady clock: construction starts "
      "it and restart() starts it again.")
      .def(py::init<>())
      .def("elapsed", &measure::Stopwatch::elapsed,
           "The seconds since construction or the last restart.")
      .def("restart", &measure::Stopwatch::restart,
           "Starts the span again from now.");

  module.def(
      "timed",
      [](const py::function& block) {
        return measure::timed([&] { block(); });
      },
      py::arg("block"),
      "How many seconds block took, called once; what it returns is "
      "discarded.");
}

void bindRun(py::module_& module) {
  py::class_<measure::Summary>(
      module, "Summary",
      "How many, how large and how spread: a run folded in one value at a "
      "time, keeping none of them. Two summaries merge into the summary of "
      "both runs.")
      .def(py::init<>())
      .def("add", &measure::Summary::add, py::arg("value"))
      .def("merge", &measure::Summary::merge, py::arg("other"))
      .def("clear", &measure::Summary::clear)
      .def("count", &measure::Summary::count)
      .def("__len__", &measure::Summary::count)
      .def("empty", &measure::Summary::empty)
      .def("sum", &measure::Summary::sum)
      .def("mean", &measure::Summary::mean)
      .def("min", &measure::Summary::min)
      .def("max", &measure::Summary::max)
      .def("range", &measure::Summary::range)
      .def("deviation", &measure::Summary::deviation,
           "The spread in the values' own units, the values taken as the "
           "whole of what there is.");

  module.def(
      "summary",
      [](const py::iterable& values, const std::optional<py::function>& key) {
        return measure::summary(numbersOf(values, key));
      },
      py::arg("values"), py::arg("key") = py::none(),
      "The summary of a run in hand; key picks the number out of each "
      "element.");

  module.def(
      "quantile",
      [](const py::iterable& values, double fraction,
         const std::optional<py::function>& key) {
        return measure::quantile(numbersOf(values, key), fraction);
      },
      py::arg("values"), py::arg("fraction"), py::arg("key") = py::none(),
      "The value at fraction of the sorted run, interpolated between the two "
      "ranks it falls between: 0.5 of [1, 2, 3, 4] is 2.5, and 0.99 is the "
      "tail. Empty reads 0.");

  py::class_<measure::Histogram>(
      module, "Histogram",
      "Equal bins across a range and the weight that fell in each; what "
      "falls outside is counted in below() and above(), never clamped.")
      .def(py::init([](double low, double high, size_t bins) {
             return measure::Histogram({.low = low, .high = high, .bins = bins});
           }),
           py::kw_only(), py::arg("low") = 0.0, py::arg("high") = 0.0,
           py::arg("bins") = 20)
      .def_static(
          "over",
          [](const py::iterable& values, size_t bins, double low, double high,
             const std::optional<py::function>& key) {
            return measure::Histogram::over(
                numbersOf(values, key),
                {.low = low, .high = high, .bins = bins});
          },
          py::arg("values"), py::kw_only(), py::arg("bins") = 20,
          py::arg("low") = 0.0, py::arg("high") = 0.0,
          py::arg("key") = py::none(),
          "The histogram of a run in hand, over the range the run covers "
          "unless low and high state one.")
      .def("add", &measure::Histogram::add, py::arg("value"),
           py::arg("weight") = 1.0)
      .def("clear", &measure::Histogram::clear)
      .def("binContaining", &measure::Histogram::binContaining,
           py::arg("value"))
      .def("bins", &measure::Histogram::bins)
      .def("low", &measure::Histogram::low)
      .def("high", &measure::Histogram::high)
      .def("binWidth", &measure::Histogram::binWidth)
      .def("edge", &measure::Histogram::edge, py::arg("bin"))
      .def("center", &measure::Histogram::center, py::arg("bin"))
      .def("counts",
           [](const measure::Histogram& histogram) {
             return copied(histogram.counts());
           })
      .def("count", &measure::Histogram::count, py::arg("bin"))
      .def("fraction", &measure::Histogram::fraction, py::arg("bin"))
      .def("density", &measure::Histogram::density, py::arg("bin"))
      .def("total", &measure::Histogram::total)
      .def("below", &measure::Histogram::below)
      .def("above", &measure::Histogram::above)
      .def("mode", &measure::Histogram::mode)
      .def("peak", &measure::Histogram::peak);
}

void bindStream(py::module_& module) {
  using Window = measure::Window<double>;
  py::class_<Window>(
      module, "Window",
      "The last few values of a stream, oldest first: the last count, what "
      "was stamped in the last span seconds, or both.")
      .def(py::init([](size_t count, measure::Duration span) {
             // No bound at all is the last 120, as the one-number form is.
             if (count == 0 && span <= measure::Duration::zero()) count = 120;
             return Window({.count = count, .span = span});
           }),
           py::arg("count") = 0, py::kw_only(), py::arg("span") = 0.0)
      .def(
          "add",
          [](Window& window, double value,
             std::optional<measure::Duration> at) {
            if (at)
              window.add(value, *at);
            else
              window.add(value);
          },
          py::arg("value"), py::arg("at") = py::none(),
          "Adds value, stamped at the time at when the window has a span.")
      .def("advance", &Window::advance, py::arg("now"),
           "Moves the window's time on with nothing added.")
      .def("clear", &Window::clear)
      .def("size", &Window::size)
      .def("__len__", &Window::size)
      .def("empty", &Window::empty)
      .def("mean", &Window::mean)
      .def("min", &Window::min)
      .def("max", &Window::max)
      .def("quantile", &Window::quantile, py::arg("fraction"))
      .def("latest", &Window::latest)
      .def(
          "values",
          [](const Window& window) { return copied(window.values()); },
          "The held values, oldest first — a sparkline's run.")
      .def(
          "times",
          [](const Window& window) { return seconds(window.times()); },
          "When each held value was stamped, in seconds, oldest first.");

  py::class_<measure::Smoothed>(
      module, "Smoothed",
      "A value followed slowly: the exponential mean over about the last "
      "`over` values, a stated `weight`, or a `timeConstant` in seconds; "
      "with peak, a marker that rises at once and falls slowly.")
      .def(py::init([](double over, measure::Duration timeConstant,
                       double weight, bool peak) {
             return measure::Smoothed({.over = over,
                                       .timeConstant = timeConstant,
                                       .weight = weight,
                                       .peak = peak});
           }),
           py::arg("over") = 0.0, py::kw_only(),
           py::arg("timeConstant") = 0.0, py::arg("weight") = 0.0,
           py::arg("peak") = false)
      .def(
          "add",
          [](measure::Smoothed& smoothed, double value,
             std::optional<measure::Duration> elapsed) {
            if (elapsed)
              smoothed.add(value, *elapsed);
            else
              smoothed.add(value);
          },
          py::arg("value"), py::arg("elapsed") = py::none())
      .def("clear", &measure::Smoothed::clear)
      .def("value", &measure::Smoothed::value)
      .def("empty", &measure::Smoothed::empty);

  py::class_<measure::Rate>(
      module, "Rate", "Events per second over the last span seconds.")
      .def(py::init<measure::Duration>(), py::arg("span") = 1.0)
      .def("mark", &measure::Rate::mark, py::arg("at"))
      .def("advance", &measure::Rate::advance, py::arg("now"))
      .def("clear", &measure::Rate::clear)
      .def("count", &measure::Rate::count)
      .def("perSecond", &measure::Rate::perSecond)
      .def("times", [](const measure::Rate& rate) {
        return seconds(rate.times());
      });
}

void bindChecks(py::module_& module) {
  py::enum_<measure::Standing>(module, "Standing",
                               "What a row's verdict means to its run.")
      .value("Claim", measure::Standing::Claim)
      .value("Finding", measure::Standing::Finding)
      .value("Reading", measure::Standing::Reading)
      .value("Heading", measure::Standing::Heading);

  py::class_<measure::Check>(
      module, "Check",
      "One claim, its evidence and its verdict, computed from the values it "
      "reports.")
      .def_readonly("label", &measure::Check::label)
      .def_readonly("expected", &measure::Check::expected)
      .def_readonly("actual", &measure::Check::actual)
      .def_readonly("passed", &measure::Check::pass)
      .def_readonly("standing", &measure::Check::standing)
      .def("judged", &measure::Check::judged);

  // Overloads in the order a value is best told apart: a bare condition,
  // two whole counts, two strings, then two reals with their tolerance.
  module.def(
      "check",
      [](std::string label, bool condition) {
        return measure::check(std::move(label), condition);
      },
      py::arg("label"), py::arg("condition"));
  module.def(
      "check",
      [](std::string label, long long expected, long long actual) {
        return measure::check(std::move(label), expected, actual);
      },
      py::arg("label"), py::arg("expected"), py::arg("actual"));
  module.def(
      "check",
      [](std::string label, std::string expected, std::string actual) {
        return measure::check(std::move(label), std::string_view(expected),
                              std::string_view(actual));
      },
      py::arg("label"), py::arg("expected"), py::arg("actual"));
  module.def(
      "check",
      [](std::string label, double expected, double actual, double tolerance) {
        return measure::check(std::move(label), expected, actual, tolerance);
      },
      py::arg("label"), py::arg("expected"), py::arg("actual"),
      py::arg("tolerance"),
      "Real agreement within tolerance, which has no default: how closely "
      "two numbers must agree is the construction's to say.");
  module.def("finding", &measure::finding, py::arg("claim"),
             "The claim restated as a finding about the subject, never "
             "counted against the run.");
  module.def(
      "reading",
      [](std::string label, std::string value) {
        return measure::reading(std::move(label), std::move(value));
      },
      py::arg("label"), py::arg("value"));
  module.def(
      "reading",
      [](std::string label, long long value) {
        return measure::reading(std::move(label), value);
      },
      py::arg("label"), py::arg("value"));
  module.def(
      "reading",
      [](std::string label, double value) {
        return measure::reading(std::move(label), value);
      },
      py::arg("label"), py::arg("value"));
  module.def("heading", &measure::heading, py::arg("title"));

  py::class_<measure::CheckTable>(
      module, "CheckTable",
      "A run of checks printed as one table, ending with its verdict.")
      .def(py::init<>())
      .def(
          "add",
          [](measure::CheckTable& table,
             const measure::Check& row) -> measure::CheckTable& {
            return table.add(row);
          },
          py::arg("row"), py::return_value_policy::reference_internal)
      .def_readonly("rows", &measure::CheckTable::rows)
      .def("failures", &measure::CheckTable::failures)
      .def("findings", &measure::CheckTable::findings)
      .def("checks", &measure::CheckTable::checks)
      .def("passed", &measure::CheckTable::pass,
           "Whether every claim held.")
      .def(
          "lines",
          [](const measure::CheckTable& table, int labelWidth,
             int valueWidth) {
            return table.lines(
                {.labelWidth = labelWidth, .valueWidth = valueWidth});
          },
          py::kw_only(), py::arg("labelWidth") = 44, py::arg("valueWidth") = 8);
}

}  // namespace

void bindMeasure(pybind11::module_& module) {
  py::module_ measure = submodule(module, "measure");
  bindTime(measure);
  bindRun(measure);
  bindStream(measure);
  bindChecks(measure);
}

}  // namespace sigil::python
