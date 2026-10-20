/** @file
 * A face bundled beside a Python sketch is one resource to Python and to
 * the native host alike, and a session opened after it is replaced on
 * disk reads the new face.
 */

#include <gtest/gtest.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontTypes.h>
#include <include/core/SkTypeface.h>
#include <pybind11/pybind11.h>
#include <sigilio/advanced/Places.h>
#include <sigilpython/skia/Values.h>
#include <sigilweave/advanced/Skia.h>
#include <sigilweave/ports/Resource.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <chrono>
#include <cstring>
#include <utility>

#include "PythonTestSupport.h"

namespace {

namespace sketch = sigil::sketch;
namespace fs = std::filesystem;
using sketch::test::familyOf;
using sketch::test::fonts;
using sketch::test::PythonSource;

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

}  // namespace
