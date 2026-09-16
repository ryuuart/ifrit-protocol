/** @file
 * Python sketch sessions and reload failures through the ordinary live host.
 */

#include <gtest/gtest.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilsketch/live/Host.h>
#include <sigilsketch/python/Python.h>

#include <chrono>
#include <filesystem>
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
               "        self.node = graphics(self.draw, key='loop', width=80, "
               "height=60)\n"
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
               "GLOBAL = graphics(draw, key='global')\n"
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
