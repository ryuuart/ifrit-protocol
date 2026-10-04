// Ceramic fills and inks with seeded crazing, pores, rounded shoulders and
// apparent formed relief. Lighting changes the normals; the planes stay flat.
// TAGS: Materials/Ceramics, Materials/Lighting, Compose/Layers,
// Interfaces/Controls

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/path/Noise.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Triangulate.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/geometric.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace media = sigil::media;
namespace motion = sigil::motion;
namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace noise = sigil::core::noise;
namespace kit = sigil::compose::kit;

namespace {

constexpr int kWidth = 1440, kHeight = 1000, kMapSize = 768;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kLoopSeconds = 36;

struct Controls {
  float roughness = 0.17f;
  float normalStrength = 1;
  float glazeWeight = 1;
  float porosity = 0.75f;
  float crackStrength = 0.72f;
  float reliefDepth = 3.4f;
  float lightSpeed = 10;
};
constexpr Controls kControls;

float bounded(float value, float lo, float hi, float fallback) {
  return std::clamp(std::isfinite(value) ? value : fallback, lo, hi);
}
material::Color ink(uint32_t rgb, float alpha = 1) {
  auto color = material::hexColor(rgb);
  color.a = alpha;
  return color;
}
std::string number(float value, int precision = 2) {
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision, value);
  return buffer.data();
}

struct Moment {
  float roughness = 0.17f, normal = 1, glaze = 1, depth = 3.4f;
  float elevation = 46;
  bool scalarRoughness = false, unlit = false;
  int phase = 0;
  const char* name = "GLAZED / WORKING";
  const char* note =
      "Crazed celadon, porcelain controls and colored glaze pads";
};
Moment moment(int phase) {
  Moment m;
  m.phase = phase;
  m.roughness = bounded(kControls.roughness, 0, 1, 0.17f);
  m.normal = bounded(kControls.normalStrength, 0, 3, 1);
  m.glaze = bounded(kControls.glazeWeight, 0, 1, 1);
  m.depth = bounded(kControls.reliefDepth, 0, 10, 3.4f);
  switch (phase) {
    case 1:
      m.roughness = 0;
      m.glaze = 1;
      m.scalarRoughness = true;
      m.name = "ROUGHNESS / ZERO";
      m.note = "Scalar zero; a moving softbox follows the normal field";
      break;
    case 2:
      m.roughness = 1;
      m.glaze = 1;
      m.scalarRoughness = true;
      m.name = "ROUGHNESS / ONE";
      m.note =
          "Rough body under a glossy coating; pore normals remain connected";
      break;
    case 3:
      m.normal = 0;
      m.name = "NORMAL / ZERO";
      m.note = "Normal perturbation removed; the stacked shapes remain";
      break;
    case 4:
      m.normal = 3;
      m.name = "NORMAL / STRONG";
      m.note = "Shoulder, craze and pore normals at the study maximum";
      break;
    case 5:
      m.glaze = 0;
      m.name = "COATING / ZERO";
      m.note =
          "Clearcoat disabled; the colored body retains its roughness and maps";
      break;
    case 6:
      m.elevation = 9;
      m.name = "RAKING / STUDIO LIGHT";
      m.note = "Low directional light reveals shallow shoulders and lettering";
      break;
    case 7:
      m.unlit = true;
      m.name = "COLOR / NO ILLUMINATION";
      m.note = "Color stack alone; the fixed coupons retain their studio";
      break;
    case 8:
      m.depth = 0;
      m.name = "PROFILE / ZERO HEIGHT";
      m.note = "Macro-height removed; fine finish maps remain connected";
      break;
    default:
      break;
  }
  return m;
}

compose::Element label(std::string text, float x, float y, float width,
                       float size, material::Color color, float weight = 450) {
  return compose::text(std::move(text))
      .left(x)
      .top(y)
      .width(width)
      .height(size * 1.6f)
      .font({.size = size, .color = color})
      .fontWeight(weight);
}
compose::Element rule(float x, float y, float width, material::Color color) {
  return kit::at(x, y, width, 1).fill(color);
}

struct CeramicMaps {
  material::Texture color, normal, roughness;
};

