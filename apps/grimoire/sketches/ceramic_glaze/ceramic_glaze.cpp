/** Three glazes, each on a thrown jar and an incised tile: a clear coat
 *  over a crazed glass whose colour pools deeper in every recess, the
 *  crazing carried by the coat's own normal so it breaks up the coat's
 *  reflections of a soft key and a studio sky, and a bare foot under no
 *  coat at all.
 */
// TAGS: Materials/Ceramics, Materials/Lighting, Geometry/3D,
// Compose/Materials

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Scatter.h>
#include <sigilgeometry/path/Through.h>
#include <sigilgeometry/path/Triangulate.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::Color;
using material::hexColor;

constexpr int kWidth = 1440, kHeight = 900;
/** The jar's page runs once round the jar across its width, and along the
 *  profile up its height: the underside at the bottom of the page, the
 *  floor inside at the top. */
constexpr int kJarPageWidth = 1024, kJarPageHeight = 640;
constexpr int kTilePage = 512;
constexpr int kProfileSamples = 320;
constexpr float kLoop = 16, kPi = 3.14159265358979f;
constexpr float kJarSpacing = 380, kTileSize = 196, kTileThickness = 14;
/** How far a tile leans back toward the eye, in degrees. */
constexpr float kTileTilt = 14;

/** ONE GLAZE: the colour it fires to where it runs thin, where it lies at
 *  an ordinary depth and where it pools; the ink its crazing takes up, in
 *  the wide cracks and in the fine ones between them; how coarse the
 *  crazing is; the clay it sits on. */
struct Glaze {
  const char* name;
  Color thin, middle, pooled;
  Color crackInk;
  float crackInkOpacity;
  Color fineInk;
  float fineInkOpacity;
  float cellSpacing;
  uint64_t seed;
  float coatRoughness;
  Color clay;
};

/** Celadon on grey stoneware, its crazing stained faintly; oxblood on
 *  porcelain, white where it thins at the rim and nearly black where it
 *  pools, crazed fine and unstained; amber on buff clay, its wide cracks
 *  inked dark and its fine ones gold. */
const std::array<Glaze, 3> kGlazes{{
    {.name = "celadon",
     .thin = hexColor(0xb9cdb0),
     .middle = hexColor(0x86ab95),
     .pooled = hexColor(0x3f6f61),
     .crackInk = hexColor(0x5d5a48),
     .crackInkOpacity = .35f,
     .fineInk = hexColor(0xd8d2b8),
     .fineInkOpacity = .25f,
     .cellSpacing = 30,
     .seed = 11,
     .coatRoughness = .03f,
     .clay = hexColor(0x9a8f82)},
    {.name = "oxblood",
     .thin = hexColor(0xd9c7b8),
     .middle = hexColor(0x9c1c1f),
     .pooled = hexColor(0x4a070c),
     .crackInk = hexColor(0x2a0508),
     .crackInkOpacity = .18f,
     .fineInk = hexColor(0x2a0508),
     .fineInkOpacity = 0,
     .cellSpacing = 22,
     .seed = 23,
     .coatRoughness = .02f,
     .clay = hexColor(0xe0d8cb)},
    {.name = "amber",
     .thin = hexColor(0xe2b25a),
     .middle = hexColor(0xa8611c),
     .pooled = hexColor(0x3b1d0b),
     .crackInk = hexColor(0x1e120a),
     .crackInkOpacity = .7f,
     .fineInk = hexColor(0xf3c96a),
     .fineInkOpacity = .2f,
     .cellSpacing = 40,
     .seed = 37,
     .coatRoughness = .06f,
     .clay = hexColor(0xb98d61)},
}};

// ---------------------------------------------------------------------------
// THE JAR: one profile, lathed, and the glaze depth its curvature implies.

/** The jar's profile as (radius, height), from the middle of the
 *  underside out over the foot ring, up the body to a rolled lip and back
 *  down the inside to the middle of the floor, spaced evenly along its
 *  length so the page's rows are spaced evenly over the jar. */
struct Profile {
  std::vector<glm::vec2> points;
  /** The glaze's depth at each point, 0 thin to 1 pooled. */
  std::vector<float> depth;
  /** How far along the profile the glaze begins above the bare foot, 0
   *  to 1. */
  float glazeLine = 0;
};

