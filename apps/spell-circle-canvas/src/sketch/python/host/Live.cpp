/** @file
 * A window in this interpreter: the sketch window library, loaded the
 * first time a script opens one, showing a sketch the interpreter holds,
 * and the hook that keeps it drawing while the prompt waits for a line.
 */

#include <Python.h>
#include <dlfcn.h>
#include <poll.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>
#include <sigilsketch/window/Window.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Held.h"
#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

namespace {

using OpenWindow = Window* (*)(const WindowOptions*, std::string*);

/** Where this extension was loaded from: the window library stands
 *  beside it. */
std::filesystem::path extensionDirectory() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<const void*>(&extensionDirectory), &info) == 0 ||
      !info.dli_fname)
    return {};
  return std::filesystem::path(info.dli_fname).parent_path();
}

/** THE WINDOW LIBRARY'S ONE DOOR, found once. A process that links the
 *  window itself answers by name; an ordinary interpreter loads the
 *  library beside this extension, which is what brings Qt into the
 *  process, and only now. */
OpenWindow windowDoor() {
  static OpenWindow door = nullptr;
  if (door) return door;
  if (auto* linked = reinterpret_cast<OpenWindow>(
          dlsym(RTLD_DEFAULT, "sigil_sketch_window_open")))
    return door = linked;
  const std::filesystem::path library =
      extensionDirectory() / "_sigil_window.so";
  std::error_code missing;
  if (!std::filesystem::is_regular_file(library, missing))
    throw std::runtime_error(
        "sigil.live opens its window through " + library.string() +
        ", which this installation does not carry: it is built beside the "
        "extension where Qt 6 was found at build time");
  void* handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    const char* reason = dlerror();
    throw std::runtime_error(
        "sigil.live could not load its window library, which needs Qt 6 "
        "(QtCore, QtGui and QtQuick) where it was built against them: " +
        std::string(reason ? reason : "no reason given"));
  }
  door =
      reinterpret_cast<OpenWindow>(dlsym(handle, "sigil_sketch_window_open"));
  if (!door)
    throw std::runtime_error(library.string() +
                             " opens no window: it carries no "
                             "sigil_sketch_window_open");
  return door;
}

/** The one font context every window in this process shapes with. */
weave::FontContext& processFonts() {
  static auto* fonts =
      new weave::FontContext(weave::ports::systemFontManager());
  return *fonts;
}

/** THE WINDOWS STILL OPEN, which the prompt's hook turns while it waits.
 *  Its own lock, because the hook may run without the interpreter's. */
std::mutex& openMutex() {
  static std::mutex mutex;
  return mutex;
}
std::vector<std::shared_ptr<Window>>& openWindows() {
  static std::vector<std::shared_ptr<Window>> windows;
  return windows;
}

void forget(const std::shared_ptr<Window>& window) {
  const std::lock_guard lock(openMutex());
  auto& windows = openWindows();
  windows.erase(std::remove(windows.begin(), windows.end(), window),
                windows.end());
}

/** Turns every open window once; false when none is left open. */
bool pumpOpenWindows() {
  std::vector<std::shared_ptr<Window>> windows;
  {
    const std::lock_guard lock(openMutex());
    windows = openWindows();
  }
  bool any = false;
  for (const std::shared_ptr<Window>& window : windows) {
    if (window->pump(std::chrono::milliseconds(0)))
      any = true;
    else
      forget(window);
  }
  return any;
}

/** THE PROMPT'S WAIT, spent drawing. The interpreter calls this while it
 *  waits for a line of input; it turns every open window until a line is
 *  there to read, and returns at once where none is open. It may be
 *  called holding the interpreter's lock or not; held, the lock is given
 *  up while it waits, so other threads run, and a sketch's own code takes
 *  it again as it draws. */
int pumpWhileWaiting() {
  PyThreadState* saved = PyGILState_Check() ? PyEval_SaveThread() : nullptr;
  constexpr int kFrameMilliseconds = 8;
  while (pumpOpenWindows()) {
    pollfd input{0, POLLIN, 0};
    if (::poll(&input, 1, kFrameMilliseconds) > 0) break;
  }
  if (saved) PyEval_RestoreThread(saved);
  return 0;
}

/** Installs the hook, leaving one another library installed alone. */
bool installPromptHook() {
  if (PyOS_InputHook == &pumpWhileWaiting) return true;
  if (PyOS_InputHook != nullptr) return false;
  PyOS_InputHook = &pumpWhileWaiting;
  return true;
}

