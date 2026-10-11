/** @file
 * The door read as values: data.connect(hub, uri) and data.replay(...),
 * the Connection handle, the Message it answers and the state it
 * reports. A handler is a Python callable run on the advancing thread,
 * with the interpreter taken for it and let go around every native call
 * that may block.
 */

#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigildata/connection/Connection.h>
#include <sigilio/hub/Hub.h>
#include <sigilpython/data/Convert.h>
#include <sigilpython/data/Registration.h>
#include <sigilpython/io/Hub.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {

template <class Function>
decltype(auto) unlocked(Function&& function) {
  if (Py_IsInitialized() && PyGILState_Check()) {
    const py::gil_scoped_release release;
    return std::forward<Function>(function)();
  }
  return std::forward<Function>(function)();
}

/** A connection opened through a hub handle. On an owned hub it keeps
 *  the hub and the connection alive as long as itself. On the host's hub
 *  the session owns the connection and this keeps it only weakly, so
 *  closing the session ends the connection's door and inlet even when
 *  Python kept the wrapper; the wrapper then refuses. */
class ConnectionHandle {
 public:
  ConnectionHandle(data::Connection connection, const HubHandle& hub)
      : m_hub(hub.owner()), m_check(hub.access()) {
    auto held = std::make_shared<data::Connection>(std::move(connection));
    if (m_check) (void)hub.retain(held->feed());
    if (m_check && hub.hold(held))
      m_borrowed = held;
    else
      m_owned = std::move(held);
  }
  ~ConnectionHandle() {
    // Letting go of the last owner may close the door, which may join a
    // transport's thread: never with the interpreter held.
    unlocked([&] {
      m_owned.reset();
      m_hub.reset();
    });
  }
  std::shared_ptr<data::Connection> get() const {
    if (m_owned) return m_owned;
    if (m_check) (void)m_check();
    std::shared_ptr<data::Connection> held = m_borrowed.lock();
    if (!held)
      throw std::runtime_error("This connection belongs to a closed session");
    return held;
  }

 private:
  std::shared_ptr<io::Hub> m_hub;
  std::function<io::Hub&()> m_check;
  /** The connection itself, on a hub this wrapper owns. */
  std::shared_ptr<data::Connection> m_owned;
  /** The session's connection, on the host's hub; it expires with the
   *  session. */
  std::weak_ptr<data::Connection> m_borrowed;
};

/** A Python callable held by native code: taken and let go only with the
 *  interpreter held, whichever thread drops the last native reference. */
data::Connection::Handler handlerOf(py::function function) {
  auto held = std::shared_ptr<py::function>(
      new py::function(std::move(function)), [](py::function* callable) {
        const py::gil_scoped_acquire acquire;
        delete callable;
      });
  return [held](const data::Message& message) {
    const py::gil_scoped_acquire acquire;
    (*held)(message);
  };
}

data::ConnectOptions optionsOf(size_t capacity, std::string peer,
                               std::optional<data::Dialect> dialect,
                               std::optional<data::Schema> schema, bool queue) {
  return {.capacity = capacity,
          .peer = std::move(peer),
          .dialect = dialect,
          .schema = schema.value_or(data::Schema{}),
          .queue = queue};
}

}  // namespace

