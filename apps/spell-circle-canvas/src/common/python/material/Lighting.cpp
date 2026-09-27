/** @file
 * The light a lit surface is shaded under, in Python: `material.studio()`
 * for a directional light, `material.environment()` for the picture
 * around the surface, and `material.Lighting` holding either or both —
 * what `surface(lighting=)` and a node's `lighting()` take. Every angle
 * and strength takes a number or an animatable.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/core/Image.h>
#include <sigilpython/Extend.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>

#include <memory>
#include <optional>
#include <utility>

namespace sigil::python {

namespace py = pybind11;

void bindMaterialLighting(py::module_& module) {
  auto materials = submodule(module, "material");
  py::enum_<material::LightKind>(materials, "LightKind")
      .value("Directional", material::LightKind::Directional)
      .value("Point", material::LightKind::Point)
      .value("Spot", material::LightKind::Spot);
  py::class_<material::Light>(materials, "Light")
      .def_property_readonly(
          "direction",
          [](const material::Light& light) { return light.direction.value(); })
      .def_property_readonly(
          "elevation",
          [](const material::Light& light) { return light.elevation.value(); })
      .def_readonly("color", &material::Light::color)
      .def_property_readonly(
          "intensity",
          [](const material::Light& light) { return light.intensity.value(); })
      .def_readonly("ambient", &material::Light::ambient)
      .def_readonly("kind", &material::Light::kind)
      .def_readonly("position", &material::Light::position)
      .def_readonly("range", &material::Light::range)
      .def_readonly("innerAngle", &material::Light::innerAngle)
      .def_readonly("outerAngle", &material::Light::outerAngle)
      .def("isRunning", &material::Light::isRunning)
      .def(py::self == py::self);
  materials.def(
      "studio",
      [](py::object direction, py::object elevation, py::handle color,
         py::object intensity, float ambient) {
        return material::studio({.direction = motionAnimatable(direction),
                                 .elevation = motionAnimatable(elevation),
                                 .color = materialColor(color),
                                 .intensity = motionAnimatable(intensity),
                                 .ambient = ambient});
      },
      py::kw_only(), py::arg("direction") = 120.0f,
      py::arg("elevation") = 45.0f,
      py::arg_v("color", material::Color{1, 1, 1, 1}, "Color(1, 1, 1, 1)"),
      py::arg("intensity") = 1.0f, py::arg("ambient") = 0.3f,
      "A directional light: where it comes from on the page (degrees "
      "counter-clockwise from three o'clock), how high above it, its colour "
      "and strength, and the ambient share every point receives.");
  py::class_<material::Environment>(materials, "Environment")
      .def_property_readonly("rotation",
                             [](const material::Environment& around) {
                               return around.options.rotation.value();
                             })
      .def_property_readonly("intensity",
                             [](const material::Environment& around) {
                               return around.options.intensity;
                             })
      .def("isRunning", &material::Environment::isRunning)
      .def(py::self == py::self);
  const auto options = [](py::object rotation, float intensity,
                          std::optional<std::pair<float, float>> size) {
    material::EnvironmentOptions made{.rotation = motionAnimatable(rotation),
                                      .intensity = intensity};
    if (size) made.size = {size->first, size->second};
    return made;
  };
  materials.def(
      "environment",
      [options](std::shared_ptr<media::Image> image, py::object rotation,
                float intensity, std::optional<std::pair<float, float>> size) {
        return material::environment(
            media::PixelSource(std::shared_ptr<const media::Image>(image)),
            options(rotation, intensity, size));
      },
      py::arg("image"), py::kw_only(), py::arg("rotation") = 0.0f,
      py::arg("intensity") = 1.0f, py::arg("size") = py::none(),
      "A latitude-longitude picture as the environment a lit surface "
      "reflects and takes its ambient colour from.");
  materials.def(
      "environment",
      [options](const material::Material& image, py::object rotation,
                float intensity, std::optional<std::pair<float, float>> size) {
        return material::environment(image, options(rotation, intensity, size));
      },
      py::arg("image"), py::kw_only(), py::arg("rotation") = 0.0f,
      py::arg("intensity") = 1.0f, py::arg("size") = py::none(),
      "Any material, read across size, as the environment.");
  py::class_<material::Lighting>(materials, "Lighting")
      .def(py::init([](std::optional<material::Light> light,
                       std::optional<material::Environment> environment) {
             material::Lighting lighting;
             lighting.light = std::move(light);
             lighting.environment = std::move(environment);
             return lighting;
           }),
           py::arg("light") = py::none(), py::arg("environment") = py::none())
      .def_readonly("light", &material::Lighting::light)
      .def_readonly("environment", &material::Lighting::environment)
      .def("isRunning", &material::Lighting::isRunning)
      .def("__bool__",
           [](const material::Lighting& lighting) { return (bool)lighting; })
      .def(py::self == py::self);
  py::implicitly_convertible<material::Light, material::Lighting>();
  py::implicitly_convertible<material::Environment, material::Lighting>();
}

}  // namespace sigil::python
