/** @file
 * A scene3d sketch written in Python: known as one from its text, unavailable
 * without a device, drawn on the device when there is one, lent its
 * session's fonts, and answering through its context only during setup.
 */

#include <gtest/gtest.h>
#include <include/core/SkTypeface.h>
#include <pybind11/pybind11.h>
#include <sigilpython/skia/Values.h>
#include <sigilsketch/core/Registry.h>
#include <sigilsketch/plate/Thumbnails.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/advanced/Skia.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::assets;
using sketch::test::drawnFrame;
using sketch::test::familyOf;
using sketch::test::fonts;
using sketch::test::PythonSource;
using sketch::test::SketchPythonBakes;

/** A file of functions whose describe function is annotated to return a
 *  world frame is a scene3d sketch, read so from its text before any
 *  import and so again from the class its functions are wrapped into. */
TEST(SketchPython,
     AFileOfFunctionsDescribingAFrameIsKnownAsScene3DFromItsText) {
  PythonSource source("sigil_python_functions_scene3d");
  source.write("entry.py",
               "from sigil import world\n"
               "from sigil.sketch.globals import *\n"
               "size(128, 96)\n"
               "def describe(seconds: float) -> world.Frame:\n"
               "    return world.Frame(world.group())\n");
  sketch::useRuntime({});
  const sketch::Kind listed = sketch::python::source(source.entry());
  EXPECT_EQ(listed->runtime(), "scene3d");
  EXPECT_TRUE(listed->needsDevice());
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_EQ(kind->runtime(), "scene3d");
  EXPECT_TRUE(kind->needsDevice());
}

/** A scene3d sketch in Python: one green page worn by a card, lit by
 *  nothing it does not state, described as a function of the scene time. */
constexpr std::string_view kScene3DSketch = R"PY(
from sigil import compose, world
from sigil.geometry import mesh
from sigil.sketch import Scene3DContext, sketch


@sketch(size=(128, 96), background="#1a1a1a", captureAt=0)
class Paged:
    def setup(self, ctx: Scene3DContext) -> None:
        self.page = ctx.page((32, 32))
        self.tree = compose.box().width(32).height(32).fill("#00ff00")
        ctx.camera(mesh.camera.Camera(eye=(0, 0, 300), target=(0, 0, 0)))

    def describe(
        self, seconds: float
    ) -> world.Frame:
        card = world.mesh(mesh.quad(100, 60)).key("card").fill(self.page.surface())
        sun = world.directionalLight((0, 0, -1)).key("sun")
        return world.Frame(world.group(sun, card).key("set")).pass_(
            self.page.render(self.tree)
        )
)PY";

TEST(SketchPython,
     AScene3DSketchIsKnownFromItsTextAndUnavailableWithoutADevice) {
  PythonSource source("sigil_python_scene3d_listed");
  source.write("entry.py", kScene3DSketch);
  sketch::useRuntime({});
  std::string expected;
  EXPECT_FALSE(sketch::runtimeAvailable(&expected));
  // The listing reads the kind from the file's text, before any import.
  const sketch::Kind listed = sketch::python::source(source.entry());
  EXPECT_EQ(listed->runtime(), "scene3d");
  EXPECT_TRUE(listed->needsDevice());
  std::string why;
  EXPECT_FALSE(listed->available(&why));
  EXPECT_EQ(why, expected);
  // The loaded class says the same, by the same rule.
  const sketch::Kind kind = sketch::python::load(source.entry());
  EXPECT_EQ(kind->runtime(), "scene3d");
  EXPECT_TRUE(kind->needsDevice());
  EXPECT_FALSE(kind->available(&why));
  EXPECT_THROW((void)kind->open(fonts(), assets(), true, "paged"),
               std::runtime_error);
}

/** A device runtime that draws nothing, so a case can install one for
 *  the process without a device. */
struct InstalledRuntime : sigil::world::Executor {
  void execute(const sigil::world::PassWork&, const sigil::world::View&,
               sigil::world::Targets&) const override {}
  bool operator==(const InstalledRuntime&) const { return true; }
};

/** The kind the registry entry below makes, set by the case that reads
 *  it: an entry names its kind through a plain function. */
sketch::Kind g_entryKind;
sketch::Kind entryKind() { return g_entryKind; }

/** A THUMBNAIL STATES NO DEVICE, AND A PYTHON KIND HEARS IT. A still is
 *  drawn on a worker beside a window that is presenting, so the empty
 *  runtime it states reaches every kind — the kind a listing reads from
 *  the file's text and the kind the import makes alike — and a scene3d
 *  sketch is then unavailable with the one message every device door
 *  speaks, though the process installed a device runtime. */
