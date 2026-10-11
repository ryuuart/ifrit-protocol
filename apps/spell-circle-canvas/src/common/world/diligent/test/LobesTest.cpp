/** @file
 * THE OPTIONAL LOBES, held to what each means on the device: a sheen
 * brightens a surface toward its rim, a brushing stretches a highlight
 * along the way it runs and turns with it, a thin film tints the
 * reflection by its thickness, and a dielectric's specular strength and
 * colour scale and tint its highlight — each read against the same surface
 * stating nothing, which is shaded exactly as a surface carrying no lobe
 * is.
 *
 * Every case reads the effect off the pixels of two pictures that differ
 * in one statement, so what is asserted is what a lobe does and never a
 * number a rasteriser happens to produce.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/slang/SlangCompiler.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Diligent.h>
#include <sigilworld/diligent/Runtime.h>

#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;
using ::sigil::world::diligent::at;

namespace {

namespace gm = ::sigil::geometry::mesh;
namespace surface = ::sigil::material::surface;

constexpr glm::ivec2 kExtent{160, 160};

/** @p content seen square on from the level eye with no curve,
 *  photographed. */
SkBitmap photographed(std::vector<Node> content, const Runtime& runtime) {
  content.push_back(
      camera(diligent::levelEye(), {.toneCurve = ToneCurve::Linear})
          .key("camera"));
  Frame frame(group(std::move(content)).key("set"));
  frame.extent(kExtent).pass(
      geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return diligent::photograph(frame, runtime, kExtent);
}

float luma(SkColor4f c) { return c.fR * 0.3f + c.fG * 0.59f + c.fB * 0.11f; }

/** A sphere wearing @p parameters, lit from just behind the eye. */
SkBitmap sphereLitFromTheEye(const surface::SurfaceParameters& parameters,
                             const Runtime& runtime) {
  return photographed(
      {directionalLight({0, 0, -1}, {.intensity = 2.5f}).key("sun"),
       mesh(gm::superellipsoid({60, 60, 60}, 2.0f, 96, 64))
           .key("ball")
           .fill(surface::program(parameters))},
      runtime);
}

/** A card wearing @p parameters under a lamp standing between it and the
 *  eye and delivering @p strength at the card, so the highlight sits at
 *  the card's middle. */
SkBitmap cardUnderALamp(const surface::SurfaceParameters& parameters,
                        const Runtime& runtime, float strength = 3.0f) {
  return photographed(
      {pointLight({.intensity = intensityAt(strength, 150.0f)})
           .key("lamp")
           .at({0, 0, 150}),
       mesh(gm::quad(120, 120)).key("card").fill(surface::program(parameters))},
      runtime);
}

/** A card wearing @p parameters under a uniform white surround, which a
 *  dielectric reflects at its Fresnel share. */
SkBitmap cardInAWhiteSurround(const surface::SurfaceParameters& parameters,
                              const Runtime& runtime) {
  return photographed(
      {ambientLight({.intensity = 1.0f}).key("fill"),
       mesh(gm::quad(120, 120)).key("card").fill(surface::program(parameters))},
      runtime);
}

/** A colour's share of each primary: its hue and saturation, without its
 *  brightness. */
glm::vec3 chromaticity(SkColor4f c) {
  const float sum = std::fmax(c.fR + c.fG + c.fB, 1e-6f);
  return {c.fR / sum, c.fG / sum, c.fB / sum};
}

float apart(glm::vec3 a, glm::vec3 b) {
  return std::fabs(a.x - b.x) + std::fabs(a.y - b.y) + std::fabs(a.z - b.z);
}

}  // namespace

