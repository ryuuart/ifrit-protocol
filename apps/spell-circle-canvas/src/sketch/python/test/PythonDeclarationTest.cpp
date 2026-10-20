/** @file
 * What a Python sketch declares and how: a tree returned from setup or
 * described at every step, a decorator's theme and capture moment, a
 * file of functions, and a capture step declared unpainted.
 */

#include <gtest/gtest.h>
#include <pybind11/pybind11.h>
#include <sigilpython/skia/Values.h>

#include <algorithm>
#include <string>
#include <vector>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::drawnFrame;
using sketch::test::fonts;
using sketch::test::PythonSource;

TEST(SketchPython, ADeclaredUnpaintedCaptureStepLeavesTheDrawToTheStill) {
  PythonSource source("sigil_python_unpainted_capture");
  source.write("entry.py",
               "import builtins\n"
               "from sigil.sketch import CanvasSpecification, sketch\n"
               "builtins._sigil_capture_counts = {'draw': 0, 'update': 0}\n"
               "@sketch(size=(64, 48), captureAt=0.5)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        ctx.canvas(CanvasSpecification(size=(64, 48),\n"
               "            captureAt=0.5, paintDiscardedFrames=False))\n"
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

TEST(SketchPython, ASetupThatReturnsATreeRendersIt) {
  PythonSource source("sigil_python_setup_returns");
  source.write("entry.py",
               "from sigil.compose import box\n"
               "from sigil.sketch import sketch\n"
               "@sketch(size=(64, 48), background='#000000', captureAt=0)\n"
               "class Study:\n"
               "    def setup(self):\n"
               "        return box().width(64).height(48).fill('#00ff00')\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_EQ(kind->runtime(), "scene2d");
  auto session = kind->open(fonts(), assets(), true, "returned");
  const SkBitmap pixels = drawnFrame(*session, 64, 48, 1.0 / 60.0);
  ASSERT_FALSE(pixels.isNull());
  EXPECT_EQ(pixels.getColor(32, 24), SK_ColorGREEN);
}

TEST(SketchPython, ADescribedTreeIsRenderedAgainAtEveryStep) {
  PythonSource source("sigil_python_describe_tree");
  // Red before half a second, green after: the frame drawn at each step
  // is the tree described at that step's moment.
  source.write("entry.py",
               "from sigil.compose import Element, box\n"
               "from sigil.sketch import sketch\n"
               "@sketch(size=(64, 48), background='#000000', captureAt=0)\n"
               "class Study:\n"
               "    def describe(self, seconds: float) -> Element:\n"
               "        colour = '#ff0000' if seconds < 0.5 else '#00ff00'\n"
               "        return box().width(64).height(48).fill(colour)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_EQ(kind->runtime(), "scene2d");
  EXPECT_EQ(sketch::python::source(source.entry())->runtime(), "scene2d");
  auto session = kind->open(fonts(), assets(), true, "described");
  const SkBitmap early = drawnFrame(*session, 64, 48, 0.1);
  ASSERT_FALSE(early.isNull());
  EXPECT_EQ(early.getColor(32, 24), SK_ColorRED);
  const SkBitmap late = drawnFrame(*session, 64, 48, 0.6);
  ASSERT_FALSE(late.isNull());
  EXPECT_EQ(late.getColor(32, 24), SK_ColorGREEN);
}

TEST(SketchPython,
     TheDecoratorsThemeStagesTheCanvasAndIsProvidedAroundEachStep) {
  PythonSource source("sigil_python_decorator_theme");
  // The theme's ground is the canvas's background where none is stated,
  // and the theme is the one in scope in setup, describe and update.
  source.write("entry.py",
               "import builtins\n"
               "from sigil.compose import Element, box\n"
               "from sigil.sketch import kit, sketch\n"
               "LOOK = kit.featureTheme(kit.Density.Spacious)\n"
               "palette = LOOK.palette\n"
               "palette.ground = '#336699'\n"
               "LOOK.palette = palette\n"
               "builtins._sigil_theme_seen = []\n"
               "def seen(where):\n"
               "    ground = tuple(kit.theme().palette.ground)\n"
               "    builtins._sigil_theme_seen.append((where, ground))\n"
               "@sketch(size=(96, 72), captureAt=0.25, theme=LOOK)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        seen('setup')\n"
               "    def update(self, elapsed):\n"
               "        seen('update')\n"
               "    def describe(self, seconds) -> Element:\n"
               "        seen('describe')\n"
               "        return box().width(10).height(10)\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "themed");
  const SkBitmap pixels = drawnFrame(*session, 96, 72, 1.0 / 60.0);
  ASSERT_FALSE(pixels.isNull());
  EXPECT_EQ(session->canvas().size, SkSize::Make(96, 72));
  EXPECT_DOUBLE_EQ(session->canvas().captureSeconds, 0.25);
  const sigil::material::Color ground = session->canvas().background;
  EXPECT_NEAR(ground.r, 0x33 / 255.0f, 1e-3f);
  EXPECT_NEAR(ground.g, 0x66 / 255.0f, 1e-3f);
  EXPECT_NEAR(ground.b, 0x99 / 255.0f, 1e-3f);
  const pybind11::gil_scoped_acquire lock;
  pybind11::module_ builtins = pybind11::module_::import("builtins");
  const pybind11::list seen = builtins.attr("_sigil_theme_seen");
  std::vector<std::string> places;
  for (pybind11::handle entry : seen) {
    const auto where = entry[pybind11::int_(0)].cast<std::string>();
    const pybind11::tuple colour = entry[pybind11::int_(1)];
    places.push_back(where);
    ASSERT_GE(colour.size(), 3u);
    EXPECT_NEAR(colour[0].cast<float>(), 0x33 / 255.0f, 1e-3f) << where;
    EXPECT_NEAR(colour[2].cast<float>(), 0x99 / 255.0f, 1e-3f) << where;
  }
  builtins.attr("__dict__").attr("pop")("_sigil_theme_seen");
  for (const char* where : {"setup", "update", "describe"})
    EXPECT_NE(std::find(places.begin(), places.end(), where), places.end())
        << where << " ran without the decorator's theme";
}

TEST(SketchPython, TheDecoratorStatesTheCaptureMomentAsCaptureAt) {
  PythonSource source("sigil_python_capture_moment");
  source.write("entry.py",
               "from sigil.sketch import sketch\n"
               "@sketch(size=(40, 30), captureAt=1.75)\n"
               "class Study:\n"
               "    def setup(self, ctx):\n"
               "        pass\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "moment");
  EXPECT_DOUBLE_EQ(session->canvas().captureSeconds, 1.75);
  EXPECT_EQ(session->canvas().size, SkSize::Make(40, 30));
}

/** A FILE OF FUNCTIONS IS A SKETCH: no class and no decorator, the canvas
 *  declared by calls at the top of the file, and the tree its describe
 *  function returns drawn at every step. */
TEST(SketchPython,
     AFileOfFunctionsDrawsTheTreeItDescribesOnTheCanvasItDeclares) {
  PythonSource source("sigil_python_functions_tree");
  source.write("entry.py",
               "from sigil.compose import Element, box\n"
               "from sigil.sketch.globals import *\n"
               "size(64, 48)\n"
               "background('#000000')\n"
               "captureAt(0)\n"
               "def describe(seconds: float) -> Element:\n"
               "    colour = '#ff0000' if seconds < 0.5 else '#00ff00'\n"
               "    return box().width(width).height(height).fill(colour)\n");
  const sketch::Kind listed = sketch::python::source(source.entry());
  EXPECT_EQ(listed->runtime(), "scene2d");
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_EQ(kind->runtime(), "scene2d");
  auto session = kind->open(fonts(), assets(), true, "functions");
  EXPECT_EQ(session->canvas().size.width(), 64);
  EXPECT_EQ(session->canvas().size.height(), 48);
  const SkBitmap early = drawnFrame(*session, 64, 48, 0.1);
  ASSERT_FALSE(early.isNull());
  EXPECT_EQ(early.getColor(32, 24), SK_ColorRED);
  const SkBitmap late = drawnFrame(*session, 64, 48, 0.6);
  ASSERT_FALSE(late.isNull());
  EXPECT_EQ(late.getColor(32, 24), SK_ColorGREEN);
}

}  // namespace
