#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sigil::sketch::kit {

weave::TextStyle Theme::style(const Register& line, SkColor4f color) const {
  return weave::textStyle({.face = line.mono ? type.mono : type.sans,
                           .size = line.size,
                           .color = color,
                           .track = line.track});
}

weave::TextStyle Theme::sans(float size, SkColor4f color, float track) const {
  return style({.size = size, .track = track, .mono = false}, color);
}

weave::TextStyle Theme::mono(float size, SkColor4f color, float track) const {
  return style({.size = size, .track = track, .mono = true}, color);
}

compose::kit::Caption Theme::voice(float noteMeasure) const {
  return {.where = captionWhere,
          .label = style(type.captionLabel, palette.ink),
          .note = style(type.captionNote, palette.ash),
          .gap = spacing.captionGap,
          .noteGap = spacing.captionNoteGap,
          .noteMeasure = noteMeasure};
}

const Theme& houseTheme() {
  // The mono face is resolved once and held for the process, so every
  // sheet under this theme sets its calls with the same face POINTER —
  // which is what a text style, an inherited value and a memo key all
  // compare by, and what keeps a description prunable across a frame.
  static const Theme house = [] {
    Theme sheet;
    sheet.type.mono = weave::ports::face(
        {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
    return sheet;
  }();
  return house;
}

const Theme& theme() {
  const Theme* bound = sigil::core::env::inherited<Theme>();
  return bound != nullptr ? *bound : houseTheme();
}

}  // namespace sigil::sketch::kit
