/** @file
 * The control surface beside a material: the rows a description asks
 * for, the pointer moving them, and the material following.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/substance/Substance.h>
#include <sigilsketch/kit/Kit.h>

#include <memory>
#include <utility>

#include "Drawn.h"

namespace {

namespace kit = sigil::sketch::kit;
namespace compose = sigil::compose;
namespace sbsar = sigil::material::sbsar;
using sigil::sketch::kit::test::kTall;
using sigil::sketch::kit::test::kWide;

/** A stand-in for what `sbsar::describe` reads out of an archive: a
 *  slider with a range, a whole-numbered input, a toggle, a choice, and a
 *  colour, which has no row. */
sbsar::Description leaves() {
  sbsar::Description described;
  described.graph = "Leaves";
  described.inputs = {
      {.name = "Season", .label = "Season", .group = "Look",
       .type = sbsar::InputType::Float, .widget = sbsar::Widget::Slider,
       .defaultValue = {0.5f}, .minimum = {0}, .maximum = {2}, .step = 0},
      {.name = "Density", .label = "Density", .group = "Look",
       .type = sbsar::InputType::Integer, .widget = sbsar::Widget::Slider,
       .defaultValue = {4}, .minimum = {1}, .maximum = {9}},
      {.name = "Shadow", .label = "Shadow", .group = "Light",
       .type = sbsar::InputType::Integer, .widget = sbsar::Widget::Toggle,
       .defaultValue = {0}},
      {.name = "LeafType", .label = "Leaf", .group = "Look",
       .type = sbsar::InputType::Integer, .widget = sbsar::Widget::Combobox,
       .defaultValue = {1},
       .choices = {{0, "Oak"}, {1, "Maple"}, {2, "Birch"}}},
      {.name = "Tint", .label = "Tint", .group = "Look",
       .type = sbsar::InputType::Float3, .widget = sbsar::Widget::Color,
       .defaultValue = {1, 1, 1}},
  };
  return described;
}

/** A composer over a raster surface that the pointer can be moved over. */
struct Surface {
  sigil::motion::Engine engine;
  compose::Composer composer{engine, sigil::sketch::test::fonts()};
  sk_sp<SkSurface> pixels =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWide, kTall));
  explicit Surface(compose::Element tree) {
    composer.setSize({(float)kWide, (float)kTall});
    composer.render(std::move(tree));
    draw();
  }
  void draw() { composer.draw(*pixels->getCanvas()); }
  void point(float x, float y, bool pressed) {
    composer.setPointer({x, y}, pressed);
    draw();
  }
};

TEST(SketchKitControls, ADescriptionAsksForOneRowPerNumber) {
  const std::vector<kit::Control> rows = kit::controlsOf(leaves());
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(rows[0].widget, kit::Widget::Slider);
  EXPECT_EQ(rows[0].maximum, 2);
  EXPECT_EQ(rows[1].step, 1);
  EXPECT_EQ(rows[2].widget, kit::Widget::Toggle);
  EXPECT_EQ(rows[2].group, "Light");
  EXPECT_EQ(rows[3].widget, kit::Widget::Choice);
  ASSERT_EQ(rows[3].options.size(), 3u);
  EXPECT_EQ(rows[3].options[2].value, 2);
}

TEST(SketchKitControls, PressingARailDragsItsValue) {
  kit::Controls surface({{.name = "Season", .maximum = 2, .value = 0.5f}});
  Surface drawn(kit::controls(surface));
  const kit::ControlsView how;
  const float railLeft = how.labelWidth;
  const float railRight =
      how.width - how.figureWidth - kit::houseTheme().spacing.labelGap;
  const float middle = how.rowHeight * 0.5f;
  drawn.point(railLeft + (railRight - railLeft) * 0.75f, middle, true);
  EXPECT_NEAR(surface.value("Season").value(), 1.5f, 0.01f);
  // The drag holds past the rail's end while the button is down…
  drawn.point(railRight + 40, middle + 30, true);
  EXPECT_NEAR(surface.value("Season").value(), 2.0f, 0.001f);
  // …and a pointer moving with the button up moves nothing.
  drawn.point(railLeft, middle, false);
  EXPECT_NEAR(surface.value("Season").value(), 2.0f, 0.001f);
}

TEST(SketchKitControls, APressFlipsAToggleOnce) {
  kit::Controls surface(
      {{.name = "Shadow", .widget = kit::Widget::Toggle, .step = 1}});
  Surface drawn(kit::controls(surface));
  const kit::ControlsView how;
  drawn.point(how.labelWidth + 4, how.rowHeight * 0.5f, true);
  drawn.point(how.labelWidth + 4, how.rowHeight * 0.5f, true);
  EXPECT_EQ(surface.value("Shadow").value(), 1.0f);
  drawn.point(how.labelWidth + 4, how.rowHeight * 0.5f, false);
  drawn.point(how.labelWidth + 4, how.rowHeight * 0.5f, true);
  EXPECT_EQ(surface.value("Shadow").value(), 0.0f);
}

TEST(SketchKitControls, AnAuthoredRangeStepsTheValue) {
  kit::Controls surface({{.name = "Season"}});
  surface.range("Season", 0, 10, 2.5f);
  surface.set("Season", 6);
  EXPECT_EQ(surface.value("Season").value(), 5.0f);
  surface.set("Season", 40);
  EXPECT_EQ(surface.value("Season").value(), 10.0f);
}

TEST(SketchKitControls, OnlyTheNamedGroupsAreDrawn) {
  kit::Controls surface(leaves());
  const auto drawnBytes = [](compose::Element tree) {
    Surface drawn(std::move(tree));
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(kWide, kTall));
    EXPECT_TRUE(drawn.pixels->readPixels(bitmap.pixmap(), 0, 0));
    return bitmap;
  };
  SkBitmap every = drawnBytes(kit::controls(surface));
  SkBitmap light = drawnBytes(kit::controls(surface, {.groups = {"Light"}}));
  // Every group reaches further down the surface than the one group does.
  const auto lowestInk = [](const SkBitmap& bitmap) {
    int lowest = -1;
    for (int y = 0; y < kTall; ++y)
      for (int x = 0; x < kWide; ++x)
        if (bitmap.getColor4f(x, y).fR > 0.05f)
          lowest = y;
    return lowest;
  };
  EXPECT_GT(lowestInk(every), lowestInk(light));
}

/** A program the material rows are read from: two float fields. */
struct GrainParameters {
  float amount = 0.3f;
  float scale = 4.0f;
};

TEST(SketchKitControls, AMaterialFollowsTheSurfaceItIsBoundTo) {
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GrainParameters>("sketch.kit.test.grain")
          .body(Target::SkSL,
                "half4 main(float2 p) { return half4(amount, scale, 0, 1); }\n"));
  sigil::material::Material grain(recipe, GrainParameters{});
  kit::Controls surface(grain);
  ASSERT_EQ(surface.parameters().size(), 2u);
  EXPECT_NEAR(surface.value("amount").value(), 0.3f, 1e-6f);
  surface.bind(grain);
  EXPECT_TRUE(grain.isBound("amount"));
  EXPECT_TRUE(grain.isBound("scale"));
  // A copy of the surface is the same surface.
  kit::Controls copy = surface;
  copy.set("amount", 0.8f);
  EXPECT_NEAR(surface.value("amount").value(), 0.8f, 1e-6f);
}

}  // namespace
