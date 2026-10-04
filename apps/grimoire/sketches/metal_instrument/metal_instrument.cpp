// A machined audio instrument: real mesh depth, moving studio reflections,
// and Compose-authored markings transported as ordinary material textures.
// TAGS: Materials/Metals, Materials/Lighting, Geometry/3D, Compose/Depth

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/set/Set.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
namespace path = sigil::geometry::path;

namespace {

constexpr int kWidth = 1440, kHeight = 960;
constexpr int kFaceWidth = 1600, kFaceHeight = 500;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kBodyWidth = 900, kBodyHeight = 300, kBodyDepth = 120;
constexpr float kPanelWidth = 870, kPanelHeight = 276;
constexpr float kLoopSeconds = 24;

// Source controls. Geometry is built once; depth scales its z axis. Zero
// depth becomes a thin finite plate instead of a singular normal transform.
struct Controls {
  float yaw = -22;
  float pitch = 4;
  float depth = 120;
  float brushedRoughness = 0.27f;
  float polishedRoughness = 0.035f;
  float blastedRoughness = 0.82f;
  float normalStrength = 0.55f;
  float lightSpeed = 8;
  float perspective = 760;
  float nearPlane = 8;
  float farPlane = 5000;
  float glassTransmission = 0.18f;
};
constexpr Controls kControls;

float finite(float value, float fallback) {
  return std::isfinite(value) ? value : fallback;
}
float bounded(float value, float lo, float hi, float fallback) {
  return std::clamp(finite(value, fallback), lo, hi);
}
float radians(float degrees) { return degrees * kPi / 180; }
float smooth(float a, float b, float value) {
  const float x = std::clamp((value - a) / (b - a), 0.0f, 1.0f);
  return x * x * (3 - 2 * x);
}
float pulse(float t, float begin, float peak, float end) {
  return smooth(begin, peak, t) * (1 - smooth(peak, end, t));
}
material::Color ink(uint32_t rgb, float alpha = 1) {
  auto color = material::hexColor(rgb);
  color.a = alpha;
  return color;
}
std::string number(float value, int precision = 1) {
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision, value);
  return buffer.data();
}

struct Moment {
  float t = 0, yaw = 0, pitch = 0, depth = 120;
  float explode = 0, nearPlane = 8, composeYaw = 0;
  const char* name = "FACING";
};

Moment moment(double seconds) {
  const double safeSeconds = std::isfinite(seconds) ? seconds : 0;
  const float t = static_cast<float>(
      std::fmod(std::max(0.0, safeSeconds), double(kLoopSeconds)));
  const float hero = smooth(0, 2.4f, t) * (1 - smooth(19, 24, t));
  Moment m;
  m.t = t;
  m.yaw = bounded(kControls.yaw, -180, 180, -22) * hero;
  m.pitch = bounded(kControls.pitch, -85, 85, 4) * hero;
  m.depth = bounded(kControls.depth, 0.5f, 240, 120);
  m.nearPlane = bounded(kControls.nearPlane, 0.1f, 1700, 8);
  m.yaw += (77 - m.yaw) * pulse(t, 4, 6, 7.4f);
  m.explode = pulse(t, 7.4f, 9, 10.8f);
  m.nearPlane += (1390 - m.nearPlane) * pulse(t, 10.8f, 12, 13.2f);
  const float reverse = pulse(t, 13.2f, 15, 16.8f);
  m.yaw += (180 - m.yaw) * reverse;
  const float thin = pulse(t, 16.8f, 18, 19.8f);
  m.depth += (0.5f - m.depth) * thin;
  // Tighten the depth range while layers are compressed. Moving the near
  // plane forward preserves their separation in a float depth map.
  m.nearPlane = std::max(m.nearPlane, 200 * thin);
  m.composeYaw = m.yaw;
  if (t < 1)
    m.name = "FACING";
  else if (t < 4)
    m.name = "MACHINED / HERO";
  else if (t < 7.4f)
    m.name = "GRAZING / SCALAR ROUGHNESS";
  else if (t < 10.8f)
    m.name = "EXPLODED / REAL DEPTH";
  else if (t < 13.2f)
    m.name = "NEAR PLANE / CLIPPED GEOMETRY";
  else if (t < 16.8f)
    m.name = "REVERSE / HIDDEN BACKFACE";
  else if (t < 19.8f)
    m.name = "ZERO DEPTH / FINITE PLATE";
  else
    m.name = "RETURN / MOVING LIGHT";
  return m;
}

