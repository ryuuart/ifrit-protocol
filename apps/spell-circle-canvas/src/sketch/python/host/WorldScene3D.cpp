/** @file
 * A compose scene painted into a texture, as a Python scene3d sketch holds
 * the one its context handed it: a 2D screen a body wears as its own
 * light, or a picture laid over the finished frame.
 */

#include <pybind11/stl.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilpython/Extend.h>

#include <cmath>
#include <memory>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

void bindSketchWorldScene3D(py::module_& module) {
  auto composition = sigil::python::submodule(module, "compose");
  py::class_<compose::TextureScene, std::shared_ptr<compose::TextureScene>>(
      composition, "TextureScene",
      "A compose tree painted into a texture: a 2D screen a body wears as "
      "its own light, or a picture laid over a finished frame. A scene3d "
      "sketch asks for one with ctx.textureScene((width, height)) at setup "
      "and holds it; describe hands it the tree with render() and puts "
      "texture() where the picture goes.")
      .def(
          "render",
          [](compose::TextureScene& scene, const compose::Element& tree,
             double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
              throw py::value_error(
                  "A texture scene's moment must be finite and nonnegative");
            scene.render(tree, seconds);
          },
          py::arg("tree"), py::arg("seconds") = 0.0,
          "Reconcile tree and paint it when it changed, with the scene's "
          "clock at seconds; an earlier moment leaves the clock where it "
          "stands.")
      .def("texture", &compose::TextureScene::texture,
           "A sampling value holding this scene and its newest paint.")
      .def_property_readonly("revision", &compose::TextureScene::revision,
                             "How many times this scene has painted.")
      .def_property_readonly(
          "size",
          [](const compose::TextureScene& scene) {
            const SkISize size = scene.size();
            return py::make_tuple(size.width(), size.height());
          },
          "The scene's size in pixels, as (width, height).");
}

}  // namespace sigil::sketch::python
