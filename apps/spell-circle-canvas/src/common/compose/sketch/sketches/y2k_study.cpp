// y2k_study.cpp — "SIGILNET 2000", the Y2K web-era chrome study. ROUND 2:
// the preset API exercise.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/y2k_study.cpp
//
// Reference: REFERENCES.md §2 (Y2K web / graphic design 1998–2005) and §3
// (Photoshop layer styles + the "Y2K Chrome" / "Aqua Gel" presets). Round 1
// hand-built every surface from the §2 recipes; this round swaps the aqua
// pills, the orb, the chrome bar and the wordmark onto the NEW preset
// bundles and keeps ONE hand-built pill as the A/B reference.
//
//   pill rack ....... styles::aquaGel(tint) on bare pills (blue/red/green
//                     recolors of one preset)
//   A/B card ........ §2-verbatim hand-built pill NEXT TO the aquaGel()
//                     preset at identical geometry — the difference panel
//   title bar ....... styles::y2kChrome() on the bar box
//   wordmark ........ styles::y2kChrome() PLATE + white type; the §2
//                     specular sliver + starburst glints garnish the
//                     horizon (the preset does not include them)
//   tagline ......... styles::textGlow chained .then() — the double
//                     frutiger-aero glow
//   aqua orb ........ styles::aquaGel() on a circle (gloss physics on a
//                     sphere, preset edition)
//   1998 button ..... §2 plastic bevel: flat web-safe fill + BevelEmboss
//                     (§3) + 1px black keyline (not covered by a preset)
//   status bar ...... util::marquee with a constant-phase Output (static
//                     capture shows the strip mid-scroll)
//   entrances ....... withFrom() mount transitions (no hand-wired
//                     timeline Outputs this round)
//
// Headless: ComposeSketch <this> --frame y2k.png --at 2.0

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
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr float W = 1280, H = 800;

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

/** Label riding a gel surface: white with a 1px tint-derived ground. */
sigil::weave::TextStyle gelLabel(SkColor4f tint, float size = 17) {
  auto s = type(size, {1, 1, 1, 0.98f}, 1.0f, 650);
  sigil::weave::PaintLayer ground;
  ground.paint.setColor4f(
      {tint.fR * 0.30f, tint.fG * 0.30f, tint.fB * 0.30f, 0.5f}, nullptr);
  ground.paint.setAntiAlias(true);
  ground.offset = {0, 1.2f};
  s.paint.addUnderlay(ground);
  return s;
}

// ---------------------------------------------------------------------------
// PRESET pill / orb — the whole recipe is one style() call.

Element gelPill(std::string_view label, SkColor4f tint, float w = 178,
                float h = 46) {
  return box().width(w).height(h).corners({h / 2})
      .style(styles::aquaGel(tint)) // body + gloss + hairline, no .fill()
      .row().justify(Justify::Center).alignItems(Align::Center)
      .child(text(toU8(std::string(label)), gelLabel(tint)));
}

Element gelOrb(float d = 120) {
  return box().width(d).height(d).corners({d / 2})
      .style(styles::aquaGel(C(0x1E8FFF))).clip()
      // §2 light-from-below, which the preset's inner lift undersells on a
      // sphere: a screen-blended bottom rim glow (child rides under the
      // preset's over-layer gloss, so the lens stays on top).
      .child(box().absolute()
                 .inset(d * 0.14f, d * 0.50f, d * 0.14f, d * 0.02f)
                 .corners({d * 0.24f})
                 .fill(Material::radial({d * 0.36f, d * 0.55f}, d * 0.52f,
                                        {{0.00f, {0.72f, 0.92f, 1.0f, 0.90f}},
                                         {0.60f, {0.55f, 0.85f, 1.0f, 0.35f}},
                                         {1.00f, {0.55f, 0.85f, 1.0f, 0.0f}}}))
                 .blend(SkBlendMode::kScreen));
}

// ---------------------------------------------------------------------------
// HAND-BUILT §2 pill (round 1, verbatim) — kept as the A/B reference.

struct PillTint {
  SkColor4f deep, mid, light, glow, halo;
};

const PillTint kBluePill{{28 / 255.f, 91 / 255.f, 155 / 255.f, 0.82f},
                         {108 / 255.f, 191 / 255.f, 255 / 255.f, 0.90f},
                         C(0x7ECBFF),
                         C(0xB0E5FF),
                         {66 / 255.f, 140 / 255.f, 240 / 255.f, 0.5f}};

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
      // label, centered, riding above the lens
      .child(box().absolute().inset(0).row().justify(Justify::Center)
                 .alignItems(Align::Center).zIndex(1)
                 .child(text(toU8(std::string(label)),
                             gelLabel({t.deep.fR * 1.3f, t.deep.fG * 1.3f,
                                       t.deep.fB * 1.3f, 1}))));
}

