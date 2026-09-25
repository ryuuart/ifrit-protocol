// Serial Experiments Lain's Copland OS, as the NAVI shows it in Layers 04
// and 07: no opaque window anywhere. Every stratum ADDS light to a dark
// night plate — Japanese prose run full bleed, a lightened panel, the
// console window whose pedestal is the blurred Copland eye, a MIPS listing
// read through one fixed focal plane, a twisted hyperboloid in dotted
// hairlines, and the English titles blooming in and out over all of it.
// The tube is the last thing: one filter on the root that blooms what is
// lit and rasters the frame into scan lines.
//
// The listing's first sixteen lines are verbatim off the Layer 04 frame;
// the rest continue the same gcc -S output so the scroll has material, and
// are not evidence. The prose is transcribed where the plate is legible and
// filled in the same register where the tube ate it.

// TAGS: Interfaces/Film

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Crt.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
using material::skia::Effect;
using material::skia::Paint;

using namespace sigil::compose;

namespace {

// The source frames are 1016 x 720, so a capture diffs against them.
constexpr float kWidth = 1016, kHeight = 720;

// Every colour below is what its stratum ADDS to the ground, so nothing in
// the interface ever has to darken anything to read.
const material::Color kGround = hexColor(0x060719);
const material::Color kCity = hexColor(0x1A2A5C);
const material::Color kProse = hexColor(0x1B2138);
const material::Color kPanel = hexColor(0x101F27);
const material::Color kBodyMiddle = hexColor(0x475F86);
const material::Color kBodyEdge = hexColor(0x040A24);
const material::Color kEyeLine = hexColor(0x0C1426);
const material::Color kRail = hexColor(0x1D3242);
const material::Color kConsoleInk = hexColor(0x46C89A);
const material::Color kWire = hexColor(0x3A6257);
const material::Color kMinds = hexColor(0xA6B7BE);
const material::Color kAlright = hexColor(0xB3B6BF);
const material::Color kCover = hexColor(0x7A3416);
const material::Color kMagenta = hexColor(0x3A1B3C);
const material::Color kWordmark = hexColor(0x2B3A54);

// The console window. The chrome bars overhang the body; the body has no
// corner anywhere.
constexpr float kBodyLeft = 160, kBodyRight = 984;
constexpr float kBodyTop = 56, kBodyBottom = 668;

// The listing: fifteen lines on a 37.5 px pitch, the focal plane fixed at
// y 402 while the text scrolls through it one line every 220 ms.
constexpr int kConsoleLines = 15;
constexpr float kPitch = 37.5f;
constexpr float kListingTop = 90;
constexpr float kFocusPlane = 402;
constexpr int kScrollPhase = 27;

// The hyperboloid: two rim circles of radius 376 at +-166 on a vertical
// axis, the top twisted against the bottom by twice the ruling angle, seen
// orthographically so every circle is an ellipse of eccentricity 0.163. The
// waist is the rim radius times cos(angle), so one angle turns the figure.
constexpr SkPoint kAxis{472, 376};
constexpr float kRim = 376, kHalfHeight = 166, kSquash = 0.163f;
constexpr float kRuling = 62.8f;
// The second orbit shares the centre and not the plane.
constexpr SkPoint kOrbitCentre{465, 300};

sk_sp<SkTypeface> monoFace() {
  return weave::ports::face({"JetBrainsMono Nerd Font", "JetBrains Mono",
                             "Andale Mono", "Menlo"},
                            300);
}
sk_sp<SkTypeface> minchoFace() {
  return weave::ports::face(
      {"Hiragino Mincho ProN", "YuMincho", "Noto Serif JP"}, 400);
}
sk_sp<SkTypeface> serifFace(int weight = 400, bool italic = false) {
  return weave::ports::face(
      {"Times New Roman", "Times", "Georgia"}, weight,
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant);
}
sk_sp<SkTypeface> titleFace() {
  return weave::ports::face({"Helvetica Neue", "Helvetica", "Arial"}, 500);
}

/** The registers the frame sets its words in. `.light` is the one law of
 *  the interface: a stratum adds to what is under it. */
StyleSheet registers() {
  return StyleSheet{
      rule(".light").blendMode(SkBlendMode::kPlus),
      rule(".listing").font({.face = monoFace(), .size = 22}).ink(kConsoleInk),
      rule(".prose")
          .font({.face = minchoFace(), .size = 28, .track = 1.5f})
          .ink(kProse),
      rule(".title").font({.face = titleFace()}),
      rule(".minds").font({.face = serifFace()}).ink(kMinds),
  };
}

/** A straight hairline from @p from to @p to. */
Element segment(SkPoint from, SkPoint to, float thickness,
                material::Color colour) {
  const float length = std::hypot(to.fX - from.fX, to.fY - from.fY);
  const float angle =
      std::atan2(to.fY - from.fY, to.fX - from.fX) * 180 / std::numbers::pi_v<float>;
  return box()
      .width(length)
      .height(thickness)
      .centerAt({(from.fX + to.fX) / 2, (from.fY + to.fY) / 2})
      .rotate(angle)
      .fill(Fill::color(colour));
}

/** An ellipse of semi-axes @p across by @p down about @p centre, turned by
 *  @p tilt degrees, drawn from @p start through @p sweep in the frame's
 *  dotted hairline. */
Element orbit(SkPoint centre, float across, float down, float tilt,
              float start, float sweep, float width, material::Color colour) {
  PathFormat dotted = stroke(width, Fill::color(colour));
  dotted.dashIntervals = {1.6f, 4.4f};
  dotted.cap = sigil::geometry::path::Cap::Round;
  return box()
      .width(across * 2)
      .height(down * 2)
      .centerAt(centre)
      .rotate(tilt)
      .shape(shapes::arc(start, sweep))
      .fill(Fill::none())
      .stroke(dotted);
}

/** A point on a circle of the hyperboloid at height @p z, projected. */
SkPoint onCircle(float radius, float z, float angle) {
  return {kAxis.fX + radius * std::cos(angle),
          kAxis.fY - z + radius * kSquash * std::sin(angle)};
}

/** A chrome bar: a sheared slab lit from outside its silhouette, so it is
 *  bright at both edges and dark through the middle, like a capstan. */
Element chromeBar(float left, float top, float width, float height,
                  float skew, material::Color bright, material::Color dark) {
  auto mix = [&](float amount) {
    return material::mixLinear(dark, bright, amount);
  };
  return kit::at(left, top, width, height)
      .styleClass("light")
      .shape(shapes::parallelogram(skew))
      .fill(Paint::linearGradient({0, 0}, {0, 1},
                                  {{0.00f, mix(0.05f)},
                                   {0.13f, mix(1.00f)},
                                   {0.30f, mix(0.32f)},
                                   {0.62f, mix(0.28f)},
                                   {0.87f, mix(1.00f)},
                                   {1.00f, mix(0.02f)}}));
}

struct LainNavi {
  sketch::kit::Document content;
  double seconds = 0;
  long long frame = -1;

