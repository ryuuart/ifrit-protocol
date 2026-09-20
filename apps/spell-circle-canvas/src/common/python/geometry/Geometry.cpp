#include <sigilgeometry/path/Arrange.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindGeometry(py::module_& root) {
  auto arrange = submodule(root, "geometry.arrange");
  py::enum_<geometry::arrange::Turn>(arrange, "Turn")
      .value("Open", geometry::arrange::Turn::Open)
      .value("Closed", geometry::arrange::Turn::Closed);
  arrange
      .def("step", &geometry::arrange::step, py::arg("extent"),
           py::arg("count"), py::arg("turn"))
      .def("along", &geometry::arrange::along, py::arg("start"),
           py::arg("extent"), py::arg("index"), py::arg("count"),
           py::arg("turn"))
      .def("onEllipse", &geometry::arrange::onEllipse, py::arg("center"),
           py::arg("radii"), py::arg("radians"))
      .def("onRing", &geometry::arrange::onRing, py::arg("index"),
           py::arg("count"), py::arg("center"), py::arg("radii"),
           py::arg("startRadians"), py::arg("sweepRadians"), py::arg("turn"));
  py::class_<geometry::arrange::Cell>(arrange, "Cell")
      .def(py::init([](int column, int row) {
             return geometry::arrange::Cell{column, row};
           }),
           py::arg("column") = 0, py::arg("row") = 0)
      .def_readwrite("column", &geometry::arrange::Cell::column)
      .def_readwrite("row", &geometry::arrange::Cell::row);
  arrange
      .def("cellAt", &geometry::arrange::cellAt, py::arg("index"),
           py::arg("columns"))
      .def("moduleSize", &geometry::arrange::moduleSize, py::arg("container"),
           py::arg("columns"), py::arg("rows"), py::arg("gap"));
  arrange.def("cellRect", &geometry::arrange::cellRect, py::arg("cell"),
              py::arg("module"), py::arg("gap") = SkSize{0, 0},
              py::arg("origin") = SkPoint{0, 0}, py::arg("columnSpan") = 1,
              py::arg("rowSpan") = 1);
}
}  // namespace sigil::python
