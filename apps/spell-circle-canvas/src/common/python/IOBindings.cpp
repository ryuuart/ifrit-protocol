#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilio/hub/Feed.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>
#include <sigilio/transport/Transport.h>
#include <sigilpython/DataBindings.h>
#include <sigilpython/IOBindings.h>
#include <sigilpython/ValueBindings.h>

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
  io::Bytes result;
  result.bytes.resize(static_cast<size_t>(view.len));
  if (view.len) std::memcpy(result.bytes.data(), view.buf, result.bytes.size());
  return result;
}

py::object copiedBytes(const std::shared_ptr<const io::Bytes>& value) {
  if (!value) return py::none();
  return py::bytes(value->bytes.empty()
                       ? ""
                       : reinterpret_cast<const char*>(value->bytes.data()),
                   value->bytes.size());
}

void validTime(double seconds) {
  if (!std::isfinite(seconds) || seconds < 0)
    throw py::value_error("Feed times must be finite and nonnegative");
}

void validRecording(const std::vector<io::Arrival>& recording) {
  double previous = 0;
  for (const auto& arrival : recording) {
    validTime(arrival.at);
    if (arrival.at < previous)
      throw py::value_error("Recording arrivals must be ordered by time");
    previous = arrival.at;
  }
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
  FeedHandle(std::string uri, io::FeedPolicy policy)
      : m_owner(std::make_shared<io::Feed>(std::move(uri), policy)),
        m_feed(m_owner) {}
  FeedHandle(std::shared_ptr<io::Feed> feed, const HubHandle& hub)
      : m_hub(hub.owner()), m_check(hub.access()), m_feed(feed) {
    if (m_check)
      hub.retain(feed);
    else
      m_owner = std::move(feed);
  }
  ~FeedHandle() {
    unlocked([&] {
      m_owner.reset();
      m_hub.reset();
    });
  }
  std::shared_ptr<io::Feed> get() const {
    if (m_check) (void)m_check();
    auto result = m_feed.lock();
    if (!result)
      throw std::runtime_error("This feed belongs to a closed session");
    return result;
  }

 private:
  std::shared_ptr<io::Hub> m_hub;
  std::function<io::Hub&()> m_check;
  std::shared_ptr<io::Feed> m_owner;
  std::weak_ptr<io::Feed> m_feed;
};

struct FeedLease;
struct FeedLeases {
  std::mutex mutex;
  std::unordered_map<io::Feed*, std::weak_ptr<FeedLease>> entries;
};
FeedLeases& feedLeases() {
  static FeedLeases leases;
  return leases;
}
struct FeedLease {
  explicit FeedLease(std::shared_ptr<io::Feed> value)
      : feed(std::move(value)) {}
  ~FeedLease() {
    unlocked([&] {
      auto& leases = feedLeases();
      const std::lock_guard lock(leases.mutex);
      const auto found = leases.entries.find(feed.get());
      // A new lease may have taken ownership while this last shared owner
      // entered destruction. Its live lease owns the transport from here.
      if (found != leases.entries.end() && !found->second.expired()) return;
      if (found != leases.entries.end()) leases.entries.erase(found);
      feed->close();
      feed.reset();
    });
  }
  std::shared_ptr<io::Feed> feed;
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
                     std::function<void(std::shared_ptr<io::Feed>)> retainFeed)
    : m_access(std::move(access)), m_retainFeed(std::move(retainFeed)) {}
HubHandle::~HubHandle() {
  unlocked([&] { m_owner.reset(); });
}
io::Hub& HubHandle::get() const { return m_access ? m_access() : *m_owner; }
void HubHandle::retain(const std::shared_ptr<io::Feed>& feed) const {
  if (m_retainFeed) m_retainFeed(feed);
}

