/** @file
 * The draw's runtime seam: the built-in value is one value however it is
 * reached, a style carries it by default, and a substituted executor
 * receives the draw instead of the canvas.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>

#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <utility>

#include "sigilgeometry/mesh/camera/Camera.h"
#include "sigilgeometry/mesh/render/Painter.h"
#include "support/RuntimeSeam.h"

using namespace sigil::geometry::mesh;

namespace {

/** An executor that records rather than draws. Two of them compare by
 *  the label they carry, which is what makes a Runtime holding one a
 *  comparable value. */
struct Recorder : render::Executor {
  explicit Recorder(std::string name) : label(std::move(name)) {}

  std::string label;
  mutable int meshes = 0;
  mutable int panels = 0;

  bool operator==(const Recorder& o) const { return label == o.label; }

  void drawMesh(SkCanvas&, const Mesh&, const glm::mat4&, const camera::Camera&,
                SkSize, const render::MeshStyle&) const override {
    ++meshes;
  }
  void drawPanel(SkCanvas&, const glm::mat4&, const camera::Camera&, SkSize,
                 const std::function<void(SkCanvas&)>&) const override {
    ++panels;
  }
};

struct DrawSeam {
  using Seam = render::Runtime;
  static Seam builtIn() { return render::Runtime::cpu(); }
  static Seam holding(const char* label) {
    return render::Runtime{Recorder{label}};
  }
};

}  // namespace

INSTANTIATE_TYPED_TEST_SUITE_P(TheDrawsRuntimeSeam, RuntimeSeam, DrawSeam);

namespace {

TEST(Runtime, TheDefaultStyleCarriesTheBuiltInRuntime) {
  EXPECT_EQ(render::MeshStyle{}.runtime, render::Runtime::cpu());
}

// The style is the whole of the switch: the same call, the same
// geometry, a different executor.
TEST(Runtime, StyleRoutesTheDrawToItsExecutor) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  surface->getCanvas()->clear(SK_ColorBLACK);

  render::MeshStyle style;
  style.runtime = Recorder{"counting"};
  render::drawMesh(*surface->getCanvas(), quad(20, 20), glm::mat4(1.0f), {},
                   {64, 48}, style);

  const auto* recorder = static_cast<const Recorder*>(style.runtime.get());
  ASSERT_NE(recorder, nullptr);
  EXPECT_EQ(recorder->meshes, 1);

  // Nothing reached the canvas: the substituted executor drew nowhere.
  SkBitmap bm;
  bm.allocPixels(surface->imageInfo());
  ASSERT_TRUE(surface->readPixels(bm.pixmap(), 0, 0));
  EXPECT_EQ(bm.getColor(32, 24), SK_ColorBLACK);
}

TEST(Runtime, PanelRoutesToTheRuntimeItIsGiven) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 48));
  const render::Runtime runtime{Recorder{"panels"}};
  int drawn = 0;
  render::drawPanel(
      *surface->getCanvas(), glm::mat4(1.0f), {}, {64, 48},
      [&](SkCanvas&) { ++drawn; }, runtime);
  EXPECT_EQ(drawn, 0);
  EXPECT_EQ(static_cast<const Recorder*>(runtime.get())->panels, 1);
}

}  // namespace
