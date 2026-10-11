/** @file
 * Python sketch sessions and reload failures through the ordinary live host.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontTypes.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <pybind11/pybind11.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Feed.h>
#include <sigilpython/skia/Values.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/advanced/Skia.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ScratchDir.h"
#include "support/Devices.h"
#include "support/Fixtures.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::fonts;

class PythonSource {
 public:
  explicit PythonSource(std::string_view name) : scratch(name) {}

  void write(const std::string& name, std::string_view source) {
    scratch.write(name, source);
    // Edits differ below one second, so timestamp-based bytecode caches
    // would reuse stale code for same-size changes.
    fs::last_write_time(scratch.path / name,
                        m_epoch + std::chrono::milliseconds(++m_edit));
  }

  [[nodiscard]] fs::path entry() const { return scratch.path / "entry.py"; }

  [[nodiscard]] sketch::Host::Options options() const {
    sketch::Host::Options result;
    result.pythonLoader = &sketch::python::load;
    result.sketchPath = entry();
    result.assetsDirectory = scratch.path;
    result.siblingScanInterval = std::chrono::milliseconds(0);
    result.clock = sigil::motion::ClockPolicy::Advance;
    return result;
  }

  sigil::test::ScratchDir scratch;

 private:
  fs::file_time_type m_epoch =
      fs::file_time_type{std::chrono::duration_cast<std::chrono::seconds>(
          fs::file_time_type::clock::now().time_since_epoch())};
  int m_edit = 0;
};

constexpr std::string_view kGood =
    "class Study:\n"
    "    def setup(self, ctx):\n"
    "        ctx.canvas(120, 90)\n";

struct FeedCounts {
  std::map<std::string, int> opened;
  std::map<std::string, int> closed;
  std::map<std::string, sigil::io::Inlet> feeds;
};

struct FeedFixture {
  std::shared_ptr<FeedCounts> counts = std::make_shared<FeedCounts>();
  sketch::Assets services{fs::path{}};

  FeedFixture() {
    sigil::io::registerTransport(
        services.hub(), "fixture",
        [counts = counts](std::string_view uri, sigil::io::Inlet inlet) {
          const std::string key(uri);
          ++counts->opened[key];
          counts->feeds[key] = std::move(inlet);
          sigil::io::TransportEnd result;
          result.close = [counts, key] { ++counts->closed[key]; };
          result.send = [](const sigil::io::Bytes&) { return true; };
          return result;
        });
  }
};

TEST(SketchPython, ASessionClosesFeedsDespiteEscapedPythonWrappers) {
  PythonSource source("sigil_python_feed_escape");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        hub = ctx.assets.hub()
        builtins._sigil_escaped_feed = hub.listen('fixture://input')
        builtins._sigil_escaped_hub = hub
        hub.listen('fixture://unreferenced')
)PY");
  auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "first");
  EXPECT_EQ(fixture.counts->closed["fixture://unreferenced"], 0);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://unreferenced"], 1);
  EXPECT_TRUE(fixture.counts->feeds["fixture://input"].expired());

  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        try:
            for action in (builtins._sigil_escaped_feed.latest,
                           lambda: builtins._sigil_escaped_hub.listen('fixture://input')):
                try:
                    action()
                except RuntimeError as error:
                    assert 'closed session' in str(error)
                else:
                    raise AssertionError('An expired session service remained usable')
        finally:
            del builtins._sigil_escaped_feed, builtins._sigil_escaped_hub
        self.feed = ctx.assets.hub().listen('fixture://input')
        assert self.feed.send(b'reopened')
)PY");
  kind = sketch::python::load(source.entry());
  session = kind->open(fonts(), fixture.services, true, "second");
  EXPECT_EQ(fixture.counts->opened["fixture://input"], 2);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 2);
}

TEST(SketchPython, ASessionClosesConnectionsDespiteEscapedPythonWrappers) {
  PythonSource source("sigil_python_connection_escape");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
import builtins
from sigil import data
class Study:
    def setup(self, ctx):
        builtins._sigil_escaped_connection = data.connect(
            ctx.assets.hub(), 'fixture://connection')
)PY");
  auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "first");
  EXPECT_EQ(fixture.counts->opened["fixture://connection"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://connection"], 0);
  EXPECT_FALSE(fixture.counts->feeds["fixture://connection"].expired());
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://connection"], 1);
  EXPECT_TRUE(fixture.counts->feeds["fixture://connection"].expired());

  source.write("entry.py", R"PY(
import builtins
class Study:
    def setup(self, ctx):
        try:
            builtins._sigil_escaped_connection.state()
        except RuntimeError as error:
            assert 'closed session' in str(error)
        else:
            raise AssertionError('An escaped connection outlived its session')
        finally:
            del builtins._sigil_escaped_connection
)PY");
  kind = sketch::python::load(source.entry());
  session = kind->open(fonts(), fixture.services, true, "second");
  session.reset();
}

TEST(SketchPython, OverlappingAndFailedGenerationsShareTheWorkingFeed) {
  PythonSource source("sigil_python_feed_overlap");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        self.feed = ctx.assets.hub().listen('fixture://shared')
    def update(self, elapsed, ctx):
        assert self.feed.send(b'working')
)PY");
  const auto good = sketch::python::load(source.entry());
  auto first = good->open(fonts(), fixture.services, true, "first");
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        ctx.assets.hub().listen('fixture://shared')
        ctx.assets.hub().listen('fixture://candidate')
        raise RuntimeError('candidate rejected')
)PY");
  const auto broken = sketch::python::load(source.entry());
  EXPECT_THROW((void)broken->open(fonts(), fixture.services, true, "broken"),
               std::runtime_error);
  EXPECT_EQ(fixture.counts->closed["fixture://candidate"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 0);
  auto replacement = good->open(fonts(), fixture.services, true, "replacement");
  first.reset();
  EXPECT_EQ(fixture.counts->opened["fixture://shared"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 0);
  SkNoDrawCanvas canvas(100, 100);
  EXPECT_NO_THROW(replacement->frame(canvas, 1.0 / 60.0));
  replacement.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://shared"], 1);
}

TEST(SketchPython, RedeclaringReleasesOmittedFeedsAndKeepsMatchingOnesOpen) {
  PythonSource source("sigil_python_feed_redeclare");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        first = not hasattr(self, 'feed')
        self.feed = ctx.assets.hub().listen('fixture://retained')
        if first:
            ctx.assets.hub().listen('fixture://removed')
        assert self.feed.send(b'working')
)PY");
  const auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "declared");
  session->redeclare();
  EXPECT_EQ(fixture.counts->opened["fixture://retained"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://retained"], 0);
  EXPECT_EQ(fixture.counts->closed["fixture://removed"], 1);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://retained"], 1);
  EXPECT_EQ(fixture.counts->closed["fixture://removed"], 1);
}

TEST(SketchPython, AFailedFrameReleasesTheSessionsLiveInputs) {
  PythonSource source("sigil_python_feed_failed_frame");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        self.feed = ctx.assets.hub().listen('fixture://input')
    def update(self):
        raise RuntimeError('frame rejected')
)PY");
  const auto kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), fixture.services, true, "failed");
  SkNoDrawCanvas canvas(100, 100);
  EXPECT_THROW(session->frame(canvas, 1.0 / 60.0), std::runtime_error);
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 1);
}

TEST(SketchPython, ASketchThatNeedsTheDeviceIsUnavailableWithoutOne) {
  PythonSource source("sigil_python_needs_device");
  source.write("entry.py", R"PY(
class Study:
    needsDevice = True
    def setup(self, ctx):
        pass
)PY");
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_TRUE(kind->needsDevice());
  sketch::useRuntime({});
  std::string why;
  EXPECT_FALSE(kind->available(&why));
  EXPECT_FALSE(why.empty());
  std::string expected;
  EXPECT_FALSE(sketch::runtimeAvailable(&expected));
  EXPECT_EQ(why, expected);

  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        pass
)PY");
  const sketch::Kind plain = sketch::python::load(source.entry());
  EXPECT_FALSE(plain->needsDevice());
  EXPECT_TRUE(plain->available(&why));

  // A SOURCE KIND ANSWERS THE SAME FROM THE FILE'S TEXT, before anything
  // is imported: a listing asks every source before one opens.
  source.write("entry.py", R"PY(
class Study:
    needsDevice = True
    def setup(self, ctx):
        pass
)PY");
  const sketch::Kind listed = sketch::python::source(source.entry());
  EXPECT_TRUE(listed->needsDevice());
  EXPECT_FALSE(listed->available(&why));
  EXPECT_EQ(why, expected);
}

TEST(SketchPython, ASourceKindCanBeListedWhenItsFileIsMissingOrInvalid) {
  PythonSource source("sigil_python_source_kind_listing");
  const sketch::Kind kind = sketch::python::source(source.entry());
  ASSERT_TRUE(kind);
  EXPECT_EQ(kind->runtime(), "scene2d");
  EXPECT_FALSE(kind->needsDevice());
  EXPECT_THROW((void)kind->open(fonts(), assets(), true, "missing"),
               std::runtime_error);

  source.write("entry.py", "class Study(:\n");
  const sketch::Kind invalid = sketch::python::source(source.entry());
  EXPECT_EQ(invalid->runtime(), "scene2d");
  EXPECT_THROW((void)invalid->open(fonts(), assets(), true, "invalid"),
               std::runtime_error);

  source.write("entry.py", kGood);
  auto session = kind->open(fonts(), assets(), true, "repaired");
  EXPECT_EQ(session->canvas().size, SkSize::Make(120, 90));
}

TEST(SketchPython,
     AvailabilityChecksSourcesAndModulesWithoutExecutingTheSketch) {
  PythonSource source("sigil_python_source_availability");
  std::string why;
  EXPECT_FALSE(sketch::python::available(source.entry(), {}, &why));
  EXPECT_NE(why.find(source.entry().string()), std::string::npos);

  source.write("entry.py",
               "raise RuntimeError('Do not execute this sketch')\n");
  EXPECT_TRUE(sketch::python::available(source.entry(), {}));
  EXPECT_TRUE(sketch::python::available(source.entry(), {"math"}));
  EXPECT_FALSE(sketch::python::available(
      source.entry(), {"_sigil_missing_requirement_for_availability"}, &why));
  EXPECT_NE(why.find("_sigil_missing_requirement_for_availability"),
            std::string::npos);

  // A dotted lookup whose parent does not exist raises during discovery.
  EXPECT_FALSE(sketch::python::available(
      source.entry(), {"_sigil_missing_requirement_for_availability.child"},
      &why));
  EXPECT_NE(why.find("discovery failed"), std::string::npos);
}

TEST(SketchPython, ArtNobodyFetchedStandsTheEntryDownBeforePythonStarts) {
  PythonSource source("sigil_python_source_cached_art");
  source.write("entry.py",
               "raise RuntimeError('Do not execute this sketch')\n");
  const char* url = "https://sketch.invalid/art/python_entry.png";

  // The cache is read before the interpreter is asked anything, so the
  // reason is the missing art and not the missing module behind it.
  std::string why;
  EXPECT_FALSE(sketch::python::available(
      source.entry(), {"_sigil_missing_requirement_for_availability"}, {url},
      &why));
  EXPECT_NE(why.find(url), std::string::npos);
  EXPECT_EQ(why.find("_sigil_missing_requirement_for_availability"),
            std::string::npos);

  // An entry that asks for no fetched art is the check that was there
  // before it.
  EXPECT_TRUE(sketch::python::available(source.entry(), {}, {}, &why));
}

TEST(SketchPython, ASourceKindImportsCurrentCodeEachTimeItOpens) {
  PythonSource source("sigil_python_source_kind_edits");
  source.write("entry.py", kGood);
  const sketch::Kind kind = sketch::python::source(source.entry());
  auto first = kind->open(fonts(), assets(), true, "first");
  source.write("entry.py",
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(240, 90)\n");
  auto second = kind->open(fonts(), assets(), true, "second");
  EXPECT_EQ(first->canvas().size, SkSize::Make(120, 90));
  EXPECT_EQ(second->canvas().size, SkSize::Make(240, 90));
}

TEST(SketchPython, AnInitializedInterpreterRejectsEnvironmentReconfiguration) {
  PythonSource source("sigil_python_initialized_environment");
  source.write("entry.py", kGood);
  const sketch::Kind kind = sketch::python::load(source.entry());
  try {
    sketch::python::configureInterpreter({});
    FAIL() << "An initialized interpreter accepted another configuration";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("already initialized"),
              std::string::npos);
  }
  auto session = kind->open(fonts(), assets(), true, "retained");
  EXPECT_EQ(session->canvas().size, SkSize::Make(120, 90));
}

TEST(SketchPython, ASourceKindIsolatesLocalModulesBetweenOpenedSessions) {
  PythonSource source("sigil_python_source_kind_generations");
  source.write("model.py", "frames = 0\n");
  source.write("entry.py",
               "from . import model\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(100, 80)\n"
               "    def update(self, elapsed, ctx):\n"
               "        model.frames += 1\n"
               "        ctx.canvas(100 + model.frames, 80)\n");
  const sketch::Kind kind = sketch::python::source(source.entry());
  auto first = kind->open(fonts(), assets(), true, "first");
  auto second = kind->open(fonts(), assets(), true, "second");
  SkNoDrawCanvas canvas(200, 200);
  first->frame(canvas, 1.0 / 60.0);
  first->frame(canvas, 1.0 / 60.0);
  second->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(first->canvas().size, SkSize::Make(102, 80));
  EXPECT_EQ(second->canvas().size, SkSize::Make(101, 80));
}

TEST(SketchPython, ARegisteredSourceLoadsOnceAndReloadsOnlyAfterAnEdit) {
  PythonSource source("sigil_python_registered_source");
  const std::string code =
      "from pathlib import Path\n"
      "counter = Path(__file__).with_suffix('.imports')\n"
      "imports = int(counter.read_text()) + 1 if counter.exists() else 1\n"
      "counter.write_text(str(imports))\n"
      "class Study:\n"
      "    def setup(self, ctx):\n"
      "        ctx.canvas(100 + imports, 80)\n";
  source.write("entry.py", code);
  const sketch::Entry entry{"entry", "entry", "Python", "",
                            +[]() -> sketch::Kind { return {}; }};
  auto options = source.options();
  options.compiledIn = &entry;
  sketch::Host host(options, fonts());
  ASSERT_TRUE(host.live()) << host.errorLog();
  ASSERT_EQ(host.canvasSize(), SkSize::Make(101, 80));
  EXPECT_EQ(host.generation(), 1);
  auto* initial = host.session();
  host.poll();
  EXPECT_EQ(host.session(), initial);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(101, 80));
  EXPECT_EQ(host.generation(), 1);

  EXPECT_TRUE(host.restartSession());
  host.poll();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(101, 80));
  EXPECT_EQ(host.generation(), 1);

  source.write("entry.py", code + "\n# saved\n");
  host.poll();
  ASSERT_EQ(host.canvasSize(), SkSize::Make(102, 80)) << host.errorLog();
  EXPECT_EQ(host.generation(), 2);
  host.poll();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(102, 80));
  EXPECT_EQ(host.generation(), 2);

  auto* previous = host.session();
  source.write("entry.py", "class Study(:\n");
  host.poll();
  EXPECT_EQ(host.session(), previous);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(102, 80));
  EXPECT_NE(host.errorLog().find("SyntaxError"), std::string::npos);
  EXPECT_EQ(host.generation(), 3);
  host.poll();
  EXPECT_EQ(host.generation(), 3);

  source.write("entry.py", code);
  host.poll();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(103, 80));
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.generation(), 4);
  host.poll();
  EXPECT_EQ(host.generation(), 4);
}

struct FailedEdit {
  const char* name;
  const char* code;
  const char* error;
};
class PythonFailedEdit : public ::testing::TestWithParam<FailedEdit> {};

TEST_P(PythonFailedEdit, KeepsTheLastSessionAndRecoversOnTheNextSave) {
  const auto& failure = GetParam();
  PythonSource source(std::string("sigil_python_failed_") + failure.name);
  source.write("entry.py", kGood);
  sketch::Host host(source.options(), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  ASSERT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  sketch::Session* previous = host.session();

  source.write("entry.py", failure.code);
  host.poll();
  EXPECT_EQ(host.session(), previous);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));
  EXPECT_NE(host.errorLog().find(failure.error), std::string::npos)
      << host.errorLog();
  EXPECT_FALSE(host.compiling());
  const int failedGeneration = host.generation();
  host.poll();
  EXPECT_EQ(host.generation(), failedGeneration);
  EXPECT_TRUE(host.restartSession());
  EXPECT_EQ(host.canvasSize(), SkSize::Make(120, 90));

  source.write("entry.py",
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(240, 180)\n");
  host.poll();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(240, 180));
  EXPECT_TRUE(host.errorLog().empty()) << host.errorLog();
  EXPECT_EQ(host.generation(), failedGeneration + 1);
}

INSTANTIATE_TEST_SUITE_P(
    ImportsAndSetup, PythonFailedEdit,
    ::testing::Values(
        FailedEdit{"Syntax", "class Study(:\n", "SyntaxError"},
        FailedEdit{"Import", "raise RuntimeError('import rejected')\n",
                   "import rejected"},
        FailedEdit{"Setup",
                   "class Study:\n"
                   "    def setup(self, ctx):\n"
                   "        ctx.canvas(640, 480)\n"
                   "        raise RuntimeError('setup rejected')\n",
                   "setup rejected"}),
    [](const ::testing::TestParamInfo<FailedEdit>& info) {
      return info.param.name;
    });

TEST(SketchPython, SameSizeHelperEditsReadFreshSourceWithinTheSameSecond) {
  PythonSource source("sigil_python_helper_edit");
  source.write("palette.py", "width = 120\n");
  source.write("entry.py",
               "from .palette import width\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(width, 90)\n");
  sketch::Host host(source.options(), fonts());
  host.poll();
  ASSERT_EQ(host.canvasSize(), SkSize::Make(120, 90)) << host.errorLog();
  const int generation = host.generation();
  source.write("palette.py", "width = 240\n");
  host.poll();
  EXPECT_EQ(host.generation(), generation + 1);
  EXPECT_EQ(host.canvasSize(), SkSize::Make(240, 90)) << host.errorLog();
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchPython, RemovingAHelperPreservesTheSessionAndRestoringItReloads) {
  PythonSource source("sigil_python_helper_removed");
  source.write("palette.py", "width = 120\n");
  source.write("entry.py",
               "from .palette import width\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(width, 90)\n");
  sketch::Host host(source.options(), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  sketch::Session* previous = host.session();
  fs::remove(source.scratch.path / "palette.py");
  host.poll();
  EXPECT_EQ(host.session(), previous);
  EXPECT_NE(host.errorLog().find("ModuleNotFoundError"), std::string::npos);
  source.write("palette.py", "width = 240\n");
  host.poll();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(240, 90)) << host.errorLog();
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchPython, TwoSessionsHaveIndependentInstanceState) {
  PythonSource source("sigil_python_independent_sessions");
  source.write("entry.py",
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        self.frames = 0\n"
               "        ctx.canvas(100, 80)\n"
               "    def update(self, elapsed, ctx):\n"
               "        self.frames += 1\n"
               "        ctx.canvas(100 + self.frames, 80)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto first = kind->open(fonts(), assets(), true, "first");
  auto second = kind->open(fonts(), assets(), true, "second");
  SkNoDrawCanvas canvas(200, 200);
  first->frame(canvas, 1.0 / 60.0);
  first->frame(canvas, 1.0 / 60.0);
  second->frame(canvas, 1.0 / 60.0);
  EXPECT_EQ(first->canvas().size, SkSize::Make(102, 80));
  EXPECT_EQ(second->canvas().size, SkSize::Make(101, 80));
}

TEST(SketchPython, ADeclaredUnpaintedCaptureStepLeavesTheDrawToTheStill) {
  PythonSource source("sigil_python_unpainted_capture");
  source.write("entry.py",
               "import builtins\n"
               "from sigil.sketch import CanvasSpecification, sketch\n"
               "builtins._sigil_capture_counts = {'draw': 0, 'update': 0}\n"
               "@sketch(size=(64, 48), capture_at=0.5)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(CanvasSpecification(size=(64, 48),\n"
               "            captureSeconds=0.5, paintDiscardedFrames=False))\n"
               "    def update(self):\n"
               "        builtins._sigil_capture_counts['update'] += 1\n"
               "    def draw(self, pen):\n"
               "        builtins._sigil_capture_counts['draw'] += 1\n");
  sketch::Host host(source.options(), fonts());
  host.poll();
  ASSERT_TRUE(host.live()) << host.errorLog();
  const auto counts = [](const char* name) {
    const pybind11::gil_scoped_acquire lock;
    return pybind11::module_::import("builtins")
        .attr("_sigil_capture_counts")[name]
        .cast<int>();
  };
  // Every step to the moment advances the body and none of them paints.
  EXPECT_EQ(host.prepareCapture(), 0.5);
  EXPECT_GT(counts("update"), 1);
  EXPECT_EQ(counts("draw"), 0);
  // The still is the one draw the capture pays.
  EXPECT_FALSE(host.still().isNull());
  EXPECT_EQ(counts("draw"), 1);
}

/** The typeface a Python setup left in `builtins._sigil_bundled_face`,
 *  taken out of Python and the name cleared. */
