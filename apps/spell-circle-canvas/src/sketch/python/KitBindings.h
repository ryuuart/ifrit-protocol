#pragma once

#include <pybind11/pybind11.h>

#include <cstddef>

namespace sigil::sketch::kit {
struct Stage;
}

namespace sigil::sketch::python {

void bindKits(pybind11::module_& module);

/** Declares a stage through a validated session context. */
void stageContext(pybind11::handle context, const kit::Stage& stage);

/** Restores theme providers opened by a Python callback, including a scope
 *  whose author forgot to close it. Construct and destroy on one thread. */
class KitScopeBoundary {
 public:
  KitScopeBoundary();
  ~KitScopeBoundary();
  KitScopeBoundary(const KitScopeBoundary&) = delete;
  KitScopeBoundary& operator=(const KitScopeBoundary&) = delete;

 private:
  std::size_t m_depth;
};

}  // namespace sigil::sketch::python
