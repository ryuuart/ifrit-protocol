#include <pybind11/stl.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/kit/Legend.h>
#include <sigilsketch/kit/Meter.h>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

void bindSketchKitLegends(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();

  auto entry = record<sketchKit::LegendEntry>(module, "LegendEntry");
  entry.doc() =
      "One entry of a key: a colour and what it stands for, with the line "
      "around its swatch, the ink of its label, a mark of its own in the "
      "swatch's place, and the beat it rides in on.";
  field(entry, "swatch", &sketchKit::LegendEntry::swatch);
  field(entry, "label", &sketchKit::LegendEntry::label);
  field(entry, "note", &sketchKit::LegendEntry::note);
  field(entry, "keyline", &sketchKit::LegendEntry::keyline);
  field(entry, "ink", &sketchKit::LegendEntry::ink);
  field(entry, "mark", &sketchKit::LegendEntry::mark);
  field(entry, "opacity", &sketchKit::LegendEntry::opacity);
  field(entry, "slide", &sketchKit::LegendEntry::slide);
  auto legend = record<sketchKit::Legend>(module, "Legend");
  legend.doc() =
      "The key to a picture: its entries, stacked or along a line, and the "
      "swatches' side, corners and outline.";
  field(legend, "entries", &sketchKit::Legend::entries);
  field(legend, "column", &sketchKit::Legend::column);
  field(legend, "swatchSide", &sketchKit::Legend::swatchSide);
  field(legend, "gap", &sketchKit::Legend::gap);
  field(legend, "corners", &sketchKit::Legend::corners);
  field(legend, "strokeWidth", &sketchKit::Legend::strokeWidth);
  field(legend, "keylineWidth", &sketchKit::Legend::keylineWidth);
  field(legend, "labelGap", &sketchKit::Legend::labelGap);
  field(legend, "wrap", &sketchKit::Legend::wrap);
  module.def("legend", &sketchKit::legend, py::arg("key"));

  auto strip = record<sketchKit::SwatchStrip>(module, "SwatchStrip");
  strip.doc() =
      "A ramp's steps in order, each a patch, with a word under each where "
      "labels are given.";
  field(strip, "swatches", &sketchKit::SwatchStrip::swatches);
  field(strip, "labels", &sketchKit::SwatchStrip::labels);
  field(strip, "inks", &sketchKit::SwatchStrip::inks);
  field(strip, "width", &sketchKit::SwatchStrip::width);
  field(strip, "height", &sketchKit::SwatchStrip::height);
  field(strip, "gap", &sketchKit::SwatchStrip::gap);
  field(strip, "corners", &sketchKit::SwatchStrip::corners);
  field(strip, "opacity", &sketchKit::SwatchStrip::opacity);
  module.def("swatchStrip", &sketchKit::swatchStrip, py::arg("strip"));

  auto chip = record<sketchKit::Chip>(module, "Chip");
  chip.doc() = "One word set inside a patch: a tag, a status, a tier.";
  field(chip, "label", &sketchKit::Chip::label);
  field(chip, "ground", &sketchKit::Chip::ground);
  field(chip, "ink", &sketchKit::Chip::ink);
  field(chip, "corners", &sketchKit::Chip::corners);
  module.def("chip", &sketchKit::chip, py::arg("tag"));

  auto meter = record<sketchKit::Meter>(module, "Meter");
  meter.doc() =
      "A level as a bar along a track, with its label and its reading; "
      "level, where given, is a live or described number the bar follows "
      "in place of fraction.";
  field(meter, "fraction", &sketchKit::Meter::fraction);
  field(meter, "level", &sketchKit::Meter::level);
  field(meter, "label", &sketchKit::Meter::label);
  field(meter, "reading", &sketchKit::Meter::reading);
  field(meter, "width", &sketchKit::Meter::width);
  field(meter, "height", &sketchKit::Meter::height);
  field(meter, "track", &sketchKit::Meter::track);
  field(meter, "bar", &sketchKit::Meter::bar);
  field(meter, "corners", &sketchKit::Meter::corners);
  field(meter, "keyline", &sketchKit::Meter::keyline);
  field(meter, "keylineWidth", &sketchKit::Meter::keylineWidth);
  field(meter, "inset", &sketchKit::Meter::inset);
  module.def("meter",
             py::overload_cast<const sketchKit::Meter&>(&sketchKit::meter),
             py::arg("bar"));

  auto gauge = record<sketchKit::Gauge>(module, "Gauge");
  gauge.doc() =
      "A level as an arc round a dial: its fraction, the dial's diameter "
      "and thickness, where the sweep starts and how far it runs, and the "
      "reading in its middle.";
  field(gauge, "fraction", &sketchKit::Gauge::fraction);
  field(gauge, "diameter", &sketchKit::Gauge::diameter);
  field(gauge, "thickness", &sketchKit::Gauge::thickness);
  field(gauge, "startDeg", &sketchKit::Gauge::startDeg);
  field(gauge, "sweepDeg", &sketchKit::Gauge::sweepDeg);
  field(gauge, "track", &sketchKit::Gauge::track);
  field(gauge, "bar", &sketchKit::Gauge::bar);
  field(gauge, "reading", &sketchKit::Gauge::reading);
  module.def("gauge", &sketchKit::gauge, py::arg("dial"));
}

}  // namespace sigil::sketch::python