sk_sp<SkTypeface> bundledFaceFromPython() {
  const pybind11::gil_scoped_acquire lock;
  pybind11::module_ builtins = pybind11::module_::import("builtins");
  sk_sp<SkTypeface> face =
      builtins.attr("_sigil_bundled_face").cast<sk_sp<SkTypeface>>();
  builtins.attr("__dict__").attr("pop")("_sigil_bundled_face");
  return face;
}

std::string familyOf(const SkTypeface* face) {
  SkString name;
  if (face) face->getFamilyName(&name);
  return name.c_str();
}

float advanceOf(sk_sp<SkTypeface> face, const char* text) {
  const SkFont font(std::move(face), 32);
  return font.measureText(text, std::strlen(text), SkTextEncoding::kUTF8);
}

TEST(SketchPython, ABundledFaceIsOneResourceToBothLanguages) {
  PythonSource source("sigil_python_bundled_face");
  const fs::path instruments = SIGIL_TEST_INSTRUMENT_DIR;
  const fs::path bundled = source.scratch.path / "face.ttf";
  fs::copy_file(instruments / "Sans.ttf", bundled);
  source.write("entry.py",
               "import builtins\n"
               "from sigil import skia\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(40, 40)\n"
               "        builtins._sigil_bundled_face = ctx.assets.hub().load(\n"
               "            skia.Typeface, 'res://face.ttf')\n");
  sketch::Assets services(source.scratch.path);
  const sketch::Kind kind = sketch::python::load(source.entry());
  sk_sp<SkTypeface> python;
  {
    auto session = kind->open(fonts(), services, true, "faced");
    python = bundledFaceFromPython();
  }
  ASSERT_NE(python, nullptr) << "the Python host read no face";
  const auto native = services.hub().load<sigil::weave::Face>("res://face.ttf");
  ASSERT_TRUE(native && *native) << "the native host read no face";
  // One resource: the face the hub keeps for the URI, whichever language
  // asked, with the family and the advances of the file it came from.
  EXPECT_EQ(python.get(), sigil::weave::borrowSk(*native));
  const sk_sp<SkTypeface> opened =
      sigil::weave::ports::systemFontManager()->makeFromFile(
          (instruments / "Sans.ttf").c_str());
  ASSERT_NE(opened, nullptr);
  EXPECT_EQ(familyOf(python.get()), familyOf(opened.get()));
  EXPECT_FLOAT_EQ(advanceOf(python, "Waveform"), advanceOf(opened, "Waveform"));

  // Replaced on disk: a session opened afterwards reads the new face and
  // nothing of the old one.
  const sk_sp<SkTypeface> replacement =
      sigil::weave::ports::systemFontManager()->makeFromFile(
          (instruments / "HanSerif.ttf").c_str());
  ASSERT_NE(replacement, nullptr);
  ASSERT_NE(familyOf(replacement.get()), familyOf(opened.get()));
  fs::copy_file(instruments / "HanSerif.ttf", bundled,
                fs::copy_options::overwrite_existing);
  fs::last_write_time(bundled,
                      fs::last_write_time(bundled) + std::chrono::seconds(5));
  sigil::io::poll(services.hub());
  {
    auto session = kind->open(fonts(), services, true, "faced");
    python = bundledFaceFromPython();
  }
  ASSERT_NE(python, nullptr);
  EXPECT_EQ(familyOf(python.get()), familyOf(replacement.get()));
  const auto reloaded =
      services.hub().load<sigil::weave::Face>("res://face.ttf");
  ASSERT_TRUE(reloaded && *reloaded);
  EXPECT_EQ(python.get(), sigil::weave::borrowSk(*reloaded));
}

