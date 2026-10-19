#include <pybind11/stl.h>
#include <sigilcompose/kit/Feed.h>
#include <sigildata/connection/Connection.h>
#include <sigilpython/compose/Kit.h>
#include <sigilpython/data/Convert.h>
#include <sigilpython/io/Hub.h>
#include <sigilsketch/kit/Channel.h>
#include <sigilsketch/kit/Connection.h>
#include <sigilsketch/kit/Console.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "Registration.h"

namespace sigil::sketch::python {
namespace py = pybind11;
namespace sketchKit = sigil::sketch::kit;
using sigil::python::kit::field;
using sigil::python::kit::record;

void bindSketchKitStreams(py::module_& root) {
  auto module = root.attr("sketch").attr("kit").cast<py::module_>();

  auto console = record<sketchKit::Console>(module, "Console");
  console.doc() =
      "The log panel a study prints into: its rings, set in one monospaced "
      "voice on one bordered plate, so many feeds to a column, so many "
      "rows of each, and the colours a row may name. The rings themselves "
      "are handed to console(), which reads their rows as it builds.";
  field(console, "stacked", &sketchKit::Console::stacked);
  field(console, "visible", &sketchKit::Console::visible);
  field(console, "levels", &sketchKit::Console::levels);
  field(console, "size", &sketchKit::Console::size);
  field(console, "ink", &sketchKit::Console::ink);
  field(console, "ground", &sketchKit::Console::ground);
  field(console, "border", &sketchKit::Console::border);
  module.def(
      "console",
      [](sketchKit::Console panel,
         const std::vector<const compose::feed::TextRing*>& feeds) {
        panel.feeds = feeds;
        return sketchKit::console(panel);
      },
      py::arg("panel"), py::arg("feeds"),
      "The panel over the rings given, in reading order: their rows as "
      "they stand when it is called, each column a ring or a stack of "
      "them.");

  auto readout = record<sketchKit::ConnectionReadout>(module, "ConnectionReadout");
  readout.doc() =
      "How a door's readout is set: what its first row calls the door "
      "where the transport bound no address, the rows' measure and air, "
      "and the readings the sketch sets after the door's own.";
  field(readout, "door", &sketchKit::ConnectionReadout::door);
  field(readout, "rows", &sketchKit::ConnectionReadout::rows);
  field(readout, "readings", &sketchKit::ConnectionReadout::readings);
  module.def(
      "connectionReadout",
      [](py::handle connection, const sketchKit::ConnectionReadout& how) {
        return sketchKit::connectionReadout(
            *sigil::python::connectionOf(connection), how);
      },
      py::arg("connection"), py::arg("how") = sketchKit::ConnectionReadout{},
      "The readout of a connection's state as it stands when called: the "
      "door, whether it is connecting, open or closed, its revision, the "
      "arrivals it dropped and those that were no message, and the sender "
      "of the newest — or the error the door reports — then the sketch's "
      "own readings.");

  // The channel keeps the Python hub and connection it was opened on
  // alive for as long as itself: the native channel names both and owns
  // neither.
  py::class_<sketchKit::Channel>(module, "Channel",
                                 "One number of a connection's newest "
                                 "message of one name, as the live value a "
                                 "binding reads.")
      .def(py::init<>())
      .def(py::init([](const sigil::python::HubHandle& hub,
                       py::handle connection, std::string name,
                       py::handle reading) {
             if (py::isinstance<py::bool_>(reading))
               throw py::type_error(
                   "A channel reads an argument's index or a field's name, "
                   "never a boolean.");
             sketchKit::Channel::Reading at =
                 py::isinstance<py::str>(reading)
                     ? sketchKit::Channel::Reading(reading.cast<std::string>())
                     : sketchKit::Channel::Reading(reading.cast<size_t>());
             return sketchKit::Channel(
                 hub.get(), *sigil::python::connectionOf(connection),
                 std::move(name), std::move(at));
           }),
           py::arg("hub"), py::arg("connection"), py::arg("name"),
           py::arg("reading"), py::keep_alive<1, 2>(), py::keep_alive<1, 3>(),
           "Follows the number `reading` names — an argument's index or a "
           "field's name — in the newest message called `name` on "
           "`connection`, driven by `hub`'s advance.")
      .def("live", &sketchKit::Channel::live,
           "The live value a binding reads; every copy reads the same cell.")
      .def("value", &sketchKit::Channel::value)
      .def("lastRead", &sketchKit::Channel::lastRead,
           "The number as the wire spelled it, or None until one arrived.")
      .def("name", &sketchKit::Channel::name);
}

}  // namespace sigil::sketch::python
