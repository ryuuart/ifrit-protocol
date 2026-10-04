// An invented die-struck control register: raised type, pierced brass,
// enamel-filled recesses and turned controls are separate physical solids.
// TAGS: Materials/Metals, Materials/Lighting, Geometry/3D, Typography/Relief

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/set/Set.h>
#include <sigilweave/choreograph/PlacedGlyph.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/layout/TextContext.h>
#include <sigilworld/element/Element.h>
#include <sigilworld/frame/Frame.h>
#include <sigilworld/light/Light.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace compose = sigil::compose;
namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;
namespace path = sigil::geometry::path;

namespace {

constexpr int kWidth = 1440, kHeight = 960;
constexpr float kPi = 3.14159265358979323846f, kLoop = 24;
constexpr float kFace = 28, kLetterDepth = 6;

struct Controls {
  float yaw = -16, pitch = 5;
  float relief = 6;
  float brassRoughness = 0.24f, bronzeRoughness = 0.52f;
  float patina = 0.42f, normalStrength = 0.7f;
  float lightSwing = 36, registerTravel = 20;
  float nearPlane = 40, farPlane = 3500;
};
constexpr Controls kControls;

float bounded(float v, float lo, float hi, float fallback) {
  return std::clamp(std::isfinite(v) ? v : fallback, lo, hi);
}
float radians(float v) { return v * kPi / 180; }
float smooth(float a, float b, float v) {
  const float x = std::clamp((v - a) / (b - a), 0.0f, 1.0f);
  return x * x * (3 - 2 * x);
}
float pulse(float t, float a, float b, float c) {
  return smooth(a, b, t) * (1 - smooth(b, c, t));
}
material::Color ink(uint32_t value, float alpha = 1) {
  auto c = material::hexColor(value);
  c.a = alpha;
  return c;
}
std::string decimal(float v, int places = 1) {
  std::array<char, 32> buf{};
  std::snprintf(buf.data(), buf.size(), "%.*f", places, v);
  return buf.data();
}

struct Moment {
  float t = 0, wave = 0, yaw = 0, pitch = 0, relief = 6;
  float roughness = 0.24f;
  const char* name = "FRONT / RELIEF AND COUNTERS";
};
Moment moment(double seconds) {
  const double safe = std::isfinite(seconds) ? std::max(0.0, seconds) : 0;
  Moment m;
  m.t = float(std::fmod(safe, double(kLoop)));
  m.wave = 2 * kPi * m.t / kLoop;
  const float hero = smooth(0, 2.4f, m.t) * (1 - smooth(22.5f, 24, m.t));
  m.yaw = bounded(kControls.yaw, -180, 180, -16) * hero;
  m.pitch = bounded(kControls.pitch, -75, 75, 5) * hero;
  m.yaw += (70 - m.yaw) * pulse(m.t, 4, 6, 7.5f);
  m.yaw += (180 - m.yaw) * pulse(m.t, 19.5f, 21, 22.5f);
  m.relief = bounded(kControls.relief, 0.8f, 18, 6);
  m.relief += (0.8f - m.relief) * pulse(m.t, 13.5f, 15, 16.5f);
  m.relief += (16 - m.relief) * pulse(m.t, 16.5f, 18, 19.5f);
  m.roughness = bounded(kControls.brassRoughness, 0, 1, 0.24f);
  m.roughness *= 1 - pulse(m.t, 7.5f, 9, 10.5f);
  m.roughness += (1 - m.roughness) * pulse(m.t, 10.5f, 12, 13.5f);
  if (m.t >= 1 && m.t < 4) m.name = "MINT / MACHINED HERO";
  if (m.t >= 4 && m.t < 7.5f) m.name = "GRAZE / REAL SIDE WALLS";
  if (m.t >= 7.5f && m.t < 10.5f) m.name = "POLISHED / ROUGHNESS ZERO";
  if (m.t >= 10.5f && m.t < 13.5f) m.name = "BLASTED / ROUGHNESS ONE";
  if (m.t >= 13.5f && m.t < 16.5f) m.name = "SHALLOW / 0.8 UNIT RELIEF";
  if (m.t >= 16.5f && m.t < 19.5f) m.name = "DEEP / 16 UNIT RELIEF";
  if (m.t >= 19.5f && m.t < 22.5f) m.name = "REVERSE / PIERCED SILHOUETTE";
  if (m.t >= 22.5f) m.name = "RETURN / PERIODIC LIGHT";
  return m;
}

path::Outline roundRect(float w, float h, float radius = 8) {
  return path::fromSk(
      SkPathBuilder()
          .addRRect(SkRRect::MakeRectXY(SkRect::MakeXYWH(-w / 2, -h / 2, w, h),
                                        radius, radius))
          .detach());
}
path::Outline circle(float r) {
  return path::fromSk(SkPathBuilder().addCircle({0, 0}, r).detach());
}
path::Outline placed(const path::Outline& outline, float x, float y,
                     float degrees = 0) {
  SkMatrix matrix = SkMatrix::RotateDeg(degrees);
  matrix.postTranslate(x, y);
  return path::fromSk(
      SkPathBuilder().addPath(path::toSk(outline), matrix).detach());
}

// Weave supplies shaped glyph IDs and placement, and Skia exposes each
// glyph's vector outline. The combined outline keeps its counters for
// the geometry extrusion; raster text would lose the physical side walls.
path::Outline lettering(weave::TextContext& text, const weave::Face& face,
                        std::u8string_view words, float size,
                        float tracking = 0) {
  weave::TextStyle style;
  style.shaping.typeface = face;
  style.shaping.fontSize = size;
  style.shaping.letterSpacing = tracking;
  const auto line = text.singleLine(words, style, {0, 0});
  SkPathBuilder builder;
  weave::forEachPlacedGlyph(
      line.layout(), line.paragraph(), [&](const weave::PlacedGlyph& glyph) {
        const auto& word = *glyph.shaped;
        const SkFont font =
            weave::makeFont(word.typeface, word.fontSize, word.scaleX);
        if (auto outline = font.getPath(glyph.glyph))
          builder.addPath(*outline,
                          SkMatrix::Translate(glyph.rest.x, glyph.rest.y));
      });
  const SkPath outline = builder.detach();
  const SkRect bounds = outline.computeTightBounds();
  return path::fromSk(
      SkPathBuilder()
          .addPath(outline,
                   SkMatrix::Translate(-bounds.centerX(), -bounds.centerY()))
          .detach());
}

world::Element part(std::string_view key, const gm::Mesh& mesh,
                    const material::Material& finish,
                    glm::vec3 at = {0, 0, 0}) {
  return world::Element().key(key).mesh(mesh).fill(finish).at(at);
}
material::Material metal(material::Color c, float roughness) {
  return material::surface::program(
      {.baseColor = c, .metallic = 1, .roughness = roughness});
}

struct FinishMaps {
  material::Texture color, normal, roughness;
};
FinishMaps bronzeMaps() {
  constexpr int width = 640, height = 256;
  std::array<SkBitmap, 3> pixels;
  for (auto& map : pixels) map.allocN32Pixels(width, height);
  const auto channel = [](float v) {
    return static_cast<U8CPU>(255 * std::clamp(v, 0.0f, 1.0f));
  };
  const float patina = bounded(kControls.patina, 0, 1, 0.42f);
  const float rough = bounded(kControls.bronzeRoughness, 0, 1, 0.52f);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const float u = float(x) / width, v = float(y) / height;
      const float stain =
          smooth(0.18f, 0.88f,
                 0.5f + 0.22f * std::sin(u * 29 + std::sin(v * 17)) +
                     0.19f * std::cos(v * 21 - u * 8) +
                     0.1f * std::sin(u * 83 + v * 51));
      const float aged = stain * patina;
      const float tool = std::sin(float(y) * 1.6f + 0.4f * std::sin(x * 0.07f));
      const float pits =
          std::pow(0.5f + 0.5f * std::sin(x * 0.81f) * std::cos(y * 0.97f), 10);
      const glm::vec3 warm{0.56f, 0.36f, 0.16f}, green{0.08f, 0.22f, 0.17f};
      const glm::vec3 c = glm::mix(warm, green, aged) +
                          glm::vec3{0.012f * tool - 0.045f * pits};
      *pixels[0].getAddr32(x, y) =
          SkPreMultiplyARGB(255, channel(c.x), channel(c.y), channel(c.z));
      const glm::vec3 normal = glm::normalize(
          glm::vec3{0.13f * std::cos(x * 0.81f) * std::cos(y * 0.97f),
                    0.07f * std::cos(y * 1.6f) -
                        0.13f * std::sin(x * 0.81f) * std::sin(y * 0.97f),
                    1});
      *pixels[1].getAddr32(x, y) = SkPreMultiplyARGB(
          255, channel(normal.x * 0.5f + 0.5f), channel(normal.y * 0.5f + 0.5f),
          channel(normal.z * 0.5f + 0.5f));
      const U8CPU r = channel(rough + 0.32f * aged + 0.12f * pits);
      *pixels[2].getAddr32(x, y) = SkPreMultiplyARGB(255, r, r, r);
    }
  FinishMaps out;
  out.color = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(pixels[0])));
  out.normal = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(pixels[1])));
  out.roughness = material::Texture(
      media::PixelSource(SkImages::RasterFromBitmap(pixels[2])));
  return out;
}
material::EnvironmentMap studio() {
  return material::EnvironmentMap::baked(256, [](float u, float v) {
    const auto box = [u, v](float centre, float wide, float high) {
      const float dx =
          std::min(std::abs(u - centre), 1 - std::abs(u - centre)) / wide;
      const float dy = (v - 0.45f) / high;
      return std::exp(-0.5f * (dx * dx + dy * dy));
    };
    glm::vec3 c{0.23f + 0.68f * (1 - v), 0.25f + 0.7f * (1 - v),
                0.29f + 0.73f * (1 - v)};
    c += glm::vec3{6.2f, 5.5f, 4.1f} * box(0.16f, 0.055f, 0.2f);
    c += glm::vec3{2.0f, 2.5f, 3.0f} * box(0.72f, 0.045f, 0.28f);
    return c;
  });
}

