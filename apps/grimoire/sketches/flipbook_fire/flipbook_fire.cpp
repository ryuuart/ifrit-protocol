// Two braziers burning on a dark floor: effects an authoring tool would
// hand over — a flame flipbook, a channel-packed mask and a flow map —
// played on World bodies, each texture generated here at setup.
// TAGS: Materials/Lighting, Geometry/3D, Effects

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilgeometry/mesh/Instances.h>
#include <sigilgeometry/mesh/Mesh.h>
#include <sigilgeometry/mesh/camera/Camera.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Channel.h>
#include <sigilmaterial/texture/Flipbook.h>
#include <sigilmaterial/texture/Flowmap.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilsketch/scene3d/Scene3D.h>
#include <sigilworld/World.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace media = sigil::media;
namespace sketch = sigil::sketch;
namespace world = sigil::world;
namespace gm = sigil::geometry::mesh;

namespace {

constexpr float kTau = 6.28318530718f;
/** The loop: the flame's 64 frames at 32 a second, and every flicker a
 *  whole number of cycles inside it. */
constexpr int kColumns = 8;
constexpr int kRows = 8;
constexpr int kFrames = kColumns * kRows;
constexpr float kFramesPerSecond = 32;
constexpr float kLoopSeconds = kFrames / kFramesPerSecond;
/** One cell of the sheet, in pixels. */
constexpr int kCell = 64;
/** Where the bowls' rims stand, and how far apart the two braziers are. */
constexpr float kRim = 92;
constexpr float kApart = 175;

float smooth(float edge0, float edge1, float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3 - 2 * t);
}

uint8_t byte(float value) {
  return (uint8_t)std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}

/** How hot a flame is at @p heat in 0..1, as a colour: dark red at the
 *  edge, through orange and yellow, to a pale core. */
std::array<float, 3> heatColour(float heat) {
  struct Stop {
    float at;
    float r, g, b;
  };
  constexpr Stop kStops[] = {{0.00f, 0.00f, 0.00f, 0.00f},
                             {0.22f, 0.45f, 0.04f, 0.00f},
                             {0.50f, 0.98f, 0.30f, 0.03f},
                             {0.76f, 1.00f, 0.70f, 0.22f},
                             {1.00f, 1.00f, 0.96f, 0.82f}};
  for (size_t i = 1; i < std::size(kStops); ++i)
    if (heat <= kStops[i].at) {
      const Stop& a = kStops[i - 1];
      const Stop& b = kStops[i];
      const float t = (heat - a.at) / (b.at - a.at);
      return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
              a.b + (b.b - a.b) * t};
    }
  return {1.0f, 0.96f, 0.82f};
}

/** One pixel of the flame at @p u across and @p v down its cell, at
 *  @p phase of the loop in 0..1: a tongue that narrows to a flickering
 *  tip, swaying and torn by ripples that rise with the loop. Every ripple
 *  turns a whole number of times a loop, so the last frame leads back
 *  into the first. */
float flameAt(float u, float v, float phase) {
  const float h = 1 - v;
  const float turn = phase * kTau;
  const float sway = 0.055f * std::sin(turn + h * 5.0f) * h +
                     0.025f * std::sin(2 * turn + h * 13.0f) * h;
  const float tear = 0.55f * std::sin(2 * turn - h * 10.0f + u * 6.0f) +
                     0.30f * std::sin(3 * turn - h * 17.0f + u * 13.0f + 1.3f) +
                     0.15f * std::sin(5 * turn - h * 29.0f - u * 7.0f + 2.1f);
  const float tip = 0.80f + 0.07f * std::sin(2 * turn + 0.7f) +
                    0.05f * std::sin(3 * turn + 2.0f);
  const float width = 0.27f * std::pow(std::max(0.0f, 1 - h / tip), 0.6f) *
                      smooth(0.0f, 0.16f, h);
  const float across =
      std::abs(u - 0.5f - sway) / std::max(width * (1 + 0.22f * tear), 1e-3f);
  const float body = smooth(1.0f, 0.25f, across);
  const float rise = smooth(tip, tip * 0.5f, h + 0.07f * tear);
  const float foot = smooth(0.0f, 0.06f, h);
  return body * rise * foot;
}

/** THE FLAME SHEET: 64 frames of one flame in an 8 x 8 grid, read row by
 *  row from the top left, on black — what a flipbook tool writes for an
 *  additive flame. */
