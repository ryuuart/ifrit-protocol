#pragma once

/** @file
 * SigilCompose KIT — pixel art: the grid a sprite is plotted on, the
 * palette it is coloured through, and the sheet many of them share.
 *
 * Two kinds of small raster live here.
 *
 * **The stamp a sink draws each point with** (`dotSprite`) — one image,
 * baked once, uploaded once, stamped thousands of times.
 *
 * **A palette-indexed sprite** (`Sprite`) — the 8-bit form: WHAT is
 * painted, in whole sprite pixels, is separate from WHICH COLOUR each mark
 * takes. The separation is the whole point of the form. One drawing serves
 * every palette a caller has: a theme swap re-colours it with no
 * re-authoring, and a shader that reads indices rather than colours (see
 * `indexImage`) can re-colour it per draw, which is how an 8-bit sprite
 * sheet always shaded a tile.
 *
 * ## Plotting, and why nothing here antialiases
 *
 * Every mark is an axis-aligned rectangle drawn with antialiasing OFF. A
 * rotated or curved edge, softened, puts colours on the canvas that are
 * not in the palette — dozens of them along one diagonal — and a drawing
 * whose census is supposed to be sixteen colours becomes one whose census
 * is four hundred. So a sprite is plotted as rows and rectangles, and a
 * circle is a run-length rasterisation, never a `drawCircle`.
 *
 * ## Which shape to reach for
 *
 * - `PixelInk` — a canvas and a cell size, with the three verbs. For a
 *   drawing made inside a paint program and thrown away, where holding the
 *   marks as a value would buy nothing.
 * - `Sprite` — the same three verbs, but recorded. Hold it, present it
 *   several ways, pack it into a sheet, bake it to an image.
 * - `pixelMap` — a `Sprite` read from a CHARACTER GRID: rows of text, each
 *   character naming a palette entry. The authoring form for art that is
 *   drawn rather than computed, and the form an XPM-era icon already has.
 * - `SpriteSheet` — sprites under names, packed onto one image, each one
 *   handed back the rectangle it occupies.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Shelf.h>

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::kit {

/** A WHITE ROUND DOT on transparency, @p px square — the stamp a point
 *  cloud is drawn with when each point is a soft blob rather than a
 *  square.
 *
 *  White because a sink tints per point: a coloured stamp multiplies into
 *  every tint and there is no way to get back to the colour that was
 *  asked for.
 *
 *  @p margin is the transparent ring left around the circle, and it is
 *  load-bearing rather than tidiness: the disc is antialiased, and a
 *  circle drawn out to the image edge has its softened edge cut off by
 *  the sampler, which reads as a faint square around every point once the
 *  stamp is scaled up.
 *
 *  Bake it once and hold the result — this allocates a surface and
 *  rasterises. */
inline sk_sp<SkImage> dotSprite(int px = 32, float margin = 2.0f) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(px, px));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(SK_ColorWHITE);
  const float centre = (float)px * 0.5f;
  canvas->drawCircle(centre, centre, centre - margin, paint);
  return surface->makeImageSnapshot();
}

// ---------------------------------------------------------------------------
// The pen

/** A canvas and the size of one sprite pixel on it. Coordinates are in
 *  SPRITE pixels throughout — the cell multiply happens here, once, so a
 *  drawing reads as the grid it was authored on rather than as arithmetic.
 *
 *  Antialiasing is off on every mark, which is what keeps the colour
 *  census equal to the palette. */
struct PixelInk {
  SkCanvas& canvas;
  float cell = 1.0f;
  /** Where the grid's origin sits on the canvas, in canvas pixels. */
  SkPoint at = {0, 0};

  void rect(float x, float y, float w, float h, const SkColor4f& colour) const {
    if (colour.fA <= 0) return;
    SkPaint paint;
    paint.setAntiAlias(false);
    paint.setColor4f(colour, nullptr);
    canvas.drawRect(SkRect::MakeXYWH(at.fX + x * cell, at.fY + y * cell,
                                     w * cell, h * cell),
                    paint);
  }
  void px(float x, float y, const SkColor4f& colour) const {
    rect(x, y, 1, 1, colour);
  }
  /** A run of one row: [x, x + w) at row @p y. */
  void row(float x, float y, float w, const SkColor4f& colour) const {
    rect(x, y, w, 1, colour);
  }
};

// ---------------------------------------------------------------------------
// The value

