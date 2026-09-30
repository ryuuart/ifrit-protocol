#pragma once

// THE PAGE'S GIFS, drawn as trees in 1996 pixels. Each is rasterised once
// at its own size and decoded the way the browser decoded it (see
// `decodeGif` in the entry), so what is here is the artwork as its artist
// drew it, before the file format had its say.

#include <include/core/SkCanvas.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/Measure.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Frame.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Face.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace spacejam {

// ---------------------------------------------------------------------------
// Colour. Every distinct component in the twelve navigation palettes sits
// on the 5-bit grid: c8 -> round(c8 * 31 / 255) -> (i << 3) | (i >> 2).

inline float snapFive(float value) {
  const int level = (int)std::lround(std::clamp(value, 0.0f, 1.0f) * 31.0f);
  return (float)(((uint32_t)level << 3u) | ((uint32_t)level >> 2u)) / 255.0f;
}
/** A navigation-art colour, snapped to the grid the shipped art lives on. */
inline material::Color C5(uint32_t rgb) noexcept {
  return {snapFive((float)((rgb >> 16u) & 0xffu) / 255.0f),
          snapFive((float)((rgb >> 8u) & 0xffu) / 255.0f),
          snapFive((float)(rgb & 0xffu) / 255.0f), 1.0f};
}

/** What a label says and the ink it is set in. Twelve labels, and the
 *  page's only typographic variation is that three of them are white. */
struct Label {
  std::string text;
  material::Color ink;
};
const material::Color kLabelInk = C5(0x080800);

inline weave::Face display() { return weave::ports::face({"Impact", "Arial Black"}, 400); }

inline weave::Type type(float size, material::Color color, float track = 0) {
  return {.face = display(), .size = size, .color = color, .track = track};
}

// ---------------------------------------------------------------------------
// Geometry

inline Element rect(float left, float top, float width, float height) {
  return kit::at(left, top, width, height);
}

/** A shaded sphere: every planet here is flat-shaded with a hard limb. */
inline Element sphere(SkPoint centre, float radius, material::Material paint) {
  return kit::dot(sigil::geometry::path::fromSk(centre), radius, std::move(paint));
}

/** A ring seen edge-on: an annulus on a squashed, rotated box. */
inline Element ring(SkPoint centre, float radiusX, float radiusY, float degrees,
                    float innerRatio, material::Material paint) {
  return rect(centre.fX - radiusX, centre.fY - radiusY, radiusX * 2, radiusY * 2)
      .shape(shapes::annulus(innerRatio))
      .fill(std::move(paint))
      .rotate(degrees);
}

/** A polygon in unit-box coordinates, stretched over the node. */
inline Shape unitPolygon(std::vector<glm::vec2> corners) {
  return [corners = std::move(corners)](glm::vec2 size) {
    SkPathBuilder builder;
    for (size_t index = 0; index < corners.size(); ++index) {
      const SkPoint point{corners[index].x * size.x, corners[index].y * size.y};
      index == 0 ? builder.moveTo(point) : builder.lineTo(point);
    }
    builder.close();
    return sigil::geometry::path::fromSk(builder.detach());
  };
}

inline Element artBox(float width, float height) {
  return stack().width(width).height(height).overflow(Overflow::Clip);
}

// ---------------------------------------------------------------------------
// The basketball: a sphere with rotating seams, one program for every
// build of it. `uSpin` is the frame's angle in GIF frames of 60 degrees.

inline material::Material ballMaterial(const sk_sp<SkRuntimeEffect>& program, float spin,
                                       material::Color light, material::Color dark,
                                       material::Color seam, float seamWidth) {
  material::Paint paint = material::skia::sksl(program, {{"uSeamW", seamWidth}});
  paint.set("uHi", light);
  paint.set("uLo", dark);
  paint.set("uSeam", seam);
  paint.set("uSpin", spin);
  return material::skia::base(paint);
}

/** fastbreak.gif as its frames, side by side: one bounce of the ball in a
 *  40x40 frame, turning 60 degrees a frame, flattened where it meets the
 *  floor. Six frames at the GIF's 100 ms delay, looped forever. */
constexpr int kBallFrames = 6;
constexpr float kBallFrame = 40;
inline Element ballFilmstrip(const sk_sp<SkRuntimeEffect>& program) {
  Element strip = stack().width(kBallFrame * kBallFrames).height(kBallFrame);
  for (int frame = 0; frame < kBallFrames; ++frame) {
    const float lift = std::sin(3.14159265f * (float)frame / kBallFrames);
    const float radius = 13;
    const bool landing = frame == 0;
    Element ball = rect(frame * kBallFrame + kBallFrame / 2 - radius,
                        kBallFrame - 2 - radius * 2 - 13 * lift, radius * 2, radius * 2)
                       .shape(shapes::circle())
                       .fill(ballMaterial(program, 0.1f * (float)frame, C5(0xFF6B29),
                                          C5(0xC64210), C5(0x521800), 0.05f));
    if (landing) ball.scaleX(1.14f).scaleY(0.84f).transformOrigin(pct(50), pct(100));
    strip.children({std::move(ball)});
  }
  return strip;
}

