// MAGI's cognitive architecture: concentric memory registers, branching neural
// pathways and a protected personality core. The diagnostic inset exposes the
// same advancing infection as a live cell field; the voting screen is separate.
// TAGS: Interfaces/Film, Geometry/Diagrams

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>

#include <array>
#include <cmath>
#include <numbers>
#include <string>

#include "EvangelionUi.h"
#include "Infection.h"

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace weave = sigil::weave;
namespace ch = choreograph;
using namespace sigil::compose;

namespace {
const SkColor4f kGround = hexColor(0x060505);
const SkColor4f kOrange = hexColor(0xF39B39);
const SkColor4f kDim = hexColor(0x704021);
const SkColor4f kRed = hexColor(0xEB3C30);
const SkColor4f kMint = hexColor(0x72D9B0);
const SkColor4f kGold = hexColor(0xE5CD69);
constexpr SkPoint kCentre{488, 589};
constexpr float kMemoryWidth = 348;
constexpr float kMemoryHeight = 142;

SkPoint polar(float radius, float degrees) {
  const float angle = degrees * std::numbers::pi_v<float> / 180.0f;
  return {kCentre.fX + radius * std::cos(angle),
          kCentre.fY + radius * std::sin(angle)};
}

Element trace(SkPath path, SkColor4f ink, float width = 1.5f) {
  return box()
      .inset(0)
      .shape(heldPath(std::move(path)))
      .fill(Fill::none())
      .stroke(PathFormat{.width = width, .strokeFill = Fill::color(ink)});
}

Element segment(SkPoint a, SkPoint b, SkColor4f ink, float width = 1.5f) {
  return trace(SkPathBuilder().moveTo(a).lineTo(b).detach(), ink, width);
}

Element ring(float radius, SkColor4f ink, float width = 1.5f) {
  return trace(
      SkPathBuilder().addCircle(kCentre.fX, kCentre.fY, radius).detach(), ink,
      width);
}

Element arc(float radius, float start, float sweep, SkColor4f ink,
            float width) {
  const SkRect bounds = SkRect::MakeXYWH(
      kCentre.fX - radius, kCentre.fY - radius, radius * 2, radius * 2);
  return trace(SkPathBuilder().arcTo(bounds, start, sweep, true).detach(), ink,
               width);
}

struct EvaMagiInterior {
  weave::FontContext* fonts = nullptr;
  sk_sp<SkRuntimeEffect> infection = magi::infectionEffect();
  magi::Arrivals arrivals;
  ch::Output<float> front{-1};
  ch::Output<float> rotation{0};
  double clock = 0;
  int statusTick = -1;

  // Display text is fitted by its measured cap height and placed by its ink
  // top, so changing a fallback face does not change the spacing between rows.
  Element label(Utf8 content, SkPoint ink, float cap, SkColor4f color,
                float measure = 0) const {
    auto face = evangelion::condensedBold();
    const auto probe =
        metrics(weave::textStyle({.face = face, .size = 100}), *fonts);
    weave::Type type{.face = face, .size = cap * 100 / probe.capHeight};
    if (measure > 0) {
      const float width =
          intrinsicSize(text(content).font(type), *fonts).width();
      if (width > measure) type.condense = measure / width;
    }
    const float slack = metrics(weave::textStyle(type), *fonts).capSlack();
    return text(content).font(type).ink(color).at({ink.fX, ink.fY - slack});
  }

  Element frame() const {
    SkPathBuilder rim;
    rim.moveTo(48, 44)
        .lineTo(1314, 44)
        .lineTo(1392, 122)
        .lineTo(1392, 1006)
        .lineTo(48, 1006)
        .close();
    Element g = box().inset(0).children(
        {trace(rim.detach(), kOrange, 3),
         label("MAGI SYSTEM", {82, 82}, 58, kOrange, 650),
         label("V2.3762.123b  /  COGNITIVE ARCHITECTURE", {86, 158}, 20,
               kOrange, 750),
         text(u8"内部構造", evangelion::minchoDisplay(67, kOrange, 1.08f))
             .centerAt({1160, 114}),
         label("INTERNAL DIAGNOSTIC / 03", {1032, 173}, 15, kGold, 314),
         segment({80, 207}, {1358, 207}, kOrange, 3),
         segment({80, 216}, {1358, 216}, kDim),
         segment({924, 248}, {924, 960}, kDim),
         label("PERSONALITY SIMULATION SYSTEM", {86, 246}, 19, kGold, 740),
         label("NEURAL TOPOLOGY / CONCEPTUAL DIAGRAM", {86, 275}, 13, kOrange,
               630),
         label("NERV / CENTRAL DOGMA", {84, 972}, 16, kOrange),
         label("MEMORY PROTECTION : ACTIVE", {968, 972}, 16, kMint, 360)});
    for (int i = 0; i < 6; ++i)
      g.children({segment({1280.0f + i * 12, 63}, {1350.0f + i * 6, 133},
                          kOrange, 3)});
    return g;
  }

