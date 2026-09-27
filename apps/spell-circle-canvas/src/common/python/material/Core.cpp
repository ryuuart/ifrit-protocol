#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>

#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

}  // namespace

void bindMaterialCore(py::module_& module) {
  auto materials = submodule(module, "material");
  py::enum_<material::MaskChannel>(materials, "MaskChannel")
      .value("Alpha", material::MaskChannel::Alpha)
      .value("Luminance", material::MaskChannel::Luminance)
      .value("Red", material::MaskChannel::Red)
      .value("Green", material::MaskChannel::Green)
      .value("Blue", material::MaskChannel::Blue);
  py::class_<material::Material> type(materials, "Material");
  py::implicitly_convertible<material::Color, material::Material>();
  py::class_<material::Mask>(materials, "Mask")
      .def(py::init([](const material::Material& source,
                       material::MaskChannel channel, float low, float high,
                       bool invert) {
             return material::Mask{source, channel, low, high, invert};
           }),
           py::arg("source"), py::arg("channel") = material::MaskChannel::Alpha,
           py::arg("low") = 0.0f, py::arg("high") = 1.0f,
           py::arg("invert") = false)
      .def(py::self == py::self);
}

void bindMaterialBuilder(py::module_& module) {
  auto materials = submodule(module, "material");
  auto type = py::reinterpret_borrow<py::class_<material::Material>>(
      py::type::of<material::Material>());
  type.def(py::init([](py::handle color) {
             return material::Material(materialColor(color));
           }),
           py::arg("color"))
      .def("copy", [](const material::Material& value) { return value; })
      .def(
          "layer",
          [](material::Material& self, const material::Material& source,
             material::BlendMode blend, float opacity,
             std::optional<material::Mask> mask) -> material::Material& {
            return self.layer(source, {blend, opacity, std::move(mask)});
          },
          py::arg("source"), py::arg("blend") = material::BlendMode::Normal,
          py::arg("opacity") = 1.0f, py::arg("mask") = py::none(), fluent)
      .def(
          "surface",
          [](material::Material& self, material::Channel metallic,
             material::Channel roughness, material::Channel occlusion,
             std::optional<material::Material> normal, float normalScale,
             bool normalDirectX, const material::Color& emission,
             float emissionStrength, std::optional<material::Material> emissionMap,
             float alphaCutoff, float clearcoat, float transmission, float ior,
             float thickness, const material::Color& absorption,
             float reflectionWeight, bool unlit) -> material::Material& {
            return self.surface(
                {std::move(metallic), std::move(roughness), std::move(occlusion),
                 std::move(normal), normalScale, normalDirectX, emission,
                 emissionStrength, std::move(emissionMap), alphaCutoff,
                 clearcoat, transmission, ior, thickness, absorption,
                 reflectionWeight, unlit});
          },
          py::kw_only(),
          py::arg_v("metallic", material::Channel(0.0f), "0.0"),
          py::arg_v("roughness", material::Channel(0.5f), "0.5"),
          py::arg_v("occlusion", material::Channel(1.0f), "1.0"),
          py::arg("normal") = py::none(), py::arg("normalScale") = 1.0f,
          py::arg("normalDirectX") = false,
          py::arg_v("emission", material::Color{0, 0, 0, 1}, "Color(0, 0, 0, 1)"),
          py::arg("emissionStrength") = 0.0f,
          py::arg("emissionMap") = py::none(), py::arg("alphaCutoff") = 0.0f,
          py::arg("clearcoat") = 0.0f, py::arg("transmission") = 0.0f,
          py::arg("ior") = 1.5f, py::arg("thickness") = 40.0f,
          py::arg_v("absorption", material::Color{0, 0, 0, 1}, "Color(0, 0, 0, 1)"),
          py::arg("reflectionWeight") = 1.0f, py::arg("unlit") = false, fluent)
      .def(
          "effects",
          [](material::Material& self, const material::Filter& chain)
              -> material::Material& { return self.effects(chain); },
          py::arg("filter"), fluent)
      .def(
          "set",
          [](material::Material& self, const std::string& name, float value)
              -> material::Material& { return self.set(name, value); },
          py::arg("name"), py::arg("value"), fluent)
      .def(
          "bind",
          [](material::Material& self, const std::string& name,
             py::object value) -> material::Material& {
            return self.bind(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("hasProgram", &material::Material::hasProgram)
      .def("isRunning", &material::Material::isRunning)
      .def("base", &material::Material::base)
      .def(py::self == py::self);
  materials.def(
      "from_",
      [](py::handle base) {
        // A chain starts from a material, a paint, or any colour spelling.
        if (py::isinstance<material::Material>(base) ||
            py::isinstance<material::Color>(base))
          return material::from(py::cast<material::Material>(base));
        try {
          return material::from(py::cast<material::Material>(base));
        } catch (const py::cast_error&) {
          return material::from(materialColor(base));
        }
      },
      py::arg("base"));
  materials.def(
      "noise",
      [](float frequency, int octaves, float seed, bool turbulence, bool grain,
         float contrast, float stretch) {
        return material::noise(frequency, {octaves, seed, turbulence, grain,
                                           contrast, stretch});
      },
      py::arg("frequency"), py::arg("octaves") = 4, py::arg("seed") = 1.0f,
      py::arg("turbulence") = false, py::arg("grain") = false,
      py::arg("contrast") = 1.0f, py::arg("stretch") = 1.0f);
}

}  // namespace sigil::python
