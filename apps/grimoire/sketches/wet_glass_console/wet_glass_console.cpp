/** A dark telemetry console under a rained-on sheet of glass: its page
 *  keeps changing, and every bead, rivulet and wipe ridge on the glass is
 *  a body of water that bends the page seen through it, under a cool key
 *  light, a drifting panel light and a room.
 */
// TAGS: Materials/Glass, Materials/Water, Materials/Lighting,
// Geometry/3D, Geometry/Points

#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Sections.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilgeometry/mesh/pop/Pop.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Through.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/field/Field.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>
#include <sigilworld/kit/Kit.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compose = sigil::compose;
namespace gm = sigil::geometry::mesh;
namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace sections = sigil::geometry::sections;
namespace sketch = sigil::sketch;
namespace world = sigil::world;

namespace {

using compose::Element;
using material::hexColor;

constexpr int kWidth = 1440, kHeight = 900;
/** The page, in page pixels, which are also scene units. */
constexpr float kPageWidth = 1600, kPageHeight = 1000;
constexpr float kLoop = 12, kPi = 3.14159265358979f;
/** How far in front of the page the glass stands at its middle, how far
 *  its top leans out toward the eye, and how wide its mount is. */
constexpr float kGlassGap = 46, kGlassLean = 3.2f, kMount = 44;
/** The face of the glass the water stands on, in the glass's own frame. */
constexpr float kGlassFace = 3;

constexpr auto kTeal = hexColor(0x3fe0c8);
constexpr auto kAmber = hexColor(0xf2a33c);
constexpr auto kInk = hexColor(0xcfe9ee);
constexpr auto kMuted = hexColor(0x5d8590);
constexpr auto kGridInk = hexColor(0x1d3a42);

float fraction(float value) { return value - std::floor(value); }

float smooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

/** A stable value in [0, 1) for @p index under @p salt. */
float hashed(uint32_t index, uint32_t salt) {
  uint32_t h = index * 374761393u + salt * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return float((h ^ (h >> 16)) & 0xffffffu) / float(0x1000000);
}

std::string decimal(float value, int precision) {
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.*f", precision, double(value));
  return buffer.data();
}

// ---------------------------------------------------------------------------
// THE PAGE: the console's face, a dark ground that takes the light with
// flat telemetry on it that shows as the console's own light.
// ---------------------------------------------------------------------------

Element words(std::string content, float x, float y, float width, float size,
              material::Color ink, float weight = 500,
              std::string_view family = "Menlo, SF Mono, monospace") {
  return compose::kit::at(compose::text(std::move(content)), x, y, width,
                          size * 1.4f)
      .fontFamily(std::string(family))
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}

Element bar(float x, float y, float width, float height, material::Color ink) {
  return compose::kit::at(x, y, width, height).fill(ink);
}

/** A line through @p points, @p width wide, laid over the whole page so
 *  its points are page pixels. */
Element line(const std::vector<glm::vec2>& points, float width,
             material::Color ink) {
  return compose::kit::at(0, 0, kPageWidth, kPageHeight)
      .shape(compose::heldPath(path::curveThrough(points)))
      .stroke(compose::brush::ribbon(path::Profile(width),
                                     material::Material(ink)));
}

/** A channel's signal at @p u, one page width per loop: whole cycles
 *  only, so the trace scrolls round the loop and meets itself. */
float signal(int channel, float u) {
  const float turn = 2 * kPi * u;
  switch (channel) {
    case 0:
      return .55f * std::sin(turn * 3) + .25f * std::sin(turn * 11 + 1.3f) +
             .12f * std::sin(turn * 29 + .4f);
    case 1:
      return .7f * std::sin(turn * 2 + .8f) + .2f * std::sin(turn * 17) +
             .1f * std::sin(turn * 41 + 2.1f);
    default: {
      // A pulse train: a sharp beat every eighth of the width.
      const float beat = fraction(u * 8);
      return -.15f + 1.1f * std::exp(-std::pow((beat - .3f) * 18, 2.0f)) -
             .45f * std::exp(-std::pow((beat - .36f) * 22, 2.0f));
    }
  }
}

/** ONE CHART: a faint grid, the trace scrolling across it with a soft
 *  wide pass under a sharp one, its name and its reading. */
std::vector<Element> chart(int channel, float x, float y, float width,
                           float height, float seconds) {
  const material::Color ink = channel == 1 ? kAmber : kTeal;
  const material::Color haze{ink.r, ink.g, ink.b, .22f};
  std::vector<Element> parts;
  for (int row = 0; row <= 4; ++row)
    parts.push_back(bar(x, y + row * height / 4, width, 1, kGridInk));
  for (int column = 0; column <= 12; ++column)
    parts.push_back(bar(x + column * width / 12, y, 1, height, kGridInk));
  std::vector<glm::vec2> trace;
  constexpr int kSamples = 220;
  const float scroll = seconds / kLoop;
  for (int index = 0; index <= kSamples; ++index) {
    const float across = float(index) / kSamples;
    trace.push_back(
        {x + across * width,
         y + height * (.5f - .42f * signal(channel, across + scroll))});
  }
  parts.push_back(line(trace, 9, haze));
  parts.push_back(line(trace, 2.4f, ink));
  constexpr std::array names{"01 HUMIDITY", "02 PRESSURE", "03 ANEMOMETER"};
  parts.push_back(words(names[channel], x, y - 34, 400, 17, ink, 600));
  parts.push_back(words(decimal(signal(channel, 1 + scroll) * 10 + 50, 2),
                        x + width - 130, y - 38, 130, 22, kInk));
  return parts;
}

/** THE CALIBRATION STRIP: groups of line pairs growing finer toward the
 *  right, then a ramp of greys — straight lines to show every bend. */
std::vector<Element> calibration(float x, float y) {
  std::vector<Element> parts{
      words("CALIBRATION / W-08", x, y - 34, 500, 15, kMuted, 600)};
  constexpr std::array widths{9.0f, 7.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.5f};
  float at = x;
  for (float width : widths) {
    for (int pair = 0; pair < 5; ++pair)
      parts.push_back(bar(at + pair * width * 2, y, width, 70, kInk));
    at += width * 10 + 26;
  }
  for (int step = 0; step < 10; ++step) {
    const float grey = .08f + .09f * step;
    parts.push_back(bar(at + step * 34.0f, y, 34, 70,
                        material::Color{grey, grey, grey, 1}));
  }
  return parts;
}

/** THE PAGE AT @p seconds. */
Element consolePage(float seconds) {
  const float turn = 2 * kPi * seconds / kLoop;
  const auto ground =
      material::from(hexColor(0x091013)).surface({.roughness = .66f});
  std::vector<Element> parts{
      words("HYGRO 08", 90, 70, 300, 26, kInk, 700),
      words("COASTAL ARRAY 04 / WINDOW STATION", 290, 76, 700, 17, kMuted),
      words("T+" + decimal(seconds, 2), 1300, 72, 220, 22, kTeal),
      bar(90, 118, 1420, 2, kGridInk),
  };
  // THE READINGS: two large numbers that drift as the weather does.
  const float humidity =
      84 + 2.6f * std::sin(turn) + .5f * std::sin(turn * 5 + .7f);
  const float dew = 18.6f + .8f * std::sin(turn * 2 + 1.1f);
  parts.push_back(words("RELATIVE HUMIDITY", 90, 170, 420, 17, kMuted, 600));
  parts.push_back(words(decimal(humidity, 1), 82, 196, 460, 150, kInk, 500));
  parts.push_back(words("% RH", 420, 360, 120, 22, kTeal));
  parts.push_back(words("DEW POINT", 90, 430, 420, 17, kMuted, 600));
  parts.push_back(words(decimal(dew, 1), 84, 456, 460, 104, kAmber, 500));
  parts.push_back(words("°C", 350, 572, 80, 22, kAmber));
  for (int channel = 0; channel < 3; ++channel)
    for (Element& part :
         chart(channel, 640, 190 + channel * 186.0f, 870, 120, seconds))
      parts.push_back(std::move(part));
  // THE CHANNEL ROW: small readings ticking over.
  constexpr std::array names{"AIR", "GLASS", "WIND", "FILM"};
  constexpr std::array units{"°C", "°C", "m/s", "mm"};
  const std::array<float, 4> values{
      23.8f + .3f * std::sin(turn * 3), 21.4f + .5f * std::sin(turn + 2),
      7.6f + 2.2f * std::sin(turn * 4 + .5f), .34f + .12f * std::sin(turn * 2)};
  for (int index = 0; index < 4; ++index) {
    const float x = 90 + index * 190.0f;
    parts.push_back(words(names[index], x, 640, 170, 15, kMuted, 600));
    parts.push_back(words(decimal(values[index], index == 3 ? 2 : 1), x, 662,
                          170, 40, kInk));
    parts.push_back(words(units[index], x + 120, 680, 60, 14, kMuted));
    parts.push_back(bar(x, 718, 160, 1, kGridInk));
    parts.push_back(bar(x, 717,
                        160 * (.3f + .6f * fraction(values[index] * .37f)), 3,
                        index == 3 ? kAmber : kTeal));
  }
  for (Element& part : calibration(90, 840)) parts.push_back(std::move(part));
  return compose::stack()
      .width(kPageWidth)
      .height(kPageHeight)
      .fill(ground)
      .children(std::move(parts));
}

// ---------------------------------------------------------------------------
// THE WATER: beads, rivulets and wipe ridges on the glass, in the glass's
// own frame — x across, y up the glass, z out of it toward the eye.
// ---------------------------------------------------------------------------

/** The wiper's pivot below the glass and the arc of glass it cleared. */
constexpr glm::vec2 kWiperPivot{-220, -770};
constexpr float kWipeInner = 735, kWipeOuter = 875;

/** A WATER CAP of unit base radius, its axis out of the glass: a slice
 *  of a sphere standing on the glass at the angle water meets it. */
gm::Mesh capStamp(float droop) {
  constexpr float kHeight = .72f;
  const float radius = (1 + kHeight * kHeight) / (2 * kHeight);
  const float reach = std::asin(1 / radius);
  std::vector<glm::vec2> profile;
  constexpr int kSteps = 14;
  for (int step = kSteps; step >= 0; --step) {
    const float angle = reach * float(step) / kSteps;
    profile.push_back({radius * std::sin(angle),
                       kHeight - radius + radius * std::cos(angle)});
  }
  gm::Mesh cap = gm::revolve(profile, {.segments = 40});
  // Stand it on the glass with its axis out of it, and let a drooping
  // drop hang a little down the pane.
  cap.transform(glm::translate(glm::mat4(1), {0, -.2f * droop, 0}) *
                glm::scale(glm::mat4(1), {1, 1 + .3f * droop, 1}) *
                glm::rotate(glm::mat4(1), kPi / 2, {1, 0, 0}));
  return cap;
}

/** The arch a run of water carries along the glass: wide on the pane,
 *  low off it. */
path::Polyline archProfile(float height) {
  path::Polyline arch;
  constexpr int kSteps = 12;
  for (int step = 0; step <= kSteps; ++step) {
    const float angle = kPi * float(kSteps - step) / kSteps;
    arch.points.push_back({std::cos(angle), -height * std::sin(angle)});
  }
  return arch;
}

struct Bead {
  glm::vec2 at;
  float radius;
  bool droops;
};

/** Whether @p at lies in the arc the wiper cleared, @p margin wider. */
bool wiped(glm::vec2 at, float margin) {
  const float reach = glm::length(at - kWiperPivot);
  return reach > kWipeInner - margin && reach < kWipeOuter + margin;
}

/** THE BEADS, thrown at the glass and kept where they fit: many small,
 *  a few large, none in the wiped arc. */
std::vector<Bead> scatterBeads() {
  std::vector<Bead> beads;
  const float halfWidth = kPageWidth / 2 - kMount - 6;
  const float halfHeight = kPageHeight / 2 - kMount - 6;
  for (uint32_t attempt = 0; attempt < 9000 && beads.size() < 620; ++attempt) {
    const float size = hashed(attempt, 3);
    const float radius = 3.5f + 38 * size * size * size;
    const glm::vec2 at{(hashed(attempt, 1) * 2 - 1) * (halfWidth - radius),
                       (hashed(attempt, 2) * 2 - 1) * (halfHeight - radius)};
    if (wiped(at, radius + 8)) continue;
    const bool fits =
        std::none_of(beads.begin(), beads.end(), [&](const Bead& other) {
          return glm::length(other.at - at) < other.radius + radius + 3;
        });
    if (!fits) continue;
    beads.push_back({at, radius, radius > 11 && hashed(attempt, 4) < .45f});
  }
  return beads;
}

/** ONE RIVULET: the lane it runs down and when in the loop it starts. */
struct Rivulet {
  float lane;
  float offset;
  float wander;
};
constexpr std::array kRivulets{Rivulet{-500, .10f, 1.3f},
                               Rivulet{90, .43f, 4.1f},
                               Rivulet{545, .76f, 2.6f}};
constexpr float kRunTop = 430, kRunBottom = -560;
/** The lowest the water shows, where the mount covers the glass. */
constexpr float kGlassBottom = -kPageHeight / 2 + kMount;

/** Where a rivulet's path crosses height @p y. */
float runX(const Rivulet& run, float y) {
  return run.lane + 22 * std::sin(y * .0105f + run.wander) +
         9 * std::sin(y * .029f + run.wander * 2.3f);
}

/** A rivulet's moment in the loop: where its head is, how much of its
 *  trail is left, and how far the beads it took up have formed again. */
struct RunState {
  float head;
  float trail;
  float regrowth;
};

RunState runAt(const Rivulet& run, float seconds) {
  const float life = fraction(seconds / kLoop + run.offset);
  // The drop clings, then lets go and runs faster as it gathers water.
  const float travel = smooth(0, .72f, life);
  return {kRunTop - (kRunTop - kRunBottom) * travel * travel,
          1 - smooth(.7f, .94f, life), smooth(.86f, 1.0f, life)};
}

/** The water surface: clear, at water's index, @p thickness the path
 *  light takes through the body. */
material::Material water(float thickness) {
  return material::from(material::Color{1, 1, 1, 1})
      .surface({.roughness = .04f,
                .transmission = 1.0f,
                .ior = 1.33f,
                .thickness = thickness});
}

/** The bead sizes and the path light takes through each. */
struct BeadClass {
  float smallest;
  float thickness;
};
constexpr std::array kBeadClasses{BeadClass{0, 6}, BeadClass{9, 16},
                                  BeadClass{18, 32}};

/** THE CAMERA: a little above the console and to its left. */
gm::camera::Camera lens() {
  gm::camera::Camera camera;
  camera.eye = {-380, 300, 1960};
  camera.target = {-60, 10, 0};
  camera.fovYDeg = 28.5f;
  camera.zNear = 80;
  camera.zFar = 8000;
  return camera;
}

struct WetGlassConsole {
  sketch::Page page;
  world::EnvironmentMap room;
  std::vector<Bead> beads;
  gm::Mesh roundCap, droopingCap, plate, face, glass, railAcross, railUp;
  gm::Mesh wipeRidges;

