#pragma once

/** @file
 * What a test in this library needs before it can author or read a
 * stage: somewhere of its own to write one, the runtime skip every case
 * here opens with, and the scene both the writer and the reader are
 * exercised over.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/kit/Surface.h>
#include <sigilusd/runtime/Runtime.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "ScratchDir.h"

/** USD's file formats are discovered on disk, and a build whose plugin
 *  registry is not beside its libraries opens nothing. That is the
 *  condition the probe exists to report, so a case reports it too. */
#define SKIP_WITHOUT_USD()                                \
  do {                                                    \
    std::string why;                                      \
    if (!sigil::usd::available(&why))                     \
      GTEST_SKIP() << "USD runtime unavailable: " << why; \
  } while (0)

namespace sigil::usd::test {

/** @p name under a directory this process alone writes to, so two runs
 *  never read each other's stages and neither leaves one behind. */
inline std::filesystem::path scratch(const char* name) {
  static const sigil::test::ScratchDir dir("sigilusd_test");
  return dir.path / name;
}

/** A 4 x 4 image of one colour, for a material that wants a texture. */
inline sk_sp<SkImage> solid(SkColor color) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(4, 4));
  surface->getCanvas()->clear(color);
  return surface->makeImageSnapshot();
}

/** A ring whose triangles alternate between two material slots, and the
 *  two materials those slots name: a red surface, and one wearing a blue
 *  base-colour texture. The scene a stage with subsets, shared material
 *  prims and one texture file beside it is authored from. */
struct Torus {
  geometry::mesh::Mesh mesh;
  material::Material red;
  material::Material textured;
};

inline Torus twoSlotTorus() {
  geometry::mesh::Mesh mesh = geometry::mesh::torus(100, 40, 24, 12);
  std::vector<glm::vec4>& lane = mesh.prim("Material", {0, 0, 0, 0});
  for (size_t t = 0; t < lane.size(); ++t) lane[t] = {(float)(t % 2), 0, 0, 0};

  material::kit::SurfaceParams redParams;
  redParams.baseColor = {1, 0, 0, 1};
  redParams.roughness = 0.3f;
  redParams.metallic = 0.75f;

  material::Material textured = material::kit::surface();
  textured.child(
      material::kit::kBaseColorSlot,
      material::Texture::of(solid(SK_ColorBLUE)).tile(SkTileMode::kRepeat));

  return Torus{std::move(mesh), material::kit::surface(redParams),
               std::move(textured)};
}

}  // namespace sigil::usd::test
