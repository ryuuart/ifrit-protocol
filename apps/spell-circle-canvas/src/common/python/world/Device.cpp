#include <sigilpython/Extend.h>
#include <sigilpython/world/Registration.h>

namespace sigil::python {

void bindWorldDevice(pybind11::module_& module) {
  submodule(module, "world.diligent");
  submodule(module, "core.hardware");
}

}  // namespace sigil::python
