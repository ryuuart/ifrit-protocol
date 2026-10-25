#pragma once

/** @file
 * What the operator seam's bindings share across their files: the
 * arrangement and the settled scope a native layout lends to Python for
 * one call, the records reached through them, and the stamp whose maker
 * is a Python callable. Each type is registered once and cast to from
 * several files, so each is one type here rather than a private copy in
 * every file that names it.
 */

#include <pybind11/pybind11.h>
#include <sigilcompose/core/Operator.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/compose/Convert.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

/** ONE NATIVE ARRANGEMENT, LENT TO PYTHON for the call it was handed to.
 *  The records it carries stand in the composer's own layout round, so
 *  every reading asks this wrapper first: it refuses once the call has
 *  returned, and on a thread other than the one that lays out. */
class BorrowedArrangement {
 public:
  explicit BorrowedArrangement(compose::Arrangement& arrangement)
      : m_thread(std::this_thread::get_id()), m_arrangement(&arrangement) {}

  compose::Arrangement& get() const {
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
  compose::Arrangement* m_arrangement;
};

/** ONE SETTLED SCOPE, LENT TO PYTHON on the same terms, with the one
 *  thing past that a lent scope has to say about itself: whether there
 *  is anything left to attach to. A program that draws is handed a
 *  scope to read, where attaching would be too late to mean anything,
 *  so it attaches nothing — silently, as the native snapshot's own
 *  nodes do. */
class BorrowedScope {
 public:
  explicit BorrowedScope(compose::Scope& scope)
      : m_thread(std::this_thread::get_id()),
        m_scope(&scope),
        m_attachable(&scope) {}
  explicit BorrowedScope(const compose::Scope& scope)
      : m_thread(std::this_thread::get_id()), m_scope(&scope) {}

  const compose::Scope& get() const {
    if (std::this_thread::get_id() != m_thread)
      throw std::runtime_error(
          "A scope can only be read on the thread that laid it out.");
    if (!m_scope)
      throw std::runtime_error(
          "This scope is no longer inside the call it was handed to.");
    return *m_scope;
  }

  /** The scope to attach to, or null where nothing can be attached. */
  compose::Scope* attachable() const {
    get();
    return m_attachable;
  }

  void invalidate() {
    m_scope = nullptr;
    m_attachable = nullptr;
  }

 private:
  const std::thread::id m_thread;
  const compose::Scope* m_scope;
  compose::Scope* m_attachable = nullptr;
};

/** ONE NODE of a lent scope, addressed by its place in the run. */
class ScopeNode {
 public:
  ScopeNode(std::shared_ptr<BorrowedScope> scope, size_t index)
      : m_scope(std::move(scope)), m_index(index) {}

  const compose::Scope::Node& get() const {
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
  explicit ArrangementLoan(compose::Arrangement& arrangement)
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
  explicit ScopeLoan(compose::Scope& scope)
      : m_view(std::make_shared<BorrowedScope>(scope)),
        m_object(py::cast(m_view)) {}
  explicit ScopeLoan(const compose::Scope& scope)
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

/** A STAMP WHOSE MAKER IS A PYTHON CALLABLE, handed each node stating the
 *  lane as the scope sees it. The native stamp walks the nodes and
 *  attaches what the maker answers; this lends it the scope for the call,
 *  so the maker reads a node exactly as an adding operator does. Two
 *  compare as the native stamp does: by a key the author states, since a
 *  callable has no equality of its own. */
struct PythonStamp {
  std::string lane;
  std::string key;
  std::shared_ptr<PythonCallback> make;
  bool operator==(const PythonStamp& other) const {
    return !key.empty() && key == other.key && lane == other.lane;
  }
  void add(compose::Scope& scope) const {
    if (!make) return;
    const py::gil_scoped_acquire lock;
    py::function maker;
    try {
      maker = make->get();
    } catch (const std::runtime_error&) {
      return;
    }
    const ScopeLoan lent{scope};
    const auto view = lent.scope().cast<std::shared_ptr<BorrowedScope>>();
    const compose::Scope::Node* first = scope.nodes().data();
    const compose::stamp::ByLane native{
        .lane = lane, .key = key, .make = [&](const compose::Scope::Node& one) {
          py::object made;
          try {
            made = maker(ScopeNode{view, (size_t)(&one - first)});
          } catch (const py::error_already_set& error) {
            throw std::runtime_error(error.what());
          }
          return node(made);
        }};
    native.add(scope);
  }
};

}  // namespace sigil::python
