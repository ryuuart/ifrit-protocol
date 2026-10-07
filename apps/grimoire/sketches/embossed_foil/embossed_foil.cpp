/** A gold thermal blanket on a dark bench, crinkled into facets, quilted
 *  into pillows, hemmed in silver tape and pressed with a word, its corner
 *  folded back to show the silver beneath; a soft panel travels behind it
 *  on a rail, so every crease flashes and goes dark as the panel and the
 *  fold move, while the die-smoothed letters hold one clean reflection.
 */
// TAGS: Compose/Materials, Materials/Metal, Materials/Lighting,
// Geometry/3D, Typography/Relief

#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilweave/layout/TextContext.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::hexColor;

constexpr float kLoop = 14, kPi = 3.14159265358979f;
constexpr material::Color kBlack{0, 0, 0, 1}, kWhite{1, 1, 1, 1};

/** THE BLANKET, in page pixels, which are also scene units. */
constexpr float kWidth = 1500, kDepth = 1000;
/** How far along each edge the folded corner reaches. */
constexpr float kCorner = 420;
/** The folded corner's hinge length and its reach from the hinge. */
const float kHinge = kCorner * std::sqrt(2.0f);
const float kReach = kHinge / 2;
/** How wide the silver hem tape is, and how far apart the quilting's
 *  stitches stand. */
constexpr float kHem = 34, kQuilt = 300;

material::Color grey(float value) { return {value, value, value, 1}; }

/** The die's lettering at @p size, centred on @p centre in page pixels. */
path::Outline lettering(weave::TextContext& text, const weave::Face& face,
                        std::u8string_view words, float size, float tracking,
                        glm::vec2 centre) {
  weave::TextStyle style;
  style.shaping.typeface = face;
  style.shaping.fontSize = size;
  style.shaping.letterSpacing = tracking;
  const auto outline =
      text.singleLine(words, style, {0, 0}).layout().glyphOutline();
  return outline.transformed(
      path::Transform::translate(centre - outline.bounds().centre()));
}

/** A line laid into a height map: a band of @p width along @p spine,
 *  tapering at both ends, at @p level. */
Element crease(const std::string& spine, glm::vec2 page, float width,
               float level) {
  return compose::kit::at(0, 0, page.x, page.y)
      .shape(compose::heldPath(path::Outline::svg(spine)))
      .stroke(compose::brush::ribbon(
          path::Profile{{0, .2f}, {.2f, width}, {.7f, width * .8f}, {1, .2f}},
          compose::Fill::color(grey(level))));
}

/** A WANDERING CREASE: a few short legs that turn a little at each
 *  joint, as a foil sheet creases when it is handled. */
std::string wander(std::mt19937& random, glm::vec2 page, float reach) {
  std::uniform_real_distribution<float> unit(0, 1);
  glm::vec2 at{unit(random) * page.x, unit(random) * page.y};
  float heading = unit(random) * 2 * kPi;
  std::string spine = "M" + std::to_string(at.x) + " " + std::to_string(at.y);
  const int legs = 2 + int(unit(random) * 4);
  for (int leg = 0; leg < legs; ++leg) {
    heading += (unit(random) - .5f) * .9f;
    at += glm::vec2(std::cos(heading), std::sin(heading)) * reach *
          (.4f + unit(random));
    spine += " L" + std::to_string(at.x) + " " + std::to_string(at.y);
  }
  return spine;
}

/** THE CRINKLE OF A FOIL SHEET @p page across, white highest: broad
 *  creased facets, finer ones and a grain, then the drawn creases — light
 *  ridges and dark valleys — and @p folds long soft folds. */
