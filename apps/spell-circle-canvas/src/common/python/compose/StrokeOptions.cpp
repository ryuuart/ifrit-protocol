#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Band.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Image.h>
#include <sigilcompose/core/Text.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {
namespace py = pybind11;

namespace {

constexpr auto fluent = py::return_value_policy::reference_internal;

/** The material keyline in its options form, on whichever kind of node
 *  wears one: the width, the side, the cap, the join and the miter limit
 *  in one record, beside the width-and-side form that states the rest at
 *  their defaults. A colour in any spelling Python writes one in is the
 *  flat material of that colour. */
template <class Node>
void bindOptionsStroke(py::module_& module, const char* path) {
  extend<Node>(module, path)
      .def(
          "stroke",
          [](Node& self, const material::Material& material,
             const compose::StrokeOptions& options) -> Node& {
            return self.stroke(material, options);
          },
          py::arg("material"), py::arg("options"), fluent)
      .def(
          "stroke",
          [](Node& self, const material::Color& color,
             const compose::StrokeOptions& options) -> Node& {
            return self.stroke(color, options);
          },
          py::arg("color"), py::arg("options"), fluent);
}

}  // namespace

void bindComposeStrokeOptions(py::module_& module) {
  py::module_ composition = submodule(module, "compose");
  auto options = bindRecord<compose::StrokeOptions>(
      composition, "StrokeOptions", "Unknown StrokeOptions field: ");
  options.doc() =
      "How a material keyline is widened from the outline it dresses: "
      "width in local pixels (zero or less draws nothing), the side of a "
      "closed contour it stands on, the cap an open contour ends with, the "
      "join its corners meet with, and how long a mitred corner may grow, "
      "in widths, before it is bevelled. The relief and the brushing it "
      "wears are its material's.";
  options.def_readwrite("width", &compose::StrokeOptions::width)
      .def_readwrite("position", &compose::StrokeOptions::position)
      .def_readwrite("cap", &compose::StrokeOptions::cap)
      .def_readwrite("join", &compose::StrokeOptions::join)
      .def_readwrite("miterLimit", &compose::StrokeOptions::miterLimit)
      .def(py::self == py::self);
  bindOptionsStroke<compose::Element>(module, "compose.Element");
  bindOptionsStroke<compose::Text>(module, "compose.Text");
  bindOptionsStroke<compose::Image>(module, "compose.Image");
  bindOptionsStroke<compose::Band>(module, "compose.Band");
}

}  // namespace sigil::python
