#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Rows.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

namespace {

/** The patch a row carries before its first cell. */
Element mark(const SurfacePaint& paint, float side, float corners) {
  Element patch = box().width(Dimension(side)).height(Dimension(side));
  paint.apply(patch);
  patch.flexShrink(0);
  if (corners > 0.0f) patch.borderRadius(Corners{corners});
  return patch;
}

}  // namespace

Element reading(const Reading& one, const Rows& how) {
  // A row's own ink stands over whatever its lines' classes name, field
  // by field, so a lit row keeps the register it was set in.
  const auto lit = [&one](Element line) {
    if (one.ink) line.font({.color = *one.ink});
    return line;
  };
  Element row =
      box().row().alignItems(Align::Center).gap(Dimension(how.labelGap));
  if (how.measure > 0.0f) row.width(Dimension(how.measure));
  if (!one.swatch.none())
    row.children({mark(one.swatch, how.swatchSide, how.swatchCorners)});
  if (!one.name.empty()) {
    Element name =
        lit(how.nameLine ? how.nameLine(one.name, how) : captionNote(one.name));
    if (how.nameMeasure > 0.0f) name.width(Dimension(how.nameMeasure));
    row.children({std::move(name)});
  }
  // With a measure the space between is what grows, which is what puts
  // every figure on one edge however long the names are.
  if (how.measure > 0.0f) row.children({box().flexGrow(1)});
  if (!one.value.empty())
    row.children({lit(how.valueLine ? how.valueLine(one.value, how)
                                    : figure(one.value))});
  if (!one.note.empty())
    row.children({lit(how.noteLine ? how.noteLine(one.note, how)
                                   : captionNote(one.note))});
  return row;
}

Element readout(std::span<const Reading> rows, const Rows& how) {
  Element column = box().column().gap(Dimension(how.gap));
  if (how.measure > 0.0f) column.width(Dimension(how.measure));
  bool first = true;
  for (const Reading& one : rows) {
    if (!first && how.divider.kind != Fill::Kind::None)
      column.children(
          {line({.thickness = how.dividerWidth, .fill = how.divider})});
    first = false;
    column.children({reading(one, how)});
  }
  return column;
}

Element table(std::span<const std::span<const Utf8>> rows, const Table& how) {
  Element column = box().column().gap(Dimension(how.rowGap));
  const auto rule = [&] {
    return line({.thickness = how.dividerWidth, .fill = how.divider});
  };
  // A cell past the last column is set in that column's class at its own
  // width, which is the shape a table whose final column is prose has.
  const auto specification = [&](size_t index) {
    return how.columns.empty()
               ? Column{}
               : how.columns[std::min(index, how.columns.size() - 1)];
  };
  const auto sized = [&](Element cell, size_t index) {
    const Column column_ = specification(index);
    if (column_.width > 0.0f && index < how.columns.size())
      cell.width(Dimension(column_.width));
    return cell;
  };

  const bool headed = std::ranges::any_of(
      how.columns, [](const Column& one) { return !one.head.empty(); });
  const bool marked = std::ranges::any_of(
      how.swatches.first(std::min(rows.size(), how.swatches.size())),
      [](const SurfacePaint& paint) { return !paint.none(); });
  if (headed) {
    Element head =
        box().row().alignItems(Align::Center).gap(Dimension(how.gap));
    if (marked) head.children({box().width(how.swatchSide).flexShrink(0)});
    for (size_t index = 0; index < how.columns.size(); ++index) {
      const Utf8& words = how.columns[index].head;
      head.children({sized(
          how.headLine ? how.headLine(words, how) : section(words), index)});
    }
    column.children({std::move(head)});
    if (how.headRuled) column.children({rule()});
  }

  bool first = true;
  for (size_t index = 0; index < rows.size(); ++index) {
    if (!first && how.divider.kind != Fill::Kind::None)
      column.children({rule()});
    first = false;
    Element row = box().row().alignItems(Align::Center).gap(Dimension(how.gap));
    if (index < how.keys.size() && !how.keys[index].empty())
      row.key(how.keys[index]);
    if (marked)
      row.children({mark(
          index < how.swatches.size() ? how.swatches[index] : SurfacePaint{},
          how.swatchSide, how.swatchCorners)});
    const std::span<const Utf8> cells = rows[index];
    for (size_t at = 0; at < cells.size(); ++at) {
      Element cell = how.cellLine
                         ? how.cellLine(cells[at], how, at, index, cells)
                         : (specification(at).figure ? figure(cells[at])
                                                     : captionNote(cells[at]));
      row.children({sized(std::move(cell), at)});
    }
    column.children({std::move(row)});
  }
  return column;
}

Element table(std::span<const Utf8> cells, const Table& how) {
  const size_t across = std::max<size_t>(how.columns.size(), 1);
  std::vector<std::span<const Utf8>> rows;
  for (size_t at = 0; at < cells.size(); at += across)
    rows.push_back(cells.subspan(at, std::min(across, cells.size() - at)));
  return table(std::span<const std::span<const Utf8>>(rows), how);
}

Element bars(std::span<const Utf8> labels, std::span<const double> values,
             const Bars& how) {
  double largest = how.largest;
  if (largest <= 0.0)
    for (const double value : values) largest = std::max(largest, value);
  Element column = box().column().gap(Dimension(how.rowGap));
  for (size_t index = 0; index < values.size(); ++index) {
    Element row = box().row().alignItems(Align::Center).gap(Dimension(how.gap));
    // A row's own ink stands over the bar's paint and over whatever its
    // two lines' classes name, so a lit row keeps the registers it was
    // set in.
    const std::optional<SkColor4f> ink =
        index < how.inks.size() ? std::optional(how.inks[index]) : std::nullopt;
    const auto lit = [&ink](Element line) {
      if (ink) line.font({.color = *ink});
      return line;
    };
    if (index < labels.size()) {
      Element label = lit(how.labelLine ? how.labelLine(labels[index], how)
                                        : captionNote(labels[index]));
      if (how.labelMeasure > 0.0f) label.width(Dimension(how.labelMeasure));
      row.children({std::move(label)});
    }
    const float run =
        largest > 0.0 ? (float)((double)how.length * values[index] / largest)
                      : 0.0f;
    Element bar = box().width(Dimension(run)).height(Dimension(how.barHeight));
    if (ink)
      bar.fill(Fill::color(*ink));
    else
      (how.bar.none() ? SurfacePaint(Fill::currentInk()) : how.bar).apply(bar);
    if (how.rest.none()) {
      row.children({std::move(bar)});
    } else {
      Element track =
          box().width(Dimension(how.length)).height(Dimension(how.barHeight));
      how.rest.apply(track);
      row.children({std::move(track.children({std::move(bar)}))});
    }
    row.children(
        {lit(how.figureLine ? how.figureLine(values[index], how)
                            : figure(formatted("%.0f", values[index])))});
    column.children({std::move(row)});
  }
  return column;
}

}  // namespace sigil::compose::kit
