#pragma once

#include <include/core/SkColor.h>
#include <pybind11/pybind11.h>

#include <memory>

namespace sigil::sketch::python {

/** Reads a native color from a string or an RGB or RGBA sequence whose
 *  channels are between zero and one. Requires the interpreter lock. */
SkColor4f color(pybind11::handle value);

/** A session's weak registry of native drawing callbacks. Closing the
 *  session releases their Python callables even when an author retains an
 *  Element containing a bound method of the authoring object itself. */
class CallbackLifetime {
 public:
  CallbackLifetime();
  ~CallbackLifetime();
  CallbackLifetime(const CallbackLifetime&) = delete;
  CallbackLifetime& operator=(const CallbackLifetime&) = delete;

  void clear();

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
  friend void bindDrawing(pybind11::module_& module);
};

/** Associates callbacks constructed on this thread with a session. The
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

/** Registers the native drawing descriptions in either an embedded module
 *  or an ordinary Python extension. The interpreter must outlive descriptions
 *  carrying Python callbacks. */
void bindDrawing(pybind11::module_& module);

}  // namespace sigil::sketch::python
