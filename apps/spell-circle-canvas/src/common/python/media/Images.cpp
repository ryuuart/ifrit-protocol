/** @file
 * `sigil.media`'s stills: the image document and its frames, the one
 * format list, the decode and encode doors, pixels made from a buffer,
 * and the pixel difference.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <pybind11/stl.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/core/Image.h>
#include <sigilmedia/difference/Difference.h>
#include <sigilmedia/image/Decode.h>
#include <sigilmedia/image/Encode.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Extend.h>
#include <sigilpython/media/Registration.h>
#include <sigilpython/skia/Values.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

namespace {

/** A native document held the way Python holds it: pybind11 takes no
 *  holder of a const type, so the document a hub or a decode shares is
 *  held mutable here and never written through. */
std::shared_ptr<media::Image> held(std::shared_ptr<const media::Image> image) {
  return std::const_pointer_cast<media::Image>(std::move(image));
}

std::span<const std::byte> bytesOf(const std::string& bytes) {
  return {reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()};
}

py::bytes pythonBytes(const std::vector<std::byte>& bytes) {
  return py::bytes(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

}  // namespace

void bindMediaImages(py::module_& module) {
  auto media = submodule(module, "media");
  media.def(
      "fromRgba",
      [](py::buffer buffer, int width, int height) {
        if (width < 1 || height < 1 || width > 16384 || height > 16384)
          throw py::value_error(
              "Image dimensions must be between one and 16384 pixels.");
        const auto source = buffer.request();
        if (source.itemsize != 1 ||
            source.size != static_cast<py::ssize_t>(width) * height * 4)
          throw py::value_error(
              "RGBA pixels need exactly four bytes per pixel.");
        py::ssize_t stride = 1;
        for (py::ssize_t axis = source.ndim; axis-- > 0;) {
          if (source.shape[axis] > 1 && source.strides[axis] != stride)
            throw py::value_error("RGBA pixels must be C-contiguous.");
          stride *= source.shape[axis];
        }
        const auto info = SkImageInfo::Make(
            width, height, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        auto image = SkImages::RasterFromPixmapCopy(
            SkPixmap(info, source.ptr, info.minRowBytes()));
        if (!image)
          throw py::value_error("The pixel image could not be allocated.");
        return image;
      },
      py::arg("pixels"), py::arg("width"), py::arg("height"));
  py::enum_<media::Format>(media, "Format")
      .value("Png", media::Format::Png)
      .value("Jpeg", media::Format::Jpeg)
      .value("Webp", media::Format::Webp)
      .value("Exr", media::Format::Exr)
      .value("Mp4", media::Format::Mp4);
  // A frame standing on a device is read back when Python asks for its
  // picture: Python draws through a canvas that holds no recorder of the
  // device a hardware decode or a publication stands on.
  py::class_<media::Frame>(media, "Frame")
      .def_property_readonly(
          "image",
          [](const media::Frame& frame) {
            return media::deviceImage(frame, nullptr);
          })
      .def_readonly("time", &media::Frame::time)
      .def_readonly("duration", &media::Frame::duration)
      .def_readonly("index", &media::Frame::index);
  // Held shared, because the image leaf keeps the document it is given
  // and compares it by identity: one Python image is one native image in
  // every describe, so the leaf over it prunes.
  py::class_<media::Image, std::shared_ptr<media::Image>>(media, "Image")
      .def_static(
          "of",
          [](sk_sp<SkImage> picture) {
            return held(media::Image::of(std::move(picture)));
          },
          py::arg("picture"))
      .def("size",
           [](const media::Image& image) {
             return py::make_tuple(image.size().x, image.size().y);
           })
      .def("duration", &media::Image::duration)
      .def("repetitions", &media::Image::repetitions)
      .def("isRunning", &media::Image::isRunning)
      .def("frames", &media::Image::frames)
      .def(
          "frameAt",
          [](const media::Image& image,
             std::chrono::duration<double> elapsed) {
            return image.frameAt(elapsed);
          },
          py::arg("elapsed"));
  media.def(
      "decode",
      [](py::handle type, py::bytes data, int width, int height,
         const std::string& layer, const std::string& hint) {
        if (!type.is(py::type::of<media::Image>()))
          throw py::type_error(
              "media.decode answers an Image; a video opens through a hub");
        const std::string bytes = data;
        auto decoded = media::decode<media::Image>(
            bytesOf(bytes), {.layer = layer, .width = width, .height = height},
            hint);
        if (!decoded) throw py::value_error("The image could not be decoded.");
        return held(std::move(decoded));
      },
      py::arg("type"), py::arg("data"), py::kw_only(), py::arg("width") = 0,
      py::arg("height") = 0, py::arg("layer") = "", py::arg("hint") = "");
  media.def(
      "encode",
      [](const SkImage& image, media::Format format, int quality) {
        auto data = media::encode(image, format, {.quality = quality});
        if (data.empty())
          throw py::value_error("The image could not be encoded.");
        return pythonBytes(data);
      },
      py::arg("image"), py::arg("format") = media::Format::Png,
      py::arg("quality") = 100);
  media.def(
      "encode",
      [](const media::Image& image, media::Format format, int quality) {
        auto data = media::encode(image, format, {.quality = quality});
        if (data.empty())
          throw py::value_error("The image could not be encoded.");
        return pythonBytes(data);
      },
      py::arg("image"), py::arg("format") = media::Format::Png,
      py::arg("quality") = 100);
  py::class_<media::PixelDifference>(media, "PixelDifference")
      .def_readonly("differingPixels", &media::PixelDifference::differingPixels)
      .def_readonly("worst", &media::PixelDifference::worst)
      .def_readonly("x", &media::PixelDifference::x)
      .def_readonly("y", &media::PixelDifference::y)
      .def("identical", &media::PixelDifference::identical);
  media.def(
      "difference",
      [](const SkImage& actual, const SkImage& expected) {
        return media::difference(actual, expected);
      },
      py::arg("actual"), py::arg("expected"));
  media.def(
      "difference",
      [](const media::Image& actual, const media::Image& expected) {
        return media::difference(actual, expected);
      },
      py::arg("actual"), py::arg("expected"));
}

}  // namespace sigil::python
