#include <include/core/SkCanvas.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "Bindings.h"

extern "C" PyObject* PyInit__sigil();

namespace sigil::sketch::python {
namespace py = pybind11;

namespace {

void interpreter() {
  // Retained callbacks can be destroyed on worker threads. The embedded
  // interpreter belongs to the process and outlives all such callbacks.
  // An ordinary Python process already owns its interpreter.
  static std::once_flag initialized;
  std::call_once(initialized, [] {
    if (!Py_IsInitialized()) {
      if (PyImport_AppendInittab("_sigil", &PyInit__sigil) == -1)
        throw std::runtime_error("Could not register the Sigil Python module");
      py::initialize_interpreter(false, 0, nullptr, false);
      PyEval_SaveThread();
    }
  });
  const py::gil_scoped_acquire lock;
  py::list path = py::module_::import("sys").attr("path");
  const py::str packageRoot(SIGIL_PYTHON_PACKAGE_DIR);
  if (!path.contains(packageRoot)) path.attr("insert")(0, packageRoot);
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
  CallbackLifetime callbacks;
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
  CallbackLifetime callbacks;
  compose::Composer* composer = nullptr;
  CanvasSpecification* specification = nullptr;
  std::thread::id thread;
  std::string key;
  double elapsed = 0;
  bool deterministic = false;
  bool valid = false;
  bool failed = false;

  void update(SketchContext& ctx) {
    composer = &ctx.composer;
    specification = ctx.specification;
    thread = std::this_thread::get_id();
    key = ctx.key;
    deterministic = ctx.deterministic;
    valid = true;
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

class Body final : public CanvasBody {
 public:
  Body(const std::shared_ptr<Generation>& generation,
       const std::shared_ptr<State>& state)
      : m_generation(generation), m_state(state) {
    const py::object instance = generation->body->get()();
    m_instance = std::make_shared<Object>(instance);
    const py::object arity = py::module_::import("sigil._loader").attr("arity");
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

  ~Body() override {
    m_state->valid = false;
    m_state->callbacks.clear();
  }

  void setup(SketchContext& ctx) override {
    m_state->update(ctx);
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
    m_state->elapsed = elapsed;
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
    m_state->valid = false;
    m_state->callbacks.clear();
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
    const CallbackScope callbacks(m_state->callbacks);
    try {
      function();
    } catch (const py::error_already_set& error) {
      m_state->failed = true;
      m_error = error.what();
      throw std::runtime_error(m_error);
    } catch (const std::exception& error) {
      m_state->failed = true;
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
      const CallbackScope callbacks(state->callbacks);
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

std::string renderFile(const std::string& source, const std::string& output,
                       std::optional<double> at) {
  const auto path = std::filesystem::absolute(source);
  weave::FontContext fonts(weave::ports::systemFontManager());
  Host::Options options;
  options.sketchPath = path;
  options.assetsDirectory = path.parent_path() / "assets";
  options.deterministic = true;
  Host host(options, fonts);
  host.poll();
  if (!host.live()) throw std::runtime_error(host.errorLog());
  const double seconds =
      at.value_or(host.captureSeconds() > 0 ? host.captureSeconds() : 1.5);
  const auto dimensions = [&] {
    const auto size = host.canvasSize();
    const double width = std::ceil(size.width());
    const double height = std::ceil(size.height());
    if (!std::isfinite(width) || !std::isfinite(height) || width < 1 ||
        height < 1 || width > 16384 || height > 16384)
      throw std::runtime_error("Canvas dimensions are invalid");
    return SkISize::Make(int(width), int(height));
  };
  if (!std::isfinite(seconds) || seconds < 0 ||
      seconds * 60 > static_cast<double>(std::numeric_limits<int>::max()))
    throw std::runtime_error("Capture time is invalid");
  const auto size = dimensions();
  SkNoDrawCanvas scratch(size.width(), size.height());
  const int frames = std::max(1, int(std::lround(seconds * 60)));
  for (int frame = 0; frame < frames; ++frame)
    if (!host.frame(scratch, 1.0 / 60.0))
      throw std::runtime_error(host.errorLog());
  dimensions();
  const auto destination = std::filesystem::absolute(output);
  if (!host.capture(destination))
    throw std::runtime_error(host.errorLog().empty()
                                 ? "Could not write the sketch capture"
                                 : host.errorLog());
  return destination.string();
}

}  // namespace

Kind load(const std::filesystem::path& source) {
  interpreter();
  const py::gil_scoped_acquire lock;
  try {
    py::module_::import("_sigil");
    auto generation = std::make_shared<Generation>();
    const CallbackScope callbacks(generation->callbacks);
    const py::tuple result =
        py::module_::import("sigil._loader").attr("load")(source.string());
    generation->body = std::make_shared<Object>(result[0]);
    generation->package = result[1].cast<std::string>();
    return Kind{PythonKind(std::move(generation))};
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

void bindRuntime(py::module_& module) {
  py::class_<Context, std::shared_ptr<Context>>(module, "Context")
      .def("canvas",
           [](const Context& ctx, float width, float height) {
             if (!std::isfinite(width) || !std::isfinite(height) ||
                 width <= 0 || height <= 0)
               throw py::value_error(
                   "Canvas dimensions must be finite and positive");
             ctx.state()->specification->size = {width, height};
           })
      .def("background",
           [](const Context& ctx, py::handle value) {
             ctx.state()->specification->background = color(value);
           })
      .def("captureAt",
           [](const Context& ctx, double seconds) {
             if (!std::isfinite(seconds) || seconds < 0)
               throw py::value_error(
                   "Capture time must be finite and nonnegative");
             ctx.state()->specification->captureSeconds = seconds;
           })
      .def("render",
           [](const Context& ctx, const compose::Element& element) {
             ctx.state()->composer->render(element);
           })
      .def("local",
           [](const Context& ctx, const std::string& name) {
             return "sketch://" + ctx.state()->key + "/" + name;
           })
      .def_property_readonly(
          "elapsed", [](const Context& ctx) { return ctx.state()->elapsed; })
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
  module.def("render_file", &renderFile, py::arg("source"), py::arg("output"),
             py::arg("at") = py::none());
}

}  // namespace sigil::sketch::python
