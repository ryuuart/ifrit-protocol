#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilimage/decode/Decoders.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>
#include <sigilio/testing/Testing.h>
#include <sigilio/transport/Transport.h>
#include <sigilpython/Extend.h>
#include <sigilpython/data/Convert.h>
#include <sigilpython/io/Hub.h>
#include <sigilpython/io/Registration.h>
#include <sigilpython/skia/Values.h>

#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

namespace sigil::python {
namespace py = pybind11;
namespace {

/** The native transport owns no Python callbacks. Blocking native work can
 * run without the GIL, including destruction of the last transport owner. */
template <class Function>
decltype(auto) unlocked(Function&& function) {
  if (Py_IsInitialized() && PyGILState_Check()) {
    const py::gil_scoped_release release;
    return std::forward<Function>(function)();
  }
  return std::forward<Function>(function)();
}

io::Bytes bytes(py::handle value) {
  Py_buffer view{};
  if (PyObject_GetBuffer(value.ptr(), &view, PyBUF_CONTIG_RO) != 0)
    throw py::error_already_set();
  struct Release {
    Py_buffer* view;
    ~Release() { PyBuffer_Release(view); }
  } release{&view};
  return io::Bytes(std::span(static_cast<const std::byte*>(view.buf),
                            static_cast<size_t>(view.len)));
}

py::object copiedBytes(const std::shared_ptr<const io::Bytes>& value) {
  if (!value) return py::none();
  return py::bytes(value->empty()
                       ? ""
                       : reinterpret_cast<const char*>(value->data()),
                   value->size());
}

void validTime(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw py::value_error("Feed times must be finite and nonnegative");
}

class ResourceHandle {
 public:
  ResourceHandle(HubHandle hub, const std::vector<std::string>& selectors)
      : m_hub(std::move(hub)) {
    auto& native = m_hub.get();
    std::vector<std::string_view> views(selectors.begin(), selectors.end());
    m_lease.emplace(unlocked([&] { return native.retain(views); }));
  }
  ~ResourceHandle() {
    unlocked([&] { m_lease.reset(); });
  }
  io::ResourceLease& get() {
    checkThread();
    m_hub.get();
    if (!m_lease) throw std::runtime_error("Resource lease is closed.");
    return *m_lease;
  }
  void close() {
    checkThread();
    unlocked([&] { m_lease.reset(); });
  }

 private:
  void checkThread() const {
    if (m_thread != std::this_thread::get_id())
      throw std::runtime_error(
          "Resource lease belongs to its creating thread.");
  }
  const std::thread::id m_thread = std::this_thread::get_id();
  HubHandle m_hub;
  std::optional<io::ResourceLease> m_lease;
};

class FeedHandle {
 public:
  FeedHandle(std::string uri, io::ListenOptions options)
      : m_feed(std::move(uri), std::move(options)) {}
  FeedHandle(io::Feed feed, const HubHandle& hub)
      : m_hub(hub.owner()), m_check(hub.access()), m_feed(std::move(feed)) {
    if (m_check) m_lease = hub.retain(m_feed);
  }
  ~FeedHandle() {
    // Letting go of the last handle closes the door, which may join a
    // transport's thread: never with the interpreter held.
    unlocked([&] {
      m_feed = {};
      m_hub.reset();
    });
  }
  io::Feed get() const {
    if (m_check) {
      (void)m_check();
      if (m_lease.expired())
        throw std::runtime_error("This feed belongs to a closed session");
    }
    return m_feed;
  }

