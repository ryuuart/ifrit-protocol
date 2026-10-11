/** @file
 * What a compose page painted as the maps of a surface states about its
 * maps: `SurfaceRange`, the strength, depth and displacement each map's
 * white stands for.
 */

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {
namespace py = pybind11;

void bindComposeTextureScene(py::module_& module) {
  auto composition = submodule(module, "compose");
  bindRecord<compose::SurfaceRange>(composition, "SurfaceRange",
                                    "Unknown SurfaceRange field: ")
      .def_readwrite("emission", &compose::SurfaceRange::emission,
                     "The light the emissive map's white stands for, in "
                     "scene units; zero learns it from the brightest "
                     "emission the page paints.")
      .def_readwrite("thickness", &compose::SurfaceRange::thickness,
                     "The depth the thickness map's white stands for; zero "
                     "learns it from the deepest volume the page paints.")
      .def_readwrite("distortion", &compose::SurfaceRange::distortion,
                     "The displacement the distortion map's full swing "
                     "stands for; zero learns it from the strongest "
                     "distortion the page paints.")
      .def(py::self == py::self)
      .doc() =
      "What a page's eight-bit maps stand for at their white. A stated "
      "number holds from the first paint and content past it clips; a "
      "number left at zero is learned as the page paints.";
}

}  // namespace sigil::python
