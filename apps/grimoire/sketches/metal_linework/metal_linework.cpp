/** Plated conductor traces on a dark glass-weave board, brushed along
 *  their own direction so each highlight is drawn out across the trace
 *  and turns with it through every bend as the key light swings; vias
 *  stand as plated rings, solder beads sit on the connector pads, and one
 *  silver line carries a pulse of light along its length.
 */
// TAGS: Compose/Materials, Materials/Metal, Materials/Lighting,
// Geometry/3D

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/verbs/Decoration.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/curve/Curve.h>
#include <sigilgeometry/mesh/pop/Builder.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Stroke.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <memory>
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
using material::hexColor;

constexpr float kLoop = 12, kPi = 3.14159265358979f;
constexpr material::Color kBlack{0, 0, 0, 1};

/** THE BOARD, in page pixels, which are also scene units. */
constexpr float kWidth = 1600, kDepth = 1000;
/** How high the plating stands above the board, in scene units. */
constexpr float kPlating = 2;

material::Color grey(float value) { return {value, value, value, 1}; }

/** A TRACE as it is routed: the corners it turns at, how wide it is,
 *  whether it is copper or silver, and how round its bends are. */
struct Trace {
  std::vector<glm::vec2> corners;
  float width;
  bool copper;
  float bend;
};

/** A pad where a trace begins or ends: a disc, holed when a via stands
 *  on it, beaded when a lead is soldered to it. */
struct Pad {
  glm::vec2 at;
  float radius;
  bool copper;
  bool via;
  bool solder;
};

/** THE BUS: four silver lines leaving the connector, dropping through a
 *  forty-five degree bend together and ending at a staggered row of
 *  vias. Perpendicular spacing holds through the bend. */
std::vector<Trace> bus() {
  std::vector<Trace> lines;
  for (int line = 0; line < 4; ++line) {
    const float top = 300 + 40.0f * float(line);
    const float diagonal = 320 - 40 * std::sqrt(2.0f) * float(line);
    const float end = line % 2 ? 1340 : 1280;
    lines.push_back({{{150, top},
                      {diagonal + top, top},
                      {diagonal + top + 300, top + 300},
                      {end, top + 300}},
                     14,
                     false,
                     24});
  }
  return lines;
}

/** THE POWER RUN: a broad copper line along the front that steps back
 *  through one bend to a stitched pad. */
Trace power() {
  return {{{130, 880}, {1000, 880}, {1090, 790}, {1460, 790}}, 46, true, 60};
}

/** THE MEANDER: a copper signal that leaves a pad at the back left,
 *  runs right, then winds through four U-turns to a via — the shape a
 *  router uses to lengthen a line, every turn a half circle. */
Trace meander() {
  std::vector<glm::vec2> corners{{160, 140}, {1040, 140}};
  float x = 1040;
  for (int turn = 0; turn < 4; ++turn) {
    corners.push_back({x, 262});
    corners.push_back({x + 48, 262});
    corners.push_back({x + 48, 140});
    corners.push_back({x + 96, 140});
    x += 96;
  }
  corners.back() = {1500, 140};
  return {corners, 16, true, 24};
}

/** THE COIL: a copper spiral inductor between the bus and the power
 *  run, three and a half turns out from a via at its heart, then a
 *  straight lead to a pad — every part of it brushed round the way a
 *  spun disc is. */
constexpr glm::vec2 kCoilCentre{430, 650};
Trace coil() {
  std::vector<glm::vec2> corners;
  constexpr float kTurns = 3.5f, kInner = 34, kPitch = 30;
  const int steps = int(kTurns * 72);
  for (int step = 0; step <= steps; ++step) {
    const float turned = float(step) / 72;
    const float angle = 2 * kPi * turned;
    const float radius = kInner + kPitch * turned;
    corners.push_back(kCoilCentre +
                      radius * glm::vec2(std::cos(angle), std::sin(angle)));
  }
  corners.push_back({250, corners.back().y});
  return {corners, 11, true, 0};
}

