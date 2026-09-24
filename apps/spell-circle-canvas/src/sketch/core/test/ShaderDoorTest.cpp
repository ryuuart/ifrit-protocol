/** @file
 * The shader door: an `.sksl` file compiled into the runtime effect a
 * paint takes, recompiled when the file changes, the last program that
 * compiled kept while an edit does not, and a checker before any has.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilsketch/core/Assets.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

#include "ScratchDir.h"

namespace {

using sigil::sketch::Assets;

/** A program painting one colour, written as SkSL. */
std::string solid(std::string_view rgb) {
  return "half4 main(float2 xy) { return half4(" + std::string(rgb) +
         ", 1.0); }\n";
}

/** The colour @p program paints at the canvas unit (4, 4). */
SkColor paintedBy(const sk_sp<SkRuntimeEffect>& program) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(8, 8));
  SkPaint paint;
  paint.setShader(program->makeShader(nullptr, {}));
  surface->getCanvas()->drawPaint(paint);
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::MakeN32Premul(8, 8));
  (void)surface->readPixels(bitmap.pixmap(), 0, 0);
  return bitmap.getColor(4, 4);
}

/** A file under @p directory written anew, and stamped @p later seconds
 *  ahead so the hub sees it changed however fast the case runs. */
void rewrite(const sigil::test::ScratchDir& directory, const std::string& name,
             std::string_view body, int later) {
  directory.write(name, body);
  std::filesystem::last_write_time(
      directory.path / name, std::filesystem::file_time_type::clock::now() +
                                 std::chrono::seconds(later));
}

TEST(ShaderDoor, AFileIsCompiledOnceAndRecompiledWhenItChanges) {
  sigil::test::ScratchDir directory("sketch_shader_door");
  directory.write("fill.sksl", solid("1.0, 0.0, 0.0"));
  Assets assets(directory.path);

  const sk_sp<SkRuntimeEffect> red = assets.shader("fill.sksl");
  ASSERT_NE(red, nullptr);
  EXPECT_EQ(paintedBy(red), SK_ColorRED);
  EXPECT_EQ(assets.shader("fill.sksl").get(), red.get())
      << "one file compiled into two programs";
  EXPECT_TRUE(assets.problems().empty()) << assets.problems();

  rewrite(directory, "fill.sksl", solid("0.0, 1.0, 0.0"), 2);
  EXPECT_TRUE(assets.poll());
  const sk_sp<SkRuntimeEffect> green = assets.shader("fill.sksl");
  EXPECT_NE(green.get(), red.get());
  EXPECT_EQ(paintedBy(green), SK_ColorGREEN);
}

TEST(ShaderDoor, AnEditThatDoesNotCompileKeepsTheLastProgramAndSaysWhy) {
  sigil::test::ScratchDir directory("sketch_shader_door_broken");
  directory.write("fill.sksl", solid("0.0, 0.0, 1.0"));
  Assets assets(directory.path);
  const sk_sp<SkRuntimeEffect> blue = assets.shader("fill.sksl");

  rewrite(directory, "fill.sksl",
          "half4 main(float2 xy) { return notDeclaredAnywhere; }\n", 2);
  EXPECT_TRUE(assets.poll());
  EXPECT_EQ(assets.shader("fill.sksl").get(), blue.get());
  const std::string said = assets.problems();
  EXPECT_NE(said.find("fill.sksl"), std::string::npos) << said;
  EXPECT_NE(said.find("notDeclaredAnywhere"), std::string::npos) << said;

  rewrite(directory, "fill.sksl", solid("1.0, 1.0, 1.0"), 4);
  EXPECT_TRUE(assets.poll());
  EXPECT_EQ(paintedBy(assets.shader("fill.sksl")), SK_ColorWHITE);
  EXPECT_TRUE(assets.problems().empty()) << assets.problems();
}

TEST(ShaderDoor, AProgramThatNeverCompiledIsTheCheckerUntilOneDoes) {
  sigil::test::ScratchDir directory("sketch_shader_door_checker");
  Assets assets(directory.path);

  const sk_sp<SkRuntimeEffect> missing = assets.shader("later.sksl");
  ASSERT_NE(missing, nullptr);
  EXPECT_EQ(paintedBy(missing), SK_ColorBLACK);  // the checker's first cell
  EXPECT_NE(assets.problems().find("later.sksl"), std::string::npos);

  directory.write("broken.sksl", "half4 main(float2 xy) {\n");
  EXPECT_EQ(assets.shader("broken.sksl").get(), missing.get());

  directory.write("later.sksl", solid("0.0, 1.0, 0.0"));
  EXPECT_TRUE(assets.poll());
  EXPECT_EQ(paintedBy(assets.shader("later.sksl")), SK_ColorGREEN);
  const std::string said = assets.problems();
  EXPECT_EQ(said.find("later.sksl"), std::string::npos) << said;
  EXPECT_NE(said.find("broken.sksl"), std::string::npos) << said;
}

}  // namespace
