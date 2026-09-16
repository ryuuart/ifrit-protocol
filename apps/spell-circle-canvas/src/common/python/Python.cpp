#include <sigilpython/BrushBindings.h>
#include <sigilpython/ComposeBindings.h>
#include <sigilpython/DataBindings.h>
#include <sigilpython/IOBindings.h>
#include <sigilpython/KitBindings.h>
#include <sigilpython/MotionBindings.h>
#include <sigilpython/PenBindings.h>
#include <sigilpython/Python.h>
#include <sigilpython/ValueBindings.h>

namespace sigil::python {
void bindLibraries(pybind11::module_& module) {
  bindCore(module);
  bindValues(module);
  bindMotion(module);
  bindCompose(module);
  bindPen(module);
  bindBrush(module);
  bindGeometry(module);
  bindData(module);
  bindIO(module);
  bindComposeKit(module);
}
}  // namespace sigil::python
