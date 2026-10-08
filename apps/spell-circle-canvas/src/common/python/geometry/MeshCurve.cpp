/** @file
 * `sigil.geometry.mesh.curve`: the 3D spline — control points, the rule
 * that reads them and whether it closes — with the positions, tangents
 * and lengths it answers. A spline is what a point chain rides when it
 * enters on an open trace rather than a closed loop.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

#include <utility>
#include <vector>

namespace sigil::python {
namespace py = pybind11;
namespace curve = geometry::mesh::curve;

void bindGeometryMeshCurve(py::module_& module) {
  auto curves = submodule(module, "geometry.mesh.curve");
  auto spline = bindRecord<curve::Spline3>(curves, "Spline3",
                                           "A spline has no field named ");
  py::enum_<curve::Spline3::Type>(spline, "Type")
      .value("Linear", curve::Spline3::Type::Linear)
      .value("CatmullRom", curve::Spline3::Type::CatmullRom)
      .value("Bezier", curve::Spline3::Type::Bezier);
  // The positional form states the three fields in their order, so a
  // trace is one call: `Spline3(points)` is open and passes through them.
  spline
      .def(py::init([](std::vector<glm::vec3> points, curve::Spline3::Type type,
                       bool closed) {
             return curve::Spline3{std::move(points), type, closed};
           }),
           py::arg("points"),
           py::arg("type") = curve::Spline3::Type::CatmullRom,
           py::arg("closed") = false)
      .def_readwrite("points", &curve::Spline3::points)
      .def_readwrite("type", &curve::Spline3::type)
      .def_readwrite("closed", &curve::Spline3::closed)
      .def("position", &curve::Spline3::position, py::arg("t"))
      .def("tangent", &curve::Spline3::tangent, py::arg("t"))
      .def("length", &curve::Spline3::length, py::arg("samples") = 256)
      .def("sample", &curve::Spline3::sample, py::arg("count"))
      .def("sampleArcLength", &curve::Spline3::sampleArcLength,
           py::arg("count"))
      .def(py::self == py::self);
}

}  // namespace sigil::python
