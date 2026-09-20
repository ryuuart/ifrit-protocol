#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeKitStrokes(pybind11::module_& module) {
  submodule(module, "compose.lines.presets");
  submodule(module, "compose.brush.presets");
}

}  // namespace sigil::python
