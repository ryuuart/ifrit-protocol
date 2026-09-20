#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPathOperations(pybind11::module_& module) {
  submodule(module, "geometry.path.operations");
}

}  // namespace sigil::python
