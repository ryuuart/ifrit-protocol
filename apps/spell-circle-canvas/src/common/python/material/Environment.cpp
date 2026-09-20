#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/kit/Pbr.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

#include <initializer_list>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

void bindMaterialEnvironment(py::module_& module) {
  using Parameters = material::kit::SurfaceParameters;
  auto kit = submodule(module, "material.kit");
  auto parameters = bindRecord<Parameters>(kit, "SurfaceParameters",
                                           "Unknown surface parameter: ");
  for (const auto& [name, member] : std::initializer_list<
           std::pair<const char*, material::Color Parameters::*>>{
           {"baseColor", &Parameters::baseColor},
           {"emissive", &Parameters::emissive},
           {"absorption", &Parameters::absorption}}) {
    parameters.def_property(
        name,
        // A colour answers as the colour class rather than as four bare
        // numbers, so a parameter taken off one surface is a value the
        // next one accepts. Nothing is converted either way: these
        // fields are factors on the maps beside them and `absorption`
        // is a coefficient that runs past one, so a transfer function
        // here would change what the author wrote and clip what does
        // not belong inside the unit range.
        [member](const Parameters& self) { return self.*member; },
        [member](Parameters& self, py::handle value) {
          self.*member = material::Color(color(value));
        });
  }
  parameters.def_readwrite("metallic", &Parameters::metallic)
      .def_readwrite("roughness", &Parameters::roughness)
      .def_readwrite("emissiveStrength", &Parameters::emissiveStrength)
      .def_readwrite("normalScale", &Parameters::normalScale)
      .def_readwrite("normalDirectX", &Parameters::normalDirectX)
      .def_readwrite("roughnessChannel", &Parameters::roughnessChannel)
      .def_readwrite("metallicChannel", &Parameters::metallicChannel)
      .def_readwrite("occlusionChannel", &Parameters::occlusionChannel)
      .def_readwrite("occlusionStrength", &Parameters::occlusionStrength)
      .def_readwrite("opacityChannel", &Parameters::opacityChannel)
      .def_readwrite("alphaCutoff", &Parameters::alphaCutoff)
      .def_readwrite("transmission", &Parameters::transmission)
      .def_readwrite("ior", &Parameters::ior)
      .def_readwrite("thickness", &Parameters::thickness)
      .def_readwrite("reflectionWeight", &Parameters::reflectionWeight)
      .def_static("chrome", &Parameters::chrome)
      .def_static("gold", &Parameters::gold)
      .def_static("glass", &Parameters::glass)
      .def_static(
          "metal",
          [](py::handle tint, float roughness) {
            return Parameters::metal(color(tint), roughness);
          },
          py::arg("tint"), py::arg("roughness"))
      .def_static(
          "dielectric",
          [](py::handle baseColor, float roughness) {
            return Parameters::dielectric(color(baseColor), roughness);
          },
          py::arg("baseColor"), py::arg("roughness"));
  py::enum_<material::kit::Reflection>(kit, "Reflection")
      .value("SplitSum", material::kit::Reflection::SplitSum)
      .value("Additive", material::kit::Reflection::Additive);
  kit.def("surface",
          py::overload_cast<const Parameters&, material::kit::Reflection>(
              &material::kit::surface),
          py::arg("parameters") = Parameters{},
          py::arg("reflection") = material::kit::Reflection::SplitSum);
  kit.def("unlit", &material::kit::unlit, py::arg("parameters") = Parameters{});
}

}  // namespace sigil::python
