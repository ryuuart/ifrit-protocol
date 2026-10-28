#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Plate.h>
#include <sigilsketch/kit/Console.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

namespace {

/** Whether @p name is one of the levels @p panel colours. */
bool coloured(const Console& panel, const std::string& name) {
  return std::any_of(panel.levels.begin(), panel.levels.end(),
                     [&](const auto& level) { return level.first == name; });
}

}  // namespace

compose::Element console(const Console& panel) {
  using compose::var;
  const compose::Dimension rowGap = var("spacing-row-gap");
  // Each row is a text leaf that inherits the console's voice, and a row
  // naming a level carries that name as its class, which the console's own
  // rules colour.
  const auto column = [&](const compose::feed::TextRing& ring) {
    return compose::feed::feed(
               ring, {.visible = panel.visible},
               [&](const compose::feed::TextRow& row) {
                 compose::Text line = compose::text(row.text);
                 if (!row.style.empty() && coloured(panel, row.style))
                   line.styleClass(row.style);
                 return compose::Element(std::move(line));
               })
        .gap(rowGap);
  };
  const std::size_t per = (std::size_t)std::max(panel.stacked, 1);
  std::vector<compose::Element> columns;
  for (std::size_t i = 0; i < panel.feeds.size(); i += per) {
    if (per == 1) {
      if (panel.feeds[i]) columns.push_back(column(*panel.feeds[i]));
      continue;
    }
    compose::Element stack = compose::box().column().gap(rowGap);
    const std::size_t end = std::min(i + per, panel.feeds.size());
    for (std::size_t j = i; j < end; ++j)
      if (panel.feeds[j]) stack.children({column(*panel.feeds[j]).flexGrow(1)});
    columns.push_back(std::move(stack));
  }
  const compose::Fill border = panel.border.value_or(var("palette-rule"));
  compose::Element plate = compose::kit::plate(
      {.columns = std::move(columns),
       .paddingHorizontal = var("spacing-panel-padding"),
       .paddingVertical = var("spacing-plate-padding-vertical"),
       .gap = var("spacing-label-gap"),
       .border = border,
       .divider = border});
  plate.fill(panel.ground.value_or(var("palette-cell-ground")));
  // THE VOICE is the sheet's `.console` rule — the face a call is set in,
  // at the caption-label size, in the ink — with the size and the ink the
  // panel states laid over it, and each level's colour a rule of its own.
  plate.styleClass("console");
  if (panel.size) plate.font({.size = *panel.size});
  if (panel.ink) plate.ink(*panel.ink);
  if (!panel.levels.empty()) {
    compose::StyleSheet levels;
    for (const auto& [name, color] : panel.levels)
      levels = levels + compose::StyleSheet{
                            compose::rule("." + name).font({.ink = color})};
    plate.applyStyleSheet(std::move(levels));
  }
  return plate;
}

}  // namespace sigil::sketch::kit
