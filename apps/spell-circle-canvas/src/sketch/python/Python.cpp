#include <include/core/SkCanvas.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilcompose/core/Measure.h>
#include <sigildata/decode/Json.h>
#include <sigildata/table/Table.h>
#include <sigilmotion/bind/Bound.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/DataBindings.h>
#include <sigilpython/IOBindings.h>
#include <sigilpython/ValueBindings.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_map>

#include "KitBindings.h"

extern "C" PyObject* PyInit__sigil();

namespace sigil::sketch::python {
namespace py = pybind11;

namespace {

struct InterpreterConfiguration {
  std::mutex mutex;
  std::filesystem::path executable;
};

InterpreterConfiguration& interpreterConfiguration() {
  static InterpreterConfiguration configuration;
  return configuration;
}

void initializeInterpreter(const std::filesystem::path& executable) {
  if (executable.empty()) {
    py::initialize_interpreter(false, 0, nullptr, false);
    return;
  }
  PyConfig config;
  PyConfig_InitIsolatedConfig(&config);
  config.site_import = 1;
  config.parse_argv = 0;
  config.install_signal_handlers = 0;
  const std::string path = executable.string();
  const auto setPath = [&](wchar_t** field) {
    const PyStatus status =
        PyConfig_SetBytesString(&config, field, path.c_str());
    if (PyStatus_Exception(status)) {
      const std::string error =
          status.err_msg ? status.err_msg : "Could not configure Python";
      PyConfig_Clear(&config);
      throw std::runtime_error(error);
    }
  };
  setPath(&config.executable);
  setPath(&config.program_name);
  // The executable locates pyvenv.cfg; site discovers its package paths.
  // The pybind11 initializer clears the configuration on success or failure.
  py::initialize_interpreter(&config, 0, nullptr, false);
}

void interpreter() {
  // Retained callbacks can be destroyed on worker threads. The embedded
  // interpreter belongs to the process and outlives all such callbacks.
  // An ordinary Python process already owns its interpreter.
  static std::once_flag initialized;
  static bool embedded = false;
  std::call_once(initialized, [] {
    auto& configuration = interpreterConfiguration();
    const std::lock_guard lock(configuration.mutex);
    if (!Py_IsInitialized()) {
      if (PyImport_AppendInittab("_sigil", &PyInit__sigil) == -1)
        throw std::runtime_error("Could not register the Sigil Python module");
      initializeInterpreter(configuration.executable);
      embedded = true;
      PyEval_SaveThread();
    }
  });
  if (!embedded) return;
  const py::gil_scoped_acquire lock;
  try {
    py::list path = py::module_::import("sys").attr("path");
    const py::str packageRoot(SIGIL_PYTHON_PACKAGE_DIR);
    if (!path.contains(packageRoot)) path.attr("insert")(0, packageRoot);
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

/** A Python reference whose final native owner may leave on any thread. */
class Object {
 public:
  explicit Object(py::handle value) : m_value(value.inc_ref().ptr()) {}
  ~Object() {
    if (!Py_IsInitialized()) return;
    if (PyGILState_Check()) {
      Py_DECREF(m_value);
    } else {
      const py::gil_scoped_acquire lock;
      Py_DECREF(m_value);
    }
  }
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;
  py::object get() const { return py::reinterpret_borrow<py::object>(m_value); }

 private:
  PyObject* m_value;
};

struct Generation {
  sigil::python::CallbackLifetime callbacks;
  std::shared_ptr<Object> body;
  std::string package;

  ~Generation() {
    if (!Py_IsInitialized()) return;
    const py::gil_scoped_acquire lock;
    callbacks.clear();
    // Remove only import entries. Existing Python references retain their
    // own objects and never observe a module dictionary being cleared.
    py::dict modules = py::module_::import("sys").attr("modules");
    const py::list names = modules.attr("keys")();
    for (py::handle name : names) {
      const std::string key = py::str(name);
      if (key == package || key.starts_with(package + "."))
        modules.attr("pop")(name, py::none());
    }
    body.reset();
  }
};

struct State {
  sigil::python::CallbackLifetime callbacks;
  compose::Composer* composer = nullptr;
  CanvasSpecification* specification = nullptr;
  motion::Ticker* ticker = nullptr;
  Assets* assets = nullptr;
  weave::FontContext* fonts = nullptr;
  std::vector<std::shared_ptr<compose::TextureScene>>* scenes = nullptr;
  std::vector<std::shared_ptr<const void>> tickerOwners;
  std::unordered_map<io::Feed*, std::shared_ptr<void>> feedLeases;
  std::thread::id thread;
  std::string key;
  bool deterministic = false;
  bool valid = false;
  bool failed = false;

  void retainFeed(std::shared_ptr<io::Feed> feed) {
    auto* identity = feed.get();
    if (!feedLeases.contains(identity))
      feedLeases.emplace(identity,
                         sigil::python::retainSessionFeed(std::move(feed)));
  }

  void close() {
    valid = false;
    callbacks.clear();
    feedLeases.clear();
  }

  void update(SketchContext& ctx) {
    composer = &ctx.composer;
    ticker = &ctx.ticker;
    assets = &ctx.assets;
    fonts = ctx.fonts;
    scenes = ctx.scenes;
    specification = ctx.specification;
    thread = std::this_thread::get_id();
    key = ctx.key;
    deterministic = ctx.deterministic;
    valid = true;
  }

  SketchContext context() const {
    return {*composer,           *ticker,       *assets,
            specification->size, specification, fonts,
            deterministic,       scenes,        key};
  }
};

/** Session-scoped access without retaining a stack-allocated context. */
class Context {
 public:
  explicit Context(const std::shared_ptr<State>& state) : m_state(state) {}
  std::shared_ptr<State> state() const {
    const auto state = m_state.lock();
    if (!state || !state->valid)
      throw std::runtime_error(
          "This sketch context belongs to a closed session");
    if (state->failed)
      throw std::runtime_error(
          "This sketch session failed; save the sketch to reload it");
    if (state->thread != std::this_thread::get_id())
      throw std::runtime_error(
          "Sketch context operations run on the sketch thread");
    return state;
  }

 private:
  std::weak_ptr<State> m_state;
};

class ComposerView : public Context {
 public:
  using Context::Context;
};
class TickerView : public Context {
 public:
  using Context::Context;
};
class AssetsView : public Context {
 public:
  using Context::Context;
};
int callbackArity(const py::function& fn, int maximum) {
  return py::module_::import("sigil._callbacks")
      .attr("arity")(fn, maximum)
      .cast<int>();
}

bool continueTick(py::object result) {
  return result.is_none() || result.cast<bool>();
}

void addTick(const TickerView& view, py::function fn) {
  const auto state = view.state();
  const int arity = callbackArity(fn, 2);
  const sigil::python::CallbackScope scope(state->callbacks);
  auto retained = sigil::python::retainCallback(std::move(fn));
  state->ticker->add([retained, arity](double dt, double elapsed) {
    const py::gil_scoped_acquire lock;
    const sigil::python::CallbackBoundary themes;
    try {
      auto callback = retained->get();
      if (arity == 0) return continueTick(callback());
      if (arity == 1) return continueTick(callback(dt));
      return continueTick(callback(dt, elapsed));
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  });
}

void addFixedTick(const TickerView& view, double hz, py::function fn,
                  int maxCatchUp,
                  std::shared_ptr<choreograph::Output<float>> alpha,
                  std::shared_ptr<motion::Ticker::FixedStatus> status) {
  if (!std::isfinite(hz) || hz <= 0 || maxCatchUp <= 0)
    throw py::value_error(
        "Fixed-step rate and catch-up limit must be positive");
  callbackArity(fn, 0);
  const auto state = view.state();
  const sigil::python::CallbackScope scope(state->callbacks);
  auto retained = sigil::python::retainCallback(std::move(fn));
  state->ticker->addFixed(
      hz,
      [retained] {
        const py::gil_scoped_acquire lock;
        const sigil::python::CallbackBoundary themes;
        try {
          return continueTick(retained->get()());
        } catch (const py::error_already_set& error) {
          throw std::runtime_error(error.what());
        }
      },
      maxCatchUp, alpha.get(), status.get());
  if (alpha) state->tickerOwners.push_back(std::move(alpha));
  if (status) state->tickerOwners.push_back(std::move(status));
}

bool deriveTick(const TickerView& view,
                std::shared_ptr<choreograph::Output<float>> destination,
                const motion::Bound& chain) {
  if (!destination) throw py::type_error("A derived output must be an Output");
  const auto state = view.state();
  if (!state->ticker->derive(destination.get(), chain)) return false;
  state->tickerOwners.push_back(std::move(destination));
  if (chain.owner()) state->tickerOwners.push_back(chain.owner());
  return true;
}

class Body final : public CanvasBody {
 public:
  Body(const std::shared_ptr<Generation>& generation,
       const std::shared_ptr<State>& state)
      : m_generation(generation), m_state(state) {
    const sigil::python::CallbackBoundary themes;
    const py::object instance = generation->body->get()();
    m_instance = std::make_shared<Object>(instance);
    const py::object arity =
        py::module_::import("sigil._callbacks").attr("arity");
    const py::object setup = instance.attr("setup");
    m_setup = std::make_shared<Object>(setup);
    m_setupArity = arity(setup, 1).cast<int>();
    if (py::hasattr(instance, "update")) {
      const py::object update = instance.attr("update");
      m_update = std::make_shared<Object>(update);
      m_updateArity = arity(update, 2).cast<int>();
    }
    m_context = std::make_shared<Context>(state);
  }

  ~Body() override { m_state->close(); }

  void setup(SketchContext& ctx) override {
    m_state->update(ctx);
    // Keep existing transports open while setup reacquires its declarations.
    // Feeds omitted by this declaration close when its previous leases leave.
    const auto previousFeeds = std::move(m_state->feedLeases);
    m_state->feedLeases.clear();
    const sigil::python::CallbackBoundary themes;
    try {
      if (m_setupArity == 0)
        m_setup->get()();
      else
        m_setup->get()(m_context);
    } catch (py::error_already_set& error) {
      m_state->valid = false;
      throw std::runtime_error(error.what());
    }
  }

  void update(double elapsed, SketchContext& ctx) override {
    m_state->update(ctx);
    const sigil::python::CallbackBoundary themes;
    if (!m_update) return;
    if (m_updateArity == 0)
      m_update->get()();
    else if (m_updateArity == 1)
      m_update->get()(elapsed);
    else
      m_update->get()(elapsed, m_context);
  }

 private:
  std::shared_ptr<Generation> m_generation;
  std::shared_ptr<State> m_state;
  std::shared_ptr<Object> m_instance;
  std::shared_ptr<Object> m_setup;
  std::shared_ptr<Object> m_update;
  std::shared_ptr<Context> m_context;
  int m_setupArity = 0;
  int m_updateArity = 0;
};

class PythonSession final : public Session {
 public:
  PythonSession(std::unique_ptr<Session> session, std::shared_ptr<State> state)
      : m_session(std::move(session)), m_state(std::move(state)) {}

  ~PythonSession() override {
    const py::gil_scoped_acquire lock;
    m_state->close();
    m_session.reset();
  }

  const CanvasSpecification& canvas() const override {
    return m_session->canvas();
  }
  float oversample() const override { return m_session->oversample(); }
  Timing timing() const override { return m_session->timing(); }
  std::span<const Lane> lanes() const override { return m_session->lanes(); }
  std::string counters() const override { return m_session->counters(); }

  void frame(SkCanvas& canvas, double dt) override {
    const SkAutoCanvasRestore restore(&canvas, true);
    invoke([&] { m_session->frame(canvas, dt); });
  }
  void repaint(SkCanvas& canvas) override {
    const SkAutoCanvasRestore restore(&canvas, true);
    invoke([&] { m_session->repaint(canvas); });
  }
  void still(SkCanvas& canvas) override {
    const SkAutoCanvasRestore restore(&canvas, true);
    invoke([&] { m_session->still(canvas); });
  }
  void redeclare() override {
    invoke([&] { m_session->redeclare(); });
  }
  void setAutoPromotion(Promotion policy) override {
    invoke([&] { m_session->setAutoPromotion(policy); });
  }
  void setProfiling(bool on) override {
    invoke([&] { m_session->setProfiling(on); });
  }
  void setCompositeCounting(bool on) override {
    invoke([&] { m_session->setCompositeCounting(on); });
  }
  CompositeCounts compositeCounts() const override {
    return m_session->compositeCounts();
  }
  void setBakeDensity(float density) override {
    invoke([&] { m_session->setBakeDensity(density); });
  }
  std::vector<std::string> costs(size_t limit) const override {
    return m_session->costs(limit);
  }
  void pointer(float x, float y, bool pressed) override {
    invoke([&] { m_session->pointer(x, y, pressed); });
  }
  void key(std::string_view name, int code, bool pressed) override {
    invoke([&] { m_session->key(name, code, pressed); });
  }

 private:
  template <class Function>
  void invoke(Function&& function) {
    const py::gil_scoped_acquire lock;
    if (m_state->failed) throw std::runtime_error(m_error);
    const sigil::python::CallbackScope callbacks(m_state->callbacks);
    const sigil::python::CallbackBoundary themes;
    try {
      function();
    } catch (const py::error_already_set& error) {
      m_state->failed = true;
      m_state->feedLeases.clear();
      m_error = error.what();
      throw std::runtime_error(m_error);
    } catch (const std::exception& error) {
      m_state->failed = true;
      m_state->feedLeases.clear();
      m_error = error.what();
      throw;
    }
  }

  std::unique_ptr<Session> m_session;
  std::shared_ptr<State> m_state;
  std::string m_error;
};

class PythonKind final : public KindOperations {
 public:
  explicit PythonKind(std::shared_ptr<Generation> generation)
      : m_generation(std::move(generation)) {}
  bool operator==(const PythonKind& other) const {
    return m_generation == other.m_generation;
  }
  std::string_view runtime() const override { return "canvas"; }
  std::unique_ptr<Session> open(weave::FontContext& fonts, Assets& assets,
                                bool deterministic,
                                std::string_view key) const override {
    const py::gil_scoped_acquire lock;
    try {
      const auto state = std::make_shared<State>();
      const sigil::python::CallbackScope callbacks(state->callbacks);
      const sigil::python::CallbackBoundary themes;
      auto body = std::make_unique<Body>(m_generation, state);
      auto session =
          openCanvas(std::move(body), fonts, assets, deterministic, key);
      return std::make_unique<PythonSession>(std::move(session), state);
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  }

 private:
  std::shared_ptr<Generation> m_generation;
};

class SourceKind final : public KindOperations {
 public:
  explicit SourceKind(std::filesystem::path source)
      : m_source(std::move(source)) {}
  bool operator==(const SourceKind& other) const {
    return m_source == other.m_source;
  }
  std::string_view runtime() const override { return "canvas"; }
  std::unique_ptr<Session> open(weave::FontContext& fonts, Assets& assets,
                                bool deterministic,
                                std::string_view key) const override {
    const Kind imported = load(m_source);
    return imported->open(fonts, assets, deterministic, key);
  }

 private:
  std::filesystem::path m_source;
};

std::string renderFile(const std::string& source, const std::string& output,
                       std::optional<double> at) {
  const auto path = std::filesystem::absolute(source);
  weave::FontContext fonts(weave::ports::systemFontManager());
  Host::Options options;
  options.pythonLoader = &load;
  options.sketchPath = path;
  options.assetsDirectory = path.parent_path() / "assets";
  options.deterministic = true;
  Host host(options, fonts);
  host.poll();
  if (!host.live()) throw std::runtime_error(host.errorLog());
  host.prepareCapture(at);
  const auto destination = std::filesystem::absolute(output);
  if (!host.capture(destination))
    throw std::runtime_error(host.errorLog().empty()
                                 ? "Could not write the sketch capture"
                                 : host.errorLog());
  return destination.string();
}

}  // namespace

std::string_view interpreterAbi() { return SIGIL_PYTHON_ABI; }

std::pair<int, int> interpreterVersion() {
  return {PY_MAJOR_VERSION, PY_MINOR_VERSION};
}

void configureInterpreter(const std::filesystem::path& executable) {
  auto& configuration = interpreterConfiguration();
  const std::lock_guard lock(configuration.mutex);
  if (Py_IsInitialized())
    throw std::runtime_error(
        "Python is already initialized; open a new Sketchbook process to "
        "select another environment");
  configuration.executable =
      executable.empty()
          ? std::filesystem::path{}
          : std::filesystem::absolute(executable).lexically_normal();
}

Kind source(const std::filesystem::path& path) {
  return Kind{SourceKind(std::filesystem::absolute(path).lexically_normal())};
}

bool available(const std::filesystem::path& path,
               std::initializer_list<const char*> modules, std::string* why) {
  const auto unavailable = [why](std::string reason) {
    if (why) *why = std::move(reason);
    return false;
  };
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error))
    return unavailable("Python source is unavailable: " + path.string());
  if (modules.size() == 0) return true;
  try {
    interpreter();
    const py::gil_scoped_acquire lock;
    try {
      const py::object find =
          py::module_::import("importlib.util").attr("find_spec");
      for (const char* module : modules) {
        if (!module || !*module)
          return unavailable("A required Python module has no name");
        if (find(module).is_none())
          return unavailable("Python module '" + std::string(module) +
                             "' is not installed");
      }
      return true;
    } catch (const py::error_already_set& failure) {
      return unavailable("Python module discovery failed: " +
                         std::string(failure.what()));
    }
  } catch (const std::exception& failure) {
    return unavailable("Python module discovery failed: " +
                       std::string(failure.what()));
  }
}

Kind load(const std::filesystem::path& source) {
  interpreter();
  const py::gil_scoped_acquire lock;
  try {
    py::module_::import("_sigil");
    auto generation = std::make_shared<Generation>();
    const sigil::python::CallbackScope callbacks(generation->callbacks);
    const sigil::python::CallbackBoundary themes;
    const py::tuple result =
        py::module_::import("sigil._loader").attr("load")(source.string());
    generation->body = std::make_shared<Object>(result[0]);
    generation->package = result[1].cast<std::string>();
    return Kind{PythonKind(std::move(generation))};
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

void stageContext(py::handle value, const kit::Stage& stage) {
  const auto state = value.cast<const Context&>().state();
  auto context = state->context();
  kit::stage(context, stage);
}

void bindRuntime(py::module_& module) {
  auto composition = module.attr("compose").cast<py::module_>();
  auto clocks = module.attr("motion").cast<py::module_>();
  auto sketches = module.def_submodule("sketch");
  py::class_<ComposerView>(composition, "Composer")
      .def(
          "render",
          [](const ComposerView& v, const compose::Element& e) {
            v.state()->composer->render(e);
          },
          py::arg("element"))
      .def(
          "renderSlot",
          [](const ComposerView& v, const std::string& key,
             const compose::Element& e) {
            v.state()->composer->renderSlot(key, e);
          },
          py::arg("name"), py::arg("element"))
      .def(
          "bounds",
          [](const ComposerView& v, const std::string& key) {
            return v.state()->composer->bounds(key);
          },
          py::arg("key"))
      .def(
          "hitTest",
          [](const ComposerView& v, py::handle at) {
            return v.state()->composer->hitTest(sigil::python::point(at));
          },
          py::arg("at"))
      .def(
          "routesAt",
          [](const ComposerView& v, const std::string& key) {
            return v.state()->composer->routesAt(key);
          },
          py::arg("key"))
      .def(
          "settling",
          [](const ComposerView& v, const std::string& key) {
            return v.state()->composer->settling(key);
          },
          py::arg("key"))
      .def("active",
           [](const ComposerView& v) { return v.state()->composer->active(); })
      .def("dirty",
           [](const ComposerView& v) { return v.state()->composer->dirty(); })
      .def("purgeCaches",
           [](const ComposerView& v) { v.state()->composer->purgeCaches(); })
      .def("stats",
           [](const ComposerView& v) { return v.state()->composer->stats(); });
  py::class_<TickerView>(clocks, "Ticker")
      .def("add", &addTick, py::arg("function"))
      .def("addFixed", &addFixedTick, py::arg("hz"), py::arg("function"),
           py::arg("maxCatchUp") = 8, py::arg("alphaOut") = nullptr,
           py::arg("statusOut") = nullptr)
      .def("derive", &deriveTick, py::arg("destination"), py::arg("chain"))
      .def("active",
           [](const TickerView& v) { return v.state()->ticker->active(); })
      .def("elapsed",
           [](const TickerView& v) { return v.state()->ticker->elapsed(); });
  py::class_<AssetsView>(sketches, "Assets")
      .def(
          "image",
          [](const AssetsView& v, const std::string& uri) {
            return *v.state()->assets->image(uri);
          },
          py::arg("uri"))
      .def(
          "json",
          [](const AssetsView& v,
             const std::string& uri) -> std::optional<data::Json> {
            const auto value = v.state()->assets->json(uri);
            if (value) return *value;
            return {};
          },
          py::arg("uri"))
      .def(
          "table",
          [](const AssetsView& v,
             const std::string& uri) -> std::optional<data::Table> {
            const auto value = v.state()->assets->table(uri);
            if (value) return *value;
            return {};
          },
          py::arg("uri"))
      .def(
          "database",
          [](const AssetsView& v, const std::string& uri) {
            return sigil::python::dataDatabase(
                v.state()->assets->database(uri));
          },
          py::arg("uri"))
      .def("hub",
           [](const AssetsView& v) {
             v.state();
             return sigil::python::HubHandle(
                 [v]() -> io::Hub& { return v.state()->assets->hub(); },
                 [v](std::shared_ptr<io::Feed> feed) {
                   v.state()->retainFeed(std::move(feed));
                 });
           })
      .def("root",
           [](const AssetsView& v) { return v.state()->assets->root(); });
  py::class_<Context, std::shared_ptr<Context>>(module, "Context")
      .def(
          "canvas",
          [](const Context& ctx, float width, float height) {
            if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 ||
                height <= 0)
              throw py::value_error(
                  "Canvas dimensions must be finite and positive");
            ctx.state()->specification->size = {width, height};
          },
          py::arg("width"), py::arg("height"))
      .def(
          "background",
          [](const Context& ctx, py::handle value) {
            ctx.state()->specification->background =
                sigil::python::color(value);
          },
          py::arg("color"))
      .def(
          "captureAt",
          [](const Context& ctx, double seconds) {
            if (!std::isfinite(seconds) || seconds < 0)
              throw py::value_error(
                  "Capture time must be finite and nonnegative");
            ctx.state()->specification->captureSeconds = seconds;
          },
          py::arg("seconds"))
      .def(
          "render",
          [](const Context& ctx, const compose::Element& element) {
            ctx.state()->composer->render(element);
          },
          py::arg("element"))
      .def_property_readonly(
          "composer",
          [](const Context& ctx) { return ComposerView(ctx.state()); })
      .def_property_readonly(
          "ticker", [](const Context& ctx) { return TickerView(ctx.state()); })
      .def_property_readonly(
          "assets", [](const Context& ctx) { return AssetsView(ctx.state()); })
      .def(
          "measured",
          [](const Context& ctx, double value, double pinned) {
            return ctx.state()->context().measured(value, pinned);
          },
          py::arg("value"), py::arg("pinned") = 0)
      .def(
          "oversample",
          [](const Context& ctx, int samples) {
            ctx.state()->context().oversample(samples);
          },
          py::arg("samples"))
      .def("plate", [](const Context& ctx) { ctx.state()->context().plate(); })
      .def(
          "nonlinearPicture",
          [](const Context& ctx) { ctx.state()->context().nonlinearPicture(); })
      .def(
          "measure",
          [](const Context& ctx, const compose::Element& element,
             SkSize maximum) {
            return ctx.state()->context().measure(element, maximum);
          },
          py::arg("element"), py::arg("maxSize") = SkSize::MakeEmpty())
      .def(
          "snapshot",
          [](const Context& ctx, const compose::Element& element,
             SkSize maximum) {
            const auto state = ctx.state();
            return compose::snapshot(element, *state->fonts, maximum);
          },
          py::arg("element"), py::arg("maxSize") = SkSize::MakeEmpty())
      .def(
          "local",
          [](const Context& ctx, const std::string& name) {
            return "sketch://" + ctx.state()->key + "/" + name;
          },
          py::arg("path"))
      .def_property_readonly(
          "elapsed",
          [](const Context& ctx) { return ctx.state()->ticker->elapsed(); })
      .def_property_readonly("width",
                             [](const Context& ctx) {
                               return ctx.state()->specification->size.width();
                             })
      .def_property_readonly("height",
                             [](const Context& ctx) {
                               return ctx.state()->specification->size.height();
                             })
      .def_property_readonly("size",
                             [](const Context& ctx) {
                               const auto state = ctx.state();
                               return py::make_tuple(
                                   state->specification->size.width(),
                                   state->specification->size.height());
                             })
      .def_property_readonly("deterministic", [](const Context& ctx) {
        return ctx.state()->deterministic;
      });
  sketches.attr("Context") = module.attr("Context");
  module.def("render_file", &renderFile, py::arg("source"), py::arg("output"),
             py::arg("at") = py::none());
}

}  // namespace sigil::sketch::python
