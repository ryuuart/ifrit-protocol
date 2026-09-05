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
//    0/.31/.48/.77/.81 -> mpattern::noise organic variation -> #007FD2
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

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilmaterial/field/Field.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmotion/values/Spring.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <sigilweave/kit/PaintLayers.h>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
namespace mpattern = sigil::material::pattern;
namespace field = sigil::material::field;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::skia::Paint;
using sigil::compose::toU8;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace persona_menu {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;

// The menu's palette, taken verbatim from the recreation.
constexpr SkColor4f kGround{0.0039f, 0.3725f, 0.8000f, 1};         // #015FCC
constexpr SkColor4f kGroundDark{0.0118f, 0.1216f, 0.3922f, 1};     // #031F64
constexpr SkColor4f kCyanA{0.0863f, 0.8118f, 0.9843f, 1};          // #16CFFB
constexpr SkColor4f kCyanB{0.4902f, 0.9020f, 0.9922f, 1};          // #7DE6FD
constexpr SkColor4f kCyanC{0.4667f, 0.9961f, 0.9882f, 1};          // #77FEFC
constexpr SkColor4f kPink{0.9922f, 0.4667f, 0.8510f, 1};           // #FD77D9
constexpr SkColor4f kRedC{1, 0, 0, 1};                             // #F00
constexpr SkColor4f kLut0{0.0235f, 0.0392f, 0.1686f, 1};           // #060A2B
constexpr SkColor4f kLut1{0.0471f, 0.0706f, 0.2980f, 1};           // #0C124C
constexpr SkColor4f kLut2{0.1059f, 0.2196f, 0.9412f, 1};           // #1B38F0
constexpr SkColor4f kLut3{0.2000f, 0.3176f, 1.0000f, 1};           // #3351FF
constexpr SkColor4f kLut4{0.9882f, 0.9961f, 0.9961f, 1};           // #FCFEFE
constexpr SkColor4f kCausLight{0.5882f, 0.8902f, 0.9412f, 0.47f};  // #96E3F0
constexpr SkColor4f kCausBub{0.3294f, 0.9294f, 0.9176f, 0.255f};   // #54EDEA
constexpr SkColor4f kBotDark{0, 0.0627f, 0.4275f, 1};              // #00106D
// The top framing gradient, at a third of the strength the recreation
// gives it: over the LUT's own near-black first band a bright cyan at
// four tenths IS the top third of the screen, and the bands under it
// stop being readable as bands.
constexpr SkColor4f kTopCyan{0, 0.9882f, 0.9490f, 0.14f};  // #00FCF2
// THE VEIL IS A TINT, NOT A WASH. At the strength this carried, a
// bright cyan over the LUT's near-black top band lifted the whole
// sea to a mid teal, and nothing on the screen was dark enough for
// anything to pop off. P3R's sea of souls runs navy to black with
// the bands reading as hard steps.
constexpr SkColor4f kTintVeil{0, 0.4980f, 0.8235f, 0.12f};  // #007FD2
// THE INDEX NUMERAL IS BEHIND THE MENU, and behind means screened:
// P3R tints the sea with an oversized digit pair rather than laying
// an opaque slab over it. Painted at #787878 it competed with the
// selection and read as a graphical fault crossing the party rail.
constexpr SkColor4f kNumeral{0.235f, 0.290f, 0.470f, 1};
constexpr SkColor4f kRing{0.3647f, 0.4157f, 0.5333f, 1};  // #5D6A88
constexpr SkColor4f kPaper{1, 1, 1, 1};
constexpr SkColor4f kInk{0, 0, 0, 1};

constexpr SkColor4f kCyans[3] = {kCyanA, kCyanB, kCyanC};

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
  SkColor4f color;
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
inline std::function<SkPath(SkSize)> sliverWedge() {
  return [](SkSize s) {
    SkPathBuilder b;
    b.moveTo(0, 0);
    b.lineTo(s.width(), s.height() * 0.20f);
    b.lineTo(s.width() * 0.96f, s.height() * 0.80f);
    b.lineTo(0, s.height());
    b.close();
    return b.detach();
  };
}

/** Heavy condensed ITALIC, standing in for FOT-Rodin, which the original
 *  sets at 84px, 0.82 condensed and italic. Avenir Next Condensed Heavy
 *  Italic is the closest face macOS ships. */
inline sk_sp<SkTypeface> menuFace(bool italic = true) {
  auto mgr = sigil::weave::ports::systemFontManager();
  const auto slant =
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant;
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      "Avenir Next Condensed", SkFontStyle(SkFontStyle::kBlack_Weight,
                                           SkFontStyle::kNormal_Width, slant));
  if (!f)
    f = mgr->matchFamilyStyle(
        "Helvetica Neue", SkFontStyle(SkFontStyle::kBlack_Weight,
                                      SkFontStyle::kCondensed_Width, slant));
  if (!f) f = mgr->matchFamilyStyle(nullptr, SkFontStyle::BoldItalic());
  return f;
}

