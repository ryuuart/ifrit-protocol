#include <sigilmaterial/core/Material.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindMaterialCore(py::module_& module) {
  auto materials = submodule(module, "material");
  py::class_<material::Material>(materials, "Material")
      .def("copy", [](const material::Material& value) { return value; });
}

}  // namespace sigil::python
