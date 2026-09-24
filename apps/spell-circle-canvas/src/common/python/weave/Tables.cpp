#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>
#include <sigilweave/kit/LineTables.h>

namespace sigil::python {

void bindWeaveTables(pybind11::module_& module) {
  namespace kit = sigil::weave::kit;
  auto kinsoku = submodule(module, "weave.kit.kinsoku");
  kinsoku.def("japanese", &kit::kinsoku::japanese);
  auto hanging = submodule(module, "weave.kit.hanging");
  hanging.def("latin", &kit::hanging::latin);
  hanging.def("japanese", &kit::hanging::japanese);
}

}  // namespace sigil::python
