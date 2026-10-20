/** @file
 * The facts a node states about itself: the typed table bound as
 * `compose.Attributes`, and the two readings between a fact and the
 * shape Python spells its value in.
 */

#include <include/core/SkPoint.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Attributes.h>
#include <sigilcompose/kit/Pin.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <string>
#include <string_view>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

constexpr auto copied = py::return_value_policy::copy;

using compose::Attributes;

void bindAttributes(py::module_& composition) {
  py::class_<Attributes> facts(
      composition, "Attributes",
      "The typed facts one node states about itself, each a name and a "
      "value read back in the type it was written in. Who reads a fact and "
      "what it means is the reader's business, and a fact nothing reads "
      "does nothing. Two tables are equal when they carry the same names "
      "with equal values of the same type, in any order.");
  facts.def(py::init<>())
      .def(
          "set",
          [](Attributes& self, const std::string& name, py::handle value) {
            stateFact(self, name, value);
          },
          py::arg("name"), py::arg("value"))
      .def(
          "get",
          [](const Attributes& self, const std::string& name) {
            return factOf(self, name);
          },
          py::arg("name"))
      .def(
          "has",
          [](const Attributes& self, const std::string& name) {
            return self.has(name);
          },
          py::arg("name"))
      .def("merge", &Attributes::merge, py::arg("other"),
           "Every fact of the other table laid over this one's, same names "
           "replaced.")
      .def("names", &Attributes::names,
           "The names stated, in the order they were first stated.")
      .def("empty", &Attributes::empty)
      .def("__len__", &Attributes::size)
      .def(
          "__contains__",
          [](const Attributes& self, const std::string& name) {
            return self.has(name);
          },
          py::arg("name"))
      .def("copy", [](const Attributes& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(facts);
}

}  // namespace

void stateFact(Attributes& facts, std::string_view name, py::handle value) {
  if (py::isinstance<py::bool_>(value)) {
    facts.set(name, value.cast<bool>());
    return;
  }
  if (py::isinstance<py::int_>(value)) {
    facts.set(name, value.cast<int>());
    return;
  }
  if (py::isinstance<py::float_>(value)) {
    facts.set(name, value.cast<float>());
    return;
  }
  if (py::isinstance<py::str>(value)) {
    facts.set(name, value.cast<std::string>());
    return;
  }
  if (py::isinstance<SkPoint>(value)) {
    facts.set(name, value.cast<SkPoint>());
    return;
  }
  if (py::isinstance<compose::pin::Request>(value)) {
    facts.set(name, value.cast<compose::pin::Request>());
    return;
  }
  if (py::isinstance<py::sequence>(value) &&
      !py::isinstance<py::bytes>(value)) {
    bool strings = true;
    bool numbers = true;
    for (py::handle item : value) {
      const bool text = py::isinstance<py::str>(item);
      strings = strings && text;
      numbers = numbers && !text && PyNumber_Check(item.ptr()) != 0;
    }
    if (strings) {
      facts.set(name, value.cast<std::vector<std::string>>());
      return;
    }
    if (numbers) {
      facts.set(name, value.cast<std::vector<float>>());
      return;
    }
  }
  throw py::type_error(
      "A fact is a bool, a whole number, a number, a string, a list of "
      "strings, a list of numbers, a point, or a pin.Request.");
}

py::object factOf(const Attributes& facts, std::string_view name) {
  if (auto value = facts.get<bool>(name)) return py::cast(*value);
  if (auto value = facts.get<int>(name)) return py::cast(*value);
  if (auto value = facts.get<float>(name)) return py::cast(*value);
  if (auto value = facts.get<std::string>(name)) return py::cast(*value);
  if (auto value = facts.get<std::vector<std::string>>(name))
    return py::cast(*value);
  if (auto value = facts.get<std::vector<float>>(name)) return py::cast(*value);
  if (auto value = facts.get<SkPoint>(name)) return py::cast(*value, copied);
  if (auto value = facts.get<compose::pin::Request>(name))
    return py::cast(*value);
  return py::none();
}

void bindComposeFacts(py::module_& module) {
  auto composition = submodule(module, "compose");
  bindAttributes(composition);
}

}  // namespace sigil::python