TEST(SketchPython, AContextKeptOutsideItsSessionFailsSafelyAfterTeardown) {
  PythonSource source("sigil_python_context_lifetime");
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        builtins._sigil_expired_context = ctx\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "saved");
  }
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(1, 1)\n"
               "        try:\n"
               "            builtins._sigil_expired_context.canvas(300, 200)\n"
               "        except RuntimeError:\n"
               "            ctx.canvas(80, 60)\n"
               "        finally:\n"
               "            del builtins._sigil_expired_context\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

TEST(SketchPython, AStoredElementDoesNotKeepItsSketchAliveAfterTeardown) {
  PythonSource source("sigil_python_callback_cycle");
  source.write("entry.py",
               "import builtins\n"
               "import weakref\n"
               "from sigil.compose import graphics\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(80, 60)\n"
               "        self.node = graphics('loop', self.draw).size(80, 60)\n"
               "        ctx.render(self.node)\n"
               "        builtins._sigil_callback_owner = weakref.ref(self)\n"
               "    def draw(self, pen):\n"
               "        pen.circle(40, 30, 20)\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "saved");
  }
  source.write("entry.py",
               "import builtins\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        previous = builtins._sigil_callback_owner\n"
               "        del builtins._sigil_callback_owner\n"
               "        assert previous() is None, 'A stored graphics callback "
               "retained its sketch'\n"
               "        ctx.canvas(80, 60)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

TEST(SketchPython, ANewHelperLoadsWhenTheDirectoryTimestampDidNotChange) {
  PythonSource source("sigil_python_helper_directory_cache");
  source.write("entry.py",
               "from .palette import width\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(width, 90)\n");
  sketch::Host host(source.options(), fonts());
  host.poll();
  ASSERT_FALSE(host.live());
  ASSERT_NE(host.errorLog().find("ModuleNotFoundError"), std::string::npos);

  const auto directoryTime = fs::last_write_time(source.scratch.path);
  source.write("palette.py", "width = 240\n");
  fs::last_write_time(source.scratch.path, directoryTime);
  host.poll();
  EXPECT_TRUE(host.live()) << host.errorLog();
  EXPECT_EQ(host.canvasSize(), SkSize::Make(240, 90));
  EXPECT_TRUE(host.errorLog().empty());
}

TEST(SketchPython, AGlobalGraphicsCallbackDoesNotKeepItsGenerationAlive) {
  PythonSource source("sigil_python_global_callback_lifetime");
  source.write("entry.py",
               "import builtins\n"
               "import weakref\n"
               "from sigil.compose import graphics\n"
               "class Token:\n"
               "    pass\n"
               "owner = Token()\n"
               "builtins._sigil_generation_owner = weakref.ref(owner)\n"
               "def draw(pen):\n"
               "    pass\n"
               "GLOBAL = graphics('global', draw)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.render(GLOBAL)\n");
  {
    const sketch::Kind kind = sketch::python::load(source.entry());
    auto session = kind->open(fonts(), assets(), true, "global");
  }
  source.write("entry.py",
               "import builtins\n"
               "import gc\n"
               "gc.collect()\n"
               "try:\n"
               "    assert builtins._sigil_generation_owner() is None\n"
               "finally:\n"
               "    del builtins._sigil_generation_owner\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(80, 60)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "checked");
  EXPECT_EQ(session->canvas().size, SkSize::Make(80, 60));
}

/** A CASE THAT BAKES A 3D FRAME from Python: the frame is drawn on the
 *  device, so the device runtime is installed for the process before it
 *  and taken away after, or the case skips naming what is missing. */
class SketchPythonBakes : public testing::Test {
 protected:
  void SetUp() override {
    std::string why;
    if (!sketch::test::installDeviceRuntime(&why))
      GTEST_SKIP() << "no Vulkan device: " << why;
  }
  void TearDown() override { sketch::useRuntime({}); }
};

TEST_F(SketchPythonBakes, APageThePythonSketchPaintsIsWornByTheBakedCard) {
  PythonSource source("sigil_python_page_bake");
  // One green page that takes no light, worn by a card under no light at
  // all: what the card shows is the page, and the bake fills the canvas.
  source.write(
      "entry.py",
      "from sigil import compose, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 96), background='#1a1a1a', capture_at=0)\n"
      "class Paged:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        self.page = ctx.page((32, 32))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        card = (world.mesh(mesh.quad(100, 60)).key('card')\n"
      "                .fill(self.page.surface()))\n"
      "        tree = compose.box().width(32).height(32)"
      ".fill('#00ff00')\n"
      "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
      "        frame = (world.Frame(world.group(sun, card).key('set'))\n"
      "                 .pass_(self.page.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 96), "
      "camera=lens, background='#1a1a1a')\n"
      "        ctx.render(compose.image(still).width(128).height(96)\n"
      "                   if still is not None else compose.box())\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  ASSERT_TRUE(kind->needsDevice());
  auto session = kind->open(fonts(), assets(), true, "paged");
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(128, 96));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(128, 96));
  ASSERT_TRUE(surface->readPixels(pixels, 0, 0));
  // The page's green is the card's base colour, lit by the sun.
  const SkColor middle = pixels.getColor(64, 48);
  EXPECT_GT(SkColorGetG(middle), 100u);
  EXPECT_GT(SkColorGetG(middle), SkColorGetR(middle) + 60);
  EXPECT_GT(SkColorGetG(middle), SkColorGetB(middle) + 60);
  // Off the card is the ground the bake was cleared to, not the page.
  EXPECT_LT(SkColorGetG(pixels.getColor(1, 1)), 50u);
}

/** The still a Python source bakes, drawn into a raster canvas of
 *  @p width by @p height and read back. */
SkBitmap bakedStill(const PythonSource& source, std::string_view key, int width,
                    int height) {
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, std::string(key));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(width, height));
  if (!surface->readPixels(pixels, 0, 0)) return {};
  return pixels;
}

