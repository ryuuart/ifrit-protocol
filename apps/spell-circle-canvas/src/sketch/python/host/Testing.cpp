/** @file
 * A sketch host in this process, spoken to as one over a socket is: the
 * in-process route Python's `sigil.testing` stands on.
 */

#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilsketch/python/Python.h>
#include <sigilsketch/testing/InProcessHost.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;

namespace {

/** The host a script opens: every registry entry this binary carries, the
 *  sketches directory it was built beside, and Python files through this
 *  interpreter's own loader. */
std::unique_ptr<testing::InProcessHost> openHost(
    const std::filesystem::path& state,
    const std::optional<std::filesystem::path>& sketches,
    const std::optional<std::filesystem::path>& assets, double patience) {
  if (!std::isfinite(patience) || patience <= 0)
    throw std::invalid_argument("patience is a finite number of seconds above zero");
  testing::InProcessHostOptions options;
  options.stateDirectory = std::filesystem::absolute(state);
  options.program = "sigil.testing";
  options.session.pythonLoader = &load;
  if (sketches) {
    options.session.sketchesDirectory = *sketches;
    options.catalog.sketchDirectory = *sketches;
  }
  if (assets) options.session.assetsDirectory = *assets;
  options.patience = std::chrono::milliseconds((long long)(patience * 1000.0));
  return std::make_unique<testing::InProcessHost>(std::move(options));
}

}  // namespace

void bindSketchTesting(py::module_& module) {
  auto tests = module.def_submodule(
      "testing",
      "A sketch host in this process, spoken to as one over a socket is.");
  py::class_<testing::InProcessHost>(
      tests, "InProcess",
      "A sketch host in this process: the registry, session and clock "
      "agents on one dispatcher, and one client attached with no socket. "
      "Every envelope sent is answered before send returns, the host's loop "
      "turned until it is.")
      .def(py::init(&openHost), py::arg("state"), py::kw_only(),
           py::arg("sketches") = py::none(), py::arg("assets") = py::none(),
           py::arg("patience") = 120.0)
      .def(
          "send",
          [](testing::InProcessHost& host, const std::string& envelope) {
            return host.send(envelope);
          },
          py::arg("envelope"),
          "One request's envelope text, answered as envelope text.")
      .def(
          "listen",
          [](testing::InProcessHost& host, const std::string& method,
             std::function<void(std::string)> listener) {
            host.caller().listen(
                method, [listener = std::move(listener)](
                            std::string_view parameters) {
                  const py::gil_scoped_acquire held;
                  listener(std::string(parameters));
                });
          },
          py::arg("method"), py::arg("listener"),
          "Hands every event of @p method, as its table's JSON text, to "
          "@p listener once its domain is enabled.")
      .def("frame", &testing::InProcessHost::frame,
           "One turn of the host's loop.")
      .def("readout", &testing::InProcessHost::readout,
           "host.describe's answer, the clock as it stands and the last "
           "still's path: what a failing case prints first.")
      .def_property_readonly(
          "state_directory",
          [](const testing::InProcessHost& host) {
            return host.stateDirectory();
          },
          "Where the host keeps its state and writes its stills.");
}

}  // namespace sigil::sketch::python
