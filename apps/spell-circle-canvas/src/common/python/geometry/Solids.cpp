/** @file
 * The solid values: each a comparable description of a stock solid — its
 * sizes, its divisions, its options — that forms its mesh on asking, so a
 * scene that names one keeps one formed mesh while the value stands
 * unchanged.
 */

#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

namespace {

namespace py = pybind11;
namespace solids = geometry::solids;

/** @p name bound as a keyword-built record that compares by value and
 *  forms its mesh through `mesh()`. */
template <class Solid>
py::class_<Solid> solidValue(py::module_& module, const char* name,
                             const char* unknownField, const char* summary) {
  auto value = bindRecord<Solid>(module, name, unknownField);
  value.def(py::self == py::self)
      .def("mesh", &Solid::mesh,
           "The mesh this value describes, formed now: what the lowercase "
           "former of the same solid answers for the same arguments.");
  value.doc() = summary;
  return value;
}

}  // namespace

void bindGeometrySolids(py::module_& module) {
  py::module_ values = submodule(module, "geometry.solids");

  solidValue<solids::Quad>(values, "Quad", "A quad has no field named ",
                           "A flat width by height card facing +z.")
      .def_readwrite("width", &solids::Quad::width)
      .def_readwrite("height", &solids::Quad::height);
  solidValue<solids::Box>(values, "Box", "A box has no field named ",
                          "The axis-aligned box from low to high.")
      .def_readwrite("low", &solids::Box::low)
      .def_readwrite("high", &solids::Box::high)
      .def_readwrite("options", &solids::Box::options);
  solidValue<solids::RegularSolid>(values, "RegularSolid",
                                   "A regular solid has no field named ",
                                   "One of the five regular solids.")
      .def_readwrite("solid", &solids::RegularSolid::solid)
      .def_readwrite("options", &solids::RegularSolid::options);
  solidValue<solids::Torus>(
      values, "Torus", "A torus has no field named ",
      "A ring around +y: majorRadius from the axis to the tube's centre, "
      "tubeRadius across the tube, ringSteps around the axis and tubeSteps "
      "around the tube.")
      .def_readwrite("majorRadius", &solids::Torus::majorRadius)
      .def_readwrite("tubeRadius", &solids::Torus::tubeRadius)
      .def_readwrite("ringSteps", &solids::Torus::ringSteps)
      .def_readwrite("tubeSteps", &solids::Torus::tubeSteps);
  solidValue<solids::Superellipsoid>(
      values, "Superellipsoid", "A superellipsoid has no field named ",
      "A superellipsoid of radii: exponent 2 is an ellipsoid and higher a "
      "rounded box.")
      .def_readwrite("radii", &solids::Superellipsoid::radii)
      .def_readwrite("exponent", &solids::Superellipsoid::exponent)
      .def_readwrite("longitudeSteps", &solids::Superellipsoid::longitudeSteps)
      .def_readwrite("latitudeSteps", &solids::Superellipsoid::latitudeSteps);
  solidValue<solids::CylinderPanel>(
      values, "CylinderPanel", "A cylinder panel has no field named ",
      "A width by height panel curved round a vertical cylinder of radius.")
      .def_readwrite("width", &solids::CylinderPanel::width)
      .def_readwrite("height", &solids::CylinderPanel::height)
      .def_readwrite("radius", &solids::CylinderPanel::radius)
      .def_readwrite("widthSteps", &solids::CylinderPanel::widthSteps)
      .def_readwrite("heightSteps", &solids::CylinderPanel::heightSteps);
  solidValue<solids::Extrusion>(values, "Extrusion",
                                "An extrusion has no field named ",
                                "A filled outline lifted into a solid.")
      .def_readwrite("outline", &solids::Extrusion::outline)
      .def_readwrite("options", &solids::Extrusion::options);
  solidValue<solids::Revolution>(
      values, "Revolution", "A revolution has no field named ",
      "A profile of (radius, height) points lathed around +y.")
      .def_readwrite("profile", &solids::Revolution::profile)
      .def_readwrite("options", &solids::Revolution::options);
  solidValue<solids::Loft>(values, "Loft", "A loft has no field named ",
                           "A skin through closed rings of points.")
      .def_readwrite("sections", &solids::Loft::sections)
      .def_readwrite("options", &solids::Loft::options);
}

}  // namespace sigil::python
