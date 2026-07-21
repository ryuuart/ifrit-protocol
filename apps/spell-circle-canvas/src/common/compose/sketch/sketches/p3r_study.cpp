// p3r_study.cpp — a STUDY of the Persona 3 Reload pause-menu grammar
// (REFERENCES.md §1, recreation-verified 2026-07-21). Round 3 rebuilds to
// the verified grammar — the corrections vs round 2:
//
//   sticker SCATTER .. no layouts::Diagonal / uniform skew. Each row gets
//                      its OWN rotation from the deltea ladder scaled to
//                      five items (−25,−15,−20,−15, last flipped +8),
//                      per-row x-jitter (0..−80px), rows overlapping ~40%
//                      of their height, shuffled zIndex — scattered
//                      stickers converging to straight.
//   selection ........ BLACK text on a WHITE sliver-triangle wedge
//                      (rotate +8°) over a PINK #FD77D9 back-wedge, plus
//                      the RED misprint echo of the label offset (3,−6),
//                      clipped inside the wedge. NOT a cyan flood.
//                      Unselected labels cycle the three cyans
//                      #16CFFB/#7DE6FD/#77FEFC. Idle heartbeat: wedge
//                      1→1.05 (100ms) →1 (50ms) every 600ms. Two-triangle
//                      cursor: red under white, offset (1,5) +2°, root −16°.
//   shadows .......... SOFT under-glow on chrome text (styles::textGlow +
//                      2px #5D6A88 ring underlay); the ONLY hard offset is
//                      the red echo. Zero-blur sticker stacks were P5.
//   backdrop ......... sea-of-souls layer order: ground #015FCC/#031F64 →
//                      5-stop posterized bands, HARD stops at the LUT
//                      positions 0/.31/.48/.77/.81 (#060A2B #0C124C
//                      #1B38F0 #3351FF #FCFEFE) → patterns::noise organic
//                      variation → #007FD2 tint veil → two caustic layers
//                      (SkSL, alpha = step(cut,|p1−p2|), light .48/α.47
//                      #96E3F0 + bubbles .79/α.255 #54EDEA rising), TIME
//                      QUANTIZED at 6 Hz (floor(t·6)/6 host-side) under a
//                      σ1.4 blur → dark #00106D bottom + cyan #00FCF2 α.39
//                      top gradients. Halftone dropped (P5 grammar).
//   chrome ........... giant rotated index numeral (rotate 90°, #787878,
//                      tracking −0.2em) behind the menu; right-anchored
//                      tooltip title over a "COMMAND ────" rule; button
//                      prompt circles. Date/clock dropped (field HUD, not
//                      pause chrome).
//   entrance ......... items fade + drop from −30px over 0.4s, 33ms
//                      stagger BOTTOM-UP — container .staggerChildren(33ms)
//                      with children DECLARED bottom-first (zIndex owns
//                      paint order, so declaration order is free to encode
//                      the stagger); cursor spawns at +0.4s (Transition::
//                      delay) with a damped diagonal overshoot along
//                      (1,−1): +40 → −20 → +10 → 0 (ticker-driven).
//
// Headless: ComposeSketch <this> --frame p3r.png --at 2.0   (settled)
//                                              --at 0.15    (mid-entrance)

#include <sigilsketch/Sketch.h>

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
#include <chrono>
#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr float W = 960, H = 640;

// REFERENCES.md §1 palette, recreation-verified, verbatim.
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
constexpr SkColor4f kTopCyan{0, 0.9882f, 0.9490f, 0.39f};    // #00FCF2 α.39
constexpr SkColor4f kTintVeil{0, 0.4980f, 0.8235f, 0.36f};   // #007FD2
constexpr SkColor4f kNumeral{0.4706f, 0.4706f, 0.4706f, 1};  // #787878
constexpr SkColor4f kRing{0.3647f, 0.4157f, 0.5333f, 1};     // #5D6A88
constexpr SkColor4f kPaper{1, 1, 1, 1};
constexpr SkColor4f kInk{0, 0, 0, 1};

constexpr SkColor4f kCyans[3] = {kCyanA, kCyanB, kCyanC};

