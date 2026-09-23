#include <sigilmaterial/color/Color.h>
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

weave::Type Theme::font(const Register& line, material::Color color) const {
  weave::Type type = font(line);
  type.color = color;
  return type;
}

compose::StyleSheet Theme::styleSheet() const {
  using compose::rule;
  const auto ink = [](material::Color colour) {
    return weave::Type{.color = colour};
  };
  return compose::StyleSheet{
      // A ROLE RULE CARRIES ITS WHOLE LOOK, colour included, as a CSS rule
      // does: the palette's ink for the lines that NAME something — the
      // page's title, the call over a cell — and its ash for the lines that
      // qualify one. A label and a caption are also classes a sheet names.
      rule("h1").font(font(type.title, palette.ink)),
      rule("lead").font(font(type.subtitle, palette.ash)),
      rule("footer").font(font(type.footer, palette.ash)),
      rule("label, .label").font(font(type.captionLabel, palette.ink)),
      rule("caption, .caption").font(font(type.captionNote, palette.ash)),
      // The two registers a sheet sets INSIDE its content name no colour,
      // so each is painted in the ink in force wherever it is read — which
      // is what a section standing on a panel of its own asks for.
      rule("eyebrow").font(font(type.eyebrow)),
      rule("h2").font(font(type.section)),
      // A MEASURED FIGURE is the one thing on the sheet that is neither
      // type nor furniture, so it is the class a CALL is set in — the
      // digits of one width — in the palette's figure colour, wherever it
      // stands: a readout's value, a figure column's cells, a reading over
      // a picture.
      rule(".readout").font(font(type.captionLabel, palette.figure)),
      // THE PARTS A CHART DRAWS. The furniture is quiet — the axis line and
      // its ticks in the ash a remark is set in, the rules across the field in
      // the hairline colour — and the reading is the figure colour, which is
      // the one thing on the sheet a reader's eye is meant to find: a curve, a
      // datum's own mark, the band it is drawn as, and the band's fill under a
      // curve dimmed so the curve still reads over it. Only `tick` and `label`
      // carry type, because only they set words; the rest name a colour alone
      // and a recording paints in it as the ink in force.
      rule(".plotAxis").font(ink(palette.ash)),
      rule(".plotTick").font(font(type.captionLabel, palette.ash)),
      rule(".plotRule").font(ink(palette.rule)),
      rule(".plotTrace").font(ink(palette.figure)),
      rule(".plotArea")
          .font(ink(sigil::material::withAlpha(palette.figure, 0.25f))),
      rule(".plotMark").font(ink(palette.figure)),
      rule(".plotBar").font(ink(palette.figure)),
      rule(".plotLabel").font(font(type.captionLabel, palette.ink)),
  };
}

Provide::Provide(Theme look) : m_look(std::move(look)) {}

weave::TextStyle Theme::style(const Register& line,
                              material::Color color) const {
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
  return style(line, material::Color{0, 0, 0, 0});
}

weave::TextStyle Theme::sans(float size, material::Color color,
                             float track) const {
  return style({.size = size, .track = track, .mono = false}, color);
}

weave::TextStyle Theme::mono(float size, material::Color color,
                             float track) const {
  return style({.size = size, .track = track, .mono = true}, color);
}

compose::kit::Caption Theme::voice(float noteMeasure) const {
  // No type: a cell's two lines have document roles `label` and
  // `caption`, styled by the sheet where the cell lands — the page
  // root's, or the one a sketch without a page states on its own root.
  return {.where = captionWhere,
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

const Theme& studyTheme() {
  static const Theme study = [] {
    Theme sheet = houseTheme();
    sheet.palette.ash = {0.65f, 0.67f, 0.72f, 1};
    sheet.type.sans = houseFace(Voice::Interface);
    sheet.type.title = {.size = 30, .track = -0.4f};
    sheet.type.subtitle = {.size = 12, .track = 0};
    sheet.type.footer = {.size = 11, .track = 0};
    sheet.type.captionLabel = {.size = 11, .track = 0.5f};
    sheet.type.captionNote = {.size = 11, .track = 0};
    sheet.spacing.marginX = 40;
    sheet.spacing.marginTop = 32;
    sheet.spacing.marginBottom = 24;
    sheet.spacing.subtitleGap = 8;
    sheet.spacing.contentGap = 26;
    sheet.spacing.captionGap = 10;
    return sheet;
  }();
  return study;
}

const Theme& specimenTheme() {
  static const Theme specimen = [] {
    Theme sheet = houseTheme();
    sheet.palette.ash = {0.65f, 0.67f, 0.72f, 1};
    sheet.type.sans = houseFace(Voice::Interface);
    sheet.type.title = {.size = 22, .track = -0.2f};
    sheet.type.subtitle = {.size = 11.5f, .track = 0};
    sheet.type.footer = {.size = 11, .track = 0};
    sheet.type.captionNote = {.size = 11, .track = 0};
    return sheet;
  }();
  return specimen;
}

const Theme& theme() {
  const Theme* bound = sigil::core::environment::inherited<Theme>();
  return bound != nullptr ? *bound : houseTheme();
}

}  // namespace sigil::sketch::kit