CeramicMaps finishMaps(uint32_t seed, float porosity, bool crazed) {
  std::array<SkBitmap, 3> pixels;
  for (auto& bitmap : pixels) bitmap.allocN32Pixels(kMapSize, kMapSize);
  SkBitmap cracks, pores;
  cracks.allocN32Pixels(kMapSize, kMapSize);
  pores.allocN32Pixels(kMapSize, kMapSize);
  cracks.eraseColor(SK_ColorBLACK);
  pores.eraseColor(SK_ColorBLACK);
  noise::Mix64Stream random(seed);
  if (crazed) {
    std::vector<glm::vec2> seeds;
    for (int i = 0; i < 62; ++i)
      seeds.emplace_back(random.range(-30, kMapSize + 30),
                         random.range(-30, kMapSize + 30));
    SkCanvas canvas(cracks);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2.0f);
    for (const auto& cell :
         path::voronoi(seeds, path::Rect::of({0, 0}, {kMapSize, kMapSize})))
      canvas.drawPath(path::toSk(path::toPath(cell)), paint);
  }
  {
    SkCanvas canvas(pores);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    for (int i = 0; i < 3100; ++i)
      canvas.drawCircle(random.range(0, kMapSize), random.range(0, kMapSize),
                        random.range(0.6f, 2.3f), paint);
  }
  const float crackStrength =
      crazed ? bounded(kControls.crackStrength, 0, 1, 0.72f) : 0;
  std::vector<float> heights(kMapSize * kMapSize);
  for (int y = 0; y < kMapSize; ++y) {
    for (int x = 0; x < kMapSize; ++x) {
      const float p = SkColorGetR(pores.getColor(x, y)) / 255.0f * porosity;
      const float c =
          SkColorGetR(cracks.getColor(x, y)) / 255.0f * crackStrength;
      const float coarse = path::valueNoise({x * 0.018f, y * 0.018f, 0}, seed);
      const float grain = noise::hash(seed, uint32_t(y * kMapSize + x));
      heights[size_t(y * kMapSize + x)] =
          0.016f * coarse + 0.004f * grain * porosity - 0.12f * p - 0.06f * c;
      const auto channel = [](float value) {
        return static_cast<U8CPU>(255 * std::clamp(value, 0.0f, 1.0f));
      };
      const float color = 0.96f + 0.027f * coarse - 0.24f * p - 0.30f * c;
      // Bitmap storage is native SkPMColor, not packed ARGB SkColor.
      *pixels[0].getAddr32(x, y) = SkPreMultiplyARGB(
          255, channel(color), channel(color), channel(color));
      const auto rough = channel(0.83f + 0.11f * p + 0.045f * coarse);
      *pixels[2].getAddr32(x, y) = SkPreMultiplyARGB(255, rough, rough, rough);
    }
  }
  const auto height = [&](int x, int y) {
    return heights[size_t(std::clamp(y, 0, kMapSize - 1) * kMapSize +
                          std::clamp(x, 0, kMapSize - 1))];
  };
  for (int y = 0; y < kMapSize; ++y) {
    for (int x = 0; x < kMapSize; ++x) {
      const glm::vec3 normal = glm::normalize(
          glm::vec3{-(height(x + 1, y) - height(x - 1, y)) * 4,
                    (height(x, y + 1) - height(x, y - 1)) * 4, 1});
      const auto channel = [](float value) {
        return static_cast<U8CPU>(255 *
                                  std::clamp(0.5f + 0.5f * value, 0.0f, 1.0f));
      };
      *pixels[1].getAddr32(x, y) = SkPreMultiplyARGB(
          255, channel(normal.x), channel(normal.y), channel(normal.z));
    }
  }
  return {material::Texture(
              media::PixelSource(SkImages::RasterFromBitmap(pixels[0]))),
          material::Texture(
              media::PixelSource(SkImages::RasterFromBitmap(pixels[1]))),
          material::Texture(
              media::PixelSource(SkImages::RasterFromBitmap(pixels[2])))};
}

material::Texture fitted(material::Texture texture, float width, float height) {
  glm::mat3 placement(1);
  placement[0][0] = width / kMapSize;
  placement[1][1] = height / kMapSize;
  texture.uv(placement).tile(material::Repeat::Pad);
  return texture;
}

