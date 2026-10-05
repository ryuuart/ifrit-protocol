/** @file
 * `sigil.material`'s textures: `Texture`, an image and how it is sampled,
 * built over a `skia.Image`, a `media.Image` or a `media.Video`; its
 * `Sampling` and `PixelRect`; `image`, a picture or a texture as a
 * material; `flipbook`, a sheet of frames played as a material; `flowmap`,
 * a material moving along a texture of directions; and
 * `instanceAttributeKey`, the number an instance scalar is asked for by.
 */

#include <include/core/SkImage.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <sigilmaterial/core/Instance.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmaterial/texture/Flowmap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/video/Video.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Casters.h>
#include <sigilpython/material/Registration.h>
#include <sigilpython/motion/Convert.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** The pixels @p value names: a decoded image or a video read on the
 *  clock the material is drawn at, or a picture the same at every time. */
media::PixelSource pixelSourceOf(py::handle value) {
  if (py::isinstance<media::Image>(value))
    return media::PixelSource(std::shared_ptr<const media::Image>(
        value.cast<std::shared_ptr<media::Image>>()));
  if (py::isinstance<media::Video>(value))
    return media::PixelSource(std::shared_ptr<const media::Video>(
        value.cast<std::shared_ptr<media::Video>>()));
  return media::PixelSource(value.cast<sk_sp<SkImage>>());
}

/** A bound frame or progress as Python reads it: the animatable itself,
 *  or None where the clock decides. */
py::object animatableReading(
    const std::optional<motion::Animatable<float>>& value) {
  if (!value) return py::none();
  return py::cast(*value);
}

std::optional<motion::Animatable<float>> optionalAnimatable(py::handle value) {
  if (value.is_none()) return std::nullopt;
  return motionAnimatable(value);
}

}  // namespace