compose::Element rect(float x, float y, float w, float h,
                      material::Color color) {
  return compose::box().left(x).top(y).width(w).height(h).fill(color);
}
compose::Element label(std::string text, float x, float y, float w, float size,
                       material::Color color) {
  return compose::text(std::move(text))
      .left(x)
      .top(y)
      .width(w)
      .height(size * 1.65f)
      .font({.size = size, .color = color});
}

struct BrushMaps {
  material::Texture color, normal, roughness;
};

// Directional finish marks and shallow tangent-space grooves. The shared
// surface program has scalar roughness, so this is not anisotropic optics.
BrushMaps brushMaps() {
  std::array<SkBitmap, 3> maps;
  for (auto& map : maps) map.allocN32Pixels(kFaceWidth, kFaceHeight);
  const auto channel = [](float value) {
    return static_cast<U8CPU>(255 * std::clamp(value, 0.0f, 1.0f));
  };
  for (int y = 0; y < kFaceHeight; ++y) {
    const float fine =
        0.62f * std::sin(float(y) * 0.93f) + 0.38f * std::sin(float(y) * 2.71f);
    const float groove = 0.045f * std::cos(float(y) * 0.93f) +
                         0.015f * std::cos(float(y) * 2.71f);
    for (int x = 0; x < kFaceWidth; ++x) {
      const float grain =
          0.029f * fine + 0.004f * std::sin(float(x) * 0.17f + y * 1.31f);
      *maps[0].getAddr32(x, y) =
          SkPreMultiplyARGB(255, channel(0.79f + grain), channel(0.81f + grain),
                            channel(0.83f + grain));
      *maps[1].getAddr32(x, y) = SkPreMultiplyARGB(
          255, channel(0.5f), channel(0.5f + groove), channel(1));
      const float roughness = bounded(kControls.brushedRoughness, 0, 1, 0.27f);
      const U8CPU r = channel(roughness + 0.045f * fine);
      *maps[2].getAddr32(x, y) = SkPreMultiplyARGB(255, r, r, r);
    }
  }
  BrushMaps out;
  out.color = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(maps[0])));
  out.normal = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(maps[1])));
  out.roughness = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(maps[2])));
  return out;
}

material::EnvironmentMap studio() {
  return material::EnvironmentMap::baked(512, [](float u, float v) {
    const auto strip = [u, v](float center, float width, float elevation,
                              float height) {
      const float du = std::min(std::abs(u - center), 1 - std::abs(u - center));
      const float x = du / width, y = (v - elevation) / height;
      return std::exp(-0.5f * (x * x + y * y));
    };
    const float ceiling = std::pow(1 - v, 1.7f);
    glm::vec3 result{0.30f + ceiling * 0.85f, 0.33f + ceiling * 0.87f,
                     0.37f + ceiling * 0.92f};
    result += glm::vec3{6.8f, 6.5f, 6.0f} * strip(0.16f, 0.06f, 0.49f, 0.23f);
    result += glm::vec3{1.2f, 1.55f, 2.1f} * strip(0.74f, 0.07f, 0.40f, 0.12f);
    result += glm::vec3{3.6f, 3.8f, 4.0f} * strip(0.01f, 0.02f, 0.44f, 0.29f);
    return result;
  });
}

