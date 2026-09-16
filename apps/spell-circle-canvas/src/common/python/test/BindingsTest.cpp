#include <gtest/gtest.h>
#include <pybind11/embed.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilpython/Bindings.h>
#include <sigilpython/Python.h>

#include <memory>
#include <stdexcept>

namespace py = pybind11;

PYBIND11_EMBEDDED_MODULE(_sigil, module) {
  sigil::python::bindLibraries(module);
}

namespace {

py::module_ native() {
  // Native descriptions may release Python values during static teardown.
  // The test interpreter remains available for those final releases.
  static auto* interpreter = new py::scoped_interpreter(false);
  (void)interpreter;
  auto path = py::module_::import("sys").attr("path");
  if (!path.attr("__contains__")(SIGIL_PYTHON_PACKAGE_DIR).cast<bool>())
    path.attr("insert")(0, SIGIL_PYTHON_PACKAGE_DIR);
  return py::module_::import("_sigil");
}

TEST(PythonBindings, CommonRegistrationAndComposeKitNeedNoSketchRuntime) {
  auto module = native();
  ASSERT_FALSE(py::hasattr(module, "sketch"));
  ASSERT_FALSE(py::hasattr(module, "Context"));
  py::dict scope;
  scope["native"] = module;
  py::exec(R"(
import sys
raw = native.compose
props = raw.kit.Caption(label=lambda words, voice: raw.text(words))
node = raw.kit.cell(props, "A", "", raw.box())
page = raw.kit.sheet(raw.kit.Sheet(title="Native"), node)
assert isinstance(page, raw.Element)
assert isinstance(raw.layouts.fr(1), raw.layouts.Track)
assert native.motion.FixedStatus().stepsRun == 0
assert hasattr(raw, "ComposerStats") and hasattr(raw, "TextSettling")
assert "sigil._loader" not in sys.modules
)",
           scope);
}

TEST(PythonBindings, HostLifetimeReleasesRetainedBoundCallbacks) {
  (void)native();
  py::dict scope;
  py::exec(R"(
import weakref
class Owner:
    def value(self):
        return 17
owner = Owner()
weak = weakref.ref(owner)
)",
           scope);
  sigil::python::CallbackLifetime lifetime;
  std::shared_ptr<sigil::python::PythonCallback> held;
  {
    const sigil::python::CallbackScope callbacks(lifetime);
    held = sigil::python::retainCallback(scope["owner"].attr("value"));
  }
  scope.attr("pop")("owner");
  EXPECT_FALSE(scope["weak"]().is_none());
  EXPECT_EQ(held->get()().cast<int>(), 17);
  lifetime.clear();
  EXPECT_TRUE(scope["weak"]().is_none());
  EXPECT_THROW((void)held->get(), std::runtime_error);
  EXPECT_NO_THROW(lifetime.clear());
}

TEST(PythonBindings, CallbackBoundariesUnwindOnlyTheirNativeScopes) {
  (void)native();
  namespace environment = sigil::core::environment;
  struct Scope {
    explicit Scope(int value)
        : native(std::make_unique<environment::Provide<int>>(value)) {}
    std::unique_ptr<environment::Provide<int>> native;
  };
  const auto retain = [](const std::shared_ptr<Scope>& scope) {
    sigil::python::retainScope(scope, [](void* value) noexcept {
      static_cast<Scope*>(value)->native.reset();
    });
  };
  const auto before = environment::capture();
  auto outer = std::make_shared<Scope>(11);
  retain(outer);
  std::shared_ptr<Scope> inner;
  try {
    const sigil::python::CallbackBoundary boundary;
    inner = std::make_shared<Scope>(23);
    retain(inner);
    ASSERT_EQ(*environment::inherited<int>(), 23);
    throw std::runtime_error("intentional callback failure");
  } catch (const std::runtime_error&) {
  }
  EXPECT_FALSE(inner->native);
  EXPECT_TRUE(outer->native);
  EXPECT_EQ(*environment::inherited<int>(), 11);
  EXPECT_TRUE(sigil::python::isCurrentScope(outer.get()));
  sigil::python::closeScope(outer.get());
  EXPECT_FALSE(outer->native);
  EXPECT_EQ(environment::capture(), before);

  // A callback may close an older scope before opening a new one. Its
  // checkpoint identifies creation order rather than the resulting depth.
  auto previous = std::make_shared<Scope>(31);
  retain(previous);
  std::shared_ptr<Scope> replacement;
  {
    const sigil::python::CallbackBoundary boundary;
    sigil::python::closeScope(previous.get());
    replacement = std::make_shared<Scope>(47);
    retain(replacement);
    EXPECT_EQ(*environment::inherited<int>(), 47);
  }
  EXPECT_FALSE(previous->native);
  EXPECT_FALSE(replacement->native);
  EXPECT_EQ(environment::capture(), before);
}

}  // namespace
