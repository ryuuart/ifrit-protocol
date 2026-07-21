// y2k_study.cpp — "SIGILNET 2000", the Y2K web-era chrome study.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/y2k_study.cpp
//
// Reference: REFERENCES.md §2 (Y2K web / graphic design 1998–2005) with the
// §3 layer-style presets. Every surface obeys the era's one rule — the
// vertical light model: body ramp with a HARD stop, a white top-highlight
// LENS, light emerging from BELOW (the Aqua bottom glow).
//
//   aqua pills ...... §2 verbatim: pill body ramp
//                     rgba(28,91,155,.82)→rgba(108,191,255,.9)@.9→#7ECBFF,
//                     bottom glow inset 2px fading by 45% (screen), lens rect
//                     x∈[5%,95%] y∈[4%,52%] white .72→0, per-side 1.5px rims
//                     #8BA2C1/#5890BF/#4F93CA/#768FA5, halo dropShadow
//                     rgba(66,140,240,.5) (0,10) blur 16 — blue/red/green
//                     recolors on the same recipe
//   title bar ....... §2 silver chrome ramp with the hard stop at .50‖.505,
//                     1px white top edge, 1px #5A6068 bottom edge
//   chrome type ..... §2 sunset chrome ramp INSIDE the glyphs
//                     (TextStyle.paint.foreground.setShader, ramp mapped to
//                     the glyph height, hard horizon at .495‖.505) + white
//                     horizon glint line + starburst glints (shapes::star)
//   aqua orb ........ layered radial circle: body radial, bottom glow
//                     (screen), top lens — §2 gloss physics on a sphere
//   1998 button ..... §2 plastic bevel: flat web-safe fill + BevelEmboss (§3)
//                     + 1px black keyline
//   ground .......... period page gray with a subtle woven checker Pattern
//
// Headless: ComposeSketch <this> --frame y2k.png --at 1.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <include/core/SkString.h>
#include <include/effects/SkGradient.h>

#include <cmath>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;

namespace {

constexpr float W = 960, H = 600;

/** 0xRRGGBB → SkColor4f. */
constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0,
                             float weight = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  if (weight > 0)
    s.shaping.variations = {sigil::weave::FontVariation("wght", weight)};
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// ---------------------------------------------------------------------------
// §2 sunset chrome — the ramp lives INSIDE the glyphs via a text shader
// mapped to the glyph height (top..baseline of the line box).

sk_sp<SkShader> sunsetShader(float yTop, float yBottom) {
  const SkPoint pts[2] = {{0, yTop}, {0, yBottom}};
  const SkColor4f cols[8] = {C(0xEAF6FF), C(0x9CCFF3), C(0x3C7FC0),
                             C(0x0B2A52), C(0x7A4A1A), C(0xB98A46),
                             C(0xE8CE9A), C(0xFDF6E3)};
  const float stops[8] = {0.0f, 0.12f, 0.35f, 0.495f, 0.505f, 0.62f, 0.82f, 1.0f};
  return SkShaders::LinearGradient(
      pts, SkGradient({{cols, 8}, {stops, 8}, SkTileMode::kClamp}, {}));
}

sigil::weave::TextStyle chromeType(float size) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = 3;
  s.shaping.variations = {sigil::weave::FontVariation("wght", 800)};
  // Map the ramp onto the glyph band (measured off the rendered line box:
  // cap tops ≈0.16·size below the line top, baseline ≈0.80·size) — so the
  // hard horizon lands mid-letter and the cream reaches the glyph feet.
  s.paint.foreground.setShader(sunsetShader(size * 0.16f, size * 0.80f));
  s.paint.foreground.setAntiAlias(true);
  // Grounding shadow behind the chrome (the era set logos on soft drops).
  sigil::weave::PaintLayer drop;
  drop.paint.setColor4f({0.10f, 0.16f, 0.28f, 0.35f}, nullptr);
  drop.paint.setAntiAlias(true);
  drop.paint.setMaskFilter(
      SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.0f));
  drop.offset = {0, 4};
  s.paint.addUnderlay(drop);
  return s;
}

// ---------------------------------------------------------------------------
// §2 aqua pill, parameterized for the blue/red/green recolors.

struct PillTint {
  SkColor4f deep, mid, light, glow, halo;
};

const PillTint kBluePill{{28 / 255.f, 91 / 255.f, 155 / 255.f, 0.82f},
                         {108 / 255.f, 191 / 255.f, 255 / 255.f, 0.90f},
                         C(0x7ECBFF),
                         C(0xB0E5FF),
                         {66 / 255.f, 140 / 255.f, 240 / 255.f, 0.5f}};
