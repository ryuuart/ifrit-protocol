/** @file
 * The brush file formats in Python: a tool encoded and decoded, a brush
 * assembled from its parts, and Photoshop and Procreate brushes read.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigildraw/brush/format/Load.h>
#include <sigildraw/brush/format/Photoshop.h>
#include <sigildraw/brush/format/Procreate.h>
#include <sigilpython/Bindings.h>

#include <cstddef>
#include <span>
#include <string>

#include "BrushBinding.h"

namespace sigil::python {

namespace {

std::span<const std::byte> bytes(const std::string& value) {
  return std::as_bytes(std::span(value.data(), value.size()));
}

}  // namespace

void bindBrushFormat(py::module_& module) {
  auto format = module.def_submodule("format");
  format.def("encodeBrush", &brush::format::encodeBrush, py::arg("tool"));
  format.def(
      "decodeBrush",
      [](py::bytes content, const std::string& hint) {
        const auto source = py::cast<std::string>(content);
        return brush::format::decodeBrush(bytes(source), hint);
      },
      py::arg("bytes"), py::arg("hint") = "");
  format.def(
      "assembleBrush",
      [](py::bytes description, py::bytes shape, py::bytes grain) {
        const auto descriptionBytes = py::cast<std::string>(description);
        const auto shapeBytes = py::cast<std::string>(shape);
        const auto grainBytes = py::cast<std::string>(grain);
        return brush::format::assembleBrush(
            bytes(descriptionBytes), bytes(shapeBytes), bytes(grainBytes));
      },
      py::arg("description") = py::bytes(), py::arg("shape") = py::bytes(),
      py::arg("grain") = py::bytes());
  format.def(
      "isPhotoshopBrushes",
      [](py::bytes content) {
        const auto source = py::cast<std::string>(content);
        return brush::format::isPhotoshopBrushes(bytes(source));
      },
      py::arg("content"));
  format.def(
      "decodePhotoshopBrushes",
      [](py::bytes content) {
        const auto source = py::cast<std::string>(content);
        return brush::format::decodePhotoshopBrushes(bytes(source));
      },
      py::arg("content"));
  format.def(
      "decodeProcreateBrush",
      [](py::bytes content) {
        const auto source = py::cast<std::string>(content);
        return brush::format::decodeProcreateBrush(bytes(source));
      },
      py::arg("content"));
}

}  // namespace sigil::python
