/** @file
 * One emitter, in Python: `material.Light` and `material.LightKind`, the
 * emitter a set renderer shades under. Every angle and strength takes a
 * number or an animatable; colour takes a constant, a colour animatable
 * or a colour tween.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/material/Convert.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>

namespace sigil::python {

namespace py = pybind11;

void bindMaterialLighting(py::module_& module) {
  auto materials = submodule(module, "material");
  py::enum_<material::LightKind>(materials, "LightKind")
      .value("Directional", material::LightKind::Directional)
      .value("Point", material::LightKind::Point)
      .value("Spot", material::LightKind::Spot);
  py::class_<material::Light> light(materials, "Light");
  light
      .def(py::init([](py::object direction, py::object elevation,
                       py::handle color, py::object intensity, float ambient,
                       material::LightKind kind, glm::vec3 position,
                       float range, float innerAngle, float outerAngle) {
             return material::Light{.direction = motionAnimatable(direction),
                                    .elevation = motionAnimatable(elevation),
                                    .color = motionInk(color),
                                    .intensity = motionAnimatable(intensity),
                                    .ambient = ambient,
                                    .kind = kind,
                                    .position = position,
                                    .range = range,
                                    .innerAngle = innerAngle,
                                    .outerAngle = outerAngle};
           }),
           py::kw_only(), py::arg("direction") = 120.0f,
           py::arg("elevation") = 45.0f,
           py::arg_v("color", material::Color{1, 1, 1, 1}, "Color(1, 1, 1, 1)"),
           py::arg("intensity") = 1.0f, py::arg("ambient") = 0.3f,
           py::arg("kind") = material::LightKind::Directional,
           py::arg_v("position", glm::vec3(0), "(0, 0, 0)"),
           py::arg("range") = 600.0f, py::arg("innerAngle") = 0.0f,
           py::arg("outerAngle") = 45.0f)
      .def_property_readonly(
          "direction",
          [](const material::Light& light) { return light.direction.value(); })
      .def_property_readonly(
          "elevation",
          [](const material::Light& light) { return light.elevation.value(); })
      .def_property_readonly(
          "color",
          [](const material::Light& light) { return light.color.value(); })
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
  copyProtocol(light);
}

}  // namespace sigil::python