  void setup(sketch::Scene3DContext& ctx) {
    sketch::kit::stage(ctx, {.size = {kWidth, kHeight},
                             .captureAt = 3.0,
                             .background = hexColor(0x06080a)});
    page = ctx.page({int(kPageWidth), int(kPageHeight)});
    // A dim room with cool windows round it, so a bead turned any way
    // finds one to hold.
    room = world::kit::environmentMap(
        {.zenith = {.05f, .06f, .08f},
         .nadir = {.02f, .02f, .025f},
         .softBoxes = {{.centre = {.0f, .34f},
                        .size = {.12f, .08f},
                        .radiance = {3.0f, 3.6f, 4.6f}},
                       {.centre = {.25f, .28f},
                        .size = {.16f, .06f},
                        .radiance = {2.6f, 3.2f, 4.2f}},
                       {.centre = {.5f, .36f},
                        .size = {.10f, .10f},
                        .radiance = {2.2f, 2.7f, 3.4f}},
                       {.centre = {.75f, .30f},
                        .size = {.18f, .06f},
                        .radiance = {2.8f, 2.5f, 2.2f}}}});
    beads = scatterBeads();
    roundCap = capStamp(0);
    droopingCap = capStamp(1);
    plate = gm::box({-kPageWidth / 2 - 30, -kPageHeight / 2 - 30, -60},
                    {kPageWidth / 2 + 30, kPageHeight / 2 + 30, -.5f},
                    {.front = false});
    face = gm::quad(kPageWidth, kPageHeight);
    glass = gm::box({-kPageWidth / 2, -kPageHeight / 2, -kGlassFace},
                    {kPageWidth / 2, kPageHeight / 2, kGlassFace});
    railAcross = gm::box({-kPageWidth / 2 - 6, -kMount / 2, -14},
                         {kPageWidth / 2 + 6, kMount / 2, 14});
    railUp = gm::box({-kMount / 2, -kPageHeight / 2 + kMount, -14},
                     {kMount / 2, kPageHeight / 2 - kMount, 14});
    wipeRidges = wipeMarks();
  }

