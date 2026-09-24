/** @file
 * Two directories of plates compared, as the values a script judges.
 */

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilsketch/plate/Compare.h>

#include <filesystem>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

void bindSketchPlates(py::module_& module) {
  auto sketches = module.def_submodule("sketch");
  py::enum_<PlateOutcome>(sketches, "PlateOutcome")
      .value("Compared", PlateOutcome::Compared)
      .value("Missing", PlateOutcome::Missing)
      .value("Unreadable", PlateOutcome::Unreadable)
      .value("Resized", PlateOutcome::Resized);
  py::enum_<PlateSide>(sketches, "PlateSide")
      .value("First", PlateSide::First)
      .value("Second", PlateSide::Second);
  py::class_<PlateComparison>(sketches, "PlateComparison")
      .def_readonly("name", &PlateComparison::name)
      .def_readonly("outcome", &PlateComparison::outcome)
      .def_readonly("side", &PlateComparison::side)
      .def_readonly("firstWidth", &PlateComparison::firstWidth)
      .def_readonly("firstHeight", &PlateComparison::firstHeight)
      .def_readonly("secondWidth", &PlateComparison::secondWidth)
      .def_readonly("secondHeight", &PlateComparison::secondHeight)
      .def_readonly("mean", &PlateComparison::mean)
      .def_readonly("p99", &PlateComparison::p99)
      .def_readonly("worst", &PlateComparison::worst)
      .def_readonly("worstOverClear", &PlateComparison::worstOverClear)
      .def_readonly("worstOverContent", &PlateComparison::worstOverContent)
      .def_readonly("worstOverGraze", &PlateComparison::worstOverGraze)
      .def_readonly("grazingPixels", &PlateComparison::grazingPixels)
      .def_readonly("worstPerComposite", &PlateComparison::worstPerComposite)
      .def_readonly("stackedPixels", &PlateComparison::stackedPixels)
      .def("__str__", &comparisonLine);
  py::class_<Comparison>(sketches, "Comparison")
      .def_readonly("plates", &Comparison::plates)
      .def_readonly("refusal", &Comparison::refusal)
      .def("status", &Comparison::status);
  // Decoding and differencing two directories touches no Python object,
  // so the interpreter is free for the whole of it.
  sketches.def(
      "compare",
      [](const std::filesystem::path& first,
         const std::filesystem::path& second) {
        return compare({first.string(), second.string()});
      },
      py::arg("first"), py::arg("second"),
      py::call_guard<py::gil_scoped_release>());
}

}  // namespace sigil::sketch::python
