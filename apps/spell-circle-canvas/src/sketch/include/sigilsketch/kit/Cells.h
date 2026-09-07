#pragma once

/** @file
 * The pieces a specimen on a sheet is made of — the well it is shown in
 * and the caption that names it — and the three ways a run of them is
 * arranged: along a line, into equal shares, and into a grid.
 */

#include <include/core/SkPoint.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Ground.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::sketch::kit {

/** THE FIXED SURFACE A SPECIMEN IS SHOWN IN, sized by the caller and
 *  grounded by the theme. */
struct Well {
  compose::Dim width;
  compose::Dim height;
  /** Unset is the theme's cell ground. Set it to `Fill::none()` for a
   *  well that paints nothing, and to a material for a well grounded in
   *  something generated per pixel. */
  std::optional<Ground> ground;
  /** Across; unset is the theme's well padding. */
  std::optional<float> padding;
  /** Down, where a plate is set tighter or looser than it is wide; unset
   *  is whatever `padding` resolves to. */
  std::optional<float> paddingY;
  bool clip = true;
  /** Rounds the well. 0 is the square corner a specimen sheet uses. */
  float corners = 0;
  /** ONE HAIRLINE ROUND THE WELL, over its ground — what turns a patch of
   *  ground into a PLATE. Unset draws none; unset is not the theme's rule,
   *  because a specimen well is grounded and unruled and that is the
   *  common case.
   *
   *  IT IS DRAWN INSIDE THE WELL'S OWN BOX. A rule centred on the
   *  boundary would put half its width outside, so a plate and the plate
   *  beside it would no longer be the width they were given — which is
   *  the one thing a fixed surface may not do. */
  std::optional<compose::Fill> keyline;
  float keylineWidth = 1;
  /** THE RECESS: what makes a well read as a HOLE PUNCHED in what holds
   *  it rather than as a patch of ground on it — a shadow cast inside the
   *  well's own edge, and the lip round that edge. Unset is flush, which
   *  is the specimen well. */
  struct Recess {
    /** The shadow inside the edge, its offset and how far it reaches. */
    compose::Fill shade = compose::Fill::color({0, 0, 0, 0.75f});
    SkVector offset{0, 2};
    float blur = 3;
    /** The lip's lit line and its shaded one, drawn SUNKEN — the hard
     *  edge under the blur, which says where the surface breaks while the
     *  blur says how deep it goes. Unset draws no lip, for the recess
     *  that is all softness. */
    std::optional<SkColor4f> lipLight;
    std::optional<SkColor4f> lipDark;
    float lipWidth = 1;
    bool operator==(const Recess&) const = default;
  };
  std::optional<Recess> recess;
};

/** @p surface, sized, grounded, padded and clipped as @p spec and the
 *  theme say.
 *
 *      sketch::kit::well({.width = kCell, .height = kPicture})
 *          .child(subject())
 *
 *  The second argument is the surface itself rather than a child wrapped
 *  in a new box: hand it `custom(key, draw)` where the drawing wants the
 *  well's resolved size, and `box().child(body)` where the well holds a
 *  laid-out body. Omitted, it is an empty box ready for children. */
[[nodiscard]] compose::Element well(const Well& spec, compose::Element surface);
[[nodiscard]] compose::Element well(const Well& spec);

/** ONE CAPTIONED SPECIMEN: @p body with @p label over it and @p note
 *  under it, set in the theme's caption registers and spaced by its
 *  caption gaps.
 *
 *      sketch::kit::caption(
 *          kCell, toU8("Border::Mode::Bracket"),
 *          toU8("only within 18 px of each corner"),
 *          well({.width = kCell, .height = kPicture}).child(plaque()))
 *
 *  @p measure is the width the remark wraps at — the cell's own width.
 *  It is the one distance a caption cannot inherit, because it is a fact
 *  about the specimen rather than about the look; 0 lets the remark take
 *  whatever width the cell resolves to. An empty label or an empty note
 *  is simply absent, and spends no gap. */
[[nodiscard]] compose::Element caption(float measure, std::u8string label,
                                       std::u8string note,
                                       compose::Element body);

/** A RUN OF CELLS along one axis, at the theme's gutter. */
struct Run {
  /** In order along the axis. */
  std::vector<compose::Element> cells;
  /** false (default) lays the cells out as a ROW; true stacks them. */
  bool column = false;
  /** Unset is the theme's cell gap. */
  std::optional<float> gap;
  /** A hairline between neighbours, in the theme's rule colour. */
  bool ruled = false;
  /** How the cells range across the axis. */
  compose::Align align = compose::Align::Start;
};

/** THE RUN.
 *
 *      sketch::kit::cells({.cells = {a, b, c}})
 *
 *  Each cell keeps the width it was given; the run places nothing and
 *  sizes nothing. */
[[nodiscard]] compose::Element cells(Run run);

/** EQUAL SHARES OF THE WIDTH — the shape a page divides itself into when
 *  the cells are panels rather than specimens, and the one a run of fixed
 *  widths cannot make because it does not know how wide the page is. */
struct PanelGrid {
  std::vector<compose::Element> cells;
  /** How many shares stand across before the next row. ZERO IS ONE ROW:
   *  every cell takes a share of the width and nothing wraps, which is
   *  the two- or three-panel band a page is divided into. */
  int columns = 3;
  /** Across; unset is the theme's cell gap. */
  std::optional<float> gap;
  /** Down; unset is whatever `gap` resolves to, so a grid is square in
   *  its air unless it is told not to be. Nothing to a single row. */
  std::optional<float> rowGap;
  bool ruled = false;
  /** How the cells of a SINGLE ROW range against each other. Stretch
   *  (default) makes every one as tall as the tallest, which is what a
   *  band of grounded panels wants. A wrapped grid ranges its rows for
   *  itself. */
  compose::Align align = compose::Align::Stretch;
};

/** THE GRID.
 *
 *      sketch::kit::panelGrid({.cells = panels, .columns = 4})
 *      sketch::kit::panelGrid({.cells = {left, middle, right},
 *                              .columns = 0})
 *
 *  Every cell is grown to one share, so a cell that carries its own width
 *  loses it; a run of cells at their own widths is `cells` above. The
 *  last row is filled out with empty shares, so four panels over three
 *  columns leave the fourth at one third of the width rather than at the
 *  whole of it. */
[[nodiscard]] compose::Element panelGrid(PanelGrid grid);

}  // namespace sigil::sketch::kit