/** One mark: a rectangle in sprite pixels, in a palette entry.
 *
 *  Coordinates are floats although the grid is whole pixels, because a
 *  mark placed by arithmetic — a point on a circle, a wobble — lands
 *  between them, and rounding it here would move the drawing. */
struct SpriteRun {
  float x = 0, y = 0, w = 1, h = 1;
  int index = 0;
  bool operator==(const SpriteRun&) const = default;
};

/** A palette-indexed sprite: the marks, in order, and the colours they
 *  name.
 *
 *  ORDER IS KEPT and marks may overlap. A later mark paints over an
 *  earlier one exactly as it would on a canvas, so a translucent
 *  highlight over a body colour composites the way it was authored. That
 *  is why this is a list of runs rather than a grid of indices: a grid can
 *  hold one entry per cell and cannot hold what two overlapping
 *  translucent marks make of it.
 *
 *  It carries the same three verbs `PixelInk` does, so a drawing moves
 *  between immediate and recorded by changing which one it is handed. */
struct Sprite {
  /** The extent in sprite pixels. It is authored, not measured: a sprite
   *  whose marks stop short of its edge still occupies its whole cell, and
   *  a sheet packs the cell. */
  SkISize grid{0, 0};
  std::vector<SkColor4f> colours;
  std::vector<SpriteRun> runs;

  bool empty() const { return runs.empty(); }

  /** The index of @p colour, appended if this is its first use. Linear —
   *  a palette is a handful of entries, and holding a map beside it would
   *  cost more than the scan saves. */
  int entry(const SkColor4f& colour) {
    for (size_t i = 0; i < colours.size(); ++i)
      if (colours[i] == colour) return (int)i;
    colours.push_back(colour);
    return (int)colours.size() - 1;
  }

  void rect(float x, float y, float w, float h, int index) {
    if (w <= 0 || h <= 0 || index < 0) return;
    runs.push_back({x, y, w, h, index});
  }
  void rect(float x, float y, float w, float h, const SkColor4f& colour) {
    rect(x, y, w, h, entry(colour));
  }
  void px(float x, float y, int index) { rect(x, y, 1, 1, index); }
  void px(float x, float y, const SkColor4f& colour) {
    rect(x, y, 1, 1, colour);
  }
  /** A run of one row: [x, x + w) at row @p y. */
  void row(float x, float y, float w, int index) { rect(x, y, w, 1, index); }
  void row(float x, float y, float w, const SkColor4f& colour) {
    rect(x, y, w, 1, colour);
  }

  const SkColor4f& colourOf(int index) const {
    static const SkColor4f none{0, 0, 0, 0};
    return index >= 0 && (size_t)index < colours.size() ? colours[(size_t)index]
                                                        : none;
  }
};

/** The palette a CHARACTER GRID names its colours through: one character
 *  per entry, in the same order, so `chars[i]` is the character that means
 *  `colours[i]`.
 *
 *  A blank is not special-cased: give the character that means "nothing"
 *  an entry whose alpha is zero, and it draws nothing like any other
 *  transparent entry. Every character a grid uses has to be in `chars` —
 *  see `pixelMap`. */
struct SpriteKey {
  std::string_view chars;
  std::span<const SkColor4f> colours;
};

/** Read a character grid into a sprite: each character names a palette
 *  entry through @p key, and the grid is @p rows tall by the length of a
 *  row wide.
 *
 *  **Nothing is guessed.** A character the key does not carry, a ragged
 *  grid, or a key whose two sides are different lengths REFUSES — the
 *  result is empty. A typo in a hand-drawn grid is otherwise a hole in the
 *  picture that nobody can find, since a missing character and a
 *  transparent one look identical.
 *
 *  Adjacent cells of one character MERGE into the largest rectangle that
 *  is all that character: a 24 x 24 icon is a few dozen marks rather than
 *  576, which is the difference between a node tree a composer walks
 *  happily and one it does not. The merge cannot change the picture — the
 *  cells it joins hold one colour and do not overlap. */
std::optional<Sprite> pixelMap(std::span<const std::string> rows,
                               const SpriteKey& key);

// ---------------------------------------------------------------------------
// Presenting one

/** How a sprite is presented. */
struct SpriteStyle {
  /** Destination pixels per sprite pixel. */
  float cell = 1.0f;
  /** Multiplied into every entry's OWN alpha, mark by mark — not applied
   *  as one layer over the finished drawing. The two differ wherever marks
   *  overlap, and the per-mark reading is the one that keeps a fade
   *  looking like the drawing it started from. */
  float alpha = 1.0f;
};

