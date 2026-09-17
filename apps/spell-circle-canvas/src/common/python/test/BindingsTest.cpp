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

TEST(PythonBindings, WorldOwnsItsTickerFrameAndPixelsWithoutASketchHost) {
  auto module = native();
  py::dict scope;
  scope["native"] = module;
  py::exec(R"(
import gc
import sys
import weakref

scene = native.world.Scene()
angle = native.motion.Output(0)
angle_ref = weakref.ref(angle)
camera = native.geometry.mesh.camera.Camera()
camera.eye = (0, 0, 320)
body = (native.world.Element().key("body")
        .mesh(native.geometry.mesh.box((-65, -35, -20), (65, 35, 20)))
        .fill(native.material.kit.unlit(
            native.material.kit.SurfaceParameters(baseColor="#e75a31")))
        .rotateZ(angle)
        .translateX(native.motion.entrance(0, 30, duration=1)))
frame = native.world.Frame(body).camera(camera)
scene.render(frame)
first = scene.image((96, 96)).rgba()
assert any(first[3::4])
handle = scene.handleOf("body")
angle.value = 90
scene.advance(0.25)
quarter = scene.image((96, 96)).rgba()
assert first != quarter

del angle, frame, body, camera
gc.collect()
assert angle_ref() is None
scene.advance(0.25)
assert scene.handleOf("body") == handle
image = scene.image((96, 96))
half = image.rgba()
assert half != quarter
assert any(half[3::4])
del scene
gc.collect()
assert image.rgba() == half
assert not hasattr(native, "Context")
assert "sigil._loader" not in sys.modules
)",
           scope);
}

TEST(PythonBindings, WeaveLayoutsAndResourceLeasesOwnTheirDependencies) {
  auto module = native();
  py::dict scope;
  scope["native"] = module;
  py::exec(R"(
import gc
import tempfile
import weakref
from pathlib import Path

w = native.weave
fonts = w.FontContext()
paragraph = w.Paragraph("hyphenation demonstration", w.Type(size=20))
patterns = w.kit.PatternHyphenator("en", w.kit.englishHyphenationPatterns())
pattern_ref = weakref.ref(patterns)
layout = w.layoutParagraph(fonts, paragraph, w.BlockFlow((0, 0, 140, 200)), hyphenator=patterns)
del patterns
gc.collect()
assert pattern_ref() is not None
outline = layout.glyphOutline()
assert not outline.isEmpty()
del fonts, paragraph
gc.collect()
assert pattern_ref() is None
assert layout.glyphOutline().getBounds().width() == outline.getBounds().width()

layer = w.PaintLayer(material=native.material.field.noise(0.1))
copy = layer.copy()
layer.material = None
gc.collect()
assert copy.material is not None

with tempfile.TemporaryDirectory() as folder:
    Path(folder, "sample.txt").write_text("sample")
    hub = native.io.Hub()
    hub.mount("res://", Path(folder))
    lease = hub.retain("res://*.txt")
    del hub
    gc.collect()
    assert lease.preload() == 1
    assert lease.uris() == ["res://sample.txt"]
    lease.close()
    lease.close()
    try:
        lease.refresh()
        assert False, "A closed lease must reject access"
    except RuntimeError:
        pass
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
