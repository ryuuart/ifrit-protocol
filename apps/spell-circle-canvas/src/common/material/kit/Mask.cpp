/** @file
 * The two mask bodies in each language a renderer speaks — a shaped
 * constant and a shaped reading of a source slot — and the factories
 * that fix which reading is used.
 *
 * A mask answers a SCALAR that its caller reads out of the red channel,
 * so both bodies return it in all three colour channels at full alpha:
 * a mask drawn on its own is a grey picture of where it applies.
 */

#include "sigilmaterial/kit/Mask.h"

#include <string>
#include <utility>

namespace sigil::material::kit {

namespace {

/** The fit both bodies end with: the raw value remapped onto 0..1 by
 *  [low, high], clamped, then flipped when inverted. */
constexpr char kFit[] = R"(
half shape(float raw) {
  float t = clamp((raw - low) / max(high - low, 1e-5), 0.0, 1.0);
  return half(inverted > 0.5 ? 1.0 - t : t);
}
)";

constexpr char kConstant[] = R"(
half4 main(float2 xy) {
  return half4(shape(value));
}
)";

constexpr char kSampled[] = R"(
half chan(half4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

float3 unit(float3 v) {
  float len = length(v);
  return len < 1e-5 ? float3(0.0, 1.0, 0.0) : v / len;
}

half4 main(float2 xy) {
  half4 s = source.eval(xy);
  float raw = reading < 0.5   ? float(chan(s, channel))
              : reading < 1.5 ? dot(float3(s.rgb) * 2.0 - 1.0, unit(axis.xyz))
                              : dot(float3(s.rgb), axis.xyz);
  return half4(shape(raw));
}
)";

/** The same two readings in Slang, for a renderer that compiles it. The
 *  params, the slot and the fit are the same ABI, because they are the
 *  same recipe; where the SkSL body evaluates the source as a shader,
 *  this samples it as a texture. */
constexpr char kSlangFit[] = R"(
float shapeS(float raw) {
  float t = clamp((raw - low) / max(high - low, 1e-5), 0.0, 1.0);
  return inverted > 0.5 ? 1.0 - t : t;
}
)";

constexpr char kSlangConstant[] = R"(
float4 surface(float2 uv) {
  float v = shapeS(value);
  return float4(v, v, v, 1.0);
}
)";

constexpr char kSlangSampled[] = R"(
float chanM(float4 c, float which) {
  return which < 0.5 ? c.r : which < 1.5 ? c.g : which < 2.5 ? c.b : c.a;
}

float3 unitM(float3 v) {
  float len = lengthP(v);
  return len < 1e-5 ? float3(0.0, 1.0, 0.0) : v / len;
}

float4 surface(float2 uv) {
  float4 s = source.Sample(uv);
  float raw = reading < 0.5   ? chanM(s, channel)
              : reading < 1.5 ? dotP(s.rgb * 2.0 - 1.0, unitM(axis.xyz))
                              : dotP(s.rgb, axis.xyz);
  float v = shapeS(raw);
  return float4(v, v, v, 1.0);
}
)";

Material sampled(Texture source, MaskParams params) {
  Material m(sampledMaskRecipe(), params);
  m.child(kMaskSourceSlot, std::move(source));
  return m;
}

}  // namespace

const std::shared_ptr<const Recipe>& constantMaskRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<MaskParams>("mask.constant")
              .body(Target::SkSL, std::string(kFit) + kConstant)
              .body(Target::Slang, std::string(kSlangFit) + kSlangConstant));
  return recipe;
}

const std::shared_ptr<const Recipe>& sampledMaskRecipe() {
  static const std::shared_ptr<const Recipe> recipe =
      std::make_shared<const Recipe>(
          Recipe::of<MaskParams>("mask.sampled")
              .child(std::string(kMaskSourceSlot))
              .body(Target::SkSL, std::string(kFit) + kSampled)
              .body(Target::Slang, std::string(kSlangFit) + kSlangSampled));
  return recipe;
}

Material maskConstant(float value) {
  MaskParams params;
  params.value = value;
  return Material(constantMaskRecipe(), params);
}

Material maskMap(Texture map, int channel) {
  MaskParams params;
  params.channel = (float)channel;
  params.reading = (float)MaskReading::Channel;
  return sampled(std::move(map), params);
}

Material maskVertexColor(Texture colors, int channel) {
  return maskMap(std::move(colors), channel);
}

Material maskSlope(Texture normals, glm::vec3 up, float low, float high) {
  MaskParams params;
  params.reading = (float)MaskReading::Slope;
  params.axis = {up.x, up.y, up.z, 0};
  params.low = low;
  params.high = high;
  return sampled(std::move(normals), params);
}

Material maskHeight(Texture positions, float low, float high, glm::vec3 axis) {
  MaskParams params;
  params.reading = (float)MaskReading::Height;
  params.axis = {axis.x, axis.y, axis.z, 0};
  params.low = low;
  params.high = high;
  return sampled(std::move(positions), params);
}

Material fit(Material mask, float low, float high) {
  mask.set("low", low);
  mask.set("high", high);
  return mask;
}

Material invert(Material mask) {
  mask.set("inverted", mask.get<float>("inverted") > 0.5f ? 0.0f : 1.0f);
  return mask;
}

}  // namespace sigil::material::kit
