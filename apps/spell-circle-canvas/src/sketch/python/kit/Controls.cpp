#include <pybind11/stl.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Controls.h>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

void bindSketchKitControls(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  py::enum_<sketchKit::Widget>(module, "Widget")
      .value("Slider", sketchKit::Widget::Slider)
      .value("Toggle", sketchKit::Widget::Toggle)
      .value("Choice", sketchKit::Widget::Choice);
  auto option = record<sketchKit::Option>(module, "Option");
  field(option, "value", &sketchKit::Option::value);
  field(option, "label", &sketchKit::Option::label);
  auto control = record<sketchKit::Control>(module, "Control");
  field(control, "name", &sketchKit::Control::name);
  field(control, "label", &sketchKit::Control::label);
  field(control, "group", &sketchKit::Control::group);
  field(control, "widget", &sketchKit::Control::widget);
  field(control, "minimum", &sketchKit::Control::minimum);
  field(control, "maximum", &sketchKit::Control::maximum);
  field(control, "step", &sketchKit::Control::step);
  field(control, "value", &sketchKit::Control::value);
  field(control, "options", &sketchKit::Control::options);
  auto view = record<sketchKit::ControlsView>(module, "ControlsView");
  field(view, "groups", &sketchKit::ControlsView::groups);
  field(view, "width", &sketchKit::ControlsView::width);
  field(view, "labelWidth", &sketchKit::ControlsView::labelWidth);
  field(view, "figureWidth", &sketchKit::ControlsView::figureWidth);
  field(view, "rowHeight", &sketchKit::ControlsView::rowHeight);

  module.def("controlsOf",
             py::overload_cast<const material::sbsar::Description&>(
                 &sketchKit::controlsOf),
             py::arg("description"));
  module.def(
      "controlsOf",
      py::overload_cast<const material::Material&>(&sketchKit::controlsOf),
      py::arg("material"));
  py::class_<sketchKit::Controls>(
      module, "Controls",
      "The live values of a control surface; copies share them.")
      .def(py::init<std::vector<sketchKit::Control>>(), py::arg("parameters"))
      .def(py::init<const material::sbsar::Description&>(),
           py::arg("description"))
      .def(py::init<const material::Material&>(), py::arg("material"))
      .def("range", &sketchKit::Controls::range, py::arg("name"),
           py::arg("minimum"), py::arg("maximum"), py::arg("step") = 0.0f,
           py::return_value_policy::reference_internal)
      .def("value", &sketchKit::Controls::value, py::arg("name"))
      .def("set", &sketchKit::Controls::set, py::arg("name"), py::arg("value"))
      .def("bind", &sketchKit::Controls::bind, py::arg("material"))
      .def("parameters", &sketchKit::Controls::parameters);
  module.def("controls", &sketchKit::controls, py::arg("surface"),
             py::arg("how") = sketchKit::ControlsView{});
}

}  // namespace sigil::sketch::python