Profile throwJar() {
  const std::vector<glm::vec2> guide{
      {0, 9},     {34, 9},    {44, 5},    {50, 0},    {60, 0},    {66, 6},
      {68, 16},   {80, 34},   {100, 66},  {116, 112}, {121, 158}, {115, 204},
      {98, 242},  {74, 270},  {54, 288},  {47, 304},  {50, 320},  {57, 331},
      {53, 338},  {44, 334},  {40, 318},  {41, 300},  {60, 278},  {90, 246},
      {106, 204}, {110, 156}, {101, 100}, {78, 50},   {40, 26},   {0, 21}};
  Profile jar;
  jar.points =
      path::resample(path::curveThrough(guide), kProfileSamples).front().points;
  auto& points = jar.points;
  const size_t count = points.size();
  const auto outward = [&](size_t i) {
    const glm::vec2 along =
        points[std::min(i + 1, count - 1)] - points[i == 0 ? 0 : i - 1];
    return glm::normalize(glm::vec2(along.y, -along.x));
  };
  // THROWING: the potter's fingers leave shallow rings up the outside of
  // the body, and a pair of grooves is cut round the shoulder.
  size_t lipIndex = 0;
  for (size_t i = 0; i < count; ++i)
    if (points[i].y > points[lipIndex].y) lipIndex = i;
  std::vector<glm::vec2> thrown = points;
  for (size_t i = 1; i + 1 < lipIndex; ++i) {
    const float height = points[i].y;
    if (height < 30 || height > 290) continue;
    const float fade =
        std::min({1.0f, (height - 30) / 30, (290 - height) / 30});
    float offset = -0.25f * fade *
                   (0.5f + 0.5f * std::cos(height * 0.5f +
                                           1.6f * std::sin(height * 0.031f)));
    for (const float groove : {214.0f, 224.0f})
      offset -= 2.4f * std::exp(-0.5f * std::pow((height - groove) / 1.8f, 2));
    thrown[i] += outward(i) * offset;
  }
  points = std::move(thrown);
  // THE GLAZE'S DEPTH: it gathers where the surface is hollow along the
  // profile and on what faces up, and lies thin over what is convex.
  std::vector<float> depth(count, 0.4f);
  for (size_t i = 1; i + 1 < count; ++i) {
    const glm::vec2 bend = points[i - 1] + points[i + 1] - 2.0f * points[i];
    const float spacing = glm::length(points[i + 1] - points[i - 1]) * 0.5f;
    const float hollow = glm::dot(bend, outward(i)) / (spacing * spacing);
    depth[i] = 0.42f + std::clamp(hollow * 5.0f, -0.35f, 0.6f) +
               0.22f * std::max(0.0f, outward(i).y);
  }
  // The glaze line, where the glaze stops above the bare foot, and the
  // roll it gathers into just above it as it runs down in the kiln.
  size_t glazeIndex = 0;
  while (glazeIndex < lipIndex && points[glazeIndex].y < 18) ++glazeIndex;
  jar.glazeLine = float(glazeIndex) / float(count - 1);
  for (size_t i = glazeIndex; i < lipIndex; ++i) {
    const float above = points[i].y - points[glazeIndex].y;
    depth[i] +=
        0.55f * std::exp(-above / 9.0f) + 0.12f * std::exp(-above / 60.0f);
  }
  // The floor inside holds a pool of its own.
  for (size_t i = lipIndex; i < count; ++i)
    depth[i] += 0.5f * std::max(0.0f, 1.0f - points[i].y / 60.0f);
  // Glaze flows, so its depth is softened along the profile.
  for (int pass = 0; pass < 2; ++pass) {
    std::vector<float> soft = depth;
    for (size_t i = 1; i + 1 < count; ++i)
      soft[i] = 0.25f * depth[i - 1] + 0.5f * depth[i] + 0.25f * depth[i + 1];
    depth = std::move(soft);
  }
  for (float& value : depth) value = std::clamp(value, 0.0f, 1.0f);
  jar.depth = std::move(depth);
  return jar;
}

