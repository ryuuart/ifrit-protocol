/** @file
 * The kit's recipes as a list of instances — every builder called once,
 * with a stand-in normal map, environment and backdrop for the surfaces
 * that read an image.
 */

#include "sigilmaterial/kit/Recipes.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkSurface.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/kit/Mask.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilmaterial/kit/Surfaces.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>

namespace sigil::material::kit {

namespace {

/** A one-colour image, for a slot that must hold something for the
 *  recipe to generate the program it generates when it is dressed. */
Texture stand(SkColor color) {
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  s->getCanvas()->clear(color);
  return Texture::of(s->makeImageSnapshot());
}

}  // namespace

std::vector<Material> everyRecipe() {
  const SkPath shape = SkPath::Circle(20, 20, 16);
  const Texture normals = bevelNormals(shape, 5);
  const EnvironmentMap env = EnvironmentMap::studio(64);
  const SkRect bounds = SkRect::MakeWH(64, 24);
  // A time other than zero, because a body whose motion is folded away at
  // t = 0 is not the body that runs.
  constexpr float kSeconds = 1.25f;
  return {
      surface({}, Reflection::SplitSum),
      surface({}, Reflection::Additive),
      unlit(),
      gold(normals, env),
      chrome(normals, env),
      glass(normals, env, stand(SK_ColorCYAN)),
      stone(),
      timber(),
      latten(),
      board(),
      maskConstant(0.5f),
      maskMap(stand(SK_ColorWHITE)),
      water(bounds, kSeconds),
      meshGradient(bounds, kSeconds),
      sparkle(bounds, kSeconds),
      starNest(bounds, kSeconds),
      clouds(bounds, kSeconds),
      tunnel(bounds, kSeconds),
  };
}

}  // namespace sigil::material::kit
