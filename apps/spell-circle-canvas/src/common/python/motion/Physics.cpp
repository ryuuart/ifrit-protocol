#include <sigilpython/Extend.h>
#include <sigilpython/motion/Registration.h>

namespace sigil::python {

void bindMotionPhysics(pybind11::module_& module) {
  submodule(module, "motion.physics");
}

}  // namespace sigil::python