  /** THE WIPE MARKS: two low ridges of water along the edges of the
   *  cleared arc, and one faint streak inside it where the blade was
   *  worn. */
  static gm::Mesh wipeMarks() {
    const auto arc = [](float radius) {
      gm::Cloud points;
      for (int step = 0; step <= 160; ++step) {
        const float angle = kPi * (.08f + .84f * step / 160.0f);
        const glm::vec2 at =
            kWiperPivot + radius * glm::vec2{-std::cos(angle), std::sin(angle)};
        if (std::abs(at.x) < kPageWidth / 2 - kMount &&
            std::abs(at.y) < kPageHeight / 2 - kMount)
          points.positions.push_back({at, kGlassFace});
      }
      return points;
    };
    gm::Mesh marks;
    const struct {
      float radius, width, height;
    } passes[] = {{kWipeInner - 4, 4, .5f},
                  {kWipeOuter + 4, 5, .55f},
                  {kWipeInner + 58, 1.2f, .4f}};
    for (const auto& pass : passes)
      marks.append(
          gm::pop::on(arc(pass.radius))
              .sweep(archProfile(pass.height), false,
                     {.segments = 220, .scale = pass.width, .up = {0, 0, 1}}));
    return marks;
  }

  /** THE BEADS AT @p seconds, one instanced body per size and shape: a
   *  bead a rivulet's head has passed over is taken up, and forms again
   *  as that trail drains. */
  std::vector<world::Node> beadBodies(float seconds) const {
    std::array<RunState, kRivulets.size()> runs{};
    for (size_t index = 0; index < kRivulets.size(); ++index)
      runs[index] = runAt(kRivulets[index], seconds);
    std::array<std::array<gm::Cloud, 2>, kBeadClasses.size()> clouds;
    for (const Bead& bead : beads) {
      float scale = 1;
      for (size_t index = 0; index < kRivulets.size(); ++index) {
        const Rivulet& run = kRivulets[index];
        const float reach = 18 + bead.radius;
        if (std::abs(bead.at.x - runX(run, bead.at.y)) < reach &&
            bead.at.y > runs[index].head)
          scale = std::min(scale, runs[index].regrowth);
      }
      if (scale < .05f) continue;
      size_t sizeClass = 0;
      while (sizeClass + 1 < kBeadClasses.size() &&
             bead.radius >= kBeadClasses[sizeClass + 1].smallest)
        ++sizeClass;
      gm::Cloud& cloud = clouds[sizeClass][bead.droops ? 1 : 0];
      cloud.positions.push_back({bead.at, kGlassFace});
      cloud.scalar("size").back() = bead.radius * scale;
    }
    std::vector<world::Node> bodies;
    for (size_t sizeClass = 0; sizeClass < kBeadClasses.size(); ++sizeClass)
      for (int shape = 0; shape < 2; ++shape) {
        const gm::Cloud& cloud = clouds[sizeClass][shape];
        if (cloud.size() == 0) continue;
        bodies.push_back(world::mesh(gm::pop::on(cloud).instances(
                                         shape ? droopingCap : roundCap))
                             .key("beads " + std::to_string(sizeClass) +
                                  (shape ? " drooping" : " round"))
                             .fill(water(kBeadClasses[sizeClass].thickness))
                             .collection("glazing"));
      }
    return bodies;
  }

