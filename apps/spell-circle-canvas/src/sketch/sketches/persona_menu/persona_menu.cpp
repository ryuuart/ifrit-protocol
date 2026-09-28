/** @file
 * persona menu — a console pause menu rebuilt from a public
 * recreation's own scene file: every sticker offset, rotation, colour and
 * z-order converted rather than eyeballed, overlaps included.
 */

// The Persona 3 Reload pause menu, rebuilt.
//
// The numbers here are not invented. Every sticker offset, rotation, colour
// and z-order is read out of Ultipuk/persona_3_reload_pause_menu's
// main_menu_pause_ui.tscn — a public Godot recreation of the menu — and
// converted; see the kRows comment for the conversion. Rows overlap in the
// original, and they overlap here.
//
//  - sticker SCATTER: nine rows, laid out from that table.
//  - the date stamp (day numeral + weekday/block + location under a
//    fading rule) and the party rail (four skewed parallelogram cards
//    with HP/SP bars, entering on a 60ms stagger from the right).
//  - selection: BLACK text on a WHITE sliver-triangle wedge (rotate
//    +8 deg) over a PINK #FD77D9 back-wedge, plus the RED misprint echo
//    of the label offset (3,-6) clipped inside the wedge. Unselected
//    labels cycle the three cyans #16CFFB/#7DE6FD/#77FEFC. Idle
//    heartbeat: wedge 1 -> 1.05 (100ms) -> 1 (50ms) every 600ms.
//  - backdrop: sea-of-souls layer order -- deep blue ground -> 5-stop
//    posterized bands with HARD stops at the LUT positions
//    0/.31/.48/.77/.81 -> material::pattern::noise organic variation -> #007FD2
//    tint veil -> two SkSL caustic layers (alpha = step(cut,|p1-p2|)),
//    TIME QUANTIZED at 6 Hz host-side (floor(t*6)/6) under a sigma-1.4
//    blur -> dark bottom + cyan top gradients.
//  - chrome: giant rotated index numeral (rotate 90 deg, #787878,
//    condense 0.88) behind the menu; right-anchored tooltip title over
//    a "COMMAND ----" rule; button prompt circles.
//  - entrance: items fade + drop from -30px over 0.4s (the recreation's
//    own 0.4s tween from Vector2.UP * 30), 33ms stagger
//    BOTTOM-UP (children declared bottom-first; zIndex owns paint
//    order); the two-triangle cursor spawns at +0.4s and flies home
//    along (1,-1) under a spring that rings +40 -> -20 -> +10 -> 0.
//
// The layout was authored on a 960-wide canvas and this one is 900 wide, so
// x positions are compressed by 0.9375 while type sizes, wedge geometry and
// every y position are left alone. Keep that split if you move anything:
// scaling the type or the wedges with the width is what breaks the look.

// TAGS: Interfaces/Game

#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilweave/style/Face.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Spring.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace field = sigil::material::field;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::Paint;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace persona_menu {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;

// The menu's palette, taken verbatim from the recreation.
constexpr material::Color kGround{0.0039f, 0.3725f, 0.8000f, 1};      // #015FCC
constexpr material::Color kGroundDark{0.0118f, 0.1216f, 0.3922f, 1};  // #031F64
constexpr material::Color kCyanA{0.0863f, 0.8118f, 0.9843f, 1};       // #16CFFB
constexpr material::Color kCyanB{0.4902f, 0.9020f, 0.9922f, 1};       // #7DE6FD
constexpr material::Color kCyanC{0.4667f, 0.9961f, 0.9882f, 1};       // #77FEFC
constexpr material::Color kPink{0.9922f, 0.4667f, 0.8510f, 1};        // #FD77D9
constexpr material::Color kRedC{1, 0, 0, 1};                          // #F00
constexpr material::Color kLut0{0.0235f, 0.0392f, 0.1686f, 1};        // #060A2B
constexpr material::Color kLut1{0.0471f, 0.0706f, 0.2980f, 1};        // #0C124C
constexpr material::Color kLut2{0.1059f, 0.2196f, 0.9412f, 1};        // #1B38F0
constexpr material::Color kLut3{0.2000f, 0.3176f, 1.0000f, 1};        // #3351FF
constexpr material::Color kLut4{0.9882f, 0.9961f, 0.9961f, 1};        // #FCFEFE
constexpr material::Color kCausLight{0.5882f, 0.8902f, 0.9412f,
                                     0.47f};  // #96E3F0
