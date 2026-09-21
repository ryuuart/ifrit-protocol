#include <sigilgeometry/path/Band.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryProfiles(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ path = submodule(module, "geometry.path");
  submodule(module, "geometry.path.profile");
  submodule(module, "geometry.shapers");

  py::enum_<geometry::path::Formation>(path, "Formation")
      .value("Center", geometry::path::Formation::Center)
      .value("Inner", geometry::path::Formation::Inner)
      .value("Outer", geometry::path::Formation::Outer);
}

}  // namespace sigil::python