std::vector<Element> crinkle(glm::vec2 page, unsigned seed, int creases,
                             const std::vector<std::string>& folds) {
  const auto facets =
      material::from(material::noise(.0042f, {.octaves = 3,
                                              .seed = float(seed),
                                              .turbulence = true,
                                              .grain = true,
                                              .contrast = 1.6f}))
          .layer(material::noise(.016f, {.octaves = 3,
                                         .seed = float(seed) + 7,
                                         .turbulence = true,
                                         .grain = true,
                                         .contrast = 1.4f}),
                 {.opacity = .42f})
          .layer(
              material::noise(
                  .07f, {.octaves = 2, .seed = float(seed) + 3, .grain = true}),
              {.opacity = .12f});
  std::vector<Element> marks{
      compose::kit::at(0, 0, page.x, page.y).fill(facets)};
  std::mt19937 random(seed);
  for (int index = 0; index < creases; ++index) {
    // A crease is a change of slope: a soft ridge or valley with a
    // sharp line along its spine.
    const bool ridge = index % 2 == 0;
    const float level = ridge ? 1.0f : 0.0f;
    const auto spine = wander(random, page, 120);
    marks.push_back(crease(spine, page, 18, level)
                        .opacity(.32f)
                        .filter(material::Filter::blur(5)));
    marks.push_back(crease(spine, page, 1.6f, level)
                        .opacity(.3f)
                        .filter(material::Filter::blur(.7f)));
  }
  for (const auto& fold : folds) {
    marks.push_back(crease(fold, page, 34, 1)
                        .opacity(.5f)
                        .filter(material::Filter::blur(12)));
    marks.push_back(crease(fold, page, 3, 1)
                        .opacity(.7f)
                        .filter(material::Filter::blur(1.2f)));
  }
  return marks;
}

/** THE GOLD BLANKET'S HEIGHT: the crinkle, the hem pressed flat and
 *  stitched, a stitch at every quilting tack, and the letter die pressed
 *  deepest of all, flat at the bottom, so the crinkle is gone inside it. */
Element blanketHeight(const path::Outline& word, const path::Outline& line) {
  const glm::vec2 page{kWidth, kDepth};
  auto marks = crinkle(page, 11, 260,
                       {"M-40 260 C300 180 620 330 980 220 S1400 120 1560 190",
                        "M180 1040 C320 760 520 640 760 600 S1180 520 1540 300",
                        "M-20 820 C220 760 380 840 560 1040"});
  // The hem: pressed flat under the tape, with a row of stitches.
  const auto band = [&](float left, float top, float width, float height) {
    return compose::kit::at(left, top, width, height).fill(grey(.42f));
  };
  marks.push_back(band(0, 0, kWidth, kHem));
  marks.push_back(band(0, kDepth - kHem, kWidth, kHem));
  marks.push_back(band(0, 0, kHem, kDepth));
  marks.push_back(band(kWidth - kHem, 0, kHem, kDepth));
  std::string stitches;
  const auto dash = [&](glm::vec2 from, glm::vec2 to) {
    stitches += "M" + std::to_string(from.x) + " " + std::to_string(from.y) +
                " L" + std::to_string(to.x) + " " + std::to_string(to.y) + " ";
  };
  for (float x = kHem; x < kWidth - kHem; x += 22) {
    dash({x, kHem + 6}, {x + 11, kHem + 6});
    dash({x, kDepth - kHem - 6}, {x + 11, kDepth - kHem - 6});
  }
  for (float y = kHem; y < kDepth - kHem; y += 22) {
    dash({kHem + 6, y}, {kHem + 6, y + 11});
    dash({kWidth - kHem - 6, y}, {kWidth - kHem - 6, y + 11});
  }
  // A cross stitch at every quilting tack.
  for (float x = kQuilt; x < kWidth; x += kQuilt)
    for (float y = kQuilt * .5f; y < kDepth; y += kQuilt) {
      dash({x - 9, y - 9}, {x + 9, y + 9});
      dash({x - 9, y + 9}, {x + 9, y - 9});
    }
  marks.push_back(compose::kit::at(0, 0, kWidth, kDepth)
                      .shape(compose::heldPath(path::Outline::svg(stitches)))
                      .stroke(kBlack, {.width = 3})
                      .opacity(.8f)
                      .filter(material::Filter::blur(1.2f)));
  // THE DIE, pressed to the bottom and softened into a shoulder.
  for (const auto* letters : {&word, &line})
    marks.push_back(compose::kit::at(0, 0, kWidth, kDepth)
                        .shape(compose::heldPath(*letters))
                        .fill(kBlack)
                        .filter(material::Filter::blur(2.4f)));
  return compose::stack().width(kWidth).height(kDepth).fill(kBlack).children(
      std::move(marks));
}

/** THE SILVER CORNER'S HEIGHT: the same foil handled harder, so its
 *  creases are closer and its folds run along the hinge. */
Element cornerHeight() {
  const glm::vec2 page{kHinge, kReach};
  return compose::stack().width(page.x).height(page.y).fill(kBlack).children(
      crinkle(page, 29, 140,
              {"M-20 40 C180 70 380 20 620 60",
               "M120 320 C200 200 330 150 420 -20"}));
}