material::Material metal(material::Color color, float roughness) {
  return material::surface::program(
      {.baseColor = color,
       .metallic = 1,
       .roughness = bounded(roughness, 0, 1, 0.27f)});
}
material::Material screen(material::Texture texture) {
  auto result = material::surface::unlit({.baseColor = {1, 1, 1, 1}});
  result.slot(material::surface::kBaseColorSlot, std::move(texture));
  return result;
}
material::Material brushed(const BrushMaps& maps, material::Texture face = {}) {
  // The map carries absolute roughness; the scalar multiplies it.
  auto result = metal({1, 1, 1, 1}, 1);
  result.slot(material::surface::kBaseColorSlot,
              face.valid() ? face : maps.color);
  result.slot(material::surface::kNormalSlot, maps.normal);
  result.slot(material::surface::kRoughnessSlot, maps.roughness);
  result.set("normalScale", bounded(kControls.normalStrength, 0, 2, 0.55f));
  return result;
}

// Clockwise when viewed from +z; the uncapped loft's walls face outwards.
std::vector<glm::vec3> ring(float w, float h, float corner, float z) {
  w *= 0.5f;
  h *= 0.5f;
  return {{-w + corner, -h, z}, {-w, -h + corner, z}, {-w, h - corner, z},
          {-w + corner, h, z},  {w - corner, h, z},   {w, h - corner, z},
          {w, -h + corner, z},  {w - corner, -h, z}};
}
gm::Mesh cap(float w, float h, float corner) {
  path::Polyline contour;
  contour.closed = true;
  for (const auto p : ring(w, h, corner, 0))
    contour.points.emplace_back(p.x, p.y);
  return gm::fill(path::toPath(contour));
}
world::Element part(std::string key, gm::Mesh mesh, material::Material finish,
                    glm::vec3 at = {0, 0, 0}) {
  return world::Element()
      .key(key)
      .mesh(std::move(mesh))
      .fill(std::move(finish))
      .at(at);
}

world::Element knob(float radius, const material::Material& bright,
                    const material::Material& dark) {
  const auto shell = gm::revolve({{0, -4},
                                  {radius - 3, -4},
                                  {radius, 0},
                                  {radius, 19},
                                  {radius - 4, 24},
                                  {0, 24}},
                                 {.segments = 96});
  world::Element result;
  result.children(
      {part("lathed", shell, bright).rotateX(90),
       part("cap",
            gm::revolve({{0, 0}, {radius - 7, 0}, {radius - 7, 1}, {0, 1}},
                        {.segments = 96}),
            dark, {0, 0, 24.2f})
           .rotateX(90),
       part("index",
            gm::box({-0.9f, radius - 20, 25.3f}, {0.9f, radius - 10, 26}),
            material::surface::unlit({.baseColor = ink(0xd5e8cb)}))});
  for (int i = 0; i < 64; ++i) {
    const float a = 2 * kPi * float(i) / 64;
    result.children({part("knurl-" + std::to_string(i),
                          gm::box({-0.7f, -1.0f, 0}, {0.7f, 1.0f, 15}), bright,
                          {radius * std::cos(a), radius * std::sin(a), 1.5f})
                         .rotateZ(float(i) * 360 / 64)});
  }
  return result;
}

world::Element screw(const material::Material& steel) {
  return world::Element().children(
      {part("head",
            gm::revolve({{0, -1},
                         {3.5f, -1},
                         {5.5f, 0},
                         {5.5f, 1.8f},
                         {4.6f, 2.8f},
                         {0, 2.8f}},
                        {.segments = 32}),
            steel)
           .rotateX(90),
       part("slot", gm::box({-3.8f, -0.55f, 2.85f}, {3.8f, 0.55f, 3.05f}),
            material::surface::program(
                {.baseColor = ink(0x182126), .roughness = 0.7f}))});
}

