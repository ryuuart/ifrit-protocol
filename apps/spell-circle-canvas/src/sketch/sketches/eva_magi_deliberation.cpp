// The flat MAGI deliberation plate is one routed system: a rear circular bus,
// three copies of one square module, and an information layer above both.

#include <include/core/SkPaint.h>
#include <shared/EvangelionUi.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

SkColor4f hex(uint32_t value, float alpha = 1.0f) {
  return {(float)((value >> 16u) & 255u) / 255.0f,
          (float)((value >> 8u) & 255u) / 255.0f,
          (float)(value & 255u) / 255.0f, alpha};
}

const SkColor4f kGround = hex(0x020202);
const SkColor4f kOrange = hex(0xF08320);
const SkColor4f kOrangeDim = hex(0xA94C15);
const SkColor4f kMint = hex(0x63E6BC);
const SkColor4f kMintRule = hex(0x2AA98A);
const SkColor4f kMintRuleHi = hex(0x65E3BC);
const SkColor4f kInk = hex(0x071615);
const SkColor4f kRed = hex(0xA20915);
const SkColor4f kRedHot = hex(0xE1262E);

std::u8string toU8(const char* value) {
  return std::u8string(reinterpret_cast<const char8_t*>(value));
}

struct EvaMagiDeliberation : sketch::Sketch {
  evangelion::MagiVoteLayout layout;
  weave::FontContext* fonts = nullptr;

  weave::TextStyle fit(const sk_sp<SkTypeface>& face, const std::u8string& run,
                       float capHeight, float maxWidth, SkColor4f color,
                       float tracking = 0.0f) const {
    float size = capHeight * 1.4f;
    if (fonts) {
      const TextMetrics probe =
          metrics(evangelion::type(face, 100.0f, color), *fonts);
      if (probe.capHeight > 1.0f) size = 100.0f * capHeight / probe.capHeight;
    }
    weave::TextStyle style =
        evangelion::type(face, size, color, 1.0f, tracking);
    if (fonts && maxWidth > 1.0f) {
      const SkSize measured = sigil::compose::measure(text(run, style), *fonts);
      if (measured.width() > maxWidth && measured.width() > 1.0f)
        style.shaping.scaleX = maxWidth / measured.width();
    }
    return style;
  }

  weave::TextStyle han(const std::u8string& run, float capHeight,
                       float maxWidth, SkColor4f color) const {
    weave::TextStyle style =
        evangelion::minchoDisplay(capHeight * 1.34f, color, 1.12f);
    if (fonts) {
      const TextMetrics probe = metrics(style, *fonts);
      if (probe.capHeight > 1.0f)
        style = evangelion::minchoDisplay(
            style.shaping.fontSize * capHeight / probe.capHeight, color, 1.12f);
      const SkSize measured = sigil::compose::measure(text(run, style), *fonts);
      if (measured.width() > maxWidth && measured.width() > 1.0f)
        style.shaping.scaleX *= maxWidth / measured.width();
    }
    return style;
  }

  Element rules(float left, float top, float width) const {
    Element group = box().inset(0);
    for (int line = 0; line < 3; ++line)
      group.child(
          box()
              .left(left)
              .top(top + (float)line * 7.0f)
              .width(width)
              .height(line == 1 ? 3.0f : 2.0f)
              .fill(mskia::Paint::solid(line == 1 ? kMintRuleHi : kMintRule)));
    return group;
  }

  Element backplane() const {
    const SkRect frame = layout.frame();
    Element group = box().inset(0);
    group.child(
        box()
            .left(frame.left())
            .top(frame.top())
            .width(frame.width())
            .height(frame.height())
            .fill(Fill::none())
            .foreground(decorations::border(7.0f, Fill::color(kOrange))));

    // The bus is deliberately earlier in the display list. Every module is an
    // opaque mask over it, so the route disappears cleanly at module edges.
    group.child(kit::disc(layout.busCentre, layout.busRadius)
                    .shape(sigil::geometry::shapes::circle())
                    .fill(Fill::none())
                    .foreground(decorations::border(
                        5.0f, Fill::color(kOrangeDim), 0.0f)));
    return group;
  }