TEST_F(SketchPythonBakes, TwoCardsWearTheTwoHalvesOfOnePage) {
  PythonSource source("sigil_python_page_regions");
  // One page, red on its left half and blue on its right, and two cards
  // side by side, each wearing one half: each card shows its own half's
  // colour across the whole of it.
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 64), background='#000000', capture_at=0)\n"
      "class Halves:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        page = ctx.page((64, 32))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        left = material.PixelRect(x=0, y=0, width=32, height=32)\n"
      "        right = material.PixelRect(x=32, y=0, width=32, height=32)\n"
      "        cards = [\n"
      "            world.mesh(mesh.quad(100, 100)).key(name).at((x, 0, 0))\n"
      "            .fill(page.surface(region))\n"
      "            for name, x, region in (('left', -60, left),\n"
      "                                    ('right', 60, right))]\n"
      "        tree = compose.box().width(64).height(32).children(\n"
      "            compose.box().absolute().rect(0, 0, 32, 32)"
      ".fill('#ff0000'),\n"
      "            compose.box().absolute().rect(32, 0, 32, 32)"
      ".fill('#0000ff'))\n"
      "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
      "        frame = (world.Frame(world.group(sun, *cards).key('set'))\n"
      "                 .pass_(page.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 64), camera=lens,\n"
      "                                background='#000000')\n"
      "        ctx.render(compose.image(still).width(128).height(64)\n"
      "                   if still is not None else compose.box())\n");
  const SkBitmap pixels = bakedStill(source, "halves", 128, 64);
  ASSERT_FALSE(pixels.isNull());
  const SkColor left = pixels.getColor(46, 32);
  const SkColor right = pixels.getColor(82, 32);
  EXPECT_GT(SkColorGetR(left), SkColorGetB(left) + 60)
      << "the left card wears the page's red half";
  EXPECT_GT(SkColorGetB(right), SkColorGetR(right) + 60)
      << "the right card wears the page's blue half";
}