constexpr material::Color kCausBub{0.3294f, 0.9294f, 0.9176f,
                                   0.255f};                  // #54EDEA
constexpr material::Color kBotDark{0, 0.0627f, 0.4275f, 1};  // #00106D
// The top framing gradient, at a third of the strength the recreation
// gives it: over the LUT's own near-black first band a bright cyan at
// four tenths IS the top third of the screen, and the bands under it
// stop being readable as bands.
constexpr material::Color kTopCyan{0, 0.9882f, 0.9490f, 0.14f};  // #00FCF2
// THE VEIL IS A TINT, NOT A WASH. At the strength this carried, a
// bright cyan over the LUT's near-black top band lifted the whole
// sea to a mid teal, and nothing on the screen was dark enough for
// anything to pop off. P3R's sea of souls runs navy to black with
// the bands reading as hard steps.
constexpr material::Color kTintVeil{0, 0.4980f, 0.8235f, 0.12f};  // #007FD2
// THE INDEX NUMERAL IS BEHIND THE MENU, and behind means screened:
// P3R tints the sea with an oversized digit pair rather than laying
// an opaque slab over it. Painted at #787878 it competed with the
// selection and read as a graphical fault crossing the party rail.
constexpr material::Color kNumeral{0.235f, 0.290f, 0.470f, 1};
constexpr material::Color kRing{0.3647f, 0.4157f, 0.5333f, 1};  // #5D6A88
constexpr material::Color kPaper{1, 1, 1, 1};
constexpr material::Color kInk{0, 0, 0, 1};

constexpr material::Color kCyans[3] = {kCyanA, kCyanB, kCyanC};

// The sticker scatter is NOT invented any more: every row's offset,
// rotation and colour comes from Ultipuk/persona_3_reload_pause_menu
// (a Godot recreation of the P3R pause menu), file
// data/ui/pause_ui/main_menu/main_menu_pause_ui.tscn. There the nine
// Labels are anchored to the screen CENTRE with pixel offsets, a
// per-label `rotation` in radians, scale (0.82, 1) and font_size 84.
//
// Converted here at s = 0.58 (1080 -> 640 stage) with the recreation's
// screen centre landing at menu-container-local (190, 250):
//     dx  = 100 + offset_left * s      (the scene's kBaseX is 90)
//     y   = 250 + offset_top  * s
//     rot = rotation * 180 / pi
// so the ladder below is the real one, digit for digit — then the y
// column is stretched 1.18x about the group centre. That last step is
// ours and it is a compromise: the recreation sets FOT-Rodin at 84px in
// a 104px box, and the face macOS ships in its place puts more ink in
// less box, so nine rows at the literal pitch buried each other. z_index
// -1 on ITEM and STATS is the recreation's too.
struct Row {
  const char* label;
  float dx, y, rot;
  int z;
  material::Color color;
};
constexpr Row kRows[] = {
    {"SKILL", 1.9f, 106.0f, -24.60f, 3, {0.4157f, 0.9020f, 0.9843f, 1}},
    {"ITEM", 29.3f, 144.7f, -9.19f, 1, {0.0431f, 0.7843f, 1.0000f, 1}},
    {"EQUIP", 2.6f, 181.6f, -17.93f, 4, {0.4157f, 0.9020f, 0.9843f, 1}},
    {"PERSONA", -27.9f, 221.3f, -17.44f, 9, {0.4078f, 1.0000f, 0.9882f, 1}},
    {"STATS", 22.3f, 266.2f, 1.79f, 1, {0.0471f, 0.7961f, 1.0000f, 1}},
    {"QUEST", -12.2f, 309.3f, -12.56f, 5, {0.4078f, 0.9098f, 0.9882f, 1}},
    {"SOCIAL LINK", -0.6f, 340.9f, -7.51f, 2, {0.4078f, 0.9922f, 0.9765f, 1}},
    {"CALENDAR", -25.9f, 387.0f, -1.51f, 6, {0.4078f, 0.9098f, 0.9882f, 1}},
    {"SYSTEM", 15.9f, 439.6f, 12.48f, 3, {0.0431f, 0.7961f, 0.9843f, 1}},
};
constexpr int kRowCount = (int)(sizeof(kRows) / sizeof(kRows[0]));
constexpr int kSelected = 3;  // PERSONA -- black on the white wedge
constexpr float kBaseX = 90;  // menu-container-local scatter origin
constexpr float kMenuX = 60, kMenuY = 70;  // menu container origin

