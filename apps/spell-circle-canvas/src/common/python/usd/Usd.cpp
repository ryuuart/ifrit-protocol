#include <sigilpython/Extend.h>
#include <sigilpython/usd/Registration.h>

namespace sigil::python {

void bindUsd(pybind11::module_& module) {
  submodule(module, "usd");
}

}  // namespace sigil::python