/** The glaze's depth up the jar's page, grey from thin to pooled. */
material::Material depthUpThePage(const Profile& jar) {
  // A ramp holds a bounded number of stops, so the depth is read at as
  // many evenly spaced places along the profile.
  constexpr size_t kStops = 256;
  std::vector<material::ColorStop> stops;
  stops.reserve(kStops);
  for (size_t stop = 0; stop < kStops; ++stop) {
    const float along = float(stop) / float(kStops - 1);
    const float grey =
        jar.depth[size_t(std::lround(along * float(jar.depth.size() - 1)))];
    stops.push_back({along, Color{grey, grey, grey, 1}});
  }
  return material::linearGradient({0, 1}, {0, 0},
                                  material::ColorStops(std::move(stops)));
}

// ---------------------------------------------------------------------------
// THE CRAZING: a cell network in a height, each cell tipped a little its
// own way, every crack a groove.

/** A network of cells about @p spacing across over @p width by @p height,
 *  repeating across its width when @p wraps. @p tipped, it is a height:
 *  each cell a plane tipped its own way about mid grey and every crack
 *  between them a groove. Otherwise it is where the cracks run, black on
 *  white, for the ink they take up. */
Element crazing(int width, int height, float spacing, uint64_t seed, bool wraps,
                bool tipped) {
  const auto count = int(float(width) * float(height) / (spacing * spacing));
  std::vector<glm::vec2> sites = path::sample(
      path::Region::of(path::Rect::of({0, 0}, {float(width), float(height)})),
      // Sites spread a little apart, not relaxed into a lattice: crazing
      // opens cells of uneven size.
      path::distribution::blueNoise(count, seed, 2));
  const size_t own = sites.size();
  if (wraps) {
    sites.reserve(own * 3);
    for (size_t i = 0; i < own; ++i)
      sites.push_back(sites[i] - glm::vec2(width, 0));
    for (size_t i = 0; i < own; ++i)
      sites.push_back(sites[i] + glm::vec2(width, 0));
  }
  const float margin = wraps ? float(width) : 0;
  const std::vector<path::Polyline> cells = path::voronoi(
      sites,
      path::Rect{{-margin, -2}, {float(width) + margin, float(height) + 2}});
  auto field = compose::stack().width(width).height(height).fill(
      Color{.5f, .5f, .5f, 1});
  std::vector<Element> pieces;
  for (size_t i = 0; i < cells.size(); ++i) {
    const path::Polyline& cell = cells[i];
    if (cell.points.size() < 3) continue;
    float left = cell.points.front().x, right = left;
    for (const glm::vec2& point : cell.points) {
      left = std::min(left, point.x);
      right = std::max(right, point.x);
    }
    if (right < 0 || left > float(width)) continue;
    // The tip is the original site's, so a cell and its repeat across the
    // seam lean the same way.
    const size_t origin = i % own;
    const float turn = float(origin) * 2.399963f + float(seed);
    const float lean = 0.08f + 0.1f * float((origin * 7919) % 13) / 12.0f;
    const glm::vec2 direction{std::cos(turn), std::sin(turn)};
    const glm::vec2 centre = cell.centroid();
    path::Polyline ring = cell;
    ring.closed = true;
    auto piece = compose::kit::at(0, 0, float(width), float(height))
                     .shape(compose::heldPath(path::toPath(ring)));
    if (tipped)
      piece
          .fill(material::linearGradient(
              centre - direction * spacing * 0.5f,
              centre + direction * spacing * 0.5f,
              {Color{.5f - lean, .5f - lean, .5f - lean, 1},
               Color{.5f + lean, .5f + lean, .5f + lean, 1}},
              {.units = material::GradientUnits::Pixels}))
          .stroke(
              material::Material(Color{.3f, .3f, .3f, 1}),
              {.width = 0.9f, .position = material::StrokePosition::Center});
    else
      piece.fill(Color{1, 1, 1, 1})
          .stroke(
              material::Material(Color{0, 0, 0, 1}),
              {.width = 1.5f, .position = material::StrokePosition::Center});
    pieces.push_back(std::move(piece));
  }
  field.children(std::move(pieces));
  return field.filter(material::Filter::blur(0.6f));
}

