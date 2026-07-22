#pragma once
// The Y2K web-era chrome card (REFERENCES.md sections 2 + 3 - Y2K web /
// graphic design 1998-2005 and the Photoshop layer-style presets), ported
// from sketch/sketches/y2k_study.cpp round 2: the preset API exercise.
//
//   pill rack ....... styles::aquaGel(tint) on bare pills (blue/red/green
//                     recolors of one preset)
//   A/B card ........ section-2-verbatim hand-built pill NEXT TO the
//                     aquaGel() preset at identical geometry
//   title bar ....... styles::y2kChrome() on the bar box
//   wordmark ........ styles::y2kChrome() PLATE + white type; specular
//                     sliver + starburst glints garnish the horizon
//   tagline ......... styles::textGlow chained .then() - the double
//                     frutiger-aero glow
//   aqua orb ........ styles::aquaGel() on a circle
//   1998 button ..... plastic bevel: flat web-safe fill + BevelEmboss +
//                     1px black keyline (no preset covers this one)
//   status bar ...... util::marquee, LIVE here: measured content width,
//                     ticker-driven wrapping phase (the sketch could not
//                     reach a FontContext and froze the phase)

#include "GalleryCore.h"

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkMaskFilter.h>

#include <cmath>
#include <string>
#include <string_view>

namespace compose_gallery {

namespace y2k_chrome {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kWindowX = 20, kWindowY = 16; // window inset in the scene
constexpr float kTitleBarH = 34;
constexpr float kPlateH = 96;   // the wordmark chrome plate
constexpr float kPillW = 158, kPillH = 42;
constexpr float kOrbD = 84;
constexpr float kStatusH = 22;
constexpr float kTickerSpeed = 70; // px/s, the lazy 56k crawl
constexpr float kTickerGap = 40;   // between the two marquee copies

/** 0xRRGGBB -> SkColor4f. */
constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f,
          a};
}

