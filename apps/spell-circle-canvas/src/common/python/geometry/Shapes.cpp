#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/kit/Hatches.h>
#include <sigilgeometry/kit/Radial.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryShapes(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ shapes = submodule(module, "geometry.shapes");

  // The one hatch value: where a natural-media brush lays its marks and
  // what a decoration strokes. Its angle is radians.
  auto hatch = bindRecord<geometry::shapes::Hatch>(shapes, "Hatch",
                                                   "Unknown hatch property: ");
  hatch.def_readwrite("spacing", &geometry::shapes::Hatch::spacing)
      .def_readwrite("angle", &geometry::shapes::Hatch::angle)
      .def_readwrite("taper", &geometry::shapes::Hatch::taper)
      .def_readwrite("origin", &geometry::shapes::Hatch::origin)
      .def_readwrite("inset", &geometry::shapes::Hatch::inset)
      .def_readwrite("cross", &geometry::shapes::Hatch::cross)
      .def_readwrite("maxLines", &geometry::shapes::Hatch::maxLines)
      .def(py::self == py::self);

  namespace kit = geometry::shapes;
  namespace path = geometry::path;

  // The two general shapes over a box and an outline fitted to one; each
  // answers its outline for a size, and draws about a centre at a radius.
  py::class_<kit::Radial> radial(shapes, "Radial");
  radial.def_readwrite("count", &kit::Radial::count)
      .def_readwrite("options", &kit::Radial::options)
      .def("outline", &kit::Radial::outline, py::arg("size"))
      .def("points", &kit::Radial::points, py::arg("size"))
      .def("at", &kit::Radial::at, py::arg("centre"), py::arg("radius"))
      .def(py::self == py::self);
  copyProtocol(radial);

  py::enum_<kit::Close>(shapes, "Close")
      .value("Open", kit::Close::Open)
      .value("Chord", kit::Close::Chord)
      .value("Pie", kit::Close::Pie);
  auto options = bindRecord<kit::EllipseOptions>(shapes, "EllipseOptions",
                                                 "Unknown ellipse option: ");
  options.def_readwrite("fromDegrees", &kit::EllipseOptions::fromDegrees)
      .def_readwrite("sweepDegrees", &kit::EllipseOptions::sweepDegrees)
      .def_readwrite("close", &kit::EllipseOptions::close)
      .def_readwrite("inner", &kit::EllipseOptions::inner)
      .def_readwrite("thickness", &kit::EllipseOptions::thickness)
      .def_readwrite("dot", &kit::EllipseOptions::dot)
      .def_readwrite("exponent", &kit::EllipseOptions::exponent)
      .def_readwrite("inset", &kit::EllipseOptions::inset)
      .def_readwrite("uniform", &kit::EllipseOptions::uniform)
      .def_readwrite("winding", &kit::EllipseOptions::winding)
      .def_readwrite("start", &kit::EllipseOptions::start)
      .def(py::self == py::self);
  py::class_<kit::Ellipse> ellipse(shapes, "Ellipse");
  ellipse.def_readwrite("options", &kit::Ellipse::options)
      .def("outline", &kit::Ellipse::outline, py::arg("size"))
      .def("at", &kit::Ellipse::at, py::arg("centre"), py::arg("radius"))
      .def(py::self == py::self);
  copyProtocol(ellipse);
  py::class_<kit::Fitted> fitted(shapes, "Fitted");
  fitted.def_readwrite("source", &kit::Fitted::source)
      .def("outline", &kit::Fitted::outline, py::arg("size"))
      .def("at", &kit::Fitted::at, py::arg("centre"), py::arg("radius"))
      .def(py::self == py::self);
  copyProtocol(fitted);

  shapes
      .def("radial", &kit::radial, py::arg("count"),
           py::arg("options") = path::RadialOptions{})
      .def("ellipse", &kit::ellipse,
           py::arg("options") = kit::EllipseOptions{})
      .def(
          "fitted",
          [](const path::Outline& source, bool preserveAspect) {
            return kit::fitted(source, {.preserveAspect = preserveAspect});
          },
          py::arg("source"), py::arg("preserveAspect") = false)
      // The stock values: each the general form with its options fixed.
      .def("polygon", &kit::polygon, py::arg("sides"),
           py::arg("rotationDegrees") = 0.0f)
      .def("star", &kit::star, py::arg("points"), py::arg("innerRatio") = 0.5f,
           py::arg("waist") = 0.0f)
      .def(
          "circle",
          [](float inset, path::Winding winding, unsigned start) {
            return kit::circle(winding, start, inset);
          },
          py::arg("inset") = 0.0f,
          py::arg("winding") = path::Winding::OutersClockwise,
          py::arg("start") = 1u)
      .def("annulus", &kit::annulus, py::arg("innerRatio") = 0.6f)
      .def("ring", &kit::ring, py::arg("thickness"), py::arg("dot") = 0.0f)
      .def("squircle", &kit::squircle, py::arg("exponent") = 4.0f)
      .def("arc", &kit::arc, py::arg("startDegrees"),
           py::arg("sweepDegrees") = 359.9f)
      .def("sector", &kit::sector, py::arg("startDegrees"),
           py::arg("sweepDegrees"), py::arg("innerRatio") = 0.0f)
      .def("svg", &kit::svg, py::arg("data"),
           py::arg("preserveAspect") = false);
}

}  // namespace sigil::python