compose::Element faceGraphic(const BrushMaps& maps) {
  auto face = compose::positioned().width(kFaceWidth).height(kFaceHeight);
  const auto dark = ink(0x283039);
  face.children(
      {compose::image(maps.color.source())
           .left(0)
           .top(0)
           .width(kFaceWidth)
           .height(kFaceHeight),
       label("TONE", 76, 40, 260, 56, dark).fontWeight(650),
       label("03 / PRECISION MONITOR", 360, 62, 640, 24, dark),
       label("STEREO  ·  CLASS A  ·  LINE / PHONO", 76, 410, 710, 20, dark),
       label("LEVEL", 1000, 66, 180, 22, dark),
       label("BALANCE", 1240, 118, 170, 19, dark),
       label("SOURCE", 1455, 142, 140, 18, dark),
       label("−48", 923, 382, 80, 18, dark),
       label("0 dB", 1120, 382, 80, 18, dark),
       label("L", 1208, 352, 45, 18, dark), label("R", 1365, 352, 45, 18, dark),
       label("I", 1430, 337, 35, 18, dark),
       label("II", 1512, 337, 40, 18, dark),
       rect(75, 377, 720, 1.5f, ink(0x657078, 0.6f)),
       label("REFERENCE   1 kHz / 0.775 V", 77, 344, 600, 19, dark),
       rect(804, 58, 1.2f, 360, ink(0x626d75, 0.35f))});
  const auto ticks = [&](float cx, float cy, float r, int count, float spread) {
    std::vector<compose::Element> result;
    for (int i = 0; i <= count; ++i) {
      const float degrees = -spread * 0.5f + spread * float(i) / float(count);
      const float angle = radians(degrees - 90);
      const float length = i % 5 == 0 ? 13.0f : 6.0f;
      result.push_back(rect(cx + std::cos(angle) * r - 0.7f,
                            cy + std::sin(angle) * r - length * 0.5f, 1.4f,
                            length, dark)
                           .rotate(degrees));
    }
    return result;
  };
  face.children(ticks(1067, 304, 128, 40, 265));
  face.children(ticks(1324, 322, 80, 10, 220));
  face.children(ticks(1508, 322, 61, 4, 150));
  return face;
}

compose::Element displayGraphic(float seconds) {
  auto result =
      compose::positioned().width(600).height(210).fill(ink(0x0a1418));
  const auto amber = ink(0xc8dd96), faint = ink(0x536a59);
  result.children({label("MONITOR", 25, 16, 225, 20, amber),
                   label("−18.4", 354, 8, 220, 56, amber),
                   label("dB", 514, 74, 58, 17, amber),
                   label("L", 23, 88, 24, 17, amber),
                   label("R", 23, 128, 24, 17, amber),
                   label("LINE 01", 25, 177, 160, 16, faint),
                   label("STEREO / DIRECT", 359, 177, 230, 16, faint)});
  for (int row = 0; row < 2; ++row) {
    const int lit =
        17 + static_cast<int>(5 * std::sin(seconds * 1.7f + row * 0.7f));
    for (int i = 0; i < 30; ++i) {
      const auto color =
          i < lit ? (i > 24 ? ink(0xd99864) : amber) : ink(0x21322a);
      result.children({rect(60 + i * 17.1f, 91 + row * 40, 13, 17, color)});
    }
  }
  return result;
}

struct MetalInstrument {
  std::shared_ptr<compose::TextureScene> faceScene, displayScene, overlayScene;
  BrushMaps maps;
  material::EnvironmentMap environment;
  material::Material polished{ink(0xadb8c2)}, dark{ink(0x263139)},
      blasted{ink(0x99a1a8)};
  world::Element enclosure, bezel, screws, mainKnob, balanceKnob, sourceKnob;
  std::array<world::Element, 3> coupons;
  gm::Mesh panel, displayPlane, glassPlane;

