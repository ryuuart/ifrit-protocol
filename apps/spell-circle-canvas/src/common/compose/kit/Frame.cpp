#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/kit/Frame.h>

#include <string_view>
#include <utility>

namespace sigil::compose::kit {

Element line(const Line& mark) {
  const bool open = mark.length.unit == Dimension::Unit::Auto;
  const Fill ink =
      mark.fill.kind == Fill::Kind::None ? Fill::currentInk() : mark.fill;
  // A PAIR IS ONE NODE, NOT TWO LINES. Its rails share one route, so the
  // companion's dashes are measured along the same curve the heavy rail is
  // and cannot drift off it; the node is as deep as both rails and the gap
  // between them, and its route runs along the FIRST.
  const float across = mark.pair ? mark.thickness * 0.5f : 0.0f;
  const float depth =
      mark.pair ? mark.thickness + mark.pair->gap + mark.pair->thickness
                : mark.thickness;

  Element rule = box();
  if (mark.column) {
    rule.width(Dimension(depth));
    if (!open) rule.height(mark.length);
    if (mark.inset != 0.0f) rule.margin(Dimension(0), Dimension(mark.inset));
  } else {
    rule.height(Dimension(depth));
    if (!open) rule.width(mark.length);
    if (mark.inset != 0.0f) rule.margin(Dimension(mark.inset), Dimension(0));
  }
  if (open) rule.alignSelf(Align::Stretch);
  if (!mark.pair) return std::move(rule.fill(ink));

  const bool column = mark.column;
  const Line::Companion& second = *mark.pair;
  // Left of travel is the convention every offset in this tree shares, so
  // the companion stands on the far side of the route from the first rail:
  // under a rule that runs across, and after one that runs down.
  const float off = across + second.gap + second.thickness * 0.5f;
  return std::move(
      rule.fill(Fill::none())
          .shape(keyedShape(std::tuple{column, across},
                            [column, across](SkSize size) {
                              SkPathBuilder route;
                              route.moveTo(column ? across : 0.0f,
                                           column ? 0.0f : across);
                              route.lineTo(column ? across : size.width(),
                                           column ? size.height() : across);
                              return route.detach();
                            }))
          .stroke(lines::Rails{
              .rails = {
                  {.across = 0.0f, .width = mark.thickness, .fill = ink},
                  {.across = column ? off : -off,
                   .width = second.thickness,
                   .fill =
                       second.fill.kind == Fill::Kind::None ? ink : second.fill,
                   .dash = second.dash}}}));
}

}  // namespace sigil::compose::kit
