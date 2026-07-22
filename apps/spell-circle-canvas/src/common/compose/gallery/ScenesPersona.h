#pragma once
// The Persona 3 Reload pause-menu card (REFERENCES.md sec.1,
// recreation-verified), ported from sketch/sketches/p3r_study.cpp:
//  - sticker SCATTER: nine rows, and every offset, rotation, colour and
//    z_index is lifted from Ultipuk/persona_3_reload_pause_menu's
//    main_menu_pause_ui.tscn rather than invented -- see the kRows
//    comment for the conversion. Rows overlap; that is the design.
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
//    0/.31/.48/.77/.81 -> patterns::noise organic variation -> #007FD2
//    tint veil -> two SkSL caustic layers (alpha = step(cut,|p1-p2|)),
//    TIME QUANTIZED at 6 Hz host-side (floor(t*6)/6) under a sigma-1.4
//    blur -> dark bottom + cyan top gradients.
//  - chrome: giant rotated index numeral (rotate 90 deg, #787878,
//    condense 0.88) behind the menu; right-anchored tooltip title over
//    a "COMMAND ----" rule; button prompt circles.
//  - entrance: items fade + drop from -30px over 0.4s (the recreation's
//    own 0.4s tween from Vector2.UP * 30), 33ms stagger
//    BOTTOM-UP (children declared bottom-first; zIndex owns paint
//    order); the two-triangle cursor spawns at +0.4s and lands with a
//    damped diagonal overshoot along (1,-1): +40 -> -20 -> +10 -> 0.
//
// 960x640 -> 900x640: x positions compressed by 0.9375 (scatter origin
// 96 -> 90, menu x 64 -> 60, numeral x 480 -> 450, right anchors 46/44
// -> 43/41); type sizes, wedge geometry, and all y positions unchanged.

#include "GalleryCore.h"

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <functional>

