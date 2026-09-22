/** @file
 * The operator seam: the facts a node states, the arrangement an
 * arranging operator places its children over, the settled scope an
 * adding operator reads and attaches to, the connecting records, and
 * the imperative door a pen draws a scope through. An operator is built
 * from a stock arranging value, from a connecting record, or from a
 * Python object that arranges or adds — and what lets the third one
 * prune is an equality of its own.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Grid.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigildraw/Pen.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Convert.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/compose/Registration.h>
#include <sigilpython/skia/Values.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;
constexpr auto copied = py::return_value_policy::copy;

namespace layouts = sigil::compose::layouts;
namespace connect = sigil::compose::connect;
using compose::Arrangement;
using compose::Attributes;
using compose::Operator;
using compose::Scope;
using kit::field;
using kit::record;

/** Whether @p value's class states an equality of its own, rather than
 *  inheriting the identity every object carries. This is what decides
 *  whether a Python operator takes part in structural equality at all.
 *  Requires the interpreter lock. */
bool definesEquality(const py::object& value) {
  const py::handle plain{(PyObject*)&PyBaseObject_Type};
  const py::handle type{(PyObject*)Py_TYPE(value.ptr())};
  return !type.attr("__eq__").is(plain.attr("__eq__"));
}

/** A RETAINED PYTHON OPERATOR'S IDENTITY AND EQUALITY. Two are equal
 *  when they hold one value, or when the value's own class states an
 *  equality and Python says the two values are equal under it. A value
 *  whose class states none is the escape hatch: it equals nothing but
 *  its own copies, so the node it was applied on is described afresh
 *  every frame. A value whose callback lifetime has closed equals
 *  nothing but itself either, so a reconcile patches the node instead
 *  of raising out of itself. */
class PythonOperatorValue {
 public:
  explicit PythonOperatorValue(std::shared_ptr<PythonValue> value)
      : m_value(std::move(value)) {}

  const PythonValue& held() const { return *m_value; }

  bool operator==(const PythonOperatorValue& other) const {
    if (m_value == other.m_value) return true;
    const py::gil_scoped_acquire lock;
    py::object left;
    py::object right;
    try {
      left = m_value->get();
      right = other.m_value->get();
    } catch (const std::runtime_error&) {
      return false;
    }
    if (!definesEquality(left) || !definesEquality(right)) return false;
    const int equal = PyObject_RichCompareBool(left.ptr(), right.ptr(), Py_EQ);
    if (equal < 0) {
      const py::error_already_set error;
      throw std::runtime_error(error.what());
    }
    return equal != 0;
  }

 private:
  std::shared_ptr<PythonValue> m_value;
};

/** ONE NATIVE ARRANGEMENT, LENT TO PYTHON for the call it was handed to.
 *  The records it carries stand in the composer's own layout round, so
 *  every reading asks this wrapper first: it refuses once the call has
 *  returned, and on a thread other than the one that lays out. */
class BorrowedArrangement {
 public:
  explicit BorrowedArrangement(Arrangement& arrangement)
      : m_thread(std::this_thread::get_id()), m_arrangement(&arrangement) {}

  Arrangement& get() const {
    if (std::this_thread::get_id() != m_thread)
      throw std::runtime_error(
          "An arrangement can only be read on the thread that lays out.");
    if (!m_arrangement)
      throw std::runtime_error(
          "This arrangement is no longer inside the call it was handed to.");
    return *m_arrangement;
  }

  void invalidate() { m_arrangement = nullptr; }

 private:
  const std::thread::id m_thread;
  Arrangement* m_arrangement;
};

/** ONE DIRECT CHILD of a lent arrangement, addressed by its place in
 *  the list, so what is written through it lands on the record the
 *  composer reads back rather than on a copy of it. */
class ArrangementChild {
 public:
  ArrangementChild(std::shared_ptr<BorrowedArrangement> arrangement,
                   size_t index)
      : m_arrangement(std::move(arrangement)), m_index(index) {}

  Arrangement::Child& get() const {
    std::vector<Arrangement::Child>& children = m_arrangement->get().children;
    if (m_index >= children.size())
      throw std::runtime_error("This child is no longer in the arrangement.");
    return children[m_index];
  }