/** Draw @p sprite with its top-left at @p at, immediate mode. */
void drawSprite(SkCanvas& canvas, const Sprite& sprite, SkPoint at,
                const SpriteStyle& style = {});

/** The same sprite as a retained node, sized `grid * cell`, one absolutely
 *  placed box per mark.
 *
 *  Boxes rather than a baked image because the marks then carry the
 *  drawing's own colours into the tree: a palette taken from a theme
 *  re-colours on a theme change with nothing re-baked, and the node can be
 *  scaled by layout without resampling anything. A sprite that never
 *  changes is a candidate for `Cache::Texture` on the node above it, which
 *  is the seam that turns a settled subtree into one image. */
Element pixelSprite(const Sprite& sprite, const SpriteStyle& style = {});

/** @p sprite rasterised on its own transparent image, `grid * cell` in
 *  size. The bake a sheet is made of, and the form to hold when the same
 *  drawing is stamped many times. */
sk_sp<SkImage> spriteImage(const Sprite& sprite, const SpriteStyle& style = {});

/** @p sprite rasterised as INDICES rather than colours: each mark writes
 *  its palette index into the red channel, opaque, and everything else is
 *  transparent. The sprite's own `colours` are not read at all.
 *
 *  This is the 8-bit sheet as a shader reads it. A runtime effect samples
 *  this image with `kNearest` — the value is data, and a linear tap
 *  between two indices is a blend of two unrelated hues — recovers the
 *  index, does whatever index arithmetic the drawing wants (a shade
 *  addend, a block replacement) and looks the result up in a palette
 *  image. One bake then serves every shade and every recolour, where a
 *  coloured bake needs one per combination.
 *
 *  Index 0 is the HOLE, the way an 8-bit sheet has always reserved it: a
 *  mark in entry 0 leaves its pixels transparent, so a sampler can tell
 *  "no sprite here" from "index zero here" with the alpha it already
 *  reads. An index above 255 does not fit in a channel and is dropped. */
sk_sp<SkImage> indexImage(const Sprite& sprite, float cell = 1.0f);

// ---------------------------------------------------------------------------
// Many, under names

/** Sprites under names, and the one image they share.
 *
 *  The names are the reason this is not a `std::vector`: a drawing refers
 *  to `"folder"` or to the file name the art came from, and an index into
 *  a table that another table has to stay in step with is how a set of
 *  icons goes wrong. Lookup is by name, and an unknown name gives nothing
 *  rather than the wrong sprite.
 *
 *  `bake()` is the atlas: every sprite rasterised onto ONE image, shelf
 *  packed, each name handed the rectangle it occupies. One image is one
 *  upload and one texture bind — which is the whole reason a sheet
 *  exists — and `rect()` is what a caller draws a sub-rectangle of it
 *  with. A sheet is worth baking when the sprites are stamped many times
 *  at a known scale; a sprite drawn once, or placed off the pixel grid, is
 *  better presented as its marks. */
class SpriteSheet {
 public:
  /** Add or replace @p name. Adding after a bake drops the sheet: the
   *  next `bake()` builds it again. */
  void add(std::string name, Sprite sprite);

  /** The sprite under @p name, or null. */
  const Sprite* find(std::string_view name) const;

  size_t size() const { return m_entries.size(); }
  /** The names, in the order they were added. */
  std::vector<std::string_view> names() const;

  /** Pack and rasterise every sprite onto one image. False when there is
   *  nothing to bake or the surface could not be made. */
  bool bake(const SpriteStyle& style = {});

  /** The baked sheet, null until `bake()` succeeds. */
  const sk_sp<SkImage>& image() const { return m_sheet; }

  /** Where @p name sits on the baked sheet, in sheet pixels — empty for an
   *  unknown name or before a bake. */
  SkRect rect(std::string_view name) const;

  /** The baked sprite under @p name as a node: the sheet, drawn through
   *  the window `rect(name)` cuts, at the size it was baked. Empty when
   *  the name or the bake is missing. */
  Element cell(std::string_view name) const;

 private:
  struct Entry {
    std::string name;
    Sprite sprite;
    SkRect rect = SkRect::MakeEmpty();
  };
  const Entry* entryOf(std::string_view name) const;
  std::vector<Entry> m_entries;
  sk_sp<SkImage> m_sheet;
};

}  // namespace sigil::compose::kit