 private:
  std::shared_ptr<io::Hub> m_hub;
  std::function<io::Hub&()> m_check;
  io::Feed m_feed;
  /** The session's lease on a borrowed feed; it expires with the session. */
  std::weak_ptr<void> m_lease;
};

struct FeedLease;
struct FeedLeases {
  std::mutex mutex;
  std::unordered_map<io::Feed, std::weak_ptr<FeedLease>> entries;
};
FeedLeases& feedLeases() {
  static FeedLeases leases;
  return leases;
}
struct FeedLease {
  explicit FeedLease(io::Feed value)
      : feed(std::move(value)) {}
  ~FeedLease() {
    unlocked([&] {
      auto& leases = feedLeases();
      const std::lock_guard lock(leases.mutex);
      const auto found = leases.entries.find(feed);
      // A new lease may have taken ownership while this last shared owner
      // entered destruction. Its live lease owns the transport from here.
      if (found != leases.entries.end() && !found->second.expired()) return;
      if (found != leases.entries.end()) leases.entries.erase(found);
      feed.close();
      feed = {};
    });
  }
  io::Feed feed;
};

template <class Native>
struct Serialized {
  template <class... Args>
  explicit Serialized(Args&&... args)
      : value(std::make_unique<Native>(std::forward<Args>(args)...)) {}
  ~Serialized() {
    unlocked([&] { value.reset(); });
  }
  std::mutex mutex;
  std::unique_ptr<Native> value;
};
using RecordingHandle = Serialized<io::RecordingWriter>;
using SharedMemoryHandle = Serialized<io::SharedMemoryWriter>;

}  // namespace

HubHandle::HubHandle() : m_owner(std::make_shared<io::Hub>()) {}
HubHandle::HubHandle(std::function<io::Hub&()> access,
                     std::function<std::shared_ptr<void>(io::Feed)> retainFeed)
    : m_access(std::move(access)), m_retainFeed(std::move(retainFeed)) {}
HubHandle::~HubHandle() {
  unlocked([&] { m_owner.reset(); });
}
io::Hub& HubHandle::get() const { return m_access ? m_access() : *m_owner; }
std::shared_ptr<void> HubHandle::retain(const io::Feed& feed) const {
  return m_retainFeed ? m_retainFeed(feed) : nullptr;
}

std::shared_ptr<void> retainSessionFeed(io::Feed feed) {
  auto& leases = feedLeases();
  const std::lock_guard lock(leases.mutex);
  auto& entry = leases.entries[feed];
  if (auto lease = entry.lock()) return lease;
  auto lease = std::make_shared<FeedLease>(std::move(feed));
  entry = lease;
  return lease;
}

void bindIO(py::module_& module) {
  auto resources = module.def_submodule("io");
  py::class_<ResourceHandle>(resources, "ResourceLease")
      .def(
          "include",
          [](ResourceHandle& lease, const std::string& selector) {
            auto& value = lease.get();
            return unlocked([&] { return value.include(selector); });
          },
          py::arg("selector"))
      .def("refresh",
           [](ResourceHandle& lease) {
             auto& value = lease.get();
             return unlocked([&] { return value.refresh(); });
           })
      .def("preload",
           [](ResourceHandle& lease) {
             auto& value = lease.get();
             return unlocked([&] { return value.preload(); });
           })
      .def("uris",
           [](ResourceHandle& lease) {
             auto uris = lease.get().uris();
             return std::vector<std::string>(uris.begin(), uris.end());
           })
      .def("close", &ResourceHandle::close)
      .def(
          "__enter__",
          [](ResourceHandle& lease) -> ResourceHandle& {
            lease.get();
            return lease;
          },
          py::return_value_policy::reference_internal)
      .def(
          "__exit__",
          [](ResourceHandle& lease, py::object, py::object, py::object) {
            lease.close();
          },
          py::arg("exc_type"), py::arg("exc_value"), py::arg("traceback"));
  py::class_<io::ResourceInfo>(resources, "ResourceInfo")
      .def_readonly("byteSize", &io::ResourceInfo::byteSize)
      .def_readonly("path", &io::ResourceInfo::path);
  py::enum_<io::NetworkPolicy>(resources, "NetworkPolicy")
      .value("CacheFirst", io::NetworkPolicy::CacheFirst)
      .value("Refresh", io::NetworkPolicy::Refresh)
      .value("Offline", io::NetworkPolicy::Offline);
  py::class_<io::ListenOptions>(resources, "ListenOptions")
      .def(py::init([](size_t capacity, std::string peer) {
             return io::ListenOptions{capacity, std::move(peer)};
           }),
           py::arg("capacity") = 256, py::arg("peer") = "")
      .def_readwrite("capacity", &io::ListenOptions::capacity)
      .def_readwrite("peer", &io::ListenOptions::peer);
  py::enum_<io::ReadyState>(resources, "ReadyState")
      .value("Connecting", io::ReadyState::Connecting)
      .value("Open", io::ReadyState::Open)
      .value("Closed", io::ReadyState::Closed);
  py::class_<io::FeedState>(resources, "FeedState")
      .def_readonly("readiness", &io::FeedState::readiness)
      .def_readonly("revision", &io::FeedState::revision)
      .def_readonly("dropped", &io::FeedState::dropped)
      .def_readonly("localAddress", &io::FeedState::localAddress)
      .def_readonly("error", &io::FeedState::error)
      .def("isOpen", &io::FeedState::isOpen)
      .def(py::self == py::self)
      .def(py::self != py::self);
  py::class_<io::Message>(resources, "Message")
      .def(py::init([](py::handle payload, std::string sender,
                       double arrivedAt, uint64_t revision) {
             return io::Message(
                 std::make_shared<const io::Bytes>(bytes(payload)),
                 std::move(sender), std::chrono::duration<double>(arrivedAt),
                 revision);
           }),
           py::arg("payload") = py::bytes(), py::arg("sender") = "",
           py::arg("arrivedAt") = 0, py::arg("revision") = 0)
      .def_property_readonly("revision", &io::Message::revision)
      .def_property_readonly(
          "arrivedAt",
          [](const io::Message& value) { return value.arrivedAt().count(); })
      .def_property_readonly("sender", &io::Message::sender)
      .def_property(
          "payload",
          [](const io::Message& value) {
            return value.payload
                       ? py::cast<py::bytes>(copiedBytes(value.payload))
                       : py::bytes();
          },
          [](io::Message& value, py::handle payload) {
            value.payload = std::make_shared<const io::Bytes>(bytes(payload));
          });
  py::class_<io::Recording>(resources, "Recording")
      .def("stop",
           [](io::Recording& recording) {
             unlocked([&] { recording.stop(); });
           })
      .def("stopped", &io::Recording::stopped)
      .def(
          "__enter__",
          [](io::Recording& recording) -> io::Recording& { return recording; },
          py::return_value_policy::reference_internal)
      .def(
          "__exit__",
          [](io::Recording& recording, py::object, py::object, py::object) {
            unlocked([&] { recording.stop(); });
          },
          py::arg("exc_type"), py::arg("exc_value"), py::arg("traceback"));
  py::class_<FeedHandle>(resources, "Feed")
      .def(py::init<std::string, io::ListenOptions>(), py::arg("uri"),
           py::arg("options") = io::ListenOptions{})
#define SIGIL_FEED_METHOD(name)                    \
  .def(#name, [](const FeedHandle& value) {        \
    auto feed = value.get();                       \
    return unlocked([&] { return feed.name(); }); \
  })
          SIGIL_FEED_METHOD(receive) SIGIL_FEED_METHOD(latest)
              SIGIL_FEED_METHOD(state) SIGIL_FEED_METHOD(uri)
                  SIGIL_FEED_METHOD(close)
#undef SIGIL_FEED_METHOD
      .def(
          "send",
          [](const FeedHandle& value, py::handle payload,
             const std::string& to) {
            const auto copied = bytes(payload);
            auto feed = value.get();
            return unlocked(
                [&] { return feed.send(copied, {.to = to}); });
          },
          py::arg("payload"), py::arg("to") = "")
      .def(
          "record",
          [](const FeedHandle& value, const std::filesystem::path& path) {
            auto feed = value.get();
            return unlocked([&] { return feed.record(path); });
          },
          py::arg("path"))
      .def(
          "__eq__",
          [](const FeedHandle& left, const FeedHandle& right) {
            return left.get() == right.get();
          },
          py::is_operator(), py::arg("other"));