// The sticker scatter, deltea's ladder scaled to five items: −25,−15,−20,
// −15, LAST flipped positive (+8). x-jitter 0..−80px, y pitch ~60% of the
// rotated row height (rows overlap), shuffled z.
struct Row {
  const char *label;
  float dx, y, rot;
  int z;
};
constexpr Row kRows[] = {{"SKILL", -12, 124, -25, 3},
                         {"ITEM", -64, 182, -15, 5},
                         {"EQUIP", -28, 238, -20, 2},
                         {"PERSONA", -70, 296, -15, 6},
                         {"SYSTEM", -6, 396, +8, 4}};
constexpr int kSelected = 3; // PERSONA — black on the white wedge
constexpr float kBaseX = 96; // menu-container-local scatter origin

/** The selection wedge: a sliver that tapers to a near-point at the right
 *  (long banner, blunt tip) — the white slab the selected label sits on. */
std::function<SkPath(SkSize)> sliverWedge() {
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

/** Heavy condensed ITALIC — the FOT-Rodin stand-in (§1: 84px, ×0.82
 *  condensed, italic; Avenir Next Condensed Heavy Italic is the closest
 *  face macOS ships). */
sk_sp<SkTypeface> menuFace(bool italic = true) {
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
 *  #5D6A88 ring underlay (§1 shadows: the chrome-text outline ring). */
sigil::weave::TextStyle menuType(float size, SkColor4f fill, float ringW,
                                 bool italic = true) {
  static sk_sp<SkTypeface> faceI = menuFace(true);
  static sk_sp<SkTypeface> faceU = menuFace(false);
  sigil::weave::TextStyle s;
  s.shaping.typeface = italic ? faceI : faceU;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = -0.08f * size; // §1 tracking ≈ −0.14em (Rodin);
                                           // Avenir Cond is tighter already
  s.paint.foreground.setColor(fill.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  if (ringW > 0)
    s.paint.addUnderlay(sigil::weave::PaintLayer::outline(
        kRing.toSkColor(), ringW, SkPaint::kRound_Join));
  return s;
}

sigil::weave::TextStyle smallType(float size, SkColor4f c, float track = 1) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.paint.foreground.setColor(c.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** The caustic layer (§1 sea-of-souls): two value-noise fields offset by
 *  .5 UV, alpha = step(cut, |p1−p2|), vertically masked into the sea band.
 *  uTime is bound to a HOST-QUANTIZED output (floor(t·6)/6) — the water
 *  steps at 6 Hz, "we imagine the interpolation ourselves". */
sk_sp<SkRuntimeEffect> causticFx() {
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
      SkDebugf("p3r_study caustics shader: %s\n", err.c_str());
    return effect;
  }();
  return fx;
}

} // namespace

struct P3RStudy : sketch::Sketch {
  // Live idle motion: 6Hz-quantized water clock, the wedge heartbeat, the
  // cursor's damped diagonal overshoot.
  ch::Output<float> qTime{0};
  ch::Output<float> wedgePulse{1};
  ch::Output<float> curDx{40}, curDy{-40};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kGroundDark);

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      // §1 CRITICAL TEXTURE: caustics step time at 6 Hz.
      qTime = (float)(std::floor(t * 6.0) / 6.0);
      // Idle heartbeat: wedge 1→1.05 (100ms) →1 (50ms) every 600ms.
      const double ph = std::fmod(t, 0.6);
      float s = 1.0f;
      if (ph < 0.1)
        s = 1.0f + 0.05f * ch::easeOutQuad((float)(ph / 0.1));
      else if (ph < 0.15)
        s = 1.05f - 0.05f * (float)((ph - 0.1) / 0.05);
      wedgePulse = s;
      // Cursor: spawn +0.4s, damped overshoot along (1,−1):
      // +40 → −20 (0.2s) → +10 (0.1s) → 0 (0.1s).
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

    ctx.composer.render(describe());
  }

  Material caustic(float cut, float scale, SkColor4f color,
                   std::array<float, 2> vel) {
    Material m = Material::sksl(causticFx(), {{"uCut", cut},
                                              {"uScale", scale}});
    m.uniform("uColor", color)
        .uniform("uVel", vel)
        .uniform("uTime", &qTime);
    return m;
  }

  /** §1 sea-of-souls, approximated in the verified layer order. */
  Element backdrop() {
    // 5-stop posterized band structure: HARD stops at the LUT positions.
    Material bands = Material::linear(
        {0, 0}, {0, H},
        {{0.000f, kLut0}, {0.309f, kLut0},
         {0.309f, kLut1}, {0.480f, kLut1},
         {0.480f, kLut2}, {0.768f, kLut2},
         {0.768f, kLut3}, {0.813f, kLut3},
         {0.813f, kLut4}, {1.000f, kLut4}});

    return box().absolute().inset(0)
        // deep blue ground
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, kGroundDark}, {1.0f, kGround}}))
        .child(box().absolute().inset(0).fill(bands).opacity(0.9f))
        // organic variation over the posterized bands
        .child(box().absolute().inset(0)
                   .fill(patterns::noise(0.006f, 4))
                   .opacity(0.32f)
                   .blend(SkBlendMode::kSoftLight))
        // flat tint pull toward #007FD2 (kept lighter than the verified
        // 82% so the band structure stays legible in the study)
        .child(box().absolute().inset(0).fill(Material::solid(kTintVeil)))
        // two caustic layers, 6Hz-stepped, under a σ1.4 blur
        .child(box().absolute().inset(0)
                   .effect(Effect::filter(
                       SkImageFilters::Blur(1.4f, 1.4f, nullptr)))
                   .child(box().absolute().inset(0)
                              .fill(caustic(0.48f, 13.0f, kCausLight,
                                            {0.03f, 0.01f})))
                   .child(box().absolute().inset(0)
                              .fill(caustic(0.79f, 22.0f, kCausBub,
                                            {0.0f, 0.115f}))))
        // dark gradient bottom + cyan gradient top
        .child(box().absolute().inset(0).fill(Material::linear(
            {0, H * 0.60f}, {0, H},
            {{0.0f, {kBotDark.fR, kBotDark.fG, kBotDark.fB, 0}},
             {1.0f, {kBotDark.fR, kBotDark.fG, kBotDark.fB, 0.88f}}})))
        .child(box().absolute().inset(0).fill(Material::linear(
            {0, 0}, {0, H * 0.42f},
            {{0.0f, kTopCyan},
             {1.0f, {kTopCyan.fR, kTopCyan.fG, kTopCyan.fB, 0}}})));
  }

  /** Unselected sticker: one of the three cyans, soft black under-glow +
   *  #5D6A88 ring (§1 shadows), its OWN rotation/jitter/z from the ladder,
   *  entering with the fade + −30px drop. */
  Element plainRow(int i) {
    const Row &r = kRows[i];
    return text(toU8(r.label), menuType(46, kCyans[i % 3], 2.0f))
        .key(r.label)
        .left(kBaseX + r.dx).top(r.y)
        .rotate(r.rot).zIndex(r.z)
        .translateY(withFrom(-30.0f, 0.0f, {400ms, &ch::easeOutQuint}))
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad}))
        .effect(styles::textGlow({0, 0, 0, 0.5f}, 3.5f));
  }

  /** The selected sticker (§1 selection): black label at 1.5× on a WHITE
   *  sliver wedge (+8°, heartbeat-scaled) over a PINK back-wedge, the RED
   *  misprint echo offset (3,−6) clipped INSIDE the wedge (counter-rotated
   *  so the echo tracks the label, not the wedge frame). */
  Element selectedRow() {
    const Row &r = kRows[kSelected];
    const float lx = 26, ly = -2; // label, row-local
    const float wW = 330, wH = 92;

    Element row = box().key(r.label)
                      .left(kBaseX + r.dx).top(r.y - 18)
                      .width(346).height(104)
                      .rotate(r.rot).zIndex(r.z)
                      .translateY(withFrom(-30.0f, 0.0f,
                                           {400ms, &ch::easeOutQuint}))
                      .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad}));
    // pink back-wedge, misregistered under the white one
    row.child(box().absolute().left(10).top(3).width(wW).height(wH)
                  .outline(sliverWedge()).rotate(8)
                  .fill(Material::solid(kPink)));
    // white wedge — clips the red echo; idle heartbeat on scale.
    // Echo top carries +5px: the wedge's +8° spin about ITS center walks
    // the echo ~5px up; the compensation restores the verified (3,−6).
    row.child(box().absolute().left(0).top(-6).width(wW).height(wH)
                  .outline(sliverWedge()).rotate(8).clip(true)
                  .fill(Material::solid(kPaper))
                  .scale(&wedgePulse)
                  .child(text(toU8(r.label), menuType(64, kRedC, 0))
                             .left(lx + 3).top(3)
                             .rotate(-8)));
    // the black label (1.5× the unselected size), no glow — ink on paper
    row.child(text(toU8(r.label), menuType(64, kInk, 0)).left(lx).top(ly));
    return row;
  }

  /** Two-triangle cursor: red under white, offset (1,5) +2°, root −16°,
   *  additive red; spawns at +0.4s and lands with the damped diagonal
   *  overshoot (ticker-bound). */
  Element cursor() {
    const Row &r = kRows[kSelected];
    // canvas coords: menu container origin (64,70) folded into the pins
    return box().key("cursor")
        .left(64 + kBaseX + r.dx - 46).top(70 + r.y + 12).width(36).height(36)
        .zIndex(7).rotate(-16)
        .translateX(&curDx).translateY(&curDy)
        .opacity(withFrom(0.0f, 1.0f, {60ms, &ch::easeOutQuad, 400ms}))
        // §1 calls this additive; at 36px over the navy sea kPlus washes
        // the red rim out entirely, so the study keeps it plain red.
        .child(box().absolute().inset(0)
                   .outline(shapes::polygon(3, 92))
                   .fill(Material::solid(kRedC))
                   .translateX(1).translateY(5))
        .child(box().absolute().inset(0)
                   .outline(shapes::polygon(3, 90))
                   .fill(Material::solid(kPaper)));
  }

  Element promptCircle(const char *glyph) {
    return box().width(32).height(32)
        .outline(shapes::squircle(2.0f))
        .fill(SkColor4f{kGroundDark.fR, kGroundDark.fG, kGroundDark.fB, 0.8f})
        .stroke(stroke(3, Fill::color(kPaper)))
        .alignItems(Align::Center).justify(Justify::Center)
        .child(text(toU8(glyph), smallType(14, kPaper, 0)));
  }

  Element describe() {
    return stack()
        .child(backdrop())
        // ---- giant rotated index numeral, behind the menu (§1 chrome) ----
        .child(text(toU8("04"), [] {
                 auto s = menuType(220, kNumeral, 0, false);
                 // §1 says −0.2em (FOT-Rodin); Avenir's digits collide into
                 // a blob there — −0.02em keeps the overlapped-pair legible.
                 s.shaping.letterSpacing = -0.02f * 220;
                 return s;
               }())
                   .centerAt({480, 306}).rotate(90).zIndex(1)
                   .opacity(withFrom(0.0f, 1.0f, {500ms})))
        // ---- the sticker scatter; stagger 33ms BOTTOM-UP: children are
        //      declared bottom-first (zIndex owns paint order) ----
        .child(box().key("menu").left(64).top(70).width(480).height(530)
                   .zIndex(2)
                   .staggerChildren(33ms)
                   .child(plainRow(4))   // SYSTEM (bottom — enters first)
                   .child(selectedRow()) // PERSONA
                   .child(plainRow(2))   // EQUIP
                   .child(plainRow(1))   // ITEM
                   .child(plainRow(0)))  // SKILL
        .child(cursor())
        // ---- right-anchored tooltip title over the COMMAND rule ----
        .child(box().key("tooltip").top(40).right(46).zIndex(8)
                   .alignItems(Align::End)
                   .translateX(withFrom(36.0f, 0.0f, {400ms, &ch::easeOutQuint}))
                   .opacity(withFrom(0.0f, 1.0f, {300ms}))
                   .child(text(toU8("PERSONA"), menuType(30, kPaper, 2))
                              .effect(styles::textGlow({0, 0, 0, 0.5f}, 3)))
                   .child(box().row().alignItems(Align::Center).margin(0, 6, 0, 0)
                              .child(text(toU8("COMMAND"),
                                          smallType(12, kCyanB, 2)))
                              .child(box().width(120).height(2)
                                         .fill(SkColor4f{1, 1, 1, 0.8f})
                                         .margin(8, 0, 0, 0))))
        // ---- button prompts, bottom-right (§1 chrome) ----
        .child(box().key("prompts").right(44).bottom(28).row()
                   .alignItems(Align::Center).zIndex(8)
                   .opacity(withFrom(0.0f, 1.0f,
                                     {400ms, &ch::easeOutQuad, 250ms}))
                   .child(promptCircle("O"))
                   .child(text(toU8("CONFIRM"), smallType(11, kCyanB, 1.5f))
                              .margin(8, 0, 22, 0))
                   .child(promptCircle("X"))
                   .child(text(toU8("BACK"), smallType(11, kCyanB, 1.5f))
                              .margin(8, 0, 0, 0)));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(P3RStudy)
