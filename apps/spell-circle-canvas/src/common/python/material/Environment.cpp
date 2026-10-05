#include <pybind11/operators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

#include <initializer_list>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

void bindMaterialEnvironment(py::module_& module) {
  using Parameters = material::surface::SurfaceParameters;
  auto surface = submodule(module, "material.surface");
  auto parameters = bindRecord<Parameters>(surface, "SurfaceParameters",
                                           "Unknown surface parameter: ");
  for (const auto& [name, member] : std::initializer_list<
           std::pair<const char*, material::Color Parameters::*>>{
           {"baseColor", &Parameters::baseColor},
           {"emissive", &Parameters::emissive},
           {"attenuationColor", &Parameters::attenuationColor},
           {"sheenColor", &Parameters::sheenColor},
           {"specularColorFactor", &Parameters::specularColorFactor}}) {
    parameters.def_property(
        name,
        // A colour answers as the colour class rather than as four bare
        // numbers, so a parameter taken off one surface is a value the
        // next one accepts. Nothing is converted either way: the colours
        // are held as the author typed them and decoded by the renderer.
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
      .def_readwrite("attenuationDistance", &Parameters::attenuationDistance)
      .def_readwrite("dispersion", &Parameters::dispersion)
      .def_readwrite("distortionStrength", &Parameters::distortionStrength)
      .def_readwrite("reflectionWeight", &Parameters::reflectionWeight)
      .def_readwrite("clearcoat", &Parameters::clearcoat)
      .def_readwrite("clearcoatRoughness", &Parameters::clearcoatRoughness)
      .def_readwrite("clearcoatChannel", &Parameters::clearcoatChannel)
      .def_readwrite("clearcoatRoughnessChannel",
                     &Parameters::clearcoatRoughnessChannel)
      .def_readwrite("sheenRoughness", &Parameters::sheenRoughness)
      .def_readwrite("sheenRoughnessChannel",
                     &Parameters::sheenRoughnessChannel)
      .def_readwrite("anisotropy", &Parameters::anisotropy)
      .def_readwrite("anisotropyRotation", &Parameters::anisotropyRotation)
      .def_readwrite("iridescence", &Parameters::iridescence)
      .def_readwrite("iridescenceIor", &Parameters::iridescenceIor)
      .def_readwrite("iridescenceThickness", &Parameters::iridescenceThickness)
      .def_readwrite("iridescenceThicknessMinimum",
                     &Parameters::iridescenceThicknessMinimum)
      .def_readwrite("iridescenceChannel", &Parameters::iridescenceChannel)
      .def_readwrite("iridescenceThicknessChannel",
                     &Parameters::iridescenceThicknessChannel)
      .def_readwrite("specular", &Parameters::specular)
      .def_readwrite("specularChannel", &Parameters::specularChannel);
  py::enum_<material::surface::Reflection>(surface, "Reflection")
      .value("SplitSum", material::surface::Reflection::SplitSum)
      .value("Additive", material::surface::Reflection::Additive);
  surface.def(
      "program",
      py::overload_cast<const Parameters&, material::surface::Reflection>(
          &material::surface::program),
      py::arg("parameters") = Parameters{},
      py::arg("reflection") = material::surface::Reflection::SplitSum);
  surface.def("unlit", &material::surface::unlit,
              py::arg("parameters") = Parameters{});
  bindRecord<material::surface::HeightNormalOptions>(
      surface, "HeightNormalOptions", "Unknown HeightNormalOptions field: ")
      .def_readwrite("depth", &material::surface::HeightNormalOptions::depth)
      .def_readwrite("step", &material::surface::HeightNormalOptions::step)
      .def_readwrite("directX",
                     &material::surface::HeightNormalOptions::directX)
      .def(py::self == py::self);
  bindRecord<material::surface::NormalBlendOptions>(
      surface, "NormalBlendOptions", "Unknown NormalBlendOptions field: ")
      .def_readwrite("baseDirectX",
                     &material::surface::NormalBlendOptions::baseDirectX)
      .def_readwrite("detailDirectX",
                     &material::surface::NormalBlendOptions::detailDirectX)
      .def_readwrite("outputDirectX",
                     &material::surface::NormalBlendOptions::outputDirectX)
      .def(py::self == py::self);
  surface.def("normalFromHeight", &material::surface::normalFromHeight,
              py::arg("height"),
              py::arg("options") = material::surface::HeightNormalOptions{});
  surface.def("blendNormals", &material::surface::blendNormals, py::arg("base"),
              py::arg("detail"),
              py::arg("options") = material::surface::NormalBlendOptions{});
}

}  // namespace sigil::python