  // A test that stands in for a transport puts messages on a feed it holds
  // through the feed's inlet, the one way in a reader does not have.
  auto testing = resources.def_submodule("testing");
  py::class_<io::Inlet>(testing, "Inlet")
      .def(
          "deliver",
          [](const io::Inlet& inlet, py::handle payload,
             const std::string& sender) {
            auto copied = bytes(payload);
            unlocked([&] { inlet.deliver(std::move(copied), sender); });
          },
          py::arg("bytes"), py::arg("sender") = "")
      .def(
          "deliver",
          [](const io::Inlet& inlet, py::handle payload, double arrivedAt) {
            validTime(arrivedAt);
            auto copied = bytes(payload);
            unlocked([&] { inlet.deliver(std::move(copied),
                                        std::chrono::duration<double>(arrivedAt)); });
          },
          py::arg("bytes"), py::arg("arrivedAt"))
      .def(
          "fail",
          [](const io::Inlet& inlet, std::string why) {
            unlocked([&] { inlet.fail(std::move(why)); });
          },
          py::arg("why"))
      .def("expired", &io::Inlet::expired);
  testing.def(
      "inletOf",
      [](const FeedHandle& value) { return io::testing::inletOf(value.get()); },
      py::arg("feed"));

  py::class_<HubHandle>(resources, "Hub")
      .def(py::init<>())
      .def(
          "load",
          [](const HubHandle& value, py::handle type,
             const std::string& uri) -> py::object {
            auto& hub = value.get();
            if (type.is(py::type::of<image::ImageAsset>())) {
              auto asset = unlocked(
                  [&] { return hub.load<image::ImageAsset>(uri); });
              return asset ? py::cast(*asset) : py::none();
            }
            return loadData(hub, type, uri);
          },
          py::arg("type"), py::arg("uri"))
      .def(
          "probe",
          [](const HubHandle& value, py::handle type,
             const std::string& uri) -> py::object {
            if (!type.is(py::type::of<io::ResourceInfo>()))
              throw py::type_error(
                  "Hub.probe answers ResourceInfo; what bytes mean is "
                  "loaded through the library that owns the meaning");
            auto& hub = value.get();
            auto info =
                unlocked([&] { return hub.probe<io::ResourceInfo>(uri); });
            return info ? py::cast(*info) : py::none();
          },
          py::arg("type"), py::arg("uri"))
      .def(
          "retain",
          [](const HubHandle& value, const std::string& selector) {
            return std::make_unique<ResourceHandle>(
                value, std::vector<std::string>{selector});
          },
          py::arg("selector"))
      .def(
          "retain",
          [](const HubHandle& value,
             const std::vector<std::string>& selectors) {
            return std::make_unique<ResourceHandle>(value, selectors);
          },
          py::arg("selectors") = std::vector<std::string>{})
      .def(
          "preload",
          [](const HubHandle& value, const std::string& selector) {
            auto& hub = value.get();
            return unlocked([&] { return hub.preload(selector); });
          },
          py::arg("selector"))
      .def(
          "preload",
          [](const HubHandle& value, const std::vector<std::string>& uris) {
            auto& hub = value.get();
            return unlocked([&] {
              return hub.preload(std::span<const std::string>(uris));
            });
          },
          py::arg("uris"))
      .def("discardUnretained",
           [](const HubHandle& value) {
             auto& hub = value.get();
             return unlocked([&] { return hub.discardUnretained(); });
           })
      .def(
          "mount",
          [](const HubHandle& value, std::string prefix,
             std::filesystem::path path) {
            auto& hub = value.get();
            unlocked([&] { hub.mount(std::move(prefix), std::move(path)); });
          },
          py::arg("prefix"), py::arg("path"))
#define SIGIL_HUB_URI(name)                                \
  .def(                                                    \
      #name,                                               \
      [](const HubHandle& value, const std::string& uri) { \
        auto& hub = value.get();                           \
        return unlocked([&] { return hub.name(uri); });    \
      },                                                   \
      py::arg("uri"))
          SIGIL_HUB_URI(resolve) SIGIL_HUB_URI(text) SIGIL_HUB_URI(select)
#undef SIGIL_HUB_URI
      .def(
          "fetch",
          [](const HubHandle& value, const std::string& uri) {
            auto& hub = value.get();
            return copiedBytes(unlocked([&] { return hub.fetch(uri); }));
          },
          py::arg("uri"))
      .def(
          "write",
          [](const HubHandle& value, const std::string& uri,
             py::handle payload) {
            const auto copied = bytes(payload);
            auto& hub = value.get();
            return unlocked([&] { return hub.write(uri, copied); });
          },
          py::arg("uri"), py::arg("bytes"))
      .def(
          "listen",
          [](const HubHandle& value, const std::string& uri,
             io::ListenOptions options) {
            auto& hub = value.get();
            return FeedHandle(unlocked([&] { return hub.listen(uri, options); }),
                              value);
          },
          py::arg("uri"), py::arg("options") = io::ListenOptions{})
      .def(
          "replay",
          [](const HubHandle& value, const std::string& uri,
             const std::filesystem::path& recording, io::ListenOptions options) {
            auto& hub = value.get();
            return FeedHandle(unlocked([&] {
                                return hub.replay(uri, recording.string(),
                                                  options);
                              }),
                              value);
          },
          py::arg("uri"), py::arg("recording"),
          py::arg("options") = io::ListenOptions{})
      .def("feeds",
           [](const HubHandle& value) {
             auto& hub = value.get();
             auto feeds = unlocked([&] { return hub.feeds(); });
             std::vector<FeedHandle> result;
             result.reserve(feeds.size());
             for (auto& feed : feeds)
               result.emplace_back(std::move(feed), value);
             return result;
           })
      .def("advance",
           [](const HubHandle& value) {
             auto& hub = value.get();
             unlocked([&] { hub.advance(); });
           })
      .def(
          "advance",
          [](const HubHandle& value, double time) {
            validTime(time);
            auto& hub = value.get();
            unlocked([&] {
              hub.advance(std::chrono::duration<double>(time));
            });
          },
          py::arg("time"))
      .def("poll",
           [](const HubHandle& value) {
             auto& hub = value.get();
             return unlocked([&] { return hub.poll(); });
           })
      .def(
          "setNetworkCacheDirectory",
          [](const HubHandle& value, std::filesystem::path path) {
            auto& hub = value.get();
            unlocked([&] { hub.setNetworkCacheDirectory(std::move(path)); });
          },
          py::arg("path"))
      .def(
          "setNetworkPolicy",
          [](const HubHandle& value, io::NetworkPolicy policy) {
            auto& hub = value.get();
            unlocked([&] { hub.setNetworkPolicy(policy); });
          },
          py::arg("policy"));

