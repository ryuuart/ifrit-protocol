/** @file
 * The arrangement an arranging operator places its children over, bound
 * as `compose.Arrangement` with one `Child` record per direct child: lent
 * for the one call, and refusing every reading once that call has
 * returned.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Operator.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "OperatorSeam.h"

namespace sigil::python {
namespace py = pybind11;
namespace {

using compose::Arrangement;

/** ONE DIRECT CHILD of a lent arrangement, addressed by its place in
 *  the list, so what is written through it lands on the record the
 *  composer reads back rather than on a copy of it. */
class ArrangementChild {
 public:
  ArrangementChild(std::shared_ptr<BorrowedArrangement> arrangement,
                   size_t index)
      : m_arrangement(std::move(arrangement)), m_index(index) {}

  Arrangement::Child& get() const {
    const std::span<Arrangement::Child> children =
        m_arrangement->get().children;
    if (m_index >= children.size())
      throw std::runtime_error("This child is no longer in the arrangement.");
    return children[m_index];
  }

 private:
  std::shared_ptr<BorrowedArrangement> m_arrangement;
  size_t m_index;
};

/** The children of a lent arrangement, one bound record each. */
py::list childrenOf(const std::shared_ptr<BorrowedArrangement>& arrangement) {
  py::list children;
  const size_t count = arrangement->get().children.size();
  for (size_t index = 0; index < count; ++index)
    children.append(py::cast(ArrangementChild{arrangement, index}));
  return children;
}

void bindArrangement(py::module_& composition) {
  py::class_<BorrowedArrangement, std::shared_ptr<BorrowedArrangement>>
      arrangement(
          composition, "Arrangement",
          "What an arranging operator is handed: the node's box, at the "
          "origin, and one record per direct child that has a box. It is "
          "lent for the one call and refuses every reading once that call "
          "has returned.");
  py::class_<ArrangementChild> child(
      arrangement, "Child",
      "One direct child as an operator sees it: measured, with its facts, "
      "and where it stands so far — where the operator before it in the "
      "list left it, or where the flex layout put it for the first — so "
      "an operator that nudges reads the rect and one that places "
      "overwrites it.");
  child
      .def_property_readonly("size",
                             [](const ArrangementChild& self) {
                               return geometry::path::toSkSize(self.get().size);
                             })
      .def_property_readonly(
          "baseline",
          [](const ArrangementChild& self) { return self.get().baseline; },
          "The first-baseline offset from the child's top, and not a number "
          "for a child with none.")
      .def_property_readonly(
          "minSize",
          [](const ArrangementChild& self) {
            return geometry::path::toSkSize(self.get().minSize);
          },
          "The smallest the child can be without spilling its content, "
          "filled only where an operator asked with readsChildMinSizes.")
      .def_property_readonly(
          "cells",
          [](const ArrangementChild& self) { return self.get().cells; })
      .def_property_readonly(
          "area", [](const ArrangementChild& self) { return self.get().area; })
      .def_property_readonly(
          "attributes",
          [](const ArrangementChild& self) { return self.get().attributes; })
      .def_property(
          "rect",
          [](const ArrangementChild& self) {
            return geometry::path::toSk(self.get().rect);
          },
          [](const ArrangementChild& self, py::handle where) {
            self.get().place(geometry::path::fromSk(rect(where)));
          })
      .def_property(
          "turnDegrees",
          [](const ArrangementChild& self) { return self.get().turnDegrees; },
          [](const ArrangementChild& self, float degrees) {
            self.get().turn(degrees);
          })
      .def(
          "attribute",
          [](const ArrangementChild& self, const std::string& name) {
            return factOf(self.get().attributes, name);
          },
          py::arg("name"))
      .def(
          "number",
          [](const ArrangementChild& self, const std::string& name) {
            return self.get().number(name);
          },
          py::arg("name"),
          "The fact under this name as a number, whichever numeric type it "
          "was written in, so an author who wrote 3 and one who wrote 3.0 "
          "are placed alike.")
      .def(
          "place",
          [](const ArrangementChild& self, py::handle where) {
            self.get().place(geometry::path::fromSk(rect(where)));
          },
          py::arg("rect"), "Puts the child here, size included.")
      .def(
          "centreAt",
          [](const ArrangementChild& self, py::handle centre) {
            self.get().centreAt(geometry::path::fromSk(point(centre)));
          },
          py::arg("centre"), "Centres the child's measured size on a point.")
      .def(
          "turn",
          [](const ArrangementChild& self, float degrees) {
            self.get().turn(degrees);
          },
          py::arg("degrees"),
          "Turns the child this many degrees clockwise about its own "
          "transform origin. The turn is paint-only and moves no layout.");

  arrangement
      .def_property_readonly("box",
                             [](const BorrowedArrangement& self) {
                               return geometry::path::toSk(self.get().box);
                             })
      .def_property_readonly(
          "children",
          [](const std::shared_ptr<BorrowedArrangement>& self) {
            return childrenOf(self);
          })
      .def_property_readonly(
          "minSizesMeasured",
          [](const BorrowedArrangement& self) {
            return self.get().minSizesMeasured;
          },
          "Whether each child's minSize was measured, which is true only "
          "where an operator in the list asked for it.");
}

}  // namespace

void bindComposeArrangement(py::module_& module) {
  auto composition = submodule(module, "compose");
  bindArrangement(composition);
}

}  // namespace sigil::python