struct Profile {
  float radius = 20;
  float shoulder = 8;
  float depth = 3.4f;
  float circular = 0;
  float waves = 0;
};
constexpr std::string_view kProfileNormal = R"(
float edgeDistance(float2 p) {
    float2 size = max(uResolution, float2(1));
    if (circular > 0.5)
        return length((p - size * .5) / (size * .5)) * min(size.x, size.y) * .5
               - min(size.x, size.y) * .5;
    float r = min(radius, min(size.x, size.y) * .5);
    float2 q = abs(p - size * .5) - size * .5 + r;
    return length(max(q, float2(0))) + min(max(q.x, q.y), 0.0) - r;
}
float heightAt(float2 p) {
    float2 size = max(uResolution, float2(1));
    float shoulderRamp = smoothstep(0.0, max(shoulder, .2), -edgeDistance(p));
    float2 q = p / size;
    float height = shoulderRamp * depth;
    if (circular > .5) {
        float rr = length((q - .5) * 2);
        height += depth * 1.3 * max(0.0, 1.0 - rr * rr);
        height += depth * .32 * exp(-abs(rr - .64) * 35);
    }
    float envelope = sin(clamp(q.x, 0.0, 1.0) * 3.141593)
                   * sin(clamp(q.y, 0.0, 1.0) * 3.141593);
    float wave = .5 + .5 * cos((q.x * 3.3 + q.y * 1.1) * 3.141593);
    return height + waves * depth * 3 * envelope * wave;
}
half4 main(float2 p) {
    float footprint = max(.55, 1.0 / max(uContentScale, .25));
    float dx = (heightAt(p + float2(footprint, 0)) - heightAt(p - float2(footprint, 0)))
               / (2 * footprint);
    float dy = (heightAt(p + float2(0, footprint)) - heightAt(p - float2(0, footprint)))
               / (2 * footprint);
    float3 fine = finish.eval(p).rgb * 2 - 1;
    float3 n = normalize(float3(-dx + fine.x, dy + fine.y, max(fine.z, .05)));
    return half4(half3(n * .5 + .5), 1);
}
)";

material::Material ceramic(const CeramicMaps& maps, material::Color color,
                           float width, float height, const Moment& m,
                           Profile profile = {}) {
  profile.depth = m.depth;
  auto base = material::image(fitted(maps.color, width, height));
  base.layer(material::from(color), {.blend = material::BlendMode::Multiply});
  auto normals =
      material::shader(kProfileNormal, profile, {.textures = {{"finish", {}}}});
  normals.slot("finish", fitted(maps.normal, width, height));
  const float roughness = m.roughness;
  material::Channel rough = roughness;
  if (!m.scalarRoughness) {
    auto map = material::image(fitted(maps.roughness, width, height));
    map.layer(
        material::from(material::Color{roughness, roughness, roughness, 1}),
        {.blend = material::BlendMode::Multiply});
    rough = std::move(map);
  }
  return base.surface({.roughness = std::move(rough),
                       .normal = std::move(normals),
                       .normalScale = m.normal,
                       .clearcoat = m.glaze});
}

material::EnvironmentMap studio() {
  return material::EnvironmentMap::baked(512, [](float u, float v) {
    const auto strip = [u, v](float center, float width, float elevation,
                              float height) {
      const float du = std::min(std::abs(u - center), 1 - std::abs(u - center));
      const float x = du / width, y = (v - elevation) / height;
      return std::exp(-.5f * (x * x + y * y));
    };
    glm::vec3 radiance{.66f, .64f, .60f};
    radiance += glm::vec3{4.8f, 4.6f, 4.2f} * strip(.12f, .04f, .40f, .24f);
    radiance += glm::vec3{2.1f, 2.8f, 3.4f} * strip(.65f, .10f, .42f, .18f);
    radiance += glm::vec3{1.9f, 1.6f, 1.3f} * strip(.96f, .03f, .50f, .25f);
    return radiance;
  });
}

