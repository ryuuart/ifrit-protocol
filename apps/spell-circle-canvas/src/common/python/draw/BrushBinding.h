#pragma once

/** @file
 * What the brush package's bindings share across their files: the
 * readings of Python values into brush records, strokes, point lists and
 * direction fields, the record and field registration every brush value
 * goes through, and one registration per subject. `bindBrush` calls the
 * registrations in the order written here, because a signature is written
 * when its function is registered and names only the classes bound
 * before it.
 */

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <sigildraw/brush/Brush.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/skia/Values.h>

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace brush = draw::brush;

/** Registers the brush's tips, rotations, drives, the records a tool is
 *  made of, the input and the dab, and the hatch, wash and mass styles. */
void bindBrushRecords(py::module_& module);
/** Registers the direction fields: curl, vortex and wave as callables,
 *  the field any callable makes, and the stock fields by name. */
void bindBrushFields(py::module_& module);
/** Registers the sampled geometry: the polygon, the plot and where it is
 *  placed, the position that walks a field, and the input sampler. */
void bindBrushGeometry(py::module_& module);
/** Registers the catalogue of tools and the engine that draws with them. */
void bindBrushEngine(py::module_& module);
/** Registers the stock tools and the stroke functions a pen draws with. */
void bindBrushStrokes(py::module_& module);
/** Registers the brush file formats on the `format` submodule. */
void bindBrushFormat(py::module_& module);

namespace brush_binding {

/** A direction field as Python holds it, so a field read once is handed
 *  back and forth without wrapping the callable again. */
struct Direction {
  brush::Direction value;
};

/** The curve @p value names, called with the interpreter held; empty for
 *  None. */
std::function<float(float)> scalarCurve(py::handle value);
/** The points in @p values. */
std::vector<SkPoint> points(py::iterable values);
/** The stroke in @p values: samples, positions with a pressure, or bare
 *  positions at full pressure. */
brush::Stroke samples(py::iterable values);
/** The direction field @p value names: a field, a stock field or a
 *  callable; empty for None. */
brush::Direction direction(py::handle value);
/** A copy of the tool @p value points at, or nothing. */
std::optional<brush::Tool> toolCopy(const brush::Tool* value);

/** A brush record registered as @p name, refusing a property it does
 *  not have in the brush's words. */
template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return bindRecord<T>(module, name, "Unknown brush property: ");
}

/** Whether a member is optional, which a property hands out by copy. */
template <class T>
struct Optional : std::false_type {};
template <class T>
struct Optional<std::optional<T>> : std::true_type {};

/** The property @p name of @p type over @p member, which reads a point,
 *  a colour or a curve the way the brush reads each and casts anything
 *  else. */
template <class T, class M>
void field(py::class_<T>& type, const char* name, M T::* member) {
  if constexpr (Optional<M>::value) {
    // Replacing or resetting an optional destroys its payload. Return an owned
    // copy so Python references cannot outlive that payload's storage.
    type.def_property(
        name, [member](const T& value) -> M { return value.*member; },
        [member](T& value, M input) { value.*member = std::move(input); });
  } else {
    type.def_property(
        name, [member](T& value) -> M& { return value.*member; },
        [member](T& value, py::handle input) {
          if constexpr (std::is_same_v<M, SkPoint>)
            value.*member = point(input);
          else if constexpr (std::is_same_v<M, SkColor4f>)
            value.*member = color(input);
          else if constexpr (std::is_same_v<M, std::function<float(float)>>)
            value.*member = scalarCurve(input);
          else
            value.*member = py::cast<M>(input);
        },
        py::return_value_policy::reference_internal);
  }
}

}  // namespace brush_binding
}  // namespace sigil::python
