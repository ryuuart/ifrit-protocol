/** @file
 * The settled scope an adding operator reads and attaches to, bound as
 * `compose.Scope` with one `Node` record per node under it: lent for the
 * one call on the same terms as an arrangement.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Operator.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "OperatorSeam.h"

namespace sigil::python {
namespace py = pybind11;
namespace {

constexpr auto copied = py::return_value_policy::copy;

using compose::Scope;

/** @p found, which points into @p scope's own run, as bound nodes. */
py::list nodesOf(const std::shared_ptr<BorrowedScope>& scope,
                 const std::vector<const Scope::Node*>& found) {
  const Scope::Node* first = scope->get().nodes().data();
  py::list nodes;
  for (const Scope::Node* one : found)
    nodes.append(py::cast(ScopeNode{scope, (size_t)(one - first)}));
  return nodes;
}

void bindScope(py::module_& composition) {
  py::class_<BorrowedScope, std::shared_ptr<BorrowedScope>> scope(
      composition, "Scope",
      "What an adding operator is handed once layout has settled: the "
      "node's box and every node under it, in the scope's own coordinates. "
      "THE SCOPE IS CLOSED — a node under it that applies operators of its "
      "own is one node here, with nothing under it, and what it wants read "
      "from outside it states as facts on its root.");
  py::class_<ScopeNode> settled(
      scope, "Node", "One node in the scope as an adding operator sees it.");
  settled
      .def_property_readonly(
          "key", [](const ScopeNode& self) { return self.get().key; })
      .def_property_readonly(
          "attributes",
          [](const ScopeNode& self) { return self.get().attributes; })
      .def_property_readonly(
          "classes", [](const ScopeNode& self) { return self.get().classes; })
      .def_property_readonly(
          "bounds",
          [](const ScopeNode& self) {
            return geometry::path::toSk(self.get().bounds);
          },
          "Where the node stands, in the scope's coordinates.")
      .def_property_readonly(
          "outline",
          [](const ScopeNode& self) {
            return geometry::path::toSk(self.get().outline);
          },
          "The outline the node resolved to — its shape, a routed path, or "
          "its box — in the scope's coordinates.")
      .def(
          "attribute",
          [](const ScopeNode& self, const std::string& name) {
            return factOf(self.get().attributes, name);
          },
          py::arg("name"))
      .def(
          "number",
          [](const ScopeNode& self, const std::string& name) {
            return self.get().number(name);
          },
          py::arg("name"))
      .def(
          "hasClass",
          [](const ScopeNode& self, const std::string& name) {
            return self.get().hasClass(name);
          },
          py::arg("name"))
      .def(
          "toLocal",
          [](const ScopeNode& self, py::handle value) -> py::object {
            if (py::isinstance<SkPath>(value))
              return py::cast(geometry::path::toSk(self.get().toLocal(
                  geometry::path::fromSk(value.cast<SkPath>()))));
            return py::cast(geometry::path::toSk(self.get().toLocal(
                                geometry::path::fromSk(point(value)))),
                            copied);
          },
          py::arg("value"),
          "This point or path, in the scope's coordinates, moved into the "
          "node's own.")
      .def(
          "attach",
          [](const ScopeNode& self, py::handle element) {
            self.get().attach(node(element));
          },
          py::arg("element"),
          "Attaches this element to the node, placed in the node's own "
          "coordinates: what is about this node alone, drawn with it, gone "
          "when it goes.");

  scope
      .def_property_readonly("box",
                             [](const BorrowedScope& self) {
                               return geometry::path::toSk(self.get().box);
                             })
      .def("nodes",
           [](const std::shared_ptr<BorrowedScope>& self) {
             py::list nodes;
             const size_t count = self->get().nodes().size();
             for (size_t index = 0; index < count; ++index)
               nodes.append(py::cast(ScopeNode{self, index}));
             return nodes;
           })
      .def(
          "find",
          [](const std::shared_ptr<BorrowedScope>& self,
             const std::string& key) -> py::object {
            const Scope::Node* found = self->get().find(key);
            if (!found) return py::none();
            return py::cast(
                ScopeNode{self, (size_t)(found - self->get().nodes().data())});
          },
          py::arg("key"),
          "The node under this key, or None: an unknown key is silent, as "
          "it is across the derive family.")
      .def(
          "having",
          [](const std::shared_ptr<BorrowedScope>& self,
             const std::string& lane) {
            return nodesOf(self, self->get().having(lane));
          },
          py::arg("lane"),
          "Every node stating a fact under this name, in tree order.")
      .def(
          "withClass",
          [](const std::shared_ptr<BorrowedScope>& self,
             const std::string& name) {
            return nodesOf(self, self->get().withClass(name));
          },
          py::arg("name"), "Every node naming this class, in tree order.")
      .def(
          "attach",
          [](const BorrowedScope& self, py::handle element) {
            if (Scope* attachable = self.attachable())
              attachable->attach(node(element));
          },
          py::arg("element"),
          "Attaches this element to the scope, placed in the scope's "
          "coordinates: what is about several nodes, or about the scope "
          "itself. A scope lent to a program that draws attaches nothing, "
          "because its pixels land after the additions are laid out.");
}

}  // namespace

void bindComposeScope(py::module_& module) {
  auto composition = submodule(module, "compose");
  bindScope(composition);
}

}  // namespace sigil::python
