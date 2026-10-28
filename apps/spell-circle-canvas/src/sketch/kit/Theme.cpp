#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include "sigilweave/advanced/Skia.h"

namespace sigil::sketch::kit {

weave::Type Theme::font(const Register& line) const {
  // The face is always STATED: a register set in the theme's sans, which
  // the house theme leaves as the font context's default family, says so,
  // so a caption under an ancestor that named a face is still set in the
  // register's own.
  weave::Type partial{
      .face = line.face ? line.face : (line.mono ? type.mono : type.sans),
      .size = line.size,
      .letterSpacing = line.letterSpacing};
  if (!line.features.empty()) partial.features = line.features;
  return partial;
}

weave::Type Theme::font(const Register& line, material::Color color) const {
  weave::Type partial = font(line);
  partial.ink = color;
  return partial;
}

namespace {

/** THE RULE @p selector NAMES, SET IN @p line, written with the CSS verbs:
 *  the face, the size, the letter spacing and — where the register asks
 *  for any — the OpenType features. */
compose::Rule registerRule(std::string selector, const weave::Type& line) {
  compose::Rule written = compose::rule(std::move(selector))
                              .fontFamily(line.face.value_or(nullptr))
                              .fontSize(line.size->value)
                              .letterSpacing(line.letterSpacing->value);
  if (line.features) written.fontFeatureSettings(*line.features);
  return written;
}

}  // namespace

compose::StyleSheet Theme::styleSheet() const {
  using compose::rule;
  // A register that names its own ink is set in it, over the colour its
  // role names; a role that names none paints in the ink in force.
  const auto typeRule = [this](std::string selector, const Register& line,
                               std::optional<material::Color> roleInk = {}) {
    compose::Rule written = registerRule(std::move(selector), font(line));
    if (const std::optional<material::Color> colour =
            line.ink ? line.ink : roleInk)
      written.ink(*colour);
    return written;
  };
  return compose::StyleSheet{
             // A ROLE RULE CARRIES ITS WHOLE LOOK, colour included, as a CSS
             // rule does: the palette's ink for the lines that NAME something
             // — the page's title, the call over a cell — and its ash for the
             // lines that qualify one. A label and a caption are also classes
             // a sheet names. A PAGE'S RUNNING TEXT, which every leaf under a
             // page that states no style of its own is set in, is the remark
             // register in the ink.
             typeRule("page", type.captionNote, palette.ink),
             typeRule("h1", type.title, palette.ink),
             typeRule("lead", type.subtitle, palette.ash),
             typeRule("footer", type.footer, palette.ash),
             typeRule("label, .label", type.label.value_or(type.captionLabel),
                      palette.ink),
             typeRule("caption, .caption", type.captionNote, palette.ash),
             // The two registers a sheet sets INSIDE its content name no
             // colour, so each is painted in the ink in force wherever it is
             // read — which is what a section standing on a panel of its own
             // asks for.
             typeRule("eyebrow", type.eyebrow),
             typeRule("h2", type.section),
             typeRule(".control", type.control, palette.ash),
             // A CONSOLE'S ROWS: the face a call is set in at the
             // caption-label size, untracked, in the ink.
             typeRule(".console",
                      {.size = type.captionLabel.size, .mono = true},
                      palette.ink),
             // A MEASURED FIGURE is the one thing on the sheet that is neither
             // type nor furniture, so it is the class a CALL is set in — the
             // digits of one width — in the palette's figure colour, wherever
             // it stands: a readout's value, a figure column's cells, a
             // reading over a picture.
             typeRule(".readout", type.captionLabel, palette.figure),
             // THE PARTS A CHART DRAWS. The furniture is quiet — the axis line
             // and its ticks in the ash a remark is set in, the rules across
             // the field in the hairline colour — and the reading is the
             // figure colour, which is the one thing on the sheet a reader's
             // eye is meant to find: a curve, a datum's own mark, the band it
             // is drawn as, and the band's fill under a curve dimmed so the
             // curve still reads over it. Only `tick` and `label` carry type,
             // because only they set words; the rest name a colour alone and
             // a recording paints in it as the ink in force.
             rule(".plotAxis").ink(palette.ash),
             typeRule(".plotTick", type.captionLabel, palette.ash),
             rule(".plotRule").ink(palette.rule),
             rule(".plotTrace").ink(palette.figure),
             rule(".plotArea")
                 .ink(sigil::material::withAlpha(palette.figure, 0.25f)),
             rule(".plotMark").ink(palette.figure),
             rule(".plotBar").ink(palette.figure),
             typeRule(".plotLabel", type.captionLabel, palette.ink),
             // A VERDICT is the one judgement on the sheet: the colour says
             // whether the check held, and the heading over a run of checks
             // is set as a section in the ink.
             rule(".checkPass").ink(palette.pass),
             rule(".checkFail").ink(palette.fail),
             typeRule(".checkHeading", type.section, palette.ink),
         } +
         properties();
}

compose::StyleSheet Theme::properties() const {
  using compose::rule;
  return compose::StyleSheet{
      // THE LOOK'S NUMBERS AND COLOURS AS CUSTOM PROPERTIES on the node the
      // sheet stands on, so a component reads one where it uses it — a
      // length written `var("spacing-row-gap")`, a fill written
      // `var("palette-figure")` — and the cascade carries it down. A value
      // a component scales is its own property, computed here.
      rule(":root")
          .var("spacing-margin-horizontal", spacing.marginHorizontal)
          .var("spacing-margin-top", spacing.marginTop)
          .var("spacing-margin-bottom", spacing.marginBottom)
          .var("spacing-subtitle-gap", spacing.subtitleGap)
          .var("spacing-content-gap", spacing.contentGap)
          .var("spacing-rule-gap", spacing.contentGap * 0.5f)
          .var("spacing-caption-gap", spacing.captionGap)
          .var("spacing-caption-note-gap", spacing.captionNoteGap)
          .var("spacing-cell-gap", spacing.cellGap)
          .var("spacing-comparison-gap", spacing.comparisonGap)
          .var("spacing-section-gap", spacing.sectionGap)
          .var("spacing-well-padding", spacing.wellPadding)
          .var("spacing-row-gap", spacing.rowGap)
          .var("spacing-label-gap", spacing.labelGap)
          .var("spacing-chip-padding-horizontal", spacing.chipPaddingX)
          .var("spacing-chip-padding-vertical", spacing.chipPaddingY)
          .var("spacing-panel-padding", spacing.panelPadding)
          .var("spacing-plate-padding-vertical", spacing.panelPadding * 0.6f)
          .var("spacing-swatch-side", spacing.swatchSide)
          .var("spacing-bar-height", spacing.barHeight)
          .var("spacing-ticker-gap", spacing.tickerGap)
          .var("spacing-panel-corners", spacing.panelCorners)
          .var("spacing-screen-corners", spacing.screenCorners)
          .var("spacing-bezel", spacing.bezel)
          .var("spacing-chip-corners", spacing.chipCorners)
          .var("spacing-tick-reach", spacing.tickReach)
          .var("palette-ground", palette.ground)
          .var("palette-cell-ground", palette.cellGround)
          .var("palette-ink", palette.ink)
          .var("palette-ash", palette.ash)
          .var("palette-rule", palette.rule)
          .var("palette-figure", palette.figure)
          .var("palette-pass", palette.pass)
          .var("palette-fail", palette.fail),
  };
}

weave::TextStyle Theme::style(const Register& line,
                              material::Color color) const {
  return weave::textStyle(
      {.face = line.face ? line.face : (line.mono ? type.mono : type.sans),
       .size = line.size,
       .ink = color,
       .letterSpacing = line.letterSpacing,
       .features = line.features.empty() ? std::nullopt
                                         : std::optional{line.features}});
}

weave::TextStyle Theme::style(const Register& line,
                              const compose::Fill& ink) const {
  switch (ink.kind) {
    case compose::Fill::Kind::Color:
      return style(line, ink.colorValue);
    case compose::Fill::Kind::Paint: {
      const material::Material* held = ink.material();
      if (held == nullptr) break;
      const material::Paint lowered = material::skia::paint(*held);
      if (lowered.isSolid()) return style(line, lowered.solidColor());
      // The glyphs are painted through the shader itself; the colour
      // under it only has to be opaque for the shader to show.
      weave::TextStyle word = style(line, SkColors::kWhite);
      word.paint.foreground.setShader(material::skia::staticShader(lowered));
      return word;
    }
    case compose::Fill::Kind::None:
      break;
  }
  return style(line, material::Color{0, 0, 0, 0});
}

weave::TextStyle Theme::sans(float size, material::Color color,
                             float letterSpacing) const {
  return style({.size = size, .letterSpacing = letterSpacing, .mono = false},
               color);
}

weave::TextStyle Theme::mono(float size, material::Color color,
                             float letterSpacing) const {
  return style({.size = size, .letterSpacing = letterSpacing, .mono = true},
               color);
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

weave::Face houseFace(Voice voice, int weight, weave::FaceSlant slant) {
  switch (voice) {
    case Voice::Book:
      return weave::ports::face({"Hoefler Text", "Baskerville"}, weight, slant);
    case Voice::Terminal:
      return weave::ports::face({"Menlo", "Courier New"}, weight, slant);
    case Voice::Serif:
      return weave::ports::face(
          {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"}, weight,
          slant);
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
    sheet.type.title = {.size = 30, .letterSpacing = -0.4f};
    sheet.type.subtitle = {.size = 12, .letterSpacing = 0};
    sheet.type.footer = {.size = 11, .letterSpacing = 0};
    sheet.type.captionLabel = {.size = 11, .letterSpacing = 0.5f};
    sheet.type.captionNote = {.size = 11, .letterSpacing = 0};
    sheet.spacing.marginHorizontal = 40;
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
    sheet.type.title = {.size = 22, .letterSpacing = -0.2f};
    sheet.type.subtitle = {.size = 11.5f, .letterSpacing = 0};
    sheet.type.footer = {.size = 11, .letterSpacing = 0};
    sheet.type.captionNote = {.size = 11, .letterSpacing = 0};
    return sheet;
  }();
  return specimen;
}

const Theme& featureTheme(Density density) {
  const auto make = [](Theme sheet) {
    sheet.palette = {.ground = {0.055f, 0.062f, 0.075f, 1},
                     .cellGround = {0.09f, 0.102f, 0.12f, 1},
                     .ink = {0.93f, 0.916f, 0.878f, 1},
                     .ash = {0.65f, 0.68f, 0.73f, 1},
                     .rule = {0.20f, 0.23f, 0.28f, 1},
                     .figure = {0.90f, 0.83f, 0.68f, 1}};
    sheet.type.sans = houseFace(Voice::Interface);
    sheet.type.mono = houseFace(Voice::Terminal);
    sheet.type.captionLabel = {.size = 11, .letterSpacing = 0.5f};
    sheet.type.control = {.size = 11, .mono = true};
    return sheet;
  };
  static const Theme compact = make(specimenTheme());
  static const Theme spacious = make(studyTheme());
  return density == Density::Spacious ? spacious : compact;
}

const Theme& darkTheme() {
  static const Theme dark = [] {
    Theme sheet = studyTheme();
    // The colours as the eight-bit words they are chosen in.
    const auto word = [](int red, int green, int blue) {
      return material::Color{red / 255.0f, green / 255.0f, blue / 255.0f, 1};
    };
    sheet.palette = {.ground = word(0x10, 0x12, 0x14),
                     .cellGround = word(0x1b, 0x1f, 0x24),
                     .ink = word(0xec, 0xea, 0xe4),
                     .ash = word(0x8b, 0x92, 0x9b),
                     .rule = word(0x34, 0x3b, 0x44),
                     .figure = word(0xff, 0x6a, 0x3d)};
    sheet.type.sans = houseFace(Voice::Interface);
    sheet.type.mono = houseFace(Voice::Terminal);
    sheet.type.label = Register{
        .size = 12, .letterSpacing = 2, .mono = true, .ink = sheet.palette.ash};
    return sheet;
  }();
  return dark;
}

}  // namespace sigil::sketch::kit
