/** @file
 * What a Python scene3d sketch is handed at setup: the plate it is
 * photographed onto, the lens its frame is seen through, and the two doors
 * a 2D picture comes in by — a page the scene lights and a screen that is
 * its own light.
 */

#include <pybind11/stl.h>
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/weave/FontContext.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

#include "Registration.h"
#include "Session.h"

namespace sigil::sketch::python {
namespace py = pybind11;

using detail::Scene3DContextView;

namespace {

/** The plate's size, refused before the host realizes it: a canvas of no
 *  width is no picture at all. */
SkSize plate(float width, float height) {
  if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
      height <= 0)
    throw py::value_error("Canvas dimensions must be finite and positive");
  return {width, height};
}

/** A page's or a screen's size in its own pixels, refused before a scene
 *  is made of it. */
glm::ivec2 pixels(std::array<int, 2> size, const char* what) {
  if (size[0] <= 0 || size[1] <= 0)
    throw py::value_error(std::string(what) + "'s size must be positive");
  return {size[0], size[1]};
}

CanvasSpecification& specification(const Scene3DContextView& view) {
  CanvasSpecification* declared = view.context().specification;
  if (!declared)
    throw std::runtime_error("This scene3d context has no plate to declare");
  return *declared;
}

}  // namespace

void bindSketchScene3DSketches(py::module_& module) {
  auto sketches = sigil::python::submodule(module, "sketch");
  py::class_<Scene3DContextView>(
      sketches, "Scene3DContext",
      "What a scene3d sketch is handed at setup: the plate, the lens its "
      "frame is seen through, and the pages and screens its bodies wear. "
      "It answers while setup runs; describe(seconds) is a function of the "
      "scene time and is handed nothing.")
      .def(
          "canvas",
          [](const Scene3DContextView& view, float width, float height) {
            specification(view).size = plate(width, height);
          },
          py::arg("width"), py::arg("height"),
          "Declare the plate's size in canvas units.")
      .def(
          "canvas",
          [](const Scene3DContextView& view,
             const CanvasSpecification& declared) {
            (void)plate(declared.size.width(), declared.size.height());
            specification(view) = declared;
          },
          py::arg("declared"),
          "Declare the whole plate: size, background, capture moment and "
          "oversample.")
      .def(
          "background",
          [](const Scene3DContextView& view, py::handle value) {
            view.context().background(sigil::python::color(value));
          },
          py::arg("color"),
          "The colour behind the scene, and what the frame's own clear is "
          "written with.")
      .def(
          "captureAt",
          [](const Scene3DContextView& view, sigil::motion::Duration moment) {
            const double seconds = moment.count();
            if (!std::isfinite(seconds) || seconds < 0)
              throw py::value_error(
                  "Capture time must be finite and nonnegative");
            view.context().captureAt(seconds);
          },
          py::arg("seconds"), "The scene time a still is taken at.")
      .def(
          "camera",
          [](const Scene3DContextView& view,
             const geometry::mesh::camera::Camera& lens) {
            view.context().camera(lens);
          },
          py::arg("lens"),
          "The lens a frame holding no camera of its own is seen through.")
      .def(
          "page",
          [](const Scene3DContextView& view, std::array<int, 2> size,
             const compose::SurfaceRange& range) {
            return view.context().page(pixels(size, "A page"), range);
          },
          py::arg("size"), py::arg("range") = compose::SurfaceRange{},
          "A page the scene lights, size pixels across: put "
          "page.render(tree) on the frame describe returns and "
          "page.surface() on the body that shows it. range, a "
          "compose.SurfaceRange, states what the page's emission, thickness "
          "and distortion maps stand for at their white; a number left at "
          "zero is learned as the page paints. The session keeps what it "
          "paints into for as long as it runs; hold the page.")
      .def(
          "textureScene",
          [](const Scene3DContextView& view, std::array<int, 2> size,
             py::handle background) {
            const glm::ivec2 extent = pixels(size, "A texture scene");
            const material::Color clear =
                background.is_none() ? material::Color{0, 0, 0, 0}
                                     : sigil::python::color(background);
            return view.context().textureScene({extent.x, extent.y}, clear);
          },
          py::arg("size"), py::arg("background") = py::none(),
          "A compose scene painted into a texture, size pixels across and "
          "cleared to background: a 2D screen that is its own light. In "
          "describe, hand it the tree at the scene time with render() and "
          "put texture() in a material's slot or on the frame with "
          "overlay(). The session keeps it for as long as it runs.")
      // The session's own font context, lent as a 2D sketch's is: every
      // font the session's hub loads is adopted there under its family, so
      // `weave.fonts` handed it lists the bundled families beside the
      // installed ones. It answers while setup runs, as the context does.
      .def_property_readonly(
          "fonts",
          [](const Scene3DContextView& view) {
            return sigil::python::FontContextHandle(
                [view]() -> weave::FontContext& {
                  return view.context().fonts;
                });
          })
      .def_property_readonly(
          "deterministic",
          [](const Scene3DContextView& view) {
            return view.context().deterministic;
          },
          "True where the host is taking a capture that will be diffed: "
          "what the scene fetches must stand complete before the frame "
          "does.")
      .def_property_readonly("width",
                             [](const Scene3DContextView& view) {
                               return specification(view).size.width();
                             })
      .def_property_readonly("height",
                             [](const Scene3DContextView& view) {
                               return specification(view).size.height();
                             })
      .def_property_readonly("size", [](const Scene3DContextView& view) {
        const SkSize size = specification(view).size;
        return py::make_tuple(size.width(), size.height());
      });
}

}  // namespace sigil::sketch::python
