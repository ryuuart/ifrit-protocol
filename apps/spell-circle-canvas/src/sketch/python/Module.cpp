#include <pybind11/pybind11.h>
#include <sigilpython/PublicNames.h>
#include <sigilpython/Python.h>

#include "host/Registration.h"
#include "kit/Registration.h"

// One registration body serves the executable's built-in module and the
// extension imported by an ordinary Python process.
PYBIND11_MODULE(_sigil, module) {
  sigil::python::bindLibraries(module);
  sigil::sketch::python::bindRuntime(module);
  sigil::sketch::python::bindSketchContextSurface(module);
  sigil::sketch::python::bindSketchEntries(module);
  sigil::sketch::python::bindSketchSchema(module);
  sigil::sketch::python::bindSketchDeviceRuntimes(module);
  sigil::sketch::python::bindSketchSetSketches(module);
  sigil::sketch::python::bindSketchWorldSet(module);
  sigil::sketch::python::bindSketchKit(module);
  sigil::sketch::python::bindSketchKitContent(module);
  sigil::sketch::python::bindSketchKitRows(module);
  sigil::sketch::python::bindSketchKitLegends(module);
  sigil::sketch::python::bindSketchKitCharts(module);
  sigil::sketch::python::bindSketchKitPanels(module);
  sigil::sketch::python::bindSketchKitStreams(module);
  // Last, so every class, enumeration and free function this process
  // registers is named for the package an author imports it from rather than
  // for this module.
  sigil::python::namePublicly(module);
}