// ---------------------------------------------------------------------------
// Gas-giant banding: torn wavy streaks, drawn as an overlay() so it paints
// over the body fill and under the sphere-shading child.

inline float hashUnit(uint32_t value) {
  value = (value ^ 61u) ^ (value >> 16u);
  value *= 9u;
  value ^= value >> 4u;
  value *= 0x27d4eb2du;
  value ^= value >> 15u;
  return (float)(value & 0xffffffu) / (float)0xffffff;
}

struct Bands {
  std::vector<material::Color> inks;
  int count = 6;
  uint32_t seed = 1;
  float thick = 0.11f;    // fraction of the box height
  float wobble = 0.055f;  // vertical excursion
  float tear = 0.55f;     // how much the thickness pinches along x
  float bow = 0.20f;      // limb curvature
  float tilt = 0.0f;      // streaks running off the horizontal

  void paint(sigil::draw::Pen& pen, const PaintContext& context) const {
    SkCanvas& canvas = *pen.canvas();
    const float width = context.size.x, height = context.size.y;
    if (width <= 0 || height <= 0 || inks.empty()) return;
    canvas.save();
    canvas.clipPath(sigil::geometry::path::toSk(context.outline), true);
    SkPaint ink;
    ink.setAntiAlias(true);
    constexpr float kTurn = 6.2831853f;
    for (int band = 0; band < count; ++band) {
      const uint32_t key = seed * 131u + (uint32_t)band * 7919u;
      const float centre = height * (0.10f + 0.80f * ((float)band + 0.5f) / (float)count +
                                     (hashUnit(key) - 0.5f) * 0.05f);
      const float thickness = height * thick * (0.55f + 0.9f * hashUnit(key + 1u));
      const float swell = height * wobble * (0.5f + hashUnit(key + 2u));
      const float ripple = height * wobble * 0.45f * hashUnit(key + 3u);
      const float swellFrequency = 0.9f + 1.1f * hashUnit(key + 4u);
      const float rippleFrequency = 2.2f + 1.8f * hashUnit(key + 5u);
      const float swellPhase = hashUnit(key + 6u) * kTurn;
      const float ripplePhase = hashUnit(key + 7u) * kTurn;
      const float tearFrequency = 1.4f + 1.6f * hashUnit(key + 8u);
      const float tearPhase = hashUnit(key + 9u) * kTurn;
      const float latitude = (centre - height * 0.5f) / (height * 0.5f);

      std::vector<SkPoint> upper, lower;
      constexpr int kSteps = 72;
      for (int step = 0; step <= kSteps; ++step) {
        const float along = (float)step / (float)kSteps;
        const float horizontal = along * width;
        const float across = (horizontal - width * 0.5f) / (width * 0.5f);
        const float wave = swell * std::sin(swellFrequency * along * kTurn + swellPhase) +
                           ripple * std::sin(rippleFrequency * along * kTurn + ripplePhase);
        const float bend = bow * height * latitude * across * across + tilt * height * across * 0.5f;
        const float pinch =
            std::max(0.06f, 1.0f - tear * (0.5f + 0.5f * std::sin(tearFrequency * along * kTurn + tearPhase)));
        const float middle = centre + wave + bend;
        const float half = thickness * 0.5f * pinch;
        upper.push_back({horizontal, middle - half});
        lower.push_back({horizontal, middle + half});
      }
      SkPathBuilder ribbon;
      ribbon.moveTo(upper.front());
      for (const SkPoint& point : upper) ribbon.lineTo(point);
      for (auto point = lower.rbegin(); point != lower.rend(); ++point) ribbon.lineTo(*point);
      ribbon.close();
      ink.setColor4f(material::skia::toSkColor(inks[(size_t)band % inks.size()]), nullptr);
      canvas.drawPath(ribbon.detach(), ink);
    }
    canvas.restore();
  }
};

// ---------------------------------------------------------------------------
// The star tile: ONE 111x111 GIF, repeated on a visible lattice. The 33
// local maxima at L >= 45 (x, y, peak) are pixel-sampled off bg_stars.gif;
// the same stars repeat every 111 px, and that repetition is part of how
// the page looks.

