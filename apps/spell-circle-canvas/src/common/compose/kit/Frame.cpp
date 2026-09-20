#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Rails.h>
#include <sigilcompose/core/Shape.h>
#include <sigilcompose/kit/Frame.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace sigil::compose::kit {

Element line(const Line& mark) {
  const bool open = mark.length.unit == Dimension::Unit::Auto;
  const SurfacePaint ink =
      mark.fill.none() ? SurfacePaint{Fill::currentInk()} : mark.fill;
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
  // A rail stores one comparable Fill, so a paint on a paired rule rides
  // the rails collapsed: a static gradient keeps its shader, a live one
  // has no colour to give a rail that is measured without a frame.
  const Fill railInk = ink.collapsedFill();
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
                  {.across = 0.0f, .width = mark.thickness, .fill = railInk},
                  {.across = column ? off : -off,
                   .width = second.thickness,
                   .fill = second.fill.none() ? railInk
                                              : second.fill.collapsedFill(),
                   .dash = second.dash}}}));
}

Element ladder(const Ladder& rungs) {
  // A rule SITS ON its line of the rhythm, so the distance between two
  // rules is the pitch LESS a rule — the one subtraction a ruled bed
  // otherwise spells at the call, twice, and gets wrong once.
  const float between = std::max(0.0f, rungs.pitch - rungs.thickness);
  Element rails = box().cover();
  // The air is before each rule and not after the last: a ladder is as
  // deep as its count says, and a rung that had to share a shortfall
  // with the box would be shrunk out of existence.
  if (rungs.column)
    rails.row()
        .padding(Dimension(between), Dimension(0), Dimension(0), Dimension(0))
        .gap(between);
  else
    rails.column()
        .padding(Dimension(0), Dimension(between), Dimension(0), Dimension(0))
        .gap(between);
  for (int i = 0; i < rungs.count; ++i)
    rails.children({line({.thickness = rungs.thickness,
                          .column = rungs.column,
                          .fill = rungs.fill})
                        .shrink(0)});
  return rails;
}

}  // namespace sigil::compose::kit
