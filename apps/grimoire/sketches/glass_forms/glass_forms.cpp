/** Glass and liquid forms in front of a patterned wall and coloured
 *  bodies, so the scene behind each one is seen bent through it: clear
 *  glass at two indices, frosted glass, glass that disperses, tinted glass
 *  at two thicknesses, an amber shell and a square of heat haze.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using material::hexColor;

constexpr int kWidth = 1280, kHeight = 720;
constexpr float kLoop = 8, kPi = 3.14159265358979f;

uint8_t byte(float value) {
  return (uint8_t)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

float smooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

/** THE WALL: a black-and-cream checker crossed by bands of colour, the
 *  kind of picture whose straight lines show every bend a glass puts in
 *  them. */
material::Texture wallPattern() {
  constexpr int kSide = 512, kCells = 16;
  const uint32_t bands[] = {0xffd8452e, 0xff2e7dd8, 0xffe8b420, 0xff2ea860};
  SkBitmap wall;
  wall.allocN32Pixels(kSide * 2, kSide);
  for (int y = 0; y < kSide; ++y)
    for (int x = 0; x < kSide * 2; ++x) {
      const int cell = (x * kCells / kSide + y * kCells / kSide) % 2;
      uint32_t colour = cell ? 0xfff2ead8 : 0xff17171c;
      // Vertical bands every quarter of the wall, two cells wide.
      const int column = (x * kCells / kSide) % 8;
      if (column == 3) colour = bands[(x * kCells / kSide / 8) % 4];
      // …and one horizontal band across the middle.
      if (y * kCells / kSide == 7)
        colour = bands[(x * kCells / kSide / 8 + 2) % 4];
      *wall.getAddr32(x, y) = SkPreMultiplyColor(colour);
    }
  wall.setImmutable();
  return material::Texture(media::PixelSource(wall.asImage()))
      .tile(material::Repeat::Pad);
}

/** A smooth value noise that repeats every @p period cells. */
float noise(float x, float y, int period) {
  const auto hash = [period](int i, int j) {
    i = ((i % period) + period) % period;
    j = ((j % period) + period) % period;
    uint32_t h = (uint32_t)i * 374761393u + (uint32_t)j * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
  };
  const int i = (int)std::floor(x), j = (int)std::floor(y);
  const float fx = smooth(0, 1, x - (float)i), fy = smooth(0, 1, y - (float)j);
  const float a = hash(i, j), b = hash(i + 1, j);
  const float c = hash(i, j + 1), d = hash(i + 1, j + 1);
  return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

/** THE HAZE: two channels of noise, red across and green up, half-grey
 *  still — DATA, read as stored — fading to still at the square's edge so
 *  the square itself never shows. */
material::Texture hazeMap() {
  constexpr int kSize = 128;
  SkBitmap haze;
  haze.allocPixels(SkImageInfo::Make(kSize, kSize, kRGBA_8888_SkColorType,
                                     kUnpremul_SkAlphaType));
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x) {
      const float u = ((float)x + 0.5f) / kSize, v = ((float)y + 0.5f) / kSize;
      const float edge = smooth(0.0f, 0.25f, std::min({u, v, 1 - u, 1 - v}));
      const float across =
          0.7f * noise(u * 6, v * 6, 6) + 0.3f * noise(u * 12, v * 12, 12);
      const float up = 0.7f * noise(u * 6 + 3.5f, v * 6 + 1.5f, 6) +
                       0.3f * noise(u * 12 + 7, v * 12 + 2, 12);
      auto* texel = static_cast<uint8_t*>(haze.getAddr(x, y));
      texel[0] = byte(0.5f + (across - 0.5f) * 2.0f * edge * 0.5f);
      texel[1] = byte(0.5f + (up - 0.5f) * 2.0f * edge * 0.5f);
      texel[2] = 128;
      texel[3] = 255;
    }
  haze.setImmutable();
  return material::Texture(media::PixelSource(haze.asImage()))
      .colorSpace(media::ColorSpace::Data)
      .tile(material::Repeat::Repeat);
}

/** Glass: white, so it tints nothing of its own, smooth unless said. */
material::Material glass(const material::SurfaceOptions& response) {
  return material::from(material::Color{1, 1, 1, 1}).surface(response);
}

material::Material painted(material::Color colour, float roughness) {
  return material::from(colour).surface({.roughness = roughness});
}

struct GlassForms {
  world::EnvironmentMap room;
  material::Texture wall;
  material::Texture haze;
  gm::Mesh ball, slab, thickSlab, shell, square, wallPlane, cube, ring;

