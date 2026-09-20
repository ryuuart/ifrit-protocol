#include <sigilpython/Extend.h>
#include <sigilpython/io/Registration.h>

namespace sigil::python {

void bindIOPublish(pybind11::module_& module) {
  submodule(module, "io.publish");
}

}  // namespace sigil::python