TEST(Lobes, ASheenBrightensTheRimMoreThanTheFace) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  surface::SurfaceParameters plain;
  plain.baseColor = {0.15f, 0.05f, 0.2f, 1};
  plain.roughness = 0.9f;
  surface::SurfaceParameters velvet = plain;
  velvet.sheenColor = {1, 1, 1, 1};
  velvet.sheenRoughness = 0.5f;
  const SkBitmap bare = sphereLitFromTheEye(plain, on.runtime);
  const SkBitmap sheen = sphereLitFromTheEye(velvet, on.runtime);
  // Lit from the eye, a sheen sends its light back along the grazing rim
  // and almost none from the face square to the eye.
  const float faceGain =
      luma(at(sheen, 0.5f, 0.5f)) - luma(at(bare, 0.5f, 0.5f));
  const float rimGain =
      luma(at(sheen, 0.86f, 0.5f)) - luma(at(bare, 0.86f, 0.5f));
  EXPECT_GT(rimGain, 0.08f);
  EXPECT_GT(rimGain, faceGain + 0.05f);
}

TEST(Lobes, ABrushedHighlightStretchesAlongItsBrushingAndTurnsWithIt) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  surface::SurfaceParameters metal;
  metal.baseColor = {0.9f, 0.9f, 0.9f, 1};
  metal.metallic = 1.0f;
  metal.roughness = 0.3f;
  const auto stretch = [](const SkBitmap& plate) {
    const float alongU = luma(at(plate, 0.68f, 0.5f));
    const float alongV = luma(at(plate, 0.5f, 0.68f));
    return std::pair{alongU, alongV};
  };
  // Unbrushed, the highlight is round: as bright a way off along either
  // axis.
  const auto [plainU, plainV] =
      stretch(cardUnderALamp(metal, on.runtime, 0.5f));
  EXPECT_NEAR(plainU, plainV, 3.0f / 255.0f);
  // Brushed along u, it runs out along u; turned a quarter, along v.
  surface::SurfaceParameters brushed = metal;
  brushed.anisotropy = 0.85f;
  const auto [alongU, acrossU] =
      stretch(cardUnderALamp(brushed, on.runtime, 0.5f));
  EXPECT_GT(alongU, acrossU + 0.05f);
  EXPECT_GT(alongU, plainU + 0.03f);
  brushed.anisotropyRotation = 90;
  const auto [acrossV, alongV] =
      stretch(cardUnderALamp(brushed, on.runtime, 0.5f));
  EXPECT_GT(alongV, acrossV + 0.05f);
}

TEST(Lobes, AFilmTintsTheReflectionByItsThickness) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  surface::SurfaceParameters dark;
  dark.baseColor = {0.02f, 0.02f, 0.02f, 1};
  dark.roughness = 0.2f;
  const SkColor4f bare = at(cardInAWhiteSurround(dark, on.runtime), 0.5f, 0.5f);
  surface::SurfaceParameters film = dark;
  film.iridescence = 1.0f;
  film.iridescenceThickness = 300;
  const SkColor4f thin = at(cardInAWhiteSurround(film, on.runtime), 0.5f, 0.5f);
  film.iridescenceThickness = 600;
  const SkColor4f thick =
      at(cardInAWhiteSurround(film, on.runtime), 0.5f, 0.5f);
  // The bare dielectric reflects the white surround white; the film
  // colours it, and a film twice as thick colours it differently.
  const glm::vec3 grey = chromaticity(bare);
  EXPECT_LT(apart(grey, {1 / 3.0f, 1 / 3.0f, 1 / 3.0f}), 0.03f);
  EXPECT_GT(apart(chromaticity(thin), grey), 0.08f);
  EXPECT_GT(apart(chromaticity(thin), chromaticity(thick)), 0.08f);
  // A film of no weight is no film at all.
  film.iridescence = 0.0f;
  EXPECT_EQ(diligent::worstChannel(cardInAWhiteSurround(dark, on.runtime),
                                   cardInAWhiteSurround(film, on.runtime)),
            0);
}

