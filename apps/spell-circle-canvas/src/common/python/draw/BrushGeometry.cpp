/** @file
 * The brush's sampled geometry in Python: the polygon, the plot and the
 * place it is drawn at, the position that walks a field, and the sampler
 * that turns input into dabs.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <optional>

#include "BrushBinding.h"

namespace sigil::python {
using namespace brush_binding;

void bindBrushGeometry(py::module_& module) {
  py::class_<brush::Polygon>(module, "Polygon")
      .def(py::init<>())
      .def(py::init([](py::iterable vertices) {
             return brush::Polygon(points(vertices));
           }),
           py::arg("vertices"))
      .def_property(
          "vertices",
          [](const brush::Polygon& value) { return value.vertices; },
          [](brush::Polygon& value, py::iterable vertices) {
            value.vertices = points(vertices);
          })
      .def(
          "intersect",
          [](const brush::Polygon& polygon, py::handle start, py::handle end) {
            return polygon.intersect(point(start), point(end));
          },
          py::arg("start"), py::arg("end"))
      .def("translated", &brush::Polygon::translated, py::arg("x"),
           py::arg("y"))
      .def("empty", &brush::Polygon::empty)
      .def("copy", [](const brush::Polygon& value) { return value; });

  py::class_<brush::Plot>(module, "Plot")
      .def(py::init<brush::PlotType>(),
           py::arg("type") = brush::PlotType::Curve)
      .def("type", &brush::Plot::type)
      .def("addSegment", &brush::Plot::addSegment, py::arg("angle"),
           py::arg("length"), py::arg("pressure") = 1.0f)
      .def("endPlot", &brush::Plot::endPlot, py::arg("angle"),
           py::arg("pressure") = 1.0f)
      .def("rotate", &brush::Plot::rotate, py::arg("angle"))
      .def("length", &brush::Plot::length)
      .def("angle", &brush::Plot::angle, py::arg("distance"))
      .def("pressure", &brush::Plot::pressure, py::arg("distance"))
      .def("empty", &brush::Plot::empty)
      .def("copy", [](const brush::Plot& value) { return value; })
      .def(
          "path",
          [](const brush::Plot& value, py::handle origin, float spacing,
             float curvature, float scale) {
            return value.path(point(origin), spacing, curvature, scale);
          },
          py::arg("origin") = py::make_tuple(0, 0), py::arg("spacing") = 1.0f,
          py::arg("curvature") = 0.5f, py::arg("scale") = 1.0f)
      .def("polygon", &brush::Plot::polygon, py::arg("x") = 0.0f,
           py::arg("y") = 0.0f, py::arg("spacing") = 1.0f,
           py::arg("curvature") = 0.5f, py::arg("scale") = 1.0f)
      .def_static(
          "fromStroke",
          [](py::iterable stroke, brush::PlotType type) {
            return brush::Plot::fromStroke(samples(stroke), type);
          },
          py::arg("stroke"), py::arg("type") = brush::PlotType::Curve);

  auto placed = record<brush::PlacedPlot>(module, "PlacedPlot");
  field(placed, "plot", &brush::PlacedPlot::plot);
  field(placed, "origin", &brush::PlacedPlot::origin);
  placed.def("empty", &brush::PlacedPlot::empty);

  py::class_<brush::Position>(module, "Position")
      .def(py::init([](float x, float y, py::handle field, float seconds,
                       py::handle bounds) {
             return brush::Position(
                 x, y, direction(field), seconds,
                 bounds.is_none() ? std::nullopt : std::optional(rect(bounds)));
           }),
           py::arg("x") = 0.0f, py::arg("y") = 0.0f,
           py::arg("field") = py::none(), py::arg("seconds") = 0.0f,
           py::arg("bounds") = py::none())
      .def("x", &brush::Position::x)
      .def("y", &brush::Position::y)
      .def("plotted", &brush::Position::plotted)
      .def(
          "moveTo",
          [](brush::Position& position, float heading, float length, float step,
             py::handle field, float seconds) {
            return position.moveTo(heading, length, step, direction(field),
                                   seconds);
          },
          py::arg("direction"), py::arg("length"), py::arg("stepLength") = 1.0f,
          py::arg("field") = py::none(), py::arg("seconds") = 0.0f)
      .def("plotTo", &brush::Position::plotTo, py::arg("plot"),
           py::arg("length"), py::arg("stepLength"), py::arg("scale") = 1.0f)
      .def("angle", py::overload_cast<>(&brush::Position::angle, py::const_))
      .def(
          "angle",
          [](const brush::Position& position, py::handle field, float seconds) {
            return position.angle(direction(field), seconds);
          },
          py::arg("field"), py::arg("seconds") = 0.0f)
      .def("isIn", &brush::Position::isIn)
      .def("isInCanvas", &brush::Position::isInCanvas)
      .def("place", &brush::Position::place, py::arg("x"), py::arg("y"))
      .def("reset", &brush::Position::reset)
      .def(
          "field",
          [](brush::Position& position, py::handle value, float seconds) {
            position.field(direction(value), seconds);
          },
          py::arg("value"), py::arg("seconds") = 0.0f);

  py::class_<brush::Sampler>(module, "Sampler")
      .def(py::init<float>(),
           py::arg("speedFilterSeconds") = brush::kSpeedFilterSeconds)
      .def("begin", &brush::Sampler::begin, py::arg("input"))
      .def("move", &brush::Sampler::move, py::arg("input"), py::arg("spacing"))
      .def("end", &brush::Sampler::end, py::arg("input"), py::arg("spacing"))
      .def("cancel", &brush::Sampler::cancel)
      .def("active", &brush::Sampler::active)
      .def("distance", &brush::Sampler::distance);
}

}  // namespace sigil::python
