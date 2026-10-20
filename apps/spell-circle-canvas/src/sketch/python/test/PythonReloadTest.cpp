/** @file
 * Reloading a Python sketch through the live host: an edit reloads it, a
 * failed edit keeps the last session until the next save, and the helper
 * modules beside it are read fresh.
 */

#include <gtest/gtest.h>
#include <sigilsketch/core/Registry.h>

#include <string>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::fonts;
using sketch::test::kGood;
using sketch::test::PythonSource;

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

}  // namespace