material::Texture flameSheet() {
  SkBitmap sheet;
  sheet.allocN32Pixels(kColumns * kCell, kRows * kCell);
  for (int frame = 0; frame < kFrames; ++frame) {
    const int left = (frame % kColumns) * kCell;
    const int top = (frame / kColumns) * kCell;
    const float phase = (float)frame / (float)kFrames;
    for (int y = 0; y < kCell; ++y)
      for (int x = 0; x < kCell; ++x) {
        const float u = ((float)x + 0.5f) / kCell;
        const float v = ((float)y + 0.5f) / kCell;
        const float strength = flameAt(u, v, phase);
        // The core runs hotter low in the flame than at its tip.
        const float heat = strength * (0.8f + 0.2f * smooth(0.6f, 0.1f, 1 - v));
        const std::array<float, 3> colour = heatColour(heat);
        const float shown = std::pow(strength, 0.85f);
        *sheet.getAddr32(left + x, top + y) =
            SkPreMultiplyARGB(255, byte(colour[0] * shown),
                              byte(colour[1] * shown), byte(colour[2] * shown));
      }
  }
  sheet.setImmutable();
  return material::Texture(media::PixelSource(sheet.asImage()))
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

/** THE PACKED MASK: three greys in one image, as an effects tool packs
 *  them — red a soft disc, green a two-octave noise that repeats across
 *  the image, blue a ring. It is data, read exactly. */
material::Texture packedMask() {
  constexpr int kSize = 128;
  SkBitmap mask;
  mask.allocPixels(SkImageInfo::Make(kSize, kSize, kRGBA_8888_SkColorType,
                                     kUnpremul_SkAlphaType));
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x) {
      const float u = ((float)x + 0.5f) / kSize, v = ((float)y + 0.5f) / kSize;
      const float r = std::hypot(u - 0.5f, v - 0.5f) * 2;
      const float grain =
          0.65f * noise(u * 8, v * 8, 8) + 0.35f * noise(u * 16, v * 16, 16);
      auto* texel = static_cast<uint8_t*>(mask.getAddr(x, y));
      texel[0] = byte(smooth(1.0f, 0.2f, r));
      texel[1] = byte(smooth(0.35f, 0.85f, grain));
      texel[2] = byte(smooth(0.25f, 0.0f, std::abs(r - 0.7f)));
      texel[3] = 255;
    }
  mask.setImmutable();
  return material::Texture(media::PixelSource(mask.asImage()))
      .colorSpace(media::ColorSpace::Data)
      .tile(material::Repeat::Repeat);
}

/** THE FLOW: a slow swirl about the middle of the image, drawn outward a
 *  little, red along u and green along v around one half. */
material::Texture swirl() {
  constexpr int kSize = 64;
  SkBitmap flow;
  flow.allocPixels(SkImageInfo::Make(kSize, kSize, kRGBA_8888_SkColorType,
                                     kUnpremul_SkAlphaType));
  for (int y = 0; y < kSize; ++y)
    for (int x = 0; x < kSize; ++x) {
      const float u = ((float)x + 0.5f) / kSize - 0.5f;
      const float v = ((float)y + 0.5f) / kSize - 0.5f;
      const float along = -v * 1.4f + u * 0.4f;
      const float down = u * 1.4f + v * 0.4f;
      auto* texel = static_cast<uint8_t*>(flow.getAddr(x, y));
      texel[0] = byte(0.5f + 0.5f * along);
      texel[1] = byte(0.5f + 0.5f * down);
      texel[2] = 0;
      texel[3] = 255;
    }
  flow.setImmutable();
  return material::Texture(media::PixelSource(flow.asImage()))
      .colorSpace(media::ColorSpace::Data)
      .tile(material::Repeat::Repeat);
}

/** A FLAME: the sheet as the light a card gives off — a black body that
 *  emits — added over the scene, fading out where it dips into the bowl
 *  rather than cut along the coals. */
material::Material flame(const material::Texture& sheet, bool blendFrames,
                         float startFrame) {
  return material::from(material::Color{0, 0, 0, 1})
      .surface({.metallic = 1.0f,
                .roughness = 1.0f,
                .emission = {1, 1, 1, 1},
                .emissionStrength = 3.2f,
                .emissionMap = material::flipbook(
                    sheet, {.columns = kColumns,
                            .rows = kRows,
                            .framesPerSecond = kFramesPerSecond,
                            .blendFrames = blendFrames,
                            .startFrame = startFrame}),
                .blend = material::BlendMode::PlusLighter,
                .depthFade = 22.0f});
}

