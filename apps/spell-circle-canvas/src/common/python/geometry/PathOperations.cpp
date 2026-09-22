#include <sigilgeometry/path/Stroke.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPathOperations(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ path = submodule(module, "geometry.path");
  submodule(module, "geometry.path.operations");

  py::enum_<geometry::path::Cap>(path, "Cap")
      .value("Butt", geometry::path::Cap::Butt)
      .value("Round", geometry::path::Cap::Round)
      .value("Square", geometry::path::Cap::Square);
  py::enum_<geometry::path::Join>(path, "Join")
      .value("Round", geometry::path::Join::Round)
      .value("Miter", geometry::path::Join::Miter)
      .value("Bevel", geometry::path::Join::Bevel);
}

}  // namespace sigil::python
