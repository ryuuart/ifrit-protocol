/** Cut panes, a lens and a liquid cell standing on a table in front of a
 *  live page of calibration paper, so what is behind each one is seen
 *  bent, tinted and magnified through it while the page keeps moving.
 */
// TAGS: Materials/Glass, Materials/Liquid, Materials/Lighting, Geometry/3D,
// Compose/Materials, Typography/Motion

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace media = sigil::media;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::hexColor;

constexpr float kLoop = 16, kPi = 3.14159265358979f;

/** THE PAGE, in its own pixels, which are also scene units: it stands
 *  upright on the back edge of the table. */
constexpr glm::vec2 kPage{2000, 900};
constexpr float kPageDepth = -260;

/** Where each form stands along the table. */
constexpr float kCellX = -480, kSquareX = -240, kVesicaX = 0, kRingX = 240,
                kLensX = 480;

/** THE CELL: its inside width, its floor and its rim, in scene units. */
constexpr float kCellInside = 160, kCellWall = 10, kCellFloor = 28,
                kCellRim = 300, kCellDepth = 120;

float smooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

uint8_t byte(float value) {
  return (uint8_t)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

// ---------------------------------------------------------------------------
// The page

/** A colour as ink or paper the scene's light falls on. */
material::Material ink(material::Color colour, float roughness = 0.6f) {
  return material::from(colour).surface({.roughness = roughness});
}

/** Lines @p spacing page pixels apart and @p width wide, across or up the
 *  whole page, as a repeating gradient over it. */
material::Material ruling(float spacing, float width, material::Color colour,
                          bool vertical) {
  const float along = spacing / (vertical ? kPage.x : kPage.y);
  const float share = width / spacing;
  material::Color clear = colour;
  clear.a = 0;
  return material::linearGradient(
      {0, 0}, vertical ? glm::vec2{along, 0} : glm::vec2{0, along},
      {{0, colour}, {share, colour}, {share, clear}, {1, clear}},
      {.repeat = material::Repeat::Repeat});
}

/** CALIBRATION PAPER: a warm sheet ruled in a fine pale-blue grid with a
 *  heavier line every fifth. */
material::Material calibrationPaper() {
  const auto fine = material::Color{.38f, .55f, .78f, .45f};
  const auto heavy = material::Color{.30f, .46f, .72f, .75f};
  return material::from(hexColor(0xf1ebdc))
      .layer(ruling(25, 1.5f, fine, true))
      .layer(ruling(25, 1.5f, fine, false))
      .layer(ruling(125, 3, heavy, true))
      .layer(ruling(125, 3, heavy, false))
      .surface({.roughness = 0.8f});
}

/** A circle of @p radius around @p centre, as an outline. */
path::Outline circle(glm::vec2 centre, float radius) {
  const auto number = [](float value) { return std::to_string(value); };
  return path::Outline::svg(
      "M" + number(centre.x - radius) + " " + number(centre.y) + " A" +
      number(radius) + " " + number(radius) + " 0 1 0 " +
      number(centre.x + radius) + " " + number(centre.y) + " A" +
      number(radius) + " " + number(radius) + " 0 1 0 " +
      number(centre.x - radius) + " " + number(centre.y) + " Z");
}

/** Lettering set for a rail, in one face. */
compose::Text lettering(const char* words, float size, int weight) {
  auto text = compose::text(words);
  text.fontFamily("Helvetica Neue, Arial, sans-serif")
      .fontSize(size)
      .fontWeight(weight)
      .letterSpacing(size * 0.04f);
  return text;
}

/** A RAIL: @p words set twice end to end inside a clipped window,
 *  slid by @p offset, which wraps over one copy's @p width. */
Element rail(const char* words, float size, int weight,
             const material::Material& colour, glm::vec2 origin,
             glm::vec2 window, float width, float offset) {
  const float slide = std::fmod(offset, width) - (offset < 0 ? 0 : width);
  std::vector<Element> copies;
  for (int copy = 0; copy < 3; ++copy)
    copies.push_back(compose::kit::at(
        lettering(words, size, weight).ink(colour, compose::PaintBox::Glyph),
        slide + width * float(copy), 0, width + 40, window.y));
  return compose::kit::at(origin.x, origin.y, window.x, window.y)
      .overflow(compose::Overflow::Clip)
      .children({compose::stack().width(window.x).height(window.y).children(
          std::move(copies))});
}

/** THE ETCHED PLATE behind the lens: a target of rings and spokes over a
 *  millimetre scale, cut fine in black on a white card. */
Element etchedPlate(glm::vec2 origin) {
  const auto etch = ink(hexColor(0x15161a), 0.35f);
  std::vector<Element> marks{
      compose::kit::at(origin.x, origin.y, 330, 250)
          .fill(ink(hexColor(0xfbfaf6), 0.5f)),
  };
  const glm::vec2 target = origin + glm::vec2{154, 112};
  for (int ring = 1; ring <= 5; ++ring)
    marks.push_back(compose::kit::at(origin.x, origin.y, 360, 300)
                        .shape(compose::heldPath(
                            circle(target - origin, 5.0f * float(ring))))
                        .stroke(etch, {.width = ring == 5 ? 2.0f : 1.0f}));
  for (int spoke = 0; spoke < 24; ++spoke) {
    const float turn = 360.0f * float(spoke) / 24;
    marks.push_back(
        compose::kit::at(target.x, target.y - 0.5f, 25, 1.0f)
            .fill(etch)
            .transformOrigin(compose::Dimension(0.0f), compose::Dimension(0.6f))
            .rotate(turn));
  }
  // The scale: a millimetre a tick, every fifth longer, every tenth
  // longest and numbered.
  const float baseline = origin.y + 160;
  marks.push_back(
      compose::kit::at(origin.x + 15, baseline, 300, 1.5f).fill(etch));
  for (int tick = 0; tick <= 50; ++tick) {
    const float length = tick % 10 == 0 ? 26 : tick % 5 == 0 ? 17 : 10;
    marks.push_back(compose::kit::at(origin.x + 15 + 6.0f * float(tick),
                                     baseline - length, 1.2f, length)
                        .fill(etch));
    if (tick % 10 == 0)
      marks.push_back(compose::kit::at(
          lettering(std::to_string(tick / 10).c_str(), 15, 500)
              .ink(etch, compose::PaintBox::Glyph),
          origin.x + 15 + 6.0f * float(tick) - 4, baseline + 6, 30, 20));
  }
  return compose::stack().width(kPage.x).height(kPage.y).children(
      std::move(marks));
}

/** THE WIDTHS OF THE RAILS' LETTERING, measured once. */
struct Rails {
  float upper = 1000, lower = 1000;
};

constexpr const char* kUpperWords = "REFRACTION · MENISCUS · ";
constexpr const char* kLowerWords =
    "n = 1.33 water · 1.36 oil · 1.50 crown · 1.62 flint "
    "· ";

/** THE PAGE at @p seconds into the loop: calibration paper, two coloured
 *  rules across it and four sliding up it, the two rails of lettering
 *  going opposite ways, the etched plate and the caption. */
Element livePage(float seconds, const Rails& rails) {
  const float travel = seconds / kLoop;
  std::vector<Element> parts{
      compose::kit::at(0, 0, kPage.x, kPage.y).fill(calibrationPaper()),
      compose::kit::at(0, 668, kPage.x, 12).fill(ink(hexColor(0xc9402c))),
      compose::kit::at(0, 862, kPage.x, 6).fill(ink(hexColor(0x2e64c4))),
  };
  // Upright rules that drift across the sheet, so every edge a glass
  // bends is moving.
  const material::Color uprights[] = {hexColor(0xe2a91c), hexColor(0x2c9a5c),
                                      hexColor(0xc9402c), hexColor(0x2e64c4)};
  for (int rule = 0; rule < 4; ++rule) {
    const float x = std::fmod(500.0f * float(rule) + kPage.x * travel, kPage.x);
    parts.push_back(compose::kit::at(x, 380, 9, 520).fill(ink(uprights[rule])));
  }
  parts.push_back(rail(kUpperWords, 132, 800, ink(hexColor(0x18181c), 0.5f),
                       {0, 704}, {1390, 150}, rails.upper,
                       rails.upper * travel));
  parts.push_back(rail(kLowerWords, 46, 500, ink(hexColor(0xb3311f), 0.5f),
                       {0, 556}, {1390, 64}, rails.lower,
                       -rails.lower * travel));
  parts.push_back(etchedPlate({1378, 520}));
  parts.push_back(compose::kit::at(
      lettering("GLASS FORMS", 28, 700)
          .ink(ink(hexColor(0x18181c)), compose::PaintBox::Glyph),
      650, 318, 700, 36));
  parts.push_back(compose::kit::at(
      lettering("panes, a lens and a liquid cell before a moving page", 20, 400)
          .ink(ink(hexColor(0x3a3a40)), compose::PaintBox::Glyph),
      650, 354, 700, 30));
  return compose::stack().width(kPage.x).height(kPage.y).children(
      std::move(parts));
}

/** SLATE for the table: a cold dark stone, its cleft running along the
 *  table's length, honed to a soft sheen. */
Element slateTable() {
  const auto cleft =
      material::noise(.012f, {.octaves = 4, .grain = true, .stretch = 3});
  return compose::stack().width(1024).height(640).fill(
      material::from(hexColor(0x1d2024))
          .layer(cleft,
                 {.blend = material::BlendMode::Multiply, .opacity = .45f})
          .layer(material::noise(.08f, {.octaves = 2, .grain = true}),
                 {.blend = material::BlendMode::Multiply, .opacity = .12f})
          .surface({.roughness = .6f,
                    .normal = material::surface::normalFromHeight(
                        cleft, {.depth = 1.0f, .directX = true}),
                    .normalDirectX = true}));
}

// ---------------------------------------------------------------------------
// The forms

/** A PANE WITH A ROUNDED EDGE: @p ring answers the pane's outline drawn
 *  @p inset inside its edge, and the pane is that outline lofted through
 *  a half-round of @p thickness, so its faces are flat and its rim turns
 *  through a semicircle. */
gm::Mesh roundedPane(const std::function<std::vector<glm::vec2>(float)>& ring,
                     float thickness, int steps = 10) {
  const float radius = thickness / 2;
  std::vector<std::vector<glm::vec3>> sections;
  for (int step = 0; step <= steps; ++step) {
    const float angle = kPi / 2 - kPi * float(step) / float(steps);
    const float inset = radius * (1 - std::cos(angle));
    const float depth = radius * std::sin(angle);
    std::vector<glm::vec3> section;
    for (const glm::vec2 point : ring(inset))
      section.push_back({point.x, point.y, depth});
    sections.push_back(std::move(section));
  }
  return gm::loft(sections, {.capEnds = true});
}

/** A rectangle @p half across each way with corners rounded to @p corner,
 *  drawn @p inset inside its edge, anticlockwise. */
std::vector<glm::vec2> softRectangle(glm::vec2 half, float corner,
                                     float inset) {
  const glm::vec2 reach = half - corner;
  const float radius = corner - inset;
  const glm::vec2 centres[] = {{reach.x, -reach.y},
                               {reach.x, reach.y},
                               {-reach.x, reach.y},
                               {-reach.x, -reach.y}};
  std::vector<glm::vec2> points;
  for (int quarter = 0; quarter < 4; ++quarter)
    for (int step = 0; step <= 8; ++step) {
      const float angle = kPi / 2 * (float(quarter) - 1 + float(step) / 8);
      points.push_back(centres[quarter] +
                       radius * glm::vec2{std::cos(angle), std::sin(angle)});
    }
  return points;
}

/** A LENS SHAPE — two arcs of @p radius whose centres stand @p apart
 *  either side of the middle — drawn @p inset inside its edge. */
std::vector<glm::vec2> vesica(float radius, float apart, float inset) {
  const float arc = radius - inset;
  const float reach = std::acos(std::clamp(apart / arc, -1.0f, 1.0f));
  std::vector<glm::vec2> points;
  constexpr int kSteps = 24;
  for (int step = 0; step < kSteps; ++step) {
    const float angle = -reach + 2 * reach * float(step) / kSteps;
    points.push_back({-apart + arc * std::cos(angle), arc * std::sin(angle)});
  }
  for (int step = 0; step < kSteps; ++step) {
    const float angle = kPi - reach + 2 * reach * float(step) / kSteps;
    points.push_back({apart + arc * std::cos(angle), arc * std::sin(angle)});
  }
  return points;
}

/** AN ANNULUS from @p inner to @p outer, @p thickness deep, both rims
 *  turned through a half-round — the profile lathed about its axis, which
 *  then faces +z. */
gm::Mesh annulus(float inner, float outer, float thickness) {
  const float radius = thickness / 2;
  std::vector<glm::vec2> profile;
  constexpr int kSteps = 12;
  for (int step = 0; step <= kSteps; ++step) {
    const float angle = kPi / 2 - kPi * float(step) / kSteps;
    profile.push_back(
        {outer - radius + radius * std::cos(angle), radius * std::sin(angle)});
  }
  for (int step = 0; step <= kSteps; ++step) {
    const float angle = -kPi / 2 - kPi * float(step) / kSteps;
    profile.push_back(
        {inner + radius + radius * std::cos(angle), radius * std::sin(angle)});
  }
  profile.push_back(profile.front());
  std::reverse(profile.begin(), profile.end());
  return gm::revolve(profile, {.segments = 96});
}

/** A BICONVEX LENS of @p radius, @p middle thick at its axis and @p edge
 *  at its rim, its two faces spherical caps, lathed about its axis. */
gm::Mesh biconvex(float radius, float middle, float edge) {
  const float sag = (middle - edge) / 2;
  const float sphere = (radius * radius + sag * sag) / (2 * sag);
  const auto height = [&](float r) {
    return edge / 2 + std::sqrt(sphere * sphere - r * r) - (sphere - sag);
  };
  std::vector<glm::vec2> profile;
  constexpr int kSteps = 24;
  for (int step = 0; step <= kSteps; ++step) {
    const float r = radius * float(step) / kSteps;
    profile.push_back({r, height(r)});
  }
  for (int step = kSteps; step >= 0; --step) {
    const float r = radius * float(step) / kSteps;
    profile.push_back({r, -height(r)});
  }
  std::reverse(profile.begin(), profile.end());
  return gm::revolve(profile, {.segments = 96});
}

/** A CYLINDER of @p radius standing @p height tall on its base. */
gm::Mesh cylinder(float radius, float height) {
  return gm::revolve({{0, 0}, {radius, 0}, {radius, height}, {0, height}},
                     {.segments = 32});
}

/** THE LIQUID at @p level: the cell's inside filled from its floor, the
 *  top flat across the middle and climbing each wall in a meniscus. Its
 *  outline is drawn with y down the page, as an extrusion reads one. */
gm::Mesh liquid(float level) {
  constexpr float kRise = 9, kReach = 11;
  const float depth = level - kCellFloor;
  std::string outline = "M0 " + std::to_string(depth) + " L" +
                        std::to_string(kCellInside) + " " +
                        std::to_string(depth);
  constexpr int kSteps = 48;
  for (int step = 0; step <= kSteps; ++step) {
    const float x = kCellInside * (1 - float(step) / kSteps);
    const float climb =
        kRise * (std::exp(-x / kReach) + std::exp(-(kCellInside - x) / kReach));
    outline += " L" + std::to_string(x) + " " + std::to_string(-climb);
  }
  outline += " Z";
  return gm::extrude(path::Outline::svg(outline), kCellDepth - 12);
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
      const float edge = smooth(0.0f, 0.3f, std::min(u, 1 - u)) *
                         smooth(0.0f, 0.2f, v) * smooth(0.0f, 0.45f, 1 - v);
      const float across =
          0.7f * noise(u * 4, v * 6, 4) + 0.3f * noise(u * 8, v * 12, 8);
      const float up = 0.7f * noise(u * 4 + 3.5f, v * 6 + 1.5f, 4) +
                       0.3f * noise(u * 8 + 7, v * 12 + 2, 8);
      auto* texel = static_cast<uint8_t*>(haze.getAddr(x, y));
      texel[0] = byte(0.5f + (across - 0.5f) * edge);
      texel[1] = byte(0.5f + (up - 0.5f) * edge);
      texel[2] = 128;
      texel[3] = 255;
    }
  haze.setImmutable();
  return material::Texture(media::PixelSource(haze.asImage()))
      .colorSpace(media::ColorSpace::Data)
      .tile(material::Repeat::Repeat);
}

