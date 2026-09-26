/** @file
 * `sigil.media`'s moving pictures: the video a hub opens, the decode pool
 * several share, and the movie encoder.
 */

#include <include/core/SkImage.h>
#include <sigilmedia/video/Encoder.h>
#include <sigilmedia/video/Video.h>
#include <sigilpython/Extend.h>
#include <sigilpython/media/Registration.h>

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace sigil::python {
namespace py = pybind11;

void bindMediaVideo(py::module_& module) {
  auto media = submodule(module, "media");
  py::class_<media::Playback, std::shared_ptr<media::Playback>>(media,
                                                                "Playback")
      .def(py::init([](std::optional<size_t> workers) {
             return std::make_shared<media::Playback>(
                 media::Playback::Options{.workers = workers});
           }),
           py::kw_only(), py::arg("workers") = py::none());
  // A video a hub opened is shared by every holder of it, so it is held
  // shared; its frames are asked through a const document.
  py::class_<media::Video, std::shared_ptr<media::Video>>(media, "Video")
      .def("size",
           [](const media::Video& video) {
             return py::make_tuple(video.size().width(), video.size().height());
           })
      .def("duration", &media::Video::duration)
      .def("isRunning", &media::Video::isRunning)
      .def("hasFrame", &media::Video::hasFrame)
      .def(
          "frameAt",
          [](const media::Video& video,
             std::chrono::duration<double> elapsed) {
            return video.frameAt(elapsed);
          },
          py::arg("elapsed"));
  py::class_<media::Encoder>(media, "Encoder")
      .def(py::init([](int width, int height, int framesPerSecond,
                       int64_t bitRate) {
             return std::make_unique<media::Encoder>(
                 media::Encoder::Options{.width = width,
                                         .height = height,
                                         .framesPerSecond = framesPerSecond,
                                         .bitRate = bitRate});
           }),
           py::kw_only(), py::arg("width"), py::arg("height"),
           py::arg("framesPerSecond") = 30, py::arg("bitRate") = 12'000'000)
      .def(
          "append",
          [](media::Encoder& encoder, const SkImage& picture) {
            return encoder.append(picture);
          },
          py::arg("picture"))
      .def(
          "append",
          [](media::Encoder& encoder, const media::Frame& frame) {
            return encoder.append(frame);
          },
          py::arg("frame"))
      .def("finish",
           [](media::Encoder& encoder) {
             const std::vector<std::byte> bytes = encoder.finish();
             return py::bytes(reinterpret_cast<const char*>(bytes.data()),
                              bytes.size());
           })
      .def("error", &media::Encoder::error)
      .def("frameCount", &media::Encoder::frameCount)
      .def("__bool__",
           [](const media::Encoder& encoder) { return static_cast<bool>(encoder); });
}

}  // namespace sigil::python