 private:
  std::shared_ptr<BorrowedArrangement> m_arrangement;
  size_t m_index;
};

/** ONE SETTLED SCOPE, LENT TO PYTHON on the same terms, with the one
 *  thing past that a lent scope has to say about itself: whether there
 *  is anything left to attach to. A program that draws is handed a
 *  scope to read, where attaching would be too late to mean anything,
 *  so it attaches nothing — silently, as the native snapshot's own
 *  nodes do. */
class BorrowedScope {
 public:
  explicit BorrowedScope(Scope& scope)
      : m_thread(std::this_thread::get_id()),
        m_scope(&scope),
        m_attachable(&scope) {}
  explicit BorrowedScope(const Scope& scope)
      : m_thread(std::this_thread::get_id()), m_scope(&scope) {}

  const Scope& get() const {
    if (std::this_thread::get_id() != m_thread)
      throw std::runtime_error(
          "A scope can only be read on the thread that laid it out.");
    if (!m_scope)
      throw std::runtime_error(
          "This scope is no longer inside the call it was handed to.");
    return *m_scope;
  }

  /** The scope to attach to, or null where nothing can be attached. */
  Scope* attachable() const {
    get();
    return m_attachable;
  }

  void invalidate() {
    m_scope = nullptr;
    m_attachable = nullptr;
  }

 private:
  const std::thread::id m_thread;
  const Scope* m_scope;
  Scope* m_attachable = nullptr;
};

/** ONE NODE of a lent scope, addressed by its place in the run. */
class ScopeNode {
 public:
  ScopeNode(std::shared_ptr<BorrowedScope> scope, size_t index)
      : m_scope(std::move(scope)), m_index(index) {}

  const Scope::Node& get() const {
    const auto nodes = m_scope->get().nodes();
    if (m_index >= nodes.size())
      throw std::runtime_error("This node is no longer in the scope.");
    return nodes[m_index];
  }

 private:
  std::shared_ptr<BorrowedScope> m_scope;
  size_t m_index;
};

/** An arrangement lent to Python for as long as this stands. The bound
 *  `compose.Arrangement` and every child reached through it refuse once
 *  this goes out of scope, however the call it was made for returned.
 *  The interpreter lock must be held for the whole of its life. */
class ArrangementLoan {
 public:
  explicit ArrangementLoan(Arrangement& arrangement)
      : m_view(std::make_shared<BorrowedArrangement>(arrangement)),
        m_object(py::cast(m_view)) {}
  ~ArrangementLoan() { m_view->invalidate(); }
  ArrangementLoan(const ArrangementLoan&) = delete;
  ArrangementLoan& operator=(const ArrangementLoan&) = delete;

  /** The bound `compose.Arrangement` to hand the operator. */
  const py::object& arrangement() const { return m_object; }

 private:
  std::shared_ptr<BorrowedArrangement> m_view;
  py::object m_object;
};

/** A scope lent to Python on the same terms, over a scope an operator
 *  may attach to or over a settled one a program only reads. */
class ScopeLoan {
 public:
  explicit ScopeLoan(Scope& scope)
      : m_view(std::make_shared<BorrowedScope>(scope)),
        m_object(py::cast(m_view)) {}
  explicit ScopeLoan(const Scope& scope)
      : m_view(std::make_shared<BorrowedScope>(scope)),
        m_object(py::cast(m_view)) {}
  ~ScopeLoan() { m_view->invalidate(); }
  ScopeLoan(const ScopeLoan&) = delete;
  ScopeLoan& operator=(const ScopeLoan&) = delete;

  /** The bound `compose.Scope` to hand the operator or the program. */
  const py::object& scope() const { return m_object; }

 private:
  std::shared_ptr<BorrowedScope> m_view;
  py::object m_object;
};

/** Asks @p held to arrange @p arrangement, with the arrangement lent to
 *  it for the one call. A value whose lifetime has closed arranges
 *  nothing: the description holding it has outlived the host that could
 *  answer for it. */
