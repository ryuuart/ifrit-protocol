#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryMeshCodec(pybind11::module_& module) {
  submodule(module, "geometry.mesh.codec");
  submodule(module, "geometry.mesh.codec.decode");
  submodule(module, "geometry.mesh.codec.encode");
}

}  // namespace sigil::python