std::shared_ptr<void> retainSessionFeed(std::shared_ptr<io::Feed> feed) {
  auto& leases = feedLeases();
  const std::lock_guard lock(leases.mutex);
  auto& entry = leases.entries[feed.get()];
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
  py::class_<io::FeedPolicy>(resources, "FeedPolicy")
      .def(py::init([](size_t capacity) { return io::FeedPolicy{capacity}; }),
           py::arg("capacity") = 256)
      .def_readwrite("capacity", &io::FeedPolicy::capacity);
  py::class_<io::Arrival>(resources, "Arrival")
      .def(py::init([](double at, py::handle payload, std::string from,
                       uint64_t generation) {
             return io::Arrival{
                 generation, at,
                 std::make_shared<const io::Bytes>(bytes(payload)),
                 std::move(from)};
           }),
           py::arg("at") = 0, py::arg("bytes") = py::bytes(),
           py::arg("from_") = "", py::arg("generation") = 0)
      .def_readwrite("generation", &io::Arrival::generation)
      .def_readwrite("at", &io::Arrival::at)
      .def_readwrite("from_", &io::Arrival::from)
      .def_property(
          "bytes",
          [](const io::Arrival& value) {
            return value.bytes ? py::cast<py::bytes>(copiedBytes(value.bytes))
                               : py::bytes();
          },
          [](io::Arrival& value, py::handle payload) {
            value.bytes = std::make_shared<const io::Bytes>(bytes(payload));
          });
  py::class_<FeedHandle>(resources, "Feed")
      .def(py::init<std::string, io::FeedPolicy>(), py::arg("uri"),
           py::arg("policy") = io::FeedPolicy{})
#define SIGIL_FEED_METHOD(name)                    \
  .def(#name, [](const FeedHandle& value) {        \
    auto feed = value.get();                       \
    return unlocked([&] { return feed->name(); }); \
  })
          SIGIL_FEED_METHOD(receive) SIGIL_FEED_METHOD(newest)
              SIGIL_FEED_METHOD(generation) SIGIL_FEED_METHOD(dropped)
                  SIGIL_FEED_METHOD(closed) SIGIL_FEED_METHOD(opened)
                      SIGIL_FEED_METHOD(error) SIGIL_FEED_METHOD(address)
                          SIGIL_FEED_METHOD(uri) SIGIL_FEED_METHOD(close)