world::Element turnControl(std::string_view key, float radius,
                           const material::Material& brass,
                           const material::Material& aluminum) {
  world::Element result;
  result.key(key);
  const auto shell = gm::revolve({{0, 0},
                                  {radius - 3, 0},
                                  {radius, 3},
                                  {radius, 22},
                                  {radius - 4, 27},
                                  {0, 27}},
                                 {.segments = 64});
  result.children(
      {part("turned-shell", shell, brass).rotateX(90),
       part("inset-cap", gm::extrude(circle(radius - 7), 2), aluminum,
            {0, 0, 28.5f}),
       part("index", gm::box({-1, radius - 19, 30}, {1, radius - 8, 31.2f}),
            material::surface::unlit({.baseColor = ink(0xd4dabc)}))});
  gm::Mesh ribs;
  for (int i = 0; i < 48; ++i) {
    const float a = 2 * kPi * i / 48;
    gm::Mesh rib = gm::box({-0.8f, -1.4f, 0}, {0.8f, 1.4f, 19});
    glm::mat4 transform = glm::translate(
        glm::mat4{1}, glm::vec3{radius * std::cos(a), radius * std::sin(a), 3});
    transform = glm::rotate(transform, a, glm::vec3{0, 0, 1});
    rib.transform(transform);
    ribs.append(rib);
  }
  result.children({part("milled-ribs", ribs, brass)});
  return result;
}