std::vector<Trace> traces() {
  auto all = bus();
  all.push_back(power());
  all.push_back(meander());
  all.push_back(coil());
  return all;
}

std::vector<Pad> pads() {
  std::vector<Pad> all;
  for (const Trace& line : bus()) {
    all.push_back({line.corners.front(), 17, false, false, true});
    all.push_back({line.corners.back(), 17, false, true, false});
  }
  all.push_back({{130, 880}, 38, true, false, true});
  all.push_back({{1460, 790}, 38, true, true, false});
  for (const float x : {1410.0f, 1510.0f})
    all.push_back({{x, 860}, 17, true, true, false});
  all.push_back({{160, 140}, 16, true, false, true});
  all.push_back({{1500, 140}, 16, true, true, false});
  all.push_back({kCoilCentre, 18, true, true, false});
  all.push_back({coil().corners.back(), 17, true, false, true});
  for (const glm::vec2 at : std::vector<glm::vec2>{
           {1190, 470}, {1250, 530}, {1530, 420}, {760, 200}, {860, 250}})
    all.push_back({at, 15, false, true, false});
  return all;
}

/** THE SPINE a trace is drawn along: its corners joined straight, each
 *  corner rounded by a curve that leaves and rejoins the legs `bend`
 *  pixels from it, sampled finely enough that every short leg holds one
 *  direction. */
std::vector<glm::vec2> spine(const Trace& trace) {
  const auto& corners = trace.corners;
  if (trace.bend <= 0) return corners;
  std::vector<glm::vec2> points{corners.front()};
  for (size_t index = 1; index + 1 < corners.size(); ++index) {
    const glm::vec2 before = corners[index - 1], corner = corners[index],
                    after = corners[index + 1];
    const float reach = std::min({trace.bend, glm::length(corner - before) / 2,
                                  glm::length(after - corner) / 2});
    const glm::vec2 enter = corner - glm::normalize(corner - before) * reach;
    const glm::vec2 leave = corner + glm::normalize(after - corner) * reach;
    constexpr int kSteps = 10;
    for (int step = 0; step <= kSteps; ++step) {
      const float t = float(step) / kSteps;
      points.push_back((1 - t) * (1 - t) * enter + 2 * (1 - t) * t * corner +
                       t * t * leave);
    }
  }
  points.push_back(corners.back());
  return points;
}

std::string svg(const std::vector<glm::vec2>& points) {
  std::string data;
  for (size_t index = 0; index < points.size(); ++index)
    data += (index ? " L" : "M") + std::to_string(points[index].x) + " " +
            std::to_string(points[index].y);
  return data;
}

/** A TRACE'S RUN: @p plating keylined along @p points, @p width wide,
 *  rounded at its ends and its bends. The plating's relief and brushing
 *  are cut from that same band. */
Element run(const std::vector<glm::vec2>& points, float width,
            material::Material plating) {
  return compose::kit::at(0, 0, kWidth, kDepth)
      .shape(compose::heldPath(path::Outline::svg(svg(points))))
      .stroke(std::move(plating), {.width = width,
                                   .position = material::StrokePosition::Center,
                                   .cap = path::Cap::Round,
                                   .join = path::Join::Round});
}

Element disc(glm::vec2 centre, float radius, compose::Fill fill) {
  return compose::kit::at(centre.x - radius, centre.y - radius, 2 * radius,
                          2 * radius)
      .borderRadius(radius)
      .fill(std::move(fill));
}

Element disc(glm::vec2 centre, float radius, material::Material finish) {
  return compose::kit::at(centre.x - radius, centre.y - radius, 2 * radius,
                          2 * radius)
      .borderRadius(radius)
      .fill(std::move(finish));
}

/** THE BOARD'S HEIGHT, white highest: the glass weave low and faint,
 *  each pad a disc with a soft shoulder, each via's drill a pit. The
 *  traces stand on their own relief, cut from the bands they are keyed
 *  into. */
