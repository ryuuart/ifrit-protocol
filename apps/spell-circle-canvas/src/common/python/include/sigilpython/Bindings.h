#pragma once

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
  explicit BorrowedPen(draw::Pen& pen);
  draw::Pen& get() const;
  void invalidate();
  void whenClosed(std::function<void()> cleanup);

 private:
  const std::thread::id m_thread;
  draw::Pen* m_pen;
  std::vector<std::function<void()>> m_cleanup;
};

draw::Pen& pen(pybind11::handle value);
void invokePen(const pybind11::function& function, draw::Pen& pen);

/** A Python value held by native descriptions, released when its host lifetime
 * closes. get() requires the interpreter lock and returns an owning reference.
 */
class PythonValue {
 public:
  explicit PythonValue(pybind11::object value);
  virtual ~PythonValue();
  PythonValue(const PythonValue&) = delete;
  PythonValue& operator=(const PythonValue&) = delete;
  pybind11::object get() const;
  void clear();

 private:
  PyObject* m_value;
};

class PythonCallback final : public PythonValue {
 public:
  explicit PythonCallback(pybind11::function function)
      : PythonValue(std::move(function)) {}
  pybind11::function get() const {
    return PythonValue::get().cast<pybind11::function>();
  }
};

std::shared_ptr<PythonCallback> retainCallback(pybind11::function function);
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