const PillTint kRedPill{{158 / 255.f, 34 / 255.f, 44 / 255.f, 0.82f},
                        {255 / 255.f, 128 / 255.f, 122 / 255.f, 0.90f},
                        C(0xFFA6A0),
                        C(0xFFD2C8),
                        {240 / 255.f, 80 / 255.f, 70 / 255.f, 0.5f}};
const PillTint kGreenPill{{26 / 255.f, 122 / 255.f, 58 / 255.f, 0.82f},
                          {126 / 255.f, 226 / 255.f, 146 / 255.f, 0.90f},
                          C(0xA2F0B2),
                          C(0xCEFFDA),
                          {70 / 255.f, 200 / 255.f, 110 / 255.f, 0.5f}};

Element aquaPill(std::string_view label, const PillTint &t, float w = 178,
                 float h = 46) {
  const float r = h / 2; // true pill
  // §2: rim 1.5px per-side #8BA2C1 / #5890BF / #4F93CA / #768FA5.
  auto rim = [](shapes::Edge e, SkColor4f c) {
    return shapes::onEdges(e, util::stroke(1.5f, Fill::color(c)));
  };
  return box().width(w).height(h).corners({r})
      // halo: rgba(66,140,240,.5) offset (0,10) blur 16 — under the fill
      .background(styles::dropShadow(t.halo, {0, 10}, 16))
      // body ramp: deep .82 → mid .9 @0.9 → light
      .fill(Material::linear({0, 0}, {0, h},
                             {{0.0f, t.deep}, {0.9f, t.mid}, {1.0f, t.light}}))
      .foreground(rim(shapes::Edge::Top, C(0x8BA2C1)))
      .foreground(rim(shapes::Edge::Right, C(0x5890BF)))
      .foreground(rim(shapes::Edge::Bottom, C(0x4F93CA)))
      .foreground(rim(shapes::Edge::Left, C(0x768FA5)))
      // bottom glow: inset 2, fades out by 45% up from the bottom, screen
      .child(box().absolute().inset(2, h * 0.55f, 2, 2).corners({r - 2})
                 .fill(Material::linear(
                     {0, h * 0.45f - 4}, {0, 0},
                     {{0.0f, {t.glow.fR, t.glow.fG, t.glow.fB, 0.85f}},
                      {1.0f, {t.glow.fR, t.glow.fG, t.glow.fB, 0.0f}}}))
                 .blend(SkBlendMode::kScreen))
      // the LENS: x∈[5%,95%] y∈[4%,52%], white .72→0, radius ≈ r − inset
      .child(box().absolute()
                 .inset(w * 0.05f, h * 0.04f, w * 0.05f, h * 0.48f)
                 .corners({h * 0.24f})
                 .fill(Material::linear({0, 0}, {0, h * 0.48f},
                                        {{0.0f, {1, 1, 1, 0.72f}},
                                         {1.0f, {1, 1, 1, 0.0f}}})))
      // label, centered, riding above the lens (1px dark ground shadow)
      .child(box().absolute().inset(0).row().justify(Justify::Center)
                 .alignItems(Align::Center).zIndex(1)
                 .child(text(toU8(std::string(label)), [&] {
                   auto s = type(17, {1, 1, 1, 0.98f}, 1.0f, 650);
                   sigil::weave::PaintLayer ground;
                   ground.paint.setColor4f(
                       {t.deep.fR * 0.4f, t.deep.fG * 0.4f, t.deep.fB * 0.4f,
                        0.45f},
                       nullptr);
                   ground.paint.setAntiAlias(true);
                   ground.offset = {0, 1.2f};
                   s.paint.addUnderlay(ground);
                   return s;
                 }())));
}

// ---------------------------------------------------------------------------
// §2 gloss physics on a sphere — the aqua orb.

Element aquaOrb(float d = 118) {
  const float r = d / 2;
  Material body = Material::blend({
      // deep aqua body, light biased low (light from below)
      {Material::radial({r, r * 1.12f}, r * 1.05f,
                        {{0.00f, C(0x9FD8FF)},
                         {0.55f, C(0x2E86DC)},
                         {0.92f, C(0x0B3A70)},
                         {1.00f, C(0x082B54)}}),
       SkBlendMode::kSrcOver},
      // the Aqua signature: glow rising from the bottom rim
      {Material::radial({r, d * 0.92f}, r * 0.85f,
                        {{0.0f, {0.78f, 0.95f, 1.0f, 0.95f}},
                         {0.6f, {0.60f, 0.88f, 1.0f, 0.45f}},
                         {1.0f, {0.60f, 0.88f, 1.0f, 0.0f}}}),
       SkBlendMode::kScreen},
  });
  return box().width(d).height(d).corners({r}).clip()
      .background(styles::dropShadow({66 / 255.f, 140 / 255.f, 240 / 255.f, 0.45f},
                                     {0, 8}, 14))
      .fill(body)
      // top lens dome: x∈[16%,84%], y∈[5%,44%], a full capsule
      .child(box().absolute().inset(d * 0.16f, d * 0.05f, d * 0.16f, d * 0.56f)
                 .corners({d * 0.195f})
                 .fill(Material::linear({0, 0}, {0, d * 0.39f},
                                        {{0.0f, {1, 1, 1, 0.85f}},
                                         {0.7f, {1, 1, 1, 0.18f}},
                                         {1.0f, {1, 1, 1, 0.0f}}})));
}

