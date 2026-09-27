#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Rows.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Theme.h>
#include <sigilsketch/kit/Verdict.h>

#include <algorithm>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

using compose::Element;
using compose::Fill;
using compose::Utf8;

namespace {

/** What a drawn row is, which decides the class its verdict cell names. */
enum class Kind { Heading, Held, Failed, Reading };

bool shown(const measure::Check& row, VerdictRows rows) {
  switch (rows) {
    case VerdictRows::Every:
      return true;
    case VerdictRows::Judged:
      return row.judged();
    case VerdictRows::Failures:
      return row.judged() && !row.pass;
  }
  return true;
}

Utf8 joined(const Utf8& head, const std::string& tail) {
  std::u8string words = head.bytes();
  words.append(tail.begin(), tail.end());
  return Utf8(std::move(words));
}

}  // namespace

Element verdict(const measure::CheckTable& table, const Verdict& how) {
  const Theme& look = theme();
  std::vector<std::vector<Utf8>> cells;
  std::vector<Kind> kinds;
  std::vector<Fill> swatches;
  for (const measure::Check& row : table.rows) {
    if (!shown(row, how.rows)) continue;
    if (row.standing == measure::Standing::Heading) {
      cells.push_back({Utf8(row.label)});
      kinds.push_back(Kind::Heading);
      swatches.emplace_back();
      continue;
    }
    if (!row.judged()) {
      cells.push_back({Utf8(row.label), Utf8(row.actual), how.unjudged});
      kinds.push_back(Kind::Reading);
      swatches.emplace_back();
      continue;
    }
    cells.push_back({Utf8(row.label), Utf8(row.actual),
                     row.pass ? how.passed : joined(how.failed, row.expected)});
    kinds.push_back(row.pass ? Kind::Held : Kind::Failed);
    swatches.push_back(how.swatches ? Fill::color(row.pass ? look.palette.pass
                                                           : look.palette.fail)
                                    : Fill{});
  }
  if (how.summary) {
    const int failures = table.failures();
    cells.push_back(
        {Utf8(compose::kit::formatted("%d checks", table.checks())), Utf8(),
         failures == 0 ? Utf8(u8"all passed")
                       : Utf8(compose::kit::formatted("%d failed", failures))});
    kinds.push_back(failures == 0 ? Kind::Held : Kind::Failed);
    swatches.emplace_back();
  }

  std::vector<compose::kit::Column> columns = how.columns;
  if (columns.empty())
    columns = {{.width = 220}, {.width = 72, .figure = true}, {}};
  std::vector<std::span<const Utf8>> rows(cells.begin(), cells.end());
  compose::kit::Table specification{
      .columns = std::move(columns),
      .gap = look.spacing.labelGap,
      .rowGap = look.spacing.rowGap,
      .divider = how.ruled ? Fill::color(look.palette.rule) : Fill{},
      .swatches = swatches,
      .swatchSide = look.spacing.swatchSide};
  specification.headLine = [look](const Utf8& words) {
    return compose::document::h2(words).role(
        "h2", look.font(look.type.section, look.palette.ink));
  };
  // The label and the value keep their column's register; the verdict
  // cell and a heading name the class the sheet colours them by.
  specification.cellLine = [look, kinds](const Utf8& words,
                                         const compose::kit::Table& shape,
                                         std::size_t column,
                                         std::size_t row) -> Element {
    const Kind kind = row < kinds.size() ? kinds[row] : Kind::Reading;
    if (kind == Kind::Heading)
      return compose::document::h2(words)
          .role("h2", look.font(look.type.section, look.palette.ink))
          .styleClass("checkHeading");
    const bool figure =
        !shape.columns.empty() &&
        shape.columns[std::min(column, shape.columns.size() - 1)].figure;
    const bool verdictCell = column + 1 >= shape.columns.size();
    std::string classes = figure ? "readout" : "";
    if (verdictCell && kind != Kind::Reading)
      classes += std::string(classes.empty() ? "" : " ") +
                 (kind == Kind::Held ? "checkPass" : "checkFail");
    Element line =
        figure ? Element(compose::document::paragraph(words).role(
                     "paragraph",
                     look.font(look.type.captionLabel, look.palette.figure)))
               : Element(compose::document::caption(words).role(
                     "caption",
                     look.font(look.type.captionNote, look.palette.ash)));
    if (!classes.empty()) line.styleClass(classes);
    return line;
  };
  return compose::kit::table(rows, specification);
}

}  // namespace sigil::sketch::kit
