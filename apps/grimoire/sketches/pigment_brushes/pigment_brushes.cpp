/** @file
 * A pigment workshop: dry ground, wet ridged brushwork and metallic overprint.
 *
 * EDIT THESE FIRST
 *   Controls — ridge strength, roughness and metallic overprint opacity.
 *   heldPhase — -1 cycles; 0–5 holds one material state.
 *   ribbon() — pressure profile and the native brush's sampling step.
 */
// TAGS: Materials/Pigment, Materials/Lighting, Brushes/Layered,
// Typography/Material ink, Composition/Editorial

#include <sigilcompose/brush/Brushes.h>
#include <sigilcompose/brush/Relief.h>
#include <sigilcompose/brush/Ribbons.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace motion = sigil::motion;
namespace geometry = sigil::geometry;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {
constexpr material::Color kPaper{0.91f, 0.88f, 0.80f, 1};
constexpr material::Color kInk{0.11f, 0.14f, 0.16f, 1};
constexpr material::Color kMuted{0.38f, 0.38f, 0.36f, 1};
constexpr material::Color kBlue{0.045f, 0.24f, 0.55f, 1};
constexpr material::Color kRed{0.72f, 0.20f, 0.10f, 1};
constexpr material::Color kGold{0.83f, 0.56f, 0.19f, 1};
constexpr std::array kPhases{"01 / LOADED BRISTLES", "02 / NORMAL ZERO",
                             "03 / ROUGHNESS ZERO",  "04 / ROUGHNESS ONE",
                             "05 / OVERPRINT ZERO",  "06 / OVERPRINT ONE"};
constexpr std::array kDetails{"Wet blue / dry red / metallic overprint",
                              "Same colors and layers, flat tangent normals",
                              "Gloss endpoint under a moving raking light",
                              "Matte endpoint; the ridge field stays present",
                              "Metal pass absent; lower strokes remain",
                              "Opaque metal pass, tight clip and counters"};

struct Controls {
  float ridge = 0.72f;
  float roughness = 0.28f;
  float overprint = 0.76f;
};
struct Pigment {
  material::Color ink = kBlue;
  float ridge = 0.72f;
  float seed = 19;
};
constexpr std::string_view kColor = R"(
float hash(float2 q) { return fract(sin(dot(q, float2(127.1,311.7)) + seed)*43758.5453); }
half4 main(float2 p) {
  float bristle = sin(p.y*1.63 + sin(p.x*.025 + seed)*.8);
  float grain = hash(floor(p*.8));
  float variation = .88 + .06*bristle + .06*grain;
  return half4(ink.rgb*variation*ink.a, ink.a);
})";
constexpr std::string_view kNormal = R"(
float height(float2 p) {
  float comb = p.y*1.63 + sin(p.x*.025 + seed)*.8;
  float crest = pow(.5 + .5*sin(comb), 3.0);
  return ridge*(.78*crest + .16*sin(p.y*.31 + sin(p.x*.011)*.7));
}
half4 main(float2 p) {
  float dx = (height(p+float2(.3,0))-height(p-float2(.3,0)))/.6;
  float dy = (height(p+float2(0,.3))-height(p-float2(0,.3)))/.6;
  float3 n = normalize(float3(-dx,-dy,1));
  return half4(n*.5+.5,1);
})";
constexpr std::string_view kSpeckles = R"(
half4 main(float2 p) {
  float2 q = floor(p/3.0);
  float v = fract(sin(dot(q,float2(127.1,311.7))+17.0)*43758.5453);
  float a = smoothstep(.91,.975,v)*.65;
  return half4(.06*a,.07*a,.07*a,a);
})";
constexpr std::string_view kEnvironment = R"(
half4 main(float2 p) {
  float2 uv = p/float2(512,256);
  float stripDistance = (uv.x-.26)/.065;
  float softDistance = (uv.x-.68)/.20;
  float strip = exp(-stripDistance*stripDistance);
  float soft = exp(-softDistance*softDistance);
  return half4(float3(.62,.65,.68)+strip*float3(.8,.84,.86)+soft*float3(.32,.23,.12),1);
})";

