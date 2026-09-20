#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPointKernels(pybind11::module_& module) {
  submodule(module, "geometry.mesh.kernel");
}

}  // namespace sigil::python
