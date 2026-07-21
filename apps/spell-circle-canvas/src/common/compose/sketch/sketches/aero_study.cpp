// aero_study.cpp — a STUDY of Windows 7 Aero glass (DWM). Round 2:
// exercises the mount-transition / aligned-stroke / textGlow API.
// REFERENCES.md §6 — the recovered colorization formula
//   out.rgb = tint·colorBalance + tint·luma(blur)·afterglowBalance
//             + blur·blurBalance
// approximated as Element::backdrop(blur σ=3, the registry's tight
// blurdeviation 30) + a Material::blend tint stack in the Win7 "Sky"
// accent (#74B8FC α≈42%, balances 8/43/49). Frame anatomy per §6:
// 1px black α.65 silhouette (stroke align Outer on the frame outline),
// 1px white α.55 glass edge inside it (align Inner, same outline),
// client hole ringed 1px black α.35 / 1px white α.45 (one ring box,
// Outer+Inner), top corners r≈6, radial white corner glows (α.35→0
// over ~30px), one diagonal desktop-space sheen (peak α≈.2). Caption
// text = black over the DrawThemeTextEx white haze — styles::textGlow.
// Close-button hover bloom radial rgba(255,196,180,.95)→
// (230,110,90,.9)@35%→(190,25,20,.85)@70%→0. Start orb: radial base
// #163A5F→#0B2340@70%→#04101E, rim strokes, top lens.
// Window-open: .scale(withFrom(.96→1, 220ms)) + .opacity(withFrom(0→1,
// 180ms)) on the window stack — capture mid-open with --at 0.08.
//
// Headless: ComposeSketch <this> --frame aero.png --at 2.5
//           ComposeSketch <this> --frame aero_open.png --at 0.08

#include <sigilsketch/Sketch.h>

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>

#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <chrono>
#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr float W = 960, H = 600;

// Window geometry (desktop space).
constexpr float WX = 170, WY = 80, WW = 620, WH = 420;
constexpr float kCaption = 30; // caption band height
// Client hole (window-local): glass border 9px, caption above.
constexpr float kCL = 9, kCT = kCaption + 1, kCR = 9, kCB = 9;

// Win7 "Sky" accent (registry): #74B8FC.
constexpr SkColor4f kSky{0.455f, 0.722f, 0.988f, 1};