struct Star {
  int column, row, peak;
};
inline constexpr auto kStarField = std::to_array<Star>(
    {{96, 64, 253}, {69, 9, 244},  {59, 101, 238},  {44, 43, 235},  {9, 104, 233},
     {3, 66, 222},  {89, 79, 219}, {40, 105, 209},  {14, 48, 205},  {16, 12, 194},
     {50, 65, 188}, {52, 30, 161}, {15, 85, 153},   {28, 52, 150},  {102, 102, 145},
     {73, 87, 143}, {38, 24, 140}, {94, 22, 138},   {107, 64, 133}, {43, 13, 130},
     {11, 32, 129}, {85, 45, 128}, {32, 84, 127},   {61, 36, 125},  {13, 5, 120},
     {107, 1, 112}, {98, 46, 112}, {22, 70, 110},   {86, 21, 109},  {68, 68, 97},
     {48, 80, 97},  {40, 0, 72},   {107, 110, 68}});
constexpr float kStarTile = 111;

inline Element starTile() {
  Element tile = stack().width(kStarTile).height(kStarTile);
  int rank = 0;
  for (const Star& star : kStarField) {
    const float light = (float)star.peak / 255.0f;
    const glm::vec2 centre{(float)star.column + 0.5f, (float)star.row + 0.5f};
    // Most of the tile sits below L16: the density on the page comes from
    // repetition, not brightness, so the glow falls off steeply.
    tile.children({kit::disc(centre, 2.3f + 7.0f * light * light)
                       .fill(material::radialGradient(
                           {0.5f, 0.5f}, 1.0f,
                           {{0.0f, {light, light, light, 1.0f}},
                            {0.24f, {light, light, light, 0.66f}},
                            {0.44f, {light, light, light, 0.26f}},
                            {0.70f, {light, light, light, 0.055f}},
                            {1.0f, {light, light, light, 0.0f}}},
                           {.extent = material::RadialExtent::ClosestSide}))
                       .blendMode(material::BlendMode::PlusLighter)});
    // The brightest half-dozen carry an eight-point spike, the next few a
    // four-point cross: on this tile the spikes are the dominant visual.
    const bool eight = rank < 6, four = rank >= 6 && rank < 14;
    ++rank;
    if (eight || four)
      tile.children({kit::disc(centre, eight ? 4.8f + 6.6f * light : 4.2f + 6.0f * light)
                         .shape(shapes::star(eight ? 8 : 4, 0.035f, eight ? 0.15f : 0.12f))
                         .fill(Fill::color({1, 1, 1, 0.38f + 0.42f * light}))
                         .blendMode(material::BlendMode::PlusLighter)});
  }
  return tile;
}

// ---------------------------------------------------------------------------
// A navigation label. The shipped ones are set much narrower than anything
// on the system: "SITE MAP" is eight glyphs in 37 px at a 10 px cap. So the
// run is sized by its cap band first, condensed with scaleX down to a 0.70
// floor, and only then gives up cap height. Its outline is eight echoes at
// one pixel and a ninth down-right: a 1 px black keyline plus a 1 px drop.

inline Element navLabel(weave::FontContext& fonts, const Label& label, float left, float top,
                        float width, float capHeight) {
  auto styleAt = [&](float size) { return type(size, label.ink, 0.4f); };
  float size = capHeight / 0.72f;  // Impact's cap height is about 0.72 em
  SkSize measured = intrinsicSize(text(label.text).font(styleAt(size)), fonts);
  float condense = 1.0f;
  if (measured.width() > width && measured.width() > 1) {
    condense = width / measured.width();
    if (condense < 0.70f) {
      size *= condense / 0.70f;
      measured = intrinsicSize(text(label.text).font(styleAt(size)), fonts);
      condense = (measured.width() > width && measured.width() > 1) ? width / measured.width() : 1.0f;
    }
  }
  material::Filter keyline;
  const float offsets[9][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1},
                               {1, -1}, {-1, 1}, {1, 1},  {2, 2}};
  for (const auto& offset : offsets)
    keyline = keyline.then(material::Filter::shadow(kLabelInk, {.offset = {offset[0], offset[1]}}));
  // scaleX is paint-only, so the node is pinned to the run's natural width
  // to keep it one line, and the paint-time condense brings it inside.
  Element run = text(label.text).font(styleAt(size));
  run.ink(material::from(label.ink).effects(keyline));
  run.left(left).top(top).width(measured.width() + 4.0f);
  if (condense < 0.999f) run.scaleX(condense).transformOrigin(pct(0), pct(50));
  return run;
}