/** AN EMBER: the flame sheet as a spark of light added over the scene,
 *  each instance showing the cell its own "cell" scalar names. */
material::Material ember(const material::Texture& sheet) {
  return material::from(material::Color{0, 0, 0, 1})
      .surface(
          {.metallic = 1.0f,
           .roughness = 1.0f,
           .emission = {1.0f, 0.78f, 0.52f, 1},
           .emissionStrength = 3.0f,
           .emissionMap = material::flipbook(sheet, {.columns = kColumns,
                                                     .rows = kRows,
                                                     .framesPerSecond = 0,
                                                     .frameAttribute = "cell"}),
           .blend = material::BlendMode::PlusLighter});
}

/** A number in 0..1 that @p n and @p salt decide and nothing else. */
float scatter(int n, float salt) {
  const float x = std::sin((float)n * 12.9898f + salt * 78.233f) * 43758.547f;
  return x - std::floor(x);
}

/** THE EMBERS over one fire at @p seconds: sparks rising off the coals
 *  and drifting, as instanced geometry. Each is a pure function of the
 *  time and its own seed — how far through its rise it is, which way it
 *  drifts, which cell of the sheet it shows — so the drift plays and
 *  nothing is simulated. Every rise lasts the loop or half of it. */
gm::Instances risingEmbers(float seconds, float salt) {
  constexpr int kEmbers = 40;
  constexpr float kRise = 250;
  gm::Instances out;
  out.stamp = gm::quad(10, 10);
  std::vector<float>& cell = out.attributes["cell"];
  for (int n = 0; n < kEmbers; ++n) {
    const float own = scatter(n, salt);
    const float life =
        scatter(n, salt + 1) < 0.5f ? kLoopSeconds : kLoopSeconds / 2;
    const float rise = seconds / life + own;
    const float age = rise - std::floor(rise);
    const float bearing = scatter(n, salt + 2) * kTau;
    const float reach = 10.0f + 26.0f * scatter(n, salt + 3);
    const float sway = 9.0f * std::sin(age * kTau * 1.5f + own * kTau) * age;
    out.positions.push_back({std::cos(bearing) * reach * (0.4f + age) + sway,
                             kRim - 6 + age * kRise,
                             std::sin(bearing) * reach * (0.4f + age)});
    out.scales.emplace_back(1.0f - 0.6f * age);
    // Brightest just off the coals, gone by the top of the rise.
    const float shown = smooth(0.0f, 0.08f, age) * std::pow(1.0f - age, 1.6f);
    out.colors.push_back({1, 1, 1, shown});
    const float frame = own * kFrames + seconds * kFramesPerSecond;
    cell.push_back(frame - std::floor(frame / kFrames) * kFrames);
  }
  return out;
}

/** THE COALS: dark where they are cold, and glowing brighter than white
 *  where the packed mask's noise — streaming slowly along the swirl —
 *  says they are hot. */
material::Material coals(const material::Texture& mask,
                         const material::Texture& flow) {
  return material::from(material::Color{0.07f, 0.05f, 0.045f, 1})
      .surface({.roughness = 0.9f,
                .emission = {1.0f, 0.36f, 0.08f, 1},
                .emissionStrength = 4.0f,
                .emissionMap = material::flowmap(
                    material::channel(material::image(mask),
                                      material::ColorChannel::Green),
                    {.flow = flow, .strength = 0.18f, .speed = 0.5f})});
}

/** The bowl and its three legs, in dark iron. */
world::Group brazier(std::string key, const material::Material& iron) {
  const gm::Mesh bowl = gm::revolve({{0, kRim - 40},
                                     {34, kRim - 38},
                                     {58, kRim - 22},
                                     {70, kRim - 2},
                                     {74, kRim},
                                     {66, kRim + 1},
                                     {56, kRim - 16},
                                     {32, kRim - 30},
                                     {0, kRim - 32}},
                                    {.segments = 64});
  std::vector<world::Node> parts{world::mesh(bowl).key("bowl").fill(iron)};
  for (int leg = 0; leg < 3; ++leg) {
    const float turn = (float)leg * kTau / 3.0f + 0.5f;
    parts.push_back(
        world::mesh(gm::box({-3.5f, 0, -3.5f}, {3.5f, kRim - 34, 3.5f}))
            .key("leg-" + std::to_string(leg))
            .at({34.0f * std::cos(turn), 0, 34.0f * std::sin(turn)})
            .fill(iron));
  }
  return world::group(parts).key(std::move(key));
}