  void setup(sketch::SetContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(2.4);
    ctx.background(ink(0x11191e));
    ctx.camera(lens(moment(0)));
    maps = brushMaps();
    environment = studio();
    polished = metal({0.83f, 0.86f, 0.89f, 1}, kControls.polishedRoughness);
    dark = metal(ink(0x263139), 0.36f);
    blasted = metal({0.60f, 0.63f, 0.66f, 1}, kControls.blastedRoughness);
    faceScene = ctx.textureScene({kFaceWidth, kFaceHeight});
    displayScene = ctx.textureScene({600, 210});
    overlayScene = ctx.textureScene({kWidth, kHeight});
    faceScene->render(faceGraphic(maps), 0);
    panel = cap(kPanelWidth, kPanelHeight, 8);
    displayPlane = gm::quad(282, 99);
    glassPlane = gm::quad(286, 103);
    const auto walls =
        gm::loft({ring(886, 286, 10, -60), ring(900, 300, 14, -53),
                  ring(900, 300, 14, 53)});
    const auto frontBevel = gm::loft(
        {ring(900, 300, 14, 53), ring(880, 280, 10, 63)}, {.capEnds = false});
    enclosure = world::Element()
                    .key("enclosure")
                    .children({part("walls", walls, brushed(maps)),
                               part("polished-chamfer", frontBevel, polished),
                               part("panel-seat", panel, dark, {0, 0, 62})});
    // Raised vent strips expose a stepped silhouette under a grazing camera.
    // The enclosure underneath stays closed; these are not through-holes.
    for (int i = 0; i < 26; ++i) {
      enclosure.children({part("vent-" + std::to_string(i),
                               gm::box({-3, 149, -40}, {3, 150.6f, 29}), dark,
                               {-260 + i * 20.0f, 0, 0})});
    }
    for (int side : {-1, 1}) {
      enclosure.children({part("rail-" + std::to_string(side),
                               gm::box({-3, -137, -49}, {3, 137, 47}), polished,
                               {side * 451.0f, 0, 0})});
      for (int rear : {-1, 1}) {
        enclosure.children({part(
            "foot-" + std::to_string(side) + "-" + std::to_string(rear),
            gm::revolve({{0, -9}, {20, -9}, {23, -6}, {23, 4}, {19, 7}, {0, 7}},
                        {.segments = 48}),
            material::surface::program(
                {.baseColor = ink(0x172025), .roughness = 0.92f}),
            {side * 350.0f, -155, rear * 37.0f})});
      }
    }
    bezel = part("display-bezel",
                 gm::loft({ring(312, 123, 7, -4), ring(286, 103, 4, 7)},
                          {.capEnds = false}),
                 polished);
    for (int x : {-1, 1})
      for (int y : {-1, 1})
        screws.children(
            {screw(polished)
                 .key("screw-" + std::to_string(x) + "-" + std::to_string(y))
                 .at({x * 419.0f, y * 122.0f, 0})
                 .rotateZ(float(x * y) * 28)});
    mainKnob = knob(54, polished, brushed(maps));
    balanceKnob = knob(32, polished, metal({0.68f, 0.72f, 0.77f, 1}, 0.14f));
    sourceKnob = knob(26, polished, blasted);
    const auto coupon =
        gm::revolve({{0, -3}, {48, -3}, {53, 1}, {53, 9}, {48, 14}, {0, 14}},
                    {.segments = 96});
    coupons = {
        part("coupon-polished", coupon, metal({0.79f, 0.82f, 0.85f, 1}, 0)),
        part("coupon-brushed", coupon, brushed(maps)),
        part("coupon-blasted", coupon, metal({0.79f, 0.82f, 0.85f, 1}, 1))};
  }

  gm::camera::Camera lens(const Moment& m) const {
    gm::camera::Camera camera;
    camera.eye = {110, 365, 1450};
    camera.target = {0, 50, 0};
    camera.fovYDeg = 40;
    camera.zNear = m.nearPlane;
    camera.zFar = std::max(camera.zNear + 100,
                           bounded(kControls.farPlane, 100, 12000, 5000));
    return camera;
  }