// ---------------------------------------------------------------------------
// The visitor counter: an odometer GIF from the counter service, seven
// white figures in black wells with a lit top and a shaded foot.

constexpr float kCounterDigit = 11, kCounterHeight = 17;
inline Element artCounter(weave::FontContext&, const std::string& figures) {
  Element counter = stack().width(kCounterDigit * (float)figures.size()).height(kCounterHeight);
  for (size_t index = 0; index < figures.size(); ++index) {
    counter.children(
        {rect((float)index * kCounterDigit, 0, kCounterDigit, kCounterHeight)
             .fill(material::linearGradient({0, 0}, {0, 1},
                                            {{0.0f, C5(0x6B6B6B)},
                                             {0.18f, C5(0x101010)},
                                             {0.82f, C5(0x000000)},
                                             {1.0f, C5(0x424242)}}))
             .stroke(stroke(1, Fill::color(C5(0x9C9C9C)), PathFormat::Align::Inner)),
         text(std::string(1, figures[index]))
             .font({.face = weave::ports::face({"Courier New"}, 700),
                    .size = 15,
                    .color = C5(0xFFFFFF),
                    .aliased = true})
             .left((float)index * kCounterDigit + 1.5f)
             .top(-0.5f)});
  }
  return counter;
}

// ---------------------------------------------------------------------------
// The twelve navigation GIFs, at the sizes the page's <IMG> tags give them.

// --- p-souvenirs.gif, 83x83 — the centred glow.
inline Element artSouvenirs(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 83, height = 83;
  return artBox(width, height).children(
      {sphere({41.5f, 47.5f}, 35,
              sigil::material::radialGradient(
                  {0.5f, 0.5f}, 1.0f,
                  {{0.0f, C5(0xEFEFEF)},
                   {0.16f, C5(0xDEEFEF)},
                   {0.52f, C5(0x29EFEF)},
                   {0.80f, C5(0x08C6C6)},
                   {1.0f, C5(0x006363)}},
                  {.extent = material::RadialExtent::ClosestSide}))
           .stroke(stroke(1.5f, Fill::color(C5(0x005252)),
                          PathFormat::Align::Inner)),
       navLabel(fonts, label, 0, -1, width, 10)});
}

// --- p-jump.gif, 58x52 — the other centred glow.
inline Element artJump(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 58, height = 52;
  return artBox(width, height).children(
      {sphere({28.5f, 30.0f}, 21,
              sigil::material::radialGradient(
                  {0.46f, 0.60f}, 1.0f,
                  {{0.0f, C5(0xFFFFFF)},
                   {0.22f, C5(0xADF7A5)},
                   {0.52f, C5(0x39D631)},
                   {0.86f, C5(0x009400)},
                   {1.0f, C5(0x006B00)}},
                  {.extent = material::RadialExtent::ClosestSide}))
           .stroke(stroke(1.5f, Fill::color(C5(0x005A00)),
                          PathFormat::Align::Inner)),
       navLabel(fonts, label, 0, 0, width, 10)});
}

// --- p-bball.gif, 62x62 — the static build of the seam shader.
inline Element artBball(sigil::weave::FontContext& fonts, const Label& label,
                        const sk_sp<SkRuntimeEffect>& ball) {
  const float width = 62, height = 62;
  return artBox(width, height).children(
      {kit::dot(glm::vec2{31, 37.5f}, 25.5f,
                ballMaterial(ball, 0.083f, C5(0xFF9C10), C5(0xC66300),
                             C5(0x843900), 0.055f))
           .stroke(stroke(1.2f, Fill::color(C5(0x632900)),
                          PathFormat::Align::Inner)),
       navLabel(fonts, label, 0, 0, width, 10)});
}