/** How hard the fire under a light burns at @p seconds: a breath and two
 *  quicker flutters, each a whole number of cycles in the loop. */
float flicker(float seconds, float offset) {
  const float t = seconds / kLoopSeconds * kTau;
  return 1.0f + 0.16f * std::sin(3 * t + offset) +
         0.09f * std::sin(7 * t + 1.7f + offset) +
         0.05f * std::sin(13 * t + 0.4f + offset * 2);
}

struct FlipbookFire {
  material::Texture sheet;
  material::Texture mask;
  material::Texture flow;
  material::Material iron{material::Color{0, 0, 0, 1}};
  material::Material embers{material::Color{0, 0, 0, 1}};
  material::Material sparks{material::Color{0, 0, 0, 1}};

  void setup(sketch::Scene3DContext& ctx) {
    ctx.canvas(960, 600);
    ctx.captureAt(0.6);
    ctx.background(material::Color{0.012f, 0.012f, 0.016f, 1});
    sheet = flameSheet();
    mask = packedMask();
    flow = swirl();
    iron = material::surface::program({.baseColor = {0.20f, 0.18f, 0.16f, 1},
                                       .metallic = 1.0f,
                                       .roughness = 0.42f});
    embers = coals(mask, flow);
    sparks = ember(sheet);
  }

  /** One brazier at @p x: its iron, its coals, a flame card turned to
   *  the camera about the up axis, embers rising off it each turned to the
   *  camera, and a light above it that burns with the flame. */
  world::Group fire(const char* key, float x, bool blendFrames,
                    float startFrame, float seconds) const {
    const float burning = flicker(seconds, startFrame * 0.37f);
    return world::group(
               {brazier("brazier", iron),
                world::mesh(
                    gm::revolve({{0, 0}, {52, 0}, {50, 4}, {34, 12}, {0, 16}},
                                {.segments = 64}))
                    .key("coals")
                    .at({0, kRim - 20, 0})
                    .fill(embers),
                world::mesh(gm::quad(150, 230))
                    .key("flame")
                    .at({0, kRim + 88, 0})
                    .facing(world::Facing::CameraUpright)
                    .fill(flame(sheet, blendFrames, startFrame)),
                world::mesh(risingEmbers(seconds, startFrame + x * 0.01f))
                    .key("embers")
                    .facing(world::Facing::Camera)
                    .fill(sparks),
                world::pointLight(
                    {.color = material::Color{1.0f, 0.62f, 0.30f, 1},
                     .intensity = world::intensityAt(1.6f * burning, 220.0f)})
                    .key("light")
                    .at({0, kRim + 70, 0})})
        .key(key)
        .at({x, 0, 0});
  }

  world::Frame describe(sigil::motion::Duration now) {
    const float seconds = (float)now.count();
    gm::camera::Camera lens;
    lens.eye = {0, 175, 560};
    lens.target = {0, 120, 0};
    lens.fovYDeg = 40;
    return world::Frame(
               world::group(
                   {world::camera(lens,
                                  {.exposure = 1.4f,
                                   .toneCurve = world::ToneCurve::Neutral}),
                    world::hemisphereLight(
                        {.color = material::Color{0.10f, 0.12f, 0.20f, 1},
                         .groundColor = material::Color{0.05f, 0.03f, 0.02f, 1},
                         .intensity = 0.25f}),
                    world::mesh(gm::quad(1600, 1600))
                        .key("floor")
                        .rotateX(-90)
                        .fill(material::surface::program(
                            {.baseColor = {0.16f, 0.14f, 0.13f, 1},
                             .roughness = 0.85f})),
                    fire("blended", -kApart, true, 0, seconds),
                    fire("stepped", kApart, false, 23, seconds)})
                   .key("set"))
        .pass(
            world::bloom({.threshold = 1.0f, .intensity = 0.7f, .radius = 48}));
  }
};

}  // namespace

SIGIL_SKETCH(FlipbookFire, "Study · Effects",
             "Two braziers burning a generated flame flipbook — one blending "
             "its frames, one stepping through them — with flowing coals "
             "from a channel-packed mask, embers drawn as instanced sparks "
             "each playing its own cell, a light that flickers with the "
             "fire, soft edges where the flame meets the bowl, and bloom");