TEST_F(SketchPythonBakes, AStatedEmissionRangeKeepsABrighterEmissionUnclipped) {
  PythonSource source("sigil_python_page_range");
  // Two pages painting the same emission of three, one stating its
  // emissive map's white at four and one at one. Seen through an exposure
  // of a fifth on a linear curve, the first card shows the three it was
  // painted with and the second the one its range clips it to.
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(128, 64), background='#000000', capture_at=0)\n"
      "class Ranged:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        wide = ctx.page((32, 32), compose.SurfaceRange(emission=4))\n"
      "        narrow = ctx.page((32, 32),\n"
      "                          range=compose.SurfaceRange(emission=1))\n"
      "        glow = material.from_(material.Color(0, 0, 0, 1)).surface(\n"
      "            emission=material.Color(1, 1, 1, 1), emissionStrength=3)\n"
      "        tree = compose.box().width(32).height(32).fill(glow)\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        eye = world.camera(lens, exposure=0.2,\n"
      "                           "
      "toneCurve=world.ToneCurve.Linear).key('eye')\n"
      "        cards = [\n"
      "            world.mesh(mesh.quad(100, 100)).key(name).at((x, 0, 0))\n"
      "            .fill(page.surface())\n"
      "            for name, x, page in (('wide', -60, wide),\n"
      "                                  ('narrow', 60, narrow))]\n"
      "        frame = (world.Frame(world.group(eye, *cards).key('set'))\n"
      "                 .pass_(wide.render(tree))\n"
      "                 .pass_(narrow.render(tree)))\n"
      "        still = ctx.bakeScene3D(frame, (128, 64), camera=lens,\n"
      "                                background='#000000')\n"
      "        ctx.render(compose.image(still).width(128).height(64)\n"
      "                   if still is not None else compose.box())\n");
  const SkBitmap pixels = bakedStill(source, "ranged", 128, 64);
  ASSERT_FALSE(pixels.isNull());
  const SkColor wide = pixels.getColor(46, 32);
  const SkColor narrow = pixels.getColor(82, 32);
  EXPECT_GT(SkColorGetG(narrow), 40u) << "the clipped card still glows";
  EXPECT_GT(SkColorGetG(wide), SkColorGetG(narrow) + 40)
      << "the range of four keeps the emission of three it was painted with";
}

