/** @file
 * Python sketch sessions and reload failures through the ordinary live host.
 */

#include <gtest/gtest.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilio/hub/Feed.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ScratchDir.h"
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
    result.deterministic = true;
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
  std::map<std::string, std::weak_ptr<sigil::io::Feed>> feeds;
};

struct FeedFixture {
  std::shared_ptr<FeedCounts> counts = std::make_shared<FeedCounts>();
  sketch::Assets services{fs::path{}};

  FeedFixture() {
    services.hub().setFeedTransport(
        "fixture", [counts = counts](std::string_view uri,
                                     std::weak_ptr<sigil::io::Feed> feed) {
          const std::string key(uri);
          ++counts->opened[key];
          counts->feeds[key] = std::move(feed);
          sigil::io::OpenedFeed result;
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
        builtins._sigil_escaped_feed = hub.feed('fixture://input')
        builtins._sigil_escaped_hub = hub
        hub.feed('fixture://unreferenced')
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
                           lambda: builtins._sigil_escaped_hub.feed('fixture://input')):
                try:
                    action()
                except RuntimeError as error:
                    assert 'closed session' in str(error)
                else:
                    raise AssertionError('An expired session service remained usable')
        finally:
            del builtins._sigil_escaped_feed, builtins._sigil_escaped_hub
        self.feed = ctx.assets.hub().feed('fixture://input')
        assert self.feed.send(b'reopened')
)PY");
  kind = sketch::python::load(source.entry());
  session = kind->open(fonts(), fixture.services, true, "second");
  EXPECT_EQ(fixture.counts->opened["fixture://input"], 2);
  session.reset();
  EXPECT_EQ(fixture.counts->closed["fixture://input"], 2);
}

TEST(SketchPython, OverlappingAndFailedGenerationsShareTheWorkingFeed) {
  PythonSource source("sigil_python_feed_overlap");
  FeedFixture fixture;
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        self.feed = ctx.assets.hub().feed('fixture://shared')
    def update(self, elapsed, ctx):
        assert self.feed.send(b'working')
)PY");
  const auto good = sketch::python::load(source.entry());
  auto first = good->open(fonts(), fixture.services, true, "first");
  source.write("entry.py", R"PY(
class Study:
    def setup(self, ctx):
        ctx.assets.hub().feed('fixture://shared')
        ctx.assets.hub().feed('fixture://candidate')
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
        self.feed = ctx.assets.hub().feed('fixture://retained')
        if first:
            ctx.assets.hub().feed('fixture://removed')
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
        self.feed = ctx.assets.hub().feed('fixture://input')
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

TEST(SketchPython, ASourceKindCanBeListedWhenItsFileIsMissingOrInvalid) {
  PythonSource source("sigil_python_source_kind_listing");
  const sketch::Kind kind = sketch::python::source(source.entry());
  ASSERT_TRUE(kind);
  EXPECT_EQ(kind->runtime(), "canvas");
  EXPECT_FALSE(kind->needsDevice());
  EXPECT_THROW((void)kind->open(fonts(), assets(), true, "missing"),
               std::runtime_error);

  source.write("entry.py", "class Study(:\n");
  const sketch::Kind invalid = sketch::python::source(source.entry());
  EXPECT_EQ(invalid->runtime(), "canvas");
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

}  // namespace
