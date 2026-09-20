#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryCharts(pybind11::module_& module) {
  submodule(module, "geometry.path");
  submodule(module, "geometry.path.crossing");
}

}  // namespace sigil::python