  compose::Element overlay(const Moment& m) const {
    const auto white = ink(0xe1e6df), muted = ink(0x8a9a9d),
               lime = ink(0xc8dd96);
    auto root = compose::positioned().width(kWidth).height(kHeight);
    root.children({label("TONE / 03", 62, 42, 450, 44, white).fontWeight(600),
                   label("MATERIAL INSTRUMENT", 64, 103, 450, 15, muted),
                   label("STUDY  07     /     MACHINED ALUMINUM", 850, 51, 520,
                         17, muted),
                   rect(63, 147, 1314, 1, ink(0x56666a, 0.6f)),
                   label(m.name, 63, 165, 900, 16, lime),
                   label("YAW " + number(m.yaw, 0) + "°   PITCH " +
                             number(m.pitch, 0) + "°   DEPTH " +
                             number(m.depth) + "   NEAR " + number(m.nearPlane),
                         840, 165, 550, 14, muted),
                   label("REAL MESH / MOVING WORLD LIGHT + REFLECTION", 64, 615,
                         900, 14, muted),
                   rect(63, 655, 1314, 1, ink(0x56666a, 0.6f)),
                   label("FINISH COUPONS", 64, 682, 370, 20, white),
                   label("Same aluminum. Roughness 0 / textured / 1.", 64, 717,
                         425, 15, muted),
                   label("POLISHED", 496, 805, 180, 14, white),
                   label("BRUSHED", 700, 805, 180, 14, white),
                   label("BLASTED", 903, 805, 180, 14, white),
                   label("COMPOSE PLANE", 1110, 683, 250, 18, white),
                   label("Projection rotates; lighting remains page-normal.",
                         1095, 861, 295, 14, muted),
                   label("24 s LOOP    /    02.4 HERO · 06 GRAZE · 09 DEPTH · "
                         "12 CLIP · 15 REVERSE · 18 THIN",
                         64, 903, 1300, 14, muted)});

    // This is actual Compose perspective. Its page-space lighting is an
    // intentional comparison with the scene's normal-aware mesh lighting.
    material::Material planePaint = material::image(maps.color.source());
    planePaint.surface(
        {.metallic = 1.0f,
         .roughness = bounded(kControls.brushedRoughness, 0, 1, 0.27f),
         .normal = material::image(maps.normal.source()),
         .normalScale = bounded(kControls.normalStrength, 0, 2, 0.55f)});
    const float perspective = bounded(kControls.perspective, 0, 3000, 760);
    const auto plane =
        compose::positioned()
            .left(1105)
            .top(746)
            .width(245)
            .height(104)
            .perspective(perspective)
            .preserve3d()
            .lighting(material::Lighting{
                material::studio({.direction = 110,
                                  .elevation = 65,
                                  .intensity = 0.8f,
                                  .ambient = 0.2f}),
                material::environment(
                    environment.texture().source(),
                    {.rotation =
                         m.t * bounded(kControls.lightSpeed, -60, 60, 8),
                     .intensity = 0.9f})})
            .children(
                {compose::positioned()
                     .key("depth-plane")
                     .left(0)
                     .top(0)
                     .width(235)
                     .height(86)
                     .fill(planePaint)
                     .rotateY(m.composeYaw)
                     .rotateX(-m.pitch)
                     .translateZ(m.explode * 32)
                     .backface(material::Backface::Hidden)
                     .borderRadius(5)
                     .children({label("TONE", 15, 11, 190, 22, ink(0x283039)),
                                rect(15, 54, 100, 12, ink(0x132026)),
                                rect(150, 42, 35, 35, ink(0x29343b))
                                    .borderRadius(30)})});
    root.children({plane});
    return root;
  }