struct CeramicGlaze {
  CeramicMaps porcelain, stoneware, crazing;
  material::EnvironmentMap environment;
  motion::Animatable<float> bearing = motion::animatable(118.0f);
  motion::Animatable<float> reflectionTurn = motion::animatable(0.0f);
  int phaseIndex = 0;

  material::Lighting lighting(float elevation = 46) const {
    return {
        material::studio({.direction = bearing,
                          .elevation = elevation,
                          .color = material::Color{.99f, .97f, .91f, 1},
                          .intensity = .62f,
                          .ambient = .42f}),
        material::environment(environment.texture().source(),
                              {.rotation = reflectionTurn, .intensity = .52f})};
  }

  compose::Element tile(float x, float y, float width, float height,
                        const CeramicMaps& maps, material::Color color,
                        const Moment& m, Profile profile = {}) const {
    return kit::at(x, y, width, height)
        .borderRadius(profile.circular > .5f ? width * .5f : profile.radius)
        .fill(ceramic(maps, color, width, height, m, profile));
  }

  compose::Element inscription(std::string text, float x, float y, float width,
                               float size, material::Color color,
                               const Moment& m, bool inset = false) const {
    // The ink's generated contour normals follow the placed glyphs.
    auto finish = material::from(color).surface({.roughness = m.roughness,
                                                 .normalScale = m.normal,
                                                 .clearcoat = m.glaze});
    return label(std::move(text), x, y, width, size, color, 600)
        .ink(material::Color{0, 0, 0, 0})
        .decorationOutline(compose::Boundary::Glyphs)
        .foreground(compose::relief(
            std::move(finish), {.shoulder = std::max(.6f, size * .028f),
                                .depth = (inset ? -1 : 1) * m.depth * .42f}));
  }

  compose::Element bench(const Moment& m) const {
    Moment body = m;
    body.roughness = .64f;
    const auto warm = ink(0xe7ddc6), inkBlue = ink(0x24464c);
    auto slab = kit::at(55, 236, 952, 508)
                    .fill(ink(0x998b73))
                    .borderRadius(29)
                    .filter(material::Filter::dropShadow(
                        ink(0x4c5141, .17f), {.blur = 15, .offset = {0, 12}}));
    auto face = tile(0, -8, 952, 508, stoneware, warm, body,
                     {.radius = 29, .shoulder = 13});
    face.children(
        {inscription("01 / CELADON RELIEF", 34, 25, 400, 14, inkBlue, body,
                     true),
         inscription("02 / PORCELAIN", 489, 25, 236, 14, inkBlue, body, true),
         inscription("03 / GLAZE", 747, 25, 195, 14, inkBlue, body, true),
         label("FORMED FINISH / NORMAL-MAP PROFILE", 33, 479, 590, 10, inkBlue),
         label("GLAZE ARCHIVE 04", 745, 479, 205, 10, inkBlue)});
    auto plate = kit::at(30, 71, 422, 371).fill(ink(0x567c72)).borderRadius(27);
    auto glazed = tile(0, -5, 422, 371, crazing, ink(0x84b6a6), m,
                       {.radius = 27, .shoulder = 13, .waves = 1});
    glazed.children({inscription("G / 04", 29, 22, 315, 39, ink(0xe8ecd2), m),
                     inscription("CELADON", 27, 242, 390, 45, ink(0xe5e2c8), m),
                     inscription("CRAZE / POROSITY / FORMED RIDGES", 30, 307,
                                 372, 11, ink(0x264d53), m, true),
                     label("A COLOR STUDY / NOT A FIRING SIMULATION", 31, 333,
                           370, 10, ink(0x284e53))});
    for (int i = 0; i <= 15; ++i)
      glazed.children({kit::at(387 - (i % 5 == 0 ? 11 : 5), 30 + i * 18,
                               i % 5 == 0 ? 11 : 5, 1)
                           .fill(ink(0x244e52, .58f))});
    plate.children({std::move(glazed)});
    face.children({std::move(plate)});

    // These are stacked Compose planes. The profile lives in the normals,
    // so rotating a lamp changes it without moving a silhouette.
    face.children(
        {tile(491, 82, 226, 226, stoneware, ink(0x8e9079), body,
              {.radius = 113, .shoulder = 15, .circular = 1}),
         tile(501, 76, 206, 206, porcelain, ink(0xeeead6), m,
              {.radius = 103, .shoulder = 11, .circular = 1}),
         tile(537, 113, 134, 134, porcelain, ink(0xe8d6b6), m,
              {.radius = 67, .shoulder = 5, .circular = 1}),
         inscription("04", 549, 147, 127, 53, inkBlue, m),
         inscription("GLAZE / WEIGHT", 501, 282, 229, 10, inkBlue, body, true),
         tile(491, 326, 98, 76, porcelain, ink(0xe5e8d5), m,
              {.radius = 17, .shoulder = 7}),
         tile(612, 326, 98, 76, porcelain, ink(0xe5e8d5), m,
              {.radius = 17, .shoulder = 7}),
         inscription("−", 520, 339, 59, 35, inkBlue, m),
         inscription("+", 640, 339, 59, 35, inkBlue, m),
         label("RAW BODY", 492, 423, 121, 10, inkBlue),
         label("GLAZED", 621, 423, 102, 10, inkBlue)});
    const std::array<material::Color, 3> colors{ink(0x366798), ink(0x925054),
                                                ink(0xb78547)};
    const std::array<const char*, 3> names{"COBALT", "OXBLOOD", "AMBER"};
    for (size_t i = 0; i < colors.size(); ++i) {
      const float y = 76 + float(i) * 113;
      auto pad = tile(750, y, 176, 83, crazing, colors[i], m,
                      {.radius = 21, .shoulder = 10});
      pad.children(
          {inscription("0" + std::to_string(i + 1), 16, 10, 72, 25,
                       ink(0xf2e4cd), m),
           inscription(names[i], 16, 50, 155, 12, ink(0xf2e4cd), m, true)});
      face.children({std::move(pad)});
    }
    slab.children({std::move(face)});
    slab.lighting(m.unlit ? material::Lighting{} : lighting(m.elevation));
    return slab;
  }

