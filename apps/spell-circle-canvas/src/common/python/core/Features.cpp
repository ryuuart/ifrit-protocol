#include <sigilpython/Extend.h>
#include <sigilpython/core/Registration.h>

namespace sigil::python {

namespace py = pybind11;

namespace {

// The build's own answer about the libraries behind a licensed SDK. A
// build that did not find one leaves out its sources, its registration
// and the definition read here together, so a name is in this answer
// exactly when the module it names was registered. The names are the
// submodules those libraries register under the extension root, which
// are also the names the package surfaces them under.
py::tuple optionalLibraries() {
  py::list carried;
#ifdef SIGIL_PYTHON_HAS_SCRY
  carried.append(py::str("scry"));
#endif
#ifdef SIGIL_PYTHON_HAS_SUBSTANCE
  carried.append(py::str("substance"));
#endif
#ifdef SIGIL_PYTHON_HAS_USD
  carried.append(py::str("usd"));
#endif
  return py::tuple(carried);
}

}  // namespace

void bindOptionalLibraries(py::module_& module) {
  submodule(module, "core")
      .def("optionalLibraries", &optionalLibraries,
           "The optional native libraries this build carries, in "
           "alphabetical order. A library whose SDK the build did not find "
           "registers nothing, and its name is absent here; `sigil.<name>` "
           "is the module each name surfaces in, so a process learns what "
           "it may import without importing it to find out.");
}

}  // namespace sigil::python
