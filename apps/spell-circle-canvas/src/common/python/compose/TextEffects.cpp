#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeTextEffects(pybind11::module_& module) {
  submodule(module, "compose.fx");
  submodule(module, "core.noise");
}

}  // namespace sigil::python