material::Material image(const std::shared_ptr<compose::TextureScene>& scene) {
  return material::image(scene->texture()
                             .tile(material::Repeat::Pad)
                             .sampling(material::Sampling::Linear));
}

/** A FOIL'S SURFACE over @p height: metal all through, its roughness
 *  following the crinkle — burnished on the ridges, duller in the
 *  creases — its normal the height's, and a faint sheen where the fabric
 *  backing shows through at grazing angles. */
material::Material foil(material::Material base,
                        const material::Material& height, float smooth,
                        float dull, material::Color sheen) {
  const auto roughness =
      material::from(grey(dull))
          .layer(grey(smooth),
                 {.mask = material::Mask{height,
                                         material::ColorChannel::Luminance}});
  return base.surface({.metallic = 1.0f,
                       .roughness = roughness,
                       .normal = material::surface::normalFromHeight(
                           height, {.depth = 9, .step = .8f, .directX = true}),
                       .normalDirectX = true,
                       .sheenColor = sheen,
                       .sheenRoughness = .55f});
}

/** THE GOLD PAGE: the blanket's foil, the die-smoothed letters, the
 *  silver hem tape and a two-line caption in flat ink. */
Element goldPage(const material::Material& height, const path::Outline& word,
                 const path::Outline& line) {
  const auto tint = [](material::Color colour, float seed) {
    return material::from(colour).layer(
        material::noise(.006f, {.octaves = 3, .seed = seed, .grain = true}),
        {.blend = material::BlendMode::Multiply, .opacity = .22f});
  };
  const auto gold =
      foil(tint(hexColor(0xe8b04f), 4), height, .07f, .34f, hexColor(0x5a3c18));
  // Inside the die the foil is pressed smooth: one roughness, deeper gold.
  const auto pressed =
      tint(hexColor(0xd99636), 4)
          .surface({.metallic = 1.0f,
                    .roughness = .1f,
                    .normal = material::surface::normalFromHeight(
                        height, {.depth = 9, .step = .8f, .directX = true}),
                    .normalDirectX = true});
  const auto tape =
      foil(tint(hexColor(0xd7dade), 9), height, .2f, .34f, hexColor(0x30343a));
  std::vector<Element> content;
  for (const auto* letters : {&word, &line})
    content.push_back(compose::kit::at(0, 0, kWidth, kDepth)
                          .shape(compose::heldPath(*letters))
                          .fill(pressed));
  const auto band = [&](float left, float top, float width, float height) {
    return compose::kit::at(left, top, width, height).fill(tape);
  };
  content.push_back(band(0, 0, kWidth, kHem));
  content.push_back(band(0, kDepth - kHem, kWidth, kHem));
  content.push_back(band(0, 0, kHem, kDepth));
  content.push_back(band(kWidth - kHem, 0, kHem, kDepth));
  const auto caption = [](std::string_view words, float top, float size) {
    return compose::kit::at(compose::text(std::string(words)), 80, top, 900,
                            size * 1.6f)
        .fontSize(size)
        .letterSpacing(size * .14f)
        .ink(hexColor(0x2a1d0c));
  };
  content.push_back(caption("EMBOSSED FOIL", kDepth - 124, 22));
  content.push_back(caption(
      "crinkled gold · a folded silver corner · a travelling soft panel",
      kDepth - 92, 16));
  return compose::stack().width(kWidth).height(kDepth).fill(gold).children(
      std::move(content));
}

/** THE SILVER PAGE the folded corner shows. */
Element silverPage(const material::Material& height) {
  return compose::stack().width(kHinge).height(kReach).fill(foil(
      material::from(hexColor(0xdadde2))
          .layer(
              material::noise(.01f, {.octaves = 3, .seed = 5, .grain = true}),
              {.blend = material::BlendMode::Multiply, .opacity = .14f}),
      height, .08f, .3f, hexColor(0x3a4048)));
}