  Element module(int number, const char* name) const {
    const SkRect rect = layout.moduleRect(number);
    const float side = layout.moduleSide;
    const std::u8string numeral =
        number == 1 ? u8"1" : (number == 2 ? u8"2" : u8"3");
    const std::u8string label = toU8(name);
    return box()
        .left(rect.left())
        .top(rect.top())
        .width(side)
        .height(side)
        .rotate(layout.rotationFor(number))
        .transformOrigin(0.5f, 0.5f)
        .fill(mskia::Paint::solid(kMint))
        .clip(true)
        .style(decorations::doubleBorder(
            decorations::border(6.0f, Fill::color(kOrange), 0.0f),
            decorations::border(3.0f, Fill::color(kInk), 9.0f)))
        .child(text(numeral, fit(evangelion::groteskBold(), numeral, 88.0f,
                                 side - 48.0f, kInk))
                   .centerAt({side * 0.5f, side * layout.numberSlotY(number)}))
        .child(text(label, fit(evangelion::groteskBold(), label, 31.0f,
                               side - 48.0f, kInk))
                   .centerAt({side * 0.5f, side * layout.nameSlotY(number)}));
  }

  Element information() const {
    Element group = box().inset(0);
    group.child(rules(145.0f, 106.0f, 375.0f));
    group.child(rules(145.0f, 241.0f, 375.0f));
    group.child(rules(920.0f, 106.0f, 375.0f));
    group.child(rules(920.0f, 241.0f, 375.0f));

    group.child(text(u8"提訴", han(u8"提訴", 83.0f, 300.0f, kOrange))
                    .centerAt({332.5f, 184.0f}));
    group.child(text(u8"決議", han(u8"決議", 83.0f, 300.0f, kOrange))
                    .centerAt({1107.5f, 184.0f}));

    group.child(
        text(u8"CODE : 132", fit(evangelion::condensedBold(), u8"CODE : 132",
                                 45.0f, 270.0f, kOrange))
            .left(151.0f)
            .top(275.0f));

    static const char* kData[] = {"FILE:MAGI_SYS", "EXTENTION:2048",
                                  "EX_MODE:ON", "PRIORITY:A__"};
    for (int line = 0; line < 4; ++line) {
      const std::u8string run = toU8(kData[line]);
      group.child(text(run, fit(evangelion::condensedBold(), run, 22.0f, 286.0f,
                                kOrange))
                      .left(151.0f)
                      .top(334.0f + (float)line * 32.0f));
    }

    group.child(text(u8"MAGI", fit(evangelion::groteskBold(), u8"MAGI", 54.0f,
                                   230.0f, kOrange))
                    .centerAt({720.0f, 535.0f}));

    group.child(
        box()
            .left(995.0f)
            .top(295.0f)
            .width(275.0f)
            .height(130.0f)
            .fill(mskia::Paint::solid(hex(0x150103)))
            .style(decorations::doubleBorder(
                decorations::border(7.0f, Fill::color(kRed), 0.0f),
                decorations::border(3.0f, Fill::color(kRedHot), 14.0f)))
            .child(text(u8"審議中", han(u8"審議中", 49.0f, 205.0f, kRedHot))
                       .centerAt({137.5f, 65.0f})));
    return group;
  }

  Element picture() const {
    Element scene = box().inset(0);
    scene.child(backplane());
    scene.child(module(2, "BALTHASAR"));
    scene.child(module(3, "CASPER"));
    scene.child(module(1, "MELCHIOR"));
    scene.child(information());
    return scene;
  }

  Element describe() const {
    Element root = box().inset(0);
    root.child(
        picture()
            .effect(mskia::Effect::phosphorBloom(10.0f, 0.48f, 0.46f, 0.84f))
            .cache(Cache::Texture)
            .key("phosphor"));
    root.child(box()
                   .inset(0)
                   .fill(mskia::Paint::recipe(
                       sigil::material::field::crtOverlay(4.0f, 0.040f)))
                   .cache(Cache::Texture));
    return root;
  }

  void setup(sketch::SketchContext& context) override {
    context.canvas(layout.canvasWidth, layout.canvasHeight);
    context.background(kGround);
    fonts = context.fonts;
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(EvaMagiDeliberation, "Study \xc2\xb7 Film",
             "Evangelion MAGI deliberation \xe2\x80\x94 one rear bus and three "
             "rotated module instances")
