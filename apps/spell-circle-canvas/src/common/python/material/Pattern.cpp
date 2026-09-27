#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/pattern/Tile.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Texture.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/skia/Values.h>

namespace sigil::python {
namespace py = pybind11;
namespace pattern = material::pattern;

void bindMaterialPattern(py::module_& module) {
  constexpr auto fluent = py::return_value_policy::reference_internal;
  auto patterns = submodule(module, "material.pattern");
  py::class_<pattern::Tile>(patterns, "Tile")
      .def("seed", &pattern::Tile::seed, py::arg("seed"), fluent)
      .def("scale", py::overload_cast<float>(&pattern::Tile::scale),
           py::arg("factor"), fluent)
      .def("rotate", py::overload_cast<float>(&pattern::Tile::rotate),
           py::arg("degrees"), fluent)
      .def(
          "offset",
          [](pattern::Tile& tile, py::handle value) -> pattern::Tile& {
            const SkPoint offset = point(value);
            return tile.offset({offset.x(), offset.y()});
          },
          py::arg("offset"), fluent)
      .def("image",
           [](const pattern::Tile& tile) {
             return material::skia::image(tile.texture());
           })
      .def("paint", [](const pattern::Tile& tile) {
        return material::skia::paint(material::skia::shader(tile.texture()));
      });
  patterns.def(
      "gridLines",
      [](float spacing, float width, py::handle value) {
        return pattern::gridLines(spacing, width, materialColor(value));
      },
      py::arg("spacing"), py::arg("width"), py::arg("color"));
  patterns.def(
      "stripes",
      [](float on, float off, py::handle value) {
        return pattern::stripes(on, off, materialColor(value));
      },
      py::arg("on"), py::arg("off"), py::arg("color"));
  patterns.def(
      "checker",
      [](float cell, py::handle first, py::handle second) {
        return pattern::checker(cell, materialColor(first),
                                materialColor(second));
      },
      py::arg("cell"), py::arg("first"), py::arg("second"));
  patterns.def(
      "halftone",
      [](float spacing, float radius, py::handle value, bool staggered) {
        return pattern::halftone(spacing, radius, materialColor(value),
                                 staggered);
      },
      py::arg("spacing"), py::arg("radius"), py::arg("color"),
      py::arg("staggered") = true);
  patterns.def(
      "scanlines",
      [](py::handle value, float period, float on, float phase) {
        return pattern::scanlines({.color = materialColor(value),
                                   .period = period,
                                   .on = on,
                                   .phase = phase});
      },
      py::arg("color"), py::arg("period") = 4.0f, py::arg("on") = 2.0f,
      py::arg("phase") = 0.0f);
  patterns.def(
      "stipple",
      [](py::handle value, uint64_t bits, int size, float cell) {
        return pattern::stipple({.color = materialColor(value),
                                 .bits = bits,
                                 .size = size,
                                 .cell = cell});
      },
      py::arg("color"), py::arg("bits") = uint64_t{0b1001},
      py::arg("size") = 2, py::arg("cell") = 1.0f);
  patterns.def("ditherBits", &pattern::ditherBits, py::arg("on"),
               py::arg("size") = 4);
}

}  // namespace sigil::python
