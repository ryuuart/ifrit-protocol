// The flat MAGI deliberation plate: a rear circular bus, three rotated
// square modules, and a front information layer. The type registers separate
// computer labels, display numerals, the serif wordmark and Japanese headings.

#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/skia/Effect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>

#include <string>

#include "eva_magi_interior/EvangelionUi.h"

namespace sketch = sigil::sketch;
namespace mskia = sigil::material::skia;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

const SkColor4f kGround = hexColor(0x020202);
const SkColor4f kOrange = hexColor(0xF39A19);
const SkColor4f kOrangeDim = hexColor(0xA94C15);
const SkColor4f kMint = hexColor(0x59E7A0);
const SkColor4f kMintRule = hexColor(0x2AA98A);
const SkColor4f kMintRuleHi = hexColor(0x65E3BC);
const SkColor4f kInk = hexColor(0x071615);
const SkColor4f kRed = hexColor(0xA20915);
const SkColor4f kRedHot = hexColor(0xE1262E);

std::u8string toUtf8(const char* value) {
  return std::u8string(reinterpret_cast<const char8_t*>(value));
}

struct EvaMagiDeliberation : sketch::Sketch {
  evangelion::MagiVoteLayout layout;
  weave::FontContext* fonts = nullptr;

  /** A Latin run solved to a cap height and a measure, as a PARTIAL (face,
   *  size, condensation) set in the ink where it lands; probes are it whole. */
  weave::Type fit(const sk_sp<SkTypeface>& face, const std::u8string& run,
                  float capHeight, float maxWidth) const {
    float size = capHeight * 1.4f;
    if (fonts) {
      const TextMetrics probe =
          metrics(weave::textStyle({.face = face, .size = 100.0f}), *fonts);
      if (probe.capHeight > 1.0f) size = 100.0f * capHeight / probe.capHeight;
    }
    weave::Type style{.face = face, .size = size};
    if (fonts && maxWidth > 1.0f) {
      const SkSize measured = sigil::compose::intrinsicSize(
          text(run, weave::textStyle(style)), *fonts);
      if (measured.width() > maxWidth && measured.width() > 1.0f)
        style.condense = maxWidth / measured.width();
    }
    return style;
  }

  /** A Han run stays a whole style: the Mincho stand-in strokes its paint. */
  weave::TextStyle han(const std::u8string& run, float capHeight,
                       float maxWidth, SkColor4f color) const {
    weave::TextStyle style =
        evangelion::minchoDisplay(capHeight * 1.34f, color, 1.30f);
    if (fonts) {
      const TextMetrics probe = metrics(style, *fonts);
      if (probe.capHeight > 1.0f)
        style = evangelion::minchoDisplay(
            style.shaping.fontSize * capHeight / probe.capHeight, color, 1.30f);
      const SkSize measured =
          sigil::compose::intrinsicSize(text(run, style), *fonts);
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
    const std::u8string label = toUtf8(name);
    return box()
        .left(rect.left())
        .top(rect.top())
        .width(side)
        .height(side)
        .rotate(layout.rotationFor(number))
        .transformOrigin(0.5f, 0.5f)
        .fill(mskia::Paint::solid(kMint))
        .ink(kInk)
        .clip(true)
        .style(decorations::doubleBorder(
            decorations::border(6.0f, Fill::color(kOrange), 0.0f),
            decorations::border(3.0f, Fill::color(kInk), 9.0f)))
        .child(text(numeral)
                   .font(fit(evangelion::voteNumeral(number), numeral, 88.0f,
                             side - 48.0f))
                   .centerAt({side * 0.5f, side * layout.numberSlotY(number)}))
        .child(text(label)
                   .font(fit(evangelion::moduleLabel(), label, 31.0f,
                             side - 48.0f))
                   .centerAt({side * 0.5f, side * layout.nameSlotY(number)}));
  }

  /** The layer's Latin is orange, stated once; the whole-style Han names its
   * own. */
  Element information() const {
    Element group = box().inset(0).ink(kOrange);
    group.child(rules(145.0f, 106.0f, 375.0f));
    group.child(rules(145.0f, 241.0f, 375.0f));
    group.child(rules(920.0f, 106.0f, 375.0f));
    group.child(rules(920.0f, 241.0f, 375.0f));

    group.child(text(u8"提訴", han(u8"提訴", 83.0f, 300.0f, kOrange))
                    .centerAt({332.5f, 184.0f}));
    group.child(text(u8"決議", han(u8"決議", 83.0f, 300.0f, kOrange))
                    .centerAt({1107.5f, 184.0f}));

    group.child(text(u8"CODE : 132")
                    .font(fit(evangelion::condensedBold(), u8"CODE : 132",
                              45.0f, 270.0f))
                    .left(151.0f)
                    .top(275.0f));

    static const char* kData[] = {"FILE:MAGI_SYS", "EXTENTION:2048",
                                  "EX_MODE:ON", "PRIORITY:A__"};
    for (int line = 0; line < 4; ++line) {
      const std::u8string run = toUtf8(kData[line]);
      group.child(
          text(run)
              .font(fit(evangelion::condensedBold(), run, 22.0f, 286.0f))
              .left(151.0f)
              .top(334.0f + (float)line * 32.0f));
    }

    group.child(
        text(u8"MAGI")
            .font(fit(evangelion::magiWordmark(), u8"MAGI", 54.0f, 230.0f))
            .centerAt({720.0f, 535.0f}));

    group.child(
        box()
            .left(995.0f)
            .top(295.0f)
            .width(275.0f)
            .height(130.0f)
            .fill(mskia::Paint::solid(hexColor(0x150103)))
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
    root.child(picture()
                   .effect(evangelion::phosphor())
                   .cache(Cache::Texture)
                   .key("phosphor"));
    root.child(box()
                   .inset(0)
                   .fill(mskia::Paint::recipe(evangelion::tube()))
                   .cache(Cache::Texture));
    return root;
  }

  void setup(sketch::SketchContext& context) override {
    context.canvas(layout.canvasWidth, layout.canvasHeight);
    context.background(kGround);
    fonts = context.fonts;
    // The plate does not move: one moment, named, so the sweep and the
    // window render the same picture.
    context.captureAt(0.05);
    context.nonlinearPicture();
    context.composer.render(describe());
  }
};

}  // namespace

SIGIL_SKETCH(EvaMagiDeliberation, "Study \xc2\xb7 Film",
             "Evangelion MAGI deliberation \xe2\x80\x94 one rear bus and three "
             "rotated module instances")
