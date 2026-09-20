#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPointBuilder(pybind11::module_& module) {
  submodule(module, "geometry.mesh.pop");
  submodule(module, "geometry.mesh.pop.profile");
}

}  // namespace sigil::python
