#pragma once

#include <sigilmaterial/advanced/UniformBlock.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>

#include <array>
#include <memory>
#include <string_view>

namespace painted_fields {

namespace material = sigil::material;

enum class Finish { Stone, Steel, Bronze };

struct Controls {
  float depth = 11;
  float wetness = .8f;
  float dryRoughness = .82f;
  float wetRoughness = .13f;
};

struct Parameters {
  material::Color color{.52f, .57f, .54f, 1};
  float dryRoughness = .82f;
  float wetRoughness = .13f;
  float absorption = .46f;
  float metallic = 0;
  std::array<float, 1> wetness{.8f};
};

inline constexpr std::string_view kColor = R"(
float hash(float2 p) { return fract(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
half4 main(float2 p) {
  float wet = clamp(water.eval(p).r * wetness[0], 0.0, 1.0);
  float noise = hash(floor(p*.8));
  float vein = .5+.5*sin(p.y*.035+sin(p.x*.018)*1.7);
  float rock = .82 + .13*noise + .08*vein;
  float brush = .90 + .045*sin(p.y*2.8) + .04*noise;
  float grain = mix(rock,brush,metallic);
  float darkening = 1.0-absorption*wet;
  return half4(color.rgb*grain*darkening*color.a,color.a);
})";

inline constexpr std::string_view kRoughness = R"(
half4 main(float2 p) {
  float wet = clamp(water.eval(p).r * wetness[0], 0.0, 1.0);
  float roughness = mix(dryRoughness,wetRoughness,wet);
  return half4(float3(roughness),1);
})";

inline constexpr std::string_view kBrushing = R"(
half4 main(float2 p) {
  float slope = .10*cos(p.y*2.8) + .04*cos(p.y*.43+p.x*.002);
  float3 n = normalize(float3(0,-slope,1));
  return half4(n*.5+.5,1);
})";

inline material::Texture placed(sk_sp<SkImage> pixels, float width,
                                float height) {
  material::Texture texture(std::move(pixels));
  const glm::ivec2 extent = texture.size();
  if (extent.x > 0 && extent.y > 0) {
    glm::mat3 mapping(1);
    mapping[0][0] = width / extent.x;
    mapping[1][1] = height / extent.y;
    texture.uv(mapping);
  }
  return texture;
}

inline Parameters parameters(Finish finish, const Controls& controls) {
  Parameters parameters;
  parameters.dryRoughness = controls.dryRoughness;
  parameters.wetRoughness = controls.wetRoughness;
  if (finish == Finish::Steel) {
    parameters.color = {.61f, .67f, .69f, 1};
    parameters.metallic = 1;
    parameters.absorption = 0;
    parameters.dryRoughness = .44f;
  } else if (finish == Finish::Bronze) {
    parameters.color = {.77f, .49f, .24f, 1};
    parameters.metallic = 1;
    parameters.absorption = 0;
    parameters.dryRoughness = .46f;
  }
  if (controls.dryRoughness == 1 && controls.wetRoughness == 1)
    parameters.dryRoughness = 1;
  return parameters;
}

inline material::Material surface(
    Finish finish, const Controls& controls, material::Texture height,
    material::Texture water,
    const std::shared_ptr<material::UniformBlock>& wetness) {
  const Parameters values = parameters(finish, controls);
  const material::ShaderOptions inputs{.textures = {{"water", {}}}};
  auto color = material::shader(kColor, values, inputs);
  color.slot("water", water).bind("wetness", wetness);
  auto roughness = material::shader(kRoughness, values, inputs);
  roughness.slot("water", water).bind("wetness", wetness);
  auto normal = material::surface::normalFromHeight(
      material::image(std::move(height)),
      {.depth = controls.depth, .step = .8f, .directX = true});
  if (finish != Finish::Stone)
    normal = material::surface::blendNormals(
        normal, material::shader(kBrushing),
        {.baseDirectX = true, .detailDirectX = true, .outputDirectX = true});
  return color.surface(
      {.metallic = values.metallic,
       .roughness = roughness,
       .normal = normal,
       .normalDirectX = true,
       .clearcoat = 0,
       .reflectionWeight = finish == Finish::Stone ? .42f : .86f});
}

}  // namespace painted_fields