/** The selection wedge: a sliver that tapers to a near-point at the right
 *  (long banner, blunt tip) -- the white slab the selected label sits on. */
inline Shape sliverWedge() {
  return keyedShape(std::string_view("persona-sliver"), [](glm::vec2 s) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(s.x, s.y * 0.20f);
    b.lineTo(s.x * 0.96f, s.y * 0.80f);
    b.lineTo(0, s.y);
    b.close();
    return sigil::geometry::path::fromSk(b.detach());
  });
}

/** Heavy condensed ITALIC, standing in for FOT-Rodin, which the original
 *  sets at 84px, 0.82 condensed and italic. Avenir Next Condensed Heavy
 *  Italic is the closest face macOS ships. */
inline sigil::weave::Face menuFace(bool italic = true) {
  const auto slant =
      italic ? sigil::weave::FaceSlant::Italic : sigil::weave::FaceSlant::Upright;
  return sigil::weave::ports::face(
      {"Avenir Next Condensed", "Helvetica Neue"},
      sigil::weave::FaceStyle{.weight = 900, .width = 5, .slant = slant});
}

/** Menu voice: heavy condensed italic with negative tracking. A partial: the colour is the reference's own ARGB
 *  word, so it takes the 8-bit ladder. */
inline sigil::weave::Type menuType(float size, material::Color fill,
                                   bool italic = true) {
  sigil::weave::Type t{.face = menuFace(italic),
                       .size = size,
                       .color = fill,
                       // The original tracks around -0.14em on Rodin; Avenir
                       // Condensed is already tighter, so it needs less
                       // taken out.
                       .track = sigil::weave::em(-0.08f),
                       // Condense the last of the way to Rodin's proportion
                       // (0.82 against its regular width); Avenir Condensed
                       // already carries most of that.
                       .condense = 0.94f,
                       .color8 = true};
  return t;
}

/** The small caps-and-figures voice, in the default family. */
inline sigil::weave::Type smallType(float size, material::Color c,
                                    float track = 1) {
  return {.size = size, .color = c, .track = track, .color8 = true};
}

}  // namespace persona_menu

