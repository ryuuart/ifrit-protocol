#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * The surface a sketch stands on: the canvas it declares, and the titled,
 * footed page drawn over it in the voice of the stylesheet in force.
 */

#include <include/core/SkColor.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/StyleSheets.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>

namespace sigil::sketch {
/** The per-frame value a sketch body is handed; its own words are
 *  `<sigilsketch/scene2d/Scene2D.h>`, which this names and does not
 *  include: the kit's headers reach one signature's worth of the scene2d
 *  runtime, not the whole compose surface behind it. */
struct Scene2DContext;
/** The same for a SCENE3D SKETCH — a world Frame, lit and photographed — whose
 *  own words are `<sigilsketch/scene3d/Scene3D.h>`. */
struct Scene3DContext;
}  // namespace sigil::sketch

namespace sigil::sketch::kit {

/** WHAT A SKETCH DECLARES ABOUT ITS SURFACE, in one value: the
 *  stylesheet it is set in, how big, when a still of it is worth taking,
 *  and — where it wants something other than the stylesheet's ground —
 *  what is behind it.
 *
 *  It is `CanvasSpecification` with the ground made optional, because the
 *  ground is the one field of a canvas that is part of the LOOK, and a
 *  stylesheet has already said what it is. */
struct Stage {
  /** THE LOOK THE SKETCH IS SET IN, as one stylesheet. The stage applies
   *  it at the root of whatever the sketch renders, over the house custom
   *  properties, so every component the sketch describes reads it where
   *  it lands and a stylesheet of a few rules still finds every spacing
   *  and palette property a component reads. A variant of a stock look is
   *  that look with rules joined after it:
   *
   *      .styleSheet = featureStyleSheet(Density::Spacious) +
   *                    compose::rule(":root").var("spacing-caption-gap", 8)
   *
   *  The last `palette-ground` colour a `:root` rule of it sets is the
   *  canvas's ground too, where `background` names none, so the canvas
   *  and the page agree. Unset is the house custom properties and ground
   *  alone, with none of the house role rules: a study that sets its own
   *  type keeps it, and a component it uses still reads the house
   *  distances and colours. */
  std::optional<compose::StyleSheet> styleSheet;
  SkSize size{900, 640};
  /** The scene time a still should be taken at. Negative states no
   *  preference and a sweep falls back to its own derived frame. */
  double captureAt = -1.0;
  /** Unset is the ground the stage's stylesheet states — the last
   *  `palette-ground` colour it sets on `:root`, else the house ground —
   *  which is what a sheet wants, since the page paints the same colour
   *  over it. */
  std::optional<material::Color> background;
  /** Device pixels per canvas pixel a PLATE is taken at; 0 lets the host
   *  pick. */
  int oversample = 0;
  /** This sketch is a plate rather than a live scene: a background still
   *  stands it down unless it was asked to walk plates. */
  bool plateOnly = false;
};

/** DECLARE THE CANVAS, THE LOOK, THE GROUND AND THE MOMENT in one call:
 *  the stylesheet the stage names stands at the root of everything the
 *  sketch renders, through `Composer::setStyleSheet`. Everything under
 *  that root reads the look as the properties, roles and classes that
 *  reach it there.
 *
 *      sketch::kit::stage(ctx, {.styleSheet = sketch::kit::studyStyleSheet(),
 *                               .size = {1100, 424}, .captureAt = 0.05});
 *
 *  It writes the whole `CanvasSpecification`, defaults included, so what a host
 *  reads back afterwards is exactly what this says. */
void stage(Scene2DContext& ctx, const Stage& surface);

/** THE SAME DECLARATION FOR A SCENE3D SKETCH, which says the same three things
 *  through three calls of its own. A scene3d sketch has no one composer
 *  root, so the stylesheet names the ground alone here; a page the sketch
 *  paints into a texture applies the stylesheet on its own root.
 *
 *      sketch::kit::stage(ctx, {.size = {640, 440}, .captureAt = 1.3,
 *                               .background = {0.03f, 0.035f, 0.05f, 1}});
 *
 *  The VIEWPOINT is not here: a camera is a fact about the scene rather
 *  than about the plate, and a sketch that puts one on a rail states it in
 *  its description. `Scene3DContext::camera` is the fallback for the sketch
 *  that does not. */
void stage(Scene3DContext& ctx, const Stage& surface);

/** THE PROSE A SHEET CARRIES. Only the prose: every face, size, colour,
 *  margin and rule the page is set in is the stylesheet's in force where
 *  the page lands — the one the stage names — which is the whole of what
 *  this component adds over `compose::kit::sheet`. */
struct Page {
  compose::Utf8 title;
  compose::Utf8 subtitle;
  compose::Utf8 footer;
  /** false rules neither the header nor the footer off from the content.
   *  A rule is the sheet's own arrangement rather than the stylesheet's
   *  colour, so it is asked for here. */
  bool ruled = true;
  /** Unset is the stylesheet's ground. A page whose ground is not a flat
   *  colour — a gradient behind the whole sheet, a material under it —
   *  names that here, because a palette holds colours and neither of
   *  those is one. */
  std::optional<compose::Fill> ground;
  /** The prefix the parts are keyed under, so a query can read the page
   *  back; empty keys nothing. */
  std::string key;
};

/** THE SHEET, over the whole canvas, in the stylesheet's voice: the title,
 *  the subtitle and the footer in document roles `h1`, `lead` and `footer`
 *  — the stylesheet's registers, resolved where these lines land — the page
 *  margins and the content gap the sheet's `spacing-margin-…` and
 *  `spacing-content-gap`, the ground and the hairline its `palette-ground`
 *  and `palette-rule`, and the running text the `page` role's rule.
 *
 *      sketch::kit::stage(ctx, {.styleSheet = sketch::kit::studyStyleSheet(),
 *                               .size = {900, 640}});
 *      ctx.composer.render(sketch::kit::page(
 *          {.title = "THE RULE AND THE STRANDS",
 *           .subtitle = "dials · the width and the inset",
 *           .footer = "a crossing is discovered, not declared"},
 *          kit::cells({.cells = {a, b, c}, .gap = 10})));
 *
 *  A PAGE IS THE WHOLE SURFACE, so this one places itself over the
 *  canvas. A sheet that stands in a rect instead is
 *  `compose::kit::sheet` called directly under a stylesheet — this
 *  library adds a layer over that one and does not close it. */
[[nodiscard]] compose::Element page(const Page& sheet,
                                    compose::Element content);

}  // namespace sigil::sketch::kit