namespace compose_gallery {

namespace persona_menu {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;

// REFERENCES.md sec.1 palette, recreation-verified, verbatim.
constexpr SkColor4f kGround{0.0039f, 0.3725f, 0.8000f, 1};   // #015FCC
constexpr SkColor4f kGroundDark{0.0118f, 0.1216f, 0.3922f, 1}; // #031F64
constexpr SkColor4f kCyanA{0.0863f, 0.8118f, 0.9843f, 1};    // #16CFFB
constexpr SkColor4f kCyanB{0.4902f, 0.9020f, 0.9922f, 1};    // #7DE6FD
constexpr SkColor4f kCyanC{0.4667f, 0.9961f, 0.9882f, 1};    // #77FEFC
constexpr SkColor4f kPink{0.9922f, 0.4667f, 0.8510f, 1};     // #FD77D9
constexpr SkColor4f kRedC{1, 0, 0, 1};                       // #F00
constexpr SkColor4f kLut0{0.0235f, 0.0392f, 0.1686f, 1};     // #060A2B
constexpr SkColor4f kLut1{0.0471f, 0.0706f, 0.2980f, 1};     // #0C124C
constexpr SkColor4f kLut2{0.1059f, 0.2196f, 0.9412f, 1};     // #1B38F0
constexpr SkColor4f kLut3{0.2000f, 0.3176f, 1.0000f, 1};     // #3351FF
constexpr SkColor4f kLut4{0.9882f, 0.9961f, 0.9961f, 1};     // #FCFEFE
constexpr SkColor4f kCausLight{0.5882f, 0.8902f, 0.9412f, 0.47f}; // #96E3F0
constexpr SkColor4f kCausBub{0.3294f, 0.9294f, 0.9176f, 0.255f};  // #54EDEA
constexpr SkColor4f kBotDark{0, 0.0627f, 0.4275f, 1};        // #00106D
constexpr SkColor4f kTopCyan{0, 0.9882f, 0.9490f, 0.39f};    // #00FCF2 a.39
constexpr SkColor4f kTintVeil{0, 0.4980f, 0.8235f, 0.36f};   // #007FD2
constexpr SkColor4f kNumeral{0.4706f, 0.4706f, 0.4706f, 1};  // #787878
constexpr SkColor4f kRing{0.3647f, 0.4157f, 0.5333f, 1};     // #5D6A88
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
  const char *label;
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
constexpr int kSelected = 3; // PERSONA -- black on the white wedge
constexpr float kBaseX = 90; // menu-container-local scatter origin
constexpr float kMenuX = 60, kMenuY = 70; // menu container origin

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

/** Heavy condensed ITALIC -- the FOT-Rodin stand-in (sec.1: 84px, x0.82
 *  condensed, italic; Avenir Next Condensed Heavy Italic is the closest
 *  face macOS ships). */
inline sk_sp<SkTypeface> menuFace(bool italic = true) {
  auto mgr = sigil::weave::ports::systemFontManager();
  const auto slant =
      italic ? SkFontStyle::kItalic_Slant : SkFontStyle::kUpright_Slant;
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      "Avenir Next Condensed",
      SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kNormal_Width,
                  slant));
  if (!f)
    f = mgr->matchFamilyStyle(
        "Helvetica Neue",
        SkFontStyle(SkFontStyle::kBlack_Weight, SkFontStyle::kCondensed_Width,
                    slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::BoldItalic());
  return f;
}

/** Menu voice: heavy condensed italic, negative tracking; optional 2px
 *  #5D6A88 ring underlay (sec.1 shadows: the chrome-text outline ring). */
inline sigil::weave::TextStyle menuType(float size, SkColor4f fill,
                                        float ringW, bool italic = true) {
  static sk_sp<SkTypeface> faceI = menuFace(true);
  static sk_sp<SkTypeface> faceU = menuFace(false);
  sigil::weave::TextStyle s;
  s.shaping.typeface = italic ? faceI : faceU;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = -0.08f * size; // sec.1 tracking ~= -0.14em
                                           // (Rodin); Avenir Cond is
                                           // tighter already
  s.shaping.scaleX = 0.94f; // condense() the last stretch to the Rodin
                            // proportion (sec.1 x0.82 vs regular; Avenir
                            // Condensed already carries most of it)
  s.paint.foreground.setColor(fill.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  if (ringW > 0)
    s.paint.addUnderlay(sigil::weave::PaintLayer::outline(
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

/** The caustic layer (sec.1 sea-of-souls): two value-noise fields offset
 *  by .5 UV, alpha = step(cut, |p1-p2|), vertically masked into the sea
 *  band. uTime is bound to a HOST-QUANTIZED output (floor(t*6)/6) -- the
 *  water steps at 6 Hz, "we imagine the interpolation ourselves". */
inline sk_sp<SkRuntimeEffect> causticFx() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
      uniform float2 uResolution;
      uniform float  uTime;   // pre-quantized to 6 Hz by the host
      uniform float  uCut;
      uniform float  uScale;
      uniform float2 uVel;    // uv/s (y+ = texture rises)
      uniform float4 uColor;
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
      half4 main(float2 xy) {
        float2 uv = xy / max(uResolution.y, 1.0);
        float2 o = uVel * uTime;
        float p1 = vnoise((uv - o) * uScale);
        float p2 = vnoise((uv - o + 0.5) * uScale);
        float band = smoothstep(0.30, 0.55, xy.y / max(uResolution.y, 1.0));
        float a = step(uCut, abs(p1 - p2)) * uColor.a * band;
        return half4(half3(uColor.rgb) * a, a);
      }
    )"));
    if (!effect)
      SkDebugf("persona_menu caustics shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

} // namespace persona_menu

struct PersonaMenuScene final : Scene {
  // Live idle motion: 6Hz-quantized water clock, the wedge heartbeat, the
  // cursor's damped diagonal overshoot.
  choreograph::Output<float> qTime{0};
  choreograph::Output<float> wedgePulse{1};
  choreograph::Output<float> curDx{40}, curDy{-40};

  const char *name() const override { return "persona menu"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    qTime = 0;
    wedgePulse = 1;
    curDx = 40;
    curDy = -40;

    ticker.add([this, t = 0.0](double dt) mutable {
      namespace ch = choreograph;
      t += dt;
      // sec.1 CRITICAL TEXTURE: caustics step time at 6 Hz.
      qTime = (float)(std::floor(t * 6.0) / 6.0);
      // Idle heartbeat: wedge 1 -> 1.05 (100ms) -> 1 (50ms) every 600ms.
      const double ph = std::fmod(t, 0.6);
      float s = 1.0f;
      if (ph < 0.1)
        s = 1.0f + 0.05f * ch::easeOutQuad((float)(ph / 0.1));
      else if (ph < 0.15)
        s = 1.05f - 0.05f * (float)((ph - 0.1) / 0.05);
      wedgePulse = s;
      // Cursor: spawn +0.4s, damped overshoot along (1,-1):
      // +40 -> -20 (0.2s) -> +10 (0.1s) -> 0 (0.1s).
      const double tau = t - 0.4;
      auto seg = [](double x, float a, float b) {
        const float e = 0.5f - 0.5f * (float)std::cos(x * 3.14159265);
        return a + (b - a) * e;
      };
      float d;
      if (tau < 0)
        d = 40;
      else if (tau < 0.2)
        d = seg(tau / 0.2, 40, -20);
      else if (tau < 0.3)
        d = seg((tau - 0.2) / 0.1, -20, 10);
      else if (tau < 0.4)
        d = seg((tau - 0.3) / 0.1, 10, 0);
      else
        d = 0;
      curDx = d;
      curDy = -d;
      return true;
    });

    composer.render(describe());
  }

  /** BOTH verified caustic layers in one pass, with a 4-tap soften that
   *  stands in for the recipe's sigma-1.4 blur — one live material, one
   *  texture bake per 6 Hz step (the memo turns the other frames into
   *  blits). */
  Material dualCaustic() {
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
      if (!effect)
        SkDebugf("persona dualCaustic: %s\n", err.c_str());
      return effect;
    }();
    Material m = Material::sksl(fx);
    m.uniform("uLight", nn::kCausLight)
        .uniform("uDark", nn::kCausBub)
        .uniform("uTime", &qTime);
    return m;
  }

  Material caustic(float cut, float scale, SkColor4f color,
                   std::array<float, 2> vel) {
    namespace nn = persona_menu;
    Material m = Material::sksl(nn::causticFx(), {{"uCut", cut},
                                                  {"uScale", scale}});
    m.uniform("uColor", color)
        .uniform("uVel", vel)
        .uniform("uTime", &qTime);
    return m;
  }

  /** sec.1 sea-of-souls, approximated in the verified layer order. */
  Element backdrop() {
    namespace nn = persona_menu;
    // 5-stop posterized band structure: HARD stops at the LUT positions.
    Material bands = Material::linear(
        {0, 0}, {0, nn::kH},
        {{0.000f, nn::kLut0}, {0.309f, nn::kLut0},
         {0.309f, nn::kLut1}, {0.480f, nn::kLut1},
         {0.480f, nn::kLut2}, {0.768f, nn::kLut2},
         {0.768f, nn::kLut3}, {0.813f, nn::kLut3},
         {0.813f, nn::kLut4}, {1.000f, nn::kLut4}});

    // Three Z-planes so steady-state recomposition is BLITS, not
    // re-raster: everything below the sea is one static texture, the sea
    // re-bakes at its own 6 Hz, the framing gradients above are another
    // static texture. A live ancestor recomposites per frame on raster —
    // each plane must therefore be one cheap draw.
    return box().absolute().inset(0)
        .child(box().absolute().inset(0)
                   .cache(Cache::Texture) // static under-plane: ground +
                                          // bands + noise + veil, one blit
                   .fill(Material::linear(
                       {0, 0}, {0, nn::kH},
                       {{0.0f, nn::kGroundDark}, {1.0f, nn::kGround}}))
                   .child(box().absolute().inset(0).fill(bands).opacity(0.9f))
                   .child(box().absolute().inset(0)
                              .fill(patterns::noise(0.006f, 4))
                              .opacity(0.32f)
                              .blend(SkBlendMode::kSoftLight))
                   .child(box().absolute().inset(0).fill(
                       Material::solid(nn::kTintVeil))))
        // The sea: one dual-layer 6Hz shader, its own texture plane --
        // baked at HALF raster scale and linear-upscaled at the blit.
        // The bands are watercolor-soft already, so the reduced bake
        // reads identically while each 6 Hz re-bake evaluates a quarter
        // of the pixels.
        .child(box().absolute().inset(0)
                   .cache(Cache::Texture).bakeScale(0.5f)
                   .fill(dualCaustic()))
        // static over-plane: the framing gradients, one blit
        .child(box().absolute().inset(0)
                   .cache(Cache::Texture)
                   .child(box().absolute().inset(0).fill(Material::linear(
                       {0, nn::kH * 0.60f}, {0, nn::kH},
                       {{0.0f,
                         {nn::kBotDark.fR, nn::kBotDark.fG, nn::kBotDark.fB,
                          0}},
                        {1.0f,
                         {nn::kBotDark.fR, nn::kBotDark.fG, nn::kBotDark.fB,
                          0.88f}}})))
                   .child(box().absolute().inset(0).fill(Material::linear(
                       {0, 0}, {0, nn::kH * 0.42f},
                       {{0.0f, nn::kTopCyan},
                        {1.0f,
                         {nn::kTopCyan.fR, nn::kTopCyan.fG, nn::kTopCyan.fB,
                          0}}}))));
  }

  /** Unselected sticker: one of the three cyans, soft black under-glow +
   *  #5D6A88 ring (sec.1 shadows), its OWN rotation/jitter/z from the
   *  ladder, entering with the fade + -30px drop. */
  Element plainRow(int i) {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const nn::Row &r = nn::kRows[i];
    // The sigma-3.5 glow re-blurs on every picture replay, and the wedge
    // heartbeat keeps this whole menu live -- so bake each sticker to a
    // texture once it settles. 14px of padding keeps raster room for the
    // glow tail; the pin shifts up-left to compensate. Entrance transforms
    // and the row rotation apply outside the bake.
    return box().key(r.label)
        .left(nn::kBaseX + r.dx - 14).top(r.y - 14)
        .padding(14)
        .rotate(r.rot).zIndex(r.z)
        .translateY(withFrom(-30.0f, 0.0f, {400ms, &ch::easeOutQuint}))
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad}))
        .cache(Cache::Texture)
        .child(text(toU8(r.label), nn::menuType(41, r.color, 1.8f))
                   .effect(styles::textGlow({0, 0, 0, 0.5f}, 3.5f)));
  }