struct PersonaMenu {
  // Live idle motion: 6Hz-quantized water clock, the wedge heartbeat, the
  // cursor's damped diagonal overshoot.
  motion::Animatable<float> qTime = motion::animatable(0.0f);
  motion::Animatable<float> wedgePulse = motion::animatable(1.0f);
  motion::Animatable<float> curDx = motion::animatable(40.0f), curDy = motion::animatable(-40.0f);
  // The cursor's landing carries its own velocity, so it is a spring and
  // not a curve: the offset is what rings down, and the two triangles
  // read it on both axes.
  motion::Spring cursorFlight{40.0f, 0.0f};
  /** The caustic shader, `caustic.sksl` beside this file, read once per
   *  declaration and held HERE for the sketch's life. */
  material::Material causticFx = material::Color{0, 0, 0, 0};

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = material::Color{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Engine& ticker = ctx.engine;
    struct CausticParameters {
      float uTime = 0;
      material::Color uLight = persona_menu::kCausLight;
      material::Color uDark = persona_menu::kCausBub;
    };
    causticFx = material::shader(ctx.assets.hub(), ctx.local("caustic.sksl"), CausticParameters{});
    qTime = 0;
    wedgePulse = 1;
    curDx = 40;
    curDy = -40;
    cursorFlight = {40.0f, 0.0f};

    ticker.timer([this, &ticker](sigil::motion::Duration stepDuration) {
      const double dt = stepDuration.count();
      const double t = ticker.elapsed().count();
      // The caustics step time at 6 Hz rather than running smoothly. This
      // is not an optimization — the stepping IS the texture, and a
      // continuous version does not look like the original.
      qTime = (float)motion::quantizeTime(t, 6.0);
      // Idle heartbeat: wedge 1 -> 1.05 (100ms) -> 1 (50ms) every 600ms.
      const double ph = std::fmod(t, 0.6);
      float s = 1.0f;
      if (ph < 0.1)
        s = 1.0f + 0.05f * motion::ease::outQuad((float)(ph / 0.1));
      else if (ph < 0.15)
        s = 1.05f - 0.05f * (float)((ph - 0.1) / 0.05);
      wedgePulse = s;
      // Cursor: it sits 40px out along (1,-1) until it spawns at +0.4s,
      // then flies home under a spring. The recreation's landing rings
      // through -20 and +10 before it settles, which is a period of
      // 0.391s at a damping of 0.215 — the successive overshoots halve.
      const double tau = t - 0.4;
      if (tau > 0)
        cursorFlight =
            cursorFlight.step(0.0f, motion::Duration(std::min(dt, tau)),
                              {.period = motion::Duration(0.391f), .damping = 0.215f});
      curDx = cursorFlight.value;
      curDy = -cursorFlight.value;
    });

    composer.render(describe());
  }

  material::Material dualCaustic() {
    namespace nn = persona_menu;
    material::Material m = causticFx;
    m.set("uLight", nn::kCausLight)
        .set("uDark", nn::kCausBub)
        .bind("uTime", qTime);
    return m;
  }

  /** The sea-of-souls backdrop, approximated but built in the original's
   *  layer order — the order is what produces the colour. */
  Element backdrop() {
    namespace nn = persona_menu;
    // 5-stop posterized band structure: HARD stops at the LUT positions.
    material::Material bands =
        sigil::material::linearGradient({0, 0}, {0, nn::kH},
                              {{0.000f, nn::kLut0},
                               {0.309f, nn::kLut0},
                               {0.309f, nn::kLut1},
                               {0.480f, nn::kLut1},
                               {0.480f, nn::kLut2},
                               {0.768f, nn::kLut2},
                               {0.768f, nn::kLut3},
                               {0.813f, nn::kLut3},
                               {0.813f, nn::kLut4},
                               {1.000f, nn::kLut4}},
                              {.units = material::GradientUnits::Pixels});

    // Three Z-planes so steady-state recomposition is BLITS, not
    // re-raster: everything below the sea is one static texture, the sea
    // re-bakes at its own 6 Hz, the framing gradients above are another
    // static texture. A live ancestor recomposites per frame on raster —
    // each plane must therefore be one cheap draw.
    return box()
        .inset(0)
        .children(
            {box()
                 .inset(0)
                 .cache(Cache::Texture)  // static under-plane: ground +
                                         // bands + noise + veil, one blit
                 .fill(sigil::material::linearGradient(
                     {0, 0}, {0, nn::kH},
                     {{0.0f, nn::kGroundDark}, {1.0f, nn::kGround}},
                     {.units = material::GradientUnits::Pixels}))
                 .children({box().inset(0).fill(bands).opacity(0.97f),
                            box()
                                .inset(0)
                                .fill(field::noise(0.006f, 4))
                                .opacity(0.20f)
                                .blendMode(material::BlendMode::SoftLight),
                            box().inset(0).fill(nn::kTintVeil)})})
        // The sea: one dual-layer 6Hz shader, its own texture plane --
        // baked at HALF raster scale and linear-upscaled at the blit.
        // The bands are watercolor-soft already, so the reduced bake
        // reads identically while each 6 Hz re-bake evaluates a quarter
        // of the pixels.
        .children({box()
                       .inset(0)
                       .cache(Cache::Texture)
                       .cacheScale(0.5f)
                       .fill(dualCaustic())})
        // static over-plane: the framing gradients, one blit
        .children(
            {box()
                 .inset(0)
                 .cache(Cache::Texture)
                 .children(
                     {box().inset(0).fill(sigil::material::linearGradient(
                          {0, nn::kH * 0.60f}, {0, nn::kH},
                          {{0.0f,
                            {nn::kBotDark.r, nn::kBotDark.g, nn::kBotDark.b,
                             0}},
                           {1.0f,
                            {nn::kBotDark.r, nn::kBotDark.g, nn::kBotDark.b,
                             0.88f}}},
                          {.units = material::GradientUnits::Pixels})),
                      box().inset(0).fill(sigil::material::linearGradient(
                          {0, 0}, {0, nn::kH * 0.42f},
                          {{0.0f, nn::kTopCyan},
                           {1.0f,
                            {nn::kTopCyan.r, nn::kTopCyan.g, nn::kTopCyan.b,
                             0}}},
                          {.units = material::GradientUnits::Pixels}))})});
  }