/** A texture painted once from @p tree, read as data. */
material::Texture paintedOnce(sketch::Scene3DContext& ctx, glm::ivec2 size,
                              const Element& tree, material::Repeat repeat) {
  return ctx.texture(tree, {size.x, size.y}, Color{0, 0, 0, 1})
      .tile(repeat)
      .sampling(material::Sampling::Linear)
      .colorSpace(sigil::media::ColorSpace::Data);
}

/** The coat's own normal: the crazing's height, differentiated. */
Element coatNormal(int width, int height, const material::Texture& crazed,
                   const std::optional<material::Material>& under = {}) {
  auto normal = material::surface::normalFromHeight(
      material::image(crazed), {.depth = 18, .step = 0.8f});
  if (under)
    normal = material::surface::blendNormals(*under, std::move(normal));
  return compose::kit::at(0, 0, float(width), float(height)).fill(normal);
}

// ---------------------------------------------------------------------------
// THE SURFACES the pages paint.

/** Glaze over @p depth: thin colour, the ordinary colour where it lies
 *  deeper, the pooled colour where it gathers, the crazing inked in, and
 *  a clear coat over all of it whose own normal is @p coatNormal. */
material::Material glazed(const Glaze& glaze, const material::Material& depth,
                          const material::Material& crazed,
                          const material::Material& fine,
                          const material::Texture& coatNormal,
                          std::optional<material::Material> normal = {}) {
  using material::ColorChannel;
  return material::from(glaze.thin)
      .layer(glaze.middle,
             {.mask = {depth, ColorChannel::Luminance, .12f, .5f}})
      .layer(glaze.pooled,
             {.mask = {depth, ColorChannel::Luminance, .5f, .95f}})
      .layer(material::noise(.02f, {.octaves = 3, .grain = true}),
             {.blend = material::BlendMode::SoftLight, .opacity = .35f})
      .layer(glaze.fineInk,
             {.opacity = glaze.fineInkOpacity,
              .mask = {fine, ColorChannel::Luminance, .2f, .8f, true}})
      .layer(glaze.crackInk,
             {.opacity = glaze.crackInkOpacity,
              .mask = {crazed, ColorChannel::Luminance, .2f, .8f, true}})
      .surface({.roughness = .35f,
                .normal = std::move(normal),
                .clearcoat = 1.0f,
                .clearcoatRoughness = glaze.coatRoughness,
                .clearcoatNormal = material::image(coatNormal)});
}

/** Bare clay: rough, dull, grained, and under no coat. */
material::Material bareClay(const Glaze& glaze) {
  return material::from(glaze.clay)
      .layer(material::noise(.09f, {.octaves = 3, .grain = true}),
             {.blend = material::BlendMode::Multiply, .opacity = .3f})
      .surface({.roughness = .9f,
                .normal = material::surface::normalFromHeight(
                    material::noise(.3f, {.octaves = 2, .grain = true}),
                    {.depth = .7f})});
}

/** The same clay as a body wears it directly: its colour and its
 *  roughness. */
material::Material clayEdge(const Glaze& glaze) {
  return material::from(glaze.clay).surface({.roughness = .9f});
}

/** THE JAR'S PAGE: bare clay over the foot, and the glaze from a wavering
 *  line above it to the floor inside, with a few runs reaching down. */
Element jarPage(const Glaze& glaze, const Profile& jar,
                const material::Material& depth,
                const material::Material& crazed,
                const material::Material& fine,
                const material::Texture& coatNormal) {
  const float line = (1 - jar.glazeLine) * kJarPageHeight;
  std::string edge = "M0 0";
  for (int x = 0; x <= kJarPageWidth; x += 8) {
    const float u = float(x) / kJarPageWidth;
    float y = line + 2.0f * std::sin(u * 2 * kPi * 5 + float(glaze.seed)) +
              1.2f * std::sin(u * 2 * kPi * 13);
    for (int run = 0; run < 4; ++run) {
      const float centre = std::fmod(
          0.17f + 0.27f * float(run) + 0.05f * float(glaze.seed % 7), 1.0f);
      const float distance =
          std::min(std::abs(u - centre), 1 - std::abs(u - centre));
      y += 7.0f * std::exp(-0.5f * std::pow(distance / 0.008f, 2));
    }
    edge += " L" + std::to_string(x) + " " + std::to_string(y);
  }
  edge += " L" + std::to_string(kJarPageWidth) + " 0 Z";
  return compose::stack()
      .width(kJarPageWidth)
      .height(kJarPageHeight)
      .fill(bareClay(glaze))
      .children({compose::kit::at(0, 0, kJarPageWidth, kJarPageHeight)
                     .shape(compose::heldPath(path::Outline::svg(edge)))
                     .fill(glazed(glaze, depth, crazed, fine, coatNormal))});
}