  world::Element overlayQuad(const gm::camera::Camera& camera) const {
    const glm::vec3 forward = glm::normalize(camera.target - camera.eye);
    const float distance = camera.zNear + 4;
    const glm::vec2 extent = camera.extentAt(distance, float(kWidth) / kHeight);
    const glm::vec3 at = camera.eye + forward * distance;
    return part("overlay", gm::quad(extent.x, extent.y),
                screen(overlayScene->texture()))
        .transform(gm::camera::faceCamera(camera.eye, at, camera.up));
  }

  world::Frame describe(double seconds) {
    const Moment m = moment(seconds);
    const double sceneTime =
        std::isfinite(seconds) ? std::max(0.0, seconds) : 0;
    const auto camera = lens(m);
    displayScene->render(displayGraphic(m.t), sceneTime);
    overlayScene->render(overlay(m), sceneTime);
    const float front = 65 + 135 * m.explode;
    auto assembly =
        world::Element()
            .key("instrument")
            .at({-50, 110, 0})
            .rotateY(m.yaw)
            .rotateX(m.pitch)
            .scaleZ(m.depth / kBodyDepth)
            .children(
                {enclosure,
                 part("engraved-face", panel,
                      brushed(maps, faceScene->texture()), {0, 0, front}),
                 screws.at({0, 0, front}), bezel.at({-253, 18, front + 2}),
                 part("display", displayPlane, screen(displayScene->texture()),
                      {-253, 18, front + 7}),
                 mainKnob.at({145, -30, front + 7})
                     .rotateZ(-44 + 10 * std::sin(m.t * 0.45f)),
                 balanceKnob.at({285, -40, front + 7})
                     .rotateZ(6 * std::sin(m.t * 0.6f)),
                 sourceKnob.at({385, -40, front + 7}).rotateZ(35)});
    auto glass = material::surface::program(
        {.baseColor = {0.72f, 0.84f, 0.9f, 0.15f},
         .roughness = 0.085f,
         .transmission = bounded(kControls.glassTransmission, 0, 1, 0.18f),
         .ior = 1.5f,
         .thickness = 1.8f,
         .reflectionWeight = 0.18f});
    assembly.children(
        {part("display-glass", glassPlane, glass, {-253, 18, front + 10})});
    world::Element scene;
    scene.camera(camera);
    const float lightTurn = m.t * bounded(kControls.lightSpeed, -60, 60, 8);
    scene.children({world::Element()
                        .key("studio")
                        .environmentMap({.map = environment,
                                         .intensity = 0.95f,
                                         .diffuse = 0.45f,
                                         .specular = 1,
                                         .exposure = 1.05f})
                        .rotateY(lightTurn),
                    world::Element().key("key").light(world::light::sun(
                        {0.9f * std::cos(radians(lightTurn)) -
                             0.38f * std::sin(radians(lightTurn)),
                         0.3f,
                         -0.9f * std::sin(radians(lightTurn)) -
                             0.38f * std::cos(radians(lightTurn))},
                        {1, 0.93f, 0.81f, 1}, 2.2f)),
                    world::Element().key("fill").light(world::light::sun(
                        {0.75f, -0.2f, -0.5f}, {0.62f, 0.79f, 1, 1}, 0.55f)),
                    assembly,
                    part("shelf", gm::box({-250, -274, 95}, {330, -268, 150}),
                         material::surface::program({.baseColor = ink(0x182126),
                                                     .roughness = 0.95f}))});
    for (size_t i = 0; i < coupons.size(); ++i)
      scene.children({coupons[i]
                          .at({-175 + float(i) * 208, -218, 115})
                          .rotateX(68)
                          .rotateY(-15 + 8 * std::sin(m.t * 0.4f))});
    scene.children({overlayQuad(camera)});
    return world::Frame(scene).extent({kWidth, kHeight});
  }
};

}  // namespace

SIGIL_SKETCH(MetalInstrument, "Study · Materials",
             "Machined aluminum audio controls, real depth, moving light and a "
             "Compose plane comparison");
