#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * The pieces a specimen on a sheet is made of — the well it is shown in
 * and the caption that names it — and the three ways a run of them is
 * arranged: along a line, into equal shares, and into a grid.
 */

#include <include/core/SkPoint.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::sketch::kit {

/** THE FIXED SURFACE A SPECIMEN IS SHOWN IN, sized by the caller and
 *  grounded by the theme. */
struct Well {
  compose::Dimension width;
  compose::Dimension height;
  /** Unset is the theme's cell ground. Set it to `Fill::none()` for a
   *  well that paints nothing, and to a material for a well grounded in
   *  something generated per pixel. */
  std::optional<compose::SurfacePaint> ground;
  /** Across; unset is the theme's well padding. */
  std::optional<float> padding;
  /** Down, where a plate is set tighter or looser than it is wide; unset
   *  is whatever `padding` resolves to. */
  std::optional<float> paddingY;
  bool clip = true;
  /** Rounds the well; unset is the square corner a specimen sheet uses.
   *  A component that grounds a PLATE rather than a specimen — a panel —
   *  reads an unset radius as the theme's own. */
  std::optional<float> corners;
  /** ONE HAIRLINE ROUND THE WELL, over its ground — what turns a patch of
   *  ground into a PLATE. Unset draws none; unset is not the theme's rule,
   *  because a specimen well is grounded and unruled and that is the
   *  common case. */
  std::optional<compose::Fill> keyline;
  float keylineWidth = 1;
  /** Whether what the well holds is PLACED rather than flowed — a stack
   *  rather than a box, for the plate whose children carry their own
   *  rects. It says what the surface-less spelling builds. */
  bool placed = false;
  /** HOW WHAT THE WELL HOLDS RANGES INSIDE IT. */
  struct Content {
    compose::Align across = compose::Align::Center;
    compose::Justify down = compose::Justify::Center;
  };
  /** THE WELL THAT HOLDS, rather than the well that IS. Unset (default),
   *  the element `well(Well, surface)` is handed BECOMES the plate — the
   *  spec is written onto it. Stated, the plate is a surface of its own
   *  and that element stands INSIDE it at its own measure, ranged as
   *  this says, which is the specimen smaller than the plate it is shown
   *  on; centred is what it says where it says nothing else. */
  std::optional<Content> content;
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
  /** THE RELIEF, which is the recess with its sign turned over: what
   *  makes a plate read as a piece STANDING PROUD of what holds it —
   *  the light one edge catches and the shadow the opposite one casts.
   *  Unset is flush. A carved frame, a raised boss and a key cap are all
   *  this over a ground; the recess under it is the hole they are cut
   *  around. */
  struct Relief {
    /** How far the surface is lifted off its plane, and how soft the
     *  lift is — a hard narrow pair is a machined edge, a wide soft one
     *  a moulded piece. */
    float depth = 3;
    float blur = 4;
    /** Where the light stands, in degrees. */
    float angleDeg = 120;
    /** What the lit edge and the shaded one are painted in. */
    SkColor4f light{1, 1, 1, 0.65f};
    SkColor4f shade{0, 0, 0, 0.45f};
    bool operator==(const Relief&) const = default;
  };
  std::optional<Relief> relief;
};

/** @p surface, sized, grounded, padded and clipped as @p specification and the
 *  theme say.
 *
 *      sketch::kit::well({.width = kCell, .height = kPicture})
 *          .children({subject()})
 *
 *  THE SECOND ARGUMENT IS THE SURFACE ITSELF rather than a child wrapped
 *  in a new box — the spec is written onto it, so it BECOMES the plate:
 *  hand it `custom(key, draw)` where the drawing wants the well's
 *  resolved size, and `box().children({body})` where the well holds a
 *  laid-out body. Omitted, it is an empty box ready for children.
 *
 *  `Well::content` is the other reading, the well that HOLDS what it is
 *  handed at that element's own measure:
 *
 *      sketch::kit::well({.width = kCell, .height = kPicture,
 *                         .content = Well::Content{}}, picture()) */
[[nodiscard]] compose::Element well(const Well& specification,
                                    compose::Element surface);
[[nodiscard]] compose::Element well(const Well& specification);

/** ONE CAPTIONED SPECIMEN: @p body with @p label over it and @p note
 *  under it, set in the classes `captionLabel` and `captionNote` — the
 *  theme's own registers, bound around the two lines this call writes —
 *  and spaced by the theme's caption gaps.
 *
 *      sketch::kit::caption(
 *          kCell, "Border::Mode::Bracket",
 *          "only within 18 px of each corner",
 *          well({.width = kCell, .height = kPicture}).children({plaque()}))
 *
 *  @p measure is the width the remark wraps at — the cell's own width.
 *  It is the one distance a caption cannot inherit, because it is a fact
 *  about the specimen rather than about the look; 0 lets the remark take
 *  whatever width the cell resolves to. An empty label or an empty note
 *  is simply absent, and spends no gap. */
[[nodiscard]] compose::Element caption(float measure, compose::Utf8 label,
                                       compose::Utf8 note,
                                       compose::Element body);

/** THE CELL A SHEET IS MADE OF, stated ONCE: the plate every specimen on
 *  it stands on, and the measure its caption is set to.
 *
 *      const sketch::kit::Cell kSpecimen{
 *          .plate = {.width = kCell, .height = kPicture, .padding = 12}};
 *      …
 *      sketch::kit::cell(kSpecimen, "shapes::chamfer(9)",
 *                        "the corner taken off square", plaque())
 *
 *  It is the four-line private helper every sheet of captioned pictures
 *  otherwise defines for itself. */
struct Cell {
  /** THE PLATE, which HOLDS the picture. Unset, `Well::content` holds it
   *  as a box holds a child; stated, it ranges the picture inside the
   *  plate — centred where it says nothing else, which is what a
   *  specimen smaller than its plate asks for. A picture that IS its
   *  plate — a drawing handed the plate's own resolved size, a well
   *  grounded in a material — is `caption(measure, label, note,
   *  well(plate, surface))`, which says that in one call already. */
  Well plate;
  /** The caption's measure, px — the width the remark wraps at. Unset is
   *  the plate's own width, which is the cell's width wherever the plate
   *  is stated in pixels. */
  std::optional<float> measure;
};

/** ONE CAPTIONED SPECIMEN on @p sheet's plate: @p picture held by that
 *  plate, with @p label over it and @p note under it in the theme's
 *  voice. */
[[nodiscard]] compose::Element cell(const Cell& sheet, compose::Utf8 label,
                                    compose::Utf8 note,
                                    compose::Element picture);

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
  /** THE WIDTH THE SHARES ARE CUT FROM. Unset is the parent's; a grid
   *  standing in a column that sizes itself from its content has none to
   *  divide, and says here how wide it is. */
  compose::Dimension measure;
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