// ---------------------------------------------------------------------------
// §2 plastic bevel — the 1998 GIF-button, via §3 BevelEmboss (no preset
// covers this one; it stays the styles-primitives build).

Element plasticButton(std::string_view label) {
  styles::BevelEmboss bevel;
  bevel.depth = 3;
  bevel.size = 0; // HARD edges: the era's 2px outset is unblurred
  bevel.highlight = {1, 1, 1, 0.88f};
  bevel.shadow = {0, 0, 0, 0.55f};
  return box().width(168).height(38)
      .fill(Fill::color(C(0x336699))) // flat web-safe fill
      .foreground(bevel)
      .stroke(util::stroke(1, Fill::color(C(0x000000)))) // keyline
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

/** Small caption under the A/B specimens. */
Element caption(std::string_view s) {
  return text(toU8(std::string(s)), type(10, C(0x59626C), 0.8f, 600));
}

} // namespace

struct Y2kStudy : sketch::Sketch {
  // marquee phase: constant — the capture shows the strip mid-scroll; a
  // live host would step this over [-(w+gap), 0] (w from compose::measure,
  // which the sketch surface cannot reach — no FontContext exposed).
  ch::Output<float> tickerPhase{-172.0f};

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(C(0xAEB4BC));
    ctx.composer.render(describe());
  }

  Element describe() {
    // ---- period page ground: gray + subtle woven checker -----------------
    Material weave =
        patterns::checker(3, {0, 0, 0, 0.035f}, {1, 1, 1, 0.05f}).material();

    // ---- title bar: the y2kChrome() PRESET on the bar box -----------------
    Element titleBar =
        box().height(40)
            .style(styles::y2kChrome()) // ramp + bevel + keyline + shadow
            .row().alignItems(Align::Center).padding(14, 0).gap(5)
            .child(text(toU8("SIGILNET 2000 — hyperportal v4.2"),
                        [] {
                          auto s = type(13, C(0xF2F6FA), 0.4f, 600);
                          sigil::weave::PaintLayer ground;
                          ground.paint.setColor4f({0, 0.04f, 0.10f, 0.6f},
                                                  nullptr);
                          ground.paint.setAntiAlias(true);
                          ground.offset = {0, 1.2f};
                          s.paint.addUnderlay(ground);
                          return s;
                        }()))
            .child(box().grow(1))
            .child(chromeSquare(C(0xD4D0C8)))
            .child(chromeSquare(C(0xD4D0C8)))
            .child(chromeSquare(C(0xC87050)));

    // ---- wordmark: the y2kChrome() PRESET as a plate ----------------------
    // Fixed height so the §3 hard horizon (49/51%) lands at a known y for
    // the sliver + glints (the preset itself ships no specular garnish).
    const float plateH = 124;
    const float horizonY = plateH * 0.50f;
    Element plate =
        box().height(plateH).corners({10})
            .style(styles::y2kChrome())
            .row().justify(Justify::Center).alignItems(Align::Center)
            .padding(46, 0)
            .child(text(toU8("MILLENNIUM"), [] {
              auto s = type(74, {1, 1, 1, 0.97f}, 3, 800);
              sigil::weave::PaintLayer ground;
              ground.paint.setColor4f({0.02f, 0.05f, 0.09f, 0.55f}, nullptr);
              ground.paint.setAntiAlias(true);
              ground.paint.setMaskFilter(
                  SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 1.6f));
              ground.offset = {0, 3};
              s.paint.addUnderlay(ground);
              return s;
            }()));

    Element wordmark =
        box().key("wordmark")
            .translateY(withFrom(18.0f, 0.0f, {550ms, &ch::easeOutQuint}))
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(plate)
            // §2: white specular sliver straddling the hard horizon
            .child(box().absolute().inset(26, horizonY - 1.0f, 26, 0)
                       .height(2.0f)
                       .fill(Material::linear(
                           {0, 0}, {500, 0},
                           {{0.00f, {1, 1, 1, 0.0f}},
                            {0.12f, {1, 1, 1, 0.85f}},
                            {0.88f, {1, 1, 1, 0.85f}},
                            {1.00f, {1, 1, 1, 0.0f}}}))
                       .blend(SkBlendMode::kPlus).zIndex(2))
            // starburst glints riding the horizon + one cap top
            .child(box().absolute().inset(30, horizonY - 15, 0, 0)
                       .child(glint(30, 0)))
            .child(box().absolute().inset(0, horizonY - 11, 34, 0)
                       .row().justify(Justify::End)
                       .child(glint(22, 18, 0.9f)))
            .child(box().absolute().inset(0, 6, 148, 0)
                       .row().justify(Justify::End)
                       .child(glint(16, 12, 0.85f)));

    // ---- tagline: styles::textGlow, chained for the hotter double glow ----
    // Aero glow on a LIGHT ground: saturated aero-blue core, white-hot
    // tight glow, wide cyan bloom — the chained double textGlow.
    Element tagline =
        box().row().justify(Justify::Center).margin(0, 15, 0, 0)
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(text(toU8("· t h e   f u t u r e   i s   c h r o m e ·"),
                        type(16, C(0x0F86C8), 2.5f, 650))
                       .effect(styles::textGlow({1.0f, 1.0f, 1.0f, 0.95f}, 2)
                                   .then(styles::textGlow(
                                       {0.36f, 0.80f, 1.0f, 0.9f}, 8))));

    // ---- pill rack: three aquaGel() recolors ------------------------------
    Element pills =
        box().row().justify(Justify::Center).gap(30).margin(0, 30, 0, 0)
            .key("pills")
            .translateY(withFrom(14.0f, 0.0f, {550ms, &ch::easeOutQuint}))
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(gelPill("ENTER  PORTAL", C(0x1E8FFF), 186, 48))
            .child(gelPill("HOT  LINKS", C(0xE03A3A), 186, 48))
            .child(gelPill("GUESTBOOK", C(0x2AA84F), 186, 48));

    // ---- the A/B card: hand-built §2 recipe vs the preset -----------------
    Element abCard =
        box().row().justify(Justify::Center).margin(0, 26, 0, 0)
            .opacity(withFrom(0.0f, 1.0f, {500ms}))
            .child(
                box().row().gap(40).padding(22, 13).corners({8})
                    .fill(Fill::color({1, 1, 1, 0.30f}))
                    .stroke(util::stroke(1, Fill::color(C(0x9AA1A9))))
                    .child(box().column().alignItems(Align::Center).gap(7)
                               .child(aquaPill("AQUA  2000", kBluePill))
                               .child(caption("HAND-BUILT · §2 VERBATIM")))
                    .child(box().column().alignItems(Align::Center).gap(7)
                               .child(gelPill("AQUA  2000", C(0x1E8FFF)))
                               .child(caption("PRESET · styles::aquaGel()"))));

    // ---- status bar: util::marquee, constant phase ------------------------
    Element strip = util::marquee(
        text(toU8("· WELCOME TO SIGILNET 2000 · Y2K COMPLIANT · BEST VIEWED "
                  "AT 800×600 · SIGN THE GUESTBOOK · NO FRAMES "),
             type(11, C(0x39424C), 1.0f, 550)),
        &tickerPhase, 48);
    strip.grow(1);
    Element statusBar =
        box().height(24).fill(Fill::color(C(0xD9DDE1)))
            .foreground(shapes::onEdges(shapes::Edge::Top,
                                        util::stroke(1, Fill::color(C(0xFFFFFF)))))
            .background(shapes::onEdges(shapes::Edge::Top,
                                        util::stroke(1, Fill::color(C(0x8F969D)))))
            .row().alignItems(Align::Center).padding(12, 0).gap(10)
            .child(strip)
            .child(box().width(1).height(12).fill(Fill::color(C(0xA6ADB4))))
            .child(text(toU8("56K"), type(10, C(0x6A737D), 1.0f, 700)));

    // ---- assembly ---------------------------------------------------------
    return stack()
        .fill(Material::linear({0, 0}, {0, H},
                               {{0.0f, C(0xB9BFC7)}, {1.0f, C(0xA2A8B1)}}))
        .child(box().absolute().inset(0).fill(weave))
        // the window
        .child(
            box().absolute().inset(28, 22, 28, 22).column()
                .background(styles::dropShadow({0, 0, 0, 0.38f}, {0, 7}, 18))
                .fill(Fill::color(C(0xE9EBEE)))
                .corners({6}).clip()
                .stroke(util::stroke(1, Fill::color(C(0x70777E))))
                .child(titleBar)
                .child(
                    box().column().grow(1).padding(44, 20)
                        .child(box().grow(0.55f))
                        .child(box().row().justify(Justify::Center)
                                   .child(wordmark))
                        .child(tagline)
                        .child(pills)
                        .child(abCard)
                        .child(box().grow(1))
                        // 3D groove rule — the <hr> of the period
                        .child(box().height(2).margin(4, 0, 4, 14)
                                   .opacity(withFrom(0.0f, 1.0f, {500ms}))
                                   .fill(Material::linear(
                                       {0, 0}, {0, 2},
                                       {{0.0f, C(0x8F969D)}, {0.5f, C(0x8F969D)},
                                        {0.501f, C(0xFFFFFF)}, {1.0f, C(0xFFFFFF)}})))
                        // footer: preset orb · caption · 1998 plastic button
                        .child(box().row().alignItems(Align::End).key("footer")
                                   .opacity(withFrom(0.0f, 1.0f, {500ms}))
                                   .child(gelOrb())
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
                                                          type(10, C(0x8A929B), 0.4f)))))
                .child(statusBar)));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(Y2kStudy)