void bindMaterialTexture(py::module_& module) {
  auto materials = submodule(module, "material");
  const auto fluent = py::return_value_policy::reference_internal;

  py::enum_<material::Sampling>(materials, "Sampling")
      .value("Nearest", material::Sampling::Nearest)
      .value("Linear", material::Sampling::Linear);

  bindRecord<material::PixelRect>(materials, "PixelRect",
                                  "Unknown PixelRect field: ")
      .def_readwrite("x", &material::PixelRect::x)
      .def_readwrite("y", &material::PixelRect::y)
      .def_readwrite("width", &material::PixelRect::width)
      .def_readwrite("height", &material::PixelRect::height)
      .def("empty", &material::PixelRect::empty)
      .def(py::self == py::self);

  py::class_<material::Texture> texture(materials, "Texture");
  texture.def(py::init<>())
      .def(py::init([](py::handle source) {
             return material::Texture(pixelSourceOf(source));
           }),
           py::arg("source"))
      .def("copy", [](const material::Texture& self) { return self; })
      .def("tile",
           py::overload_cast<material::Repeat, material::Repeat>(
               &material::Texture::tile),
           py::arg("horizontal"), py::arg("vertical"), fluent)
      .def("tile",
           py::overload_cast<material::Repeat>(&material::Texture::tile),
           py::arg("both"), fluent)
      .def("uv", py::overload_cast<const glm::mat3&>(&material::Texture::uv),
           py::arg("matrix"), fluent)
      .def("at", &material::Texture::at, py::arg("origin"), fluent)
      .def(
          "region",
          [](material::Texture& self, const material::PixelRect& rect)
              -> material::Texture& { return self.region(rect); },
          py::arg("rect"), fluent)
      .def(
          "sampling",
          [](material::Texture& self, material::Sampling mode)
              -> material::Texture& { return self.sampling(mode); },
          py::arg("mode"), fluent)
      .def(
          "colorSpace",
          [](material::Texture& self, media::ColorSpace space)
              -> material::Texture& { return self.colorSpace(space); },
          py::arg("space"), fluent)
      .def(
          "mipmaps",
          [](material::Texture& self, bool on) -> material::Texture& {
            return self.mipmaps(on);
          },
          py::arg("on"), fluent)
      .def("tileX", &material::Texture::tileX)
      .def("tileY", &material::Texture::tileY)
      .def("uv", [](const material::Texture& self) { return self.uv(); })
      .def("region",
           [](const material::Texture& self) { return self.region(); })
      .def("sampling",
           [](const material::Texture& self) { return self.sampling(); })
      .def("colorSpace",
           [](const material::Texture& self) { return self.colorSpace(); })
      .def("mipmaps",
           [](const material::Texture& self) { return self.mipmaps(); })
      .def("valid", &material::Texture::valid)
      .def("animated", &material::Texture::animated)
      .def("size", &material::Texture::size)
      .def(
          "frameAt",
          [](const material::Texture& self,
             std::chrono::duration<double> time) { return self.frameAt(time); },
          py::arg("time") = std::chrono::duration<double>{})
      .def(py::self == py::self);
  copyProtocol(texture);

  bindRecord<material::ImageOptions>(materials, "ImageOptions",
                                     "Unknown ImageOptions field: ")
      .def_readwrite("repeat", &material::ImageOptions::repeat)
      .def_readwrite("repeatY", &material::ImageOptions::repeatY)
      .def_readwrite("colorSpace", &material::ImageOptions::colorSpace)
      .def(py::self == py::self);
  materials.def(
      "image",
      [](const material::Texture& texture) { return material::image(texture); },
      py::arg("texture"));
  materials.def(
      "image",
      [](py::handle pixels, const material::ImageOptions& options) {
        return material::image(pixelSourceOf(pixels), options);
      },
      py::arg("pixels"), py::arg("options") = material::ImageOptions{});

  using material::FlipbookOptions;
  bindRecord<FlipbookOptions>(materials, "FlipbookOptions",
                              "Unknown FlipbookOptions field: ")
      .def_readwrite("columns", &FlipbookOptions::columns)
      .def_readwrite("rows", &FlipbookOptions::rows)
      .def_readwrite("frames", &FlipbookOptions::frames)
      .def_readwrite("framesPerSecond", &FlipbookOptions::framesPerSecond)
      .def_readwrite("loop", &FlipbookOptions::loop)
      .def_readwrite("blendFrames", &FlipbookOptions::blendFrames)
      .def_readwrite("startFrame", &FlipbookOptions::startFrame)
      .def_property(
          "frame",
          [](const FlipbookOptions& self) {
            return animatableReading(self.frame);
          },
          [](FlipbookOptions& self, py::handle value) {
            self.frame = optionalAnimatable(value);
          })
      .def_property(
          "progress",
          [](const FlipbookOptions& self) {
            return animatableReading(self.progress);
          },
          [](FlipbookOptions& self, py::handle value) {
            self.progress = optionalAnimatable(value);
          })
      .def_readwrite("frameAttribute", &FlipbookOptions::frameAttribute)
      .def_readwrite("motionVectors", &FlipbookOptions::motionVectors)
      .def_readwrite("motionStrength", &FlipbookOptions::motionStrength);
  materials.def("flipbook", &material::flipbook, py::arg("sheet"),
                py::arg("options") = FlipbookOptions{});

  using material::FlowmapOptions;
  bindRecord<FlowmapOptions>(materials, "FlowmapOptions",
                             "Unknown FlowmapOptions field: ")
      .def_readwrite("flow", &FlowmapOptions::flow)
      .def_readwrite("strength", &FlowmapOptions::strength)
      .def_readwrite("speed", &FlowmapOptions::speed);
  materials.def("flowmap", &material::flowmap, py::arg("source"),
                py::arg("options"));

  materials.def(
      "instanceAttributeKey",
      [](const std::string& name) {
        return material::instanceAttributeKey(name);
      },
      py::arg("name"));
}

}  // namespace sigil::python