  compose::Element parameters(const Moment& m) const {
    const auto dark = ink(0x28474b), muted = ink(0x758278),
               green = ink(0x347c6e);
    auto out = kit::at(1050, 229, 338, 516);
    out.children({label("SURFACE PARAMETERS", 0, 0, 338, 13, dark, 600),
                  label("Body + dielectric coating", 0, 27, 338, 12, muted)});
    const std::array<std::string, 5> values{
        number(m.roughness), number(m.normal), number(m.glaze), number(m.depth),
        number(m.elevation, 0) + "°"};
    const std::array<const char*, 5> names{"ROUGHNESS", "NORMAL STRENGTH",
                                           "GLAZE WEIGHT", "PROFILE HEIGHT",
                                           "LIGHT ELEVATION"};
    for (size_t i = 0; i < values.size(); ++i) {
      const float y = 70 + float(i) * 78;
      out.children(
          {label(names[i], 0, y, 180, 10, muted),
           label(values[i], 0, y + 19, 240, 29, dark),
           label(i == 2 ? "coating / 0–1" : "", 185, y + 38, 145, 10, muted),
           rule(0, y + 61, 320, ink(0xb4bdae))});
    }
    out.children(
        {label("36 s / NINE DETERMINISTIC STATES", 0, 475, 335, 10, green)});
    for (int i = 0; i < 9; ++i)
      out.children({kit::at(i * 34.5f, 498, 26, 5)
                        .borderRadius(2.5f)
                        .fill(i == m.phase ? green : ink(0xcdd1c1))});
    return out;
  }

