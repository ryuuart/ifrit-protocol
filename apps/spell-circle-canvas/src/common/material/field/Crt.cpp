/** @file
 * The screen's three subjects as three recipes — the beam, the light it
 * throws, the glass over both — and the composition that is the whole
 * tube: their bodies in one program, the glass reading the beam where it
 * reads a bound picture when it stands alone.
 */

#include <sigilmaterial/field/Crt.h>
#include <sigilshaders/MaterialField.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

namespace sigil::material::field {

namespace {

/** WHAT THE GLASS READS WHEN IT IS THE WHOLE SURFACE: the picture bound
 *  to it, at the coordinate it bent to. */
constexpr std::string_view kScreenIsTheContent =
    "half4 crtScreen(float2 p) { return content.eval(p); }";
/** And in the composed screen: the beam's own picture, so the tube is
 *  drawn flat and bent once by the glass over it. */
constexpr std::string_view kScreenIsTheBeam =
    "half4 crtScreen(float2 p) { return crtBeam(p); }";

/** A body out of @p parts — each module before the ones that read it —
 *  entered at @p call, the one line saying which of them is the whole
 *  picture. */
std::string bodyOf(std::initializer_list<std::string_view> parts,
                   std::string_view call) {
  std::string source;
  for (std::string_view part : parts) {
    source += part;
    source += '\n';
  }
  source += "half4 main(float2 p) { return ";
  source += call;
  source += "; }\n";
  return source;
}

}  // namespace

const std::shared_ptr<const Recipe>& crtBeamRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtBeamParameters>("field.crt.beam")
          .slot("content")
          .body(Target::SkSL,
                bodyOf({shaderSource("CrtBeam.sksl")}, "crtBeam(p)")));
  return recipe;
}

Material crtBeam(const CrtBeamParameters& parameters) {
  return Material(crtBeamRecipe(), parameters);
}

float crtBeamSampleRadius(const CrtBeamParameters& p) {
  return std::abs(p.uRgbShift) + std::abs(p.uJitter) + std::abs(p.uSync);
}

const std::shared_ptr<const Recipe>& crtBloomRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtBloomParameters>("field.crt.bloom")
          .slot("bloom", LayerFilter::Blurred, "uBloomRadius")
          .body(Target::SkSL,
                bodyOf({shaderSource("CrtBloom.sksl")}, "crtLight(p)")));
  return recipe;
}

Material crtBloom(const CrtBloomParameters& parameters) {
  return Material(crtBloomRecipe(), parameters);
}

const std::shared_ptr<const Recipe>& crtGlassRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtGlassParameters>("field.crt.glass")
          .slot("content")
          .slot("bloom", LayerFilter::Blurred, "uBloomRadius")
          .body(Target::SkSL,
                bodyOf({kScreenIsTheContent, shaderSource("CrtBloom.sksl"),
                        shaderSource("CrtGlass.sksl")},
                       "crtGlass(p)")));
  return recipe;
}

Material crtGlass(const CrtGlassParameters& parameters) {
  return Material(crtGlassRecipe(), parameters);
}

float crtGlassSampleRadius(const CrtGlassParameters& p) {
  const float extent = std::max(std::abs(p.uBounds.z), std::abs(p.uBounds.w));
  const float distortion = std::abs(p.uCurvature) * 0.5f;
  return extent * 0.5f * (1 + distortion) * distortion;
}

const std::shared_ptr<const Recipe>& crtRecipe() {
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<CrtParameters>("field.crt")
          .slot("content")
          .slot("bloom", LayerFilter::Blurred, "uBloomRadius")
          .body(Target::SkSL,
                bodyOf({shaderSource("CrtBeam.sksl"), kScreenIsTheBeam,
                        shaderSource("CrtBloom.sksl"),
                        shaderSource("CrtGlass.sksl")},
                       "crtGlass(p)")));
  return recipe;
}

Material crt(const CrtParameters& parameters) {
  return Material(crtRecipe(), parameters);
}

float crtSampleRadius(const CrtParameters& p) {
  // A COMPOSITION REACHES AS FAR AS WHAT IT COMPOSES. The glass bends a
  // reading, the beam sweeps one sideways and spreads it across the
  // guns, and the light asks for no reach at all: nothing gathers it,
  // and the executor's blur grows its own input bounds.
  CrtGlassParameters glass;
  glass.uBounds = p.uBounds;
  glass.uCurvature = p.uCurvature;
  CrtBeamParameters beam;
  beam.uRgbShift = p.uRgbShift;
  beam.uJitter = p.uJitter;
  beam.uSync = p.uSync;
  return crtGlassSampleRadius(glass) + crtBeamSampleRadius(beam);
}

}  // namespace sigil::material::field
