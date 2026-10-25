/** @file
 * Python access to native natural-media tools, sampled geometry and engines:
 * the package's one registration, which calls each subject's in turn, and
 * the readings of Python values the subjects share.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/draw/Registration.h>
#include <sigilpython/skia/Values.h>

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

#include "BrushBinding.h"

namespace sigil::python {
namespace brush_binding {

std::function<float(float)> scalarCurve(py::handle value) {
  if (value.is_none()) return {};
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("A brush curve or field must be callable");
  auto held = retainCallback(py::reinterpret_borrow<py::function>(value));
  return [held](float x) {
    const py::gil_scoped_acquire lock;
    try {
      return held->get()(x).template cast<float>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

std::vector<SkPoint> points(py::iterable values) {
  std::vector<SkPoint> result;
  for (auto value : values) result.push_back(point(value));
  return result;
}

brush::Stroke samples(py::iterable values) {
  brush::Stroke result;
  for (auto value : values) {
    if (py::isinstance<brush::Sample>(value)) {
      result.push_back(py::cast<brush::Sample>(value));
      continue;
    }
    const auto sample = py::reinterpret_borrow<py::sequence>(value);
    if (sample.size() == 3)
      result.push_back(
          {{py::cast<float>(sample[0]), py::cast<float>(sample[1])},
           py::cast<float>(sample[2])});
    else if (sample.size() == 2 && (py::isinstance<py::tuple>(sample[0]) ||
                                    py::isinstance<py::list>(sample[0]) ||
                                    py::isinstance<SkPoint>(sample[0])))
      result.push_back({point(sample[0]), py::cast<float>(sample[1])});
    else
      result.push_back({point(value), 1.0f});
  }
  return result;
}

brush::Direction direction(py::handle value) {
  if (value.is_none()) return {};
  if (py::isinstance<Direction>(value)) return py::cast<Direction>(value).value;
  if (py::isinstance<brush::Curl>(value)) return py::cast<brush::Curl>(value);
  if (py::isinstance<brush::Vortex>(value))
    return py::cast<brush::Vortex>(value);
  if (py::isinstance<brush::Wave>(value)) return py::cast<brush::Wave>(value);
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("A brush curve or field must be callable");
  auto held = retainCallback(py::reinterpret_borrow<py::function>(value));
  return [held](SkPoint at, float seconds) {
    const py::gil_scoped_acquire lock;
    try {
      return held->get()(at, seconds).template cast<float>();
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

std::optional<brush::Tool> toolCopy(const brush::Tool* value) {
  return value ? std::optional(*value) : std::nullopt;
}

}  // namespace brush_binding

void bindBrush(py::module_& root) {
  auto drawing = root.attr("draw").cast<py::module_>();
  auto module = drawing.def_submodule("brush");
  bindBrushRecords(module);
  bindBrushFields(module);
  bindBrushGeometry(module);
  bindBrushEngine(module);
  bindBrushStrokes(module);
  bindBrushFormat(module);
}

}  // namespace sigil::python
