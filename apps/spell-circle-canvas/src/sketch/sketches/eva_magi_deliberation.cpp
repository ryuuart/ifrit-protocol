// The flat MAGI deliberation plate: a rear circular bus, three rotated
// square modules, and a front information layer. The type registers separate
// computer labels, display numerals, the serif wordmark and Japanese headings.

// TAGS: Geometry/Diagrams, Interfaces/Film

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

#include <array>
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

struct EvaMagiDeliberation {
  evangelion::MagiVoteLayout layout;
  weave::FontContext* fonts = nullptr;

  /** A Latin run solved to a cap height and a measure, as a PARTIAL (face,
   *  size, condensation) set in the ink where it lands, or over the initial
   *  values when a probe lays it out alone; the metrics probe takes it whole,
   *  since metrics() reads a style rather than a tree. */
  weave::Type fit(const sk_sp<SkTypeface>& face, const Utf8& run,
                  float capHeight, float maxWidth) const {
    float size = capHeight * 1.4f;
    if (fonts) {
      const TextMetrics probe =
          metrics(weave::textStyle({.face = face, .size = 100.0f}), *fonts);
      if (probe.capHeight > 1.0f) size = 100.0f * capHeight / probe.capHeight;
    }
    weave::Type style{.face = face, .size = size};
    if (fonts && maxWidth > 1.0f) {
      const SkSize measured =
          sigil::compose::intrinsicSize(text(run).font(style), *fonts);
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

  /** Three rails at one pitch, the middle one heavier and lit. */
  static Element rules(SkPoint corner, float width) {
    static constexpr std::array<float, 3> kRails{{2.0f, 3.0f, 2.0f}};
    return kit::at(corner.fX, corner.fY, width, 17.0f)
        .column()
        .gap(5)
        .children(each(kRails, [width](float weight) {
          return kit::line(
              {.length = Dimension(width),
               .thickness = weight,
               .fill = Fill::color(weight > 2.0f ? kMintRuleHi : kMintRule)});
        }));
  }

  Element backplane() const {
    const SkRect frame = layout.frame();
    return box().inset(0).children(
        {kit::at(
             box()
                 .fill(Fill::none())
                 .foreground(decorations::border(7.0f, Fill::color(kOrange))),
             frame.left(), frame.top(), frame.width(), frame.height()),
         // The bus is deliberately earlier in the display list. Every module is
         // an opaque mask over it, so the route disappears cleanly at module
         // edges.
         kit::disc(layout.busCentre, layout.busRadius)
             .shape(sigil::geometry::shapes::circle())
             .fill(Fill::none())
             .foreground(
                 decorations::border(5.0f, Fill::color(kOrangeDim), 0.0f))});
  }

  Element module(int number, const char* name) const {
    const SkRect rect = layout.moduleRect(number);
    const float side = layout.moduleSide;
    const std::u8string numeral =
        number == 1 ? u8"1" : (number == 2 ? u8"2" : u8"3");
    return kit::at(box()
                       .rotate(layout.rotationFor(number))
                       .transformOrigin(pct(50), pct(50))
                       .fill(mskia::Paint::solid(kMint)),
                   rect.left(), rect.top(), side, side)
        // The module's ink is its label colour; the inner rule is drawn in it.
        .ink(kInk)
        .clip(true)
        .style(decorations::doubleBorder(
            decorations::border(6.0f, Fill::color(kOrange), 0.0f),
            decorations::border(3.0f, Fill::currentInk(), 9.0f)))
        .children(
            {text(numeral)
                 .ink({0, 0, 0, 1})
                 .blendMode(SkBlendMode::kDstOut)
                 .font(fit(evangelion::groteskBold(), numeral, 88.0f,
                           side - 48.0f))
                 .centerAt({side * 0.5f, side * layout.numberSlotY(number)}),
             text(name)
                 .font(
                     fit(evangelion::moduleLabel(), name, 31.0f, side - 48.0f))
                 .centerAt({side * 0.5f, side * layout.nameSlotY(number)})});
  }

  /** The layer's Latin is orange, stated once; the whole-style Han names its
   * own. */
  Element information() const {
    // The four blocks of ruling, by the corner each starts at.
    static constexpr std::array<SkPoint, 4> kRuled{
        {{145, 106}, {145, 241}, {920, 106}, {920, 241}}};
    static constexpr const char8_t* kReadout =
        u8"FILE:MAGI_SYS\nEXTENTION:2048\nEX_MODE:ON\nPRIORITY:A__";
    return box().inset(0).ink(kOrange).children(
        {each(kRuled, [](SkPoint corner) { return rules(corner, 375.0f); }),
         text(u8"提訴", han(u8"提訴", 83.0f, 300.0f, kOrange))
             .centerAt({332.5f, 184.0f}),
         text(u8"決議", han(u8"決議", 83.0f, 300.0f, kOrange))
             .centerAt({1107.5f, 184.0f}),
         text(u8"CODE : 132")
             .font(fit(evangelion::condensedBold(), u8"CODE : 132", 45.0f,
                       270.0f))
             .left(151.0f)
             .top(275.0f),
         // The four data lines are one passage on a 32 px pitch.
         text(kReadout)
             .font(fit(evangelion::condensedBold(), kReadout, 22.0f, 286.0f))
             .block({.leading = weave::Leading::absolute(32.0f)})
             .left(151.0f)
             .top(354.0f),
         text(u8"MAGI")
             .font(fit(evangelion::magiWordmark(), u8"MAGI", 54.0f, 230.0f))
             .centerAt({720.0f, 535.0f}),
         kit::at(
             box()
                 .fill(mskia::Paint::solid(hexColor(0x150103)))
                 .style(decorations::doubleBorder(
                     decorations::border(7.0f, Fill::color(kRed), 0.0f),
                     decorations::border(3.0f, Fill::color(kRedHot), 14.0f)))
                 .children(
                     {text(u8"審議中", han(u8"審議中", 49.0f, 205.0f, kRedHot))
                          .centerAt({137.5f, 65.0f})}),
             995.0f, 295.0f, 275.0f, 130.0f)});
  }

  Element describe() const {
    return box().inset(0).children(
        {box()
             .inset(0)
             .fill(mskia::Paint::solid(kGround))
             .filter(evangelion::crt(layout.canvasWidth, layout.canvasHeight))
             .cache(Cache::Texture)
             .key("crt")
             .children({backplane(), module(2, "BALTHASAR"),
                        module(3, "CASPER"), module(1, "MELCHIOR"),
                        information()})});
  }

  void setup(sketch::SketchContext& context) {
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

SIGIL_SKETCH(EvaMagiDeliberation, "Study · Film",
             "Evangelion MAGI deliberation — one rear bus and three "
             "rotated module instances")
