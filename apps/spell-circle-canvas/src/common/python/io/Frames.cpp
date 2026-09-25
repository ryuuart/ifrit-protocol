#include <sigilpython/Extend.h>
#include <sigilpython/io/Registration.h>

namespace sigil::python {

void bindIOFrames(pybind11::module_& module) {
  submodule(module, "io.frames");
}

}  // namespace sigil::python
