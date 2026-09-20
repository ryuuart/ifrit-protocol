#pragma once

/** @file
 * The pieces every binding in this library is written out of: record
 * construction from keyword arguments, the colour reading, the pen a
 * callback may draw through, and the ownership that lets a Python
 * callable be held by a native description without outliving its host.
 */

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace sigil::draw {
class Pen;
}

/** THE NATIVE LIBRARIES AS ONE PYTHON EXTENSION MODULE. The bindings
 *  themselves, the conversions every one of them shares, and the
 *  ownership that decides when a Python callable held by a native
 *  description is released. Reach for it to build an extension module
 *  over these libraries, or to add a binding to the one that exists.
 *  It imports no sketch runtime and no host: who initialises the
 *  interpreter and assembles the module is the caller's business. */
namespace sigil::python {

/** Construct native records through their bound setters so keyword arguments
 *  and subsequent assignments share conversion and ownership policies. */
template <class T>
T keywordValue(pybind11::kwargs fields,
               const char* unknownField = "Unknown field: ") {
  auto value = pybind11::cast(T{});
  for (const auto& [key, item] : fields) {
    const auto name = pybind11::cast<std::string>(key);
    if (!pybind11::hasattr(value, name.c_str()))
      throw pybind11::type_error(std::string(unknownField) + name);
    pybind11::setattr(value, name.c_str(), item);
  }
  return value.template cast<T>();
}

/** Binds a native record as @p name on @p module: constructible from
 *  keyword arguments through `keywordValue`, and copyable. @p
 *  unknownField opens the message a field the record does not have
 *  raises. */
template <class T>
pybind11::class_<T> bindRecord(pybind11::module_& module, const char* name,
                               const char* unknownField) {
  return pybind11::class_<T>(module, name)
      .def(pybind11::init([unknownField](pybind11::kwargs fields) {
        return keywordValue<T>(fields, unknownField);
      }))
      .def("copy", [](const T& value) { return value; });
}

/** Reads a native color from a string or an RGB or RGBA sequence whose
 *  channels are between zero and one. Requires the interpreter lock. */
SkColor4f color(pybind11::handle value);

/** A callback-scoped pen. Native extensions pass this same checked wrapper
 *  through every binding that draws, including offscreen and brush calls. */
class BorrowedPen {
 public:
  /** Borrows @p pen for the thread this is constructed on. */
  explicit BorrowedPen(draw::Pen& pen);
  /** The pen itself. Throws once the callback it was lent to has
   *  closed, or when another thread asks. */
  draw::Pen& get() const;
  /** Ends the loan and runs whatever was registered to run at close. */
  void invalidate();
  /** Registers @p cleanup to run when the loan ends. */
  void whenClosed(std::function<void()> cleanup);

 private:
  const std::thread::id m_thread;
  draw::Pen* m_pen;
  std::vector<std::function<void()>> m_cleanup;
};

/** The pen inside @p value, which must be a bound `BorrowedPen`. */
draw::Pen& pen(pybind11::handle value);
/** Calls @p function with @p pen lent to it, and ends the loan however
 *  the call returns, so no pen survives the callback that was given
 *  it. */
void invokePen(const pybind11::function& function, draw::Pen& pen);

/** A Python value held by native descriptions, released when its host lifetime
 * closes. get() requires the interpreter lock and returns an owning reference.
 */
class PythonValue {
 public:
  /** Takes a reference to @p value; the interpreter lock must be
   *  held. */
  explicit PythonValue(pybind11::object value);
  virtual ~PythonValue();
  PythonValue(const PythonValue&) = delete;
  PythonValue& operator=(const PythonValue&) = delete;
  /** The value as an owning reference; the interpreter lock must be
   *  held. Empty once it has been cleared. */
  pybind11::object get() const;
  /** Releases the reference, leaving the handle inert. */
  void clear();

 private:
  PyObject* m_value;
};

/** A retained Python value that is known to be callable, so a native
 *  description holding one can call it back without casting at every
 *  site. */
class PythonCallback final : public PythonValue {
 public:
  /** Takes a reference to @p function; the interpreter lock must be
   *  held. */
  explicit PythonCallback(pybind11::function function)
      : PythonValue(std::move(function)) {}
  /** The callable as an owning reference; the interpreter lock must be
   *  held. */
  pybind11::function get() const {
    return PythonValue::get().cast<pybind11::function>();
  }
};

/** Retains @p function against the lifetime of the scope in force on
 *  this thread, so closing that lifetime releases it. */
std::shared_ptr<PythonCallback> retainCallback(pybind11::function function);
/** Retains @p value on the same terms as `retainCallback`. */
std::shared_ptr<PythonValue> retainValue(pybind11::object value);

/** A host's weak registry of retained Python values. Closing the
 *  lifetime releases their Python callables even when an author retains an
 *  Element containing a bound method of the authoring object itself. */
class CallbackLifetime {
 public:
  CallbackLifetime();
  ~CallbackLifetime();
  CallbackLifetime(const CallbackLifetime&) = delete;
  CallbackLifetime& operator=(const CallbackLifetime&) = delete;

  /** Releases every value retained against this lifetime. */
  void clear();

 private:
  void retain(const std::shared_ptr<PythonValue>& value);
  struct Impl;
  std::unique_ptr<Impl> m_impl;
  friend std::shared_ptr<PythonCallback> retainCallback(pybind11::function);
  friend std::shared_ptr<PythonValue> retainValue(pybind11::object);
};

/** Associates callbacks constructed on this thread with a host lifetime. The
 *  owner must outlive the scope; nested scopes restore their prior owner. */
class CallbackScope {
 public:
  /** Makes @p owner the lifetime callbacks made on this thread are
   *  retained against, until this scope ends. */
  explicit CallbackScope(CallbackLifetime& owner);
  ~CallbackScope();
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope& operator=(const CallbackScope&) = delete;

 private:
  CallbackLifetime* m_previous;
};

/** Keeps an active native scope on its owning thread until explicitly closed
 *  or unwound at a Python callback boundary. The close function must not throw
 *  and releases only the scope's native state, leaving retained handles inert.
 */
void retainScope(std::shared_ptr<void> owner, void (*close)(void*) noexcept);
/** Whether @p owner is the innermost retained scope on this thread. */
[[nodiscard]] bool isCurrentScope(const void* owner);
/** Closes the innermost retained scope. The caller validates its thread and
 *  native environment before requesting a close. */
void closeScope(const void* owner);

/** Closes native scopes opened by a Python callback before the enclosing
 *  native environment is restored, including scopes the author abandoned. */
class CallbackBoundary {
 public:
  CallbackBoundary();
  ~CallbackBoundary();
  CallbackBoundary(const CallbackBoundary&) = delete;
  CallbackBoundary& operator=(const CallbackBoundary&) = delete;

 private:
  std::uint64_t m_lastScope;
};

}  // namespace sigil::python
