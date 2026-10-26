/** @file
 * The operator a node runs over its children, bound as
 * `compose.Operator`, and the readings that build one: from a stock
 * arranging value, from a connecting or adding record, or from a Python
 * object that arranges or adds — and what lets the third one prune is
 * an equality of its own.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Outline.h>
#include <sigilcompose/kit/Pin.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Operators.h>
#include <sigilpython/compose/Registration.h>

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "OperatorSeam.h"

namespace sigil::python {
namespace py = pybind11;
namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;

namespace layouts = sigil::compose::layouts;
namespace connect = sigil::compose::connect;
using compose::Arrangement;
using compose::Operator;
using compose::Scope;

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
  const ArrangementLoan lent{arrangement};
  try {
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
    value.attr("add")(lent.scope());
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** A Python value with `arrange(arrangement)` as a native arranging
 *  operator. @p Measures is what the value asked for with
 *  `readsChildMinimumSizes`, and it is the model's TYPE that carries the
 *  answer: the measure costs one text layout per text child, so a value
 *  that did not ask makes the model that does not pay for it. */
template <bool Measures>
struct PythonArranging {
  PythonOperatorValue value;
  static constexpr bool readsChildMinimumSizes = Measures;
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
  const bool measures = py::hasattr(value, "readsChildMinimumSizes") &&
                        value.attr("readsChildMinimumSizes").cast<bool>();
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
  if (py::isinstance<connect::Along>(value))
    return Operator{value.cast<connect::Along>()};
  if (py::isinstance<compose::pin::ByLane>(value))
    return Operator{value.cast<compose::pin::ByLane>()};
  if (py::isinstance<compose::outline::Around>(value))
    return Operator{value.cast<compose::outline::Around>()};
  if (py::isinstance<compose::outline::Hull>(value))
    return Operator{value.cast<compose::outline::Hull>()};
  if (py::isinstance<PythonStamp>(value))
    return Operator{value.cast<PythonStamp>()};
  return std::nullopt;
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
      .def("readsChildMinimumSizes", &Operator::readsChildMinimumSizes)
      .def("__bool__", [](const Operator& self) { return (bool)self; })
      .def("copy", [](const Operator& self) { return self; })
      .def(py::self == py::self);
  copyProtocol(value);
}

}  // namespace

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
  bindOperator(composition);
}

}  // namespace sigil::python