TEST_F(SketchPythonBakes, ASizedLightsShadowIsSofterFartherFromItsCaster) {
  PythonSource source("sigil_python_shadow_size");
  // Seen from straight above, a white ground lit at forty-five degrees
  // from the left by a sun casting shadows, and two slabs hung over it at
  // the left, one low over the lower half and one high over the upper:
  // each throws its shadow to the right by its height. The sun spans
  // eight degrees in the right-hand bake and is a point in the left-hand
  // one, so on the right the high slab's shadow edge is soft and the low
  // one's nearly sharp, and on the left both are sharp.
  source.write(
      "entry.py",
      "from sigil import material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(512, 256), background='#000000', capture_at=0)\n"
      "class Shadowed:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 400)\n"
      "        lens.target = (0, 0, 0)\n"
      "        matte = material.from_('#e0e0e0').surface(roughness=1.0)\n"
      "        def scene(size):\n"
      "            ground = world.mesh(mesh.quad(600, 600)).key('ground')"
      ".fill(matte)\n"
      "            low = world.mesh(mesh.box((-110, -90, 30), (-50, -30, 40)))"
      ".key('low').fill(matte)\n"
      "            high = world.mesh(mesh.box((-110, 30, 120), (-50, 90, 130)))"
      ".key('high').fill(matte)\n"
      "            sun = world.directionalLight((1, 0, -1), castShadow=True,\n"
      "                shadow=world.LightShadow(size=size)).key('sun')\n"
      "            return world.Frame(world.group(sun, ground, low, high)"
      ".key('set'))\n"
      "        self.stills = [ctx.bakeScene3D(scene(size), (256, 256),\n"
      "                                       camera=lens,\n"
      "                                       background='#000000')\n"
      "                       for size in (0.0, 8.0)]\n"
      "\n"
      "    def draw(self, pen, ctx):\n"
      "        for index, still in enumerate(self.stills):\n"
      "            if still is not None:\n"
      "                pen.image(still, 256 * index, 0)\n");
  const SkBitmap pixels = bakedStill(source, "shadowed", 512, 256);
  ASSERT_FALSE(pixels.isNull());
  // Where a world x on the ground lands across one 256-pixel bake: the
  // lens sees 2·400·tan(20°) units across it.
  const double unitsAcross = 2 * 400 * std::tan(20.0 * M_PI / 180.0);
  const auto column = [&](int bake, double x) {
    return bake * 256 + int(std::lround(128 + x * 256 / unitsAcross));
  };
  const auto row = [&](double y) {
    return int(std::lround(128 - y * 256 / unitsAcross));
  };
  const auto lightness = [&](int x, int y) {
    return int(SkColorGetG(pixels.getColor(x, y)));
  };
  // How many pixels a shadow's right edge takes to rise from a fifth of
  // the lit ground to four fifths of it, walking right from inside it.
  const auto edgeWidth = [&](int bake, double shadowX, double y) {
    const int lit = lightness(column(bake, 135), row(y));
    int dark = -1;
    int bright = -1;
    for (int x = column(bake, shadowX); x < column(bake, 135); ++x) {
      const int value = lightness(x, row(y));
      if (dark < 0 && value > lit / 5) dark = x;
      if (value > lit * 4 / 5) {
        bright = x;
        break;
      }
    }
    return dark < 0 || bright < 0 ? -1 : bright - dark;
  };
  ASSERT_GT(lightness(column(1, 135), row(60)), 60) << "the ground is lit";
  ASSERT_LT(lightness(column(1, 45), row(60)), 30)
      << "the high slab shades the ground at its right";
  const int pointLow = edgeWidth(0, -30, -60);
  const int pointHigh = edgeWidth(0, 45, 60);
  const int sizedLow = edgeWidth(1, -30, -60);
  const int sizedHigh = edgeWidth(1, 45, 60);
  ASSERT_GE(pointLow, 0);
  ASSERT_GE(pointHigh, 0);
  ASSERT_GE(sizedLow, 0);
  ASSERT_GE(sizedHigh, 0);
  EXPECT_GT(sizedHigh, sizedLow + 5)
      << "a sized sun's penumbra widens with the caster's height";
  EXPECT_GT(sizedHigh, pointHigh + 5)
      << "a point sun casts the same shadow sharp";
  EXPECT_LE(pointHigh, pointLow + 3)
      << "a point sun's edge is as sharp high as low";
}

