#pragma once

/** @file
 * The surface a sketch stands on: the canvas it declares, and the titled,
 * footed page drawn over it in the theme's voice.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>

namespace sigil::sketch::kit {

/** WHAT A SKETCH DECLARES ABOUT ITS SURFACE, in one value: how big, when
 *  a still of it is worth taking, and — where it wants something other
 *  than the theme's ground — what is behind it.
 *
 *  It is `CanvasSpec` with the ground made optional, because the ground
 *  is the one field of a canvas that is part of the LOOK, and a sheet
 *  set under a theme has already said what it is. */
struct Stage {
  SkSize size{900, 640};
  /** The scene time a still should be taken at. Negative states no
   *  preference and a sweep falls back to its own derived frame. */
  double captureAt = -1.0;
  /** Unset is the theme's ground — which is what a sheet wants, since
   *  the page paints the same colour over it. */
  std::optional<SkColor4f> background;
  /** Device pixels per canvas pixel a PLATE is taken at; 0 lets the host
   *  pick. */
  int oversample = 0;
  /** This sketch is a plate rather than a live scene: `--bench` judges
   *  it on the cost of its capture. */
  bool plateOnly = false;
};

/** DECLARE THE CANVAS, THE GROUND AND THE MOMENT in one call, over the
 *  theme in scope.
 *
 *      sketch::kit::stage(ctx, {.size = {1100, 424}, .captureAt = 0.05});
 *
 *  It writes the whole `CanvasSpec`, defaults included, so what a host
 *  reads back afterwards is exactly what this says. */
void stage(SketchContext& ctx, const Stage& surface);

/** THE PROSE A SHEET CARRIES. Only the prose: every face, size, colour,
 *  margin and rule the page is set in is the theme's, which is the whole
 *  of what this component adds over `compose::kit::sheet`. */
struct Page {
  std::u8string title;
  std::u8string subtitle;
  std::u8string footer;
  /** false rules neither the header nor the footer off from the content.
   *  A rule is the sheet's own arrangement rather than the theme's
   *  colour, so it is asked for here. */
  bool ruled = true;
  /** Unset is the theme's ground. A page whose ground is not a flat
   *  colour — a gradient behind the whole sheet — names that fill here,
   *  because a palette holds colours and a gradient is not one. */
  std::optional<compose::Fill> ground;
  /** The prefix the parts are keyed under, so a query can read the page
   *  back; empty keys nothing. */
  std::string key;
};

/** THE SHEET, over the whole canvas, in the theme's voice: the title, the
 *  subtitle and the footer set in the theme's three registers, the page
 *  margins and the content gap the theme's distances, the ground and the
 *  hairline the theme's colours.
 *
 *      ctx.composer.render(sketch::kit::page(
 *          {.title = toU8("THE RULE AND THE STRANDS"),
 *           .subtitle = toU8("dials · the width and the inset"),
 *           .footer = toU8("a crossing is discovered, not declared")},
 *          kit::cells({.cells = {a, b, c}, .gap = 10})));
 *
 *  A PAGE IS THE WHOLE SURFACE, so this one places itself over the
 *  canvas. A sheet that stands in a rect instead is
 *  `compose::kit::sheet` called directly with the theme's styles — this
 *  library adds a layer over that one and does not close it. */
[[nodiscard]] compose::Element page(const Page& sheet,
                                    compose::Element content);

}  // namespace sigil::sketch::kit
