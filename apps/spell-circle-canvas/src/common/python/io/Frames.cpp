#include <pybind11/stl.h>
#include <sigilio/frames/Frame.h>
#include <sigilio/frames/Publisher.h>
#include <sigilio/frames/Subscription.h>
#include <sigilio/hub/Hub.h>
#include <sigilpython/Extend.h>
#include <sigilpython/io/Hub.h>
#include <sigilpython/io/Registration.h>

#include <cstdint>
#include <optional>
#include <string>

namespace sigil::python {

namespace py = pybind11;

namespace {

/** A native handle as the integer Python carries it. */
void* handleOf(std::uintptr_t value) {
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return reinterpret_cast<void*>(value);
}

std::uintptr_t integerOf(void* handle) {
  return reinterpret_cast<std::uintptr_t>(handle);
}

}  // namespace

void bindIOFrames(pybind11::module_& module) {
  auto frames = submodule(module, "io.frames");
  py::class_<io::frames::Frame>(frames, "Frame")
      .def(py::init([](std::uintptr_t texture, std::uintptr_t commandBuffer,
                       int width, int height) {
             return io::frames::Frame{handleOf(texture),
                                      handleOf(commandBuffer), width, height};
           }),
           py::arg("texture") = 0, py::arg("commandBuffer") = 0,
           py::arg("width") = 0, py::arg("height") = 0)
      .def_property(
          "texture",
          [](const io::frames::Frame& frame) { return integerOf(frame.texture); },
          [](io::frames::Frame& frame, std::uintptr_t value) {
            frame.texture = handleOf(value);
          })
      .def_property(
          "commandBuffer",
          [](const io::frames::Frame& frame) {
            return integerOf(frame.commandBuffer);
          },
          [](io::frames::Frame& frame, std::uintptr_t value) {
            frame.commandBuffer = handleOf(value);
          })
      .def_readwrite("width", &io::frames::Frame::width)
      .def_readwrite("height", &io::frames::Frame::height);
  py::class_<io::frames::Publisher>(frames, "Publisher")
      .def("send", &io::frames::Publisher::send, py::arg("frame"))
      .def_property_readonly("name",
                             [](const io::frames::Publisher& publisher) {
                               return std::string(publisher.name());
                             })
      .def("__bool__", [](const io::frames::Publisher& publisher) {
        return static_cast<bool>(publisher);
      });
  py::class_<io::frames::Subscription>(frames, "Subscription")
      .def("latest", &io::frames::Subscription::latest)
      .def("state", &io::frames::Subscription::state)
      .def_property_readonly("name",
                             [](const io::frames::Subscription& subscription) {
                               return std::string(subscription.name());
                             })
      .def("__bool__", [](const io::frames::Subscription& subscription) {
        return static_cast<bool>(subscription);
      });

  // The hub's own verbs, added to the hub the resources module made:
  // publishing and subscribing are asked of a hub, as in C++.
  py::object hub = module.attr("io").attr("Hub");
  hub.attr("publish") = py::cpp_function(
      [](const HubHandle& value, const std::string& uri,
         std::uintptr_t device) {
        auto& owner = value.get();
        return owner.publish(uri, {.device = {.handle = handleOf(device)}});
      },
      py::name("publish"), py::is_method(hub), py::arg("uri"),
      py::arg("device") = 0);
  hub.attr("subscribe") = py::cpp_function(
      [](const HubHandle& value, const std::string& uri,
         const std::string& application, std::uintptr_t device) {
        auto& owner = value.get();
        return owner.subscribe(uri, {.application = application,
                                     .device = {.handle = handleOf(device)}});
      },
      py::name("subscribe"), py::is_method(hub), py::arg("uri"),
      py::arg("application") = "", py::arg("device") = 0);
}

}  // namespace sigil::python