compose::Element label(std::string words, float x, float y, float w, float size,
                       material::Color color) {
  return compose::text(std::move(words))
      .left(x)
      .top(y)
      .width(w)
      .height(size * 1.8f)
      .font({.size = size, .color = color});
}
compose::Element rule(float x, float y, float w, material::Color color) {
  return compose::box().left(x).top(y).width(w).height(1).fill(color);
}

struct StruckMetal {
  std::shared_ptr<compose::TextureScene> hud;
  compose::Element staticHud;
  FinishMaps maps;
  material::EnvironmentMap environment;
  material::Material bronze{material::Color{0, 0, 0, 1}};
  material::Material brass{material::Color{0, 0, 0, 1}};
  material::Material aluminum{material::Color{0, 0, 0, 1}};
  material::Material enamel{material::Color{0, 0, 0, 1}};
  world::Element chassis, medallion, controls, registerStrip, coupons;
  world::Element piercedBody, raisedTitle, raisedNumber;

  gm::camera::Camera lens() const {
    gm::camera::Camera eye;
    eye.eye = {140, 410, 1550};
    eye.target = {0, 10, 0};
    eye.fovYDeg = 42;
    eye.zNear = bounded(kControls.nearPlane, 1, 1400, 40);
    eye.zFar =
        std::max(eye.zNear + 100, bounded(kControls.farPlane, 100, 7000, 3500));
    return eye;
  }