  /** THE RIVULETS AT @p seconds: each a trail swept down its lane to
   *  where its head has reached, thin behind and swelling toward the
   *  head, and the head itself a heavy drop. */
  std::vector<world::Node> rivulets(float seconds) const {
    std::vector<world::Node> bodies;
    for (size_t index = 0; index < kRivulets.size(); ++index) {
      const Rivulet& run = kRivulets[index];
      const RunState state = runAt(run, seconds);
      const std::string key = "rivulet " + std::to_string(index);
      if (state.trail > .02f && kRunTop - state.head > 12) {
        // The rail ends where the head is, or at the mount if the head
        // has run in under it.
        const float end = std::max(state.head, kGlassBottom);
        gm::Cloud rail;
        for (float y = kRunTop; y > end; y -= 14)
          rail.positions.push_back({runX(run, y), y, kGlassFace});
        rail.positions.push_back({runX(run, end), end, kGlassFace});
        const float trail = state.trail;
        bodies.push_back(
            world::mesh(gm::pop::on(rail).sweep(
                            archProfile(.6f), false,
                            {.segments = 120,
                             .scale = 14,
                             .taper =
                                 [trail](float t) {
                                   return trail *
                                          (.35f + .65f * smooth(.55f, 1, t));
                                 },
                             .up = {0, 0, 1}}))
                .key(key + " trail")
                .fill(water(8))
                .collection("glazing"));
      }
      if (state.head > kGlassBottom + 10) {
        gm::Cloud head;
        head.positions.push_back(
            {runX(run, state.head), state.head - 6, kGlassFace});
        head.scalar("size").back() = 17;
        bodies.push_back(world::mesh(gm::pop::on(head).instances(droopingCap))
                             .key(key + " head")
                             .fill(water(40))
                             .collection("glazing"));
      }
    }
    return bodies;
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = float(std::fmod(now.count(), double(kLoop)));
    const float drift = std::sin(2 * kPi * seconds / kLoop);
    const auto mountMetal = material::from(hexColor(0x1b1f23))
                                .surface({.metallic = .85f, .roughness = .38f});
    // The cover glass is coated against reflection, as a console's is,
    // so the room lies faintly on it and brightly on the water.
    const material::Material clearGlass =
        material::from(material::Color{1, 1, 1, 1})
            .surface({.roughness = .02f,
                      .specular = .3f,
                      .transmission = 1.0f,
                      .ior = 1.5f,
                      .thickness = 2 * kGlassFace});

    std::vector<world::Node> onGlass{
        world::mesh(glass).key("glass").fill(clearGlass).collection("glazing"),
        world::mesh(railAcross)
            .key("mount top")
            .at({0, kPageHeight / 2 - kMount / 2, 0})
            .fill(mountMetal),
        world::mesh(railAcross)
            .key("mount bottom")
            .at({0, -kPageHeight / 2 + kMount / 2, 0})
            .fill(mountMetal),
        world::mesh(railUp)
            .key("mount left")
            .at({-kPageWidth / 2 + kMount / 2, 0, 0})
            .fill(mountMetal),
        world::mesh(railUp)
            .key("mount right")
            .at({kPageWidth / 2 - kMount / 2, 0, 0})
            .fill(mountMetal),
        world::mesh(wipeRidges)
            .key("wipe marks")
            .fill(water(5))
            .collection("glazing"),
    };
    for (world::Node& body : beadBodies(seconds))
      onGlass.push_back(std::move(body));
    for (world::Node& body : rivulets(seconds))
      onGlass.push_back(std::move(body));

    return world::Frame(
               world::group(
                   {world::camera(lens(),
                                  {.exposure = .95f,
                                   .toneCurve = world::ToneCurve::Neutral}),
                    // THE KEY: cool, high on the left and in front,
                    // throwing the mount's shadow down across the page.
                    world::directionalLight({.42f, -.55f, -.72f},
                                            {.color = hexColor(0xc8dcff),
                                             .intensity = 2.4f,
                                             .castShadow = true,
                                             .shadow = {.bias = 2,
                                                        .normalBias = 1.5f,
                                                        .radius = 2.5f,
                                                        .mapSize = 2048}})
                        .key("key"),
                    // THE PANEL every bead shows as its highlight,
                    // drifting slowly across above the eye; it reaches
                    // the glass and the water only, so the page keeps
                    // its dark.
                    world::rectLight({-420 * drift + 120, -640, -1000},
                                     {.width = 1300,
                                      .height = 380,
                                      .color = hexColor(0xe6f0ff),
                                      .intensity = 30})
                        .key("panel")
                        .lightLink("glazing")
                        .at({420 * drift - 120, 640, 1000}),
                    world::ambientLight(
                        {.color = hexColor(0x8fa6c0), .intensity = .05f})
                        .key("fill"),
                    world::group(
                        {world::mesh(plate).key("housing").fill(mountMetal),
                         world::mesh(face).key("face").fill(page.surface()),
                         world::group(std::move(onGlass))
                             .key("glass rig")
                             .at({0, 0, kGlassGap})
                             .rotateX(kGlassLean)})
                        .key("console")
                        .rotateX(-7)}))
        .environment(room, {.intensity = .6f, .diffuse = .08f})
        .pass(page.render(consolePage(seconds)));
  }
};

}  // namespace

SIGIL_SKETCH(WetGlassConsole, "Study · Materials",
             "A live telemetry console under rained-on glass: beads, "
             "rivulets and wipe ridges of water bend the changing page "
             "beneath them")
