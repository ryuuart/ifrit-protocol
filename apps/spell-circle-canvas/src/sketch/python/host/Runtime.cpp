#include <include/core/SkCanvas.h>
#include <pybind11/embed.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/io/Hub.h>
#include <sigilpython/skia/Values.h>
#include <sigilsketch/core/Assets.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>
#include <sigilsketch/scene2d/Scene2D.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilsketch/testing/InProcessHost.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <unistd.h>

#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <span>
#include <stdexcept>
#include <system_error>
#include <thread>

#include "../kit/Registration.h"
#include "Held.h"
#include "Registration.h"
#include "Session.h"

extern "C" PyObject* PyInit__sigil();

namespace sigil::sketch::python {
namespace py = pybind11;

using detail::AssetsView;
using detail::Context;
using detail::Scene3DContextView;
using detail::Scene3DLoan;
using detail::State;

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

/** Binds the name `_sigil` to the module compiled into this process
 *  before any import can resolve it elsewhere. The environment a sketch
 *  project selects may hold a compiled copy of the same module — an
 *  editable install of the package puts a finder for its own copy ahead
 *  of the built-in importer — and that copy carries a second set of the
 *  native libraries: its nodes, paints and contexts are laid out by
 *  whatever build produced it, and the host cannot read them. The host's
 *  own copy is the one every sketch it runs must build with. */
void bindBuiltinModule() {
  try {
    const py::object spec = py::module_::import("importlib.machinery")
                                .attr("BuiltinImporter")
                                .attr("find_spec")("_sigil");
    if (spec.is_none())
      throw std::runtime_error("The Sigil Python module is not built in");
    const py::object module =
        py::module_::import("importlib.util").attr("module_from_spec")(spec);
    spec.attr("loader").attr("exec_module")(module);
    py::module_::import("sys").attr("modules")["_sigil"] = module;
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
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
      bindBuiltinModule();
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
  /** The body class's own `needsDevice`, read once when it is loaded: a
   *  scene2d sketch that bakes a 3D frame says so there, as a C++ body
   *  does with its `static constexpr bool needsDevice`, and a host brings
   *  the device up before the session opens. */
  bool needsDevice = false;
  /** Whether the body describes a world frame, as the `@sketch` decorator
   *  found it: a `describe` whose return annotation names `Frame`. */
  bool scene3d = false;

  ~Generation() {
    if (!Py_IsInitialized()) return;
    const py::gil_scoped_acquire lock;
    callbacks.clear();
    // Remove only import entries. Existing Python references retain their
    // own objects and never observe a module dictionary being cleared.
    // A body the author's interpreter handed over imported nothing of its
    // own, and names no package.
    if (!package.empty()) {
      py::dict modules = py::module_::import("sys").attr("modules");
      const py::list names = modules.attr("keys")();
      for (py::handle name : names) {
        const std::string key = py::str(name);
        if (key == package || key.starts_with(package + "."))
          modules.attr("pop")(name, py::none());
      }
    }
    body.reset();
  }
};

class Body final : public Scene2DBody {
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

  /** The running instance, as a client's evaluated text reaches it. */
  [[nodiscard]] const std::shared_ptr<Object>& instance() const {
    return m_instance;
  }
  /** The context its hooks are handed. */
  [[nodiscard]] const std::shared_ptr<Context>& context() const {
    return m_context;
  }

  void setup(Scene2DContext& ctx) override {
    m_state->update(ctx);
    // Keep existing transports open while setup reacquires its declarations.
    // Feeds omitted by this declaration close when its previous leases leave,
    // after the connections the previous declaration opened over them.
    const auto previousFeeds = std::move(m_state->feedLeases);
    m_state->feedLeases.clear();
    const auto previousHeld = std::move(m_state->held);
    m_state->held.clear();
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

  void update(double elapsed, Scene2DContext& ctx) override {
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

/** A SCENE3D BODY WRITTEN IN PYTHON: an optional `setup(ctx)` handed the
 *  scene3d context while it runs, and `describe(seconds)` returning the
 *  world frame at a scene time. The session that drives it holds the
 *  interpreter around every call. */
class Scene3DPythonBody final : public Scene3DBody {
 public:
  Scene3DPythonBody(const std::shared_ptr<Generation>& generation,
                    const std::shared_ptr<State>& state)
      : m_generation(generation), m_state(state) {
    const sigil::python::CallbackBoundary themes;
    const py::object instance = generation->body->get()();
    m_instance = std::make_shared<Object>(instance);
    const py::object arity =
        py::module_::import("sigil._callbacks").attr("arity");
    if (py::hasattr(instance, "setup")) {
      const py::object setup = instance.attr("setup");
      m_setup = std::make_shared<Object>(setup);
      m_setupArity = arity(setup, 1).cast<int>();
    }
    const py::object describe = instance.attr("describe");
    m_describe = std::make_shared<Object>(describe);
    m_describeArity = arity(describe, 1).cast<int>();
    m_state->valid = true;
  }

  ~Scene3DPythonBody() override { m_state->close(); }

  /** The running instance, as a client's evaluated text reaches it. */
  [[nodiscard]] const std::shared_ptr<Object>& instance() const {
    return m_instance;
  }

  void setup(Scene3DContext& ctx) override {
    if (!m_setup) return;
    const auto loan = std::make_shared<Scene3DLoan>();
    loan->context = &ctx;
    loan->thread = std::this_thread::get_id();
    // The context answers only while setup runs, whichever way it ends.
    struct Returned {
      Scene3DLoan& loan;
      ~Returned() { loan.context = nullptr; }
    } returned{*loan};
    const sigil::python::CallbackBoundary themes;
    try {
      if (m_setupArity == 0)
        m_setup->get()();
      else
        m_setup->get()(Scene3DContextView(loan));
    } catch (py::error_already_set& error) {
      m_state->valid = false;
      throw std::runtime_error(error.what());
    }
  }

  world::Frame describe(motion::Duration now) override {
    const sigil::python::CallbackBoundary themes;
    const py::object described = m_describeArity == 0
                                     ? m_describe->get()()
                                     : m_describe->get()(now.count());
    if (!py::isinstance<world::Frame>(described))
      throw std::runtime_error(
          "A scene3d sketch's describe(seconds) returns a world.Frame, not " +
          std::string(py::str(py::type::of(described).attr("__name__"))));
    return described.cast<world::Frame>();
  }

 private:
  std::shared_ptr<Generation> m_generation;
  std::shared_ptr<State> m_state;
  std::shared_ptr<Object> m_instance;
  std::shared_ptr<Object> m_setup;
  std::shared_ptr<Object> m_describe;
  int m_setupArity = 0;
  int m_describeArity = 1;
};

/** WHAT TEXT A CLIENT SENDS IS EVALUATED AGAINST: the running instance,
 *  whose class names the module the text runs in, and the context its
 *  hooks are handed — none for a scene3d sketch, whose context answers
 *  only while its setup runs. */
struct Evaluated {
  std::shared_ptr<Object> instance;
  std::shared_ptr<Context> context;
};

/** The exception a native error carries in its words, read back from the
 *  form the interpreter's error takes — `Type: message`, then the trace —
 *  where the error crossed into native code before it could be read. */
EvaluationError errorFromText(const std::string& text) {
  EvaluationError error;
  const std::string first = text.substr(0, text.find('\n'));
  const auto colon = first.find(": ");
  if (colon != std::string::npos &&
      first.substr(0, colon).find(' ') == std::string::npos) {
    error.type = first.substr(0, colon);
    error.message = first.substr(colon + 2);
  } else {
    error.type = "RuntimeError";
    error.message = first;
  }
  error.traceback = text;
  return error;
}

class PythonSession final : public Session {
 public:
  PythonSession(std::unique_ptr<Session> session, std::shared_ptr<State> state,
                Evaluated evaluated)
      : m_session(std::move(session)),
        m_state(std::move(state)),
        m_evaluated(std::move(evaluated)) {}

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
  std::span<const LaneCost> lanes() const override {
    return m_session->lanes();
  }
  std::string counters() const override { return m_session->counters(); }
  void timeDevice(bool on) override { m_session->timeDevice(on); }
  std::string deviceTimes() const override { return m_session->deviceTimes(); }

  void frame(SkCanvas& canvas, double dt) override {
    const SkAutoCanvasRestore restore(&canvas, true);
    invoke([&] { m_session->frame(canvas, dt); });
  }
  // Forwarded rather than left to the base, whose default paints: the
  // wrapped session is the one that knows whether the sketch declared its
  // unobserved steps need no painting.
  void discardedFrame(SkCanvas& canvas, double dt) override {
    const SkAutoCanvasRestore restore(&canvas, true);
    invoke([&] { m_session->discardedFrame(canvas, dt); });
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
  void setProfiling(bool on) override {
    invoke([&] { m_session->setProfiling(on); });
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
  bool hasViewpoint() const override { return m_session->hasViewpoint(); }
  std::optional<geometry::mesh::camera::Orbit> orbit() const override {
    return m_session->orbit();
  }
  void viewpoint(float yawDegrees, float pitchDegrees,
                 float distance) override {
    invoke([&] { m_session->viewpoint(yawDegrees, pitchDegrees, distance); });
  }

  /** THE TEXT RUN IN THE SKETCH'S MODULE by the package's evaluator, which
   *  catches whatever it raises, then — where it ran to its end and the
   *  body declares its tree in setup — the body declared again. A
   *  declaration that raises fails the session as a frame that raises
   *  does, and is answered as the error. */
  std::optional<Evaluation> evaluate(std::string_view code,
                                     EvaluationMode mode) override {
    const py::gil_scoped_acquire lock;
    if (m_state->failed) throw std::runtime_error(m_error);
    Evaluation evaluation;
    bool redescribe = false;
    {
      const sigil::python::CallbackScope callbacks(m_state->callbacks);
      const sigil::python::CallbackBoundary themes;
      try {
        const py::object context =
            m_evaluated.context ? py::cast(m_evaluated.context) : py::none();
        const py::tuple answered =
            py::module_::import("sigil._evaluation")
                .attr("evaluate")(m_evaluated.instance->get(), context,
                                  py::str(code.data(), code.size()),
                                  mode == EvaluationMode::Expression);
        evaluation.value = answered[0].cast<std::string>();
        evaluation.output = answered[1].cast<std::string>();
        if (!answered[2].is_none()) {
          const py::tuple raised = answered[2].cast<py::tuple>();
          evaluation.error = EvaluationError{raised[0].cast<std::string>(),
                                             raised[1].cast<std::string>(),
                                             raised[2].cast<std::string>()};
        }
        redescribe = answered[3].cast<bool>();
      } catch (const py::error_already_set& error) {
        // The evaluator catches what the text raises; reaching here means
        // the evaluator itself could not run, and the sketch is untouched.
        evaluation.error = errorFromText(error.what());
        return evaluation;
      }
    }
    if (evaluation.error || !redescribe) return evaluation;
    try {
      invoke([&] { m_session->redeclare(); });
    } catch (const std::exception& error) {
      evaluation.error = errorFromText(error.what());
      evaluation.error->message +=
          " — raised as the sketch described itself again after the text "
          "ran, so the sketch has stopped until its file is saved or "
          "reloaded";
    }
    return evaluation;
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
      m_state->held.clear();
      m_state->feedLeases.clear();
      m_error = error.what();
      throw std::runtime_error(m_error);
    } catch (const std::exception& error) {
      m_state->failed = true;
      m_state->held.clear();
      m_state->feedLeases.clear();
      m_error = error.what();
      throw;
    }
  }

  std::unique_ptr<Session> m_session;
  std::shared_ptr<State> m_state;
  Evaluated m_evaluated;
  std::string m_error;
};

/** WHERE A KIND A PYTHON SOURCE MAKES OPENS, when a host said so: the
 *  device runtime a world frame is drawn on — null being the process's
 *  own. A thumbnail states it empty, and a kind that keeps it reaches
 *  neither the process's device nor its queue. */
struct StatedRuntimes {
  std::shared_ptr<const world::Runtime> runtime;

  bool operator==(const StatedRuntimes& other) const {
    return runtime == other.runtime ||
           (runtime && other.runtime && *runtime == *other.runtime);
  }
  /** The device runtime a session opens on. */
  [[nodiscard]] const world::Runtime& opening() const {
    return runtime ? *runtime : sketch::runtime();
  }
  /** @p kind, told what was stated here. */
  [[nodiscard]] Kind applied(Kind kind) const {
    if (runtime) kind = onRuntime(kind, *runtime);
    return kind;
  }
};

class PythonKind final : public KindOperations, public OpensOnRuntime {
 public:
  explicit PythonKind(std::shared_ptr<Generation> generation)
      : m_generation(std::move(generation)) {}
  bool operator==(const PythonKind& other) const {
    return m_generation == other.m_generation && m_stated == other.m_stated;
  }
  Kind opensOn(const world::Runtime& runtime) const override {
    PythonKind told = *this;
    told.m_stated.runtime = std::make_shared<const world::Runtime>(runtime);
    return Kind{std::move(told)};
  }
  std::string_view runtime() const override {
    return m_generation->scene3d ? "scene3d" : "scene2d";
  }
  bool needsDevice() const override {
    return m_generation->scene3d || m_generation->needsDevice;
  }
  /** A sketch that describes or bakes a 3D frame draws on the device
   *  runtime it was told, or the one the process installed, so where
   *  that is none it is listed unavailable with the one message every
   *  device door speaks, rather than opened to draw its fallback. */
  bool available(std::string* why) const override {
    return !needsDevice() || runtimeAvailable(m_stated.opening(), why);
  }
  std::unique_ptr<Session> open(weave::FontContext& fonts, Assets& assets,
                                bool deterministic,
                                std::string_view key) const override {
    const py::gil_scoped_acquire lock;
    try {
      const auto state = std::make_shared<State>();
      const sigil::python::CallbackScope callbacks(state->callbacks);
      const sigil::python::CallbackBoundary themes;
      if (m_generation->scene3d) {
        // Refused before a body is made, so a scene3d sketch that cannot be
        // drawn runs none of its own Python.
        const world::Runtime& opening = m_stated.opening();
        std::string why;
        if (!runtimeAvailable(opening, &why)) throw std::runtime_error(why);
        auto body = std::make_unique<Scene3DPythonBody>(m_generation, state);
        Evaluated evaluated{body->instance(), nullptr};
        auto session =
            openScene3D(std::move(body), fonts, assets, deterministic, opening);
        return std::make_unique<PythonSession>(std::move(session), state,
                                               std::move(evaluated));
      }
      auto body = std::make_unique<Body>(m_generation, state);
      Evaluated evaluated{body->instance(), body->context()};
      auto session = openScene2D(std::move(body), fonts, assets, deterministic,
                                 key, m_stated.runtime);
      return std::make_unique<PythonSession>(std::move(session), state,
                                             std::move(evaluated));
    } catch (const py::error_already_set& error) {
      throw std::runtime_error(error.what());
    }
  }

 private:
  std::shared_ptr<Generation> m_generation;
  StatedRuntimes m_stated;
};

/** Whether the sketch written in @p source declares `needsDevice = True`
 *  on a line of its own — in its class, or at the top of a file of
 *  functions — read from the text rather than by importing it: a listing
 *  asks every source kind before any is opened, and importing each would
 *  run every sketch's module to answer one line. A file that cannot be
 *  read declares nothing. */
bool declaresDevice(const std::filesystem::path& source) {
  std::ifstream file(source);
  std::string line;
  while (std::getline(file, line)) {
    std::string_view text = line;
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) continue;
    text.remove_prefix(first);
    constexpr std::string_view kName = "needsDevice";
    if (!text.starts_with(kName)) continue;
    text.remove_prefix(kName.size());
    const auto equals = text.find_first_not_of(" \t");
    if (equals == std::string_view::npos || text[equals] != '=') continue;
    text.remove_prefix(equals + 1);
    const auto value = text.find_first_not_of(" \t");
    if (value != std::string_view::npos &&
        text.substr(value).starts_with("True"))
      return true;
  }
  return false;
}

/** Whether the sketch written in @p source describes a world frame: a
 *  method `describe`, or a function `describe` of a file of functions,
 *  whose return annotation names `Frame` — `-> Frame`, `-> world.Frame`,
 *  quoted or not — read from the text by the rule the `@sketch` decorator
 *  applies to the class it loads, so a listing and an opened session
 *  agree on the kind without importing the file. A signature may run over
 *  several lines; it ends at the line whose text ends with its colon. A
 *  file that cannot be read describes none. */
bool describesFrame(const std::filesystem::path& source) {
  std::ifstream file(source);
  std::string line;
  while (std::getline(file, line)) {
    std::string_view text = line;
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) continue;
    text.remove_prefix(first);
    constexpr std::string_view kSignature = "def describe(";
    if (!text.starts_with(kSignature)) continue;
    std::string signature(text);
    constexpr int kLongestSignature = 12;
    for (int more = 0; more < kLongestSignature; ++more) {
      const auto last = signature.find_last_not_of(" \t\r");
      if (last != std::string::npos && signature[last] == ':') break;
      if (!std::getline(file, line)) break;
      signature += ' ';
      signature += line;
    }
    const auto arrow = signature.rfind("->");
    if (arrow == std::string::npos) continue;
    std::string_view annotation = std::string_view(signature).substr(arrow + 2);
    const auto colon = annotation.rfind(':');
    if (colon != std::string_view::npos)
      annotation = annotation.substr(0, colon);
    const auto begin = annotation.find_first_not_of(" \t\"'");
    const auto end = annotation.find_last_not_of(" \t\r\"'");
    if (begin == std::string_view::npos) continue;
    annotation = annotation.substr(begin, end - begin + 1);
    const auto dot = annotation.rfind('.');
    if (dot != std::string_view::npos) annotation.remove_prefix(dot + 1);
    if (annotation == "Frame") return true;
  }
  return false;
}

class SourceKind final : public KindOperations, public OpensOnRuntime {
 public:
  explicit SourceKind(std::filesystem::path source)
      : m_source(std::move(source)) {}
  bool operator==(const SourceKind& other) const {
    return m_source == other.m_source && m_stated == other.m_stated;
  }
  Kind opensOn(const world::Runtime& runtime) const override {
    SourceKind told = *this;
    told.m_stated.runtime = std::make_shared<const world::Runtime>(runtime);
    return Kind{std::move(told)};
  }
  std::string_view runtime() const override {
    return describesFrame(m_source) ? "scene3d" : "scene2d";
  }
  bool needsDevice() const override {
    return describesFrame(m_source) || declaresDevice(m_source);
  }
  bool available(std::string* why) const override {
    return !needsDevice() || runtimeAvailable(m_stated.opening(), why);
  }
  /** The imported kind is told what this one was, so the stated
   *  runtimes reach the session whichever kind the source turns out to
   *  make. */
  std::unique_ptr<Session> open(weave::FontContext& fonts, Assets& assets,
                                bool deterministic,
                                std::string_view key) const override {
    const Kind imported = m_stated.applied(load(m_source));
    return imported->open(fonts, assets, deterministic, key);
  }

 private:
  std::filesystem::path m_source;
  StatedRuntimes m_stated;
};

/** The answer's result, or the refusal it carries raised as the error a
 *  script sees. */
template <class Result>
Result answered(protocol::Answer<Result> answer) {
  if (!answer) throw std::runtime_error(answer.error().message);
  return std::move(answer.result());
}

/** A HARNESS SESSION OVER ONE FILE: opened under a client's clock,
 *  stepped to the moment it was asked for — or the one it declared, or
 *  one and a half seconds — and photographed under that moving clock at
 *  one pixel per canvas unit, the density its bakes are pinned to from
 *  the first frame. That is the sweep's photograph and the one a written
 *  `--frame` takes: a runtime that re-renders its still draws one frame
 *  more to take it. A step of zero seconds is one frame that moves
 *  nothing. The state the session keeps is a directory of its own, gone
 *  once the picture is copied out. */
std::string renderFile(const std::string& source, const std::string& output,
                       std::optional<double> at) {
  static int rendered = 0;
  const auto path = std::filesystem::absolute(source);
  const std::filesystem::path state =
      std::filesystem::temp_directory_path() /
      ("sigil-render-" + std::to_string(::getpid()) + "-" +
       std::to_string(++rendered));
  const auto destination = std::filesystem::absolute(output);
  {
    testing::InProcessHostOptions options;
    options.stateDirectory = state;
    options.program = "sigil render";
    options.session.pythonLoader = &load;
    testing::InProcessHost host(std::move(options));
    (void)answered(host.clock(protocol::clock::Policy_Advance));
    (void)answered(host.pinDensity(1.0));
    const protocol::session::values::Summary opened =
        answered(host.open(path.string()));
    const double seconds =
        at.value_or(opened.moment >= 0 ? opened.moment : 1.5);
    if (!std::isfinite(seconds) || seconds < 0)
      throw std::invalid_argument("Capture time or frame rate is invalid");
    (void)answered(host.step(seconds));
    const protocol::session::values::StillResult still =
        answered(host.still(1.0));
    std::error_code error;
    std::filesystem::create_directories(destination.parent_path(), error);
    if (!std::filesystem::copy_file(
            still.path, destination,
            std::filesystem::copy_options::overwrite_existing, error))
      throw std::runtime_error("Could not write the sketch capture to " +
                               destination.string() + ": " + error.message());
  }
  std::error_code error;
  std::filesystem::remove_all(state, error);
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
        "Python is already initialized; open a new host process to "
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
  return available(path, modules, {}, why);
}

bool available(const std::filesystem::path& path,
               std::initializer_list<const char*> modules,
               std::initializer_list<std::string_view> cachedUrls,
               std::string* why) {
  const auto unavailable = [why](std::string reason) {
    if (why) *why = std::move(reason);
    return false;
  };
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error))
    return unavailable("Python source is unavailable: " + path.string());
  if (!requireCached(std::span(cachedUrls.begin(), cachedUrls.size()), why))
    return false;
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
    generation->needsDevice =
        py::getattr(result[0], "needsDevice", py::bool_(false)).cast<bool>();
    generation->scene3d =
        py::str(py::getattr(result[0], "__sigil_kind__", py::str("scene2d")))
            .cast<std::string>() == "scene3d";
    return Kind{PythonKind(std::move(generation))};
  } catch (const py::error_already_set& error) {
    throw std::runtime_error(error.what());
  }
}

Kind held(py::handle factory, bool scene3d, bool needsDevice) {
  if (!PyCallable_Check(factory.ptr()))
    throw std::invalid_argument(
        "a held sketch is made by a callable that answers its body");
  auto generation = std::make_shared<Generation>();
  generation->body = std::make_shared<Object>(factory);
  generation->scene3d = scene3d;
  generation->needsDevice = needsDevice;
  return Kind{PythonKind(std::move(generation))};
}

void stageContext(py::handle value, const kit::Stage& stage) {
  if (py::isinstance<Scene3DContextView>(value)) {
    kit::stage(value.cast<const Scene3DContextView&>().context(), stage);
    return;
  }
  const auto state = value.cast<const Context&>().state();
  auto context = state->context();
  kit::stage(context, stage);
}

void bindRuntime(py::module_& module) {
  auto sketches = module.def_submodule("sketch");
  py::class_<AssetsView>(sketches, "Assets")
      .def("hub",
           [](const AssetsView& v) {
             v.state();
             return sigil::python::HubHandle(
                 [v]() -> io::Hub& { return v.state()->assets->hub(); },
                 [v](io::Feed feed) {
                   return v.state()->retainFeed(std::move(feed));
                 },
                 [v](std::shared_ptr<void> value) {
                   v.state()->held.push_back(std::move(value));
                 });
           })
      .def("root",
           [](const AssetsView& v) { return v.state()->assets->root(); });
  module.def("renderFile", &renderFile, py::arg("source"), py::arg("output"),
             py::arg("at") = py::none());
}

}  // namespace sigil::sketch::python