Element heightPage() {
  const auto weave =
      material::from(grey(.1f))
          .layer(material::noise(.06f, {.octaves = 3,
                                        .seed = 4,
                                        .grain = true,
                                        .contrast = 1.4f,
                                        .stretch = 3}),
                 {.blend = material::BlendMode::Multiply, .opacity = .35f})
          .layer(material::noise(.06f, {.octaves = 3,
                                        .seed = 9,
                                        .grain = true,
                                        .contrast = 1.4f,
                                        .stretch = .33f}),
                 {.blend = material::BlendMode::Multiply, .opacity = .35f});
  std::vector<Element> marks;
  for (const Pad& pad : pads()) {
    marks.push_back(
        disc(pad.at, pad.radius + 3, compose::Fill::color(grey(.5f)))
            .filter(material::Filter::blur(3)));
    marks.push_back(disc(pad.at, pad.radius, compose::Fill::color(grey(.63f)))
                        .filter(material::Filter::blur(1)));
    if (pad.via)
      marks.push_back(
          disc(pad.at, pad.radius * .5f, compose::Fill::color(kBlack)));
  }
  return compose::stack().width(kWidth).height(kDepth).fill(weave).children(
      std::move(marks));
}

material::Material mapImage(const material::Texture& map) {
  return material::image(material::Texture(map)
                             .tile(material::Repeat::Pad)
                             .sampling(material::Sampling::Linear));
}

material::Color copperColour() { return hexColor(0xe08a5a); }
material::Color silverColour() { return hexColor(0xd8dbe0); }

/** A PAD'S PLATING: polished metal over the board's height. */
material::Material padPlating(material::Color colour,
                              const material::Material& normal) {
  return material::from(colour).surface(
      {.metallic = 1.0f, .roughness = .2f, .normal = normal});
}

/** A TRACE'S PLATING: polished metal standing proud of the board on a
 *  soft shoulder cut from the trace's own band, and brushed along it, so
 *  its highlight turns with the trace through every bend. */
material::Material tracePlating(material::Color colour) {
  return material::from(colour)
      .surface({.metallic = 1.0f, .roughness = .2f})
      .relief({.shoulder = 4, .depth = .6f})
      .brushed({.strength = .9f});
}

/** THE COLOUR PAGE: the substrate dark and rough, each trace a run of
 *  plating, each pad a polished disc with its drill dark, and two lines
 *  of silkscreen ink. */
Element boardPage(const material::Material& height) {
  const auto normal = material::surface::normalFromHeight(
      height, {.depth = 6, .step = .8f, .directX = true});
  const auto substrate =
      material::from(hexColor(0x26302a))
          .layer(material::noise(.05f, {.octaves = 3,
                                        .seed = 2,
                                        .grain = true,
                                        .contrast = 1.3f,
                                        .stretch = 2.5f}),
                 {.blend = material::BlendMode::Multiply, .opacity = .5f})
          .surface({.roughness = .78f,
                    .normal = normal,
                    .normalDirectX = true,
                    .specular = .6f});
  const auto copper = tracePlating(copperColour());
  const auto silver = tracePlating(silverColour());
  const auto copperPad = padPlating(copperColour(), normal);
  const auto silverPad = padPlating(silverColour(), normal);
  std::vector<Element> content;
  for (const Trace& trace : traces())
    content.push_back(
        run(spine(trace), trace.width, trace.copper ? copper : silver));
  for (const Pad& pad : pads()) {
    content.push_back(
        disc(pad.at, pad.radius, pad.copper ? copperPad : silverPad));
    if (pad.via)
      content.push_back(
          disc(pad.at, pad.radius * .5f,
               material::from(hexColor(0x050505)).surface({.roughness = .9f})));
  }
  const auto silkscreen =
      material::from(hexColor(0xd9d6cc)).surface({.roughness = .85f});
  const auto caption = [&](std::string words, float top, float size) {
    return compose::kit::at(compose::text(std::move(words)), 250, top, 1100,
                            size * 1.6f)
        .fontSize(size)
        .letterSpacing(size * .16f)
        .ink(silkscreen, compose::PaintBox::Glyph);
  };
  content.push_back(caption("METAL LINEWORK", 924, 24));
  content.push_back(
      caption("brushed along the trace  ·  plated vias  ·  a pulse on line 0",
              960, 15));
  return compose::stack().width(kWidth).height(kDepth).fill(substrate).children(
      std::move(content));
}

