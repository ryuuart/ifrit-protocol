#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryFrames(pybind11::module_& module) {
  submodule(module, "geometry.path");
  submodule(module, "geometry.shapes");
}

}  // namespace sigil::python
