#pragma once

/** @file
 * What the instanced leaf's bindings share across their files: the size
 * and the point read in the words of the slot that asked, the lane of a
 * pool as Python holds it, and the registration of the lane of flights,
 * which stands between the flight it holds and the pool accessor that
 * answers it.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Instances.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <span>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

/** A size read from @p value: a size, or a width and a height. None
 *  loads as a null size that no conversion reports, so it is refused
 *  here in the words of the slot that asked. */
inline SkSize sizeFrom(py::handle value, const char* refusal) {
  if (value.is_none()) throw py::type_error(refusal);
  try {
    return py::cast<SkSize>(value);
  } catch (const py::cast_error&) {
    throw py::type_error(refusal);
  }
}

/** A point read from @p value: a point, or an x and a y. The shared
 *  reading raises a cast error, which reaches Python as a runtime one, so
 *  what is no point is refused here as a type error, in the words of the
 *  slot that asked. */
inline SkPoint pointFrom(py::handle value, const char* refusal) {
  try {
    return point(value);
  } catch (const py::cast_error&) {
    throw py::type_error(refusal);
  }
}

/** ONE LANE OF A POOL, as Python holds it. It keeps the pool and the
 *  accessor that reads the lane, and asks the pool again at every use: the
 *  native span is a view into storage the pool lets go of whenever its
 *  length changes, so nothing here keeps one past the call that took it.
 *  A lane held across an `add`, a `resize` or a `clear` therefore reads
 *  the pool as it then stands. */
template <class Item>
class Lane {
 public:
  using Access = std::span<Item> (*)(compose::instancing::Pool&);

  Lane(std::shared_ptr<compose::instancing::Pool> pool, Access access)
      : m_pool(std::move(pool)), m_access(access) {}

  /** The lane as it stands now. An opt-in lane is materialised by the
   *  asking, as the native accessor materialises it. */
  std::span<Item> items() const { return m_access(*m_pool); }

 private:
  std::shared_ptr<compose::instancing::Pool> m_pool;
  Access m_access;
};

/** Registers the lane of flights on @p module, on the terms every lane
 *  is registered on. Its caller registers the flight first, because a
 *  signature is written when its function is registered. */
void bindFlightLane(py::module_& module);

}  // namespace sigil::python