// ---------------------------------------------------------------------------
// §2 plastic bevel — the 1998 GIF-button, via §3 BevelEmboss.

Element plasticButton(std::string_view label) {
  styles::BevelEmboss bevel;
  bevel.depth = 3;
  bevel.size = 0; // HARD edges: the era's 2px outset is unblurred
  bevel.highlight = {1, 1, 1, 0.88f};
  bevel.shadow = {0, 0, 0, 0.55f};
  return box().width(168).height(38)
      .fill(Fill::color(C(0x336699))) // flat web-safe fill
      .foreground(bevel)
      .stroke(util::stroke(1, Fill::color(C(0x000000)))) // optional keyline
      .row().justify(Justify::Center).alignItems(Align::Center)
      .child(text(toU8(std::string(label)), type(14, C(0xFFFFFF), 0.5f, 600)));
}

/** Tiny window-chrome bevel square (the min/max/close cluster). */
Element chromeSquare(SkColor4f fill) {
  styles::BevelEmboss bevel;
  bevel.depth = 1.5f;
  bevel.size = 0;
  return box().width(15).height(14).fill(Fill::color(fill)).foreground(bevel).stroke(
      util::stroke(1, Fill::color({0.25f, 0.27f, 0.30f, 0.8f})));
}

/** White starburst glint (shapes::star, thin 4-point). */
Element glint(float size, float rotationDeg, float alpha = 0.95f) {
  return box().width(size).height(size)
      .outline(shapes::star(4, 0.10f))
      .fill(Fill::color({1, 1, 1, alpha}))
      .rotate(rotationDeg)
      .zIndex(3);
}

} // namespace

struct Y2kStudy : sketch::Sketch {
  ch::Output<float> fadeType{0}, dropType{18};
  ch::Output<float> fadePills{0}, dropPills{14};
  ch::Output<float> fadeFoot{0};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(C(0xAEB4BC));

    auto &tl = ctx.ticker.timeline();
    auto enter = [&](ch::Output<float> &fade, ch::Output<float> *drop,
                     float from, float delay) {
      fade = 0.0f;
      tl.apply(&fade).then<ch::Hold>(0.0f, delay).then<ch::RampTo>(
          1.0f, 0.5f, &ch::easeOutQuad);
      if (drop) {
        *drop = from;
        tl.apply(drop).then<ch::Hold>(from, delay).then<ch::RampTo>(
            0.0f, 0.55f, &ch::easeOutQuint);
      }
    };
    enter(fadeType, &dropType, 18, 0.08f);
    enter(fadePills, &dropPills, 14, 0.28f);
    enter(fadeFoot, nullptr, 0, 0.45f);

