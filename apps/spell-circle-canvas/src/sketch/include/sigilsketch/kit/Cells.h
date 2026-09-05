#pragma once

/** @file
 * The pieces a specimen on a sheet is made of — the well it is shown in
 * and the caption that names it — and the three ways a run of them is
 * arranged: along a line, into equal shares, and into a grid.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
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
   *  well that paints nothing. */
  std::optional<compose::Fill> ground;
  /** Unset is the theme's well padding. */
  std::optional<float> padding;
  bool clip = true;
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
[[nodiscard]] compose::Element well(const Well& spec,
                                    compose::Element surface);
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

/** EQUAL SHARES OF THE WIDTH, one per cell — the shape a page divides
 *  itself into when the cells are panels rather than specimens, and the
 *  one a run of fixed widths cannot make because it does not know how
 *  wide the page is. */
struct Columns {
  std::vector<compose::Element> cells;
  /** Unset is the theme's cell gap. */
  std::optional<float> gap;
  bool ruled = false;
  /** Stretch (default) makes every column as tall as the tallest, which
   *  is what a row of grounded panels wants. */
  compose::Align align = compose::Align::Stretch;
};

/** THE COLUMNS.
 *
 *      sketch::kit::columns({.cells = {left, middle, right}})
 *
 *  Every cell is grown to one share, so a cell that carries its own width
 *  loses it. A run of cells at their own widths is `cells` above. */
[[nodiscard]] compose::Element columns(Columns run);

/** A GRID OF PANELS: equal shares across, wrapped every @p columns. */
struct PanelGrid {
  std::vector<compose::Element> cells;
  int columns = 3;
  /** Across; unset is the theme's cell gap. */
  std::optional<float> gap;
  /** Down; unset is whatever `gap` resolves to, so a grid is square in
   *  its air unless it is told not to be. */
  std::optional<float> rowGap;
  bool ruled = false;
};

/** THE GRID.
 *
 *      sketch::kit::panelGrid({.cells = panels, .columns = 4})
 *
 *  The last row is filled out with empty shares, so four panels over
 *  three columns leave the fourth at one third of the width rather than
 *  at the whole of it. */
[[nodiscard]] compose::Element panelGrid(PanelGrid grid);

}  // namespace sigil::sketch::kit
