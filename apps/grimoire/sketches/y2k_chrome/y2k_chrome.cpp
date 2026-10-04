/** @file
 * A desktop hyperportal dressed in liquid chrome, brushed aluminium,
 * smoked glass and optical gel. The classic Aqua control sits beside
 * its deeper gel counterpart, with the same geometry and label.
 */

// TAGS: Materials/Compositing, Interfaces/Desktop

#include <include/core/SkMaskFilter.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Presets.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/filter/Filter.h>
#include <sigilmaterial/paint/Bases.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Ticker.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <string_view>

#include "Aqua.h"
#include "ChromeType.h"
#include "Gloss.h"

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace motion = sigil::motion;

using namespace sigil::compose;
using sigil::material::hexColor;
using sigil::material::Paint;
using namespace std::chrono_literals;

namespace {
/** The canvas this piece was drawn against, which is also the default a
 *  sketch gets when it declares none. */
constexpr SkSize kSceneSize = {900, 640};

namespace y2k_chrome {

constexpr float kW = kSceneSize.fWidth, kH = kSceneSize.fHeight;
constexpr float kWindowX = 20, kWindowY = 16;  // window inset in the scene
constexpr float kTitleBarH = 34;
constexpr float kPlateH = 96;  // the wordmark inset
constexpr float kPillW = 158, kPillH = 42;
constexpr float kOrbD = 84;
constexpr float kStatusH = 22;
constexpr float kTickerSpeed = 70;  // px/s, the lazy 56k crawl
constexpr float kTickerGap = 40;    // between the two marquee copies

inline sigil::weave::TextStyle type(float size, material::Color color,
                                    float tracking = 0, float weight = 0) {
  return sigil::weave::textStyle(
      {.size = size, .color = color, .track = tracking, .weight = weight});
}

/** A gel label's ink and the dark ground cast just below it. */
inline material::Material gelInk(material::Color tint) {
  return material::from(material::Color{1, 1, 1, 0.98f})
      .effects(material::Filter::shadow(
          {tint.r * 0.30f, tint.g * 0.30f, tint.b * 0.30f, 0.5f},
          {.offset = {0, 1.2f}}));
}

/** The type this card names, bound around the description: `gelLabel` is
 *  the white label riding a gel surface, over the ground each pill derives
 *  from its own tint; `caption` the small line under the A/B specimens;
 *  `note` the footer's fine print. */
inline sigil::compose::StyleSheet classes() {
  return sigil::compose::StyleSheet{
      sigil::compose::rule(".gelLabel")
          .font({.size = 16,
                 .color = material::Color{1, 1, 1, 0.98f},
                 .track = 1.0f,
                 .weight = 650}),
      sigil::compose::rule("caption, .caption")
          .font({.size = 10,
                 .color = hexColor(0xAFC0DE),
                 .track = 0.8f,
                 .weight = 600}),
      sigil::compose::rule(".note").font(
          {.size = 10, .color = hexColor(0x8DA0C4), .track = 0.4f})};
}

// ---------------------------------------------------------------------------
// PRESET pill / orb - the whole recipe is one style() call.

inline Element gelPill(std::string_view label, material::Color tint,
                       float w = kPillW, float h = kPillH) {
  return box()
      .width(w)
      .height(h)
      .borderRadius({h / 2})
      .fill(y2k::aquaGel(tint, h))  // body, glow, recess, halo, hairline
      .foreground(y2k::aquaLens())
      .row()
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({text(label)
                     .styleClass("gelLabel")
                     .decorationOutline(Boundary::Glyphs)
                     .ink(gelInk(tint))});
}

inline Element gelOrb(float d = kOrbD) {
  return box()
      .width(d)
      .height(d)
      .borderRadius({d / 2})
      .fill(y2k::aquaOrb(hexColor(0x087FEB), d))
      .stroke(y2k::liquidMetal(0.28f), {.width = 1.25f})
      .foreground(y2k::aquaOrbLens())
      .foreground(y2k::gloss({0.85f, 0.95f, 1.0f, 0.18f}, d * 0.07f,
                             {0, -d * 0.04f}, 0.48f, 0.30f));
}

// ---------------------------------------------------------------------------
// The HAND-BUILT Aqua pill, kept as the A/B reference against the preset:
// five stops and a knockout highlight, spelled out.

struct PillTint {
  material::Color deep, mid, light, glow, halo;
};

inline constexpr PillTint kBluePill{
    {28 / 255.f, 91 / 255.f, 155 / 255.f, 0.82f},
    {108 / 255.f, 191 / 255.f, 255 / 255.f, 0.90f},
    hexColor(0x7ECBFF),
    hexColor(0xB0E5FF),
    {66 / 255.f, 140 / 255.f, 240 / 255.f, 0.5f}};

inline Element aquaPill(std::string_view label, const PillTint& t,
                        float w = kPillW, float h = kPillH) {
  const float r = h / 2;  // true pill
  // rim 1.5px per-side #8BA2C1 / #5890BF / #4F93CA / #768FA5.
  auto rim = [](path::Edge e, material::Color c) {
    return onEdges(e, stroke(1.5f, Fill::color(c)));
  };
  return box()
      .width(w)
      .height(h)
      .borderRadius({r})
      // halo: rgba(66,140,240,.5) offset (0,10) blur 16 - under the fill

      // body ramp: deep .82 -> mid .9 @0.9 -> light
      .fill(sigil::material::from(
                sigil::material::linearGradient(
                    {0, 0}, {0, h},
                    {{0.0f, t.deep}, {0.9f, t.mid}, {1.0f, t.light}},
                    {.units = material::GradientUnits::Pixels}))
                .effects(sigil::material::Filter::shadow(
                    t.halo, {.blur = 16, .offset = {0, 10}})))
      .foreground(rim(path::Edge::Top, hexColor(0x8BA2C1)))
      .foreground(rim(path::Edge::Right, hexColor(0x5890BF)))
      .foreground(rim(path::Edge::Bottom, hexColor(0x4F93CA)))
      .foreground(rim(path::Edge::Left, hexColor(0x768FA5)))
      // bottom glow: inset 2, fades out by 45% up from the bottom, screen
      .children(
          {box()
               .inset(h * 0.55f, 2, 2, 2)
               .borderRadius({r - 2})
               .fill(sigil::material::linearGradient(
                   {0, h * 0.45f - 4}, {0, 0},
                   {{0.0f, {t.glow.r, t.glow.g, t.glow.b, 0.85f}},
                    {1.0f, {t.glow.r, t.glow.g, t.glow.b, 0.0f}}},
                   {.units = material::GradientUnits::Pixels}))
               .blendMode(material::BlendMode::Screen),
           // the LENS: x in [5%,95%] y in [4%,52%], white .72->0
           box()
               .inset(h * 0.04f, w * 0.05f, h * 0.48f, w * 0.05f)
               .borderRadius({h * 0.24f})
               .fill(sigil::material::linearGradient(
                   {0, 0}, {0, h * 0.48f},
                   {{0.0f, {1, 1, 1, 0.72f}}, {1.0f, {1, 1, 1, 0.0f}}},
                   {.units = material::GradientUnits::Pixels})),
           // label, centered, riding above the lens
           kit::centred()
               .inset(0)
               .row()

               .zIndex(1)
               .children({text(label)
                              .styleClass("gelLabel")
                              .decorationOutline(Boundary::Glyphs)
                              .ink(gelInk({t.deep.r * 1.3f, t.deep.g * 1.3f,
                                           t.deep.b * 1.3f, 1}))})});
}

// ---------------------------------------------------------------------------
// The action key carries the same polished metal as the window controls.

inline Element metalButton(std::string_view label) {
  return box()
      .width(150)
      .height(34)
      .borderRadius({4})
      .fill(y2k::y2kChrome({.palette = y2k::ChromePalette::Silver}))
      .row()
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({text(label, type(13, hexColor(0x091420), 0.5f, 750))});
}

/** Polished window keys; the close key carries a ruby gel face. */
inline Element chromeControl(std::string_view mark, bool close = false) {
  const auto finish =
      close ? y2k::aquaGel(hexColor(0xDA493F), 13, {.halo = false})
            : y2k::liquidMetal(0.22f, 0.24f)
                  .effects(
                      material::Filter::bevel({.depth = 0.75f, .size = 0.4f}));
  return box()
      .width(14)
      .height(13)
      .borderRadius({2})
      .fill(finish)
      .stroke(stroke(0.7f, Fill::color({0.07f, 0.12f, 0.19f, 0.8f})))
      .row()
      .alignItems(Align::Center)
      .justifyContent(Justify::Center)
      .children({text(mark).font(
          {.size = 10,
           .color = close ? hexColor(0xFFFFFF) : hexColor(0x101A25),
           .weight = 700})});
}

/** White starburst glint (shapes::star, thin 4-point). */
inline Element glint(float size, float rotationDeg, float alpha = 0.95f) {
  return box()
      .width(size)
      .height(size)
      .shape(shapes::star(4, 0.10f))
      .fill(Fill::color({1, 1, 1, alpha}))
      .rotate(rotationDeg)
      .zIndex(3);
}

}  // namespace y2k_chrome

struct Y2kChrome {
  motion::Animatable<float> tickX = motion::animatable(0.0f);
  float unitW = 0;  // strip content's intrinsic width (compose::intrinsicSize)
  float wrapLen = 1;  // marquee wrap length = unitW + gap

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 6.0,
                             .background = material::Color{0, 0, 0, 1}});
    Composer& composer = ctx.composer;
    sigil::motion::Engine& ticker = ctx.engine;
    namespace yc = y2k_chrome;
    tickX = 0;

    const SkSize unit = ctx.measure(stripContent());
    unitW = std::ceil(unit.width());
    wrapLen = unitW + yc::kTickerGap;

    ticker.timer([this, &ticker] {
      namespace yc = y2k_chrome;
      const double t = ticker.elapsed().count();
      tickX = -(float)std::fmod(t * yc::kTickerSpeed, (double)wrapLen);
    });

    composer.render(describe());
  }

  Element stripContent() {
    namespace yc = y2k_chrome;
    const char* unit =
        "· WELCOME TO SIGILNET 2000 · "
        "Y2K COMPLIANT · BEST VIEWED AT 800×600 · SIGN THE GUESTBOOK · NO "
        "FRAMES ";
    Element content =
        box()
            .row()
            .alignItems(Align::Center)
            .height(yc::kStatusH)
            .children({text(unit, yc::type(11, hexColor(0x39424C), 1.0f, 550))
                           .flexShrink(0)});
    if (unitW > 0) content.width(unitW).flexShrink(0);
    return content;
  }

  Element describe() {
    namespace yc = y2k_chrome;
    using namespace std::chrono_literals;

    // The title bar's restrained brushing keeps its small caption legible.
    Element titleBar =
        box()
            .height(yc::kTitleBarH)
            .fill(y2k::brushedMetal().effects(material::Filter::bevel(
                {.depth = 1.5f,
                 .size = 0.7f,
                 .highlight = {1, 1, 1, 0.85f},
                 .shadow = {0.04f, 0.08f, 0.13f, 0.8f}})))
            .row()
            .alignItems(Align::Center)
            .padding(0, 12)
            .gap(5)
            .children(
                {text("SIGILNET 2000 — hyperportal v4.2")
                     .font({.size = 12,
                            .color = hexColor(0x172431),
                            .track = 0.4f,
                            .weight = 600})
                     .decorationOutline(Boundary::Glyphs)
                     .ink(material::from(hexColor(0x172431))
                              .effects(material::Filter::shadow(
                                  {1, 1, 1, 0.65f}, {.offset = {0, 0.65f}}))),
                 box().flexGrow(1), yc::chromeControl("−"),
                 yc::chromeControl("□"), yc::chromeControl("×", true)});

    // The dark inset separates the reflected silver face from its metal rim.
    const float plateH = yc::kPlateH;
    const float horizonY = plateH * 0.50f;
    Element plate =
        kit::centred()
            .height(plateH)
            .borderRadius({10})
            .fill(y2k::smokedGlass())
            .stroke(y2k::liquidMetal(0.30f), {.width = 2.5f})
            .row()
            .padding(0, 34)
            .children(
                {text("MILLENNIUM")
                     .font({.face = weave::ports::face(
                                {"Helvetica Neue", "Arial"}, 800),
                            .size = 54,
                            .color = hexColor(0xFFFFFF),
                            .track = 3,
                            .weight = 800})
                     .decorationOutline(Boundary::Glyphs)
                     .ink(y2k::silverChromeType().effects(
                         material::Filter::shadow(
                             {0, 0, 0, 0.9f}, {.blur = 2.5f, .offset = {0, 3}})
                             .then(material::Filter::bevel(
                                 {.depth = 1.5f,
                                  .size = 0.7f,
                                  .highlight = {1, 1, 1, 0.9f},
                                  .shadow = {0.02f, 0.04f, 0.08f, 0.9f}}))
                             .then(material::Filter::stroke(
                                 {0.01f, 0.025f, 0.05f, 0.9f},
                                 {.width = 0.65f}))))});

    Element wordmark =
        box()
            .key("wordmark")
            .translateY(
                sigil::motion::animate({.from = 14.0f,
                                        .to = 0.0f,
                                        .duration = 550ms,
                                        .ease = motion::ease::outQuint}))
            .opacity(sigil::motion::animate(
                {.from = 0.0f, .to = 1.0f, .duration = 400ms}))
            .children(
                {plate,
                 // Glints mark the outer metal lip without crossing the type.
                 box()
                     .inset(horizonY - 12, 0, 0, -12)
                     .children({yc::glint(24, 0)}),
                 box()
                     .inset(horizonY - 9, -9, 0, 0)
                     .row()
                     .justifyContent(Justify::End)
                     .children({yc::glint(18, 18, 0.9f)}),
                 box()
                     .inset(-6, 110, 0, 0)
                     .row()
                     .justifyContent(Justify::End)
                     .children({yc::glint(13, 12, 0.85f)})});

    // ---- tagline: sigil::material::Filter::glow, chained for the hotter
    // double glow ---- A tight white glint and a wider cyan halo surround the
    // blue letters.
    Element tagline =
        // Compensated padding preserves the original y/height while giving
        // the cached raster 24px of transparent room for its 7px glow.
        box()
            .row()
            .justifyContent(Justify::Center)
            .padding(24, 0)
            .margin(-12, 0, -24, 0)
            .cache(Cache::Texture)
            .opacity(sigil::motion::animate(
                {.from = 0.0f, .to = 1.0f, .duration = 400ms}))
            .children({text("· t h e   f u t u r e   i s   "
                            "c h r o m e ·",
                            yc::type(14, hexColor(0x7FD0FF), 2.5f, 650))
                           .filter(sigil::material::Filter::glow(
                                       {1.0f, 1.0f, 1.0f, 0.95f}, 2)
                                       .then(sigil::material::Filter::glow(
                                           {0.36f, 0.80f, 1.0f, 0.9f}, 7)))});

    // ---- pill rack: three aquaGel() recolors ------------------------------
    Element pills =
        box()
            .row()
            .justifyContent(Justify::Center)
            .gap(22)
            .margin(18, 0, 0, 0)
            .key("pills")
            .translateY(
                sigil::motion::animate({.from = 12.0f,
                                        .to = 0.0f,
                                        .duration = 550ms,
                                        .ease = motion::ease::outQuint}))
            .opacity(sigil::motion::animate(
                {.from = 0.0f, .to = 1.0f, .duration = 400ms}))
            .children({yc::gelPill("ENTER  PORTAL", hexColor(0x1E8FFF)),
                       yc::gelPill("HOT  LINKS", hexColor(0xE03A3A)),
                       yc::gelPill("GUESTBOOK", hexColor(0x2AA84F))});

    // ---- the A/B card: hand-built recipe vs the preset --------------------
    // ONE SPECIMEN OF THE A/B: a pill over the caption that says how it
    // was made. The two halves differ in the pill alone, which is the
    // whole point of standing them side by side.
    const auto specimen = [](Element pill, const Utf8& how) {
      return box()
          .column()
          .alignItems(Align::Center)
          .gap(6)
          .children({std::move(pill), text(how).styleClass("caption")});
    };
    Element abCard =
        box()
            .row()
            .justifyContent(Justify::Center)
            .margin(16, 0, 0, 0)
            .opacity(sigil::motion::animate(
                {.from = 0.0f, .to = 1.0f, .duration = 500ms}))
            .children({box()
                           .row()
                           .gap(38)
                           .padding(12, 24)
                           .borderRadius({8})
                           .fill(y2k::smokedGlass())
                           .children({specimen(yc::aquaPill("AQUA  2000",
                                                            yc::kBluePill),
                                               "CLASSIC AQUA"),
                                      specimen(yc::gelPill("AQUA  2000",
                                                           hexColor(0x1E8FFF)),
                                               "OPTICAL GEL")})});

    // ---- status bar: marquee, ticker-driven phase -------------------
    // The crawl, named: a strip run past a window twice so the loop has no
    // seam, with the wrap this file already keeps on `tickX`.
    Element strip = sketch::kit::ticker(
        {.content = stripContent(), .phase = tickX, .gap = yc::kTickerGap});
    strip.flexGrow(1);
    Element statusBar =
        box()
            .height(yc::kStatusH)
            .fill(y2k::brushedMetal())
            .foreground(onEdges(path::Edge::Top,
                                stroke(1, Fill::color(hexColor(0xFFFFFF)))))
            .background(onEdges(path::Edge::Top,
                                stroke(1, Fill::color(hexColor(0x8F969D)))))
            .row()
            .alignItems(Align::Center)
            .padding(0, 10)
            .gap(8)
            .children(
                {strip,
                 kit::line({.length = Dimension(11),
                            .column = true,
                            .fill = Fill::color(hexColor(0xA6ADB4))}),
                 text("56K", yc::type(10, hexColor(0x6A737D), 1.0f, 700))});

    // Keep the static shadow and tiled ground outside the moving ticker.
    Element windowBackplate =
        box()
            .inset(yc::kWindowY, yc::kWindowX)

            .fill(sigil::material::from(
                      sigil::material::linearGradient(
                          {0, 0}, {0, yc::kH},
                          {{0.00f, hexColor(0x16204A)},
                           {0.48f, hexColor(0x0B1030)},
                           {1.00f, hexColor(0x050817)}},
                          {.units = material::GradientUnits::Pixels}))
                      .effects(sigil::material::Filter::shadow(
                          {0, 0, 0, 0.38f}, {.blur = 18, .offset = {0, 7}})))
            .borderRadius({6})
            .overflow(Overflow::Clip)
            .children({box()
                           .inset(0)
                           .fill(Pattern(material::pattern::stripes(
                                             1, 4, hexColor(0x6E8CD8, 0.10f)))
                                     .rotate(45)
                                     .material())
                           .blendMode(material::BlendMode::PlusLighter),
                       box().inset(0).fill(sigil::material::radialGradient(
                           {0.5f, 0.42f}, 1.02f,
                           {{0.0f, {0.36f, 0.52f, 0.92f, 0.16f}},
                            {0.55f, {0, 0, 0, 0.0f}},
                            {1.0f, {0, 0, 0, 0.45f}}},
                           {.extent = material::RadialExtent::ClosestSide}))})
            .cache(Cache::Texture);

    // ---- assembly ---------------------------------------------------------
    // The card's sheet stands on the root, so every class written under it
    // — the pill helpers included — resolves here.
    return stack()
        .applyStyleSheet(yc::classes())
        .children(
            {box().inset(0).fill(y2k::brushedMetal()).cache(Cache::Texture),
             windowBackplate,
             // the window
             box()
                 .inset(yc::kWindowY, yc::kWindowX)
                 .column()
                 .borderRadius({6})
                 .overflow(Overflow::Clip)
                 .stroke(stroke(1, Fill::color(hexColor(0x70777E))))
                 .children(
                     {titleBar,
                      box().column().flexGrow(1).padding(12, 28).children(
                          {box().flexGrow(0.55f),
                           box()
                               .row()
                               .justifyContent(Justify::Center)
                               .children({wordmark}),
                           tagline, pills, abCard, box().flexGrow(1),
                           // 3D groove rule - the <hr> of the period
                           box()
                               .height(2)
                               .margin(0, 4, 10, 4)
                               .opacity(
                                   sigil::motion::animate({.from = 0.0f,
                                                           .to = 1.0f,
                                                           .duration = 500ms}))
                               .fill(sigil::material::linearGradient(
                                   {0, 0}, {0, 2},
                                   {{0.0f, hexColor(0x8F969D)},
                                    {0.5f, hexColor(0x8F969D)},
                                    {0.501f, hexColor(0xFFFFFF)},
                                    {1.0f, hexColor(0xFFFFFF)}},
                                   {.units = material::GradientUnits::Pixels})),
                           // Gel orb, streaming caption and polished action
                           // key.
                           box()
                               .row()
                               .alignItems(Align::End)
                               .key("footer")
                               .opacity(
                                   sigil::motion::animate({.from = 0.0f,
                                                           .to = 1.0f,
                                                           .duration = 500ms}))
                               .children(
                                   {yc::gelOrb(),
                                    box()
                                        .column()
                                        .margin(0, 0, 4, 14)
                                        .gap(3)
                                        .children(
                                            {text("now streaming @ 56k",
                                                  yc::type(12,
                                                           hexColor(0xC8D6EE),
                                                           0.6f, 600)),
                                             text("© 2000 sigilnet "
                                                  "industries — "
                                                  "best viewed at 800×600")
                                                 .styleClass("note")}),
                                    box().flexGrow(1),
                                    box()
                                        .column()
                                        .alignItems(Align::End)
                                        .gap(5)
                                        .margin(0, 0, 2, 0)
                                        .children(
                                            {yc::metalButton("ENTER SITE >>"),
                                             text("[ no frames · "
                                                  "spacer.gif free ]")
                                                 .styleClass("note")})})}),
                      statusBar})});
  }
};

}  // namespace

SIGIL_SKETCH_AS(Y2kChrome, "y2k chrome", "Catalog · Chrome",
                "liquid chrome, optical gel and brushed desktop metal")