    ctx.composer.render(describe());
  }

  Element describe() {
    // ---- period page ground: gray + subtle woven checker -----------------
    Material weave =
        patterns::checker(3, {0, 0, 0, 0.035f}, {1, 1, 1, 0.05f}).material();

    // ---- §2 silver chrome ramp (hard stop .50 ‖ .505) ---------------------
    const float barH = 36;
    Material silver = Material::linear(
        {0, 0}, {0, barH},
        {{0.00f, C(0xFDFDFD)}, {0.20f, C(0xD2D8DD)}, {0.48f, C(0xA5ADB5)},
         {0.50f, C(0x6F7880)}, {0.505f, C(0xE9ECEF)}, {0.80f, C(0xC6CDD3)},
         {1.00f, C(0x9BA3AC)}});

    Element titleBar =
        box().height(barH).fill(silver)
            .foreground(shapes::onEdges(shapes::Edge::Top,
                                        util::stroke(1, Fill::color(C(0xFFFFFF)))))
            .foreground(shapes::onEdges(shapes::Edge::Bottom,
                                        util::stroke(1, Fill::color(C(0x5A6068)))))
            .row().alignItems(Align::Center).padding(12, 0)
            .gap(5)
            .child(text(toU8("SIGILNET 2000 — hyperportal v4.2"),
                        type(13, C(0x2A2E33), 0.4f, 600)))
            .child(box().grow(1))
            .child(chromeSquare(C(0xD4D0C8)))
            .child(chromeSquare(C(0xD4D0C8)))
            .child(chromeSquare(C(0xC87050)));

    // ---- chrome type block: sunset ramp in the glyphs + glints ------------
    const float typeSize = 96;
    // horizon (ramp .495–.505) in type-box space: lerp of the shader map
    const float horizonY = typeSize * 0.16f +
                           0.5f * (typeSize * 0.80f - typeSize * 0.16f);
    Element chromeWord =
        box().height(typeSize * 1.16f).key("chrome-type")
            .translateY(&dropType).opacity(&fadeType)
            .child(box().absolute().inset(0).row().justify(Justify::Center)
                       .child(text(toU8("MILLENNIUM"), chromeType(typeSize))))
            // the white horizon glint straddling the hard stop
            .child(box().absolute()
                       .inset(W * 0.10f, horizonY - 1.0f, W * 0.10f, 0)
                       .height(2.0f)
                       .fill(Material::linear(
                           {0, 0}, {W * 0.66f, 0},
                           {{0.00f, {1, 1, 1, 0.0f}},
                            {0.12f, {1, 1, 1, 0.9f}},
                            {0.88f, {1, 1, 1, 0.9f}},
                            {1.00f, {1, 1, 1, 0.0f}}}))
                       .blend(SkBlendMode::kPlus).zIndex(2))
            // starburst glints riding the specular horizon + a cap top
            .child(box().absolute().inset(W * 0.115f, horizonY - 15, 0, 0)
                       .child(glint(30, 0)))
            .child(box().absolute().inset(0, horizonY - 11, W * 0.116f, 0)
                       .row().justify(Justify::End)
                       .child(glint(22, 18, 0.9f)))
            .child(box().absolute().inset(W * 0.318f, 2, 0, 0)
                       .child(glint(16, 12, 0.85f)));

    // ---- assembly ---------------------------------------------------------
    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, C(0xB9BFC7)}, {1.0f, C(0xA2A8B1)}}))
        .child(box().absolute().inset(0).fill(weave))
        // the window
        .child(
            box().absolute().inset(24, 20, 24, 20).column()
                .background(styles::dropShadow({0, 0, 0, 0.38f}, {0, 7}, 18))
                .fill(Fill::color(C(0xE9EBEE)))
                .corners({5}).clip()
                .stroke(util::stroke(1, Fill::color(C(0x70777E))))
                .child(titleBar)
                .child(
                    box().column().grow(1).padding(30, 18)
                        .child(box().grow(0.55f))
                        .child(chromeWord)
                        // tagline under the horizon
                        .child(box().row().justify(Justify::Center)
                                   .margin(0, 2, 0, 0).opacity(&fadeType)
                                   .child(text(
                                       toU8("· t h e   f u t u r e   i s   c h r o m e ·"),
                                       type(13, C(0x66707B), 2.5f))))
                        // the aqua pill rack
                        .child(box().row().justify(Justify::Center).gap(30)
                                   .margin(0, 26, 0, 0).key("pills")
                                   .translateY(&dropPills).opacity(&fadePills)
                                   .child(aquaPill("ENTER  PORTAL", kBluePill))
                                   .child(aquaPill("HOT  LINKS", kRedPill))
                                   .child(aquaPill("GUESTBOOK", kGreenPill)))
                        .child(box().grow(1))
                        // 3D groove rule — the <hr> of the period
                        .child(box().height(2).margin(4, 0, 4, 14)
                                   .opacity(&fadeFoot)
                                   .fill(Material::linear(
                                       {0, 0}, {0, 2},
                                       {{0.0f, C(0x8F969D)}, {0.5f, C(0x8F969D)},
                                        {0.501f, C(0xFFFFFF)}, {1.0f, C(0xFFFFFF)}})))
                        // footer: orb · caption · 1998 plastic button
                        .child(box().row().alignItems(Align::End)
                                   .opacity(&fadeFoot).key("footer")
                                   .child(aquaOrb())
                                   .child(box().column().margin(18, 0, 0, 6)
                                              .gap(4)
                                              .child(text(toU8("now streaming @ 56k"),
                                                          type(13, C(0x4E5760), 0.6f, 600)))
                                              .child(text(toU8("© 2000 sigilnet industries — best viewed at 800×600"),
                                                          type(11, C(0x7A828B), 0.4f))))
                                   .child(box().grow(1))
                                   .child(box().column().alignItems(Align::End)
                                              .gap(6).margin(0, 0, 0, 4)
                                              .child(plasticButton("ENTER SITE >>"))
                                              .child(text(toU8("[ no frames · spacer.gif free ]"),
                                                          type(10, C(0x8A929B), 0.4f)))))));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Y2kStudy)
