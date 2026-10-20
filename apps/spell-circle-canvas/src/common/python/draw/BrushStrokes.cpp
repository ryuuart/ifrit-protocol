/** @file
 * The brush's stock tools and stroke functions in Python: pencil,
 * charcoal, marker, watercolor and spray, and the functions that sample,
 * trace, warp and deposit strokes on a pen.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <vector>

#include "BrushBinding.h"

namespace sigil::python {
using namespace brush_binding;

void bindBrushStrokes(py::module_& module) {
  module.def(
      "pencil",
      [](py::handle pigment, float width) {
        return brush::pencil(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 1.4f);
  module.def(
      "charcoal",
      [](py::handle pigment, float width) {
        return brush::charcoal(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 9.0f);
  module.def(
      "marker",
      [](py::handle pigment, float width) {
        return brush::marker(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 16.0f);
  module.def(
      "watercolor",
      [](py::handle pigment, float width) {
        return brush::watercolor(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 22.0f);
  module.def(
      "spray",
      [](py::handle pigment, float width) {
        return brush::spray(color(pigment), width);
      },
      py::arg("color"), py::arg("width") = 18.0f);

  module.def("spacingOf", &brush::spacingOf, py::arg("tool"));
  module.def(
      "prepareStroke",
      [](BorrowedPen& pen, const brush::Tool& tool) {
        return brush::prepareStroke(pen.get(), tool);
      },
      py::arg("pen"), py::arg("tool"));
  module.def(
      "segment",
      [](py::handle from, py::handle to, float spacing, float p0, float p1) {
        return brush::segment(point(from), point(to), spacing, p0, p1);
      },
      py::arg("from_"), py::arg("to"), py::arg("spacing") = 1.0f,
      py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f);
  module.def(
      "spline",
      [](py::iterable controls, float spacing, float curvature) {
        return brush::spline(samples(controls), spacing, curvature);
      },
      py::arg("controls"), py::arg("spacing") = 1.0f,
      py::arg("curvature") = 0.5f);
  module.def(
      "spline",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable controls,
         float curvature) {
        brush::spline(pen.get(), tool, samples(controls), curvature);
      },
      py::arg("pen"), py::arg("tool"), py::arg("controls"),
      py::arg("curvature") = 0.5f);
  module.def(
      "line",
      [](BorrowedPen& pen, const brush::Tool& tool, py::handle from,
         py::handle to, float p0, float p1) {
        brush::line(pen.get(), tool, point(from), point(to), p0, p1);
      },
      py::arg("pen"), py::arg("tool"), py::arg("from_"), py::arg("to"),
      py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f);
  module.def(
      "paint",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable stroke) {
        brush::paint(pen.get(), tool, samples(stroke));
      },
      py::arg("pen"), py::arg("tool"), py::arg("stroke"));
  module.def(
      "trace",
      [](py::handle start, float length, float spacing, float seconds,
         py::handle field, float pressure) {
        return brush::trace(point(start), length, spacing, seconds,
                            direction(field), pressure);
      },
      py::arg("start"), py::arg("length"), py::arg("spacing"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "warp",
      [](py::iterable polygon, float spacing, float amount, float seconds,
         py::handle field, float pressure) {
        return brush::warp(points(polygon), spacing, amount, seconds,
                           direction(field), pressure);
      },
      py::arg("polygon"), py::arg("spacing"), py::arg("amount"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "flowLine",
      [](BorrowedPen& pen, const brush::Tool& tool, py::handle start,
         float length, float seconds, py::handle field, float pressure) {
        brush::flowLine(pen.get(), tool, point(start), length, seconds,
                        direction(field), pressure);
      },
      py::arg("pen"), py::arg("tool"), py::arg("start"), py::arg("length"),
      py::arg("seconds"), py::arg("field"), py::arg("pressure") = 1.0f);
  module.def(
      "dabs",
      [](const std::vector<brush::Input>& input, float spacing,
         float speedFilterSeconds) {
        return brush::dabs(input, spacing, speedFilterSeconds);
      },
      py::arg("input"), py::arg("spacing"),
      py::arg("speedFilterSeconds") = brush::kSpeedFilterSeconds);
  module.def(
      "deposit",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Dab>& dabs, brush::DepositOptions options) {
        brush::deposit(pen.get(), tool, dabs, options);
      },
      py::arg("pen"), py::arg("tool"), py::arg("dabs"),
      py::arg("options") = brush::DepositOptions{});
  module.def(
      "wash",
      [](BorrowedPen& pen, const brush::Wash& pigment, py::iterable polygon) {
        brush::wash(pen.get(), pigment, points(polygon));
      },
      py::arg("pen"), py::arg("pigment"), py::arg("polygon"));
  module.def(
      "hatch",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable polygon,
         const brush::Hatch& style) {
        brush::hatch(pen.get(), tool, points(polygon), style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygon"),
      py::arg("style") = brush::Hatch{});
  module.def(
      "hatchArray",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Polygon>& polygons,
         const brush::Hatch& style) {
        brush::hatchArray(pen.get(), tool, polygons, style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygons"),
      py::arg("style") = brush::Hatch{});
  module.def(
      "mass",
      [](BorrowedPen& pen, const brush::Tool& tool, py::iterable polygon,
         const brush::Mass& style) {
        brush::mass(pen.get(), tool, points(polygon), style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygon"),
      py::arg("style") = brush::Mass{});
  module.def(
      "massArray",
      [](BorrowedPen& pen, const brush::Tool& tool,
         const std::vector<brush::Polygon>& polygons,
         const brush::Mass& style) {
        brush::massArray(pen.get(), tool, polygons, style);
      },
      py::arg("pen"), py::arg("tool"), py::arg("polygons"),
      py::arg("style") = brush::Mass{});

  module.def(
      "randomBelow",
      [](BorrowedPen& pen, float total) {
        return brush::randomBelow(pen.get(), total);
      },
      py::arg("pen"), py::arg("total"));
  module.attr("Stroke") = py::module_::import("builtins").attr("list");
}

}  // namespace sigil::python