Element label(std::string_view words, float x, float y, float w,
              float size = 11, material::Color ink = kMuted,
              float weight = 500) {
  return kit::at(text(std::string(words)), x, y, w, size * 1.5f)
      .fontSize(size)
      .fontWeight(weight)
      .ink(ink);
}
Element rule(float x, float y, float w, float h = 1) {
  return kit::at(x, y, w, h).fill(material::Color{0.33f, 0.34f, 0.32f, .32f});
}
std::string decimal(float value) {
  std::array<char, 24> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%.2f",
                static_cast<double>(value));
  return buffer.data();
}
material::Material paint(material::Color ink, const Controls& c,
                         bool metal = false, bool dry = false,
                         float seed = 19) {
  const Pigment p{.ink = ink, .ridge = c.ridge, .seed = seed};
  return material::shader(kColor, p)
      .layer(material::shader(kSpeckles), {.opacity = dry ? .34f : .09f})
      .surface({.metallic = metal ? 1.0f : 0.0f,
                .roughness = dry ? .9f : c.roughness,
                .normal = material::shader(kNormal, p),
                .normalDirectX = true,
                .clearcoat = dry || metal ? 0.0f : .55f,
                .reflectionWeight = metal ? .82f : .35f});
}
brush::Ribbon ribbon(float width, material::Material finish,
                     float taper = .08f) {
  brush::Ribbon out;
  out.fillMaterial = std::move(finish);
  out.width =
      geometry::path::Profile({{0, width * .17f},
                               {.08f, width * .9f},
                               {.24f, width},
                               {.76f, width * .86f},
                               {.95f, width * .42f},
                               {1, width * taper}},
                              {.between = geometry::path::Between::Smooth});
  out.step = 2;
  out.join = geometry::path::Join::Round;
  return out;
}
Element mark(float x, float y, float w, float h, std::string_view path,
             float width, material::Material finish) {
  return kit::at(x, y, w, h)
      .shape(shapes::svg(std::string(path)))
      .stroke(ribbon(width, std::move(finish)));
}
Element raised(std::string_view words, float x, float y, float w, float size,
               material::Material finish) {
  return label(words, x, y, w, size, kInk, 760)
      .ink(material::Color{0, 0, 0, 0})
      .decorationOutline(Boundary::Glyphs)
      .foreground(relief(std::move(finish), {.shoulder = 1.8f, .depth = .8f}));
}

struct PigmentBrushes {
  Controls controls;
  int heldPhase = -1;
  int phaseIndex = 0;
  motion::Animatable<float> bearing = motion::animatable(140.0f);
  motion::Animatable<float> surround = motion::animatable(0.0f);