/** Glass: white, so it tints nothing of its own. */
material::Material glass(const material::SurfaceOptions& response) {
  return material::from(material::Color{1, 1, 1, 1}).surface(response);
}

material::Material solid(material::Color colour, float roughness,
                         float metallic = 0) {
  return material::from(colour).surface(
      {.metallic = metallic, .roughness = roughness});
}

struct GlassForms {
  sketch::Page page, tablePage;
  Element tableTree = compose::stack();
  Rails rails;
  world::EnvironmentMap room;
  material::Texture haze;
  gm::Mesh pageSheet, table, square, lensPane, ring, lens, rim, stem, footing,
      foot, plinth, walls, backWall, frontWall, rod, tick, longTick, shimmer,
      cover;

  static gm::camera::Camera camera(float phase) {
    gm::camera::Camera lensOfEye;
    // The eye drifts a little either way, so what is behind slides
    // through every glass.
    lensOfEye.eye = {110 * std::sin(phase), 310, 1350};
    lensOfEye.target = {0, 250, -80};
    lensOfEye.fovYDeg = 30;
    lensOfEye.zNear = 40;
    lensOfEye.zFar = 5000;
    return lensOfEye;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1280, 720},
                             .captureAt = 3.0,
                             .background = hexColor(0x0d0e11)});
    page = ctx.page({int(kPage.x), int(kPage.y)});
    tablePage = ctx.page({1024, 640});
    tableTree = slateTable();
    rails.upper =
        compose::intrinsicSize(lettering(kUpperWords, 132, 800), ctx.fonts)
            .width();
    rails.lower =
        compose::intrinsicSize(lettering(kLowerWords, 46, 500), ctx.fonts)
            .width();
    room = world::kit::environmentMap(
        {.zenith = {.55f, .57f, .64f},
         .nadir = {.08f, .08f, .09f},
         .softBoxes = {{.centre = {.30f, .36f},
                        .size = {.09f, .16f},
                        .radiance = {6.0f, 5.6f, 5.0f}},
                       {.centre = {.72f, .42f},
                        .size = {.05f, .24f},
                        .radiance = {1.6f, 2.0f, 2.8f}},
                       {.centre = {.52f, .18f},
                        .size = {.30f, .04f},
                        .radiance = {2.4f, 2.4f, 2.4f}}}});
    haze = hazeMap();

    pageSheet = gm::quad(kPage.x, kPage.y);
    table = gm::quad(3400, 2000);
    square = roundedPane(
        [](float inset) { return softRectangle({110, 110}, 40, inset); }, 34);
    lensPane =
        roundedPane([](float inset) { return vesica(166, 71, inset); }, 26);
    ring = annulus(48, 118, 28);
    lens = biconvex(96, 46, 8);
    rim = gm::torus(100, 7);
    stem = cylinder(6, 120);
    footing = cylinder(48, 12);
    foot = gm::box({-58, 0, -24}, {58, 24, 24});
    plinth = gm::box({-104, 0, -70}, {104, 18, 70});
    const float outer = kCellInside + 2 * kCellWall;
    const float tall = kCellRim - (kCellFloor - kCellWall);
    walls = gm::extrude(
        path::Outline::rectangle(path::Rect::of({0, 0}, {outer, tall}))
            .subtracted(path::Outline::rectangle(
                path::Rect::of({kCellWall, -kCellWall}, {kCellInside, tall}))),
        kCellDepth);
    backWall = gm::box({-kCellInside / 2, kCellFloor, -kCellDepth / 2},
                       {kCellInside / 2, kCellRim, -kCellDepth / 2 + 6});
    frontWall = gm::quad(kCellInside, kCellRim - kCellFloor);
    rod = cylinder(7, 330);
    tick = gm::box({0, -1.2f, 0}, {12, 1.2f, 1.5f});
    longTick = gm::box({0, -1.4f, 0}, {24, 1.4f, 1.5f});
    shimmer = gm::quad(230, 260);
    cover = roundedPane(
        [](float inset) { return softRectangle({270, 62}, 14, inset); }, 12);
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;

    // CLEAR glass, bending only at its rounded rim.
    const material::SurfaceOptions clear{.roughness = 0.02f,
                                         .transmission = 1.0f,
                                         .ior = 1.5f,
                                         .thickness = 40.0f};
    // The lens-shaped pane DISPERSES: each colour bent by its own index.
    material::SurfaceOptions prism = clear;
    prism.dispersion = 14;
    // The annulus is GREEN glass, darker where the light crosses more.
    material::SurfaceOptions green = clear;
    green.thickness = 30.0f;
    green.attenuationColor = hexColor(0x3fb58a);
    green.attenuationDistance = 40;
    // The magnifier, thick at its axis.
    material::SurfaceOptions magnifier = clear;
    magnifier.thickness = 360.0f;
    // The SMOKED cover: a grey-brown medium over a short distance.
    const material::SurfaceOptions smoked{
        .roughness = 0.05f,
        .transmission = 1.0f,
        .ior = 1.5f,
        .thickness = 30.0f,
        .attenuationColor = hexColor(0x4a4038),
        .attenuationDistance = 60};
    // The cell's warm AMBER liquid, bent as a deep body of it would bend
    // the light, so the rod standing in it breaks at the surface.
    const material::SurfaceOptions amber{.roughness = 0.03f,
                                         .transmission = 1.0f,
                                         .ior = 1.36f,
                                         .thickness = 320.0f,
                                         .attenuationColor = hexColor(0xe8962a),
                                         .attenuationDistance = 260};
    // The cell's front glass, flat and square on: what it shows is only
    // the room it reflects, added over the liquid behind it.
    const material::Material frontGlass =
        material::from(material::Color{0, 0, 0, 1})
            .surface({.roughness = 0.03f,
                      .blend = material::BlendMode::PlusLighter,
                      .depthWrite = false});
    // HEAT HAZE over the warm liquid: no colour of its own, what is
    // behind displaced by the map, breathing.
    const material::Material shimmerSurface =
        material::from(material::Color{1, 1, 1, 0})
            .surface({.roughness = 1.0f,
                      .distortion = material::image(haze),
                      .distortionStrength = 7 + 3 * std::sin(phase * 2)});

    const auto brass = solid(hexColor(0xb08a4a), 0.32f, 1.0f);
    const auto enamel = solid(hexColor(0xf4f1e8), 0.45f);
    const auto walnut = solid(hexColor(0x3b2618), 0.5f);

    // The fill level rises and falls against the graduation.
    const float level = 175 + 75 * std::sin(phase);
    const float pool = level - kCellFloor;

    std::vector<world::Node> nodes{
        world::camera(camera(phase), {.exposure = 1.0f,
                                      .toneCurve = world::ToneCurve::Neutral}),
        world::directionalLight({0.5f, -0.55f, -0.67f},
                                {.color = material::Color{1, .95f, .86f, 1},
                                 .intensity = 2.6f,
                                 .castShadow = true,
                                 .shadow = {.radius = 1.6f, .cascades = 2}})
            .key("key"),
        world::hemisphereLight(
            {.color = material::Color{.6f, .66f, .78f, 1},
             .groundColor = material::Color{.1f, .09f, .08f, 1},
             .intensity = .25f})
            .key("fill"),
        world::mesh(table)
            .key("table")
            .at({0, 0, kPageDepth + 1000})
            .rotateX(-90)
            .fill(tablePage.surface()),
        world::mesh(pageSheet)
            .key("page")
            .at({0, kPage.y / 2, kPageDepth})
            .fill(page.surface()),
        world::mesh(cover)
            .key("smoked cover")
            .at({110, 405, kPageDepth + 14})
            .castShadow(false)
            .fill(glass(smoked)),
    };

    // THE STANDS: a brass foot under each pane.
    for (const float x : {kSquareX, kVesicaX, kRingX})
      nodes.push_back(world::mesh(foot)
                          .key("foot " + std::to_string(int(x)))
                          .at({x, 0, 0})
                          .fill(walnut));
    const auto pane = [&](std::string key, const gm::Mesh& shape, float x,
                          float y, material::Material surface) {
      nodes.push_back(world::mesh(shape)
                          .key(std::move(key))
                          .at({x, y, 0})
                          .castShadow(false)
                          .fill(std::move(surface)));
    };
    pane("square", square, kSquareX, 18 + 110, glass(clear));
    pane("vesica", lensPane, kVesicaX, 18 + 150, glass(prism));
    nodes.push_back(world::mesh(ring)
                        .key("annulus")
                        .at({kRingX, 18 + 118, 0})
                        .rotateX(90)
                        .castShadow(false)
                        .fill(glass(green)));

    // THE MAGNIFIER on its brass stand, over the etched plate.
    const float lensHeight = 230;
    nodes.push_back(
        world::mesh(footing).key("lens foot").at({kLensX, 0, 0}).fill(brass));
    nodes.push_back(
        world::mesh(stem).key("lens stem").at({kLensX, 10, 0}).fill(brass));
    nodes.push_back(world::mesh(rim)
                        .key("lens rim")
                        .at({kLensX, lensHeight, 0})
                        .rotateX(90)
                        .fill(brass));
    nodes.push_back(world::mesh(lens)
                        .key("lens")
                        .at({kLensX, lensHeight, 0})
                        .rotateX(90)
                        .castShadow(false)
                        .fill(glass(magnifier)));

    // THE CELL: a plinth, glass walls, the liquid, the rod standing in it
    // and the graduation enamelled on the front glass.
    nodes.push_back(world::mesh(plinth)
                        .key("plinth")
                        .at({kCellX, 0, 0})
                        .fill(solid(hexColor(0x141518), 0.4f)));
    nodes.push_back(
        world::mesh(walls)
            .key("cell walls")
            .at({kCellX, (kCellFloor - kCellWall + kCellRim) / 2, 0})
            .castShadow(false)
            .fill(glass({.roughness = 0.02f,
                         .transmission = 1.0f,
                         .ior = 1.5f,
                         .thickness = 12.0f})));
    nodes.push_back(world::mesh(backWall)
                        .key("cell back")
                        .at({kCellX, 0, 0})
                        .castShadow(false)
                        .fill(glass({.roughness = 0.02f,
                                     .transmission = 1.0f,
                                     .ior = 1.5f,
                                     .thickness = 6.0f})));
    nodes.push_back(world::mesh(rod)
                        .key("rod")
                        .at({kCellX + 46, kCellFloor, -8})
                        .rotateZ(15)
                        .rotateX(-6)
                        .fill(solid(hexColor(0x1d3aa6), 0.12f)));
    nodes.push_back(world::mesh(liquid(level))
                        .key("liquid")
                        .at({kCellX, kCellFloor + pool / 2 + 4.5f, 0})
                        .castShadow(false)
                        .fill(glass(amber)));
    nodes.push_back(
        world::mesh(frontWall)
            .key("cell front")
            .at({kCellX, (kCellFloor + kCellRim) / 2, kCellDepth / 2 + 0.5f})
            .castShadow(false)
            .fill(frontGlass));
    for (int mark = 0; mark <= 10; ++mark)
      nodes.push_back(
          world::mesh(mark % 5 == 0 ? longTick : tick)
              .key("graduation " + std::to_string(mark))
              .at({kCellX + kCellInside / 2 - (mark % 5 == 0 ? 26.0f : 14.0f),
                   kCellFloor + 22 + 24.0f * float(mark), kCellDepth / 2 + 1})
              .fill(enamel));
    nodes.push_back(world::mesh(shimmer)
                        .key("haze")
                        .at({kCellX, kCellRim + 145, 20})
                        .castShadow(false)
                        .fill(shimmerSurface));

    return world::Frame(world::group(std::move(nodes)))
        .environment(room, {.intensity = 0.8f, .diffuse = 0.45f})
        .pass(page.render(livePage(seconds, rails)))
        .pass(tablePage.render(tableTree));
  }
};

}  // namespace

SIGIL_SKETCH(GlassForms, "Study · Materials",
             "Cut panes, a magnifier and a liquid cell with a meniscus "
             "standing in front of a live page, each bending, tinting or "
             "enlarging the moving lettering and rules behind it")
