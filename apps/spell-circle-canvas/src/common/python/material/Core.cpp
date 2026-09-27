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

/** A surface channel as Python writes it: a number, or a material. */
material::Channel channel(py::handle value) {
  if (py::isinstance<material::Material>(value))
    return py::cast<material::Material>(value);
  return py::cast<float>(value);
}

/** The surface's fields from keyword arguments; an unknown one raises. */
material::SurfaceOptions surfaceOptions(const py::kwargs& fields) {
  material::SurfaceOptions options;
  for (const auto& [key, value] : fields) {
    const std::string name = py::cast<std::string>(key);
    if (name == "metallic") options.metallic = channel(value);
    else if (name == "roughness") options.roughness = channel(value);
    else if (name == "occlusion") options.occlusion = channel(value);
    else if (name == "normal") options.normal = py::cast<material::Material>(value);
    else if (name == "normalScale") options.normalScale = py::cast<float>(value);
    else if (name == "normalDirectX") options.normalDirectX = py::cast<bool>(value);
    else if (name == "emission") options.emission = materialColor(value);
    else if (name == "emissionStrength") options.emissionStrength = py::cast<float>(value);
    else if (name == "emissionMap") options.emissionMap = py::cast<material::Material>(value);
    else if (name == "alphaCutoff") options.alphaCutoff = py::cast<float>(value);
    else if (name == "clearcoat") options.clearcoat = py::cast<float>(value);
    else if (name == "transmission") options.transmission = py::cast<float>(value);
    else if (name == "ior") options.ior = py::cast<float>(value);
    else if (name == "thickness") options.thickness = py::cast<float>(value);
    else if (name == "absorption") options.absorption = materialColor(value);
    else if (name == "reflectionWeight") options.reflectionWeight = py::cast<float>(value);
    else if (name == "unlit") options.unlit = py::cast<bool>(value);
    else throw py::type_error("Unknown surface field: " + name);
  }
  return options;
}

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
  type.def(py::init([](py::handle color) {
             return material::Material(materialColor(color));
           }),
           py::arg("color"))
      .def("copy", [](const material::Material& value) { return value; })
      .def(
          "layer",
          [](material::Material& self, const material::Material& source,
             py::object blend, float opacity,
             std::optional<material::Mask> mask) -> material::Material& {
            material::LayerOptions options;
            if (!blend.is_none()) options.blend = py::cast<material::BlendMode>(blend);
            options.opacity = opacity;
            options.mask = std::move(mask);
            return self.layer(source, options);
          },
          py::arg("source"), py::arg("blend") = py::none(),
          py::arg("opacity") = 1.0f, py::arg("mask") = py::none(), fluent)
      .def(
          "surface",
          [](material::Material& self, const py::kwargs& fields)
              -> material::Material& {
            return self.surface(surfaceOptions(fields));
          },
          fluent)
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
  materials.def(
      "from_", [](const material::Material& base) { return material::from(base); },
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