/** THE TILE'S INCISIONS: a field of @p field with a border, rings and
 *  petals cut into it in @p cut, softened by @p softness — a height when
 *  the field is white and the cuts black, the glaze's depth when the
 *  field is an ordinary grey and the cuts white. */
Element incisions(float softness, Color field, Color cutColour) {
  constexpr float size = kTilePage, middle = kTilePage / 2.0f;
  const auto cut = [&](const path::Outline& line, float width) {
    return compose::kit::at(0, 0, size, size)
        .shape(compose::heldPath(line))
        .stroke(material::Material(cutColour),
                {.width = width, .position = material::StrokePosition::Center});
  };
  std::vector<Element> cuts{
      cut(path::Outline::rectangle(path::Rect::of({34, 34}, {444, 444})), 7),
      cut(path::Outline::rectangle(path::Rect::of({52, 52}, {408, 408})), 3)};
  for (const float radius : {44.0f, 150.0f, 172.0f})
    cuts.push_back(cut(
        path::Outline::svg(
            "M" + std::to_string(middle - radius) + " " +
            std::to_string(middle) + " a" + std::to_string(radius) + " " +
            std::to_string(radius) + " 0 1 0 " + std::to_string(2 * radius) +
            " 0 a" + std::to_string(radius) + " " + std::to_string(radius) +
            " 0 1 0 " + std::to_string(-2 * radius) + " 0 Z"),
        radius < 100 ? 5.0f : 4.0f));
  // Eight petals between the inner ring and the outer pair, each a lens
  // with a vein down its middle.
  for (int petal = 0; petal < 8; ++petal) {
    const float turn = float(petal) * kPi / 4;
    const glm::vec2 along{std::cos(turn), std::sin(turn)};
    const glm::vec2 across{-along.y, along.x};
    const glm::vec2 root = glm::vec2(middle) + along * 52.0f;
    const glm::vec2 tip = glm::vec2(middle) + along * 142.0f;
    const glm::vec2 bulgeA =
        glm::vec2(middle) + along * 100.0f + across * 34.0f;
    const glm::vec2 bulgeB =
        glm::vec2(middle) + along * 100.0f - across * 34.0f;
    const auto at = [](glm::vec2 p) {
      return std::to_string(p.x) + " " + std::to_string(p.y);
    };
    cuts.push_back(cut(
        path::Outline::svg("M" + at(root) + " Q" + at(bulgeA) + " " + at(tip) +
                           " Q" + at(bulgeB) + " " + at(root) + " Z"),
        4));
    cuts.push_back(cut(path::Outline::svg("M" + at(root + along * 14.0f) +
                                          " L" + at(tip - along * 16.0f)),
                       2));
  }
  return compose::stack()
      .width(size)
      .height(size)
      .fill(field)
      .children(std::move(cuts))
      .filter(material::Filter::blur(softness));
}

/** THE TILE'S PAGE: glaze over the incisions, pooled where they are cut,
 *  with the cuts' own shoulders in the surface's normal. */
Element tilePage(const Glaze& glaze, const material::Texture& cut,
                 const material::Texture& depth,
                 const material::Material& crazed,
                 const material::Material& fine,
                 const material::Texture& coatNormal) {
  return compose::stack().width(kTilePage).height(kTilePage).children(
      {compose::kit::at(0, 0, kTilePage, kTilePage)
           .fill(
               glazed(glaze, material::image(depth), crazed, fine, coatNormal,
                      material::surface::normalFromHeight(
                          material::image(cut), {.depth = 5, .step = 1.2f})))});
}

/** A studio of one broad soft box high on the left and a narrow strip on
 *  the right over a graded room, for the coat to reflect. */
