#include <sigilcore/compute/Chance.h>
#include <sigilpython/Extend.h>
#include <sigilpython/core/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindCore(py::module_& module) {
  auto chance = submodule(module, "core.chance");
  namespace chanceNative = core::chance;
  py::enum_<chanceNative::Source>(chance, "Source")
      .value("Pcg", chanceNative::Source::Pcg)
      .value("Mix64", chanceNative::Source::Mix64)
      .value("Xorshift", chanceNative::Source::Xorshift)
      .value("Halton", chanceNative::Source::Halton)
      .value("Sobol", chanceNative::Source::Sobol)
      .value("Golden", chanceNative::Source::Golden)
      .value("Stratified", chanceNative::Source::Stratified);
  using Stream = chanceNative::Stream;
  py::class_<Stream>(chance, "Stream")
      .def(py::init<>())
      .def_static("pcg", &Stream::pcg, py::arg("seed"))
      .def_static("mix64", &Stream::mix64, py::arg("seed"))
      .def_static("xorshift", &Stream::xorshift, py::arg("seed"))
      .def_static("halton", &Stream::halton, py::arg("base"),
                  py::arg("skip") = 0)
      .def_static("sobol", &Stream::sobol, py::arg("skip") = 0)
      .def_static("golden", &Stream::golden, py::arg("seed") = 0)
      .def_static("stratified", &Stream::stratified, py::arg("strata"),
                  py::arg("seed") = 0)
      .def_static("of", &Stream::of, py::arg("source"), py::arg("seed"),
                  py::arg("parameter") = 0)
      .def("copy", [](const Stream& self) { return self; })
      .def("__copy__", [](const Stream& self) { return self; })
      .def("bits", &Stream::bits)
      .def("unit", &Stream::unit)
      .def("signedUnit", &Stream::signedUnit)
      .def("range", &Stream::range, py::arg("low"), py::arg("high"))
      .def("below", &Stream::below, py::arg("upper"))
      .def("normal", &Stream::normal)
      .def("source", &Stream::source)
      .def("parameter", &Stream::parameter)
      .def("drawn", &Stream::drawn);
}

void bindCoreValues(py::module_&) {}

}  // namespace sigil::python