  void setup(sketch::SetContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(2.4);
    ctx.background(ink(0x171b19));
    ctx.camera(lens());
    hud = ctx.textureScene({kWidth, kHeight});
    maps = bronzeMaps();
    environment = studio();
    bronze = metal({1, 1, 1, 1}, 1);
    bronze.slot(material::surface::kBaseColorSlot, maps.color);
    bronze.slot(material::surface::kNormalSlot, maps.normal);
    bronze.slot(material::surface::kRoughnessSlot, maps.roughness);
    bronze.set("normalScale", bounded(kControls.normalStrength, 0, 2, 0.7f));
    brass = metal({0.80f, 0.59f, 0.24f, 1},
                  bounded(kControls.brassRoughness, 0, 1, 0.24f));
    aluminum = metal({0.73f, 0.78f, 0.80f, 1}, 0.2f);
    enamel = material::surface::program(
        {.baseColor = ink(0x12382e), .roughness = 0.14f});
    weave::TextContext text(ctx.fonts);
    auto sans = ctx.fonts.familyTypeface("Helvetica Neue", {.weight = 700});
    auto serif = ctx.fonts.familyTypeface("Baskerville", {.weight = 700});
    if (!sans) sans = ctx.fonts.defaultTypeface();
    if (!serif) serif = sans;

    const path::Outline outer = roundRect(1030, 390, 24);
    const path::Outline aperture = placed(circle(153), -270, 0);
    const path::Outline window = placed(roundRect(368, 73, 7), 225, 82);
    const path::Outline caseOutline =
        outer.subtracted(aperture).subtracted(window);
    const path::Outline recessWords = lettering(text, sans, u8"STRIKE", 45, 4);
    const path::Outline recess = placed(recessWords, 210, -115);
    const auto frontOutline = caseOutline.subtracted(recess);
    chassis.key("chassis").children(
        {part("cast-body", gm::extrude(caseOutline, 42), bronze),
         part("face-with-recesses", gm::extrude(frontOutline, 6), bronze,
              {0, 0, 25}),
         part("enamel-recess-floor", gm::extrude(recessWords, 1.6f), enamel,
              {210, 115, 24.5f}),
         part("bright-perimeter",
              gm::extrude(outer.subtracted(roundRect(1018, 378, 19)), 6), brass,
              {0, 0, 26}),
         part("register-window-rim",
              gm::extrude(
                  roundRect(378, 83, 11).subtracted(roundRect(368, 73, 7)), 3),
              aluminum, {225, -82, 29})});

    // The aperture continues through the cast body. The medallion's own
    // holes therefore reveal the background in both front and rear views.
    path::Outline pierced = circle(143);
    for (int i = 0; i < 12; ++i) {
      const float a = radians(float(i) * 30);
      pierced =
          pierced.subtracted(placed(roundRect(29, 65, 13), 100 * std::sin(a),
                                    -100 * std::cos(a), float(i) * 30));
    }
    piercedBody = part("pierced-brass", gm::extrude(pierced, 10), brass);
    medallion.key("pierced-medallion")
        .children({part("raised-rim",
                        gm::extrude(circle(140).subtracted(circle(135)), 3),
                        aluminum, {0, 0, 6.5f}),
                   part("hub-enamel", gm::extrude(circle(59), 2), enamel,
                        {0, 0, 6.5f}),
                   part("hub-border",
                        gm::extrude(circle(62).subtracted(circle(58)), 3),
                        brass, {0, 0, 8})});
    gm::Mesh ticks;
    for (int i = 0; i < 72; ++i) {
      const float a = 2 * kPi * i / 72;
      const float length = i % 6 == 0 ? 9 : 4;
      auto tick =
          gm::box({-0.6f, -length / 2, -0.6f}, {0.6f, length / 2, 0.6f});
      auto transform = glm::translate(
          glm::mat4{1}, glm::vec3{129 * std::sin(a), 129 * std::cos(a), 6});
      transform = glm::rotate(transform, -a, glm::vec3{0, 0, 1});
      tick.transform(transform);
      ticks.append(tick);
    }
    medallion.children({part("engraved-scale", ticks, aluminum)});
    raisedNumber = part(
        "raised-counter",
        gm::extrude(lettering(text, serif, u8"08", 61), kLetterDepth), brass);
    raisedTitle =
        part("raised-title",
             gm::extrude(lettering(text, sans, u8"AURUM", 44, 5), kLetterDepth),
             brass);

    controls.key("turned-controls")
        .children({turnControl("brass-control", 38, brass, aluminum)
                       .at({88, 8, kFace + 3}),
                   turnControl("bronze-control", 38, bronze, brass)
                       .at({228, 8, kFace + 3}),
                   turnControl("aluminum-control", 38, aluminum, brass)
                       .at({368, 8, kFace + 3})});
    for (int i = 0; i < 3; ++i) {
      const auto caption = lettering(text, sans,
                                     i == 0   ? u8"IMPULSE"
                                     : i == 1 ? u8"RELIEF"
                                              : u8"INDEX",
                                     13, 1.2f);
      chassis.children(
          {part("control-name-" + std::to_string(i), gm::extrude(caption, 1.6f),
                aluminum, {88 + i * 140.0f, -43, kFace + 1.6f})});
    }
    const auto registerWords = lettering(text, sans, u8"0 2 4 8", 43, 7);
    registerStrip.key("sliding-register")
        .children({part("cut-register",
                        gm::extrude(
                            roundRect(410, 66, 6).subtracted(registerWords), 6),
                        brass),
                   part("enamel-register-floor",
                        gm::extrude(roundRect(410, 66, 6), 1.5f), enamel,
                        {0, 0, -3.1f}),
                   part("lower-rail", gm::box({-202, -36, -4}, {202, -32, 1}),
                        aluminum),
                   part("upper-rail", gm::box({-202, 32, -4}, {202, 36, 1}),
                        aluminum)});
    for (int x : {-1, 1})
      for (int y : {-1, 1}) {
        world::Element screw;
        screw.key("screw-" + std::to_string(x) + "-" + std::to_string(y));
        screw.children(
            {part("head", gm::extrude(circle(6), 3), aluminum),
             part("slot", gm::box({-4, -0.7f, 1.6f}, {4, 0.7f, 2.2f}),
                  enamel)});
        chassis.children({screw.at({x * 485.0f, y * 165.0f, kFace + 1.5f})
                              .rotateZ(float(x * y) * 26)});
      }

    const auto sampleWord = lettering(text, serif, u8"R8", 59);
    for (int i = 0; i < 2; ++i) {
      const float relief = i == 0 ? 0.8f : 16;
      world::Element coupon;
      coupon.key(i == 0 ? "shallow-coupon" : "deep-coupon")
          .children({part("coupon-base",
                          gm::extrude(roundRect(182, 89, 10), 12), bronze),
                     part("raised-type", gm::extrude(sampleWord, relief), brass,
                          {0, 0, 6 + relief * 0.5f + 0.8f})});
      coupons.children({coupon.at({-330 + i * 315.0f, -277, 95}).rotateX(-5)});
    }
    const auto stencil = lettering(text, sans, u8"B8", 59, 3);
    path::Outline cuts = stencil;
    // Narrow bridges keep counter islands attached to a real stencil.
    for (float y : {-15.0f, 0.0f, 15.0f})
      cuts = cuts.subtracted(placed(roundRect(170, 2.8f, 0), 0, y));
    coupons.children(
        {part("cut-letter-stencil",
              gm::extrude(roundRect(182, 89, 10).subtracted(cuts), 12),
              aluminum, {300, -277, 95})
             .rotateX(-5)});

    const auto white = ink(0xe5dcca), muted = ink(0x9faaa3),
               gold = ink(0xdab875);
    staticHud = compose::positioned().width(kWidth).height(kHeight).children(
        {label("STRUCK / 08", 62, 38, 800, 45, white).fontWeight(600),
         label("RELIEF METAL REGISTER", 64, 103, 600, 15, muted),
         label("BRASS · BRONZE · ENAMEL · ALUMINUM", 877, 54, 500, 15, gold),
         rule(63, 147, 1314, ink(0x7b7867, 0.55f)),
         label("DIE-STRUCK GLYPHS / PIERCED APERTURE / SLIDING REGISTER", 64,
               615, 1240, 14, muted),
         rule(63, 655, 1314, ink(0x7b7867, 0.55f)),
         label("RELIEF / CUT COMPARISON", 64, 688, 600, 21, white),
         label("SHALLOW 0.8", 407, 835, 230, 15, gold),
         label("DEEP 16", 710, 835, 200, 15, gold),
         label("THROUGH CUT / BRIDGES", 998, 835, 350, 15, gold),
         label("24 s / 02.4 HERO · 06 GRAZE · 09 POLISH · 12 ROUGH · 15 LOW · "
               "18 HIGH · 21 REVERSE",
               64, 912, 1300, 13, muted)});
  }