void bindDataConnection(py::module_& root) {
  auto module = root.def_submodule("data");

  py::class_<data::ConnectionState>(module, "ConnectionState")
      .def_readonly("readiness", &data::ConnectionState::readiness)
      .def_readonly("revision", &data::ConnectionState::revision)
      .def_readonly("dropped", &data::ConnectionState::dropped)
      .def_readonly("localAddress", &data::ConnectionState::localAddress)
      .def_readonly("error", &data::ConnectionState::error)
      .def_readonly("undecodable", &data::ConnectionState::undecodable)
      .def("isOpen", &data::ConnectionState::isOpen)
      .def(py::self == py::self)
      .def(py::self != py::self);

  py::class_<data::Message>(module, "Message")
      .def(py::init<>())
      .def_property_readonly(
          "payload", [](const data::Message& value) { return value.payload; })
      .def(
          "__getitem__",
          [](const data::Message& value, py::handle key) -> data::Json {
            if (py::isinstance<py::str>(key))
              return value[py::cast<std::string>(key)];
            if (!py::isinstance<py::int_>(key))
              throw py::type_error("A message index needs text or an integer.");
            const auto index = py::cast<py::ssize_t>(key);
            return index < 0 ? data::Json{} : value[static_cast<size_t>(index)];
          },
          py::arg("key"))
      .def("number", &data::Message::number, py::arg("fallback") = 0)
      .def(
          "string",
          [](const data::Message& value, std::string_view fallback) {
            return std::string(value.string(fallback));
          },
          py::arg("fallback") = "")
      .def("boolean", &data::Message::boolean, py::arg("fallback") = false)
      .def("address",
           [](const data::Message& value) {
             return std::string(value.address());
           })
      .def("name", &data::Message::name)
      .def("sender", &data::Message::sender)
      .def("arrivedAt",
           [](const data::Message& value) { return value.arrivedAt().count(); })
      .def("receivedAt",
           [](const data::Message& value) {
             return std::chrono::duration<double>(
                        value.receivedAt().time_since_epoch())
                 .count();
           })
      .def("revision", &data::Message::revision)
      .def("bytes",
           [](const data::Message& value) {
             const auto& bytes = value.bytes();
             if (!bytes) return py::bytes();
             const std::string_view text = bytes->asText();
             return py::bytes(text.data(), text.size());
           })
      .def("empty", &data::Message::empty)
      .def("to_python", [](const data::Message& value) {
        return dataPython(value.payload);
      });

  py::class_<ConnectionHandle>(module, "Connection")
      .def(
          "latest",
          [](ConnectionHandle& value, std::optional<std::string> address) {
            const auto connection = value.get();
            return address ? connection->latest(*address)
                           : connection->latest();
          },
          py::arg("address") = py::none())
      .def("receive",
           [](ConnectionHandle& value) { return value.get()->receive(); })
      .def(
          "on",
          [](ConnectionHandle& value, const std::string& pattern,
             py::function handler) {
            value.get()->on(pattern, handlerOf(std::move(handler)));
          },
          py::arg("pattern"), py::arg("handler"))
      .def(
          "otherwise",
          [](ConnectionHandle& value, py::function handler) {
            value.get()->otherwise(handlerOf(std::move(handler)));
          },
          py::arg("handler"))
      .def(
          "send",
          [](ConnectionHandle& value, py::handle message, const std::string& to,
             bool reply) {
            const data::Json written = dataJson(message);
            const auto connection = value.get();
            return unlocked([&] {
              return reply ? connection->reply(written)
                           : connection->send(written, {.to = to});
            });
          },
          py::arg("message"), py::arg("to") = "", py::arg("reply") = false)
      .def(
          "reply",
          [](ConnectionHandle& value, py::handle message) {
            const data::Json written = dataJson(message);
            const auto connection = value.get();
            return unlocked([&] { return connection->reply(written); });
          },
          py::arg("message"))
      .def(
          "record",
          [](ConnectionHandle& value, const std::filesystem::path& path) {
            const auto connection = value.get();
            return unlocked([&] { return connection->record(path); });
          },
          py::arg("path"))
      .def("state",
           [](ConnectionHandle& value) { return value.get()->state(); })
      .def("close",
           [](ConnectionHandle& value) {
             const auto connection = value.get();
             unlocked([&] { connection->close(); });
           })
      .def("uri", [](ConnectionHandle& value) { return value.get()->uri(); });

  module.def(
      "connect",
      [](const HubHandle& hub, const std::string& uri, size_t capacity,
         std::string peer, std::optional<data::Dialect> dialect,
         std::optional<data::Schema> schema, bool queue) {
        auto& native = hub.get();
        data::ConnectOptions options = optionsOf(
            capacity, std::move(peer), dialect, std::move(schema), queue);
        return ConnectionHandle(
            unlocked(
                [&] { return data::connect(native, uri, std::move(options)); }),
            hub);
      },
      py::arg("hub"), py::arg("uri"), py::arg("capacity") = 256,
      py::arg("peer") = "", py::arg("dialect") = py::none(),
      py::arg("schema") = py::none(), py::arg("queue") = false);
  module.def(
      "replay",
      [](const HubHandle& hub, const std::string& uri,
         const std::filesystem::path& recording, size_t capacity,
         std::string peer, std::optional<data::Dialect> dialect,
         std::optional<data::Schema> schema, bool queue) {
        auto& native = hub.get();
        data::ConnectOptions options = optionsOf(
            capacity, std::move(peer), dialect, std::move(schema), queue);
        return ConnectionHandle(unlocked([&] {
                                  return data::replay(native, uri,
                                                      recording.string(),
                                                      std::move(options));
                                }),
                                hub);
      },
      py::arg("hub"), py::arg("uri"), py::arg("recording"),
      py::arg("capacity") = 256, py::arg("peer") = "",
      py::arg("dialect") = py::none(), py::arg("schema") = py::none(),
      py::arg("queue") = false);
  module.def("matchesAddress", &data::matchesAddress, py::arg("pattern"),
             py::arg("name"));
}

}  // namespace sigil::python