/** A page point on the board's top, in the scene. */
glm::vec3 onBoard(glm::vec2 at, float lift) {
  return {at.x - kWidth / 2, lift, at.y - kDepth / 2};
}

/** A VIA'S BARREL AND RING: a plated annulus standing on its pad, its
 *  rim rounded, lathed about the up axis. */
gm::Mesh ring(float outer) {
  const float inner = outer * .5f, height = outer * .5f;
  return gm::revolve(std::vector<glm::vec2>{{inner, -2},
                                            {inner, height * .7f},
                                            {inner + 1.5f, height * .95f},
                                            {(inner + outer) / 2, height},
                                            {outer - 1.5f, height * .95f},
                                            {outer, height * .7f},
                                            {outer, -2}},
                     {.segments = 64});
}

struct MetalLinework {
  sketch::Page board;
  Element boardTree = compose::stack();
  world::EnvironmentMap room;
  gm::Mesh slab, floor;
  /** Per pad, the via ring or solder bead standing on it, if any. */
  std::vector<gm::Mesh> rings, beads;
  gm::curve::Spline3 pulseRail;

  static gm::camera::Camera lens() {
    gm::camera::Camera camera;
    camera.eye = {-30, 1180, 860};
    camera.target = {0, 0, 70};
    camera.verticalFieldOfViewDegrees = 46;
    camera.nearPlane = 40;
    camera.farPlane = 20000;
    return camera;
  }

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1440, 900},
                             .captureAt = 4.0,
                             .background = hexColor(0x0a0b0c)});
    board = ctx.page({int(kWidth), int(kDepth)});
    boardTree = boardPage(mapImage(
        ctx.texture(heightPage(), {int(kWidth), int(kDepth)}, kBlack)));

    slab = gm::box({-kWidth / 2, -24, -kDepth / 2},
                   {kWidth / 2, -.5f, kDepth / 2});
    floor = gm::quad(20000, 20000);
    for (const Pad& pad : pads()) {
      rings.push_back(pad.via ? ring(pad.radius) : gm::Mesh{});
      beads.push_back(
          pad.solder ? gm::superellipsoid({pad.radius * .85f, pad.radius * .55f,
                                           pad.radius * .85f},
                                          2.0f, 40, 24)
                     : gm::Mesh{});
    }
    // The pulse's rail: the first bus line, evenly resampled, standing on
    // the plating, and open, so a window rides it only from end to end.
    gm::curve::Spline3 line;
    line.type = gm::curve::Spline3::Type::Linear;
    for (const glm::vec2 point : spine(bus().front()))
      line.points.push_back(onBoard(point, kPlating + 3));
    pulseRail.points = line.sampleArcLength(400);

    room = world::kit::environmentMap(
        {.zenith = {.1f, .11f, .13f},
         .nadir = {.02f, .02f, .02f},
         .softBoxes = {{.centre = {.25f, .34f},
                        .size = {.1f, .12f},
                        .radiance = {3.6f, 3.2f, 2.7f}},
                       {.centre = {.66f, .3f},
                        .size = {.2f, .04f},
                        .radiance = {2.2f, 2.3f, 2.6f}}}});
  }

  /** THE PULSE at @p head: a short bright window riding the rail and a
   *  longer, dimmer one trailing it. */
  std::vector<world::Node> pulse(float head) const {
    const auto glow = [](float strength) {
      return material::from(hexColor(0xbfe4ff))
          .surface(
              {.emission = hexColor(0x9fd8ff), .emissionStrength = strength});
    };
    const auto bead = gm::superellipsoid({3.4f, 3.4f, 8}, 2.0f, 16, 10);
    return {
        world::mesh(
            gm::pop::on(pulseRail).count(36).window(head, .03f).instances(bead))
            .key("pulse head")
            .fill(glow(22)),
        world::mesh(
            gm::pop::on(pulseRail)
                .count(60)
                .window(head - .03f, .09f)
                .instances(gm::superellipsoid({1.6f, 1.6f, 6}, 2.0f, 12, 8)))
            .key("pulse tail")
            .fill(glow(6))};
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float phase = 2 * kPi * seconds / kLoop;
    // The key rides an arc behind the board, low, from left to right and
    // back, so the plating mirrors it toward the eye.
    const float swing = 1.05f * std::sin(phase);
    const glm::vec3 key =
        glm::normalize(glm::vec3{std::sin(swing), -.42f, std::cos(swing)});
    // The pulse runs the bus line once a loop, its trail entering whole at
    // the connector and its head ending on the via.
    const float travel = seconds / kLoop;
    const float head = .12f + travel * (1 - .12f);

    const auto viaFinish = [](bool copper) {
      return material::from(copper ? copperColour() : silverColour())
          .surface({.metallic = 1.0f, .roughness = .22f, .anisotropy = .6f});
    };
    const auto solderFinish =
        material::from(hexColor(0xc9ccd0))
            .surface({.metallic = 1.0f, .roughness = .1f});
    std::vector<world::Node> nodes{
        world::camera(lens(), {.exposure = 1.15f,
                               .toneCurve = world::ToneCurve::Neutral}),
        world::directionalLight(key, {.color = hexColor(0xfff1e2),
                                      .intensity = 2.6f,
                                      .castShadow = true})
            .key("key"),
        // THE PANEL: a warm soft box high behind the board, where the
        // plating mirrors it toward the eye.
        world::kit::visibleLight(
            world::rectLight(glm::normalize(glm::vec3{.15f, -.75f, 1}),
                             {.width = 700,
                              .height = 160,
                              .color = hexColor(0xffe6c8),
                              .intensity = 6.0f})
                .key("panel"))
            .key("panel rig")
            .at({-180, 820, -1350}),
        world::mesh(floor)
            .key("floor")
            .at({0, -24, 0})
            .rotateX(-90)
            .fill(
                material::from(hexColor(0x0d0e10)).surface({.roughness = .8f})),
        world::mesh(slab).key("slab").fill(
            material::from(hexColor(0x141a17)).surface({.roughness = .7f})),
        world::mesh(gm::quad(kWidth, kDepth))
            .key("board")
            .rotateX(-90)
            .fill(board.surface())};
    const auto all = pads();
    for (size_t index = 0; index < all.size(); ++index) {
      const Pad& pad = all[index];
      const std::string name = std::to_string(index);
      if (pad.via)
        nodes.push_back(world::mesh(rings[index])
                            .key("via " + name)
                            .at(onBoard(pad.at, kPlating))
                            .fill(viaFinish(pad.copper)));
      if (pad.solder)
        nodes.push_back(world::mesh(beads[index])
                            .key("solder " + name)
                            .at(onBoard(pad.at, 0))
                            .fill(solderFinish));
    }
    for (auto& node : pulse(head)) nodes.push_back(std::move(node));
    return world::Frame(world::group(std::move(nodes)))
        .environment(room, {.intensity = .5f, .diffuse = .25f})
        .pass(world::bloom({.threshold = 1.2f, .intensity = .6f, .radius = 18}))
        .pass(board.render(boardTree));
  }
};

}  // namespace

SIGIL_SKETCH(MetalLinework, "Study · Materials",
             "Plated copper and silver traces on a dark board, brushed along "
             "their own direction so each highlight turns with the trace as "
             "the key swings, with plated vias, solder beads and a pulse of "
             "light running one line")