/** A FRESH PAGE WEARING @p content, described before it first paints and
 *  baked once, beside the same page baked from a frame described after
 *  that paint: what the two halves of the picture show. */
std::pair<SkBitmap, SkBitmap> firstAndSecondDescriptions(
    std::string_view name, const std::string& content) {
  PythonSource source(name);
  source.write(
      "entry.py",
      "from sigil import compose, material, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "@sketch(size=(192, 96), background='#000000', capture_at=0)\n"
      "class Described:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        page = ctx.page((64, 64))\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 200)\n"
      "        lens.target = (0, 0, 0)\n"
      "        tree = " +
          content +
          "\n"
          "        def frame():\n"
          "            eye = world.camera(lens, "
          "toneCurve=world.ToneCurve.Linear)"
          ".key('eye')\n"
          "            lamp = "
          "world.pointLight(intensity=world.intensityAt(0.5, "
          "150.0)).key('lamp').at((20, 30, 150))\n"
          "            card = world.mesh(mesh.quad(120, 90)).key('card')"
          ".fill(page.surface())\n"
          "            return (world.Frame(world.group(eye, lamp, card)"
          ".key('set'))\n"
          "                    .pass_(page.render(tree)))\n"
          "        self.stills = [ctx.bakeScene3D(frame(), (96, 96), "
          "camera=lens, background='#000000') for _ in range(2)]\n"
          "\n"
          "    def draw(self, pen, ctx):\n"
          "        for index, still in enumerate(self.stills):\n"
          "            if still is not None:\n"
          "                pen.image(still, 96 * index, 0)\n");
  const SkBitmap both = bakedStill(source, name, 192, 96);
  SkBitmap first, second;
  if (both.isNull()) return {};
  both.extractSubset(&first, SkIRect::MakeXYWH(0, 0, 96, 96));
  both.extractSubset(&second, SkIRect::MakeXYWH(96, 0, 96, 96));
  return {first, second};
}

