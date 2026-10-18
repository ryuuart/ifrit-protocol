#include <pybind11/stl.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Instrument.h>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

void bindSketchKitContent(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();
  auto instrument = record<sketchKit::Instrument>(module, "Instrument");
  instrument.doc() =
      "A live picture beside its readings on one page: the picture's "
      "authored size and the width it is shown at, the two columns' "
      "headings, and a note.";
  field(instrument, "page", &sketchKit::Instrument::page);
  field(instrument, "pictureSize", &sketchKit::Instrument::pictureSize);
  field(instrument, "pictureWidth", &sketchKit::Instrument::pictureWidth);
  field(instrument, "pictureLabel", &sketchKit::Instrument::pictureLabel);
  field(instrument, "readingsLabel", &sketchKit::Instrument::readingsLabel);
  field(instrument, "note", &sketchKit::Instrument::note);
  module.def("instrument", &sketchKit::instrument, py::arg("specification"),
             py::arg("picture"), py::arg("readings"));
}

}  // namespace sigil::sketch::python
