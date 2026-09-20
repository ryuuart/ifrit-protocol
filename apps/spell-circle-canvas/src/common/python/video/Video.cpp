#include <sigilpython/Extend.h>
#include <sigilpython/video/Registration.h>

namespace sigil::python {

void bindVideo(pybind11::module_& module) {
  submodule(module, "video");
}

}  // namespace sigil::python
