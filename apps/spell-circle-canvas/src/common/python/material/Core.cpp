#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/texture/Channel.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>

#include <optional>
#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {
constexpr auto fluent = py::return_value_policy::reference_internal;

/** A layer's mask as a `Mask`, None, or the mask's own fields in a tuple
 *  — `(depth, ColorChannel.Luminance)`, with its range and inversion
 *  after the channel — the way the native layer takes them in braces. */
material::LayerMask layerMask(const py::object& mask) {
  if (mask.is_none()) return {};
  if (py::isinstance<material::Mask>(mask))
    return std::optional<material::Mask>(mask.cast<material::Mask>());
  if (py::isinstance<py::tuple>(mask) || py::isinstance<py::list>(mask)) {
    const auto fields = mask.cast<py::sequence>();
    if (fields.size() < 2 || fields.size() > 5)
      throw py::type_error(
          "A mask tuple is (source, channel) with an optional low, high and "
          "invert after the channel.");
    material::Mask value{fields[0].cast<material::Material>(),
                         fields[1].cast<material::ColorChannel>()};
    if (fields.size() > 2) value.low = fields[2].cast<float>();
    if (fields.size() > 3) value.high = fields[3].cast<float>();
    if (fields.size() > 4) value.invert = fields[4].cast<bool>();
    return std::optional<material::Mask>(std::move(value));
  }
  throw py::type_error(
      "A layer's mask is a Mask, a (source, channel) tuple, or None.");
}

}  // namespace