void callArrange(const PythonValue& held, Arrangement& arrangement) {
  const py::gil_scoped_acquire lock;
  py::object value;
  try {
    value = held.get();
  } catch (const std::runtime_error&) {
    return;
  }
  // The loan outlives the callback boundary, so a native scope Python
  // left open is closed while the arrangement it read is still lent.
  const ArrangementLoan lent{arrangement};
  try {
    const CallbackBoundary boundary;
    value.attr("arrange")(lent.arrangement());
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** Asks @p held to add to @p scope, on the same terms. */
void callAdd(const PythonValue& held, Scope& scope) {
  const py::gil_scoped_acquire lock;
  py::object value;
  try {
    value = held.get();
  } catch (const std::runtime_error&) {
    return;
  }
  const ScopeLoan lent{scope};
  try {
    const CallbackBoundary boundary;
    value.attr("add")(lent.scope());
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** A Python value with `arrange(arrangement)` as a native arranging
 *  operator. @p Measures is what the value asked for with
 *  `readsChildMinSizes`, and it is the model's TYPE that carries the
 *  answer: the measure costs one text layout per text child, so a value
 *  that did not ask makes the model that does not pay for it. */
template <bool Measures>
struct PythonArranging {
  PythonOperatorValue value;
  static constexpr bool readsChildMinSizes = Measures;
  bool operator==(const PythonArranging&) const = default;
  void arrange(Arrangement& arrangement) const {
    callArrange(value.held(), arrangement);
  }
};

/** A Python value with `add(scope)` as a native adding operator. */
struct PythonAdding {
  PythonOperatorValue value;
  bool operator==(const PythonAdding&) const = default;
  void add(Scope& scope) const { callAdd(value.held(), scope); }
};

/** An operator over a Python object, which either arranges the children
 *  or adds to the settled scope and cannot do both: the two run in
 *  different phases of one layout, so a value carrying both states no
 *  order for them. */
Operator pythonOperator(py::handle value) {
  const bool arranges = py::hasattr(value, "arrange");
  const bool adds = py::hasattr(value, "add");
  if (arranges && adds)
    throw py::type_error(
        "An operator either arranges the children or adds elements to the "
        "settled scope. A value carrying both arrange and add states no "
        "order for them: write one of each instead.");
  if (!arranges && !adds)
    throw py::type_error(
        "An operator is a stock arranging value, a connecting record, or an "
        "object with arrange(arrangement) or add(scope).");
  PythonOperatorValue held{
      retainValue(py::reinterpret_borrow<py::object>(value))};
  if (adds) return Operator{PythonAdding{std::move(held)}};
  const bool measures = py::hasattr(value, "readsChildMinSizes") &&
                        value.attr("readsChildMinSizes").cast<bool>();
  if (measures) return Operator{PythonArranging<true>{std::move(held)}};
  return Operator{PythonArranging<false>{std::move(held)}};
}

/** The stock value @p value holds as an operator, or nothing where it
 *  holds none of them. */
std::optional<Operator> stockOperator(py::handle value) {
  if (py::isinstance<layouts::Radial>(value))
    return Operator{value.cast<layouts::Radial>()};
  if (py::isinstance<layouts::Grid>(value))
    return Operator{value.cast<layouts::Grid>()};
  if (py::isinstance<layouts::Diagonal>(value))
    return Operator{value.cast<layouts::Diagonal>()};
  if (py::isinstance<layouts::BaselineGrid>(value))
    return Operator{value.cast<layouts::BaselineGrid>()};
  if (py::isinstance<layouts::Jittered>(value))
    return Operator{value.cast<layouts::Jittered>()};
  if (py::isinstance<layouts::Jitter>(value))
    return Operator{value.cast<layouts::Jitter>()};
  if (py::isinstance<layouts::AlongPath>(value))
    return Operator{value.cast<layouts::AlongPath>()};
  if (py::isinstance<connect::Between>(value))
    return Operator{value.cast<connect::Between>()};
  if (py::isinstance<connect::ByLane>(value))
    return Operator{value.cast<connect::ByLane>()};
  return std::nullopt;
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

/** The children of a lent arrangement, one bound record each. */
py::list childrenOf(const std::shared_ptr<BorrowedArrangement>& arrangement) {
  py::list children;
  const size_t count = arrangement->get().children.size();
  for (size_t index = 0; index < count; ++index)
    children.append(py::cast(ArrangementChild{arrangement, index}));
  return children;
}

/** @p found, which points into @p scope's own run, as bound nodes. */
py::list nodesOf(const std::shared_ptr<BorrowedScope>& scope,
                 const std::vector<const Scope::Node*>& found) {
  const Scope::Node* first = scope->get().nodes().data();
  py::list nodes;
  for (const Scope::Node* one : found)
    nodes.append(py::cast(ScopeNode{scope, (size_t)(one - first)}));
  return nodes;
}

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

void bindOperator(py::module_& composition) {
  py::class_<Operator> value(
      composition, "Operator",
      "One operator a node runs over its children: a stock arranging "
      "value, a connecting record, or a Python object with "
      "arrange(arrangement) or add(scope). Equality is what lets the node "
      "prune, and a Python object takes part in it only where its own "
      "class states an equality — a frozen dataclass prunes, and a plain "
      "class is the escape hatch that never does.");
  value
      .def(py::init([](py::handle source) { return operatorValue(source); }),
           py::arg("value"))
      .def("zIndex", &Operator::zIndex, py::arg("index"), fluent,
           "Where this operator's additions paint among the owner's "
           "children. Negative paints them behind the authored ones.")
      .def("styleClass", &Operator::styleClass, py::arg("names"), fluent,
           "The classes the additions are dressed by, laid under any the "
           "element names itself.")
      .def("arranges", &Operator::arranges)
      .def("adds", &Operator::adds)
      .def("comparable", &Operator::comparable,
           "Whether this operator takes part in structural equality.")
      .def("readsChildMinSizes", &Operator::readsChildMinSizes)
      .def("__bool__", [](const Operator& self) { return (bool)self; })
      .def("copy", [](const Operator& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(value);
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
      .def_property_readonly(
          "size", [](const ArrangementChild& self) { return self.get().size; })
      .def_property_readonly(
          "baseline",
          [](const ArrangementChild& self) { return self.get().baseline; },
          "The first-baseline offset from the child's top, and not a number "
          "for a child with none.")
      .def_property_readonly(
          "minSize",
          [](const ArrangementChild& self) { return self.get().minSize; },
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
          "rect", [](const ArrangementChild& self) { return self.get().rect; },
          [](const ArrangementChild& self, py::handle where) {
            self.get().place(rect(where));
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
            self.get().place(rect(where));
          },
          py::arg("rect"), "Puts the child here, size included.")
      .def(
          "centreAt",
          [](const ArrangementChild& self, py::handle centre) {
            self.get().centreAt(point(centre));
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
      .def_property_readonly(
          "box", [](const BorrowedArrangement& self) { return self.get().box; })
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
          "bounds", [](const ScopeNode& self) { return self.get().bounds; },
          "Where the node stands, in the scope's coordinates.")
      .def_property_readonly(
          "outline", [](const ScopeNode& self) { return self.get().outline; },
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
              return py::cast(self.get().toLocal(value.cast<SkPath>()));
            return py::cast(self.get().toLocal(point(value)), copied);
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
      .def_property_readonly(
          "box", [](const BorrowedScope& self) { return self.get().box; })
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
      "strings, a list of numbers, or a point.");
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
  return py::none();
}

Operator operatorValue(py::handle value) {
  if (py::isinstance<Operator>(value)) return value.cast<Operator>();
  if (std::optional<Operator> stock = stockOperator(value)) return *stock;
  return pythonOperator(value);
}

std::vector<Operator> operatorList(py::handle value) {
  if (!py::isinstance<py::sequence>(value) || py::isinstance<py::str>(value))
    throw py::type_error("The operators are an ordered sequence of them.");
  std::vector<Operator> list;
  for (py::handle one : value) list.push_back(operatorValue(one));
  return list;
}

bool namesOperator(py::handle value) {
  return py::hasattr(value, "arrange") || py::hasattr(value, "add");
}

void bindComposeSchemes(py::module_& module) {
  auto composition = submodule(module, "compose");
  bindAttributes(composition);
  bindOperator(composition);
  bindArrangement(composition);
  bindScope(composition);
  bindConnect(module);

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
