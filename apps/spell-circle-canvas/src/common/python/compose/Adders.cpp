/** @file
 * The stock adding operators and their records: the connecting wires,
 * the pin a node asks to have hung off it, the bands and hulls laid
 * along outlines, the stamp whose maker is a Python callable, and the
 * imperative door a pen draws a scope through.
 */

#include <include/core/SkPoint.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Outline.h>
#include <sigilcompose/kit/Pin.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "OperatorSeam.h"

namespace sigil::python {
namespace py = pybind11;
namespace {

namespace connect = sigil::compose::connect;
using compose::Scope;
using kit::field;
using kit::record;

/** One stop of a wire from any of the spellings Python writes it in. */
compose::Anchor readAnchor(py::handle value) {
  if (py::isinstance<compose::Anchor>(value))
    return value.cast<compose::Anchor>();
  if (py::isinstance<py::str>(value))
    return compose::Anchor::on(value.cast<std::string>());
  if (py::isinstance<py::sequence>(value)) {
    const auto items = value.cast<py::sequence>();
    if (items.size() == 2 && py::isinstance<py::str>(items[0]))
      return compose::Anchor::on(items[0].cast<std::string>(),
                                 geometry::path::fromSk(point(items[1])));
    return compose::Anchor::at(geometry::path::fromSk(point(value)));
  }
  throw py::type_error(
      "A stop is an Anchor, a node's key, a (key, (x, y)) pair read on that "
      "node's bounds, or a free (x, y) point.");
}

/** Every stop of a run, in order. */
std::vector<compose::Anchor> readAnchors(py::handle value) {
  if (py::isinstance<py::str>(value))
    throw py::type_error("The stops are a sequence of them, not one key.");
  std::vector<compose::Anchor> stops;
  for (py::handle one : value) stops.push_back(readAnchor(one));
  return stops;
}

void bindConnect(py::module_& module) {
  auto wires = submodule(module, "compose.connect");
  auto between = record<connect::Between>(wires, "Between");
  // `from` is Python's own word, so this one field carries the trailing
  // underscore the language forces.
  between.def_property(
      "from_", [](const connect::Between& self) { return self.from; },
      [](connect::Between& self, const std::string& key) { self.from = key; });
  field(between, "to", &connect::Between::to);
  field(between, "router", &connect::Between::router);
  field(between, "gap", &connect::Between::gap);
  field(between, "wire", &connect::Between::wire);
  field(between, "bleed", &connect::Between::bleed);
  between.def(py::self == py::self);
  auto byLane = record<connect::ByLane>(wires, "ByLane");
  field(byLane, "lane", &connect::ByLane::lane);
  field(byLane, "router", &connect::ByLane::router);
  field(byLane, "gap", &connect::ByLane::gap);
  field(byLane, "wire", &connect::ByLane::wire);
  field(byLane, "bleed", &connect::ByLane::bleed);
  byLane.def(py::self == py::self);

  auto along = record<connect::Along>(wires, "Along");
  along.doc() =
      "A wire along an ordered run of stops: each an Anchor, a node's key "
      "(its centre), a (key, (x, y)) pair read on that node's bounds, or a "
      "free (x, y) point in the scope's coordinates. A key the scope does "
      "not hold draws nothing.";
  along.def_property(
      "stops", [](const connect::Along& self) { return self.stops; },
      [](connect::Along& self, py::handle value) {
        self.stops = readAnchors(value);
      });
  field(along, "router", &connect::Along::router);
  field(along, "wire", &connect::Along::wire);
  field(along, "bleed", &connect::Along::bleed);
  field(along, "key", &connect::Along::key);
  along.def(py::self == py::self);
}

void bindAdders(py::module_& module) {
  auto pins = submodule(module, "compose.pin");
  auto request = record<compose::pin::Request>(pins, "Request");
  request.doc() =
      "What a node asks to have hung off it, stated as a fact under the "
      "lane a pin.ByLane reads: the element, the box it is given and "
      "where it hangs. The tether's key is ignored; the node stating the "
      "request is the anchor.";
  request.def_property(
      "element", [](const compose::pin::Request& self) { return self.element; },
      [](compose::pin::Request& self, py::handle value) {
        self.element = node(value);
      });
  request.def_property(
      "size",
      [](const compose::pin::Request& self) {
        return std::pair<float, float>{self.size.x, self.size.y};
      },
      [](compose::pin::Request& self, py::handle value) {
        self.size = geometry::path::fromSk(point(value));
      });
  field(request, "where", &compose::pin::Request::where);
  request.def(py::self == py::self);
  auto pinning = record<compose::pin::ByLane>(pins, "ByLane");
  pinning.doc() =
      "An element hung off every node stating a Request under the lane, "
      "keyed <node key>-pin and placed by the request's tether, or by the "
      "first fallback that stays inside the scope.";
  field(pinning, "lane", &compose::pin::ByLane::lane);
  pinning.def(py::self == py::self);

  auto outlines = submodule(module, "compose.outline");
  auto around = record<compose::outline::Around>(outlines, "Around");
  around.doc() =
      "A band laid along the outline of the keyed node, as wide as its "
      "width law and on the side its formation says.";
  field(around, "key", &compose::outline::Around::key);
  field(around, "across", &compose::outline::Around::across);
  field(around, "formation", &compose::outline::Around::formation);
  field(around, "fill", &compose::outline::Around::fill);
  around.def(py::self == py::self);
  auto hull = record<compose::outline::Hull>(outlines, "Hull");
  hull.doc() =
      "The region around every node stating the lane or naming the class: "
      "the convex hull, or with a finite alpha the concave one, grown by "
      "the margin.";
  field(hull, "lane", &compose::outline::Hull::lane);
  field(hull, "styleClass", &compose::outline::Hull::styleClass);
  field(hull, "alpha", &compose::outline::Hull::alpha);
  field(hull, "margin", &compose::outline::Hull::margin);
  field(hull, "fill", &compose::outline::Hull::fill);
  hull.def(py::self == py::self);

  auto stamps = submodule(module, "compose.stamp");
  py::class_<PythonStamp> stamp(
      stamps, "ByLane",
      "One element made per node stating the lane, by `make` handed the "
      "node as the scope sees it, attached to that node and keyed "
      "<node key>-stamp. Two stamps are equal when they name one non-empty "
      "key over one lane, so a stamp that states its key prunes.");
  stamp
      .def(py::init([](std::string lane, py::function make, std::string key) {
             return PythonStamp{std::move(lane), std::move(key),
                                retainCallback(std::move(make))};
           }),
           py::arg("lane"), py::arg("make"), py::arg("key") = "")
      .def_readwrite("lane", &PythonStamp::lane)
      .def_readwrite("key", &PythonStamp::key)
      .def_property(
          "make",
          [](const PythonStamp& self) -> py::object {
            if (!self.make) return py::none();
            try {
              return self.make->get();
            } catch (const std::runtime_error&) {
              return py::none();
            }
          },
          [](PythonStamp& self, py::function make) {
            self.make = retainCallback(std::move(make));
          })
      .def(py::self == py::self);
}

/** A scope program read from @p value, a callable naming none, one or
 *  both of the pen and the scope, in that order. The callable is
 *  retained against the callback lifetime in force, the pen and the
 *  scope it is handed are lent for the one call, and what it raises
 *  leaves the native draw as a runtime error. */
compose::ScopeProgram scopeProgram(py::handle value) {
  if (!PyCallable_Check(value.ptr()))
    throw py::type_error("A scope program is a callable.");
  auto function = py::reinterpret_borrow<py::function>(value);
  int named = 2;
  try {
    named = py::module_::import("sigil._callbacks")
                .attr("arity")(function, 2)
                .cast<int>();
  } catch (py::error_already_set& error) {
    // A callable Python cannot read a signature from is offered both.
    if (!error.matches(PyExc_ValueError)) throw;
  }
  auto held = retainCallback(std::move(function));
  return [held, named](draw::Pen& pen, const Scope& scope) {
    const py::gil_scoped_acquire lock;
    const py::function program = held->get();
    if (named == 1) {
      invokePen(program, pen);
      return;
    }
    std::shared_ptr<BorrowedPen> borrowed;
    std::optional<ScopeLoan> lent;
    if (named == 2) {
      borrowed = std::make_shared<BorrowedPen>(pen);
      lent.emplace(scope);
    }
    struct EndLoan {
      BorrowedPen* pen;
      ~EndLoan() {
        if (pen) pen->invalidate();
      }
    } endLoan{borrowed.get()};
    try {
      const CallbackBoundary boundary;
      if (named == 0)
        program();
      else
        program(borrowed, lent->scope());
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  };
}

}  // namespace

void bindComposeAdders(py::module_& module) {
  bindConnect(module);
  bindAdders(module);

  auto composition = submodule(module, "compose");
  composition.def(
      "drawWith",
      [](py::handle program) {
        return compose::drawWith(scopeProgram(program));
      },
      py::arg("program"),
      "The imperative door of the operator family: an adding operator that "
      "attaches one pen over the scope's box and hands the program the pen "
      "and the scope. The output is pixels, so nothing downstream reads it, "
      "and a bare program is a callable with no equality, so its node is "
      "described afresh every frame.");
  composition.def(
      "drawWith",
      [](const std::string& key, py::handle program) {
        return compose::drawWith(key, scopeProgram(program));
      },
      py::arg("key"), py::arg("program"),
      "The keyed spelling, which vouches for the program's identity as the "
      "keyed pen does — equal keys assert equal programs — so the operator "
      "and the pen it attaches both prune while the scope they read is "
      "unchanged.");
}

}  // namespace sigil::python
