#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <sigilimage/decode/Decode.h>
#include <sigilimage/encode/Encode.h>
#include <sigilpython/Extend.h>
#include <sigilpython/image/Registration.h>
#include <sigilpython/skia/Values.h>

#include <memory>
#include <string>

namespace sigil::python {
namespace py = pybind11;

void bindImageValues(py::module_& module) {
  auto images = submodule(module, "image");
  images.def(
      "from_rgba",
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
  py::enum_<image::Format>(images, "Format")
      .value("Png", image::Format::Png)
      .value("Jpeg", image::Format::Jpeg)
      .value("Webp", image::Format::Webp)
      .value("Exr", image::Format::Exr);
  // Held shared, because the image leaf keeps the asset it is given and
  // compares it by identity: one Python asset is one native asset in
  // every describe, so the leaf over it prunes.
  py::class_<image::ImageAsset, std::shared_ptr<image::ImageAsset>>(
      images, "ImageAsset")
      .def("width", &image::ImageAsset::width)
      .def("height", &image::ImageAsset::height)
      .def("animated", &image::ImageAsset::animated)
      .def("totalDurationMs", &image::ImageAsset::totalDurationMs)
      .def(
          "frameAt",
          [](const image::ImageAsset& asset, double milliseconds) {
            return asset.frameAt(milliseconds).image;
          },
          py::arg("milliseconds"));
  const auto decode = [](py::bytes encoded, int width, int height,
                         const std::string& hint) {
    const std::string bytes = encoded;
    auto decoded = image::decodeImage(
        reinterpret_cast<const std::byte*>(bytes.data()), bytes.size(),
        {.width = width, .height = height}, hint);
    if (!decoded) throw py::value_error("The image could not be decoded.");
    return *decoded;
  };
  images.def("decodeAsset", decode, py::arg("data"), py::arg("width") = 0,
             py::arg("height") = 0, py::arg("hint") = "");
  images.def(
      "decode",
      [decode](py::bytes data, int width, int height, const std::string& hint) {
        return decode(data, width, height, hint).frameAt(0).image;
      },
      py::arg("data"), py::arg("width") = 0, py::arg("height") = 0,
      py::arg("hint") = "");
  images.def(
      "encode",
      [](const SkImage& image, image::Format format, int quality) {
        auto data = image::encodeImage(image, format, {.quality = quality});
        if (!data) throw py::value_error("The image could not be encoded.");
        return py::bytes(static_cast<const char*>(data->data()), data->size());
      },
      py::arg("image"), py::arg("format") = image::Format::Png,
      py::arg("quality") = 100);
}

}  // namespace sigil::python
