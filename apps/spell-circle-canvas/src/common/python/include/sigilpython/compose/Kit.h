#pragma once

/** @file
 * Binding the component kit's records: the conversion every kit
 * property goes through, the property registration that picks the
 * right ownership for it, and the field lists the shared records use.
 */

#include <pybind11/stl.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/skia/Values.h>

#include <array>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose {
struct PathFormat;
}  // namespace sigil::compose

namespace sigil::python {

/** Conversions shared by native kit record registrations. */
namespace kit {
namespace py = pybind11;
/** Whether T is an optional, which decides that None is a value it can
 *  take and that a Python reader gets an owned copy. */
template <class T>
struct Optional : std::false_type {};
/** The optional case, carrying what it holds as `Value`. */
template <class T>
struct Optional<std::optional<T>> : std::true_type {
  /** What the optional holds. */
  using Value = T;
};
/** Whether T is a vector, which decides that a Python reader gets an
 *  owned copy rather than a reference into storage that resizing would
 *  invalidate. */
template <class T>
struct Vector : std::false_type {};
/** The vector case. */
template <class T, class Allocator>
struct Vector<std::vector<T, Allocator>> : std::true_type {};

/** Whether T is a pair, whose two halves are each read as their own
 *  type, so a pair of a word and a colour takes a CSS string. */
template <class T>
struct Pair : std::false_type {};
/** The pair case. */
template <class First, class Second>
struct Pair<std::pair<First, Second>> : std::true_type {};
/** @p value read as a T, through this library's own reading for the
 *  types Python spells more loosely than C++ does — a colour, a point,
 *  a size, a dimension, a fill, an alignment, a string — and pybind11's
 *  cast for everything else. None is nothing, for an optional T. */
template <class T>
T converted(py::handle value) {
  if constexpr (Optional<T>::value) {
    if (value.is_none()) return std::nullopt;
    return converted<typename Optional<T>::Value>(value);
  } else if constexpr (std::is_same_v<T, SkColor4f> ||
                       std::is_same_v<T, material::Color>) {
    return color(value);
  } else if constexpr (std::is_same_v<T, SkPoint>) {
    return point(value);
  } else if constexpr (std::is_same_v<T, SkSize>) {
    if (py::isinstance<SkSize>(value)) return py::cast<SkSize>(value);
    const auto pair = py::cast<std::array<float, 2>>(value);
    return {pair[0], pair[1]};
  } else if constexpr (std::is_same_v<T, compose::Dimension>) {
    return dimension(value);
  } else if constexpr (std::is_same_v<T, compose::Fill>) {
    return fill(value);
  } else if constexpr (std::is_same_v<T, compose::Align>) {
    return alignment(value);
  } else if constexpr (std::is_same_v<T, compose::Justify>) {
    return justification(value);
  } else if constexpr (std::is_same_v<T, compose::Utf8>) {
    return py::cast<std::string>(value);
  } else if constexpr (std::is_same_v<T, compose::PathFormat>) {
    // A pen is most often only its width, so a number is one.
    if (PyNumber_Check(value.ptr()) && !py::isinstance<py::bool_>(value))
      return T{.width = py::cast<float>(value)};
    return py::cast<T>(value);
  } else if constexpr (std::is_same_v<T, compose::Element>) {
    return node(value);
  } else if constexpr (std::is_same_v<T, motion::Animatable<float>>) {
    return motionAnimatable(value);
  } else if constexpr (Pair<T>::value) {
    const auto halves = py::cast<py::sequence>(value);
    if (py::len(halves) != 2) throw py::type_error("A pair is two values.");
    return T{converted<typename T::first_type>(halves[0]),
             converted<typename T::second_type>(halves[1])};
  } else if constexpr (Vector<T>::value) {
    T run;
    for (py::handle item : py::iter(value))
      run.push_back(converted<typename T::value_type>(item));
    return run;
  } else {
    return py::cast<T>(value);
  }
}

/** Binds a kit record as @p name on @p module, so an unknown keyword
 *  argument names itself as an unknown kit property. */
template <class T>
py::class_<T> record(py::module_& module, const char* name) {
  return bindRecord<T>(module, name, "Unknown kit property: ");
}

/** Registers @p member as the property @p name on @p type, read
 *  through `converted` and handed back as a reference into the record
 *  or as an owned copy, whichever the member's type can safely be. */
template <class T, class M>
void field(py::class_<T>& type, const char* name, M T::* member) {
  if constexpr (std::is_same_v<M, compose::Utf8>) {
    type.def_property(
        name,
        [member](const T& value) {
          const auto& bytes = (value.*member).bytes();
          return std::string(bytes.begin(), bytes.end());
        },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        });
  } else if constexpr (std::is_same_v<M, std::vector<compose::Utf8>>) {
    type.def_property(
        name,
        [member](const T& value) {
          std::vector<std::string> words;
          for (const compose::Utf8& each : value.*member) {
            const auto& bytes = each.bytes();
            words.emplace_back(bytes.begin(), bytes.end());
          }
          return words;
        },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        });
  } else if constexpr (Optional<M>::value || Vector<M>::value) {
    // Replaced payloads and resized vectors invalidate references into storage.
    // A saved Python child therefore belongs to an owned copy.
    type.def_property(
        name, [member](const T& value) -> M { return value.*member; },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        });
  } else {
    type.def_property(
        name, [member](T& value) -> M& { return value.*member; },
        [member](T& value, py::handle input) {
          value.*member = converted<M>(input);
        },
        py::return_value_policy::reference_internal);
  }
}

/** Registers every property a well-shaped record carries, so the
 *  several records that share that shape are bound once each. */
template <class Well>
void wellFields(py::class_<Well>& type) {
  field(type, "width", &Well::width);
  field(type, "height", &Well::height);
  field(type, "ground", &Well::ground);
  field(type, "padding", &Well::padding);
  field(type, "paddingVertical", &Well::paddingVertical);
  field(type, "clip", &Well::clip);
  field(type, "corners", &Well::corners);
  field(type, "keyline", &Well::keyline);
  field(type, "keylineWidth", &Well::keylineWidth);
  field(type, "placed", &Well::placed);
  field(type, "content", &Well::content);
}

/** Binds a two-axis content record as @p name on @p module. */
template <class Content>
void contentType(py::module_& module, const char* name) {
  auto type = record<Content>(module, name);
  field(type, "across", &Content::across);
  field(type, "down", &Content::down);
}

}  // namespace kit
}  // namespace sigil::python
