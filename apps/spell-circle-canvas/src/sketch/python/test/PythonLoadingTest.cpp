/** @file
 * Loading a Python sketch: what a source kind answers before anything
 * is imported, when it imports, whether it is available, and that each
 * session it opens holds state of its own.
 */

#include <gtest/gtest.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/scene3d/Scene3D.h>

#include <stdexcept>
#include <string>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::fonts;
using sketch::test::kGood;
using sketch::test::PythonSource;

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

}  // namespace