// --- p-jamcentral.gif, 55x67 — purple globe, green continents.
inline Element artJamCentral(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 55, height = 67;
  const SkPoint centre{27.5f, 40};
  const float radius = 26;
  Element globe = sphere(centre, radius,
                         sigil::material::radialGradient(
                             {0.34f, 0.28f}, 1.32f,
                             {{0.0f, C5(0xA542DE)},
                              {0.30f, C5(0x8418CE)},
                              {0.62f, C5(0x7B10C6)},
                              {1.0f, C5(0x630894)}},
                             {.extent = material::RadialExtent::ClosestSide}))
                      .overflow(Overflow::Clip)
                      .stroke(stroke(1.5f, Fill::color(C5(0x9400DE)),
                                     PathFormat::Align::Inner));
  // Six landmasses, seeded blobs clipped to the disc. Their coordinates are
  // fractions of the disc box (2r), not of the image box.
  const float diameter = radius * 2;
  struct Mass {
    float left, top, across, down;
    uint32_t seed;
    uint32_t ink;
  };
  const Mass masses[6] = {{0.04f, 0.10f, 0.44f, 0.26f, 11, 0x08E700},
                        {0.14f, 0.30f, 0.26f, 0.52f, 27, 0x00EF00},
                        {0.46f, 0.14f, 0.44f, 0.26f, 43, 0x00EF00},
                        {0.50f, 0.36f, 0.30f, 0.50f, 61, 0x08E700},
                        {0.74f, 0.28f, 0.24f, 0.30f, 83, 0x08E700},
                        {0.28f, 0.80f, 0.34f, 0.16f, 97, 0x00EF00}};
  for (const Mass& land : masses)
    globe.children({rect(land.left * diameter, land.top * diameter, land.across * diameter,
                         land.down * diameter)
                        .shape(shapes::blob(land.seed, 0.62f, 13))
                        .fill(Fill::color(C5(land.ink)))});
  return artBox(width, height).children(
      {std::move(globe), navLabel(fonts, label, 0, -1, width, 10)});
}

/** The shared gas-giant recipe: solid body, torn bands as an overlay(),
 *  and one shading child for the highlight and the hard limb. */
inline Element gasGiant(SkPoint centre, float radius, sigil::material::Color body,
                        sigil::material::Color limb, sigil::material::Color highlight,
                        Bands bands) {
  Element planet =
      sphere(centre, radius, body)
          .overflow(Overflow::Clip)
          .overlay(std::move(bands))
          .stroke(stroke(1.5f, Fill::color(limb), PathFormat::Align::Inner));
  planet.children({box().inset(0).fill(material::radialGradient(
      {0.34f, 0.28f}, 1.35f,
      {{0.0f, sigil::material::withAlpha(highlight, 0.42f)},
       {0.34f, sigil::material::withAlpha(highlight, 0.10f)},
       {0.62f, {0, 0, 0, 0}},
       {0.90f, {0, 0, 0, 0.30f}},
       {1.0f, {0, 0, 0, 0.62f}}},
      {.extent = material::RadialExtent::ClosestSide}))});
  return planet;
}

// --- p-junior.gif, 49x57 — green body, yellow bands.
inline Element artJunior(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 49, height = 57;
  Bands bands{{C5(0xFFFF00), C5(0xBDF700), C5(0xA5F700), C5(0x7BEF00), C5(0xFFFF00),
           C5(0xBDF700)},
          6,
          5,
          0.135f,
          0.040f,
          0.42f,
          0.24f,
          -0.05f};
  return artBox(width, height).children(
      {gasGiant({24, 34}, 22.5f, C5(0x00DE00), C5(0x005A00),
                C5(0xCEFFB5), std::move(bands)),
       navLabel(fonts, label, 0, -1, width, 10)});
}

// --- p-studiostore.gif, 94x72 — orange body, purple streaks, torn hard.
inline Element artStudioStore(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 94, height = 72;
  Bands bands{
      {C5(0x7310C6), C5(0x6B08CE), C5(0x7310C6), C5(0x6B08CE), C5(0x7310C6)},
      5,
      17,
      0.150f,
      0.055f,
      0.60f,
      0.28f,
      -0.07f};
  return artBox(width, height).children(
      {gasGiant({48, 42}, 29.5f, C5(0xFF9400), C5(0xE7A518),
                C5(0xFFDE9C), std::move(bands)),
       navLabel(fonts, label, 0, -1, width, 10)});
}

// --- p-behind.gif, 67x63 — navy body, four thin cyan cloud streaks.
inline Element artBehind(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 67, height = 63;
  Bands bands{{C5(0x21FFFF), C5(0x21FFFF), C5(0x18E7EF), C5(0x21FFFF)},
          4,
          31,
          0.085f,
          0.045f,
          0.52f,
          0.30f,
          -0.17f};
  return artBox(width, height).children(
      {gasGiant({33, 37}, 25.5f, C5(0x000873), C5(0x0010BD),
                C5(0x2131C6), std::move(bands)),
       navLabel(fonts, label, 0, -1, width, 10)});
}