  /** The selected sticker (sec.1 selection): black label at 1.5x on a
   *  WHITE sliver wedge (+8 deg, heartbeat-scaled) over a PINK back-wedge,
   *  the RED misprint echo offset (3,-6) clipped INSIDE the wedge
   *  (counter-rotated so the echo tracks the label, not the wedge
   *  frame). */
  Element selectedRow() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const nn::Row &r = nn::kRows[nn::kSelected];
    // The wedge is cut to the SELECTED label, not to the widest one:
    // at nine rows a 330px slab buried its neighbours.
    const float lx = 20, ly = -2; // label, row-local
    const float wW = 250, wH = 68;

    Element row = box().key(r.label)
                      .left(nn::kBaseX + r.dx).top(r.y - 12)
                      .width(264).height(78)
                      .rotate(r.rot).zIndex(r.z)
                      .translateY(withFrom(-30.0f, 0.0f,
                                           {400ms, &ch::easeOutQuint}))
                      .opacity(withFrom(0.0f, 1.0f,
                                        {400ms, &ch::easeOutQuad}));
    // pink back-wedge, misregistered under the white one
    row.child(box().absolute().left(10).top(3).width(wW).height(wH)
                  .outline(nn::sliverWedge()).rotate(8)
                  .fill(Material::solid(nn::kPink)));
    // white wedge -- clips the red echo; idle heartbeat on scale.
    // Echo top carries +5px: the wedge's +8 deg spin about ITS center
    // walks the echo ~5px up; the compensation restores the verified
    // (3,-6).
    row.child(box().absolute().left(0).top(-6).width(wW).height(wH)
                  .outline(nn::sliverWedge()).rotate(8).clip(true)
                  .fill(Material::solid(nn::kPaper))
                  .scale(&wedgePulse)
                  .child(text(toU8(r.label), nn::menuType(50, nn::kRedC, 0))
                             .left(lx + 3).top(3)
                             .rotate(-8)));
    // the black label (1.5x the unselected size), no glow -- ink on paper
    row.child(text(toU8(r.label), nn::menuType(50, nn::kInk, 0))
                  .left(lx).top(ly));
    return row;
  }

  /** Two-triangle cursor: red under white, offset (1,5) +2 deg, root
   *  -16 deg, additive red; spawns at +0.4s and lands with the damped
   *  diagonal overshoot (ticker-bound). */
  Element cursor() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    const nn::Row &r = nn::kRows[nn::kSelected];
    // canvas coords: the menu container origin folded into the pins
    return box().key("cursor")
        .left(nn::kMenuX + nn::kBaseX + r.dx - 46)
        .top(nn::kMenuY + r.y + 12).width(36).height(36)
        .zIndex(7).rotate(-16)
        .translateX(&curDx).translateY(&curDy)
        .opacity(withFrom(0.0f, 1.0f, {60ms, &ch::easeOutQuad, 400ms}))
        // sec.1 calls this additive; at 36px over the navy sea kPlus
        // washes the red rim out entirely, so the scene keeps it plain
        // red.
        .child(box().absolute().inset(0)
                   .outline(shapes::polygon(3, 92))
                   .fill(Material::solid(nn::kRedC))
                   .translateX(1).translateY(5))
        .child(box().absolute().inset(0)
                   .outline(shapes::polygon(3, 90))
                   .fill(Material::solid(nn::kPaper)));
  }

  Element promptCircle(const char *glyph) {
    namespace nn = persona_menu;
    return box().width(32).height(32)
        .outline(shapes::squircle(2.0f))
        .fill(SkColor4f{nn::kGroundDark.fR, nn::kGroundDark.fG,
                        nn::kGroundDark.fB, 0.8f})
        .stroke(stroke(3, Fill::color(nn::kPaper)))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(toU8(glyph), nn::smallType(14, nn::kPaper, 0)));
  }

  /** The date stamp the pause menu wears in its top-left corner: the day
   *  as a big italic numeral pair, the weekday and the block of day beside
   *  it, the location under a hairline. */
  Element dateBlock() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    return box().key("date").left(44).top(34).column().zIndex(8)
        .translateX(withFrom(-30.0f, 0.0f, {420ms, &ch::easeOutQuint}))
        .opacity(withFrom(0.0f, 1.0f, {340ms}))
        .child(box().row().alignItems(Align::End)
                   .child(text(toU8("07/22"),
                               nn::menuType(38, nn::kPaper, 2.0f))
                              .effect(styles::textGlow({0, 0, 0, 0.45f}, 3)))
                   .child(box().column().margin(11, 0, 0, 5)
                              .child(text(toU8("SUNDAY"),
                                          nn::smallType(11, nn::kCyanC, 2.6f)))
                              .child(text(toU8("EVENING"),
                                          nn::smallType(11, nn::kCyanB, 2.6f))
                                         .margin(0, 3, 0, 0))))
        .child(box().width(168).height(2).margin(0, 7, 0, 5)
                   .fill(Material::linear({0, 0}, {168, 0},
                                          {{0.0f, {1, 1, 1, 0.85f}},
                                           {1.0f, {1, 1, 1, 0.0f}}})))
        .child(text(toU8("IWATODAI DORM"),
                    nn::smallType(11, nn::kPaper, 2.2f)));
  }

  /** The party rail: four slanted cards with HP and SP. P3R skews every
   *  card, so these are parallelograms, not rectangles, and each one
   *  slides in from the right on the list's own stagger. */
  Element partyPanel() {
    namespace nn = persona_menu;
    namespace ch = choreograph;
    using namespace std::chrono_literals;
    struct Member {
      const char *name;
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

    auto bar = [&](const char *label, int value, int max, SkColor4f color) {
      const float frac = max > 0 ? (float)value / (float)max : 0.0f;
      char numbers[24];
      std::snprintf(numbers, sizeof(numbers), "%d/%d", value, max);
      return box().row().alignItems(Align::Center).gap(6)
          .child(text(toU8(label), nn::smallType(9, color, 1.4f))
                     .width(16))
          .child(box().width(84).height(6).grow(0)
                     .fill(Material::solid({0, 0.05f, 0.18f, 0.55f}))
                     .child(box().absolute().left(0).top(0)
                                .width(Dim(84 * frac)).height(Dim(6.0f))
                                .fill(Material::linear(
                                    {0, 0}, {0, 6},
                                    {{0.0f, {std::min(1.0f, color.fR * 1.4f),
                                             std::min(1.0f, color.fG * 1.4f),
                                             std::min(1.0f, color.fB * 1.4f),
                                             1}},
                                     {1.0f, color}}))))
          .child(text(toU8(numbers), nn::smallType(9, nn::kPaper, 0.6f)));
    };

    Element rail = box().key("party").right(41).bottom(74).column().gap(7)
                       .zIndex(8).staggerChildren(60ms);
    for (const Member &m : kParty) {
      char level[16];
      std::snprintf(level, sizeof(level), "LV %d", m.level);
      rail.child(
          box().width(246).height(52).rotate(-4)
              .translateX(withFrom(46.0f, 0.0f, {440ms, &ch::easeOutQuint}))
              .opacity(withFrom(0.0f, 1.0f, {360ms}))
              .outline(shapes::parallelogram(9))
              .fill(Material::linear({0, 0}, {246, 0},
                                     {{0.0f, {0.02f, 0.16f, 0.42f, 0.78f}},
                                      {1.0f, {0.02f, 0.30f, 0.62f, 0.55f}}}))
              .stroke(stroke(1.4f, Fill::color({1, 1, 1, 0.55f})))
              .column().padding(17, 7).gap(2)
              .child(box().row().alignItems(Align::End)
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
        // ---- giant rotated index numeral, behind the menu (sec.1) ----
        .child(text(toU8("04"), [] {
                 auto s = nn::menuType(220, nn::kNumeral, 0, false);
                 // sec.1 says -0.2em (FOT-Rodin). Avenir's digit shapes
                 // merge sooner than Rodin's: x0.88 condensation + -0.05em
                 // is the deepest overlap that keeps "04" reading as two
                 // digits.
                 s.shaping.scaleX = 0.88f;
                 s.shaping.letterSpacing = -0.05f * 220;
                 return s;
               }())
                   .centerAt({450, 306}).rotate(90).zIndex(1)
                   .opacity(withFrom(0.0f, 1.0f, {500ms}))
                   // 220px digits render as glyph PATHS (over the atlas
                   // cutoff); bake them once, the rotation rides outside
                   .cache(Cache::Texture))
        // ---- the sticker scatter; stagger 33ms BOTTOM-UP: children are
        //      declared bottom-first (zIndex owns paint order) ----
        .child(box().key("menu").left(nn::kMenuX).top(nn::kMenuY)
                   .width(450).height(530)
                   .zIndex(2)
                   .staggerChildren(33ms)
                   // declared BOTTOM-UP: the stagger runs in declaration
                   // order and zIndex owns paint order, so the list
                   // enters from SYSTEM upward the way the game does
                   .child(plainRow(8))   // SYSTEM (bottom -- enters first)
                   .child(plainRow(7))   // CALENDAR
                   .child(plainRow(6))   // SOCIAL LINK
                   .child(plainRow(5))   // QUEST
                   .child(plainRow(4))   // STATS
                   .child(selectedRow()) // PERSONA
                   .child(plainRow(2))   // EQUIP
                   .child(plainRow(1))   // ITEM
                   .child(plainRow(0)))  // SKILL
        .child(cursor())
        .child(dateBlock())
        .child(partyPanel())
        // ---- right-anchored tooltip title over the COMMAND rule ----
        .child(box().key("tooltip").top(40 - 12).right(43 - 12).zIndex(8)
                   .alignItems(Align::End)
                   // texture-baked (the sigma-3 glow otherwise re-blurs
                   // on every root replay); 12px padding keeps raster
                   // room for the glow tail, pins shifted to compensate
                   .padding(12)
                   .cache(Cache::Texture)
                   .translateX(withFrom(36.0f, 0.0f,
                                        {400ms, &ch::easeOutQuint}))
                   .opacity(withFrom(0.0f, 1.0f, {300ms}))
                   .child(text(toU8("PERSONA"), nn::menuType(30, nn::kPaper, 2))
                              .effect(styles::textGlow({0, 0, 0, 0.5f}, 3)))
                   .child(box().row().alignItems(Align::Center)
                              .margin(0, 6, 0, 0)
                              .child(text(toU8("COMMAND"),
                                          nn::smallType(12, nn::kCyanB, 2)))
                              .child(box().width(120).height(2)
                                         .fill(SkColor4f{1, 1, 1, 0.8f})
                                         .margin(8, 0, 0, 0))))
        // ---- button prompts, bottom-right (sec.1 chrome) ----
        .child(box().key("prompts").right(41).bottom(28).row()
                   .alignItems(Align::Center).zIndex(8)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {400ms, &ch::easeOutQuad, 250ms}))
                   .child(promptCircle("O"))
                   .child(text(toU8("CONFIRM"),
                               nn::smallType(11, nn::kCyanB, 1.5f))
                              .margin(8, 0, 22, 0))
                   .child(promptCircle("X"))
                   .child(text(toU8("BACK"), nn::smallType(11, nn::kCyanB, 1.5f))
                              .margin(8, 0, 0, 0)));
  }
};

} // namespace compose_gallery