void bindMaterialCore(py::module_& module) {
  auto materials = submodule(module, "material");
  py::enum_<material::ColorChannel>(materials, "ColorChannel")
      .value("Alpha", material::ColorChannel::Alpha)
      .value("Luminance", material::ColorChannel::Luminance)
      .value("Red", material::ColorChannel::Red)
      .value("Green", material::ColorChannel::Green)
      .value("Blue", material::ColorChannel::Blue);
  py::class_<material::Material> type(materials, "Material");
  py::implicitly_convertible<material::Color, material::Material>();
  py::class_<material::Mask>(materials, "Mask")
      .def(py::init([](const material::Material& source,
                       material::ColorChannel channel, float low, float high,
                       bool invert) {
             return material::Mask{source, channel, low, high, invert};
           }),
           py::arg("source"),
           py::arg("channel") = material::ColorChannel::Alpha,
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
             const py::object& mask) -> material::Material& {
            return self.layer(source, {blend, opacity, layerMask(mask)});
          },
          py::arg("source"), py::arg("blend") = material::BlendMode::Normal,
          py::arg("opacity") = 1.0f, py::arg("mask") = py::none(), fluent)
      .def(
          "surface",
          [](material::Material& self, material::Channel metallic,
             material::Channel roughness, material::Channel occlusion,
             std::optional<material::Material> normal, float normalScale,
             bool normalDirectX, const material::Color& emission,
             float emissionStrength,
             std::optional<material::Material> emissionMap, float alphaCutoff,
             material::Channel clearcoat, material::Channel clearcoatRoughness,
             std::optional<material::Material> clearcoatNormal,
             material::ColorInput sheenColor, material::Channel sheenRoughness,
             float anisotropy, float anisotropyRotation,
             std::optional<material::Material> anisotropyDirection,
             material::Channel iridescence, float iridescenceIor,
             float iridescenceThickness, float iridescenceThicknessMinimum,
             std::optional<material::Material> iridescenceThicknessMap,
             material::Channel specular, material::ColorInput specularColor,
             material::Channel transmission, float ior,
             material::Channel thickness, std::string thicknessAttribute,
             const material::Color& attenuationColor, float attenuationDistance,
             float dispersion, std::optional<material::Material> distortion,
             float distortionStrength, float reflectionWeight, bool unlit,
             material::BlendMode blend, std::optional<bool> depthWrite,
             float depthFade) -> material::Material& {
            return self.surface({std::move(metallic),
                                 std::move(roughness),
                                 std::move(occlusion),
                                 std::move(normal),
                                 normalScale,
                                 normalDirectX,
                                 std::nullopt,
                                 emission,
                                 emissionStrength,
                                 std::move(emissionMap),
                                 alphaCutoff,
                                 std::move(clearcoat),
                                 std::move(clearcoatRoughness),
                                 std::move(clearcoatNormal),
                                 std::move(sheenColor),
                                 std::move(sheenRoughness),
                                 anisotropy,
                                 anisotropyRotation,
                                 std::move(anisotropyDirection),
                                 false,
                                 std::move(iridescence),
                                 iridescenceIor,
                                 iridescenceThickness,
                                 iridescenceThicknessMinimum,
                                 std::move(iridescenceThicknessMap),
                                 std::move(specular),
                                 std::move(specularColor),
                                 std::move(transmission),
                                 ior,
                                 std::move(thickness),
                                 std::move(thicknessAttribute),
                                 attenuationColor,
                                 attenuationDistance,
                                 dispersion,
                                 std::move(distortion),
                                 distortionStrength,
                                 reflectionWeight,
                                 unlit,
                                 blend,
                                 depthWrite,
                                 depthFade});
          },
          py::kw_only(), py::arg_v("metallic", material::Channel(0.0f), "0.0"),
          py::arg_v("roughness", material::Channel(0.5f), "0.5"),
          py::arg_v("occlusion", material::Channel(1.0f), "1.0"),
          py::arg("normal") = py::none(), py::arg("normalScale") = 1.0f,
          py::arg("normalDirectX") = false,
          py::arg_v("emission", material::Color{0, 0, 0, 1},
                    "Color(0, 0, 0, 1)"),
          py::arg("emissionStrength") = 0.0f,
          py::arg("emissionMap") = py::none(), py::arg("alphaCutoff") = 0.0f,
          py::arg_v("clearcoat", material::Channel(0.0f), "0.0"),
          py::arg_v("clearcoatRoughness", material::Channel(0.0f), "0.0"),
          py::arg("clearcoatNormal") = py::none(),
          py::arg_v("sheenColor",
                    material::ColorInput(material::Color{0, 0, 0, 1}),
                    "Color(0, 0, 0, 1)"),
          py::arg_v("sheenRoughness", material::Channel(0.0f), "0.0"),
          py::arg("anisotropy") = 0.0f, py::arg("anisotropyRotation") = 0.0f,
          py::arg("anisotropyDirection") = py::none(),
          py::arg_v("iridescence", material::Channel(0.0f), "0.0"),
          py::arg("iridescenceIor") = 1.3f,
          py::arg("iridescenceThickness") = 400.0f,
          py::arg("iridescenceThicknessMinimum") = 100.0f,
          py::arg("iridescenceThicknessMap") = py::none(),
          py::arg_v("specular", material::Channel(1.0f), "1.0"),
          py::arg_v("specularColor",
                    material::ColorInput(material::Color{1, 1, 1, 1}),
                    "Color(1, 1, 1, 1)"),
          py::arg_v("transmission", material::Channel(0.0f), "0.0"),
          py::arg("ior") = 1.5f,
          py::arg_v("thickness", material::Channel(0.0f), "0.0"),
          py::arg("thicknessAttribute") = "",
          py::arg_v("attenuationColor", material::Color{1, 1, 1, 1},
                    "Color(1, 1, 1, 1)"),
          py::arg("attenuationDistance") = 0.0f, py::arg("dispersion") = 0.0f,
          py::arg("distortion") = py::none(),
          py::arg("distortionStrength") = 0.0f,
          py::arg("reflectionWeight") = 1.0f, py::arg("unlit") = false,
          py::arg("blend") = material::BlendMode::Normal,
          py::arg("depthWrite") = py::none(), py::arg("depthFade") = 0.0f,
          fluent)
      .def(
          "relief",
          [](material::Material& self, float shoulder, float depth)
              -> material::Material& { return self.relief({shoulder, depth}); },
          py::kw_only(),
          py::arg("shoulder") = material::ReliefOptions{}.shoulder,
          py::arg("depth") = material::ReliefOptions{}.depth, fluent)
      .def(
          "brushed",
          [](material::Material& self, float strength,
             float rotation) -> material::Material& {
            return self.brushed({strength, rotation});
          },
          py::kw_only(),
          py::arg("strength") = material::BrushedOptions{}.strength,
          py::arg("rotation") = material::BrushedOptions{}.rotation, fluent)
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
            // A bound number is a single value, so every sequence here is
            // a colour's three or four channels, as is a CSS string.
            if (py::isinstance<material::Color>(value) ||
                py::isinstance<py::str>(value) ||
                py::isinstance<py::tuple>(value) ||
                py::isinstance<py::list>(value) ||
                py::isinstance<motion::Animatable<material::Color>>(value) ||
                py::isinstance<motion::Tween<material::Color>>(value))
              return self.bind(name, motionInk(value));
            return self.bind(name, motionAnimatable(value));
          },
          py::arg("name"), py::arg("value"), fluent)
      .def("hasProgram", &material::Material::hasProgram)
      .def("isRunning", &material::Material::isRunning)
      .def("base", &material::Material::base)
      .def(py::self == py::self);
  materials.def("channel", &material::channel, py::arg("source"),
                py::arg("which"));
  auto valueRange = bindRecord<material::ValueRange>(
      materials, "ValueRange", "A value range has no field named ");
  valueRange
      .def(py::init([](float low, float high) {
             return material::ValueRange{low, high};
           }),
           py::arg("low"), py::arg("high"))
      .def_readwrite("low", &material::ValueRange::low)
      .def_readwrite("high", &material::ValueRange::high)
      .def(py::self == py::self);
  auto remapOptions = bindRecord<material::RemapOptions>(
      materials, "RemapOptions", "A remap has no option named ");
  remapOptions.def_readwrite("from_", &material::RemapOptions::from)
      .def_readwrite("to", &material::RemapOptions::to)
      .def_readwrite("channel", &material::RemapOptions::channel);
  // The record form first, so a held record is not asked to convert; the
  // keyword form names the two stretches as (low, high) pairs.
  materials.def("remap", &material::remap, py::arg("source"),
                py::arg("options") = material::RemapOptions{});
  materials.def(
      "remap",
      [](const material::Material& source, std::pair<float, float> from,
         std::pair<float, float> to, material::ColorChannel channel) {
        return material::remap(source, {.from = {from.first, from.second},
                                        .to = {to.first, to.second},
                                        .channel = channel});
      },
      py::arg("source"), py::arg("from_") = std::pair<float, float>{0, 1},
      py::arg("to") = std::pair<float, float>{0, 1},
      py::arg("channel") = material::ColorChannel::Luminance);
  materials.def("readsUnit", &material::readsUnit, py::arg("material"));
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
         float contrast, float stretch, bool cellular, float jitter) {
        return material::noise(frequency, {.octaves = octaves,
                                           .seed = seed,
                                           .turbulence = turbulence,
                                           .grain = grain,
                                           .contrast = contrast,
                                           .stretch = stretch,
                                           .cellular = cellular,
                                           .jitter = jitter});
      },
      py::arg("frequency"), py::arg("octaves") = 4, py::arg("seed") = 1.0f,
      py::arg("turbulence") = false, py::arg("grain") = false,
      py::arg("contrast") = 1.0f, py::arg("stretch") = 1.0f,
      py::arg("cellular") = false, py::arg("jitter") = 1.0f);
}

}  // namespace sigil::python