  /** The photographed city under everything, defocused until it has no
   *  edge left: a few lit slabs, the bright massing in the right third. */
  Element city() const {
    return box().inset(0).filter(Effect::blur(34)).children(
        {each(content["city"].items(), [](const sigil::data::Json& slab) {
          return kit::at(slab["x"].number(), slab["y"].number(),
                         slab["width"].number(), slab["height"].number())
              .fill(Fill::color(material::scale(
                  kCity, static_cast<float>(slab["light"].number()))));
        })});
  }

  /** The prose runs off all four edges and ignores the window over it.
   *  No two lines start at the same x: a vertical original set
   *  horizontally by a compositor who did not align it. */
  Element prose() const {
    return box().inset(0).styleClass("light").filter(Effect::blur(0.9f)).children(
        {each(content["prose"].items(),
              [](const sigil::data::Json& line, size_t index) {
                return text(line.text())
                    .styleClass("prose")
                    .left(-34 + 14 * std::sin(index * 1.7f))
                    .top(-52 + 48.5f * index);
              })});
  }

  /** The Copland eye, which on the plate is only a pedestal of light: the
   *  radial ramp dips at the iris gap and recovers at the iris ring, and
   *  the eyelids, four satellites and the stem are blurred almost into it. */
  Element body() const {
    const SkPoint eye{558, 334};
    const float radius = 92;
    PathFormat lid = stroke(16, Fill::color(kEyeLine));
    return box().inset(0).children({
        kit::at(kBodyLeft, kBodyTop, kBodyRight - kBodyLeft,
                kBodyBottom - kBodyTop)
            .styleClass("light")
            .backdropFilter(Effect::blur(1.2f))
            .fill(Paint::radialGradient(
                {0.483f, 0.456f}, 0.70f,
                {{0.00f, kBodyMiddle},
                 {0.10f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.73f)},
                 {0.17f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.80f)},
                 {0.30f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.72f)},
                 {0.40f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.50f)},
                 {0.53f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.33f)},
                 {0.77f, material::mixLinear(kBodyEdge, kBodyMiddle, 0.18f)},
                 {1.00f, material::scale(kBodyEdge, 0.55f)}})),
        box()
            .inset(0)
            .styleClass("light")
            .filter(Effect::blur(10))
            .children({
                kit::disc({eye.fX + radius * 0.62f, eye.fY}, radius * 0.5f)
                    .height(radius * 1.56f)
                    .centerAt({eye.fX + radius * 0.62f, eye.fY})
                    .shape(shapes::arc(270, 180))
                    .fill(Fill::none())
                    .stroke(lid),
                kit::disc({eye.fX - radius * 0.62f, eye.fY}, radius * 0.5f)
                    .height(radius * 1.56f)
                    .centerAt({eye.fX - radius * 0.62f, eye.fY})
                    .shape(shapes::arc(90, 180))
                    .fill(Fill::none())
                    .stroke(lid),
                each(4,
                     [&](size_t corner) {
                       const float across = corner % 2 ? 1 : -1;
                       const float down = corner / 2 ? 1 : -1;
                       return kit::dot({eye.fX + across * radius * 0.92f,
                                        eye.fY + down * radius * 0.92f},
                                       radius * 0.155f, Fill::color(kEyeLine));
                     }),
                segment({eye.fX, eye.fY + radius * 0.95f},
                        {eye.fX, eye.fY + radius * 1.52f}, 16, kEyeLine),
            }),
        // The side rails: single hairlines at the body's edges, dimmer
        // than the bars, which simply overhang them.
        segment({kBodyLeft, 88}, {kBodyLeft + 8, 650}, 2, kRail)
            .styleClass("light"),
        segment({kBodyRight, 88}, {kBodyRight - 6, 650}, 2, kRail)
            .styleClass("light"),
    });
  }

  /** Fifteen lines of the listing through one focal plane: the blur grows
   *  linearly with distance from y 402 and the ink never dims, which is
   *  what a lens does and a fade does not. */
  Element listing() const {
    const auto lines = content["listing"].items();
    const long long scroll = static_cast<long long>(seconds / 0.220);
    std::string passage;
    for (int line = 0; line < kConsoleLines && !lines.empty(); ++line) {
      const size_t source = (scroll + line + kScrollPhase) % lines.size();
      passage += std::string(lines[source].text()) + "\n";
    }
    const float blockHeight = kConsoleLines * kPitch;
    const float focus = (kFocusPlane - kListingTop) / blockHeight;
    // The red channel is the blur's sigma as a fraction of 3 px: a tenth
    // of it on the plane, most of it at the block's two ends.
    const Paint depth = Paint::linearGradient(
        {0, 0}, {0, 1},
        {{0, {0.8f, 0, 0, 1}}, {focus, {0.1f, 0, 0, 1}}, {1, {0.7f, 0, 0, 1}}});
    return kit::at(text(passage)
                       .styleClass("listing light")
                       .paragraph({.leading = weave::Leading::absolute(kPitch)})
                       .filter(Effect::blur(depth, 3)),
                   188, kListingTop, 800, blockHeight);
  }

  /** The hyperboloid, its ruling angle sweeping sixteen degrees either
   *  side of 62.8 over 24 s — past 90 it would turn inside out. */
  Element hyperboloid() const {
    const float turn = static_cast<float>(seconds / 24.0 * 2 * std::numbers::pi);
    const float ruling = kRuling + 16 * std::sin(turn);
    const float twist = ruling * std::numbers::pi_v<float> / 180;
    const float waist = kRim * std::cos(twist);
    const material::Color faint = material::scale(kWire, 0.44f);
    const material::Color rim = material::scale(kWire, 0.72f);
    return box().inset(0).styleClass("light").children({
        // Seven straight rulings, each stopping short of both rims.
        each(7,
             [&](size_t index) {
               const float angle = index * 2 * std::numbers::pi_v<float> / 7;
               const SkPoint low = onCircle(kRim, -kHalfHeight, angle - twist);
               const SkPoint high = onCircle(kRim, kHalfHeight, angle + twist);
               auto along = [&](float amount) {
                 return SkPoint{low.fX + (high.fX - low.fX) * amount,
                                low.fY + (high.fY - low.fY) * amount};
               };
               return segment(along(0.07f), along(0.93f), 1.5f, faint);
             }),
        // The rims leave frame and never close; the waist and the tilted
        // orbit are whole.
        orbit({kAxis.fX, kAxis.fY - kHalfHeight}, kRim, kRim * kSquash, 0,
              203, 175, 1.7f, rim),
        orbit({kAxis.fX, kAxis.fY + kHalfHeight}, kRim, kRim * kSquash, 0, 17,
              158, 1.7f, rim),
        orbit(kAxis, waist, waist * kSquash, 0, 0, 360, 2, kWire),
        orbit(kOrbitCentre, 373, 187,
              -17.4f + 5 * std::sin(turn * 0.5f + 1.1f), 0, 360, 2, kWire),
        // The axis is a real line, a few pixels off the waist's centre.
        segment({477, 62}, {477, 558}, 2.4f, rim),
        text(u8"make me feel alright?")
            .styleClass("title")
            .font({.size = 44})
            .ink(kAlright)
            .filter(Effect::glow(material::scale(kAlright, 0.4f), 3))
            .centerAt({455, 392}),
    });
  }

  /** The Layer 07 titles, each blooming in over 0.8 s, holding, and
   *  fading over 0.9 s, so two can overlap and add where they cross. The
   *  sequence loops at 13.6 s. */
  Element titles() const {
    const double clock = std::fmod(seconds, 13.6);
    return box().inset(0).styleClass("light").children(
        {each(content["phrases"].items(), [&](const sigil::data::Json& title) {
          const double since = clock - title["at"].number();
          const double hold = title["hold"].number();
          float level = since < 0.8 ? since / 0.8 : 1 - (since - hold) / 0.9;
          level = std::clamp(level, 0.0f, 1.0f);
          level = level * level * (3 - 2 * level);
          return text(title["words"].text())
              .styleClass("minds")
              .font({.size = static_cast<float>(title["size"].number())})
              .opacity(level)
              .filter(Effect::glow(material::scale(kMinds, 0.8f), 8))
              .centerAt({static_cast<float>(title["x"].number()),
                         static_cast<float>(title["y"].number())});
        })});
  }

  /** The magenta smears: flat bands streaked along x only. */
  Element streaks() const {
    return box()
        .inset(0)
        .styleClass("light")
        .filter(Effect::directionalBlur(36, 0))
        .children({each(content["streaks"].items(),
                        [](const sigil::data::Json& band) {
                          return kit::at(band["x"].number(), band["y"].number(),
                                         band["width"].number(), 15)
                              .fill(Fill::color(material::scale(
                                  kMagenta,
                                  static_cast<float>(band["strength"].number()))));
                        })});
  }

  /** The tube: what is lit blooms, then the beam rasters the whole frame
   *  at its own 4.42 px pitch. */
  static Effect tube() {
    return Effect::phosphorBloom(8, 0.45f, 0.40f, 0.5f)
        .then(Effect::recipe(material::field::crtBeam(
            {.uBounds = {0, 0, kWidth, kHeight},
             .uScanPitch = 4.42f,
             .uRaster = 0.45f,
             .uNoise = 0.02f})));
  }

  Element describe() const {
    return box()
        .inset(0)
        .fill(Fill::color(kGround))
        .applyStyleSheet(registers())
        .filter(tube())
        .children({
            city(),
            prose(),
            // The lightened panel behind the window's left half, soft to
            // nothing at its edge.
            kit::at(178, 88, 304, 304)
                .styleClass("light")
                .fill(Paint::radialGradient(
                    {0.48f, 0.46f}, 0.95f,
                    {{0.00f, kPanel},
                     {0.55f, material::scale(kPanel, 0.86f)},
                     {0.86f, material::scale(kPanel, 0.30f)},
                     {1.00f, material::scale(kPanel, 0)}})),
            body(),
            listing(),
            chromeBar(137, 62, 856, 30, -28.4f, hexColor(0x587962),
                      hexColor(0x234A3C)),
            chromeBar(140, 646, 855, 26, 24.7f, hexColor(0x6FA586),
                      hexColor(0x578C70)),
            // The Copland lockup up the left margin.
            box()
                .centerAt({88, 300})
                .rotate(-55)
                .column()
                .alignItems(Align::Center)
                .styleClass("light")
                .ink(kWordmark)
                .filter(Effect::blur(1.4f))
                .children({text(u8"Copland OS Enterprise")
                               .font({.face = serifFace(700, true),
                                      .size = 34,
                                      .track = 1.0f}),
                           text(u8"Produced By Tachibana Lab")
                               .font({.face = serifFace(700, true),
                                      .size = 16,
                                      .track = 0.8f})
                               .opacity(0.7f)}),
            hyperboloid(),
            // The only warm thing in the frame.
            text(u8"cover me")
                .styleClass("title light")
                .font({.size = 62})
                .ink(kCover)
                .filter(Effect::glow(material::scale(kCover, 0.5f), 6.5f))
                .centerAt({730, 182}),
            streaks(),
            titles(),
        });
  }

  void setup(sketch::SketchContext& context) {
    context.canvas(kWidth, kHeight);
    context.background(kGround);
    // At 2.5 s the listing's `.frame $fp,40,$31` sits on the focal plane
    // and `no double minds` is at full bloom.
    context.captureAt(2.5);
    content = sketch::kit::Document(context, "data/content.json");
    context.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& context) {
    // The titles fade at twelve steps a second; the listing and the
    // hyperboloid move within those steps.
    const long long step = static_cast<long long>(elapsed * 12);
    if (step == frame) return;
    frame = step;
    seconds = elapsed;
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(LainNavi, "Study · Film",
             "Serial Experiments Lain's Copland OS — no opaque window "
             "anywhere, and text through a fixed focal plane")