float smoothstep(float edge0, float edge1, float value) {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

/** THE BLANKET'S RISE above the bench at a point of its face, in its own
 *  coordinates — x across, y from the front edge back: quilted pillows
 *  between the tacks, two broad folds, all of it settling to the bench at
 *  the edges and along the cut where the corner was folded away. */
float rise(float x, float y) {
  const float across = x + kWidth / 2, back = y + kDepth / 2;
  const float toEdge =
      std::min({across, kWidth - across, back, kDepth - back,
                // the cut: the line from (W - corner, 0) to (W, corner)
                (kWidth - across + back - kCorner) / std::sqrt(2.0f)});
  const float settle = smoothstep(0, 140, toEdge);
  const float pillowX = std::sin(kPi * across / kQuilt);
  const float pillowY = std::sin(kPi * (back - kQuilt * .5f) / kQuilt);
  const float pillows = 20 * pillowX * pillowX * pillowY * pillowY;
  const auto ridge = [](float distance, float width) {
    return std::exp(-distance * distance / (width * width));
  };
  const float firstFold =
      ridge(back - (620 + .22f * x + 50 * std::sin(x / 260)), 95);
  const float secondFold =
      ridge(across * .7f + back - (1350 + 40 * std::sin(back / 210)), 80);
  return settle * (5 + pillows + 30 * firstFold + 24 * secondFold);
}

/** THE BLANKET: a sheet over its face with the front-right corner cut
 *  away, lying in its own xy plane facing +z, risen by `rise`, with UVs
 *  over the page in page pixels. */
gm::Mesh blanket() {
  auto sheet = gm::grid(220, 150, [](float u, float v) -> glm::vec3 {
    const float back = v * kDepth;
    // A row near the front is shortened by the cut.
    const float cut = std::max(0.0f, kCorner - back);
    const float x = -kWidth / 2 + u * (kWidth - cut);
    const float y = back - kDepth / 2;
    return {x, y, rise(x, y)};
  });
  for (size_t index = 0; index < sheet.positions.size(); ++index)
    sheet.uvs[index] = {sheet.positions[index].x / kWidth + .5f,
                        .5f - sheet.positions[index].y / kDepth};
  return sheet;
}

/** THE SILVER CORNER: the triangle the cut took, laid out from its hinge
 *  along x through the origin and reaching toward +z, its silver face
 *  toward -y so it shows once folded over, and curled a little away from
 *  that face toward its tip. */
gm::Mesh corner() {
  auto flap = gm::grid(48, 32, [](float u, float v) -> glm::vec3 {
    return {(u - .5f) * kHinge * (1 - v), -26 * v * v, v * kReach};
  });
  for (size_t index = 0; index < flap.positions.size(); ++index)
    flap.uvs[index] = {flap.positions[index].x / kHinge + .5f,
                       flap.positions[index].z / kReach};
  return flap;
}

/** THE STUDIO SWEEP the blanket lies on: a dark floor that curves up
 *  into a wall behind it, so there is no horizon. */
gm::Mesh sweep() {
  constexpr float floorFront = 5000, floorBack = 2400, bend = 900, wall = 4000;
  const float floorLength = floorFront + floorBack;
  const float bendLength = kPi / 2 * bend;
  const float length = floorLength + bendLength + wall;
  return gm::grid(4, 160, [=](float u, float v) -> glm::vec3 {
    const float x = (u - .5f) * 20000;
    const float along = v * length;
    if (along < floorLength) return {x, 0, floorFront - along};
    if (along < floorLength + bendLength) {
      const float angle = (along - floorLength) / bend;
      return {x, bend - bend * std::cos(angle),
              -floorBack - bend * std::sin(angle)};
    }
    return {x, bend + along - floorLength - bendLength, -floorBack - bend};
  });
}

/** THE PANEL'S RAIL: a shallow arc high behind the blanket, ridden there
 *  and back once a loop, easing at its ends. */
glm::vec3 railAt(float seconds) {
  const float swing = -std::cos(2 * kPi * seconds / kLoop);
  const float angle = swing * 40 * kPi / 180;
  return {1300 * std::sin(angle), 470, -1300 * std::cos(angle) + 150};
}

struct EmbossedFoil {
  std::shared_ptr<compose::TextureScene> goldHeight, silverHeight;
  sketch::Page gold, silver;
  Element goldTree = compose::stack(), silverTree = compose::stack();
  world::EnvironmentMap room;
  gm::Mesh sheet, flap, bench;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {-60, 620, 1320};
    camera.target = {40, 10, -100};
    camera.fovYDeg = 46;
    camera.zNear = 40;
    camera.zFar = 20000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1440, 900},
                             .captureAt = 4.0,
                             .background = hexColor(0x0b0c0e)});
    weave::TextContext text(ctx.fonts);
    auto face = ctx.fonts.familyTypeface("Futura", {.weight = 700});
    if (!face) face = ctx.fonts.defaultTypeface();
    const auto word =
        lettering(text, face, u8"HELIOS", 230, 34, {kWidth / 2 - 40, 360});
    const auto line = lettering(text, face, u8"THERMAL CONTROL  ·  PANEL 7", 40,
                                10, {kWidth / 2 - 40, 545});

    goldHeight = ctx.textureScene({int(kWidth), int(kDepth)}, kBlack);
    goldHeight->render(blanketHeight(word, line));
    silverHeight = ctx.textureScene({int(kHinge), int(kReach)}, kBlack);
    silverHeight->render(cornerHeight());

    gold = ctx.page({int(kWidth), int(kDepth)});
    silver = ctx.page({int(kHinge), int(kReach)});
    goldTree = goldPage(image(goldHeight), word, line);
    silverTree = silverPage(image(silverHeight));

    sheet = blanket();
    flap = corner();
    bench = sweep();
    room = world::kit::environmentMap(
        {.zenith = {.12f, .13f, .15f},
         .nadir = {.02f, .02f, .025f},
         .softBoxes = {{.centre = {.2f, .36f},
                        .size = {.08f, .14f},
                        .radiance = {4.2f, 3.7f, 3.0f}},
                       {.centre = {.62f, .3f},
                        .size = {.16f, .05f},
                        .radiance = {2.6f, 2.6f, 2.8f}},
                       {.centre = {.8f, .44f},
                        .size = {.04f, .2f},
                        .radiance = {1.2f, 1.6f, 2.3f}}}});
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const glm::vec3 panel = railAt(seconds);
    const glm::vec3 aim{0, 0, 40};
    // The corner lifts off the gold and settles back twice a loop.
    const float lift = .5f - .5f * std::cos(4 * kPi * seconds / kLoop);
    const float fold = 176 - 20 * lift;
    // The hinge: the cut from (W/2 - corner, front) to (W/2, front -
    // corner), in the scene, where the blanket has settled to the bench.
    const glm::vec3 hinge{kWidth / 2 - kCorner / 2, 6,
                          kDepth / 2 - kCorner / 2};
    return world::Frame(
               world::group(
                   {world::camera(lens(),
                                  {.exposure = .95f,
                                   .toneCurve = world::ToneCurve::Neutral}),
                    // THE PANEL: a soft rect light on its rail behind the
                    // blanket, turned each frame to face it, so its
                    // reflection runs across the foil toward the eye.
                    world::kit::visibleLight(
                        world::rectLight(glm::normalize(aim - panel),
                                         {.width = 560,
                                          .height = 190,
                                          .color = hexColor(0xfff2e0),
                                          .intensity = 10.0f,
                                          .castShadow = true,
                                          .shadow = {.bias = 3,
                                                     .normalBias = 2,
                                                     .radius = 3,
                                                     .mapSize = 2048}})
                            .key("panel"))
                        .key("panel rig")
                        .at(panel),
                    // THE FILL: a broad, dim, cool panel low at the front
                    // left, so the creases turned away still read.
                    world::rectLight(glm::normalize(glm::vec3{.8f, -.5f, -1}),
                                     {.width = 600,
                                      .height = 400,
                                      .color = hexColor(0x9fb8e8),
                                      .intensity = .08f})
                        .key("fill")
                        .at({-1400, 600, 1300}),
                    world::mesh(bench).key("sweep").fill(
                        material::from(hexColor(0x101113))
                            .surface({.roughness = .7f})),
                    world::mesh(sheet)
                        .key("blanket")
                        .at({0, 1.5f, 0})
                        .rotateX(-90)
                        .fill(gold.surface()),
                    world::group({world::group({world::mesh(flap)
                                                    .key("corner")
                                                    .rotateX(-fold)
                                                    .fill(silver.surface())})
                                      .key("corner turn")
                                      .rotateY(45)})
                        .key("corner hinge")
                        .at(hinge)}))
        .environment(room, {.intensity = .45f, .diffuse = .2f})
        .pass(world::bloom({.threshold = 1.4f, .intensity = .35f}))
        .pass(gold.render(goldTree))
        .pass(silver.render(silverTree));
  }
};

}  // namespace

SIGIL_SKETCH(EmbossedFoil, "Study · Materials",
             "A crinkled gold thermal blanket pressed with a word and folded "
             "back at one silver corner, under a soft panel travelling on a "
             "rail, so every crease flashes and goes dark as it passes")
