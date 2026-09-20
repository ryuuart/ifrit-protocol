#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryDeviceHandle(pybind11::module_& module) {
  submodule(module, "geometry.device");
}

}  // namespace sigil::python