/** A WINDOW THIS INTERPRETER OPENED, as the extension holds it. */
class LiveWindow {
 public:
  LiveWindow(int width, int height, const std::string& title, bool headless) {
    WindowOptions options;
    options.width = width;
    options.height = height;
    options.title = title;
    options.headless = headless;
    std::string why;
    Window* opened = windowDoor()(&options, &why);
    if (!opened)
      throw std::runtime_error("sigil.live could not open a window: " + why);
    m_window = std::shared_ptr<Window>(opened);
    const std::lock_guard lock(openMutex());
    openWindows().push_back(m_window);
  }

  ~LiveWindow() { close(); }

  /** Opens a session on a body @p factory makes and shows it. */
  void show(py::handle factory, bool scene3d, bool needsDevice,
            const std::optional<std::filesystem::path>& assets) {
    Window& window = opened();
    Host::Options options;
    options.kind = held(factory, scene3d, needsDevice);
    options.pythonLoader = &load;
    if (assets) options.assetsDirectory = std::filesystem::absolute(*assets);
    auto host = std::make_unique<Host>(std::move(options), processFonts());
    window.show(std::move(host));
  }

  bool pump(double seconds) {
    if (!m_window) return false;
    if (!std::isfinite(seconds) || seconds < 0)
      throw std::invalid_argument(
          "seconds is a finite number at or above zero");
    const bool open = m_window->pump(
        std::chrono::milliseconds((long long)std::llround(seconds * 1000.0)));
    if (!open) forget(m_window);
    return open;
  }

  bool still(const std::filesystem::path& path, float density) {
    if (!std::isfinite(density) || density <= 0)
      throw std::invalid_argument("density is a finite number above zero");
    return opened().still(std::filesystem::absolute(path), density);
  }

  bool restart() {
    Host* host = opened().host();
    return host && host->restartSession();
  }

  void close() {
    if (!m_window) return;
    forget(m_window);
    m_window->close();
    m_window.reset();
  }

  [[nodiscard]] bool isOpen() const { return m_window && m_window->isOpen(); }

  [[nodiscard]] bool live() const {
    Host* host = m_window ? m_window->host() : nullptr;
    return host && host->live();
  }

  [[nodiscard]] std::string status() const {
    Host* host = m_window ? m_window->host() : nullptr;
    return host ? host->status() : std::string{};
  }

  [[nodiscard]] std::string errorLog() const {
    Host* host = m_window ? m_window->host() : nullptr;
    return host ? host->errorLog() : std::string{};
  }

 private:
  Window& opened() {
    if (!m_window) throw std::runtime_error("this window has been closed");
    return *m_window;
  }

  std::shared_ptr<Window> m_window;
};

}  // namespace

void bindSketchLive(py::module_& module) {
  auto live = module.def_submodule(
      "live", "A window in this interpreter, showing a sketch it holds.");
  py::class_<LiveWindow>(
      live, "Window",
      "A window this interpreter opened: one live host, drawn while the "
      "window is pumped, on the thread that opened it.")
      .def(py::init<int, int, const std::string&, bool>(), py::arg("width"),
           py::arg("height"), py::kw_only(), py::arg("title") = "Sigil",
           py::arg("headless") = false)
      .def("show", &LiveWindow::show, py::arg("factory"), py::kw_only(),
           py::arg("scene3d") = false, py::arg("needsDevice") = false,
           py::arg("assets") = py::none(),
           "Opens a session on the body factory() answers and shows it.")
      .def("pump", &LiveWindow::pump, py::arg("seconds") = 0.0,
           py::call_guard<py::gil_scoped_release>(),
           "Turns the window once, handling its events for up to seconds; "
           "answers whether it is still open.")
      .def("still", &LiveWindow::still, py::arg("path"),
           py::arg("density") = 1.0f,
           "Writes the sketch on screen to path as a picture.")
      .def("restart", &LiveWindow::restart,
           "Opens the shown sketch again from its setup.")
      .def("close", &LiveWindow::close, "Takes the window off screen.")
      .def_property_readonly("isOpen", &LiveWindow::isOpen,
                             "Whether the window is still open.")
      .def_property_readonly("live", &LiveWindow::live,
                             "Whether a sketch is drawing.")
      .def_property_readonly("status", &LiveWindow::status,
                             "What the host says it is doing.")
      .def_property_readonly("errorLog", &LiveWindow::errorLog,
                             "Why the sketch is not drawing, when it is not.");
  live.def("installPromptHook", &installPromptHook,
           "Draws every open window while the interpreter waits for a line "
           "of input; false where another library's hook is installed.");
  live.def(
      "pumpOpenWindows",
      [] {
        const py::gil_scoped_release released;
        return pumpOpenWindows();
      },
      "Turns every open window once; false when none is open.");
}

}  // namespace sigil::sketch::python