  static gm::camera::Camera lens(float phase) {
    gm::camera::Camera camera;
    // The eye drifts a little either way, so what is behind slides
    // through every glass.
    camera.eye = {140 * std::sin(phase), 70, 1150};
    camera.target = {0, 0, 0};
    camera.fovYDeg = 30;
    camera.zNear = 40;
    camera.zFar = 4000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    ctx.canvas(kWidth, kHeight);
    ctx.captureAt(1.0);
    ctx.background(hexColor(0x0d0e11));
    room = world::kit::environmentMap(
        {.zenith = {.60f, .62f, .70f},
         .nadir = {.12f, .11f, .10f},
         .softBoxes = {{.centre = {.28f, .38f},
                        .size = {.10f, .16f},
                        .radiance = {6.0f, 5.6f, 5.0f}},
                       {.centre = {.70f, .42f},
                        .size = {.05f, .22f},
                        .radiance = {1.6f, 2.0f, 2.8f}}}});
    wall = wallPattern();
    haze = hazeMap();
    ball = gm::superellipsoid({78, 78, 78}, 2.0f, 96, 64);
    slab = gm::box({-80, -95, -10}, {80, 95, 10});
    thickSlab = gm::box({-80, -95, -35}, {80, 95, 35});
    shell = gm::cylinderPanel(190, 190, 110, 48, 8);
    square = gm::quad(200, 200);
    wallPlane = gm::quad(2600, 1300);
    cube = gm::box({-55, -55, -55}, {55, 55, 55});
    ring = gm::torus(70, 22);
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;
    const float upper = 120, lower = -150;

    // CLEAR glass at the index of glass and of water; FROSTED; and glass
    // that DISPERSES, its edges fringed in colour.
    const material::SurfaceOptions clear{.roughness = 0.02f,
                                         .transmission = 1.0f,
                                         .ior = 1.5f,
                                         .thickness = 60.0f};
    material::SurfaceOptions water = clear;
    water.ior = 1.33f;
    material::SurfaceOptions frosted = clear;
    frosted.roughness = 0.4f;
    material::SurfaceOptions prism = clear;
    prism.dispersion = 12;
    // TINTED glass, the same green medium through a slab of 20 and of 70.
    const material::SurfaceOptions tinted{
        .roughness = 0.02f,
        .transmission = 1.0f,
        .ior = 1.5f,
        .thickness = 20.0f,
        .attenuationColor = hexColor(0x3fb58a),
        .attenuationDistance = 45};
    material::SurfaceOptions deep = tinted;
    deep.thickness = 70.0f;
    // An AMBER liquid in a curved shell.
    const material::SurfaceOptions amber{.roughness = 0.05f,
                                         .transmission = 1.0f,
                                         .ior = 1.36f,
                                         .thickness = 40.0f,
                                         .attenuationColor = hexColor(0xe08a1e),
                                         .attenuationDistance = 35};
    // HEAT HAZE: no colour of its own, what is behind displaced by the
    // map, breathing.
    const material::Material shimmer =
        material::from(material::Color{1, 1, 1, 0})
            .surface({.roughness = 1.0f,
                      .distortion = material::image(haze),
                      .distortionStrength = 9 + 5 * std::sin(phase * 2)});

    std::vector<world::Node> nodes{
        world::camera(lens(phase), {.exposure = 0.9f,
                                    .toneCurve = world::ToneCurve::Neutral}),
        world::directionalLight(
            {-0.4f, -0.5f, -0.8f},
            {.color = material::Color{1, .96f, .9f, 1}, .intensity = 2.0f})
            .key("key"),
        world::pointLight({.color = material::Color{1, .9f, .8f, 1},
                           .intensity = world::intensityAt(1.6f, 500.0f)})
            .key("lamp")
            .at({0, 260, 420}),
        // The wall, as it is: no light changes it.
        world::mesh(wallPlane)
            .key("wall")
            .at({0, 0, -520})
            .fill(
                material::from(material::image(wall)).surface({.unlit = true})),
        // Coloured bodies between the glass and the wall, sliding.
        world::mesh(cube)
            .key("cube")
            .at({-330 + 90 * std::sin(phase), 60, -260})
            .rotateY(30 + 45 * seconds)
            .fill(painted(hexColor(0xd23b2a), 0.45f)),
        world::mesh(ball)
            .key("orb")
            .at({120 - 90 * std::sin(phase), -40, -300})
            .fill(painted(hexColor(0x2b63c9), 0.3f)),
        world::mesh(ring)
            .key("ring")
            .at({440, 40 + 40 * std::cos(phase), -280})
            .rotateX(70)
            .fill(painted(hexColor(0xf0c030), 0.35f)),
    };
    const auto form = [&](std::string key, const gm::Mesh& shape, float x,
                          float y, float turn, material::Material surface) {
      nodes.push_back(world::mesh(shape)
                          .key(std::move(key))
                          .at({x, y, 0})
                          .rotateY(turn)
                          .backface(material::Backface::Visible)
                          .fill(std::move(surface)));
    };
    form("clear", ball, -450, upper, 0, glass(clear));
    form("water", ball, -150, upper, 0, glass(water));
    form("frosted", ball, 150, upper, 0, glass(frosted));
    form("prism", ball, 450, upper, 0, glass(prism));
    form("thin", slab, -450, lower, 28, glass(tinted));
    form("thick", thickSlab, -150, lower, 28, glass(deep));
    form("shell", shell, 150, lower, 0, glass(amber));
    form("haze", square, 450, lower, 0, shimmer);
    return world::Frame(world::group(std::move(nodes)))
        .environment(room, {.intensity = 0.7f, .diffuse = 0.6f});
  }
};

}  // namespace

SIGIL_SKETCH(GlassForms, "Study · Materials",
             "Clear glass at two indices, frosted glass, dispersion, tinted "
             "slabs at two thicknesses, an amber shell and heat haze, each "
             "bending the wall and the bodies behind it")
