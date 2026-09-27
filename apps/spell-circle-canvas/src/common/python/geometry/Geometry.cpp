#include <pybind11/operators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindGeometry(py::module_& root) {
  namespace arrange = geometry::arrange;
  auto module = submodule(root, "geometry.arrange");
  py::enum_<arrange::Turn>(module, "Turn")
      .value("Open", arrange::Turn::Open)
      .value("Closed", arrange::Turn::Closed);
  module
      .def("step", &arrange::step, py::arg("extent"), py::arg("count"),
           py::arg("turn"))
      .def("along", &arrange::along, py::arg("start"), py::arg("extent"),
           py::arg("index"), py::arg("count"), py::arg("turn"))
      .def("direction", &arrange::direction, py::arg("radians"))
      .def("heading", &arrange::heading, py::arg("vector"))
      .def("onEllipse", &arrange::onEllipse, py::arg("center"),
           py::arg("radii"), py::arg("radians"));

  auto ring = bindRecord<arrange::Ring>(module, "Ring", "Unknown ring field: ");
  ring.def_readwrite("center", &arrange::Ring::center)
      .def_readwrite("radii", &arrange::Ring::radii)
      .def_readwrite("fromDegrees", &arrange::Ring::fromDegrees)
      .def_readwrite("sweepDegrees", &arrange::Ring::sweepDegrees)
      .def_readwrite("turn", &arrange::Ring::turn)
      .def(py::self == py::self);
  auto placement = bindRecord<arrange::Placement>(
      module, "Placement", "Unknown placement field: ");
  placement.def_readwrite("position", &arrange::Placement::position)
      .def_readwrite("headingDegrees", &arrange::Placement::headingDegrees)
      .def(py::self == py::self);
  module
      .def("radiansOnRing", &arrange::radiansOnRing, py::arg("index"),
           py::arg("count"), py::arg("ring") = arrange::Ring{})
      .def("radiansAt", &arrange::radiansAt, py::arg("fraction"),
           py::arg("ring") = arrange::Ring{})
      .def("onRing", &arrange::onRing, py::arg("index"), py::arg("count"),
           py::arg("ring") = arrange::Ring{})
      .def("placeOnEllipse", &arrange::placeOnEllipse, py::arg("center"),
           py::arg("radii"), py::arg("radians"))
      .def("placeOnRing", &arrange::placeOnRing, py::arg("index"),
           py::arg("count"), py::arg("ring") = arrange::Ring{})
      .def("placeAlong", &arrange::placeAlong, py::arg("position"),
           py::arg("tangent"));

  py::class_<arrange::Cell>(module, "Cell")
      .def(py::init([](int column, int row) {
             return arrange::Cell{column, row};
           }),
           py::arg("column") = 0, py::arg("row") = 0)
      .def_readwrite("column", &arrange::Cell::column)
      .def_readwrite("row", &arrange::Cell::row)
      .def(py::self == py::self);
  auto block = bindRecord<arrange::CellBlock>(module, "CellBlock",
                                              "Unknown cell block field: ");
  block.def_readwrite("gap", &arrange::CellBlock::gap)
      .def_readwrite("origin", &arrange::CellBlock::origin)
      .def_readwrite("columnSpan", &arrange::CellBlock::columnSpan)
      .def_readwrite("rowSpan", &arrange::CellBlock::rowSpan)
      .def(py::self == py::self);
  module
      .def("cellAt", &arrange::cellAt, py::arg("index"), py::arg("columns"))
      .def("moduleSize", &arrange::moduleSize, py::arg("container"),
           py::arg("columns"), py::arg("rows"),
           py::arg("gap") = glm::vec2{0, 0})
      .def("cellRect", &arrange::cellRect, py::arg("cell"), py::arg("module"),
           py::arg("block") = arrange::CellBlock{});
}
}  // namespace sigil::python