/** Menu voice: heavy condensed italic, negative tracking, with an optional
 *  2px #5D6A88 ring underlay — the outline ring the original's text
 *  shadows produce. */
inline sigil::weave::TextStyle menuType(float size, SkColor4f fill, float ringW,
                                        bool italic = true) {
  static sk_sp<SkTypeface> faceI = menuFace(true);
  static sk_sp<SkTypeface> faceU = menuFace(false);
  sigil::weave::TextStyle s;
  s.shaping.typeface = italic ? faceI : faceU;
  s.shaping.fontSize = size;
  // The original tracks around -0.14em on Rodin; Avenir Condensed is
  // already tighter, so it needs less taken out.
  s.shaping.letterSpacing = -0.08f * size;
  // Condense the last of the way to Rodin's proportion (0.82 against its
  // regular width); Avenir Condensed already carries most of that.
  s.shaping.scaleX = 0.94f;
  s.paint.foreground.setColor(fill.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  if (ringW > 0)
    s.paint.addUnderlay(sigil::weave::kit::outline(
        kRing.toSkColor(), ringW, SkPaint::kRound_Join));
  return s;
}

inline sigil::weave::TextStyle smallType(float size, SkColor4f c,
                                         float track = 1) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor(c.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

}  // namespace persona_menu

struct PersonaMenu final : sketch::Sketch {
  // Live idle motion: 6Hz-quantized water clock, the wedge heartbeat, the
  // cursor's damped diagonal overshoot.
  choreograph::Output<float> qTime{0};
  choreograph::Output<float> wedgePulse{1};
  choreograph::Output<float> curDx{40}, curDy{-40};
  // The cursor's landing carries its own velocity, so it is a spring and
  // not a curve: the offset is what rings down, and the two triangles
  // read it on both axes.
  motion::Spring cursorFlight{40.0f, 0.0f};

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kSceneSize.fWidth, kSceneSize.fHeight);
    ctx.captureAt(6.0);
    ctx.background({0, 0, 0, 1});
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    qTime = 0;
    wedgePulse = 1;
    curDx = 40;
    curDy = -40;
    cursorFlight = {40.0f, 0.0f};

    ticker.add([this, t = 0.0](double dt) mutable {
      namespace ch = choreograph;
      t += dt;
      // The caustics step time at 6 Hz rather than running smoothly. This
      // is not an optimization — the stepping IS the texture, and a
      // continuous version does not look like the original.
      qTime = (float)motion::quantizeTime(t, 6.0);
      // Idle heartbeat: wedge 1 -> 1.05 (100ms) -> 1 (50ms) every 600ms.
      const double ph = std::fmod(t, 0.6);
      float s = 1.0f;
      if (ph < 0.1)
        s = 1.0f + 0.05f * ch::easeOutQuad((float)(ph / 0.1));
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
            motion::spring(cursorFlight, 0.0f, std::min(dt, tau),
                           {.periodSeconds = 0.391f, .damping = 0.215f});
      curDx = cursorFlight.value;
      curDy = -cursorFlight.value;
      return true;
    });

    composer.render(describe());
  }

  /** Both caustic layers in one pass, with a 4-tap soften standing in for
   *  the reference's sigma-1.4 blur. One live material and one texture bake
   *  per 6 Hz step: because the time input holds between steps, the memo
   *  turns every intermediate frame into a blit. */
  Paint dualCaustic() {
    namespace nn = persona_menu;
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float2 uResolution;
        uniform float  uTime;   // pre-quantized to 6 Hz by the host
        uniform float4 uLight;  // cut .48 layer color (alpha = strength)
        uniform float4 uDark;   // cut .79 layer color
        float vhash(float2 p) {
          return fract(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
        }
        float vnoise(float2 p) {
          float2 i = floor(p);
          float2 f = fract(p);
          f = f * f * (3.0 - 2.0 * f);
          return mix(mix(vhash(i), vhash(i + float2(1, 0)), f.x),
                     mix(vhash(i + float2(0, 1)), vhash(i + float2(1, 1)), f.x),
                     f.y);
        }
        float layerA(float2 uv) {
          float2 o = float2(0.03, 0.01) * uTime;
          float p1 = vnoise((uv - o) * 13.0);
          float p2 = vnoise((uv - o + 0.5) * 13.0);
          return step(0.48, abs(p1 - p2));
        }
        float layerB(float2 uv) {
          float2 o = float2(0.02, 0.115) * uTime;
          float p1 = vnoise((uv - o) * 21.0);
          float p2 = vnoise((uv - o + 0.5) * 21.0);
          return step(0.79, abs(p1 - p2));
        }
        half4 sample1(float2 xy) {
          float2 uv = xy / max(uResolution.y, 1.0);
          float band = smoothstep(0.30, 0.55, uv.y);
          float a1 = layerA(uv) * uLight.a * band;
          float a2 = layerB(uv) * uDark.a * band;
          float3 rgb = uLight.rgb * a1 + uDark.rgb * a2 * (1.0 - a1);
          float a = a1 + a2 * (1.0 - a1);
          return half4(half3(rgb), a);
        }
        half4 main(float2 xy) {
          // 4-tap soften ~ the sigma-1.4 blur at 1/3 res of the recipe.
          half4 acc = sample1(xy + float2(-1.1, -0.7)) +
                      sample1(xy + float2(1.1, -0.7)) +
                      sample1(xy + float2(-1.1, 0.9)) +
                      sample1(xy + float2(1.1, 0.9));
          return acc * 0.25;
        }
      )"));
      if (!effect) SkDebugf("persona dualCaustic: %s\n", err.c_str());
      return effect;
    }();
    Paint m = Paint::sksl(fx);
    m.uniform("uLight", nn::kCausLight)
        .uniform("uDark", nn::kCausBub)
        .uniform("uTime", &qTime);
    return m;
  }

  /** The sea-of-souls backdrop, approximated but built in the original's
   *  layer order — the order is what produces the colour. */
  Element backdrop() {
    namespace nn = persona_menu;
    // 5-stop posterized band structure: HARD stops at the LUT positions.
    Paint bands = Paint::linear({0, 0}, {0, nn::kH},
                                      {{0.000f, nn::kLut0},
                                       {0.309f, nn::kLut0},
                                       {0.309f, nn::kLut1},
                                       {0.480f, nn::kLut1},
                                       {0.480f, nn::kLut2},
                                       {0.768f, nn::kLut2},
                                       {0.768f, nn::kLut3},
                                       {0.813f, nn::kLut3},
                                       {0.813f, nn::kLut4},
                                       {1.000f, nn::kLut4}});

    // Three Z-planes so steady-state recomposition is BLITS, not
    // re-raster: everything below the sea is one static texture, the sea
    // re-bakes at its own 6 Hz, the framing gradients above are another
    // static texture. A live ancestor recomposites per frame on raster —
    // each plane must therefore be one cheap draw.
    return box()
        .inset(0)
        .child(box()
                   .inset(0)
                   .cache(Cache::Texture)  // static under-plane: ground +
                                           // bands + noise + veil, one blit
                   .fill(Paint::linear(
                       {0, 0}, {0, nn::kH},
                       {{0.0f, nn::kGroundDark}, {1.0f, nn::kGround}}))
                   .child(box().inset(0).fill(bands).opacity(0.97f))
                   .child(box()
                              .inset(0)
                              .fill(Paint::recipe(field::noise(0.006f, 4)))
                              .opacity(0.20f)
                              .blend(SkBlendMode::kSoftLight))
                   .child(box().inset(0).fill(Paint::solid(nn::kTintVeil))))
        // The sea: one dual-layer 6Hz shader, its own texture plane --
        // baked at HALF raster scale and linear-upscaled at the blit.
        // The bands are watercolor-soft already, so the reduced bake
        // reads identically while each 6 Hz re-bake evaluates a quarter
        // of the pixels.
        .child(box()
                   .inset(0)
                   .cache(Cache::Texture)
                   .bakeScale(0.5f)
                   .fill(dualCaustic()))
        // static over-plane: the framing gradients, one blit
        .child(
            box()
                .inset(0)
                .cache(Cache::Texture)
                .child(box().inset(0).fill(Paint::linear(
                    {0, nn::kH * 0.60f}, {0, nn::kH},
                    {{0.0f,
                      {nn::kBotDark.fR, nn::kBotDark.fG, nn::kBotDark.fB, 0}},
                     {1.0f,
                      {nn::kBotDark.fR, nn::kBotDark.fG, nn::kBotDark.fB,
                       0.88f}}})))
                .child(box().inset(0).fill(
                    Paint::linear({0, 0}, {0, nn::kH * 0.42f},
                                     {{0.0f, nn::kTopCyan},
                                      {1.0f,
                                       {nn::kTopCyan.fR, nn::kTopCyan.fG,
                                        nn::kTopCyan.fB, 0}}}))));
  }

  /** Unselected sticker: one of the three cyans, soft black under-glow +
   *  #5D6A88 ring standing in for the original's text shadows, its OWN
   *  rotation, jitter and z from the
   *  ladder, entering with the fade + -30px drop. */
  Element plainRow(int i) {
    namespace nn = persona_menu;
    namespace ch = choreograph;
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
        .translateY(animate(motion::from(-30.0f).to(0.0f), {400ms, &ch::easeOutQuint}))
        .opacity(animate(motion::from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad}))
        .cache(Cache::Texture)
        .child(text(toU8(r.label), nn::menuType(41, r.color, 1.8f))
                   .effect(styles::textGlow({0, 0, 0, 0.5f}, 3.5f)));
  }

  /** The selected sticker: black label at 1.5x on a
   *  WHITE sliver wedge (+8 deg, heartbeat-scaled) over a PINK back-wedge,
   *  the RED misprint echo offset (3,-6) clipped INSIDE the wedge
   *  (counter-rotated so the echo tracks the label, not the wedge
   *  frame). */
  Element selectedRow() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const nn::Row& r = nn::kRows[nn::kSelected];
    // The wedge is cut to the SELECTED label, not to the widest one:
    // at nine rows a 330px slab buried its neighbours.
    const float lx = 20, ly = -2;  // label, row-local
    const float wW = 250, wH = 68;

    Element row =
        box()
            .key(r.label)
            .left(nn::kBaseX + r.dx)
            .top(r.y - 12)
            .width(264)
            .height(78)
            .rotate(r.rot)
            .zIndex(r.z)
            .translateY(
                animate(motion::from(-30.0f).to(0.0f), {400ms, &ch::easeOutQuint}))
            .opacity(animate(motion::from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad}));
    // pink back-wedge, misregistered under the white one
    row.child(box()
                  .left(10)
                  .top(3)
                  .width(wW)
                  .height(wH)
                  .shape(nn::sliverWedge())
                  .rotate(8)
                  .fill(Paint::solid(nn::kPink)));
    // white wedge -- clips the red echo; idle heartbeat on scale.
    // The echo's top carries an extra +5px. The wedge rotates +8 deg about
    // its OWN centre, which walks the echo up by about that much, so the
    // offset has to be pre-compensated for the misprint to land at its
    // intended (3,-6).
    row.child(box()
                  .left(0)
                  .top(-6)
                  .width(wW)
                  .height(wH)
                  .shape(nn::sliverWedge())
                  .rotate(8)
                  .clip(true)
                  .fill(Paint::solid(nn::kPaper))
                  .scale(&wedgePulse)
                  .child(text(toU8(r.label), nn::menuType(50, nn::kRedC, 0))
                             .left(lx + 3)
                             .top(3)
                             .rotate(-8)));
    // the black label (1.5x the unselected size), no glow -- ink on paper
    row.child(
        text(toU8(r.label), nn::menuType(50, nn::kInk, 0)).left(lx).top(ly));
    return row;
  }

  /** Two-triangle cursor: red under white, offset (1,5) +2 deg, root
   *  -16 deg, additive red; spawns at +0.4s and rings home on the
   *  spring the ticker steps. */
  Element cursor() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
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
        .translateX(&curDx)
        .translateY(&curDy)
        .opacity(animate(motion::from(0.0f).to(1.0f), {60ms, &ch::easeOutQuad, 400ms}))
        // The original draws this additively. At this size over the navy
        // sea, kPlus washes the red rim out completely, so it stays a plain
        // red fill.
        .child(box()
                   .inset(0)
                   .shape(shapes::polygon(3, 92))
                   .fill(Paint::solid(nn::kRedC))
                   .translateX(1)
                   .translateY(5))
        .child(box()
                   .inset(0)
                   .shape(shapes::polygon(3, 90))
                   .fill(Paint::solid(nn::kPaper)));
  }

  Element promptCircle(const char* glyph) {
    namespace nn = persona_menu;
    return box()
        .width(32)
        .height(32)
        .shape(shapes::squircle(2.0f))
        .fill(SkColor4f{nn::kGroundDark.fR, nn::kGroundDark.fG,
                        nn::kGroundDark.fB, 0.8f})
        .stroke(stroke(3, Fill::color(nn::kPaper)))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(text(toU8(glyph), nn::smallType(14, nn::kPaper, 0)));
  }

  /** The date stamp the pause menu wears in its top-left corner: the day
   *  as a big italic numeral pair, the weekday and the block of day beside
   *  it, the location under a hairline. */
  Element dateBlock() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    return box()
        .key("date")
        .left(44)
        .top(34)
        .column()
        .zIndex(8)
        .translateX(animate(motion::from(-30.0f).to(0.0f), {420ms, &ch::easeOutQuint}))
        .opacity(animate(motion::from(0.0f).to(1.0f), {340ms}))
        .child(
            box()
                .row()
                .alignItems(Align::End)
                .child(text(toU8("07/22"), nn::menuType(38, nn::kPaper, 2.0f))
                           .effect(styles::textGlow({0, 0, 0, 0.45f}, 3)))
                .child(box()
                           .column()
                           .margin(11, 0, 0, 5)
                           .child(text(toU8("SUNDAY"),
                                       nn::smallType(11, nn::kCyanC, 2.6f)))
                           .child(text(toU8("EVENING"),
                                       nn::smallType(11, nn::kCyanB, 2.6f))
                                      .margin(0, 3, 0, 0))))
        .child(box()
                   .width(168)
                   .height(2)
                   .margin(0, 7, 0, 5)
                   .fill(Paint::linear(
                       {0, 0}, {168, 0},
                       {{0.0f, {1, 1, 1, 0.85f}}, {1.0f, {1, 1, 1, 0.0f}}})))
        .child(
            text(toU8("IWATODAI DORM"), nn::smallType(11, nn::kPaper, 2.2f)));
  }

  /** The party rail: four slanted cards with HP and SP. P3R skews every
   *  card, so these are parallelograms, not rectangles, and each one
   *  slides in from the right on the list's own stagger. */
  Element partyPanel() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
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
    constexpr SkColor4f kHp{0.549f, 0.910f, 0.627f, 1};  // #8CE8A0
    constexpr SkColor4f kSp{0.416f, 0.722f, 1.000f, 1};  // #6ABBFF

    auto bar = [&](const char* label, int value, int max, SkColor4f color) {
      const float frac = max > 0 ? (float)value / (float)max : 0.0f;
      char numbers[24];
      std::snprintf(numbers, sizeof(numbers), "%d/%d", value, max);
      return box()
          .row()
          .alignItems(Align::Center)
          .gap(6)
          .child(text(toU8(label), nn::smallType(9, color, 1.4f)).width(16))
          .child(box()
                     .width(84)
                     .height(6)
                     .grow(0)
                     .fill(Paint::solid({0, 0.05f, 0.18f, 0.55f}))
                     .child(box()
                                .left(0)
                                .top(0)
                                .width(Dim(84 * frac))
                                .height(Dim(6.0f))
                                .fill(Paint::linear(
                                    {0, 0}, {0, 6},
                                    {{0.0f,
                                      {std::min(1.0f, color.fR * 1.4f),
                                       std::min(1.0f, color.fG * 1.4f),
                                       std::min(1.0f, color.fB * 1.4f), 1}},
                                     {1.0f, color}}))))
          .child(text(toU8(numbers), nn::smallType(9, nn::kPaper, 0.6f)));
    };

    Element rail = box()
                       .key("party")
                       .right(41)
                       .bottom(74)
                       .column()
                       .gap(7)
                       .zIndex(8)
                       .staggerChildren(60ms);
    for (const Member& m : kParty) {
      char level[16];
      std::snprintf(level, sizeof(level), "LV %d", m.level);
      rail.child(
          box()
              .width(246)
              .height(52)
              .rotate(-4)
              .translateX(
                  animate(motion::from(46.0f).to(0.0f), {440ms, &ch::easeOutQuint}))
              .opacity(animate(motion::from(0.0f).to(1.0f), {360ms}))
              .shape(shapes::parallelogram(9))
              .fill(Paint::linear({0, 0}, {246, 0},
                                     {{0.0f, {0.02f, 0.16f, 0.42f, 0.78f}},
                                      {1.0f, {0.02f, 0.30f, 0.62f, 0.55f}}}))
              .stroke(stroke(1.4f, Fill::color({1, 1, 1, 0.55f})))
              .column()
              .padding(17, 7)
              .gap(2)
              .child(box()
                         .row()
                         .alignItems(Align::End)
                         .child(text(toU8(m.name),
                                     nn::menuType(17, nn::kPaper, 1.0f))
                                    .grow(1))
                         .child(text(toU8(level),
                                     nn::smallType(10, nn::kCyanB, 1.6f))))
              .child(bar("HP", m.hp, m.hpMax, kHp))
              .child(bar("SP", m.sp, m.spMax, kSp)));
    }
    return rail;
  }

  Element describe() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    return stack()
        .fill(nn::kGroundDark)
        .child(backdrop())
        // ---- giant rotated index numeral, behind the menu ----
        .child(text(toU8("04"),
                    [] {
                      auto s = nn::menuType(220, nn::kNumeral, 0, false);
                      // The original tracks this at -0.2em on FOT-Rodin.
                      // Avenir's digit shapes merge sooner than Rodin's, so
                      // 0.88 condensation with -0.05em is the deepest overlap
                      // that still reads as two digits.
                      s.shaping.scaleX = 0.88f;
                      s.shaping.letterSpacing = -0.05f * 220;
                      return s;
                    }())
                   .centerAt({450, 306})
                   .rotate(90)
                   .zIndex(1)
                   .blend(SkBlendMode::kScreen)
                   .opacity(animate(motion::from(0.0f).to(0.85f), {500ms}))
                   // 220px digits render as glyph PATHS (over the atlas
                   // cutoff); bake them once, the rotation rides outside
                   .cache(Cache::Texture))
        // ---- the sticker scatter; stagger 33ms BOTTOM-UP: children are
        //      declared bottom-first (zIndex owns paint order) ----
        .child(box()
                   .key("menu")
                   .left(nn::kMenuX)
                   .top(nn::kMenuY)
                   .width(450)
                   .height(530)
                   .zIndex(2)
                   .staggerChildren(33ms)
                   // declared BOTTOM-UP: the stagger runs in declaration
                   // order and zIndex owns paint order, so the list
                   // enters from SYSTEM upward the way the game does
                   .child(plainRow(8))    // SYSTEM (bottom -- enters first)
                   .child(plainRow(7))    // CALENDAR
                   .child(plainRow(6))    // SOCIAL LINK
                   .child(plainRow(5))    // QUEST
                   .child(plainRow(4))    // STATS
                   .child(selectedRow())  // PERSONA
                   .child(plainRow(2))    // EQUIP
                   .child(plainRow(1))    // ITEM
                   .child(plainRow(0)))   // SKILL
        .child(cursor())
        .child(dateBlock())
        .child(partyPanel())
        // ---- right-anchored tooltip title over the COMMAND rule ----
        .child(box()
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
                   .translateX(animate(motion::from(36.0f).to(0.0f),
                                       {400ms, &ch::easeOutQuint}))
                   .opacity(animate(motion::from(0.0f).to(1.0f), {300ms}))
                   .child(text(toU8("PERSONA"), nn::menuType(30, nn::kPaper, 2))
                              .effect(styles::textGlow({0, 0, 0, 0.5f}, 3)))
                   .child(box()
                              .row()
                              .alignItems(Align::Center)
                              .margin(0, 6, 0, 0)
                              .child(text(toU8("COMMAND"),
                                          nn::smallType(12, nn::kCyanB, 2)))
                              .child(box()
                                         .width(120)
                                         .height(2)
                                         .fill(SkColor4f{1, 1, 1, 0.8f})
                                         .margin(8, 0, 0, 0))))
        // ---- button prompts, bottom-right ----
        .child(
            box()
                .key("prompts")
                .right(41)
                .bottom(28)
                .row()
                .alignItems(Align::Center)
                .zIndex(8)
                .opacity(animate(motion::from(0.0f).to(1.0f),
                                 {400ms, &ch::easeOutQuad, 250ms}))
                .child(promptCircle("O"))
                .child(
                    text(toU8("CONFIRM"), nn::smallType(11, nn::kCyanB, 1.5f))
                        .margin(8, 0, 22, 0))
                .child(promptCircle("X"))
                .child(text(toU8("BACK"), nn::smallType(11, nn::kCyanB, 1.5f))
                           .margin(8, 0, 0, 0)));
  }
};

}  // namespace

SIGIL_SKETCH_AS(PersonaMenu, "persona menu", "Catalog \xc2\xb7 Game UI",
                "P3R menu grammar")
