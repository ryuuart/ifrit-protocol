#include <include/core/SkCanvas.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Composer.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/motion/Convert.h>
#include <sigilpython/weave/FontContext.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilsketch/scene2d/Guest.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Page.h>
#include <sigilworld/frame/Frame.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Registration.h"
#include "Session.h"

namespace sigil::sketch::python {
namespace material = sigil::material;
namespace py = pybind11;

using detail::AssetsView;
using detail::Context;

namespace {
using sigil::python::kit::field;

/** The canvas a declaration asks for, refused before the host realizes
 *  it: a surface of no width or of an unrepresentable one is not a
 *  smaller picture but no picture at all, and the failure would surface
 *  frames later as an empty capture. */
void checkCanvasSize(SkSize size) {
  if (!std::isfinite(size.width()) || !std::isfinite(size.height()) ||
      size.width() <= 0 || size.height() <= 0)
    throw py::value_error("Canvas dimensions must be finite and positive");
}

/** A page's size in its own pixels, refused before a scene is made of
 *  it: a page of no width paints nothing a body could wear. */
glm::ivec2 pagePixels(std::array<int, 2> size) {
  if (size[0] <= 0 || size[1] <= 0)
    throw py::value_error("A page's size must be positive");
  return {size[0], size[1]};
}

/** A bake's size in canvas units, refused before a scene is made of it. */
SkISize pixels(std::array<int, 2> size) {
  if (size[0] <= 0 || size[1] <= 0)
    throw py::value_error("A baked 3D frame's extent must be positive");
  return {size[0], size[1]};
}

/** The canvas the drawing is landing on, taken off the pen a paint
 *  program was handed — the one door Python has onto a canvas, and the
 *  same checked loan every other drawing binding reads. */
SkCanvas& canvasOf(py::handle drawing) {
  SkCanvas* canvas = sigil::python::pen(drawing).canvas();
  if (!canvas) throw std::runtime_error("The pen has no open canvas");
  return *canvas;
}

void bindCanvasSpecification(py::module_& sketches) {
  auto declared = sigil::python::bindRecord<CanvasSpecification>(
      sketches, "CanvasSpecification", "Unknown canvas property: ");
  field(declared, "size", &CanvasSpecification::size);
  field(declared, "background", &CanvasSpecification::background);
  field(declared, "captureAt", &CanvasSpecification::captureSeconds);
  field(declared, "oversample", &CanvasSpecification::oversample);
  field(declared, "paintDiscardedFrames",
        &CanvasSpecification::paintDiscardedFrames);
  field(declared, "plateOnly", &CanvasSpecification::plateOnly);
}

void bindContext(py::module_& module) {
  py::class_<Context, std::shared_ptr<Context>>(module, "Context")
      .def(
          "canvas",
          [](const Context& ctx, float width, float height) {
            checkCanvasSize({width, height});
            ctx.state()->specification->size = {width, height};
          },
          py::arg("width"), py::arg("height"))
      .def(
          "canvas",
          [](const Context& ctx, const CanvasSpecification& declared) {
            checkCanvasSize(declared.size);
            ctx.state()->context().canvas(declared);
          },
          py::arg("declared"))
      .def(
          "background",
          [](const Context& ctx, py::handle value) {
            ctx.state()->specification->background =
                sigil::python::color(value);
          },
          py::arg("color"))
      .def(
          "captureAt",
          [](const Context& ctx, double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
              throw py::value_error(
                  "Capture time must be finite and nonnegative");
            ctx.state()->specification->captureSeconds = seconds;
          },
          py::arg("seconds"))
      .def(
          "render",
          [](const Context& ctx, const compose::Element& element) {
            ctx.state()->composer->render(element);
          },
          py::arg("element"))
      // The session's own composer, handed over as the one Composer
      // class Python has: the host sizes it, sets its clock and draws
      // it, so the handle borrows rather than owns, and every call on it
      // asks the session again whether it is still open.
      .def_property_readonly(
          "composer",
          [](const Context& ctx) {
            return sigil::python::ComposerHandle(
                [view = Context(ctx.state())]() -> compose::Composer& {
                  return *view.state()->composer;
                });
          })
      // The session's own engine, handed over as the one Engine class
      // Python has: the host moves the clock, so the handle borrows
      // rather than owns, and the callables a body hands it are held
      // against the session rather than by the wrapper a body kept.
      .def_property_readonly(
          "engine",
          [](const Context& ctx) {
            const auto state = ctx.state();
            return sigil::python::EngineHandle(
                [view = Context(state)]() -> motion::Engine& {
                  return *view.state()->engine;
                },
                &state->callbacks);
          })
      .def_property_readonly(
          "assets", [](const Context& ctx) { return AssetsView(ctx.state()); })
      // The session's own font context, lent: every font the session's hub
      // loads is adopted there under its family, so `weave.fonts` handed it
      // lists the bundled families beside the installed ones.
      .def_property_readonly(
          "fonts",
          [](const Context& ctx) {
            return sigil::python::FontContextHandle(
                [view = Context(ctx.state())]() -> weave::FontContext& {
                  return *view.state()->fonts;
                });
          })
      .def(
          "measured",
          [](const Context& ctx, double value, double pinned) {
            return ctx.state()->context().measured(value, pinned);
          },
          py::arg("value"), py::arg("pinned") = 0)
      .def(
          "oversample",
          [](const Context& ctx, int samples) {
            ctx.state()->context().oversample(samples);
          },
          py::arg("samples"))
      .def("plate", [](const Context& ctx) { ctx.state()->context().plate(); })
      .def(
          "measure",
          [](const Context& ctx, const compose::Element& element,
             SkSize maximum) {
            return ctx.state()->context().measure(element, maximum);
          },
          py::arg("element"), py::arg("maxSize") = SkSize::MakeEmpty())
      .def(
          "snapshot",
          [](const Context& ctx, const compose::Element& element,
             SkSize maximum) {
            const auto state = ctx.state();
            return compose::snapshot(element, *state->fonts, maximum);
          },
          py::arg("element"), py::arg("maxSize") = SkSize::MakeEmpty())
      .def(
          "bakeScene3D",
          [](const Context& ctx, const world::Frame& frame,
             std::array<int, 2> size,
             const geometry::mesh::camera::Camera& camera,
             material::Color background, double at,
             std::optional<double> density) {
            if (!std::isfinite(at) || at < 0)
              throw py::value_error(
                  "A bake's moment must be finite and nonnegative");
            if (density && (!std::isfinite(*density) || *density <= 0))
              throw py::value_error(
                  "A bake's density must be finite and positive");
            const SkISize extent = pixels(size);
            // No density stated is zero to the context, which then bakes
            // at the density the sketch is drawn at.
            return ctx.state()->context().bakeScene3D(
                frame, extent,
                {.camera = camera,
                 .background = background,
                 .at = motion::Duration(at),
                 .density = density ? (float)*density : 0.0f});
          },
          py::arg("frame"), py::arg("size"),
          py::arg("camera") = geometry::mesh::camera::Camera{},
          py::arg("background") = material::Color{0, 0, 0, 0},
          py::arg("at") = 0.0, py::arg("density") = std::nullopt,
          "The frame drawn once into an image on the device the host brought "
          "up, or None where there is none. size is the panel the image "
          "fills, in canvas units; the image is size times density pixels "
          "across, and density, unless stated, is the density the sketch is "
          "drawn at, so the bake lands on the device's pixels. Paint it "
          "stretched over size. at is the moment of the bake on the baked "
          "scene's own clock.")
      .def(
          "page",
          [](const Context& ctx, std::array<int, 2> size,
             const compose::SurfaceRange& range) {
            return ctx.state()->context().page(pagePixels(size), range);
          },
          py::arg("size"), py::arg("range") = compose::SurfaceRange{},
          "A page a baked 3D frame lights, size pixels across: put "
          "page.render(tree) on the frame and page.surface() on the body "
          "that shows it. range, a compose.SurfaceRange, states what the "
          "page's emission, thickness and distortion maps stand for at "
          "their white; a number left at zero is learned as the page "
          "paints. The session keeps what it paints into until the "
          "sketch declares again; ask while declaring and hold the page.")
      .def(
          "local",
          [](const Context& ctx, const std::string& name) {
            return "sketch://" + ctx.state()->key + "/" + name;
          },
          py::arg("path"))
      .def_property_readonly(
          "key", [](const Context& ctx) { return ctx.state()->key; })
      .def_property_readonly(
          "elapsed",
          [](const Context& ctx) { return ctx.state()->engine->elapsed(); })
      .def_property_readonly("width",
                             [](const Context& ctx) {
                               return ctx.state()->specification->size.width();
                             })
      .def_property_readonly("height",
                             [](const Context& ctx) {
                               return ctx.state()->specification->size.height();
                             })
      .def_property_readonly("size",
                             [](const Context& ctx) {
                               const auto state = ctx.state();
                               return py::make_tuple(
                                   state->specification->size.width(),
                                   state->specification->size.height());
                             })
      // What the host fed the session, read when asked: a body with no
      // paint program reads the pointer and the keyboard here.
      .def_property_readonly(
          "pointer",
          [](const Context& ctx) { return ctx.state()->composer->pointer(); },
          "Where the pointer stands over the canvas, in canvas units, and "
          "whether its button is down, as a compose.PaintContext.Pointer: "
          "what the host fed the session, read when it is asked. The origin "
          "with the button up where nobody points.")
      .def_property_readonly(
          "keys",
          [](const Context& ctx) { return ctx.state()->composer->keys(); },
          "The keyboard as the host fed the session, as a compose.KeyState: "
          "the last key to go down by name and code, whether any key is "
          "down, and every key that is.")
      .def_property_readonly("deterministic", [](const Context& ctx) {
        return ctx.state()->deterministic;
      });
}

void bindGuest(py::module_& sketches) {
  py::class_<Guest>(sketches, "Guest")
      .def(py::init([](const Context& context, std::string name,
                       std::string application) {
             auto handed = context.state()->context();
             return std::make_unique<Guest>(handed, std::move(name),
                                            std::move(application));
           }),
           py::arg("context"), py::arg("name"),
           py::arg("application") = std::string())
      .def(
          "frame",
          [](Guest& guest, py::handle pen) {
            return guest.frame(canvasOf(pen));
          },
          py::arg("pen"))
      .def("publishing", &Guest::publishing)
      .def("name", [](const Guest& guest) { return std::string(guest.name()); })
      .def("application",
           [](const Guest& guest) { return std::string(guest.application()); });
}

void bindCachedArt(py::module_& sketches) {
  sketches.def(
      "requireCached",
      [](const std::vector<std::string>& urls,
         const std::optional<std::filesystem::path>& cacheDirectory) {
        // The span reads the strings the vector owns, so both outlive
        // the call that is given them.
        std::vector<std::string_view> asked;
        asked.reserve(urls.size());
        for (const std::string& url : urls) asked.emplace_back(url);
        std::string why;
        const bool present = sigil::sketch::requireCached(
            asked, &why,
            cacheDirectory ? *cacheDirectory : std::filesystem::path{});
        return py::make_tuple(present, why);
      },
      py::arg("urls"),
      py::arg("cacheDirectory") = std::optional<std::filesystem::path>());
}

}  // namespace

void bindSketchContextSurface(py::module_& module) {
  auto sketches = sigil::python::submodule(module, "sketch");
  // The specification is registered before the call that names it, so
  // the declaration a generator reads carries the Python spelling.
  bindCanvasSpecification(sketches);
  bindContext(module);
  // The context stays at the extension root, where the sketch package
  // imports it from, and answers to its own module as well.
  sketches.attr("Context") = module.attr("Context");
  bindGuest(sketches);
  bindCachedArt(sketches);
}

}  // namespace sigil::sketch::python
