/** @file
 * The plot frame and every chart layer: a frame of two scales, the layers
 * a plot calls with it, and the plot that stacks them. A curve given as a
 * Python function is called as the plot is laid out; the readers of a run
 * of rows are called once, as the layer is made.
 */

#include <pybind11/stl.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Chart.h>

#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

namespace {

/** Calls @p held with the interpreter taken, reading its answer as
 *  @p Result through @p read. */
template <class Result, class Read, class... Arguments>
Result calledBack(const std::shared_ptr<sigil::python::PythonCallback>& held,
                  Read read, Arguments&&... arguments) {
  const py::gil_scoped_acquire lock;
  const sigil::python::CallbackBoundary scopes;
  try {
    return read(held->get()(std::forward<Arguments>(arguments)...));
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

std::shared_ptr<sigil::python::PythonCallback> held(py::handle function) {
  if (!PyCallable_Check(function.ptr()))
    throw py::type_error("A curve is a function of one number.");
  return sigil::python::retainCallback(
      py::reinterpret_borrow<py::function>(function));
}

/** A curve of one variable held as a Python function. */
sigil::core::Callable<double(double)> curve(py::handle function) {
  return [function = held(function)](double at) {
    return calledBack<double>(
        function, [](py::object value) { return value.cast<double>(); }, at);
  };
}

/** A point as a Python function answers one: a Datum or an (x, y). */
sketchKit::Datum datum(py::handle value) {
  if (py::isinstance<sketchKit::Datum>(value))
    return value.cast<sketchKit::Datum>();
  const auto pair = value.cast<std::pair<double, double>>();
  return {pair.first, pair.second};
}

/** What @p reader answers for @p row at @p index; the index where no
 *  reader is given, which is what a band, point or ordinal scale takes. */
double number(py::handle reader, py::handle row, std::size_t index) {
  if (reader.is_none()) return (double)index;
  return reader(row).cast<double>();
}

/** @p mark built for @p row at @p index — a function of the row, then of
 *  its index — or an empty box where none is given. */
compose::Element marked(py::handle mark, py::handle row, std::size_t index) {
  if (mark.is_none()) return compose::box();
  const int arity = py::module_::import("sigil._callbacks")
                        .attr("arity")(mark, 2)
                        .cast<int>();
  const py::object made = arity == 0   ? mark()
                          : arity == 1 ? mark(row)
                                       : mark(row, index);
  return sigil::python::node(made);
}

/** A layer: a native one, or a Python function of the frame, then of the
 *  plot's key, then of the layer's index, answering the element that
 *  draws it. */
sketchKit::Layer layerOf(py::handle value) {
  if (py::isinstance<sketchKit::Layer>(value))
    return value.cast<sketchKit::Layer>();
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error(
        "A plot's layer is a layer or a function of the frame, the key and "
        "the index.");
  const int arity = py::module_::import("sigil._callbacks")
                        .attr("arity")(value, 3)
                        .cast<int>();
  return [function = held(value), arity](const sketchKit::Plot& frame,
                                         std::string_view key,
                                         std::size_t index) {
    const auto read = [](py::object made) {
      return sigil::python::node(made);
    };
    if (arity == 0) return calledBack<compose::Element>(function, read);
    if (arity == 1) return calledBack<compose::Element>(function, read, frame);
    if (arity == 2)
      return calledBack<compose::Element>(function, read, frame,
                                          std::string(key));
    return calledBack<compose::Element>(function, read, frame,
                                        std::string(key), index);
  };
}

void bindFrame(py::module_& module) {
  py::enum_<sketchKit::Axis>(module, "Axis",
                             "Which of a frame's two scales a layer reads.")
      .value("X", sketchKit::Axis::X)
      .value("Y", sketchKit::Axis::Y);
  auto datumType = record<sketchKit::Datum>(module, "Datum");
  datumType.doc() = "One datum, in the frame's own domain units.";
  field(datumType, "x", &sketchKit::Datum::x);
  field(datumType, "y", &sketchKit::Datum::y);
  datumType.def(
      "__eq__",
      [](const sketchKit::Datum& left, const sketchKit::Datum& right) {
        return left == right;
      },
      py::arg("other"));
  auto polar = record<sketchKit::Polar>(module, "Polar");
  polar.doc() =
      "Polar coordinates for a frame: the angles the x domain is carried "
      "onto, in degrees, and where the y domain's low end stands as a "
      "fraction of the outer radius.";
  field(polar, "sweep", &sketchKit::Polar::sweep);
  field(polar, "inner", &sketchKit::Polar::inner);
  auto plot = record<sketchKit::Plot>(module, "Plot");
  plot.doc() =
      "The frame every layer is a function of: what the two axes mean, the "
      "room left inside the box, and whether the box is read as a plane or "
      "as a wheel. The ranges are the box's and are filled in where the box "
      "is known.";
  field(plot, "x", &sketchKit::Plot::x);
  field(plot, "y", &sketchKit::Plot::y);
  field(plot, "pad", &sketchKit::Plot::pad);
  field(plot, "polar", &sketchKit::Plot::polar);
  plot.def(
          "scale",
          [](const sketchKit::Plot& frame, sketchKit::Axis which,
             py::handle size) {
            return frame.scale(which, sigil::python::kit::converted<SkSize>(size));
          },
          py::arg("which"), py::arg("size"))
      .def(
          "at",
          [](const sketchKit::Plot& frame, double xValue, double yValue,
             py::handle size) {
            const SkPoint point = frame.at(
                xValue, yValue, sigil::python::kit::converted<SkSize>(size));
            return py::make_tuple(point.x(), point.y());
          },
          py::arg("xValue"), py::arg("yValue"), py::arg("size"))
      .def("angle", &sketchKit::Plot::angle, py::arg("xValue"))
      .def("radiusFraction", &sketchKit::Plot::radiusFraction,
           py::arg("yValue"))
      .def(
          "__eq__",
          [](const sketchKit::Plot& left, const sketchKit::Plot& right) {
            return left == right;
          },
          py::arg("other"));
  py::class_<sketchKit::Layer>(
      module, "Layer",
      "One layer of a plot: a function of the frame, the plot's key and "
      "the layer's index, answering the element that draws it.");
}

void bindPaths(py::module_& module) {
  auto ruler = record<sketchKit::Ruler>(module, "Ruler");
  ruler.doc() =
      "How an axis is drawn: which scale it measures, where it stands on "
      "the other, what it ticks and how its ticks are numbered.";
  field(ruler, "of", &sketchKit::Ruler::of);
  field(ruler, "at", &sketchKit::Ruler::at);
  field(ruler, "ticks", &sketchKit::Ruler::ticks);
  field(ruler, "count", &sketchKit::Ruler::count);
  field(ruler, "line", &sketchKit::Ruler::line);
  field(ruler, "width", &sketchKit::Ruler::width);
  field(ruler, "reach", &sketchKit::Ruler::reach);
  field(ruler, "gap", &sketchKit::Ruler::gap);
  field(ruler, "numbers", &sketchKit::Ruler::numbers);
  module.def("axis", &sketchKit::axis, py::arg("how"));

  auto rules = record<sketchKit::Rules>(module, "Rules");
  rules.doc() =
      "Hairlines across the field at the domain values they name, stroked "
      "with one pen.";
  field(rules, "x", &sketchKit::Rules::x);
  field(rules, "y", &sketchKit::Rules::y);
  field(rules, "pen", &sketchKit::Rules::pen);
  field(rules, "styleClass", &sketchKit::Rules::styleClass);
  module.def("rules", &sketchKit::rules, py::arg("how"));

  auto trace = record<sketchKit::Trace>(module, "Trace");
  trace.doc() =
      "How a curve is drawn: its pen, how finely it is walked, how much of "
      "it is drawn, and the class it reads.";
  field(trace, "pen", &sketchKit::Trace::pen);
  field(trace, "samples", &sketchKit::Trace::samples);
  field(trace, "along", &sketchKit::Trace::along);
  field(trace, "styleClass", &sketchKit::Trace::styleClass);
  module.def(
      "trace",
      [](py::handle curveOrSeries, const sketchKit::Trace& how) {
        if (PyCallable_Check(curveOrSeries.ptr()))
          return sketchKit::trace(curve(curveOrSeries), how);
        const auto series = curveOrSeries.cast<std::vector<double>>();
        return sketchKit::trace(std::span<const double>(series), how);
      },
      py::arg("curve"), py::arg("how") = sketchKit::Trace{},
      "A curve: a function of one number walked across the x domain, or a "
      "measured series spread across it, first sample to last.");

  auto area = record<sketchKit::Area>(module, "Area");
  area.doc() = "The band between a curve and a base.";
  field(area, "base", &sketchKit::Area::base);
  field(area, "samples", &sketchKit::Area::samples);
  field(area, "styleClass", &sketchKit::Area::styleClass);
  module.def(
      "area",
      [](py::handle function, const sketchKit::Area& how) {
        return sketchKit::area(curve(function), how);
      },
      py::arg("curve"), py::arg("how") = sketchKit::Area{});

  auto path = record<sketchKit::Path>(module, "Path");
  path.doc() =
      "A curve walked over a parameter in both coordinates: its pen, its "
      "sampling, the parameter's interval and how much of it is drawn.";
  field(path, "pen", &sketchKit::Path::pen);
  field(path, "samples", &sketchKit::Path::samples);
  field(path, "over", &sketchKit::Path::over);
  field(path, "along", &sketchKit::Path::along);
  field(path, "styleClass", &sketchKit::Path::styleClass);
  module.def(
      "path",
      [](py::handle function, const sketchKit::Path& how) {
        return sketchKit::path(
            [function = held(function)](double at) {
              return calledBack<sketchKit::Datum>(
                  function, [](py::object value) { return datum(value); }, at);
            },
            how);
      },
      py::arg("at"), py::arg("how") = sketchKit::Path{},
      "A parametric curve: at is a function of the parameter answering a "
      "Datum or an (x, y).");
}

void bindPlacing(py::module_& module) {
  auto anchor = record<sketchKit::Anchor>(module, "Anchor");
  anchor.doc() =
      "Where a placed element sits on the point its datum maps to: centred, "
      "or its near or far edge there, per axis, and moved by an offset.";
  field(anchor, "across", &sketchKit::Anchor::across);
  field(anchor, "down", &sketchKit::Anchor::down);
  field(anchor, "offset", &sketchKit::Anchor::offset);
  auto label = record<sketchKit::Label>(module, "Label");
  label.doc() = "How a word placed in the field stands.";
  field(label, "anchor", &sketchKit::Label::anchor);
  field(label, "styleClass", &sketchKit::Label::styleClass);
  module.def(
      "label",
      [](const std::string& words, double x, double y,
         const sketchKit::Label& how) {
        return sketchKit::label(compose::Utf8(words), x, y, how);
      },
      py::arg("words"), py::arg("x"), py::arg("y"),
      py::arg("how") = sketchKit::Label{});

  module.def(
      "marks",
      [](py::iterable rows, py::handle mark, py::handle x, py::handle y,
         const sketchKit::Anchor& anchor, const std::string& styleClass) {
        std::vector<sketchKit::Datum> data;
        std::vector<compose::Element> children;
        std::size_t index = 0;
        for (py::handle row : rows) {
          data.push_back({number(x, row, index), number(y, row, index)});
          children.push_back(marked(mark, row, index));
          ++index;
        }
        return sketchKit::detail::anchored(
            std::move(data), std::move(children), anchor, "mark",
            sketchKit::detail::classOf(styleClass, "plotMark"));
      },
      py::arg("rows"), py::arg("mark") = py::none(), py::kw_only(),
      py::arg("x") = py::none(), py::arg("y") = py::none(),
      py::arg("anchor") = sketchKit::Anchor{}, py::arg("styleClass") = "",
      "One element per row, placed where the frame maps the row's datum: "
      "x and y read a row's two numbers, its index where one is not given, "
      "and mark builds the row's element from the row and its index.");
  module.def(
      "segments",
      [](py::iterable rows, py::handle mark, py::handle x, py::handle y,
         py::handle toX, py::handle toY, const std::string& styleClass) {
        std::vector<sketchKit::Datum> ends;
        std::vector<compose::Element> children;
        std::size_t index = 0;
        for (py::handle row : rows) {
          ends.push_back({number(x, row, index), number(y, row, index)});
          ends.push_back({number(toX, row, index), number(toY, row, index)});
          children.push_back(marked(mark, row, index));
          ++index;
        }
        return sketchKit::detail::between(
            std::move(ends), std::move(children), "segment",
            sketchKit::detail::classOf(styleClass, "plotMark"));
      },
      py::arg("rows"), py::arg("mark") = py::none(), py::kw_only(),
      py::arg("x") = py::none(), py::arg("y") = py::none(),
      py::arg("toX") = py::none(), py::arg("toY") = py::none(),
      py::arg("styleClass") = "",
      "One line per row from its first point to its other, each standing at "
      "the bounds of its own two ends.");
  module.def(
      "bands",
      [](py::iterable rows, py::handle x, py::handle y, double base,
         float corners, sketchKit::Axis along, py::handle part,
         const std::string& styleClass) {
        std::vector<sketchKit::Datum> data;
        std::size_t index = 0;
        for (py::handle row : rows) {
          data.push_back({number(x, row, index), number(y, row, index)});
          ++index;
        }
        compose::kit::Part<std::size_t, double> drawn;
        if (!part.is_none()) {
          const int arity = py::module_::import("sigil._callbacks")
                                .attr("arity")(part, 2)
                                .cast<int>();
          drawn = [function = held(part), arity](const std::size_t& at,
                                                 const double& value) {
            const auto read = [](py::object made) {
              return sigil::python::node(made);
            };
            if (arity == 0) return calledBack<compose::Element>(function, read);
            if (arity == 1)
              return calledBack<compose::Element>(function, read, at);
            return calledBack<compose::Element>(function, read, at, value);
          };
        }
        return sketchKit::detail::banded(
            std::move(data), base, corners, along, std::move(drawn), "bar",
            sketchKit::detail::classOf(styleClass, "plotBar"));
      },
      py::arg("rows"), py::kw_only(), py::arg("x") = py::none(),
      py::arg("y") = py::none(), py::arg("base") = 0.0,
      py::arg("corners") = 0.0f, py::arg("along") = sketchKit::Axis::X,
      py::arg("part") = py::none(), py::arg("styleClass") = "",
      "The band each row owns, drawn out from base to the row's value: a "
      "column, a row reading along Y, or a wedge on a polar frame; part "
      "builds a band's element from its index and its value.");

  module.def(
      "plot",
      [](const std::string& key, const sketchKit::Plot& frame,
         py::iterable layers) {
        std::vector<sketchKit::Layer> run;
        for (py::handle layer : layers) run.push_back(layerOf(layer));
        return sketchKit::plot(key, frame, std::move(run));
      },
      py::arg("key"), py::arg("frame"), py::arg("layers"),
      "The plot: the frame, and its layers over it in the order written, "
      "each filling the plot's own box. It does not place or size itself.");
}

}  // namespace

void bindSketchKitCharts(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  bindFrame(module);
  bindPaths(module);
  bindPlacing(module);
}

}  // namespace sigil::sketch::python
