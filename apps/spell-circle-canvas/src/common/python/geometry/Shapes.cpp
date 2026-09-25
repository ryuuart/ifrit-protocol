#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilgeometry/kit/Hatches.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryShapes(pybind11::module_& module) {
  namespace py = pybind11;
  py::module_ shapes = submodule(module, "geometry.shapes");

  // The one hatch value: where a natural-media brush lays its marks and
  // what a decoration strokes. Its angle is radians.
  auto hatch = bindRecord<geometry::shapes::Hatch>(shapes, "Hatch",
                                                   "Unknown hatch property: ");
  hatch.def_readwrite("spacing", &geometry::shapes::Hatch::spacing)
      .def_readwrite("angle", &geometry::shapes::Hatch::angle)
      .def_readwrite("taper", &geometry::shapes::Hatch::taper)
      .def_readwrite("origin", &geometry::shapes::Hatch::origin)
      .def_readwrite("inset", &geometry::shapes::Hatch::inset)
      .def_readwrite("cross", &geometry::shapes::Hatch::cross)
      .def_readwrite("maxLines", &geometry::shapes::Hatch::maxLines)
      .def(py::self == py::self);
}

}  // namespace sigil::python
