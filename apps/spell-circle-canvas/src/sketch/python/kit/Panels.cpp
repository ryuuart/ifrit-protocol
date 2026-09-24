#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Heading.h>

#include "Registration.h"

namespace sigil::sketch::python {

void bindSketchKitPanels(pybind11::module_& root) {
  namespace py = pybind11;
  namespace kit = sigil::sketch::kit;
  using sigil::python::kit::field;
  using sigil::python::kit::record;
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  auto header = record<kit::SectionHeader>(module, "SectionHeader");
  field(header, "label", &kit::SectionHeader::label);
  field(header, "note", &kit::SectionHeader::note);
  field(header, "ruled", &kit::SectionHeader::ruled);
  module.def("sectionHeader", &kit::sectionHeader, py::arg("header"));
  module.def("section", &kit::section, py::arg("header"), py::arg("content"));
}

}  // namespace sigil::sketch::python