  Element architecture() const {
    Element g = box().inset(0);
    for (float radius : {136.0f, 146.0f, 222.0f, 229.0f, 279.0f, 324.0f})
      g.children({ring(radius, radius == 229 ? kOrange : kDim,
                       radius == 229 ? 2.0f : 1.0f)});
    g.children({arc(338, -90, 117, kMint, 4), arc(338, 30, 117, kRed, 4),
                arc(338, 150, 117, kGold, 4)});

    // Each sector contains eight register blocks. Their branching pathways
    // terminate at the inner bus; they do not pass through the personality
    // core.
    for (int i = 0; i < 24; ++i) {
      const float angle = i * 15.0f - 82.5f;
      const SkColor4f color = i < 8 ? kMint : (i < 16 ? kRed : kGold);
      const SkPoint soma = polar(302, angle);
      SkPathBuilder hexagon;
      for (int corner = 0; corner < 6; ++corner) {
        const float a = (angle + corner * 60) * std::numbers::pi_v<float> / 180;
        const SkPoint p{soma.fX + 13 * std::cos(a), soma.fY + 13 * std::sin(a)};
        if (corner == 0)
          hexagon.moveTo(p);
        else
          hexagon.lineTo(p);
      }
      hexagon.close();
      g.children({trace(hexagon.detach(), color, 1.8f)});
      SkPathBuilder dendrites;
      for (int branch = -2; branch <= 2; ++branch) {
        const SkPoint elbow = polar(258, angle + branch * 1.7f);
        const SkPoint inner = polar(234, angle + branch * 2.8f);
        dendrites.moveTo(soma).lineTo(elbow).lineTo(inner);
      }
      g.children({trace(dendrites.detach(), color, 1.1f)});
      g.children({segment(polar(318, angle), polar(328, angle), color, 2)});
    }
    for (int i = 0; i < 120; ++i) {
      const float angle = i * 3.0f;
      g.children(
          {segment(polar(i % 5 == 0 ? 206 : 212, angle), polar(218, angle),
                   i % 5 == 0 ? kOrange : kDim, i % 5 == 0 ? 2 : 1)});
    }
    // Address routes follow an orthogonal backplane inside the annular bus.
    for (int i = 0; i < 12; ++i) {
      const float angle = i * 30.0f + 15;
      const SkPoint a = polar(198, angle);
      const SkPoint b = polar(153, angle);
      SkPathBuilder route;
      route.moveTo(a).lineTo(a.fX, b.fY).lineTo(b);
      g.children(
          {trace(route.detach(), i >= 3 && i < 7 ? kRed : kOrange, 1.4f)});
      g.children({trace(SkPathBuilder().addCircle(a.fX, a.fY, 4).detach(),
                        kGold, 1.2f)});
    }
    // The glass-covered core carries a bilateral branching network. Mirrored
    // paths share the central trunk while their branch lengths remain unequal.
    const SkRect core = SkRect::MakeXYWH(390, 478, 196, 216);
    g.children(
        {box()
             .rect(core)
             .shape(evangelion::panel({.cut = {22, 22},
                                       .cutMask = evangelion::CutTopLeft |
                                                  evangelion::CutBottomRight}))
             .fill(Fill::color(hexColor(0x06130F)))
             .foreground(decorations::border(2, Fill::color(kMint)))});
    SkPathBuilder brain;
    brain.moveTo(488, 628).lineTo(488, 509);
    for (int side : {-1, 1}) {
      for (int level = 0; level < 7; ++level) {
        const float y = 524.0f + level * 13;
        const float reach = 31.0f + 24 * std::sin((level + 1) * 0.42f);
        brain.moveTo(488, y + 14)
            .lineTo(488 + side * reach, y)
            .lineTo(488 + side * (reach + 12), y + 4);
        brain.moveTo(488 + side * reach * 0.55f, y + 6)
            .lineTo(488 + side * reach * 0.72f, y - 7);
      }
    }
    g.children({trace(brain.detach(), kMint, 1.9f),
                label("CASPER", {426, 643}, 25, kMint, 126),
                label("PERSONALITY CORE / 03", {410, 675}, 11, kMint, 158),
                label("01", {470, 942}, 26, kRed),
                label("02", {134, 442}, 26, kGold),
                label("03", {808, 442}, 26, kMint),
                label("02 / BALTHASAR", {112, 966}, 15, kGold, 240),
                label("03 / CASPER", {660, 966}, 15, kMint, 220)});
    return g;
  }

  Element scanner() const {
    return box()
        .rect(SkRect::MakeXYWH(kCentre.fX - 185, kCentre.fY - 185, 370, 370))
        .rotate(&rotation)
        .transformOrigin(pct(50), pct(50))
        .children(
            {trace(SkPathBuilder()
                       .arcTo(SkRect::MakeXYWH(0, 0, 370, 370), -60, 78, true)
                       .detach(),
                   kGold, 4)});
  }