TEST(SketchPython,
     AThumbnailOfAScene3DSketchStatesNoDeviceWhateverTheProcessHolds) {
  PythonSource source("sigil_python_scene3d_thumbnail");
  source.write("entry.py", kScene3DSketch);
  sketch::useRuntime(sigil::world::Runtime(InstalledRuntime{}));
  std::string expected;
  EXPECT_FALSE(sketch::runtimeAvailable(sigil::world::Runtime{}, &expected));

  for (const sketch::Kind& kind : {sketch::python::source(source.entry()),
                                   sketch::python::load(source.entry())}) {
    g_entryKind = kind;
    const sketch::Entry entry{.key = "python_scene3d_thumbnail",
                              .name = "python scene3d thumbnail",
                              .kind = &entryKind};
    EXPECT_TRUE(entry.available()) << "the process installed a runtime";
    std::string why;
    EXPECT_FALSE(sketch::thumbnailAvailable(entry, &why));
    EXPECT_EQ(why, expected);
    EXPECT_FALSE(sketch::onRuntime(kind, {})->available(nullptr));
  }
  g_entryKind = {};
  sketch::useRuntime({});
}

TEST_F(SketchPythonBakes, AScene3DSketchDescribesAFrameTheDeviceDraws) {
  PythonSource source("sigil_python_scene3d_frame");
  source.write("entry.py", kScene3DSketch);
  const sketch::Kind kind = sketch::python::load(source.entry());
  ASSERT_EQ(kind->runtime(), "scene3d");
  ASSERT_TRUE(kind->available(nullptr));
  auto session = kind->open(fonts(), assets(), true, "paged");
  EXPECT_EQ(session->canvas().size, SkSize::Make(128, 96));
  EXPECT_TRUE(session->hasViewpoint());
  const SkBitmap pixels = drawnFrame(*session, 128, 96, 1.0 / 60.0);
  ASSERT_FALSE(pixels.isNull());
  // The page's green is the card's base colour, lit by the sun.
  const SkColor middle = pixels.getColor(64, 48);
  EXPECT_GT(SkColorGetG(middle), 100u);
  EXPECT_GT(SkColorGetG(middle), SkColorGetR(middle) + 60);
  EXPECT_GT(SkColorGetG(middle), SkColorGetB(middle) + 60);
  // Off the card is the background the frame was cleared to.
  EXPECT_LT(SkColorGetG(pixels.getColor(1, 1)), 50u);
}

/** A SCENE3D SKETCH IS LENT ITS SESSION'S FONTS as a 2D sketch is: a face
 *  the session's font context adopted is a bundled family to
 *  `weave.fonts` handed `ctx.fonts`. */
TEST_F(SketchPythonBakes, AScene3DContextListsItsSessionsBundledFamilies) {
  const fs::path instruments = SIGIL_TEST_INSTRUMENT_DIR;
  const sk_sp<SkTypeface> sans =
      sigil::weave::ports::systemFontManager()->makeFromFile(
          (instruments / "Sans.ttf").c_str());
  ASSERT_NE(sans, nullptr);
  sigil::weave::FontContext own(sigil::weave::ports::systemFontManager());
  own.adopt(sigil::weave::fromSk(sans), "res://Sans.ttf");
  PythonSource source("sigil_python_scene3d_fonts");
  source.write(
      "entry.py",
      "import builtins\n"
      "from sigil import weave, world\n"
      "from sigil.sketch import sketch\n"
      "@sketch(size=(32, 32), captureAt=0)\n"
      "class Lettered:\n"
      "    def setup(self, ctx):\n"
      "        name = weave.fonts.families(ctx.fonts)[0]\n"
      "        family = weave.fonts.family(name, ctx.fonts)\n"
      "        builtins._sigil_scene3d_family = (name, family.bundled)\n"
      "    def describe(self, seconds) -> world.Frame:\n"
      "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
      "        return world.Frame(world.group(sun).key('set'))\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(own, assets(), true, "lettered");
  const pybind11::gil_scoped_acquire lock;
  const pybind11::module_ builtins = pybind11::module_::import("builtins");
  const auto [name, bundled] = builtins.attr("_sigil_scene3d_family")
                                   .cast<std::pair<std::string, bool>>();
  builtins.attr("__dict__").attr("pop")("_sigil_scene3d_family");
  EXPECT_EQ(name, familyOf(sans.get()));
  EXPECT_TRUE(bundled);
}

TEST_F(SketchPythonBakes, AScene3DContextAnswersOnlyWhileSetupRuns) {
  PythonSource source("sigil_python_scene3d_loan");
  source.write("entry.py",
               "from sigil import world\n"
               "from sigil.sketch import sketch\n"
               "@sketch(size=(32, 32), captureAt=0)\n"
               "class Kept:\n"
               "    def setup(self, ctx):\n"
               "        self.ctx = ctx\n"
               "    def describe(self, seconds) -> world.Frame:\n"
               "        self.ctx.captureAt(1)\n"
               "        sun = world.directionalLight((0, 0, -1)).key('sun')\n"
               "        return world.Frame(world.group(sun).key('set'))\n");
  const sketch::Kind kind = sketch::python::load(source.entry());
  auto session = kind->open(fonts(), assets(), true, "kept");
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(32, 32));
  try {
    session->frame(*surface->getCanvas(), 1.0 / 60.0);
    ADD_FAILURE() << "a context kept past setup answered";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("handed at setup"),
              std::string::npos)
        << error.what();
  }
}

}  // namespace