  /** Unselected sticker: one of the three cyans, soft black under-glow +
   *  #5D6A88 ring standing in for the original's text shadows, its OWN
   *  rotation, jitter and z from the
   *  ladder, entering with the fade + -30px drop. */
  Element plainRow(int i) {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;
    const nn::Row& r = nn::kRows[i];
    // The sigma-3.5 glow re-blurs on every picture replay, and the wedge
    // heartbeat keeps this whole menu live -- so bake each sticker to a
    // texture once it settles. 14px of padding keeps raster room for the
    // glow tail; the pin shifts up-left to compensate. Entrance transforms
    // and the row rotation apply outside the bake.
    return box()
        .key(r.label)
        .left(nn::kBaseX + r.dx - 14)
        .top(r.y - 14)
        .padding(14)
        .rotate(r.rot)
        .zIndex(r.z)
        .translateY(
            motion::animate({.from = -30.0f, .to = 0.0f, .duration = 400ms, .delay = motion::stagger(33ms), .ease = motion::ease::outQuint}))
        .opacity(
            motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms, .delay = motion::stagger(33ms), .ease = motion::ease::outQuad}))
        .cache(Cache::Texture)
        .children({text(r.label)
                       .font(nn::menuType(41, r.color)).ink(material::from(r.color).effects(
                            material::Filter::stroke(nn::kRing, {.width = 0.9f})))
                       .filter(sigil::material::Filter::glow({0, 0, 0, 0.5f}, 3.5f))});
  }

  /** The selected sticker: black label at 1.5x on a
   *  WHITE sliver wedge (+8 deg, heartbeat-scaled) over a PINK back-wedge,
   *  the RED misprint echo offset (3,-6) clipped INSIDE the wedge
   *  (counter-rotated so the echo tracks the label, not the wedge
   *  frame). */
  Element selectedRow() {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;
    const nn::Row& r = nn::kRows[nn::kSelected];
    // The wedge is cut to the SELECTED label, not to the widest one:
    // at nine rows a 330px slab buried its neighbours.
    const float lx = 20, ly = -2;  // label, row-local
    const float wW = 250, wH = 68;

    Element row = box()
                      .key(r.label)
                      .left(nn::kBaseX + r.dx)
                      .top(r.y - 12)
                      .width(264)
                      .height(78)
                      .rotate(r.rot)
                      .zIndex(r.z)
                      .translateY(motion::animate({.from = -30.0f, .to = 0.0f, .duration = 400ms, .delay = motion::stagger(33ms), .ease = motion::ease::outQuint}))
                      .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms, .delay = motion::stagger(33ms), .ease = motion::ease::outQuad}));
    // pink back-wedge, misregistered under the white one
    row.children(
        {kit::at(10, 3, wW, wH)
             .shape(nn::sliverWedge())
             .rotate(8)
             .fill(nn::kPink),
         // white wedge -- clips the red echo; idle heartbeat on scale.
         // The echo's top carries an extra +5px. The wedge rotates +8 deg about
         // its OWN centre, which walks the echo up by about that much, so the
         // offset has to be pre-compensated for the misprint to land at its
         // intended (3,-6).
         kit::at(0, -6, wW, wH)
             .shape(nn::sliverWedge())
             .rotate(8)
             .overflow(Overflow::Clip)
             .fill(nn::kPaper)
             .scale(wedgePulse)
             .children({text(r.label)
                            .font(nn::menuType(50, nn::kRedC))
                            .left(lx + 3)
                            .top(3)
                            .rotate(-8)}),
         // the black label (1.5x the unselected size), no glow -- ink on paper
         text(r.label).font(nn::menuType(50, nn::kInk)).left(lx).top(ly)});
    return row;
  }

  /** Two-triangle cursor: red under white, offset (1,5) +2 deg, root
   *  -16 deg, additive red; spawns at +0.4s and rings home on the
   *  spring the ticker steps. */
  Element cursor() {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;
    const nn::Row& r = nn::kRows[nn::kSelected];
    // canvas coords: the menu container origin folded into the pins
    return box()
        .key("cursor")
        .left(nn::kMenuX + nn::kBaseX + r.dx - 46)
        .top(nn::kMenuY + r.y + 12)
        .width(36)
        .height(36)
        .zIndex(7)
        .rotate(-16)
        .translateX(curDx)
        .translateY(curDy)
        .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 60ms, .delay = 400ms, .ease = motion::ease::outQuad}))
        // The original draws this additively. At this size over the navy
        // sea, kPlus washes the red rim out completely, so it stays a plain
        // red fill.
        .children({box()
                       .inset(0)
                       .shape(shapes::polygon(3, 92))
                       .fill(nn::kRedC)
                       .translateX(1)
                       .translateY(5),
                   box()
                       .inset(0)
                       .shape(shapes::polygon(3, 90))
                       .fill(nn::kPaper)});
  }

  Element promptCircle(const char* glyph) {
    namespace nn = persona_menu;
    // THE RING AND THE LETTER IN IT ARE ONE MARK, so the white is named
    // once: a stroke that names no colour is painted in the ink, and so is
    // the glyph inside it.
    return kit::centred()
        .width(32)
        .height(32)
        .shape(shapes::squircle(2.0f))
        .fill(material::Color{nn::kGroundDark.r, nn::kGroundDark.g,
                              nn::kGroundDark.b, 0.8f})
        .ink(nn::kPaper)
        .stroke(stroke(3))

        .children({text(glyph).font({.size = 14, .color8 = true})});
  }

  /** The date stamp the pause menu wears in its top-left corner: the day
   *  as a big italic numeral pair, the weekday and the block of day beside
   *  it, the location under a hairline. */
  Element dateBlock() {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;
    return box()
        .key("date")
        .left(44)
        .top(34)
        .column()
        .zIndex(8)
        .translateX(motion::animate({.from = -30.0f, .to = 0.0f, .duration = 420ms, .ease = motion::ease::outQuint}))
        .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 340ms}))
        .children(
            {box()
                 .row()
                 .alignItems(Align::End)
                 .children({text("07/22")
                                .font(nn::menuType(38, nn::kPaper)).ink(material::from(nn::kPaper).effects(
                            material::Filter::stroke(nn::kRing, {.width = 1.0f})))
                                .filter(sigil::material::Filter::glow({0, 0, 0, 0.45f}, 3)),
                            box()
                                .column()
                                .margin(0, 0, 5, 11)
                                .children({text("SUNDAY").font(nn::smallType(
                                               11, nn::kCyanC, 2.6f)),
                                           text("EVENING")
                                               .font(nn::smallType(
                                                   11, nn::kCyanB, 2.6f))
                                               .margin(3, 0, 0, 0)})}),
             box()
                 .width(168)
                 .height(2)
                 .margin(7, 0, 5, 0)
                 .fill(sigil::material::linearGradient(
                     {0, 0}, {168, 0},
                     {{0.0f, {1, 1, 1, 0.85f}}, {1.0f, {1, 1, 1, 0.0f}}},
                     {.units = material::GradientUnits::Pixels})),
             text("IWATODAI DORM").font(nn::smallType(11, nn::kPaper, 2.2f))});
  }

  /** The party rail: four slanted cards with HP and SP. P3R skews every
   *  card, so these are parallelograms, not rectangles, and each one
   *  slides in from the right on the list's own stagger. */
  Element partyPanel() {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;
    struct Member {
      const char* name;
      int level, hp, hpMax, sp, spMax;
    };
    static const Member kParty[] = {
        {"MAKOTO", 42, 312, 380, 88, 150},
        {"YUKARI", 41, 268, 296, 121, 164},
        {"JUNPEI", 41, 355, 355, 42, 96},
        {"MITSURU", 43, 241, 302, 149, 188},
    };
    constexpr material::Color kHp{0.549f, 0.910f, 0.627f, 1};  // #8CE8A0
    constexpr material::Color kSp{0.416f, 0.722f, 1.000f, 1};  // #6ABBFF

    // A ROW, NOT `sketch::kit::meter`. That component sets the label and
    // the reading OVER the bar and the rail under them, which is the
    // reading a specimen sheet wants; P3R runs the three across one line
    // with the label ranged left of the rail and the numbers right of it,
    // and the two are different pictures rather than one with a field set.
    auto bar = [&](const char* label, int value, int max,
                   material::Color color) {
      const float frac = max > 0 ? (float)value / (float)max : 0.0f;
      const std::string numbers = kit::formatted("%d/%d", value, max);
      return box()
          .row()
          .alignItems(Align::Center)
          .gap(6)
          // The gauge's colour IS the label's: HP is green wherever it is
          // written, so the row names it and the two letters take it.
          .ink(color)
          .children(
              {text(label)
                   .font({.size = 9, .track = 1.4f, .color8 = true})
                   .width(16),
               box()
                   .width(84)
                   .height(6)
                   .flexGrow(0)
                   .fill({0, 0.05f, 0.18f, 0.55f})
                   .children(
                       {kit::at(0, 0, 84 * frac, 6.0f)
                            .fill(sigil::material::linearGradient(
                                {0, 0}, {0, 6},
                                {{0.0f,
                                  {std::min(1.0f, color.r * 1.4f),
                                   std::min(1.0f, color.g * 1.4f),
                                   std::min(1.0f, color.b * 1.4f), 1}},
                                 {1.0f, color}},
                                {.units = material::GradientUnits::Pixels}))}),
               text(numbers).font(nn::smallType(9, nn::kPaper, 0.6f))});
    };

    Element rail = box()
                       .key("party")
                       .right(41)
                       .bottom(74)
                       .column()
                       .gap(7)
                       .zIndex(8);
    rail.children({each(kParty, [&](const Member& m) {
      return box()
          .width(246)
          .height(52)
          .rotate(-4)
          .translateX(motion::animate({.from = 46.0f, .to = 0.0f, .duration = 440ms, .delay = motion::stagger(60ms), .ease = motion::ease::outQuint}))
          .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 360ms, .delay = motion::stagger(60ms)}))
          .shape(shapes::parallelogram(9))
          .fill(
              sigil::material::linearGradient({0, 0}, {246, 0},
                                    {{0.0f, {0.02f, 0.16f, 0.42f, 0.78f}},
                                     {1.0f, {0.02f, 0.30f, 0.62f, 0.55f}}},
                                    {.units = material::GradientUnits::Pixels}))
          .stroke(stroke(1.4f, Fill::color({1, 1, 1, 0.55f})))
          .column()
          .padding(7, 17)
          .gap(2)
          .children(
              {box()
                   .row()
                   .alignItems(Align::End)
                   .children({text(m.name)
                                  .font(nn::menuType(17, nn::kPaper)).ink(material::from(nn::kPaper).effects(
                            material::Filter::stroke(nn::kRing, {.width = 0.5f})))
                                  .flexGrow(1),
                              text(kit::formatted("LV %d", m.level))
                                  .font(nn::smallType(10, nn::kCyanB, 1.6f))}),
               bar("HP", m.hp, m.hpMax, kHp), bar("SP", m.sp, m.spMax, kSp)});
    })});
    return rail;
  }

  Element describe() {
    namespace nn = persona_menu;
    using namespace std::chrono_literals;

    return stack()
        .fill(nn::kGroundDark)
        .children({backdrop()})
        // ---- giant rotated index numeral, behind the menu ----
        .children({text("04")
                       .font([] {
                         auto s = nn::menuType(220, nn::kNumeral, false);
                         // The original tracks this at -0.2em on FOT-Rodin.
                         // Avenir's digit shapes merge sooner than Rodin's, so
                         // 0.88 condensation with -0.05em is the deepest
                         // overlap that still reads as two digits.
                         s.condense = 0.88f;
                         s.track = sigil::weave::em(-0.05f);
                         return s;
                       }())
                       .centerAt({450, 306})
                       .rotate(90)
                       .zIndex(1)
                       .blendMode(material::BlendMode::Screen)
                       .opacity(motion::animate({.from = 0.0f, .to = 0.85f, .duration = 500ms}))
                       // 220px digits render as glyph PATHS (over the atlas
                       // cutoff); bake them once, the rotation rides outside
                       .cache(Cache::Texture)})
        // ---- the sticker scatter; stagger 33ms BOTTOM-UP ----
        .children(
            {box()
                 .key("menu")
                 .left(nn::kMenuX)
                 .top(nn::kMenuY)
                 .width(450)
                 .height(530)
                 .zIndex(2)
                 // Declared BOTTOM-UP, and the declaration order is what
                 // the stickers OVERLAP in — a lower row's skewed plate
                 // laps over the one above it. The stagger runs in that
                 // same order, so the list enters from SYSTEM upward the
                 // way the game does, and this file needs no
                 // `Spread::From::End` to reverse the cascade against a
                 // paint order it does not have.
                 // The bottom row enters first, and the selected one falls
                 // out of the ladder rather than being placed by hand.
                 .children({each(nn::kRowCount,
                                 [this](int n) {
                                   const int i = nn::kRowCount - 1 - n;
                                   return i == nn::kSelected ? selectedRow()
                                                             : plainRow(i);
                                 })}),
             cursor(), dateBlock(), partyPanel()})
        // ---- right-anchored tooltip title over the COMMAND rule ----
        .children(
            {box()
                 .key("tooltip")
                 .top(40 - 12)
                 .right(43 - 12)
                 .zIndex(8)
                 .alignItems(Align::End)
                 // texture-baked (the sigma-3 glow otherwise re-blurs
                 // on every root replay); 12px padding keeps raster
                 // room for the glow tail, pins shifted to compensate
                 .padding(12)
                 .cache(Cache::Texture)
                 .translateX(motion::animate({.from = 36.0f, .to = 0.0f, .duration = 400ms, .ease = motion::ease::outQuint}))
                 .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 300ms}))
                 .children(
                     {text("PERSONA")
                          .font(nn::menuType(30, nn::kPaper)).ink(material::from(nn::kPaper).effects(
                            material::Filter::stroke(nn::kRing, {.width = 1.0f})))
                          .filter(sigil::material::Filter::glow({0, 0, 0, 0.5f}, 3)),
                      box()
                          .row()
                          .alignItems(Align::Center)
                          .margin(6, 0, 0, 0)
                          .children({text("COMMAND").font(
                                         nn::smallType(12, nn::kCyanB, 2)),
                                     box()
                                         .width(120)
                                         .height(2)
                                         .fill(material::Color{1, 1, 1, 0.8f})
                                         .margin(0, 0, 0, 8)})})})
        // ---- button prompts, bottom-right ----
        .children({box()
                       .key("prompts")
                       .right(41)
                       .bottom(28)
                       .row()
                       .alignItems(Align::Center)
                       .zIndex(8)
                       .opacity(motion::animate({.from = 0.0f, .to = 1.0f, .duration = 400ms, .delay = 250ms, .ease = motion::ease::outQuad}))
                       .children({promptCircle("O"),
                                  text("CONFIRM")
                                      .font(nn::smallType(11, nn::kCyanB, 1.5f))
                                      .margin(0, 22, 0, 8),
                                  promptCircle("X"),
                                  text("BACK")
                                      .font(nn::smallType(11, nn::kCyanB, 1.5f))
                                      .margin(0, 0, 0, 8)})});
  }
};

}  // namespace

SIGIL_SKETCH_AS(PersonaMenu, "persona menu", "Catalog · Game UI",
                "P3R menu grammar")