#undef SIGIL_FEED_METHOD
      .def(
          "fail",
          [](const FeedHandle& value, std::string why) {
            auto feed = value.get();
            unlocked([&] { feed->fail(std::move(why)); });
          },
          py::arg("why"))
      .def("latest",
           [](const FeedHandle& value) {
             auto feed = value.get();
             return copiedBytes(unlocked([&] { return feed->latest(); }));
           })
      .def(
          "send",
          [](const FeedHandle& value, py::handle payload) {
            const auto copied = bytes(payload);
            auto feed = value.get();
            return unlocked([&] { return feed->send(copied); });
          },
          py::arg("bytes"))
      .def(
          "sendTo",
          [](const FeedHandle& value, const std::string& to,
             py::handle payload) {
            const auto copied = bytes(payload);
            auto feed = value.get();
            return unlocked([&] { return feed->sendTo(to, copied); });
          },
          py::arg("to"), py::arg("bytes"))
      .def(
          "deliver",
          [](const FeedHandle& value, py::handle payload) {
            auto copied = bytes(payload);
            auto feed = value.get();
            unlocked([&] { feed->deliver(std::move(copied)); });
          },
          py::arg("bytes"))
      .def(
          "deliver",
          [](const FeedHandle& value, py::handle payload,
             const std::string& from) {
            auto copied = bytes(payload);
            auto feed = value.get();
            unlocked([&] { feed->deliver(std::move(copied), from); });
          },
          py::arg("bytes"), py::arg("from_"))
      .def(
          "deliver",
          [](const FeedHandle& value, py::handle payload, double at) {
            validTime(at);
            auto copied = bytes(payload);
            auto feed = value.get();
            unlocked([&] { feed->deliver(std::move(copied), at); });
          },
          py::arg("bytes"), py::arg("at"))
      .def(
          "record",
          [](const FeedHandle& value, const std::filesystem::path& path) {
            auto feed = value.get();
            unlocked([&] { feed->record(path); });
          },
          py::arg("path"))
      .def(
          "replay",
          [](const FeedHandle& value, std::vector<io::Arrival> recording) {
            validRecording(recording);
            auto feed = value.get();
            unlocked([&] { feed->replay(std::move(recording)); });
          },
          py::arg("recording"))
      .def(
          "advance",
          [](const FeedHandle& value, double seconds) {
            validTime(seconds);
            auto feed = value.get();
            unlocked([&] { feed->advance(seconds); });
          },
          py::arg("seconds"))
      .def(
          "__eq__",
          [](const FeedHandle& left, const FeedHandle& right) {
            return left.get() == right.get();
          },
          py::is_operator(), py::arg("other"));

  py::class_<HubHandle>(resources, "Hub")
      .def(py::init<>())
      .def(
          "load",
          [](const HubHandle& value, py::handle type,
             const std::string& uri) -> py::object {
            auto& hub = value.get();
            if (type.is(py::type::of<image::ImageAsset>())) {
              auto asset = unlocked([&] { return hub.image(uri); });
              return asset ? py::cast(*asset) : py::none();
            }
            return loadData(hub, type, uri);
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
          SIGIL_HUB_URI(resolve) SIGIL_HUB_URI(text) SIGIL_HUB_URI(probe)
              SIGIL_HUB_URI(select)
#undef SIGIL_HUB_URI
      .def(
          "blob",
          [](const HubHandle& value, const std::string& uri) {
            auto& hub = value.get();
            return copiedBytes(unlocked([&] { return hub.blob(uri); }));
          },
          py::arg("uri"))
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
          "feed",
          [](const HubHandle& value, const std::string& uri,
             io::FeedPolicy policy) {
            auto& hub = value.get();
            return FeedHandle(unlocked([&] { return hub.feed(uri, policy); }),
                              value);
          },
          py::arg("uri"), py::arg("policy") = io::FeedPolicy{})
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
      .def("dispatch",
           [](const HubHandle& value) {
             auto& hub = value.get();
             unlocked([&] { hub.dispatch(); });
           })
      .def(
          "dispatch",
          [](const HubHandle& value, double seconds) {
            validTime(seconds);
            auto& hub = value.get();
            unlocked([&] { hub.dispatch(seconds); });
          },
          py::arg("seconds"))
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

#define SIGIL_REGISTER(name)              \
  resources.def(                          \
      #name,                              \
      [](const HubHandle& value) {        \
        auto& hub = value.get();          \
        unlocked([&] { io::name(hub); }); \
      },                                  \
      py::arg("hub"));
  SIGIL_REGISTER(registerUdp)
  SIGIL_REGISTER(registerWebSocket)
  SIGIL_REGISTER(registerWebSocketClient)
  SIGIL_REGISTER(registerSharedMemory)
  SIGIL_REGISTER(registerMidi)
  SIGIL_REGISTER(registerSerial)
  SIGIL_REGISTER(registerGrpc)
  SIGIL_REGISTER(registerQuic)
  SIGIL_REGISTER(registerWebRtc)
  SIGIL_REGISTER(registerTransports)
#undef SIGIL_REGISTER

  resources.def("readRecording", &io::readRecording, py::arg("path"),
                py::call_guard<py::gil_scoped_release>());
  py::class_<RecordingHandle>(resources, "RecordingWriter")
      .def(py::init<const std::filesystem::path&>(), py::arg("path"),
           py::call_guard<py::gil_scoped_release>())
      .def(
          "append",
          [](RecordingHandle& writer, io::Arrival arrival) {
            validTime(arrival.at);
            return unlocked([&] {
              const std::lock_guard lock(writer.mutex);
              return writer.value->append(arrival);
            });
          },
          py::arg("arrival"))
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
              return writer.value->write(copied.bytes);
            });
          },
          py::arg("bytes"));
}

}  // namespace sigil::python