  world::Element overlay(const gm::camera::Camera& eye) const {
    const auto forward = glm::normalize(eye.target - eye.eye);
    const float distance = eye.zNear + 4;
    const auto extent = eye.extentAt(distance, float(kWidth) / kHeight);
    auto finish = material::surface::unlit({.baseColor = {1, 1, 1, 1}});
    finish.slot(material::surface::kBaseColorSlot, hud->texture());
    return part("status-overlay", gm::quad(extent.x, extent.y), finish)
        .transform(gm::camera::faceCamera(eye.eye, eye.eye + forward * distance,
                                          eye.up));
  }

  world::Frame describe(double seconds) {
    const Moment m = moment(seconds);
    auto hudTree = staticHud;
    const auto gold = ink(0xdab875), muted = ink(0x9faaa3);
    hudTree.children(
        {label(m.name, 64, 166, 820, 16, gold),
         label("YAW " + decimal(m.yaw, 0) + "°  RELIEF " + decimal(m.relief) +
                   "  ROUGHNESS " + decimal(m.roughness, 2),
               830, 166, 560, 14, muted)});
    hud->render(hudTree, std::isfinite(seconds) ? std::max(0.0, seconds) : 0);
    auto currentBrass = brass;
    currentBrass.set("roughness", m.roughness);
    auto rose = medallion;
    auto counter = raisedNumber;
    counter.fill(currentBrass)
        .at({0, 0, 10 + m.relief * 0.5f})
        .scaleZ(m.relief / kLetterDepth);
    auto pierced = piercedBody;
    rose.children({pierced.fill(currentBrass), counter});
    auto title = raisedTitle;
    title.fill(currentBrass)
        .at({225, 161, kFace + 0.8f + m.relief * 0.5f})
        .scaleZ(m.relief / kLetterDepth);
    world::Element assembly;
    assembly.key("struck-register")
        .at({0, 110, 0})
        .rotateY(m.yaw)
        .rotateX(m.pitch)
        .children({chassis, rose.at({-270, 0, 32}), controls, title,
                   registerStrip.at(
                       {225 + bounded(kControls.registerTravel, 0, 28, 20) *
                                  std::sin(m.wave),
                        -82, 14})});
    const float turn =
        bounded(kControls.lightSwing, -90, 90, 36) * std::sin(m.wave);
    const auto eye = lens();
    world::Element scene;
    scene.camera(eye).children(
        {world::Element()
             .key("studio")
             .environmentMap({.map = environment,
                              .intensity = 0.92f,
                              .diffuse = 0.45f,
                              .specular = 1,
                              .exposure = 1.05f})
             .rotateY(turn),
         world::Element().key("key").light(
             world::light::sun({0.7f * std::cos(radians(turn)), 0.24f,
                                -0.7f + 0.5f * std::sin(radians(turn))},
                               {1, 0.89f, 0.68f, 1}, 2.4f)),
         world::Element().key("fill").light(world::light::sun(
             {-0.7f, -0.3f, -0.6f}, {0.58f, 0.72f, 0.85f, 1}, 0.65f)),
         assembly, coupons, overlay(eye)});
    return world::Frame(scene).extent({kWidth, kHeight});
  }
};

}  // namespace

SIGIL_SKETCH(StruckMetal, "Study · Materials",
             "Pierced brass, die-struck glyph solids, enamel recesses and a "
             "moving engraved register")
