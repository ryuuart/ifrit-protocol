#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

namespace sigil::sketch::kit {

weave::Type Theme::font(const Register& line) const {
  // The face is always STATED: a register set in the theme's sans, which
  // the house theme leaves as the font context's default family, says so,
  // so a caption under an ancestor that named a face is still set in the
  // register's own.
  return {.face = line.face ? line.face : (line.mono ? type.mono : type.sans),
          .size = line.size,
          .track = line.track};
}

weave::Type Theme::font(const Register& line, SkColor4f color) const {
  weave::Type type = font(line);
  type.color = color;
  return type;
}

weave::StyleSheet Theme::styleSheet() const {
  weave::StyleSheet classes;
  // A CLASS CARRIES ITS WHOLE LOOK, colour included, as a CSS class does:
  // the palette's ink for the lines that NAME something — the page's title,
  // the call over a cell — and its ash for the lines that qualify one.
  classes.set("title", font(type.title, palette.ink));
  classes.set("subtitle", font(type.subtitle, palette.ash));
  classes.set("footer", font(type.footer, palette.ash));
  classes.set("captionLabel", font(type.captionLabel, palette.ink));
  classes.set("captionNote", font(type.captionNote, palette.ash));
  // The two registers a sheet sets INSIDE its content name no colour, so
  // each is painted in the ink in force wherever it is read — which is
  // what a section standing on a panel of its own asks for.
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
  // No type: a cell's two lines are set in the classes `captionLabel` and
  // `captionNote`, which `styleSheet()` registers.
  return {.where = captionWhere,
          .gap = spacing.captionGap,
          .noteGap = spacing.captionNoteGap,
          .noteMeasure = noteMeasure,
          .styles = styleSheet()};
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
