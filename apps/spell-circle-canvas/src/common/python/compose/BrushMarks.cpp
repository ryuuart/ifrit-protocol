#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Registration.h>

#include <optional>
#include <utility>

namespace sigil::python {

void bindComposeBrushMarks(pybind11::module_& module) {
  namespace py = pybind11;
  submodule(module, "compose.brush.strand");
  submodule(module, "compose.lines.presets");
  submodule(module, "compose.brush.presets");
  submodule(module, "compose.lines");
  using compose::brush::Ribbon;
  py::module_ brushes = submodule(module, "compose.brush");

  py::class_<Ribbon> ribbon(
      brushes, "Ribbon",
      "A filled band swept along the outline it strokes, as wide across "
      "the outline as its width law says at each place along it, joined at "
      "the corners.");
  ribbon
      .def_property(
          "fill", [](const Ribbon& self) { return self.fill; },
          [](Ribbon& self, py::object value) { self.fill = fill(value); })
      .def_property(
          "material", [](const Ribbon& self) { return self.fillMaterial; },
          [](Ribbon& self, std::optional<material::Material> value) {
            self.fillMaterial = std::move(value);
          },
          "The material the band is painted with, ahead of `fill`.")
      .def_readwrite("width", &Ribbon::width,
                     "The width law. Set, it is the band's width; left "
                     "empty, the nib and then the taper decide it.")
      .def_readwrite("widthStart", &Ribbon::widthStart,
                     "The taper's width at the start, and a nib's full "
                     "width.")
      .def_readwrite("widthEnd", &Ribbon::widthEnd,
                     "The taper's width at the end.")
      .def_readwrite("nibAngleDegrees", &Ribbon::nibAngleDegrees,
                     "The nib's angle in degrees; zero or more makes the "
                     "band calligraphic, widest where the outline runs "
                     "across the nib. Negative is no nib.")
      .def_readwrite("nibContrast", &Ribbon::nibContrast,
                     "The fraction of the full width a nib keeps where the "
                     "outline runs along it.")
      .def("hasProfile", &Ribbon::hasProfile)
      .def(
          "band",
          [](const Ribbon& self, const SkPath& spine) {
            return self.band(spine);
          },
          py::arg("spine"),
          "The region this ribbon fills along `spine`: the geometry it "
          "paints, handed back to be measured.")
      .def_readwrite("step", &Ribbon::step)
      .def_readwrite("join", &Ribbon::join)
      .def_readwrite("miterLimit", &Ribbon::miterLimit)
      .def("bleed", py::overload_cast<>(&Ribbon::bleed, py::const_))
      .def("isRunning", &Ribbon::isRunning)
      .def(py::self == py::self);
  copyProtocol(ribbon);
  py::implicitly_convertible<Ribbon, compose::Decoration>();

  brushes.def(
      "ribbon",
      [](py::object width, py::object paint,
         std::optional<float> nibAngleDegrees, float nibContrast,
         std::optional<float> widthEnd) {
        Ribbon made;
        if (nibAngleDegrees || widthEnd) {
          // A nib or a taper reads the width as one number, and the law
          // stays empty so that they decide the band.
          made.widthStart = width.cast<float>();
          made.widthEnd = widthEnd.value_or(made.widthStart);
          made.nibAngleDegrees = nibAngleDegrees.value_or(-1.0f);
          made.nibContrast = nibContrast;
        } else {
          made.width = width.cast<geometry::path::Profile>();
        }
        if (py::isinstance<material::Material>(paint))
          made.fillMaterial = paint.cast<material::Material>();
        else
          made.fill = fill(paint);
        return made;
      },
      py::arg("width"), py::arg("paint"), py::kw_only(),
      py::arg("nibAngleDegrees") = py::none(), py::arg("nibContrast") = 0.15f,
      py::arg("widthEnd") = py::none(),
      "A ribbon of `width` — a number, a list of (along, width) stops or a "
      "geometry.path.Profile — painted with `paint`: a colour, a fill or a "
      "material, whose layers, surface and effects it keeps. Give it to "
      "stroke() to sweep it along a node's outline. With `nibAngleDegrees` the "
      "band is a calligraphic stroke `width` across where the outline runs "
      "across the nib and `nibContrast` of it where it runs along; with "
      "`widthEnd` it tapers from `width` at the start to `widthEnd` at the "
      "end.");
}

}  // namespace sigil::python