world::EnvironmentMap studio() {
  return world::kit::environmentMap(
      {.zenith = {.62f, .66f, .74f},
       .nadir = {.08f, .07f, .06f},
       .exponent = 1.4f,
       .softBoxes = {{.centre = {.30f, .36f},
                      .size = {.10f, .14f},
                      .radiance = {5.5f, 5.2f, 4.6f}},
                     {.centre = {.74f, .42f},
                      .size = {.03f, .22f},
                      .radiance = {1.8f, 2.1f, 2.6f}}}});
}

struct Specimen {
  sketch::Page jarPage;
  sketch::Page tilePage;
  Element jarTree = compose::stack();
  Element tileTree = compose::stack();
};

struct CeramicGlaze {
  std::vector<Specimen> specimens;
  world::EnvironmentMap room;
  gm::Mesh jar, tileTop, tileSides, stilt, ground;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {0, 700, 1300};
    camera.target = {0, 110, 90};
    camera.verticalFieldOfViewDegrees = 31;
    camera.nearPlane = 40;
    camera.farPlane = 6000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 2.5,
                             .background = hexColor(0x0e0f11)});
    const Profile thrown = throwJar();
    jar = gm::revolve(thrown.points, {.segments = 160});
    tileTop = gm::box({-kTileSize / 2, 0, -kTileSize / 2},
                      {kTileSize / 2, kTileThickness, kTileSize / 2},
                      {.front = false,
                       .back = false,
                       .right = false,
                       .left = false,
                       .bottom = false});
    tileSides = gm::box({-kTileSize / 2, 0, -kTileSize / 2},
                        {kTileSize / 2, kTileThickness, kTileSize / 2},
                        {.top = false, .bottom = false});
    ground = gm::quad(16000, 9000);
    const float under = 0.9f * kTileSize * std::sin(kTileTilt * kPi / 180);
    stilt = gm::box({-30, 0, -14}, {30, under, 14});
    room = studio();

    const glm::ivec2 jarSize{kJarPageWidth, kJarPageHeight};
    const glm::ivec2 tileSize{kTilePage, kTilePage};
    const Color white{1, 1, 1, 1}, black{0, 0, 0, 1},
        ordinary{.48f, .48f, .48f, 1};
    const auto cut = paintedOnce(ctx, tileSize, incisions(1.4f, white, black),
                                 material::Repeat::Pad);
    const auto pooled = paintedOnce(
        ctx, tileSize, incisions(6.0f, ordinary, white), material::Repeat::Pad);
    const auto filled = paintedOnce(
        ctx, tileSize, incisions(5.0f, white, black), material::Repeat::Pad);
    // The glaze's depth is painted once into a strip as tall as the page,
    // repeated across it, so a mask reading it samples an image rather
    // than walking the gradient's stops at every pixel of every map.
    constexpr int kStrip = 8;
    const material::Material depth =
        material::image(paintedOnce(ctx, {kStrip, kJarPageHeight},
                                    compose::stack()
                                        .width(kStrip)
                                        .height(kJarPageHeight)
                                        .fill(depthUpThePage(thrown)),
                                    material::Repeat::Repeat));
    for (const Glaze& glaze : kGlazes) {
      Specimen specimen;
      // The fine crazing runs between the wide cracks at a smaller
      // spacing, as a second, later network.
      const auto jarCrazing = [&](bool tipped, float scale = 1,
                                  uint64_t other = 0) {
        return paintedOnce(
            ctx, jarSize,
            crazing(kJarPageWidth, kJarPageHeight, glaze.cellSpacing * scale,
                    glaze.seed + other, true, tipped),
            material::Repeat::Repeat);
      };
      const auto tileCrazing = [&](bool tipped, float scale = 1,
                                   uint64_t other = 0) {
        return paintedOnce(
            ctx, tileSize,
            crazing(kTilePage, kTilePage, glaze.cellSpacing * 1.1f * scale,
                    glaze.seed + 101 + other, false, tipped),
            material::Repeat::Pad);
      };
      const material::Texture jarCoat = paintedOnce(
          ctx, jarSize,
          coatNormal(kJarPageWidth, kJarPageHeight, jarCrazing(true)),
          material::Repeat::Repeat);
      // The coat lies over the tile's incisions, softened by the glaze
      // that fills them, and is crazed over that.
      const material::Texture tileCoat = paintedOnce(
          ctx, tileSize,
          coatNormal(kTilePage, kTilePage, tileCrazing(true),
                     material::surface::normalFromHeight(
                         material::image(filled), {.depth = 4, .step = 1.5f})),
          material::Repeat::Pad);
      specimen.jarPage = ctx.page(jarSize);
      specimen.tilePage = ctx.page(tileSize);
      specimen.jarTree =
          jarPage(glaze, thrown, depth, material::image(jarCrazing(false)),
                  material::image(jarCrazing(false, .42f, 7)), jarCoat);
      specimen.tileTree =
          tilePage(glaze, cut, pooled, material::image(tileCrazing(false)),
                   material::image(tileCrazing(false, .42f, 7)), tileCoat);
      specimens.push_back(std::move(specimen));
    }
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;
    // The jars turn once each loop, so their crazing passes under the
    // key's reflection; the key glides across the front and back.
    const float turn = 360.0f * seconds / kLoop;
    const glm::vec3 keyAt{-260 + 460 * std::sin(phase), 820, -180};

    std::vector<world::Node> nodes{
        world::camera(lens(), {.exposure = 0.95f,
                               .toneCurve = world::ToneCurve::Neutral}),
        world::null().key("table").at({0, 110, 80}),
        world::hemisphereLight({.color = hexColor(0xc9d3e2),
                                .groundColor = hexColor(0x3a3029),
                                .intensity = 0.12f})
            .key("sky"),
        world::rectLight({0, -1, 0},
                         {.width = 380,
                          .height = 240,
                          .color = hexColor(0xfff1df),
                          .intensity = 15.0f,
                          .castShadow = true,
                          .shadow = {.bias = 3, .normalBias = 2, .radius = 4}})
            .key("key")
            .at(keyAt)
            .lookAt("table"),
        world::mesh(ground).key("slab").rotateX(-90).fill(
            material::surface::program(
                {.baseColor = hexColor(0x3e3b37), .roughness = .75f})),
    };
    std::vector<world::Pass> passes;
    for (size_t i = 0; i < specimens.size(); ++i) {
      const Specimen& specimen = specimens[i];
      const Glaze& glaze = kGlazes[i];
      const float x = kJarSpacing * (float(i) - 1);
      nodes.push_back(world::mesh(jar)
                          .key(std::string(glaze.name) + " jar")
                          .at({x, 0, -60})
                          .rotateY(turn + 120.0f * float(i))
                          .fill(specimen.jarPage.surface()));
      // THE TILE leans back on a kiln stilt, its front edge on the slab,
      // tipped toward the eye far enough to mirror the key overhead.
      const std::string name = glaze.name;
      const float lift = 0.5f * kTileSize * std::sin(kTileTilt * kPi / 180);
      nodes.push_back(
          world::group({world::group({world::mesh(tileTop)
                                          .key(name + " tile")
                                          .fill(specimen.tilePage.surface()),
                                      world::mesh(tileSides)
                                          .key(name + " tile edge")
                                          .fill(clayEdge(glaze))})
                            .key(name + " tile tipped")
                            .at({0, lift, 0})
                            .rotateX(kTileTilt),
                        world::mesh(stilt)
                            .key(name + " stilt")
                            .at({0, 0, -0.4f * kTileSize})
                            .fill(clayEdge(glaze))})
              .key(name + " tile stand")
              .at({x, 0, 320})
              .rotateY(-8 + 8 * float(i)));
      passes.push_back(specimen.jarPage.render(specimen.jarTree));
      passes.push_back(specimen.tilePage.render(specimen.tileTree));
    }
    world::Frame frame(world::group(std::move(nodes)));
    frame.environment(room, {.intensity = 0.5f, .diffuse = 0.4f});
    for (auto& pass : passes) frame.pass(std::move(pass));
    return frame;
  }
};

}  // namespace

SIGIL_SKETCH(CeramicGlaze, "Study · Materials",
             "Celadon, oxblood and amber, each on a thrown jar and an incised "
             "tile: a clear coat over crazing that breaks up its reflections, "
             "colour pooling darker in every recess, and a bare foot")