// --- p-lunartunes.gif, 95x77 — blue Saturn, red ring: two arcs and a
// --- z-order, since the ring passes behind at the top and in front below.
inline Element artLunarTunes(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 95, height = 77;
  const SkPoint centre{48, 46};
  auto ringMat = [] {
    return sigil::material::linearGradient({0, 0}, {0, 1},
                                                 {{0.0f, C5(0xF71018)},
                                                  {0.38f, C5(0xF773A5)},
                                                  {0.62f, C5(0xF71818)},
                                                  {1.0f, C5(0xAD0810)}});
  };
  return artBox(width, height).children(
      {ring(centre, 47, 16, -20, 0.62f, ringMat()).zIndex(0),
       sphere(centre, 30,
              sigil::material::radialGradient(
                  {0.34f, 0.28f}, 1.32f,
                  {{0.0f, C5(0x0073E7)},
                   {0.30f, C5(0x006BD6)},
                   {0.66f, C5(0x0052AD)},
                   {1.0f, C5(0x00317B)}},
                  {.extent = material::RadialExtent::ClosestSide}))
           .stroke(stroke(1.5f, Fill::color(C5(0x00397B)),
                          PathFormat::Align::Inner))
           .zIndex(1),
       // the front half: the same ellipse, clipped to below the sphere's centre
       rect(0, centre.fY, width, height - centre.fY)
           .overflow(Overflow::Clip)
           .zIndex(2)
           .children({ring({centre.fX, -0}, 47, 16, -20, 0.62f, ringMat())
                          .top(-centre.fY)}),
       navLabel(fonts, label, 19, 0, 57, 9)});
}

// --- p-lineup.gif, 63x52 — the same construction, red and cyan.
inline Element artLineup(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 63, height = 52;
  const SkPoint centre{33, 31};
  auto ringMat = [] {
    return sigil::material::linearGradient({0, 0}, {0, 1},
                                                 {{0.0f, C5(0x21FFFF)},
                                                  {0.45f, C5(0x9CFFFF)},
                                                  {0.75f, C5(0x21FFFF)},
                                                  {1.0f, C5(0x089494)}});
  };
  return artBox(width, height).children(
      {ring(centre, 29, 15, -22, 0.60f, ringMat()).zIndex(0),
       sphere({38, 32}, 17,
              sigil::material::radialGradient(
                  {0.34f, 0.30f}, 1.30f,
                  {{0.0f, C5(0xFF4A6B)},
                   {0.28f, C5(0xFF425A)},
                   {0.62f, C5(0xF71818)},
                   {1.0f, C5(0xBD0810)}},
                  {.extent = material::RadialExtent::ClosestSide}))
           .stroke(stroke(1.4f, Fill::color(C5(0xA50008)),
                          PathFormat::Align::Inner))
           .zIndex(1),
       rect(0, 34, width, height - 34)
           .overflow(Overflow::Clip)
           .zIndex(2)
           .children({ring({centre.fX, 0}, 29, 15, -22, 0.60f, ringMat())
                          .top(centre.fY - 34 - 15)}),
       navLabel(fonts, label, 8, -1, 50, 9)});
}

// --- p-sitemap.gif, 104x67 — a rainbow vortex and four yellow darts.
inline Element artSitemap(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 104, height = 67;
  const SkPoint centre{36, 32};
  // The closest side, not the farthest corner: on a 2:1 box a radius
  // against the corner is a fraction of the HALF-DIAGONAL, so the whole
  // band stack lands inside t < 0.71 and the outer bands never appear.
  // Against the closest side the radius is the half side, so t = 1 IS the ellipse edge and the bands sit where
  // they were authored.
  Element vortex = rect(centre.fX - 35, centre.fY - 17, 70, 34)
                       .shape(shapes::annulus(0.30f))
                       .fill(material::radialGradient(
                           {0.5f, 0.5f}, 1.0f,
                           {{0.0f, C5(0xFFFF00)},
                            {0.34f, C5(0xFFEF00)},
                            {0.52f, C5(0xFFAD42)},
                            {0.68f, C5(0xFF5A00)},
                            {0.86f, C5(0xF70000)},
                            {1.0f, C5(0x8C0000)}},
                           {.extent = material::RadialExtent::ClosestSide}))
                       .rotate(-33);
  Element out = artBox(width, height).children({std::move(vortex)});
  // four darts, outside the vortex on its two axes
  const float dartWidth = 17, dartHeight = 16;
  const float angles[4] = {122, -58, 210, 30};
  const float acrossAxis[4] = {0.28f, -0.28f, 0.86f, -0.86f};
  const float alongAxis[4] = {-0.95f, 0.95f, 0.42f, -0.42f};
  for (int index = 0; index < 4; ++index)
    out.children({rect(centre.fX + acrossAxis[index] * 34 - dartWidth * 0.5f,
                       centre.fY + alongAxis[index] * 31 - dartHeight * 0.5f, dartWidth, dartHeight)
                      .shape(unitPolygon({{1, 0.5f}, {0, 0}, {0.34f, 0.5f}, {0, 1}}))
                      .fill(Fill::color(C5(0xFFFF00)))
                      .rotate(angles[index])});
  out.children({navLabel(fonts, label, 64, 26, 39, 10)});
  return out;
}

