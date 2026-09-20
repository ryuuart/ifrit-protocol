#include <include/core/SkCanvas.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilcompose/core/Measure.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/compose/Kit.h>
#include <sigilsketch/canvas/Guest.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/core/CanvasSpecification.h>
#include <sigilworld/frame/Frame.h>

#include <array>
#include <cmath>
#include <filesystem>
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
namespace py = pybind11;

using detail::AssetsView;
using detail::ComposerView;
using detail::Context;
using detail::TickerView;

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

SkISize pixels(std::array<int, 2> size) {
  if (size[0] <= 0 || size[1] <= 0)
    throw py::value_error("A baked set's extent must be positive");
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
  field(declared, "captureSeconds", &CanvasSpecification::captureSeconds);
  field(declared, "oversample", &CanvasSpecification::oversample);
  field(declared, "plateOnly", &CanvasSpecification::plateOnly);
  field(declared, "nonlinearPicture",
        &CanvasSpecification::nonlinearPicture);
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
      .def_property_readonly(
          "composer",
          [](const Context& ctx) { return ComposerView(ctx.state()); })
      .def_property_readonly(
          "ticker", [](const Context& ctx) { return TickerView(ctx.state()); })
      .def_property_readonly(
          "assets", [](const Context& ctx) { return AssetsView(ctx.state()); })
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
          "nonlinearPicture",
          [](const Context& ctx) { ctx.state()->context().nonlinearPicture(); })
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
          "bakeSet",
          [](const Context& ctx, const world::Frame& frame,
             const geometry::mesh::camera::Camera& camera,
             std::array<int, 2> size, SkColor4f background, double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
              throw py::value_error(
                  "A bake's moment must be finite and nonnegative");
            const SkISize extent = pixels(size);
            return ctx.state()->context().bakeSet(frame, camera, extent,
                                                  background, seconds);
          },
          py::arg("frame"), py::arg("camera"), py::arg("size"),
          py::arg("background") = SkColor4f{0, 0, 0, 0},
          py::arg("seconds") = 0.0)
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
          [](const Context& ctx) { return ctx.state()->ticker->elapsed(); })
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
      .def("application", [](const Guest& guest) {
        return std::string(guest.application());
      });
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
