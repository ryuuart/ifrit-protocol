#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryProfiles(pybind11::module_& module) {
  submodule(module, "geometry.path");
  submodule(module, "geometry.path.profile");
  submodule(module, "geometry.shapers");
}

}  // namespace sigil::python