  compose::Element specimens() const {
    auto out = kit::at(55, 814, 1334, 151).lighting(lighting());
    const std::array<const char*, 6> titles{"PORCELAIN",      "STONEWARE",
                                            "CRAZED CELADON", "GLAZE 0",
                                            "GLAZE 1",        "NORMAL 0 / 3"};
    const std::array<const char*, 6> notes{
        "roughness 0 / smooth",          "roughness 1 / porous",
        "colored cracks / fine normals", "same body / coating 0",
        "same body / coating 1",         "same color / changed normal"};
    Moment smooth, rough, crazed, raw, glazed, flat;
    smooth.roughness = 0;
    smooth.scalarRoughness = true;
    smooth.normal = .25f;
    rough.roughness = 1;
    rough.scalarRoughness = true;
    crazed.roughness = .2f;
    raw.roughness = .65f;
    raw.glaze = 0;
    glazed.roughness = .65f;
    flat.roughness = .2f;
    flat.normal = 0;
    const std::array<Moment, 6> controls{smooth, rough,  crazed,
                                         raw,    glazed, flat};
    for (size_t i = 0; i < controls.size(); ++i) {
      const float x = float(i) * 223;
      const CeramicMaps& maps =
          i == 0 ? porcelain
                 : (i == 1 || i == 3 || i == 4 ? stoneware : crazing);
      const auto color =
          i == 0 ? ink(0xe9e4d1)
                 : (i == 1 || i == 3 || i == 4 ? ink(0xb8a68a) : ink(0x84b6a6));
      out.children({label("0" + std::to_string(i + 1) + " / " + titles[i], x, 0,
                          213, 10, ink(0x354f53), 550)});
      if (i == 5) {
        auto high = controls[i];
        high.normal = 3;
        out.children({tile(x, 25, 97, 94, maps, color, controls[i],
                           {.radius = 16, .shoulder = 8, .waves = 1}),
                      tile(x + 101, 25, 97, 94, maps, color, high,
                           {.radius = 16, .shoulder = 8, .waves = 1})});
      } else {
        out.children({tile(
            x, 25, 198, 94, maps, color, controls[i],
            {.radius = 16, .shoulder = 8, .waves = i == 2 ? 1.0f : 0.0f})});
      }
      out.children({label(notes[i], x, 128, 211, 9, ink(0x6a7d72))});
    }
    return out;
  }

  compose::Element describe(int phase) const {
    const Moment m = moment(phase);
    const auto dark = ink(0x2c464b), muted = ink(0x758278);
    return compose::positioned()
        .width(kWidth)
        .height(kHeight)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .children(
            {label("KILN / 04", 52, 38, 785, 57, dark, 500),
             label("CERAMIC GLAZE / A STUDY OF FORMED COLOR AND LIGHT", 55, 118,
                   970, 12, muted, 550),
             label("SPECIMEN LABORATORY", 1060, 43, 325, 11, dark, 600),
             label("PORCELAIN / STONEWARE / GLAZE", 1060, 68, 325, 10, muted),
             label("Compose fills + material inks", 1060, 93, 325, 10, muted),
             rule(55, 156, 1330, ink(0xafb9a7)),
             label(m.name, 55, 178, 980, 20, ink(0x317f6f), 600),
             label(m.note, 55, 208, 1015, 12, muted), bench(m), parameters(m),
             label("FIXED FINISH COUPONS", 55, 777, 690, 13, dark, 600),
             label("ENDPOINTS / ONE SHARED STUDIO", 1027, 779, 364, 10, muted),
             specimens(),
             label(
                 "36 s LOOP   /   02 GLAZED · 06 R0 · 10 R1 · 14 N0 · 18 N3 · "
                 "22 COAT0 · 26 RAKE · 30 UNLIT · 34 ZERO HEIGHT",
                 55, 975, 1338, 10, muted)});
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.oversample(1);
    ctx.captureAt(2);
    ctx.background(ink(0xf1eddf));
    const float porosity = bounded(kControls.porosity, 0, 1, .75f);
    porcelain = finishMaps(41, .035f, false);
    stoneware = finishMaps(107, porosity, false);
    crazing = finishMaps(317, porosity * .21f, true);
    environment = studio();
    ctx.composer.render(describe(0));
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    const double seconds = std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0;
    const float t =
        static_cast<float>(std::fmod(seconds, double(kLoopSeconds)));
    bearing = 118 + 43 * std::sin(t * 2 * kPi / kLoopSeconds);
    reflectionTurn = t * bounded(kControls.lightSpeed, -60, 60, 10);
    const int next = std::min(8, static_cast<int>(t / 4));
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(next));
    }
  }
};

}  // namespace

SIGIL_SKETCH(CeramicGlaze, "Study · Materials",
             "Crazed celadon, porcelain and stoneware material fills and inks "
             "under a moving studio with fixed finish comparisons");
