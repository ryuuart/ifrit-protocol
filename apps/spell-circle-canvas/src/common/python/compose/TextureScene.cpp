/** @file
 * A compose scene painted into a texture: the `TextureScene` a body wears
 * as its own light or a frame lays over itself, and what a page painted as
 * the maps of a surface states about its maps — `SurfaceRange`, the
 * strength, depth and displacement each map's white stands for.
 */

#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

#include <cmath>
#include <memory>

namespace sigil::python {
namespace py = pybind11;

void bindComposeTextureScene(py::module_& module) {
  auto composition = submodule(module, "compose");
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