/** How far apart two pictures of one size are: the largest difference of
 *  any channel of any pixel. */
int largestDifference(const SkBitmap& first, const SkBitmap& second) {
  int largest = 0;
  for (int y = 0; y < first.height(); ++y)
    for (int x = 0; x < first.width(); ++x) {
      const SkColor a = first.getColor(x, y);
      const SkColor b = second.getColor(x, y);
      for (int shift : {0, 8, 16, 24})
        largest = std::max(largest, std::abs(int((a >> shift) & 0xff) -
                                             int((b >> shift) & 0xff)));
    }
  return largest;
}

TEST_F(SketchPythonBakes,
       APageDescribedBeforeItsFirstPaintIsWornWithTheLobesItPaints) {
  const std::vector<std::pair<std::string, std::string>> contents = {
      {"brushed",
       "compose.box().width(64).height(64).fill(material.from_('#e6e6e6')"
       ".surface(metallic=1.0, roughness=0.35).brushed(strength=0.9))"},
      {"anisotropic",
       "compose.box().width(64).height(64).fill(material.from_('#e6e6e6')"
       ".surface(metallic=1.0, roughness=0.35, anisotropy=0.9))"},
  };
  for (const auto& [name, content] : contents) {
    SCOPED_TRACE(name);
    const auto [first, second] = firstAndSecondDescriptions(
        "sigil_python_first_description_" + name, content);
    ASSERT_FALSE(first.isNull());
    EXPECT_LE(largestDifference(first, second), 1)
        << "the first bake wears the page as a later description does";
  }
}

TEST_F(SketchPythonBakes, AKeyframedTurnBakedAtAMomentStandsBetweenItsSteps) {
  PythonSource source("sigil_python_keyframed_bake");
  // A green card turned about y by a path of three keyframes and no
  // `from_`: it starts at -80 degrees, reaches 0 a second later and 80 a
  // second after that. Baked at four moments side by side, the card's
  // width on the middle row is how far it has turned: thin at the start
  // and at rest, whole at 0, and in between half way through the first
  // step.
  source.write(
      "entry.py",
      "from sigil import motion, world\n"
      "from sigil.geometry import mesh\n"
      "from sigil.material import surface as surfaces\n"
      "from sigil.motion import ease\n"
      "from sigil.sketch import sketch\n"
      "\n"
      "MOMENTS = (0.0, 0.5, 1.0, 5.0)\n"
      "\n"
      "@sketch(size=(384, 96), background='#000000', capture_at=0)\n"
      "class Turned:\n"
      "    needsDevice = True\n"
      "\n"
      "    def setup(self, ctx):\n"
      "        lens = mesh.camera.Camera()\n"
      "        lens.eye = (0, 0, 300)\n"
      "        lens.target = (0, 0, 0)\n"
      "        turn = motion.animate(keyframes=[\n"
      "            motion.Keyframe(-80, 0.0),\n"
      "            motion.Keyframe(0, 1.0, ease.linear),\n"
      "            motion.Keyframe(80, 1.0, ease.linear)])\n"
      "        card = (world.mesh(mesh.quad(160, 100)).key('card')\n"
      "                .rotateY(turn)\n"
      "                .fill(surfaces.unlit(\n"
      "                    surfaces.SurfaceParameters(baseColor='#00ff00'))))\n"
      "        frame = world.Frame(world.group(card).key('set'))\n"
      "        self.stills = [ctx.bakeScene3D(frame, (96, 96), camera=lens,\n"
      "                                       background='#000000', at=at)\n"
      "                       for at in MOMENTS]\n"
      "\n"
      "    def draw(self, pen, ctx):\n"
      "        for index, still in enumerate(self.stills):\n"
      "            if still is not None:\n"
      "                pen.image(still, 96 * index, 0)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "turned");
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(384, 96));
  session->frame(*surface->getCanvas(), 1.0 / 60.0);
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(384, 96));
  ASSERT_TRUE(surface->readPixels(pixels, 0, 0));
  const auto widthAt = [&pixels](int panel) {
    int width = 0;
    for (int x = panel * 96; x < (panel + 1) * 96; ++x) {
      const SkColor colour = pixels.getColor(x, 48);
      if (SkColorGetG(colour) > 160 && SkColorGetR(colour) < 80) ++width;
    }
    return width;
  };
  const int start = widthAt(0);
  const int turned = widthAt(1);
  const int whole = widthAt(2);
  const int rest = widthAt(3);
  ASSERT_GT(whole, 30) << "the card square on fills its share of the row";
  EXPECT_LT(start, whole * 35 / 100) << "the path starts at its first step";
  EXPECT_LT(rest, whole * 35 / 100) << "the path rests at its last step";
  EXPECT_GT(turned, whole * 60 / 100)
      << "half way through the first step the card has turned toward 0";
  EXPECT_LT(turned, whole * 92 / 100)
      << "half way through the first step the card is still turned";
}

}  // namespace