// --- p-pressbox.gif, 131x56 — the one genuinely photographic asset, and
// --- the one deliberate approximation: a lozenge fuselage and swept fins.
inline Element artPressBox(sigil::weave::FontContext& fonts, const Label& label) {
  const float width = 131, height = 56;
  const sigil::material::Color hull = C5(0xFF0042), hullLo = C5(0xCE0031),
                               hullHi = C5(0xFF8CA5), green = C5(0x319431),
                               greenShade = C5(0x101800), gold = C5(0xFFFF00);

  // The measured body axis runs from the tail at (4.5, 41) to the nose at
  // (125, 14): atan2(-27, 120) = -12.6 degrees.
  Element ship =
      rect(0, 0, width, height).rotate(-12.6f).transformOrigin(pct(50), pct(50));
  // dorsal fin, swept back from mid-body
  ship.children(
      {rect(38, 6, 52, 20)
           .shape(unitPolygon({{1, 1}, {0.86f, 0}, {0, 1}}))
           .fill(material::linearGradient(
               {0, 0}, {0, 1}, {{0.0f, C5(0xF71039)}, {1.0f, hullLo}})),
       // ventral fin
       rect(58, 36, 40, 15)
           .shape(unitPolygon({{0, 0}, {1, 0}, {0.62f, 1}}))
           .fill(Fill::color(C5(0xA50029))),
       // rear nacelle
       rect(4, 25, 36, 14)
           .shape(shapes::squircle(2.6f))
           .fill(material::linearGradient(
               {0, 0}, {0, 1},
               {{0.0f, C5(0x8CDE73)}, {0.42f, green}, {1.0f, greenShade}})),
       // fuselage
       rect(16, 23, 100, 17)
           .shape(shapes::squircle(2.2f))
           .fill(material::linearGradient({0, 0}, {0, 1},
                                                       {{0.0f, hullHi},
                                                        {0.26f, hull},
                                                        {0.68f, hullLo},
                                                        {1.0f, C5(0x8C0021)}})),
       // dorsal ridge highlight
       rect(28, 25, 72, 3)
           .shape(shapes::squircle(2.0f))
           .fill(Fill::color(sigil::material::withAlpha(C5(0xFFC6D6), 0.85f))),
       // nose spike
       rect(108, 27, 24, 8)
           .shape(shapes::arrow(0.28f, 0.90f))
           .fill(Fill::color(hull))});
  // window strip
  for (int index = 0; index < 5; ++index)
    ship.children({rect(44 + index * 7.0f, 29, 4, 4)
                       .borderRadius({1})
                       .fill(Fill::color(gold))});

  return artBox(width, height).children(
      {std::move(ship), navLabel(fonts, label, 50, 38, 80,
                                 10)});
}

