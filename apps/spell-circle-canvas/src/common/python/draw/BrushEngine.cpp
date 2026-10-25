/** @file
 * The brush engine in Python: the catalogue of named tools, and the
 * engine that holds the current stroke, fill, wash, hatch, mass, field and
 * clip and draws lines, shapes, plots and input with them.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <string>
#include <vector>

#include "BrushBinding.h"

namespace sigil::python {
using namespace brush_binding;

void bindBrushEngine(py::module_& module) {
  py::class_<brush::Catalogue>(module, "Catalogue")
      .def(py::init<>())
      .def_static("stock", &brush::Catalogue::stock)
      .def(
          "add",
          [](brush::Catalogue& catalogue, const std::string& name,
             const brush::Tool& tool) {
            return toolCopy(catalogue.add(name, tool));
          },
          py::arg("name"), py::arg("tool"))
      .def(
          "find",
          [](const brush::Catalogue& catalogue, const std::string& name) {
            return toolCopy(catalogue.find(name));
          },
          py::arg("name"))
      .def("contains", &brush::Catalogue::contains, py::arg("name"))
      .def("names", &brush::Catalogue::names)
      .def("scale", &brush::Catalogue::scale, py::arg("factor"));

  auto engine =
      py::class_<brush::Engine>(module, "Engine")
          .def(py::init<>())
          .def(py::init<brush::Catalogue>(), py::arg("catalogue"))
          .def(
              "add",
              [](brush::Engine& engine, const std::string& name,
                 const brush::Tool& tool) {
                return toolCopy(engine.add(name, tool));
              },
              py::arg("name"), py::arg("tool"))
          .def(
              "pick",
              [](brush::Engine& engine, const std::string& name) {
                return toolCopy(engine.pick(name));
              },
              py::arg("name"))
          .def(
              "set",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, float weight) {
                return toolCopy(engine.set(name, color(pigment), weight));
              },
              py::arg("name"), py::arg("color"), py::arg("weight") = 1.0f)
          .def("names", &brush::Engine::names)
          .def("scaleBrushes", &brush::Engine::scaleBrushes, py::arg("factor"))
          .def(
              "stroke",
              [](brush::Engine& engine, py::handle pigment) {
                engine.stroke(color(pigment));
              },
              py::arg("pigment"))
          .def("noStroke", &brush::Engine::noStroke)
          .def("strokeWeight", &brush::Engine::strokeWeight, py::arg("weight"))
          .def("hasStroke", &brush::Engine::hasStroke)
          .def("tool", py::overload_cast<>(&brush::Engine::tool, py::const_))
          .def(
              "fill",
              [](brush::Engine& engine, py::handle pigment, float opacity) {
                engine.fill(color(pigment), opacity);
              },
              py::arg("color"), py::arg("opacity") = 150.0f / 255.0f)
          .def(
              "wash",
              [](brush::Engine& engine, py::handle pigment, float opacity) {
                engine.wash(color(pigment), opacity);
              },
              py::arg("color"), py::arg("opacity") = 150.0f / 255.0f)
          .def("noFill", &brush::Engine::noFill)
          .def("noWash", &brush::Engine::noWash)
          .def(
              "fillBleed",
              [](brush::Engine& engine, float bleed,
                 brush::BleedDirection direction,
                 std::optional<AngleInRadians> angle) {
                engine.fillBleed(bleed, direction,
                                 angle ? std::optional<float>(angle->value)
                                       : std::optional<float>());
              },
              py::arg("bleed"),
              py::arg("direction") = brush::BleedDirection::Out,
              py::arg("angle") = py::none())
          .def("fillTexture", &brush::Engine::fillTexture,
               py::arg("texture") = 0.4f, py::arg("border") = 0.4f,
               py::arg("scatter") = true)
          .def("hatch",
               py::overload_cast<const brush::Hatch&>(&brush::Engine::hatch),
               py::arg("style") = brush::Hatch{})
          .def(
              "hatch",
              [](brush::Engine& engine, BorrowedPen& pen, float spacing,
                 AngleInPenMode angle, float jitter, float gradient,
                 bool continuous) {
                engine.hatch(pen.get(), spacing, angleOnPen(pen.get(), angle),
                             jitter, gradient, continuous);
              },
              py::arg("pen"), py::arg("spacing"), py::arg("angle"),
              py::arg("jitter") = 0.0f, py::arg("gradient") = 0.0f,
              py::arg("continuous") = false)
          .def("noHatch", &brush::Engine::noHatch)
          .def(
              "hatchStyle",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, float weight) {
                return toolCopy(
                    engine.hatchStyle(name, color(pigment), weight));
              },
              py::arg("name"), py::arg("color") = "#000000",
              py::arg("weight") = 1.0f)
          .def(
              "mass",
              [](brush::Engine& engine, const std::string& name,
                 py::handle pigment, const brush::Mass& style) {
                return toolCopy(engine.mass(name, color(pigment), style));
              },
              py::arg("name"), py::arg("color"),
              py::arg("style") = brush::Mass{})
          .def("noMass", &brush::Engine::noMass)
          .def(
              "addField",
              [](brush::Engine& engine, const std::string& name,
                 py::handle field, draw::Constant units) {
                return engine.addField(name, direction(field), units);
              },
              py::arg("name"), py::arg("field"),
              py::arg("units") = draw::RADIANS)
          .def("listFields", &brush::Engine::listFields)
          .def("field", &brush::Engine::field, py::arg("name"))
          .def("noField", &brush::Engine::noField)
          .def("wiggle", &brush::Engine::wiggle, py::arg("amount") = 1.0f)
          .def(
              "clip",
              [](brush::Engine& engine, py::handle region) {
                engine.clip(rect(region));
              },
              py::arg("region"))
          .def(
              "clip",
              [](brush::Engine& engine, BorrowedPen& pen, py::handle region) {
                engine.clip(pen.get(), rect(region));
              },
              py::arg("pen"), py::arg("region"))
          .def("noClip", &brush::Engine::noClip)
          .def("push", &brush::Engine::push)
          .def("pop", &brush::Engine::pop)
          .def(
              "paint",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::iterable path) { engine.paint(pen.get(), samples(path)); },
              py::arg("pen"), py::arg("path"))
          .def(
              "line",
              [](const brush::Engine& engine, BorrowedPen& pen, py::handle from,
                 py::handle to, float p0, float p1) {
                engine.line(pen.get(), point(from), point(to), p0, p1);
              },
              py::arg("pen"), py::arg("from_"), py::arg("to"),
              py::arg("startPressure") = 1.0f, py::arg("endPressure") = 1.0f)
          .def(
              "flowLine",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::handle start, float length, AngleInPenMode heading) {
                engine.flowLine(pen.get(), point(start), length,
                                angleOnPen(pen.get(), heading));
              },
              py::arg("pen"), py::arg("start"), py::arg("length"),
              py::arg("direction"))
          .def(
              "spline",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::iterable path, float curvature) {
                return engine.spline(pen.get(), samples(path), curvature);
              },
              py::arg("pen"), py::arg("controls"), py::arg("curvature") = 0.5f)
          .def(
              "polygon",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 py::iterable vertices) {
                return engine.polygon(pen.get(), points(vertices));
              },
              py::arg("pen"), py::arg("vertices"))
          .def(
              "polygon",
              [](const brush::Engine& engine, BorrowedPen& pen,
                 const brush::Polygon& polygon) {
                engine.polygon(pen.get(), polygon);
              },
              py::arg("pen"), py::arg("polygon"))
          .def(
              "rect",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float width, float height, draw::Constant mode) {
                engine.rect(pen.get(), x, y, width, height, mode);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("width"),
              py::arg("height"), py::arg("mode") = draw::CORNER)
          .def(
              "rect",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float width, float height, float radius) {
                engine.rect(pen.get(), x, y, width, height, radius);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("width"),
              py::arg("height"), py::arg("radius"))
          .def(
              "circle",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float radius, float irregularity) {
                return engine.circle(pen.get(), x, y, radius, irregularity);
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("radius"),
              py::arg("irregularity") = 0.0f)
          .def(
              "arc",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y, float radius, AngleInPenMode start,
                 AngleInPenMode stop) {
                return engine.arc(pen.get(), x, y, radius,
                                  angleOnPen(pen.get(), start),
                                  angleOnPen(pen.get(), stop));
              },
              py::arg("pen"), py::arg("x"), py::arg("y"), py::arg("radius"),
              py::arg("start"), py::arg("stop"))
          .def("beginShape", &brush::Engine::beginShape,
               py::arg("curvature") = 0.0f)
          .def("vertex", &brush::Engine::vertex, py::arg("x"), py::arg("y"),
               py::arg("pressure") = 1.0f)
          .def(
              "endShape",
              [](brush::Engine& engine, BorrowedPen& pen, bool close) {
                return engine.endShape(pen.get(), close);
              },
              py::arg("pen"), py::arg("close") = false)
          .def("position",
               py::overload_cast<float, float>(&brush::Engine::position,
                                               py::const_),
               py::arg("x") = 0.0f, py::arg("y") = 0.0f)
          .def(
              "position",
              [](const brush::Engine& engine, BorrowedPen& pen, float x,
                 float y) { return engine.position(pen.get(), x, y); },
              py::arg("pen"), py::arg("x") = 0.0f, py::arg("y") = 0.0f)
          .def(
              "beginStroke",
              [](brush::Engine& engine, brush::PlotType kind, py::handle at) {
                engine.beginStroke(kind, point(at));
              },
              py::arg("kind"), py::arg("at"))
          .def(
              "move",
              [](brush::Engine& engine, BorrowedPen& pen, AngleInPenMode angle,
                 float length, float pressure) {
                engine.move(pen.get(), angleOnPen(pen.get(), angle), length,
                            pressure);
              },
              py::arg("pen"), py::arg("angle"), py::arg("length"),
              py::arg("pressure") = 1.0f)
          .def(
              "endStroke",
              [](brush::Engine& engine, BorrowedPen& pen, AngleInPenMode angle,
                 float pressure) {
                return engine.endStroke(pen.get(), angleOnPen(pen.get(), angle),
                                        pressure);
              },
              py::arg("pen"), py::arg("angle"), py::arg("pressure") = 1.0f)
          .def("cancelStroke", &brush::Engine::cancelStroke)
          .def("cancelInput", &brush::Engine::cancelInput);
  engine.def(
      "beginInput",
      [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
        engine.beginInput(pen.get(), input);
      },
      py::arg("pen"), py::arg("input"));
  engine.def(
      "moveInput",
      [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
        engine.moveInput(pen.get(), input);
      },
      py::arg("pen"), py::arg("input"));
  engine.def(
      "endInput",
      [](brush::Engine& engine, BorrowedPen& pen, brush::Input input) {
        engine.endInput(pen.get(), input);
      },
      py::arg("pen"), py::arg("input"));
  engine
      .def(
          "draw",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Polygon& polygon) {
            engine.draw(pen.get(), polygon);
          },
          py::arg("pen"), py::arg("polygon"))
      .def(
          "draw",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.draw(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def(
          "fill",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Polygon& polygon) {
            engine.fill(pen.get(), polygon);
          },
          py::arg("pen"), py::arg("polygon"))
      .def(
          "fill",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.fill(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def(
          "wash",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Polygon& polygon) {
            engine.wash(pen.get(), polygon);
          },
          py::arg("pen"), py::arg("polygon"))
      .def(
          "wash",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.wash(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def(
          "hatch",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Polygon& polygon) {
            engine.hatch(pen.get(), polygon);
          },
          py::arg("pen"), py::arg("polygon"))
      .def(
          "hatch",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.hatch(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def(
          "mass",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Polygon& polygon) {
            engine.mass(pen.get(), polygon);
          },
          py::arg("pen"), py::arg("polygon"))
      .def(
          "mass",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const brush::Plot& plot, float x, float y,
             float scale) { engine.mass(pen.get(), plot, x, y, scale); },
          py::arg("pen"), py::arg("plot"), py::arg("x") = 0.0f,
          py::arg("y") = 0.0f, py::arg("scale") = 1.0f);
  engine
      .def(
          "hatchArray",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const std::vector<brush::Polygon>& polygons) {
            engine.hatchArray(pen.get(), polygons);
          },
          py::arg("pen"), py::arg("polygons"))
      .def(
          "massArray",
          [](const brush::Engine& engine, BorrowedPen& pen,
             const std::vector<brush::Polygon>& polygons) {
            engine.massArray(pen.get(), polygons);
          },
          py::arg("pen"), py::arg("polygons"));
}

}  // namespace sigil::python