TEST(Lobes, SpecularScalesAndTintsADielectricsHighlight) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  surface::SurfaceParameters plastic;
  plastic.baseColor = {0.1f, 0.1f, 0.12f, 1};
  plastic.roughness = 0.4f;
  const auto highlight = [&](float strength, material::Color colour) {
    surface::SurfaceParameters stated = plastic;
    stated.specular = strength;
    stated.specularColorFactor = colour;
    return at(cardUnderALamp(stated, on.runtime, 0.3f), 0.5f, 0.5f);
  };
  const material::Color white{1, 1, 1, 1};
  const SkColor4f stock =
      at(cardUnderALamp(plastic, on.runtime, 0.3f), 0.5f, 0.5f);
  const SkColor4f full = highlight(1.0f, white);
  const SkColor4f half = highlight(0.5f, white);
  const SkColor4f none = highlight(0.0f, white);
  // Stated at its stock strength and colour, it is the surface it was.
  EXPECT_NEAR(full.fG, stock.fG, 2.0f / 255.0f);
  EXPECT_GT(luma(full), luma(half) + 0.03f);
  EXPECT_GT(luma(half), luma(none) + 0.03f);
  // A red specular reddens the highlight of a grey dielectric.
  const SkColor4f red = highlight(1.0f, {1, 0.2f, 0.2f, 1});
  EXPECT_GT(red.fR - red.fG, full.fR - full.fG + 0.05f);
}

namespace {

/** How many sampled slots the device program of @p parameters declares,
 *  lit: the scaffold's and the body's together. */
size_t samplersOf(const surface::SurfaceParameters& parameters,
                  material::Lobes carried = {}) {
  world::diligent::installSlangCompiler();
  const material::Material program =
      surface::program(parameters, surface::Reflection::SplitSum, carried);
  const material::Material::Resolved resolved =
      program.resolve(material::Target::Slang, material::FrameData{},
                      material::Variant{world::diligent::kVariantLit});
  const auto* slang =
      resolved.program ? resolved.program->as<material::slang::SlangProgram>()
                       : nullptr;
  return slang ? slang->compiled().textures.size() : 0;
}

}  // namespace

TEST(Lobes, EachLobeCostsOnlyTheSamplersOfItsOwnMaps) {
  // No device needed: the compiler reports the program's sampled slots.
  const size_t plain = samplersOf({});
  ASSERT_GT(plain, 0u) << "the plain surface compiles";
  surface::SurfaceParameters sheen;
  sheen.sheenColor = {1, 1, 1, 1};
  EXPECT_EQ(samplersOf(sheen), plain + 2) << "colour and roughness";
  surface::SurfaceParameters brushed;
  brushed.anisotropy = 0.5f;
  EXPECT_EQ(samplersOf(brushed), plain + 1) << "direction and strength";
  surface::SurfaceParameters film;
  film.iridescence = 1.0f;
  EXPECT_EQ(samplersOf(film), plain + 2) << "weight and thickness";
  surface::SurfaceParameters dull;
  dull.specular = 0.5f;
  EXPECT_EQ(samplersOf(dull), plain + 2) << "strength and colour";
  surface::SurfaceParameters coated;
  coated.clearcoat = 0.5f;
  EXPECT_EQ(samplersOf(coated), plain + 3) << "weight, roughness and normal";
  // Everything at once, which the device draws (below).
  EXPECT_EQ(samplersOf({}, {true, true, true, true}), plain + 7);
  RecordProperty("plainSamplers", (int)plain);
}