inline sigil::weave::TextStyle type(float size, SkColor4f color,
                                    float tracking = 0, float weight = 0) {
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
inline sigil::weave::TextStyle gelLabel(SkColor4f tint, float size = 16) {
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
// PRESET pill / orb - the whole recipe is one style() call.

inline Element gelPill(std::string_view label, SkColor4f tint,
                       float w = kPillW, float h = kPillH) {
  return box().width(w).height(h).corners({h / 2})
      .style(styles::aquaGel(tint)) // body + gloss + hairline, no .fill()
      .row().justify(Justify::Center).alignItems(Align::Center)
      .child(text(toU8(std::string(label)), gelLabel(tint)));
}

inline Element gelOrb(float d = kOrbD) {
  return box().width(d).height(d).corners({d / 2})
      .style(styles::aquaGel(C(0x1E8FFF))).clip()
      // The PS Gloss Contour proper (styles::gloss — blurred coverage
      // through a ring table): a shape-following light band the preset's
      // axis-aligned lens can't produce on a sphere.
      .foreground(styles::gloss({0.85f, 0.95f, 1.0f, 0.5f}, d * 0.09f,
                                {0, -d * 0.06f}, 0.48f, 0.30f))
      // light-from-below, which the preset's inner lift undersells on a
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
// HAND-BUILT section-2 pill (round 1, verbatim) - the A/B reference.

struct PillTint {
  SkColor4f deep, mid, light, glow, halo;
};

inline constexpr PillTint kBluePill{{28 / 255.f, 91 / 255.f, 155 / 255.f, 0.82f},
                                    {108 / 255.f, 191 / 255.f, 255 / 255.f, 0.90f},
                                    C(0x7ECBFF),
                                    C(0xB0E5FF),
                                    {66 / 255.f, 140 / 255.f, 240 / 255.f, 0.5f}};

inline Element aquaPill(std::string_view label, const PillTint &t,
                        float w = kPillW, float h = kPillH) {
  const float r = h / 2; // true pill
  // rim 1.5px per-side #8BA2C1 / #5890BF / #4F93CA / #768FA5.
  auto rim = [](shapes::Edge e, SkColor4f c) {
    return shapes::onEdges(e, util::stroke(1.5f, Fill::color(c)));
  };
  return box().width(w).height(h).corners({r})
      // halo: rgba(66,140,240,.5) offset (0,10) blur 16 - under the fill
      .background(styles::dropShadow(t.halo, {0, 10}, 16))
      // body ramp: deep .82 -> mid .9 @0.9 -> light
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
      // the LENS: x in [5%,95%] y in [4%,52%], white .72->0
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
// Plastic bevel - the 1998 GIF-button, via BevelEmboss (no preset covers
// this one; it stays the styles-primitives build).

inline Element plasticButton(std::string_view label) {
  styles::BevelEmboss bevel;
  bevel.depth = 3;
  bevel.size = 0; // HARD edges: the era's 2px outset is unblurred
  bevel.highlight = {1, 1, 1, 0.88f};
  bevel.shadow = {0, 0, 0, 0.55f};
  return box().width(150).height(34)
      .fill(Fill::color(C(0x336699))) // flat web-safe fill
      .foreground(bevel)
      .stroke(util::stroke(1, Fill::color(C(0x000000)))) // keyline
      .row().justify(Justify::Center).alignItems(Align::Center)
      .child(text(toU8(std::string(label)), type(13, C(0xFFFFFF), 0.5f, 600)));
}

/** Tiny window-chrome bevel square (the min/max/close cluster). */
inline Element chromeSquare(SkColor4f fill) {
  styles::BevelEmboss bevel;
  bevel.depth = 1.5f;
  bevel.size = 0;
  return box().width(14).height(13).fill(Fill::color(fill))
      .foreground(bevel)
      .stroke(util::stroke(1, Fill::color({0.25f, 0.27f, 0.30f, 0.8f})));
}

/** White starburst glint (shapes::star, thin 4-point). */
inline Element glint(float size, float rotationDeg, float alpha = 0.95f) {
  return box().width(size).height(size)
      .outline(shapes::star(4, 0.10f))
      .fill(Fill::color({1, 1, 1, alpha}))
      .rotate(rotationDeg)
      .zIndex(3);
}

/** Small caption under the A/B specimens. */
inline Element caption(std::string_view s) {
  return text(toU8(std::string(s)), type(10, C(0x59626C), 0.8f, 600));
}

} // namespace y2k_chrome

struct Y2kChromeScene final : Scene {
  choreograph::Output<float> tickX{0};
  float unitW = 0;   // strip content's intrinsic width (compose::measure)
  float wrapLen = 1; // marquee wrap length = unitW + gap

  const char *name() const override { return "y2k chrome"; }

  void setup(Composer &composer, sigil::motion::Ticker &ticker) override {
    namespace yc = y2k_chrome;
    tickX = 0;

    const SkSize unit = sigil::compose::measure(stripContent(), fonts());
    unitW = std::ceil(unit.width());
    wrapLen = unitW + yc::kTickerGap;

    ticker.add([this, t = 0.0](double dt) mutable {
      namespace yc = y2k_chrome;
      t += dt;
      tickX = -(float)std::fmod(t * yc::kTickerSpeed, (double)wrapLen);
      return true;
    });

    composer.render(describe());
  }

  Element stripContent() {
    namespace yc = y2k_chrome;
    const char *unit = "\xc2\xb7 WELCOME TO SIGILNET 2000 \xc2\xb7 "
                       "Y2K COMPLIANT \xc2\xb7 BEST VIEWED AT 800\xc3\x97"
                       "600 \xc2\xb7 SIGN THE GUESTBOOK \xc2\xb7 NO FRAMES ";
    Element content =
        box().row().alignItems(Align::Center).height(Dim(yc::kStatusH))
            .child(text(toU8(unit), yc::type(11, yc::C(0x39424C), 1.0f, 550))
                       .shrink(0));
    if (unitW > 0)
      content.width(Dim(unitW)).shrink(0);
    return content;
  }

  Element describe() {
    namespace yc = y2k_chrome;
    namespace ch = choreograph;
    using namespace std::chrono_literals;

    // ---- period page ground: gray + subtle woven checker -----------------
    Material weave =
        patterns::checker(3, {0, 0, 0, 0.035f}, {1, 1, 1, 0.05f}).material();

    // ---- title bar: the y2kChrome() PRESET on the bar box -----------------
    Element titleBar =
        box().height(yc::kTitleBarH)
            .style(styles::y2kChrome()) // ramp + bevel + keyline + shadow
            .row().alignItems(Align::Center).padding(12, 0).gap(5)
            .child(text(toU8("SIGILNET 2000 \xe2\x80\x94 hyperportal v4.2"),
                        [] {
                          auto s = yc::type(12, yc::C(0xF2F6FA), 0.4f, 600);
                          sigil::weave::PaintLayer ground;
                          ground.paint.setColor4f({0, 0.04f, 0.10f, 0.6f},
                                                  nullptr);
                          ground.paint.setAntiAlias(true);
                          ground.offset = {0, 1.2f};
                          s.paint.addUnderlay(ground);
                          return s;
                        }()))
            .child(box().grow(1))
            .child(yc::chromeSquare(yc::C(0xD4D0C8)))
            .child(yc::chromeSquare(yc::C(0xD4D0C8)))
            .child(yc::chromeSquare(yc::C(0xC87050)));

    // ---- wordmark: the y2kChrome() PRESET as a plate ----------------------
    // Fixed height so the hard horizon (49/51%) lands at a known y for the
    // sliver + glints (the preset ships no starburst garnish).
    const float plateH = yc::kPlateH;
    const float horizonY = plateH * 0.50f;
    Element plate =
        box().height(plateH).corners({10})
            .style(styles::y2kChrome())
            .row().justify(Justify::Center).alignItems(Align::Center)
            .padding(34, 0)
            .child(text(toU8("MILLENNIUM"), [] {
              namespace yc = y2k_chrome;
              auto s = yc::type(54, {1, 1, 1, 0.97f}, 3, 800);
              sigil::weave::PaintLayer ground;
              ground.paint.setColor4f({0.02f, 0.05f, 0.09f, 0.55f}, nullptr);
              ground.paint.setAntiAlias(true);
              ground.paint.setMaskFilter(
                  SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 1.6f));
              ground.offset = {0, 2.4f};
              s.paint.addUnderlay(ground);
              return s;
            }()));

    Element wordmark =
        box().key("wordmark")
            .translateY(withFrom(14.0f, 0.0f, {550ms, &ch::easeOutQuint}))
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(plate)
            // white specular sliver straddling the hard horizon
            .child(box().absolute().inset(20, horizonY - 1.0f, 20, 0)
                       .height(2.0f)
                       .fill(Material::linear(
                           {0, 0}, {380, 0},
                           {{0.00f, {1, 1, 1, 0.0f}},
                            {0.12f, {1, 1, 1, 0.85f}},
                            {0.88f, {1, 1, 1, 0.85f}},
                            {1.00f, {1, 1, 1, 0.0f}}}))
                       .blend(SkBlendMode::kPlus).zIndex(2))
            // starburst glints riding the horizon + one cap top
            .child(box().absolute().inset(22, horizonY - 12, 0, 0)
                       .child(yc::glint(24, 0)))
            .child(box().absolute().inset(0, horizonY - 9, 26, 0)
                       .row().justify(Justify::End)
                       .child(yc::glint(18, 18, 0.9f)))
            .child(box().absolute().inset(0, 4, 110, 0)
                       .row().justify(Justify::End)
                       .child(yc::glint(13, 12, 0.85f)));

    // ---- tagline: styles::textGlow, chained for the hotter double glow ----
    // Aero glow on a LIGHT ground: saturated aero-blue core, white-hot
    // tight glow, wide cyan bloom - the chained double textGlow.
    Element tagline =
        // Compensated padding preserves the original y/height while giving
        // the cached raster 24px of transparent room for its 7px glow.
        box().row().justify(Justify::Center).padding(0, 24)
            .margin(0, -12, 0, -24)
            .cache(Cache::Texture)
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(text(toU8("\xc2\xb7 t h e   f u t u r e   i s   "
                             "c h r o m e \xc2\xb7"),
                        yc::type(14, yc::C(0x0F86C8), 2.5f, 650))
                       .effect(styles::textGlow({1.0f, 1.0f, 1.0f, 0.95f}, 2)
                                   .then(styles::textGlow(
                                       {0.36f, 0.80f, 1.0f, 0.9f}, 7))));

    // ---- pill rack: three aquaGel() recolors ------------------------------
    Element pills =
        box().row().justify(Justify::Center).gap(22).margin(0, 18, 0, 0)
            .key("pills")
            .translateY(withFrom(12.0f, 0.0f, {550ms, &ch::easeOutQuint}))
            .opacity(withFrom(0.0f, 1.0f, {400ms}))
            .child(yc::gelPill("ENTER  PORTAL", yc::C(0x1E8FFF)))
            .child(yc::gelPill("HOT  LINKS", yc::C(0xE03A3A)))
            .child(yc::gelPill("GUESTBOOK", yc::C(0x2AA84F)));

    // ---- the A/B card: hand-built recipe vs the preset --------------------
    Element abCard =
        box().row().justify(Justify::Center).margin(0, 16, 0, 0)
            .opacity(withFrom(0.0f, 1.0f, {500ms}))
            .child(
                box().row().gap(28).padding(16, 10).corners({8})
                    .fill(Fill::color({1, 1, 1, 0.30f}))
                    .stroke(util::stroke(1, Fill::color(yc::C(0x9AA1A9))))
                    .child(box().column().alignItems(Align::Center).gap(6)
                               .child(yc::aquaPill("AQUA  2000", yc::kBluePill))
                               .child(yc::caption("HAND-BUILT \xc2\xb7 \xc2\xa7"
                                                  "2 VERBATIM")))
                    .child(box().column().alignItems(Align::Center).gap(6)
                               .child(yc::gelPill("AQUA  2000",
                                                  yc::C(0x1E8FFF)))
                               .child(yc::caption(
                                   "PRESET \xc2\xb7 styles::aquaGel()"))));

    // ---- status bar: util::marquee, ticker-driven phase -------------------
    Element strip = util::marquee(stripContent(), &tickX, yc::kTickerGap);
    strip.grow(1);
    Element statusBar =
        box().height(yc::kStatusH).fill(Fill::color(yc::C(0xD9DDE1)))
            .foreground(shapes::onEdges(
                shapes::Edge::Top, util::stroke(1, Fill::color(yc::C(0xFFFFFF)))))
            .background(shapes::onEdges(
                shapes::Edge::Top, util::stroke(1, Fill::color(yc::C(0x8F969D)))))
            .row().alignItems(Align::Center).padding(10, 0).gap(8)
            .child(strip)
            .child(box().width(1).height(11).fill(Fill::color(yc::C(0xA6ADB4))))
            .child(text(toU8("56K"), yc::type(10, yc::C(0x6A737D), 1.0f, 700)));

    // The window's large blurred shadow is static, but the old combined node
    // inherited volatility from the marquee. Split only its backplate so the
    // filter bakes once; the identically rounded live content still clips and
    // keeps the border in its original foreground paint order.
    Element windowBackplate =
        box().absolute()
            .inset(yc::kWindowX, yc::kWindowY, yc::kWindowX, yc::kWindowY)
            .background(styles::dropShadow({0, 0, 0, 0.38f}, {0, 7}, 18))
            .fill(Fill::color(yc::C(0xE9EBEE)))
            .corners({6})
            .cache(Cache::Texture);

    // ---- assembly ---------------------------------------------------------
    return stack()
        .fill(Material::linear({0, 0}, {0, yc::kH},
                               {{0.0f, yc::C(0xB9BFC7)},
                                {1.0f, yc::C(0xA2A8B1)}}))
        .child(box().absolute().inset(0).fill(weave))
        .child(windowBackplate)
        // the window
        .child(
            box().absolute()
                .inset(yc::kWindowX, yc::kWindowY, yc::kWindowX, yc::kWindowY)
                .column()
                .corners({6}).clip()
                .stroke(util::stroke(1, Fill::color(yc::C(0x70777E))))
                .child(titleBar)
                .child(
                    box().column().grow(1).padding(28, 12)
                        .child(box().grow(0.55f))
                        .child(box().row().justify(Justify::Center)
                                   .child(wordmark))
                        .child(tagline)
                        .child(pills)
                        .child(abCard)
                        .child(box().grow(1))
                        // 3D groove rule - the <hr> of the period
                        .child(box().height(2).margin(4, 0, 4, 10)
                                   .opacity(withFrom(0.0f, 1.0f, {500ms}))
                                   .fill(Material::linear(
                                       {0, 0}, {0, 2},
                                       {{0.0f, yc::C(0x8F969D)},
                                        {0.5f, yc::C(0x8F969D)},
                                        {0.501f, yc::C(0xFFFFFF)},
                                        {1.0f, yc::C(0xFFFFFF)}})))
                        // footer: preset orb, caption, 1998 plastic button
                        .child(
                            box().row().alignItems(Align::End).key("footer")
                                .opacity(withFrom(0.0f, 1.0f, {500ms}))
                                .child(yc::gelOrb())
                                .child(
                                    box().column().margin(14, 0, 0, 4).gap(3)
                                        .child(text(
                                            toU8("now streaming @ 56k"),
                                            yc::type(12, yc::C(0x4E5760),
                                                     0.6f, 600)))
                                        .child(text(
                                            toU8("\xc2\xa9 2000 sigilnet "
                                                 "industries \xe2\x80\x94 "
                                                 "best viewed at 800\xc3\x97"
                                                 "600"),
                                            yc::type(10, yc::C(0x7A828B),
                                                     0.4f))))
                                .child(box().grow(1))
                                .child(
                                    box().column().alignItems(Align::End)
                                        .gap(5).margin(0, 0, 0, 2)
                                        .child(yc::plasticButton(
                                            "ENTER SITE >>"))
                                        .child(text(
                                            toU8("[ no frames \xc2\xb7 "
                                                 "spacer.gif free ]"),
                                            yc::type(10, yc::C(0x8A929B),
                                                     0.4f))))))
                .child(statusBar));
  }
};

} // namespace compose_gallery