sigil::weave::TextStyle type(float size, SkColor4f color,
                             float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// The aurora wallpaper: deep vertical ground, flowing diagonal light
// bands, fine filaments + speckle stars (high-frequency detail so the
// tight σ=3 glass blur actually READS through the frame).
sk_sp<SkRuntimeEffect> auroraEffect() {
  static const char *kSkSL = R"(
    uniform float uTime;
    uniform float2 uResolution;
    half4 main(float2 p) {
      float2 uv = p / uResolution;
      // deep night ground, faint teal horizon at the bottom
      float3 col = mix(float3(0.006, 0.014, 0.042),
                       float3(0.014, 0.052, 0.096), uv.y);
      col += float3(0.02, 0.10, 0.12) * smoothstep(0.55, 1.05, uv.y);
      // aurora curtains: wide, soft, diagonal (~40 deg), drifting slowly
      float d = uv.x * 0.62 - uv.y * 0.78;
      float t = uTime * 0.09;
      // curtain waviness — bands bend instead of staying ruler-straight
      float wob = 0.045 * sin(uv.x * 5.1 + t * 2.0)
                + 0.030 * sin(uv.y * 7.3 - t * 1.4);
      float b1 = exp(-pow((d - 0.05 + wob) * 3.4, 2.0));
      float b2 = exp(-pow((d + 0.34 - wob * 0.7) * 2.8, 2.0));
      float b3 = exp(-pow((d - 0.46 + wob * 1.3) * 5.0, 2.0));
      // curtains hang in the upper sky
      float sky = 1.0 - smoothstep(0.35, 0.95, uv.y);
      // the main green curtain shifts green→cyan along its run
      float3 c1 = mix(float3(0.05, 0.48, 0.24), float3(0.07, 0.40, 0.46),
                      uv.x);
      col += c1 * b1 * 0.62 * (0.35 + 0.65 * sky);
      col += float3(0.34, 0.24, 0.68) * b2 * 0.40 * (0.45 + 0.55 * sky);
      col += float3(0.06, 0.34, 0.38) * b3 * 0.38 * (0.35 + 0.65 * sky);
      // faint filaments inside the curtains (detail for the glass blur)
      float f = 0.5 + 0.5 * sin(d * 150.0 + wob * 40.0 + uTime * 0.4);
      col += float3(0.35, 0.90, 0.65) * pow(f, 18.0) * b1 * 0.20 * sky;
      float f2 = 0.5 + 0.5 * sin(d * 95.0 - uTime * 0.25 + 1.7);
      col += float3(0.55, 0.50, 0.95) * pow(f2, 24.0) * b2 * 0.16 * sky;
      // sparse small stars, brighter high in the sky
      float2 cell = floor(p / 3.0);
      float h = fract(sin(dot(cell, float2(127.1, 311.7))) * 43758.5453);
      float star = step(0.9982, h);
      col += float3(0.80, 0.88, 1.0) * star *
             (0.12 + 0.45 * fract(h * 91.7)) * (1.0 - uv.y * 0.75);
      col = clamp(col, 0.0, 1.0);
      return half4(half3(col), 1.0);
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx)
      SkDebugf("aurora shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

// The DWM colorization approximated as ONE flattened shader stack over
// the blurred backdrop: Sky tint (colorBalance+afterglow read), a top
// glass sheen, and the diagonal desktop reflection (screen, peak α≈.2).
Material glassTint(float w, float h) {
  return Material::blend({
      // tint·colorBalance — the flat Sky wash
      {Material::solid({kSky.fR, kSky.fG, kSky.fB, 0.54f}),
       SkBlendMode::kSrcOver},
      // afterglow stand-in: brighter accent breathing down from the top
      {Material::linear({0, 0}, {0, h},
                        {{0.00f, {0.62f, 0.82f, 1.00f, 0.38f}},
                         {0.10f, {0.55f, 0.78f, 1.00f, 0.18f}},
                         {0.30f, {0.45f, 0.72f, 0.99f, 0.05f}},
                         {1.00f, {0.45f, 0.72f, 0.99f, 0.12f}}}),
       SkBlendMode::kSrcOver},
      // the desktop-space diagonal sheen (~30°, peak α≈.2)
      {Material::linear({0, h * 0.85f}, {w, h * 0.15f},
                        {{0.00f, {1, 1, 1, 0.00f}},
                         {0.40f, {1, 1, 1, 0.00f}},
                         {0.52f, {1, 1, 1, 0.20f}},
                         {0.66f, {1, 1, 1, 0.03f}},
                         {0.78f, {1, 1, 1, 0.12f}},
                         {0.88f, {1, 1, 1, 0.00f}},
                         {1.00f, {1, 1, 1, 0.00f}}}),
       SkBlendMode::kScreen},
  });
}

// Radial white corner glow, α.35→0 over ~30px, centered on a top corner.
Material cornerGlow(SkPoint center) {
  return Material::radial(center, 34,
                          {{0.0f, {1, 1, 1, 0.35f}},
                           {1.0f, {1, 1, 1, 0.0f}}});
}

// §6 close-button hover bloom.
Material closeBloom(float w, float h) {
  return Material::radial(
      {w * 0.5f, h * 0.42f}, w * 0.60f,
      {{0.00f, {1.000f, 0.769f, 0.706f, 0.95f}},
       {0.35f, {0.902f, 0.431f, 0.353f, 0.90f}},
       {0.70f, {0.745f, 0.098f, 0.078f, 0.85f}},
       {1.00f, {0.60f, 0.05f, 0.04f, 0.0f}}});
}

// The DWM window shadow: a rounded-box SDF falloff painted INSIDE its
// own node bounds. Still a shader (not util::Shadow) on purpose: the
// glass is translucent, so the shadow must be HOLLOW under the window
// (the smoothstep knockout below) or the backdrop blur samples its own
// black core and the whole pane goes murky. util::Shadow has no
// knockout — see the friction note in the study report.
sk_sp<SkRuntimeEffect> windowShadowEffect() {
  static const char *kSkSL = R"(
    uniform float2 uResolution;
    uniform float4 uMargins; // l, t, r, b
    half4 main(float2 p) {
      float2 rectMin = uMargins.xy;
      float2 rectMax = uResolution - uMargins.zw;
      float2 center = (rectMin + rectMax) * 0.5 + float2(0.0, 5.0);
      float2 halfSize = (rectMax - rectMin) * 0.5;
      float2 q = abs(p - center) - halfSize + float2(7.0, 7.0);
      float d = length(max(q, float2(0.0)))
              + min(max(q.x, q.y), 0.0) - 7.0;
      float a = exp(-pow(max(d, 0.0) / 12.0, 1.6)) * 0.55;
      a *= smoothstep(-3.0, 3.0, d); // hollow under the glass
      return half4(0.0, 0.0, 0.0, half(a));
    }
  )";
  static auto effect = [] {
    auto [fx, err] = SkRuntimeEffect::MakeForShader(SkString(kSkSL));
    if (!fx)
      SkDebugf("window shadow shader: %s\n", err.c_str());
    return fx;
  }();
  return effect;
}

// Caption-button glass base (idle): faint vertical white gradient.
Material buttonBase(float h) {
  return Material::linear({0, 0}, {0, h},
                          {{0.00f, {1, 1, 1, 0.28f}},
                           {0.45f, {1, 1, 1, 0.10f}},
                           {0.50f, {1, 1, 1, 0.04f}},
                           {1.00f, {1, 1, 1, 0.12f}}});
}

} // namespace

struct AeroStudy : sketch::Sketch {
  ch::Output<float> bloom{0};    // close-button hover bloom fade-in
  ch::Output<float> orbGlow{0};  // start-orb ambient breathing

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background({0.01f, 0.03f, 0.08f, 1});

    auto &tl = ctx.ticker.timeline();
    bloom = 0.0f;
    tl.apply(&bloom)
        .then<ch::Hold>(0.0f, 0.45f)
        .then<ch::RampTo>(1.0f, 0.10f, &ch::easeOutQuad); // ~100ms in (§6)

    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      orbGlow = 0.55f + 0.25f * (float)std::sin(t * 1.4);
      return true;
    });

    ctx.composer.render(describe());
  }

  // ---- caption buttons -----------------------------------------------
  Element buttonGlyphMinimize() {
    return box().absolute().inset(10, 12, 10, 4)
        .fill(Fill::color({1, 1, 1, 0.95f}))
        .corners({0.5f});
  }
  Element buttonGlyphMaximize() {
    return box().absolute().inset(9, 5, 9, 5)
        .stroke(stroke(1.2f, Fill::color({1, 1, 1, 0.95f})));
  }
  Element buttonGlyphClose(float w, float h) {
    const float cx = w * 0.5f, cy = h * 0.5f;
    auto bar = [&](float deg) {
      return box().absolute()
          .inset(cx - 5.5f, cy - 1.0f, w - cx - 5.5f, h - cy - 1.0f)
          .fill(Fill::color({1, 1, 1, 0.97f}))
          .corners({1})
          .rotate(deg);
    };
    return stack().absolute().inset(0)
        .child(bar(45))
        .child(bar(-45));
  }

  Element captionButton(float w, float h, Corners c, Element glyph,
                        bool hovered) {
    // Inner-aligned edge: the full 1px lands inside the clip (a Center
    // stroke under clip() kept only its inner half).
    auto b = box().width(w).height(h).corners(c).clip()
        .fill(buttonBase(h))
        .stroke(stroke(1, Fill::color({1, 1, 1, 0.30f}),
                       PathFormat::Align::Inner));
    if (hovered)
      b.child(box().absolute().inset(0).corners(c)
                  .fill(closeBloom(w, h))
                  .opacity(&bloom));
    // faint inner top light
    b.child(box().absolute().inset(1, 1, 1, h - 2)
                .fill(Fill::color({1, 1, 1, 0.22f})));
    b.child(std::move(glyph));
    return b;
  }

  Element captionButtons() {
    const float bh = 19;
    const float wMin = 29, wMax = 27, wClose = 47;
    auto seam = [&] { // 1px dark seam between the glass buttons
      return box().width(1).height(bh).fill(Fill::color({0, 0, 0, 0.35f}));
    };
    // Pinned, not stretched: top/right only — the row shrink-wraps.
    return box().row().top(1).right(8)
        .child(captionButton(wMin, bh, {0, 0, 0, 4}, buttonGlyphMinimize(),
                             false))
        .child(seam())
        .child(captionButton(wMax, bh, {0}, buttonGlyphMaximize(), false))
        .child(seam())
        .child(captionButton(wClose, bh, {0, 0, 4, 0},
                             buttonGlyphClose(wClose, bh), true));
  }

  // ---- caption text over the DrawThemeTextEx white haze ---------------
  Element captionText() {
    // §6: black text, white backplate blur σ≈5 α.9 — textGlow re-emits
    // the glyph layer blurred beneath itself; a tight+wide chain gives
    // the dense core with the soft 6–8px falloff.
    return box().absolute().inset(36, 8, 130, WH - kCaption)
        .child(text(toU8("Aurora Borealis — Aero Glass"),
                    type(12.5f, {0.05f, 0.05f, 0.05f, 1}))
                   .absolute().inset(0, 0, 0, 0)
                   .effect(styles::textGlow({1, 1, 1, 0.90f}, 2.2f)
                               .then(styles::textGlow({1, 1, 1, 0.50f},
                                                      4.5f))));
  }

  // ---- the client area (white, so the glass frame reads) --------------
  Element clientArea() {
    auto gray = [](float size, float g, float a = 1.0f) {
      return type(size, {g, g, g, a});
    };
    return box().absolute().inset(kCL, kCT, kCR, kCB)
        .fill(Fill::color({1, 1, 1, 1}))
        .clip()
        // toolbar strip
        .child(box().absolute().inset(0, 0, 0, WH - kCT - kCB - 34)
                   .fill(Material::linear(
                       {0, 0}, {0, 34},
                       {{0.0f, {0.937f, 0.957f, 0.980f, 1}},
                        {1.0f, {0.867f, 0.906f, 0.949f, 1}}})))
        .child(box().absolute().inset(0, 34, 0, WH - kCT - kCB - 35)
                   .fill(Fill::color({0.71f, 0.76f, 0.82f, 1})))
        .child(text(toU8("Organize ▾      Share with ▾      Burn"),
                    gray(12, 0.28f))
                   .absolute().inset(14, 9, 0, 0))
        // left navigation pane
        .child(box().absolute().inset(0, 35, 0, 0).width(150)
                   .fill(Fill::color({0.965f, 0.973f, 0.984f, 1})))
        .child(box().absolute().inset(150, 35, 0, 0).width(1)
                   .fill(Fill::color({0.88f, 0.90f, 0.93f, 1})))
        .child(text(toU8("★ Favorites"), gray(12, 0.25f))
                   .absolute().inset(12, 48, 0, 0))
        .child(text(toU8("Desktop"), gray(12, 0.42f))
                   .absolute().inset(30, 70, 0, 0))
        .child(text(toU8("Downloads"), gray(12, 0.42f))
                   .absolute().inset(30, 90, 0, 0))
        .child(text(toU8("▣ Libraries"), gray(12, 0.25f))
                   .absolute().inset(12, 118, 0, 0))
        .child(text(toU8("Documents"), gray(12, 0.42f))
                   .absolute().inset(30, 140, 0, 0))
        .child(text(toU8("Pictures"), gray(12, 0.42f))
                   .absolute().inset(30, 160, 0, 0))
        // main pane: a selected row + file rows
        .child(box().absolute().inset(162, 50, 12, 0).height(22)
                   .corners({2})
                   .fill(Material::linear(
                       {0, 0}, {0, 22},
                       {{0.0f, {0.86f, 0.92f, 0.98f, 1}},
                        {1.0f, {0.74f, 0.85f, 0.96f, 1}}}))
                   .stroke(stroke(1, Fill::color({0.52f, 0.70f, 0.88f, 1}))))
        .child(text(toU8("aurora_over_tromso.jpg"), gray(12, 0.15f))
                   .absolute().inset(172, 54, 0, 0))
        .child(text(toU8("colorization_formula.txt"), gray(12, 0.35f))
                   .absolute().inset(172, 82, 0, 0))
        .child(text(toU8("blurdeviation_30.reg"), gray(12, 0.35f))
                   .absolute().inset(172, 106, 0, 0))
        .child(text(toU8("sky_74B8FC_balances_8_43_49.theme"),
                    gray(12, 0.35f))
                   .absolute().inset(172, 130, 0, 0));
  }

  // ---- the window ------------------------------------------------------
  Element window() {
    // The clipped glass pane: backdrop blur + colorization, children in
    // window-local coordinates.
    auto glass =
        box().absolute().inset(0).corners({6, 6, 0, 0}).clip()
            // the DWM pass: blur what's behind (σ=3, tight — §6)…
            .backdrop(Effect::filter(SkImageFilters::Blur(3, 3, nullptr)))
            // …then the colorization tint stack over it
            .fill(glassTint(WW, WH))
            // top-corner radial glows
            .child(box().absolute().inset(0, 0, WW - 70, WH - 46)
                       .fill(cornerGlow({0, 0})))
            .child(box().absolute().inset(WW - 70, 0, 0, WH - 46)
                       .fill(cornerGlow({70, 0})))
            // client hole rings on ONE box: 1px black α.35 outside its
            // outline, 1px white α.45 inside it (stroke align does the
            // -2/-1 inset bookkeeping the old version did by hand)
            .child(box().absolute()
                       .inset(kCL - 1, kCT - 1, kCR - 1, kCB - 1)
                       .stroke(stroke(1, Fill::color({0, 0, 0, 0.35f}),
                                      PathFormat::Align::Outer))
                       .stroke(stroke(1, Fill::color({1, 1, 1, 0.45f}),
                                      PathFormat::Align::Inner)))
            .child(clientArea())
            // window icon
            .child(box().absolute().inset(14, 8, WW - 30, WH - 24)
                       .corners({3})
                       .fill(Material::linear(
                           {0, 0}, {0, 16},
                           {{0.0f, {0.55f, 0.80f, 1.0f, 1}},
                            {1.0f, {0.10f, 0.38f, 0.75f, 1}}}))
                       .stroke(stroke(1, Fill::color({1, 1, 1, 0.6f}))))
            .child(captionText())
            .child(captionButtons());

    // The frame wrapper is UNclipped and carries both 1px edges on one
    // outline: black α.65 silhouette Outer, white α.55 glass edge Inner.
    // (clip() clips foreground decorations too, so the Outer stroke must
    // live on this wrapper, not on the clipped glass node.)
    auto frame =
        box().absolute().inset(WX, WY, W - WX - WW, H - WY - WH)
            .corners({6, 6, 0, 0})
            .stroke(stroke(1, Fill::color({0, 0, 0, 0.65f}),
                           PathFormat::Align::Outer))
            .stroke(stroke(1, Fill::color({1, 1, 1, 0.55f}),
                           PathFormat::Align::Inner))
            .child(std::move(glass));

    // The Win7 window-open zoom: shadow + frame scale up from 96% while
    // fading in — mount transitions, so a re-describe prunes clean.
    return stack().absolute().inset(0)
        .transformOrigin((WX + WW * 0.5f) / W, (WY + WH * 0.5f) / H)
        .scale(withFrom(0.96f, 1.0f, {220ms}))
        .opacity(withFrom(0.0f, 1.0f, {180ms}))
        // the DWM soft drop shadow (SDF ring — no filter, no overflow)
        .child(box().absolute()
                   .inset(WX - 34, WY - 30, W - WX - WW - 34,
                          H - WY - WH - 40)
                   .fill(Material::sksl(windowShadowEffect())
                             .uniform("uMargins",
                                      SkColor4f{34, 30, 34, 40})))
        .child(std::move(frame));
  }

  // ---- Start orb + taskbar ---------------------------------------------
  Element startOrb() {
    // 34px sphere, window-local to the taskbar strip.
    const float d = 34;
    return box().absolute().inset(14, 3, 0, 0).width(d).height(d)
        // ambient aqua glow behind the orb (breathing)
        .child(box().absolute().inset(-8)
                   .fill(Material::radial({d / 2 + 8, d / 2 + 8}, d / 2 + 8,
                                          {{0.0f, {0.35f, 0.75f, 1.0f, 0.35f}},
                                           {1.0f, {0.35f, 0.75f, 1.0f, 0}}}))
                   .opacity(&orbGlow))
        .child(
            box().absolute().inset(0).corners({d / 2}).clip()
                // §6 radial base
                .fill(Material::radial(
                    {d * 0.5f, d * 0.42f}, d * 0.62f,
                    {{0.00f, {0.086f, 0.227f, 0.373f, 1}},   // #163A5F
                     {0.70f, {0.043f, 0.137f, 0.251f, 1}},   // #0B2340
                     {1.00f, {0.016f, 0.063f, 0.118f, 1}}})) // #04101E
                // rim strokes
                .stroke(stroke(1.2f, Fill::color({0.55f, 0.78f, 1.0f, 0.55f})))
                // the four-pane flag, gently rotated
                .child(box().absolute()
                           .inset(d / 2 - 8, d / 2 - 7, 0, 0)
                           .width(16).height(14).rotate(-8.0f)
                           .child(box().absolute().inset(0, 0, 8.5f, 7.5f)
                                      .corners({1.5f})
                                      .fill(Fill::color({0.91f, 0.31f, 0.22f, 1})))
                           .child(box().absolute().inset(8.5f, 0, 0, 7.5f)
                                      .corners({1.5f})
                                      .fill(Fill::color({0.50f, 0.76f, 0.24f, 1})))
                           .child(box().absolute().inset(0, 7.5f, 8.5f, 0)
                                      .corners({1.5f})
                                      .fill(Fill::color({0.22f, 0.63f, 0.87f, 1})))
                           .child(box().absolute().inset(8.5f, 7.5f, 0, 0)
                                      .corners({1.5f})
                                      .fill(Fill::color({0.98f, 0.74f, 0.10f, 1}))))
                // top lens
                .child(box().absolute().inset(4, 1.5f, 4, d * 0.52f)
                           .corners({d * 0.36f, d * 0.36f, d * 0.20f,
                                     d * 0.20f})
                           .fill(Material::linear(
                               {0, 0}, {0, d * 0.46f},
                               {{0.0f, {1, 1, 1, 0.55f}},
                                {1.0f, {1, 1, 1, 0.04f}}}))));
  }

  Element taskbar() {
    const float th = 40;
    return box().absolute().inset(0, H - th, 0, 0)
        .backdrop(Effect::filter(SkImageFilters::Blur(3, 3, nullptr)))
        .fill(Material::blend({
            {Material::solid({0.02f, 0.05f, 0.10f, 0.52f}),
             SkBlendMode::kSrcOver},
            {Material::solid({kSky.fR, kSky.fG, kSky.fB, 0.16f}),
             SkBlendMode::kSrcOver},
            {Material::linear({0, 0}, {0, th},
                              {{0.00f, {1, 1, 1, 0.22f}},
                               {0.08f, {1, 1, 1, 0.05f}},
                               {0.55f, {1, 1, 1, 0.00f}},
                               {1.00f, {0, 0, 0, 0.18f}}}),
             SkBlendMode::kSrcOver},
        }))
        // 1px light top edge over a dark seam
        .child(box().absolute().inset(0, 0, 0, th - 1)
                   .fill(Fill::color({1, 1, 1, 0.30f})))
        .child(startOrb())
        // one running-app glass button
        .child(box().absolute().inset(62, 4, 0, 4).width(54)
                   .corners({3})
                   .fill(Material::linear({0, 0}, {0, th - 8},
                                          {{0.0f, {1, 1, 1, 0.26f}},
                                           {0.5f, {1, 1, 1, 0.08f}},
                                           {1.0f, {1, 1, 1, 0.16f}}}))
                   .stroke(stroke(1, Fill::color({1, 1, 1, 0.35f})))
                   .child(box().absolute().inset(19, 9, 0, 0)
                              .width(16).height(13).corners({2})
                              .fill(Material::linear(
                                  {0, 0}, {0, 13},
                                  {{0.0f, {1.0f, 0.87f, 0.55f, 1}},
                                   {1.0f, {0.90f, 0.67f, 0.25f, 1}}}))
                              .stroke(stroke(1, Fill::color(
                                  {0.55f, 0.40f, 0.10f, 0.8f})))))
        // tray clock, pinned to the right edge (right-aligned for free)
        .child(text(toU8("4:20 PM"), type(12, {1, 1, 1, 0.92f}))
                   .top(13).right(10))
        .child(text(toU8("7/20/2026"), type(10, {1, 1, 1, 0.65f}))
                   .top(27).right(10));
  }

  // Desktop icons: white label over a soft dark shadow (the Win7 look).
  Element desktopIcon(float x, float y, Element glyph, const char *label) {
    auto lbl = [&](SkColor4f c) {
      return box().absolute().inset(0, 52, 0, 0).row()
          .justify(Justify::Center)
          .child(text(toU8(label), type(11.5f, c)));
    };
    return box().absolute().inset(x, y, 0, 0).width(92).height(72)
        .child(box().absolute().inset(24, 2, 24, 26).child(std::move(glyph)))
        .child(lbl({0, 0, 0, 0.85f})
                   .effect(Effect::filter(SkImageFilters::Blur(1.6f, 1.6f,
                                                               nullptr))))
        .child(lbl({1, 1, 1, 0.95f}));
  }

  Element folderGlyph() {
    return stack().absolute().inset(0)
        .child(box().absolute().inset(2, 6, 4, 8).corners({2, 2, 3, 3})
                   .fill(Material::linear({0, 0}, {0, 30},
                                          {{0.0f, {1.00f, 0.88f, 0.55f, 1}},
                                           {1.0f, {0.86f, 0.62f, 0.20f, 1}}}))
                   .stroke(stroke(1, Fill::color({0.45f, 0.32f, 0.08f, 0.7f}))))
        .child(box().absolute().inset(2, 2, 22, 34).corners({2, 2, 0, 0})
                   .fill(Fill::color({0.93f, 0.74f, 0.34f, 1})));
  }

  Element binGlyph() {
    return stack().absolute().inset(0)
        .child(box().absolute().inset(8, 10, 8, 4).corners({3, 3, 6, 6})
                   .fill(Material::linear(
                       {0, 0}, {28, 0},
                       {{0.00f, {0.75f, 0.88f, 0.97f, 0.55f}},
                        {0.50f, {0.45f, 0.62f, 0.80f, 0.35f}},
                        {1.00f, {0.75f, 0.88f, 0.97f, 0.55f}}}))
                   .stroke(stroke(1, Fill::color({0.85f, 0.93f, 1.0f, 0.8f}))))
        .child(box().absolute().inset(5, 6, 5, 32).corners({2})
                   .fill(Fill::color({0.60f, 0.76f, 0.90f, 0.7f}))
                   .stroke(stroke(1, Fill::color({0.90f, 0.96f, 1.0f, 0.8f}))));
  }

  Element describe() {
    return stack()
        .fill(Material::sksl(auroraEffect())) // uTime keeps it alive
        .child(desktopIcon(24, 22, binGlyph(), "Recycle Bin"))
        .child(desktopIcon(24, 116, folderGlyph(), "Nightscapes"))
        .child(window())
        .child(taskbar());
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(AeroStudy)