  Controls stateFor(int phase) const {
    Controls c = controls;
    if (phase == 1) c.ridge = 0;
    if (phase == 2) c.roughness = 0;
    if (phase == 3) c.roughness = 1;
    if (phase == 4) c.overprint = 0;
    if (phase == 5) c.overprint = 1;
    return c;
  }
  material::Lighting light(bool moving = true) const {
    return material::Lighting{
        material::studio(
            {.direction = moving ? bearing : motion::Animatable<float>(140),
             .elevation = 23,
             .intensity = .72f,
             .ambient = .52f}),
        material::environment(
            material::shader(kEnvironment),
            {.rotation = moving ? surround : motion::Animatable<float>(0),
             .intensity = 1.05f,
             .size = {512, 256}})};
  }
  Element hero(const Controls& c) const {
    auto gold = kGold;
    gold.a = c.overprint;
    auto blue = paint(kBlue, c).effects(
        material::Filter::shadow(material::Color{.015f, .027f, .04f, .26f},
                                 {.blur = 3, .offset = {1, 3}}));
    const std::string spine = "M0 65 C18 23 35 10 57 28 C75 46 83 75 100 41";
    return kit::at(0, 0, 942, 610)
        .overflow(Overflow::Clip)
        .fill(material::from(kPaper).layer(
            material::noise(.08f, {.octaves = 2, .seed = 41, .grain = true}),
            {.blend = material::BlendMode::Multiply, .opacity = .08f}))
        .children(
            {label("PIGMENT / WORKING SHEET 07", 24, 18, 650, 10, kInk, 600),
             label("LAYER 1 → 4", 786, 18, 145, 10, kInk, 600),
             kit::at(42, 133, 849, 172)
                 .shape(shapes::svg(spine))
                 .stroke(brush::layers(
                     {ribbon(124,
                             paint({.075f, .095f, .12f, 1}, c, false, true)),
                      ribbon(105, blue),
                      ribbon(39, paint(gold, c, true, false, 29))})),
             mark(126, 270, 685, 153,
                  "M0 9 C21 50 43 80 64 34 C82 -2 92 48 100 71", 79,
                  paint(kRed, c, false, true, 37)),
             mark(61, 354, 820, 82,
                  "M0 76 C30 12 48 2 66 30 C85 58 88 83 100 9", 20,
                  paint(gold, c, true, false, 43)),
             raised("BODY", 49, 57, 818, 119,
                    paint({.91f, .88f, .8f, 1}, c, false, false, 11)),
             raised("& TRACE", 321, 401, 588, 70,
                    paint(kBlue, c, false, false, 23)),
             label("A brush can carry a surface.", 26, 500, 857, 26, kInk, 520),
             label("Dry pigment below. Wet ridges above. Metal crossing both.",
                   27, 544, 850, 13, kMuted),
             rule(25, 579, 893),
             label("HOG BRISTLE / OIL / COLD WAX / METALLIC OVERPRINT", 27, 589,
                   810, 9)});
  }
  Element inspector(int phase, const Controls& c) const {
    std::vector<Element> parts{
        label("MATERIAL WORKSHOP", 0, 0, 345, 11, kRed, 700),
        label(kPhases[phase], 0, 28, 345, 24, kInk, 620),
        label(kDetails[phase], 0, 73, 355, 11),
        rule(0, 112, 355),
        label("RIDGE NORMAL", 0, 133, 231, 10),
        label(decimal(c.ridge), 272, 127, 80, 24, kInk),
        label("WET ROUGHNESS", 0, 180, 231, 10),
        label(decimal(c.roughness), 272, 174, 80, 24, kInk),
        label("METAL OVERPRINT", 0, 227, 231, 10),
        label(decimal(c.overprint), 272, 221, 80, 24, kInk),
        rule(0, 270, 355),
        label("MATERIAL LINE WEIGHTS", 0, 287, 355, 10, kInk, 650)};
    for (int i = 0; i < 4; ++i) {
      const float width = std::pow(2.0f, static_cast<float>(i));
      parts.push_back(mark(51, 324 + i * 35.0f, 281, 6,
                           "M0 30 C24 0 70 90 100 40", width,
                           paint(kGold, c, true, false, 31)));
      parts.push_back(label(std::to_string(static_cast<int>(width)) + " PX", 0,
                            316 + i * 35.0f, 53, 10, kMuted));
    }
    parts.push_back(
        label("O8 Aa", 0, 475, 355, 64, kInk, 720).ink(paint(kGold, c, true)));
    parts.push_back(
        label("Live surface ink / open glyph counters", 0, 560, 355, 11));
    for (int i = 0; i < 6; ++i)
      parts.push_back(
          kit::at(i * 59.0f, 599, 43, 4).fill(i == phase ? kRed : kMuted));
    return box().width(355).height(610).children(std::move(parts));
  }
  Element coupon(int i) const {
    constexpr std::array names{"DRY / ROUGH", "WET / RIDGES", "METAL / LAYERS",
                               "ALPHA / OVERLAP", "CLIP / COUNTERS"};
    Controls c = controls;
    auto ink = i < 2 ? kBlue : kGold;
    if (i == 3) ink.a = .48f;
    Element sample = kit::at(0, 28, 254, 156)
                         .overflow(Overflow::Clip)
                         .fill(material::Color{.86f, .82f, .72f, 1})
                         .lighting(light(false));
    sample.children(
        {mark(-23, 49, 288, 46, "M0 55 C32 0 65 90 100 20", i == 4 ? 9 : 58,
              paint(ink, c, i > 1, i == 0, 11 + i))});
    if (i > 1)
      sample.children({mark(32, 46, 228, 52, "M0 0 C40 80 70 0 100 70", 25,
                            paint(kRed, c, false, false, 39))});
    sample.children({label("O8", 23, 26, 220, i == 4 ? 57 : 53, kInk, 750)
                         .ink(paint(ink, c, i > 1, i == 0, 23))});
    if (i == 4)
      sample.children({mark(-11, 131, 277, 12, "M0 80 C20 -10 70 150 100 0",
                            .75f, paint(kGold, c, true))});
    return kit::at(i * 276.0f, 0, 254, 213)
        .children({label(names[i], 0, 0, 254, 10, kInk, 650), std::move(sample),
                   label(i == 4 ? "Negative origins / 0.75 px / O and 8 holes"
                                : "Same finish on native brush and text ink",
                         0, 197, 260, 9)});
  }
  Element describe(int phase) const {
    const Controls c = stateFor(phase);
    std::vector<Element> samples;
    for (int i = 0; i < 5; ++i) samples.push_back(coupon(i));
    return box()
        .width(1440)
        .height(1080)
        .fontFamily("Inter, Helvetica Neue, sans-serif")
        .fill(kPaper)
        .lighting(light())
        .children(
            {label("ATELIER / 07  •  PIGMENT, BODY & TRACE", 40, 24, 930, 11,
                   kRed, 650),
             label("The mark is the material.", 36, 52, 1190, 49, kInk, 650),
             label("COMPOSE  /  BRUSHES + LINES + LETTERS", 1033, 29, 366, 10,
                   kInk, 650),
             label("24 S / SIX MATERIAL STATES", 1125, 70, 274, 10),
             rule(40, 133, 1359), kit::at(hero(c), 40, 162, 942, 610),
             kit::at(inspector(phase, c), 1037, 162, 355, 610),
             label("THE SAME VOCABULARY, AT SMALLER MEASURES", 40, 801, 998, 10,
                   kInk, 650),
             label("FIXED COMPARISONS / IDENTICAL LIGHT", 1085, 801, 315, 9),
             rule(40, 827, 1359),
             kit::at(box().width(1358).height(213).children(std::move(samples)),
                     40, 844, 1358, 213)});
  }
  void setup(sketch::SketchContext& ctx) {
    ctx.canvas(1440, 1080);
    ctx.background(kPaper);
    ctx.captureAt(2);
    phaseIndex = heldPhase < 0 ? 0 : std::clamp(heldPhase, 0, 5);
    ctx.composer.render(describe(phaseIndex));
  }
  void update(double elapsed, sketch::SketchContext& ctx) {
    const double t = std::isfinite(elapsed) ? std::max(0.0, elapsed) : 0;
    const double loop = std::fmod(t, 24.0);
    bearing =
        140 + 32 * static_cast<float>(std::sin(loop * 6.28318530718 / 24));
    surround = 20 * static_cast<float>(std::sin(loop * 6.28318530718 / 24));
    const int next = heldPhase < 0 ? static_cast<int>(loop / 4)
                                   : std::clamp(heldPhase, 0, 5);
    if (next != phaseIndex) {
      phaseIndex = next;
      ctx.composer.render(describe(phaseIndex));
    }
  }
};
}  // namespace

SIGIL_SKETCH(PigmentBrushes, "Study · Materials",
             "Layered pigment brushes, metallic lines and material lettering "
             "under a moving light.")