  resources.def(
      "registerTransports",
      [](const HubHandle& value, const std::vector<std::string>& schemes) {
        auto& hub = value.get();
        unlocked([&] { io::registerTransports(hub, schemes); });
      },
      py::arg("hub"), py::arg("schemes") = std::vector<std::string>{});

  resources.def("readRecording", &io::readRecording, py::arg("path"),
                py::call_guard<py::gil_scoped_release>());
  py::class_<RecordingHandle>(resources, "RecordingWriter")
      .def(py::init<const std::filesystem::path&>(), py::arg("path"),
           py::call_guard<py::gil_scoped_release>())
      .def(
          "append",
          [](RecordingHandle& writer, io::Message message) {
            validTime(message.arrivedAt().count());
            return unlocked([&] {
              const std::lock_guard lock(writer.mutex);
              return writer.value->append(message);
            });
          },
          py::arg("message"))
      .def("good", [](RecordingHandle& writer) {
        return unlocked([&] {
          const std::lock_guard lock(writer.mutex);
          return writer.value->good();
        });
      });
  py::class_<SharedMemoryHandle>(resources, "SharedMemoryWriter")
      .def(py::init<std::string_view, size_t>(), py::arg("name"),
           py::arg("capacity"), py::call_guard<py::gil_scoped_release>())
      .def("open",
           [](SharedMemoryHandle& writer) {
             return unlocked([&] {
               const std::lock_guard lock(writer.mutex);
               return writer.value->open();
             });
           })
      .def(
          "write",
          [](SharedMemoryHandle& writer, py::handle payload) {
            const auto copied = bytes(payload);
            return unlocked([&] {
              const std::lock_guard lock(writer.mutex);
              return writer.value->write(copied);
            });
          },
          py::arg("bytes"));

  // SigilImage puts its own decoders on a hub, as SigilData does: an
  // owned hub loads an ImageAsset once this has run, and a session's hub
  // already has.
  submodule(module, "image")
      .def(
          "registerDecoders",
          [](const HubHandle& value) {
            auto& hub = value.get();
            unlocked([&] { image::registerDecoders(hub); });
          },
          py::arg("hub"));
}

}  // namespace sigil::python