  Element diagnostic() const {
    Element g = box().inset(0).children(
        {text(u8"侵入警報", evangelion::minchoDisplay(66, kRed, 1.0f))
             .centerAt({1151, 282}),
         label("FOREIGN PATTERN DETECTED", {968, 333}, 18, kRed, 348),
         segment({968, 365}, {1358, 365}, kRed, 2),
         label("THREE-IN-ONE / SYSTEM STATUS", {968, 390}, 15, kOrange, 348),
         label("CODE : 132", {968, 649}, 30, kOrange),
         label("SOURCE : EXTERNAL NEURAL BUS", {968, 696}, 14, kOrange, 348),
         label("BALTHASAR / MEMORY SECTOR 02", {968, 743}, 16, kGold, 348)});
    const auto paint =
        mskia::Paint::sksl(infection)
            .slot("uArrival", mskia::Paint::shader(arrivals.field))
            .uniform("uCells", std::array<float, 2>{(float)arrivals.columns,
                                                    (float)arrivals.rows})
            .uniform("uPour", kRed)
            .uniform("uKey", hexColor(0x050606))
            .uniform("uFront", &front);
    g.children(
        {box()
             .rect(SkRect::MakeXYWH(968, 778, kMemoryWidth, kMemoryHeight))
             .clip(true)
             .fill(Fill::color(hexColor(0x183128)))
             .foreground(decorations::border(1.5f, Fill::color(kOrange)))
             .children({box().inset(0).fill(paint)})});
    for (int i = 0; i < 20; ++i)
      g.children({segment({968, 778.0f + i * 7}, {1316, 778.0f + i * 7},
                          hexColor(0x05130D), 1)});
    g.children({label("LIVE CELL MAP / ORTHOGONAL PROPAGATION", {968, 937}, 11,
                      kOrange, 348)});
    return g;
  }

  float fraction() const {
    const float phase = (float)std::fmod(clock, 18.0) / 12.0f;
    return std::clamp(phase, 0.0f, 1.0f);
  }

  Element status() const {
    Element g = box().inset(0);
    const float progress = fraction();
    const std::array<const char*, 3> names{"MELCHIOR / 01", "BALTHASAR / 02",
                                           "CASPER / 03"};
    const std::array<const char*, 3> states{"INVADED", "ISOLATING",
                                            "PROTECTED"};
    const std::array<SkColor4f, 3> colors{kRed, kGold, kMint};
    for (int i = 0; i < 3; ++i) {
      const float y = 432.0f + i * 67;
      const float coverage = i == 0 ? 1 : (i == 1 ? progress : 0);
      g.children({label(names[i], {968, y}, 20, colors[i], 198),
                  label(states[i], {1201, y + 2}, 13, colors[i], 151),
                  box()
                      .rect(SkRect::MakeXYWH(968, y + 32, 348, 5))
                      .fill(Fill::color(kDim))});
      if (coverage > 0)
        g.children({box()
                        .rect(SkRect::MakeXYWH(968, y + 32, coverage * 348, 5))
                        .fill(Fill::color(colors[i]))});
    }
    return g;
  }

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = {1440, 1052},
                             .captureAt = 4.5,
                             .background = kGround,
                             .nonlinearPicture = true});
    fonts = ctx.fonts;
    const SkRect memory = SkRect::MakeWH(kMemoryWidth, kMemoryHeight);
    arrivals = magi::arrivalTable(memory, 3.5f, {0, 14},
                                  SkPathBuilder().addRect(memory).detach());
    clock = 0;
    statusTick = -1;
    ctx.ticker.add([this](double dt) {
      clock += dt;
      front = magi::frontFor(arrivals, fraction());
      rotation = (float)std::fmod(clock * 7, 360.0);
    });
    ctx.composer.render(box().inset(0).children(
        {box()
             .inset(0)
             .children({frame().cache(Cache::Texture),
                        box()
                            .inset(0)
                            .transformOrigin(pct(100.0f * (kCentre.fX / 1440)),
                                             pct(100.0f * (kCentre.fY / 1052)))
                            .scale(0.86f)
                            .translateY(20)
                            .children({architecture().cache(Cache::Texture),
                                       scanner()}),
                        diagnostic(), slot("status")})
             .fill(mskia::Paint::solid(kGround))
             .effect(evangelion::crt(1440, 1052))}));
    ctx.composer.renderSlot("status", status());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const int tick = (int)(elapsed * 8);
    if (tick == statusTick) return;
    statusTick = tick;
    ctx.composer.renderSlot("status", status());
  }
};
}  // namespace

SIGIL_SKETCH(EvaMagiInterior, "Study · Film",
             "MAGI cognitive architecture: neural registers, a protected "
             "personality core and live infection diagnostics")
