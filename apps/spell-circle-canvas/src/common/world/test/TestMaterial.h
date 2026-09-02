#pragma once

/** @file
 * THE SURFACE THESE TESTS PAINT WITH, and the ink count a plate is read
 * by.
 *
 * A test that needs a surface needs one it can compare and one it can
 * tell from another, and nothing more: no shading model, no catalog, no
 * second copy of what the material kit already states. So there is one
 * recipe here carrying one colour, in two builds, because a tier with a
 * compiler and a tier without ask different things of a material.
 * `paint()` is the colour alone, which is all a tier that reads a base
 * colour can use; `slangPaint()` carries a body, which is what a tier
 * that compiles one runs.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec4.hpp>
#include <memory>

namespace sigil::world::test {

/** One colour, which is the whole of what a test surface has to say. */
struct Paint {
  glm::vec4 baseColor{1, 1, 1, 1};
};

/** The plain recipe: a colour a frame extracts, and no program at all. */
inline const std::shared_ptr<const material::Recipe>& paintRecipe() {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.test.paint"));
  return recipe;
}

/** …and the same colour with a body, for a tier that compiles one. The
 *  body returns the recipe's own parameter, so what reaches the pixels
 *  says whether the parameter reached the program. */
inline const std::shared_ptr<const material::Recipe>& slangPaintRecipe() {
  static const std::shared_ptr<const material::Recipe> recipe =
      std::make_shared<const material::Recipe>(
          material::Recipe::of<Paint>("world.test.paint.slang")
              .body(material::Target::Slang,
                    "\nfloat4 surface(float2 uv) { return baseColor; }\n"));
  return recipe;
}

/** A surface of @p colour that no tier compiles. */
inline material::Material paint(glm::vec4 colour) {
  return material::Material(paintRecipe(), Paint{colour});
}

/** …and one that a tier with a compiler runs. */
inline material::Material slangPaint(glm::vec4 colour) {
  return material::Material(slangPaintRecipe(), Paint{colour});
}

/** HOW MUCH INK stands in one half of @p plate — every pixel carrying
 *  any alpha at all, counted left of the middle or right of it. A
 *  selection that reached one body and not the other is visible as which
 *  half was painted, which is a reading no rasteriser's own precision
 *  can move. */
inline int paintedIn(const SkBitmap& plate, bool leftHalf) {
  int count = 0;
  const int mid = plate.width() / 2;
  for (int y = 0; y < plate.height(); ++y)
    for (int x = 0; x < plate.width(); ++x) {
      if (leftHalf != (x < mid)) continue;
      if (SkColorGetA(plate.getColor(x, y)) > 0) ++count;
    }
  return count;
}

}  // namespace sigil::world::test
