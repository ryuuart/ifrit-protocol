#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sigil::sketch::kit {

weave::Type Theme::font(const Register& line) const {
  return {.face = line.face ? line.face : (line.mono ? type.mono : type.sans),
          .size = line.size,
          .track = line.track};
}

weave::StyleSheet Theme::styleSheet() const {
  weave::StyleSheet classes;
  classes.set("title", font(type.title));
  classes.set("subtitle", font(type.subtitle));
  classes.set("footer", font(type.footer));
  classes.set("captionLabel", font(type.captionLabel));
  classes.set("captionNote", font(type.captionNote));
  classes.set("eyebrow", font(type.eyebrow));
  classes.set("section", font(type.section));
  return classes;
}

Provide::Provide(Theme look) : Provide(look, look.styleSheet()) {}

Provide::Provide(Theme look, weave::StyleSheet classes)
    : m_look(std::move(look)), m_classes(std::move(classes)) {}

weave::TextStyle Theme::style(const Register& line, SkColor4f color) const {
  return weave::textStyle(
      {.face = line.face ? line.face : (line.mono ? type.mono : type.sans),
       .size = line.size,
       .color = color,
       .track = line.track});
}

weave::TextStyle Theme::style(const Register& line,
                              const compose::Fill& ink) const {
  switch (ink.kind) {
    case compose::Fill::Kind::Color:
      return style(line, ink.colorValue);
    case compose::Fill::Kind::Shader: {
      // The glyphs are painted through the shader itself; the colour
      // under it only has to be opaque for the shader to show.
      weave::TextStyle word = style(line, SkColors::kWhite);
      word.paint.foreground.setShader(ink.shaderValue);
      return word;
    }
    case compose::Fill::Kind::None:
      break;
  }
  return style(line, SkColor4f{0, 0, 0, 0});
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

sk_sp<SkTypeface> houseFace(Voice voice, int weight, SkFontStyle::Slant slant) {
  switch (voice) {
    case Voice::Book:
      return weave::ports::face({"Hoefler Text", "Baskerville"}, weight, slant);
    case Voice::Terminal:
      return weave::ports::face({"Menlo", "Courier New"}, weight, slant);
    case Voice::Interface:
      break;
  }
  return weave::ports::face({".SF NS", "SF Pro", "Helvetica Neue"}, weight,
                            slant);
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
  const Theme* bound = sigil::core::environment::inherited<Theme>();
  return bound != nullptr ? *bound : houseTheme();
}

}  // namespace sigil::sketch::kit