// --- p-jamlogo.gif, 272x165 — the largest object on the page by a factor
// --- of four, and the one a viewer judges the sketch on.
inline Element artLogo(sigil::weave::FontContext& fonts, const Label&) {
  const float width = 272, height = 165;
  // Swirl geometry measured off p-jamlogo_x2: centre ~(172, 62) in the
  // 272x165 box, spanning x 65..250 and y 5..145.
  const SkPoint centre{176, 54};
  const float radiusX = 92, radiusY = 58;

  auto swirlFill = [] {
    // The closest side again, for the reason artSitemap() gives: on this
    // 1.4:1 box the farthest corner would put the whole rainbow inside
    // t < 0.71 and the outer band would never draw.
    return sigil::material::radialGradient(
        {0.5f, 0.5f}, 1.0f,
        {{0.0f, C5(0x101831)},
         {0.44f, C5(0x21103A)},
         {0.56f, C5(0xFFEF00)},
         {0.70f, C5(0xFFAD42)},
         {0.86f, C5(0xF70000)},
         {1.0f, C5(0x7310C6)}},
        {.extent = material::RadialExtent::ClosestSide});
  };
  auto swirl = [&] {
    return rect(centre.fX - radiusX, centre.fY - radiusY, radiusX * 2, radiusY * 2)
        .shape(shapes::annulus(0.44f))
        .fill(swirlFill())
        .rotate(-18);
  };

  // The letters. Chunky angular caps in the original, set MUCH narrower
  // than a system grotesque: measured "JAM" is 128 px wide at a 90 px cap
  // (0.47 per glyph), where Impact is 0.62. So size by cap height and
  // condense with scaleX — the same move the nav labels need, and the same
  // one a 1996 art director made by hand.
  //
  // ink() maps the ramp onto the TEXT METRICS (cap top to baseline),
  // so the teal -> yellow-green horizon crosses the capitals at any size
  // with no hand-positioned gradient. That is the whole reason it exists —
  // but NOT in box units. ink already installs a local matrix mapping
  // [0,1]^2 onto the metric band, and a box-unit gradient's own SkSL then
  // divides by uResolution (the NODE size) on top of it, so t collapses to
  // ~0 and every glyph comes out the first stop, flat. A pixel-unit
  // gradient with unit-square endpoints is the spelling that works.
  auto letters = [&](const char* words, float capPx, float targetW, float left,
                     float capTopY, float lean) {
    const float size = capPx / 0.72f;
    Text glyphs = text(words).font(type(size, C5(0x2FA9A0)));
    auto letterInk = material::linearGradient(
        {0, 0}, {0, 1},
        {{0.0f, C5(0x006BA5)},
         {0.22f, C5(0x007BAD)},
         {0.52f, C5(0x00A584)},
         {0.78f, C5(0x9CCE84)},
         {1.0f, C5(0xCEDE73)}},
        {.units = material::GradientUnits::Pixels});
    const float radius = 2.2f;
    const float offsets[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                           {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    material::Filter echoes;
    for (const auto& offset : offsets)
      echoes = echoes.then(material::Filter::shadow(
          C5(0x101831), {.offset = {offset[0] * radius, offset[1] * radius}}));
    // and the hard drop a 1996 logotype was cast down and right with.
    echoes = echoes.then(material::Filter::shadow(C5(0x000000), {.offset = {3, 3}}));
    glyphs.ink(letterInk.effects(echoes));
    const SkSize natural =
        intrinsicSize(text(words).font(type(size, C5(0xFFFF00))), fonts);
    const float condense = natural.width() > 1 ? targetW / natural.width() : 1.0f;
    return glyphs.left(left)
        .top(capTopY - 0.20f * size)
        .width(natural.width() + 4.0f)
        .scaleX(condense)
        .skewX(lean)
        .transformOrigin(pct(0), pct(50));
  };

  return artBox(width, height).children(
      {swirl().zIndex(0),
       // "SPACE": measured x 17..132, cap band y 47..87
       letters("SPACE", 40, 112, 16, 47, -7).zIndex(1),
       // "JAM": measured x 122..250, cap band y 25..115
       letters("JAM", 80, 132, 124, 28, -8).zIndex(2),
       // The swirl passing IN FRONT of the bottom of the J — the single cue
       // that makes this read as a 1996 logotype instead of a gradient
       // wordmark. Four concentric arcs, not one stroke: a PathFormat's fill
       // is evaluated in node-local space, so it can vary ALONG a stroked
       // band but not ACROSS its width, which is the direction the rainbow
       // runs.
       [&] {
         Element band = rect(centre.fX - radiusX, centre.fY - radiusY, radiusX * 2, radiusY * 2).rotate(-18);
         // Same four bands, at the same four radii the annulus ramp puts
         // them at, so the ring READS as one ring that goes behind at the
         // top and comes round in front at the bottom left.
         const uint32_t ink[4] = {0x7310C6, 0xF70000, 0xFFAD42, 0xFFEF00};
         for (int index = 0; index < 4; ++index) {
           const float extent = 0.960f - (float)index * 0.092f;
           band.children(
               {rect(radiusX * (1 - extent), radiusY * (1 - extent), radiusX * 2 * extent, radiusY * 2 * extent)
                    .shape(shapes::arc(96, 90))
                    .stroke(stroke(9.4f, Fill::color(C5(ink[index])),
                                   PathFormat::Align::Center))});
         }
         return band.zIndex(3);
       }()});
}

// --- fast.gif and break.gif, 50x11 each, bright red caps with a dark red
// --- drop. fast.gif is set flush right inside its box, the left two fifths
// --- empty; break.gif fills its own.
inline Element wordmark(weave::FontContext& fonts, const Label& label, bool flushRight) {
  const float width = 50, height = 11;
  const float measure = (flushRight ? 0.60f : 0.97f) * width;
  const weave::Type style = type(height * 1.16f, label.ink, 0.35f);
  const SkSize natural = intrinsicSize(text(label.text).font(style), fonts);
  Element run = text(label.text).font(style);
  run.ink(material::from(label.ink).effects(
      material::Filter::shadow(C5(0x8C0000), {.offset = {1, 1}})));
  run.left(flushRight ? width - measure : 0).top(-height * 0.22f).width(natural.width() + 4.0f);
  if (natural.width() > measure)
    run.scaleX(measure / natural.width()).transformOrigin(pct(0), pct(50));
  return artBox(width, height).children({std::move(run)});
}

}  // namespace spacejam