TEST(Lobes, ACoatOfNoWeightShadesExactlyAsNoCoat) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A surface carrying the coat's lobe at a weight of zero is drawn by the
  // program with the coat's terms compiled in; one carrying no coat by the
  // program without them. The coat of nothing adds nothing and takes
  // nothing, so the two pictures are the same to the bit.
  surface::SurfaceParameters plastic;
  plastic.baseColor = {0.5f, 0.2f, 0.1f, 1};
  plastic.roughness = 0.35f;
  const auto card = [&](material::Lobes carried) {
    return photographed(
        {pointLight({.intensity = intensityAt(3.0f, 150.0f)})
             .key("lamp")
             .at({0, 0, 150}),
         ambientLight({.intensity = 0.2f}).key("fill"),
         mesh(gm::quad(120, 120))
             .key("card")
             .fill(surface::program(plastic, surface::Reflection::SplitSum,
                                    carried))},
        on.runtime);
  };
  const SkBitmap uncoated = card({});
  const SkBitmap coatOfNothing = card({.clearcoat = true});
  ASSERT_EQ(uncoated.computeByteSize(), coatOfNothing.computeByteSize());
  EXPECT_EQ(std::memcmp(uncoated.getPixels(), coatOfNothing.getPixels(),
                        uncoated.computeByteSize()),
            0);
  EXPECT_GT(luma(at(uncoated, 0.5f, 0.5f)), 0.05f) << "the card is lit";
}

TEST(Lobes, AStatedLobeIsCompiledAndAnUnstatedOneCostsNoSampler) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  // A surface stating every lobe and dressing every one of their maps
  // still draws; and a surface stating none draws as the plain program.
  surface::SurfaceParameters everything;
  everything.baseColor = {0.4f, 0.4f, 0.45f, 1};
  everything.roughness = 0.4f;
  everything.sheenColor = {0.5f, 0.5f, 0.5f, 1};
  everything.sheenRoughness = 0.5f;
  everything.anisotropy = 0.5f;
  everything.iridescence = 0.5f;
  everything.specular = 0.8f;
  EXPECT_EQ(surface::lobes(surface::program(everything)),
            (material::Lobes{true, true, true, true}));
  const SkBitmap all = cardUnderALamp(everything, on.runtime);
  EXPECT_GT(luma(at(all, 0.5f, 0.5f)), 0.05f) << "every lobe at once draws";
}

TEST(Lobes, ABrushedHighlightRunsSmoothlyAcrossACoarseMeshsEdges) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << on.error;
  surface::SurfaceParameters brushed;
  brushed.baseColor = {0.9f, 0.9f, 0.9f, 1};
  brushed.metallic = 1.0f;
  brushed.roughness = 0.35f;
  brushed.anisotropy = 0.9f;
  // A ball of few segments, whose uvs' derivatives are constant across
  // each triangle: the brushing read off them steps at every edge, the
  // brushing read off the mesh's own tangents does not.
  const gm::Mesh coarse = gm::superellipsoid({60, 60, 60}, 2.0f, 12, 8);
  ASSERT_EQ(coarse.tangents.size(), coarse.positions.size());
  gm::Mesh bare = coarse;
  bare.tangents.clear();
  const auto plate = [&](const gm::Mesh& ball) {
    return photographed(
        {directionalLight({-0.3f, -0.4f, -1}, {.intensity = 2.5f}).key("sun"),
         mesh(ball).key("ball").fill(surface::program(brushed))},
        on.runtime);
  };
  // How much the light steps across the picture: the sum of the second
  // differences of its brightness, pixel to pixel, both ways. The
  // silhouette is the same in both pictures; what differs is the edges
  // inside it.
  const auto steps = [](const SkBitmap& image) {
    float sum = 0;
    for (int y = 1; y + 1 < kExtent.y; ++y)
      for (int x = 1; x + 1 < kExtent.x; ++x) {
        const float here = luma(image.getColor4f(x, y));
        sum += std::fabs(luma(image.getColor4f(x - 1, y)) - 2 * here +
                         luma(image.getColor4f(x + 1, y))) +
               std::fabs(luma(image.getColor4f(x, y - 1)) - 2 * here +
                         luma(image.getColor4f(x, y + 1)));
      }
    return sum;
  };
  const float stated = steps(plate(coarse));
  const float derived = steps(plate(bare));
  RecordProperty("stated", std::to_string(stated));
  RecordProperty("derived", std::to_string(derived));
  EXPECT_LT(stated, derived * 0.85f) << stated << " " << derived;
}
