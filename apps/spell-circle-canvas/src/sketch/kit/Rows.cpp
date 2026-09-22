#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Rows.h>
#include <sigildata/table/Table.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilsketch/kit/Rows.h>

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

using compose::Element;
using compose::Fill;
using compose::Utf8;

namespace {

/** The two registers a reading is set in, as the parts the arrangement
 *  writes its lines with: the name and the note in the theme's quiet
 *  register, the figure in the register a CALL is set in, in the figure
 *  colour. */
struct Registers {
  weave::Type quiet;
  weave::Type number;
};

Registers registers(const Theme& look) {
  return {look.font(look.type.captionNote, look.palette.ash),
          look.font(look.type.captionLabel, look.palette.figure)};
}

compose::kit::Rows arrangement(const Readout& how, const Theme& look) {
  compose::kit::Rows rows{
      .measure = how.measure,
      .nameMeasure = how.nameMeasure,
      .gap = look.spacing.rowGap,
      .labelGap = look.spacing.labelGap,
      .divider = how.ruled ? Fill::color(look.palette.rule) : Fill{},
      .swatchSide = how.swatchSide.value_or(look.spacing.swatchSide),
      .swatchCorners = how.swatchCorners};
  const Registers set = registers(look);
  rows.nameLine = [quiet = set.quiet](const Utf8& words) {
    return compose::document::caption(words).role(
        weave::rule("caption").font(quiet));
  };
  rows.valueLine = [number = set.number](const Utf8& words) {
    return compose::document::paragraph(words)
        .role(weave::rule("paragraph").font(number))
        .styleClass("readout");
  };
  rows.noteLine = rows.nameLine;
  return rows;
}

}  // namespace

compose::Element labelRow(const Reading& reading, const Readout& how) {
  return compose::kit::reading({.name = reading.name,
                                .value = reading.value,
                                .note = reading.note,
                                .swatch = reading.swatch,
                                .ink = reading.ink},
                               arrangement(how, theme()));
}

compose::Element readout(std::vector<Reading> rows, const Readout& how) {
  std::vector<compose::kit::Reading> readings;
  readings.reserve(rows.size());
  for (Reading& one : rows)
    readings.push_back({.name = std::move(one.name),
                        .value = std::move(one.value),
                        .note = std::move(one.note),
                        .swatch = std::move(one.swatch),
                        .ink = one.ink});
  return compose::kit::readout(readings, arrangement(how, theme()));
}

compose::Element table(std::vector<Row> rows, const Table& how) {
  const Theme& look = theme();
  std::vector<compose::kit::Column> columns;
  columns.reserve(how.columns.size());
  for (const Column& one : how.columns)
    columns.push_back(
        {.head = one.head, .width = one.width, .figure = one.figure});
  // The rows are held by value for the length of this call, so the spans
  // the arrangement reads stand on them.
  std::vector<std::span<const Utf8>> cells;
  std::vector<compose::SurfacePaint> swatches;
  std::vector<std::string> keys;
  std::vector<std::optional<material::Color>> inks;
  cells.reserve(rows.size());
  swatches.reserve(rows.size());
  keys.reserve(rows.size());
  inks.reserve(rows.size());
  for (const Row& row : rows) {
    cells.emplace_back(row.cells);
    swatches.push_back(row.swatch);
    keys.push_back(row.key);
    inks.push_back(row.ink);
  }
  compose::kit::Table specification{
      .columns = std::move(columns),
      .gap = how.gap.value_or(look.spacing.labelGap),
      .rowGap = look.spacing.rowGap,
      .divider = how.ruled ? Fill::color(look.palette.rule) : Fill{},
      .headRuled = how.headRuled,
      .swatches = swatches,
      .swatchSide = how.swatchSide.value_or(look.spacing.swatchSide),
      .swatchCorners = how.swatchCorners,
      .keys = keys};
  specification.headLine = [look](const Utf8& words) {
    return compose::document::h2(words).role(
        weave::rule("h2").font(look.font(look.type.section, look.palette.ink)));
  };
  // The cell names four parameters, so a row that states an ink is set in
  // that colour and keeps the register its column decides.
  specification.cellLine = [look, inks](const Utf8& words,
                                        const compose::kit::Table& shape,
                                        std::size_t column, std::size_t row) {
    const bool figure =
        !shape.columns.empty() &&
        shape.columns[std::min(column, shape.columns.size() - 1)].figure;
    const Registers set = registers(look);
    Element line = figure ? compose::document::paragraph(words)
                                .role(weave::rule("paragraph").font(set.number))
                                .styleClass("readout")
                          : compose::document::caption(words).role(
                                weave::rule("caption").font(set.quiet));
    if (row < inks.size() && inks[row]) line.ink(*inks[row]);
    return line;
  };
  return compose::kit::table(cells, specification);
}

compose::Element bars(std::span<const compose::Utf8> labels,
                      std::span<const double> values, const Bars& how) {
  const Theme& look = theme();
  compose::kit::Bars specification{
      .length = how.length,
      .largest = how.largest,
      .labelMeasure = how.labelMeasure,
      .barHeight = how.barHeight.value_or(look.spacing.barHeight),
      .gap = how.gap.value_or(look.spacing.labelGap),
      .rowGap = how.rowGap.value_or(look.spacing.rowGap),
      .bar = how.bar.value_or(Fill::color(look.palette.figure)),
      .rest = how.rest.value_or(
          Fill::color(sigil::material::withAlpha(look.palette.figure, 0.25f))),
      .inks = how.inks};
  const Registers set = registers(look);
  specification.labelLine = [quiet = set.quiet](const Utf8& words) {
    return compose::document::caption(words).role(
        weave::rule("caption").font(quiet));
  };
  specification.figureLine = [number = set.number](double value) {
    return compose::document::paragraph(compose::kit::formatted("%.0f", value))
        .role(weave::rule("paragraph").font(number))
        .styleClass("readout");
  };
  return compose::kit::bars(labels, values, specification);
}

compose::Element bars(const data::Table& table, std::string_view labels,
                      std::string_view values, const Bars& how) {
  const std::span<const std::string> names = table.column<std::string>(labels);
  std::vector<compose::Utf8> words;
  words.reserve(names.size());
  for (const std::string& one : names) words.emplace_back(one);
  return bars(words, table.column<double>(values), how);
}

}  // namespace sigil::sketch::kit
