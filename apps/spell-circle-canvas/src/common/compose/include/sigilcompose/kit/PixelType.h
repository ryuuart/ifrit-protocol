#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose KIT — the aliased bitmap-font bake.
 *
 * Shape a run with antialiasing off, rasterise it, threshold it to a 1-bit
 * A8 mask, and present that mask at an integer scale with nearest
 * sampling. The result is pixel type: a Minecraft-style or PS1-style face
 * whose every edge lands on the grid.
 *
 * ## This is a workaround, and it is worth knowing what for
 *
 * `text()` takes a `std::u8string`, not an animatable value, so a LIVE
 * numeric readout cannot be a text node at all. Baking to a mask and
 * blitting it with a pen inside a `custom()` leaf is how a readout gets drawn from a
 * live value without re-describing anything. If `text()` ever accepts
 * an animatable, most of the reason to reach for this file goes away —
 * what would remain is the aliased look itself.
 *
 * ## The four traps
 *
 * 1. **`intrinsicSize()` returns the ADVANCE, and glyph ink escapes it.** Ink
 *    overhanging the advance is normal — an italic's exit stroke, a
 *    negative left side-bearing, a swash — and a scratch surface sized to
 *    the advance clips it. The tell is that a clipped bake looks the same
 *    at every font size, where a rasterisation fault would move with the
 *    size.
 *
 *    No fixed pad is a fix; it is only a bigger guess. A boxed monospace
 *    never overhangs at all, while a script face can still reach past a
 *    generous pad, and padding only right and bottom leaves a negative
 *    left side-bearing clipping at the left where no pad value reaches it.
 *    So `Pad` is slack on **each** side, the run is drawn inset by it, and
 *    if ink still touches any edge the bake retries with the pad doubled.
 *    The mask is cropped to its lit bbox afterwards, so slack costs a
 *    larger scratch surface and nothing in the output.
 *
 * 2. **The SIZE is the control, and the threshold is inert.** Under
 *    `ShapingStyle::aliased` Skia lights a pixel iff its centre is inside
 *    the outline, so the coverage handed back is already 0 or 1 and any
 *    threshold in (0, 1] classifies it identically. What decides
 *    legibility is whether the x-height rounds up or down: at one size a
 *    lowercase `e`'s counter contains no pixel centre and every `e` prints
 *    as an `a`; one pixel larger and they all open; one larger again and
 *    an `m`'s gaps close instead. **Sweep the size, not the threshold.**
 *    The threshold becomes a real control only with `aliased = false`.
 *
 * 3. **Digits want one tabular advance,** or a rolling readout shivers as
 *    a `1` narrows the string.
 *
 * 4. **Present at an INTEGER scale with nearest sampling.** A bitmap face
 *    at a fractional scale is a blurry bitmap face. `Material::image` is
 *    the image path that takes a sampling parameter.
 */

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilgeometry/path/Outline.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmedia/core/Image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::draw {
class Pen;
}

namespace sigil::compose::kit {

/** Slack around the measured run, in px **on each side** — ink overhangs
 *  the advance a measurement reports, and it overhangs on the left as
 *  readily as on the right. The defaults are a starting point, not a
 *  guarantee: `coverage()` grows them, up to a limit, until no ink
 *  touches an edge. */
struct Pad {
  int x = 8;
  int y = 4;
  bool operator==(const Pad&) const = default;
};

/** How many times `coverage()` doubles the pad before giving up. Four
 *  doublings take the default 8 px to 128 px a side. */
inline constexpr int kPadRetries = 4;

/** The raw bake: a coverage plane and the box its ink actually occupies.
 *
 *  Coverage rather than a finished mask, because the useful next step
 *  varies: thresholding to 1-bit A8 is one, quantising to a few levels and
 *  looking each up in a palette is another, and a colour-aware
 *  classification reads the plane directly. */
struct Coverage {
  /** Premultiplied RGBA per pixel, row by row, `pad` bigger than the
   *  measured advance. Read it with `alphaAt`, or directly for a
   *  colour-aware classification. */
  std::vector<glm::vec4> plane;
  /** The plane's width and height in pixels. */
  glm::ivec2 planeSize{0, 0};
  /** The bbox of pixels with any coverage at all, in whole plane pixels.
   *  Empty when nothing lit (a space, an unmapped codepoint). */
  geometry::path::Rect ink;
  /** What `intrinsicSize()` reported — the ADVANCE, which is what a layout
   *  wants and is NOT the ink extent. */
  glm::vec2 advance{0, 0};
  /** The slack the bake actually used, which is NOT the slack asked for:
   *  a run whose ink touched an edge was baked again with the pad doubled.
   *  It is the origin of the run's own line box inside the plane, so it is
   *  what turns a plane coordinate into a typographic one. */
  Pad pad;

  bool valid() const { return !plane.empty(); }
  int width() const { return planeSize.x; }
  int height() const { return planeSize.y; }
  float alphaAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= planeSize.x || y >= planeSize.y) return 0.0f;
    return plane[(size_t)y * (size_t)planeSize.x + (size_t)x].a;
  }
};

/** Shape @p run in @p style, rasterise it, and hand back the coverage.
 *
 *  The pad grows by doubling until no ink touches an edge of the scratch
 *  surface. **After `kPadRetries` doublings it gives up silently and
 *  returns the clipped bake** — there is no error channel, so a face that
 *  overhangs further than that comes back cropped and looks like a
 *  rasterisation bug rather than a sizing one.
 *
 *  Built on `snapshot()`, which sizes by the root's CHILDREN, so the
 *  wrapper must carry explicit dimensions or an absolutely-placed child
 *  resolves against nothing. */
Coverage coverage(std::u8string_view run, sigil::weave::FontContext& fonts,
                  const sigil::weave::TextStyle& style, Pad pad = {});

/** A baked run: a 1-bit A8 image plus the numbers a caller needs to place
 *  and advance past it. */
struct Mask {
  std::shared_ptr<const media::Image> image;
  int w = 0, h = 0;
  /** Where the ink sat inside the padded plane, before cropping — the
   *  offset to add back if you want the run on its own baseline rather
   *  than flush to its ink. */
  int inkX = 0, inkY = 0;
  /** The measured advance, for laying the next run out. */
  float advance = 0.0f;

  explicit operator bool() const { return image != nullptr; }
};

/** Threshold a `Coverage` to 1-bit A8 and (by default) crop to its ink.
 *
 *  @p threshold is in coverage units [0, 1] and is **inert under aliased
 *  shaping**: there the renderer lights a pixel iff its centre is inside
 *  the outline, so the coverage is already 0 or 1 and every threshold in
 *  (0, 1] classifies it identically. It becomes a real control only when
 *  the run was deliberately shaped antialiased and is being quantised
 *  afterwards. */
Mask threshold(const Coverage& cov, float threshold = 0.5f,
               bool cropToInk = true);

/** The whole bake in one call: shape, rasterise, threshold, crop. */
inline Mask bakeRun(std::u8string_view run, sigil::weave::FontContext& fonts,
                    const sigil::weave::TextStyle& style, Pad pad = {},
                    float thresholdAt = 0.5f) {
  return threshold(coverage(run, fonts, style, pad), thresholdAt);
}
/** A bake is a root: a PARTIAL resolves against the initial values, as the
 *  leaf it would set does under `snapshot`, so a partial naming its face
 *  and size bakes exactly what the whole style would. */
inline Coverage coverage(std::u8string_view run,
                         sigil::weave::FontContext& fonts,
                         const sigil::weave::Type& type, Pad pad = {}) {
  return coverage(run, fonts, sigil::weave::textStyle(type), pad);
}
inline Mask bakeRun(std::u8string_view run, sigil::weave::FontContext& fonts,
                    const sigil::weave::Type& type, Pad pad = {},
                    float thresholdAt = 0.5f) {
  return bakeRun(run, fonts, sigil::weave::textStyle(type), pad, thresholdAt);
}

/** How a baked mask is presented. */
struct Present {
  material::Color colour = {1, 1, 1, 1};
  /** INTEGER, please: a bitmap face at 1.5× is a blurry bitmap face. */
  float scale = 1.0f;
  /** A second pass underneath, offset by this many DESTINATION px, with
   *  the colour's RGB multiplied by `shadowMultiplier`. The defaults follow
   *  Minecraft's own font renderer, which offsets by one GUI pixel and
   *  multiplies by a quarter. Zero offset = no shadow pass. */
  glm::vec2 shadowOffset = {0, 0};
  float shadowMultiplier = 0.25f;
};

/** Draw a baked mask at @p at (top-left) with @p pen, immediate mode.
 *
 *  An A8 image drawn over a rectangle modulates the paint's colour, which
 *  is the reason to blit rather than fill: the mask carries only coverage,
 *  so one bake serves every tint the drawing needs. */
void draw(draw::Pen& pen, const Mask& m, glm::vec2 at, const Present& p = {});

/** The same as a retained leaf sized to the mask, for a STATIC run. The
 *  node is `m.w × scale` by `m.h × scale`; place it with `.at()`/`.rect()`
 *  like any other absolute node.
 *
 *  Note this is a `custom()` leaf, so it never records a picture and its
 *  program runs every frame it is visible — cheap here (one image draw)
 *  but not free. A static run that never changes colour is better as
 *  `.cache(Cache::Texture)` on its parent. */
Element masked(const Mask& m, const Present& p = {});

// ---------------------------------------------------------------------------
// The 96-cell font — what a LIVE readout needs.

/** One baked cell.
 *
 *  The mask is cropped to its ink, so where that ink sat inside the cell's
 *  own LINE BOX is the cell's to carry: a `T` and a `p` cropped flush to
 *  their ink and drawn at one y would stand on no common line at all.
 *  `inkX` is the left side bearing and `inkY` the drop from the top of the
 *  line box, both in px, and `blit` adds them back. */
struct Cell {
  std::shared_ptr<const media::Image> mask;
  int w = 0, h = 0;
  /** The shaped advance, rounded — NOT the ink width. */
  int advance = 0;
  /** Where this cell's ink sits inside its line box. */
  int inkX = 0, inkY = 0;
};

/** ASCII 32..127, baked once. */
struct PixFont {
  std::array<Cell, 96> cells{};
  /** How deep the cells reach below the top of their shared line box —
   *  a line box for the caller. It is NOT the tallest cell: a cell is
   *  cropped to its ink and sits at its own drop inside the box. */
  int lineHeight = 0;
  /** The widest DIGIT advance, shared by all ten, so a rolling readout
   *  does not shiver as a `1` narrows the string. */
  int digitAdvance = 0;

  const Cell& cell(char c) const {
    const int i = (int)(unsigned char)c - 32;
    static const Cell empty{};
    return (i < 0 || i >= 96) ? empty : cells[(size_t)i];
  }
};

/** Bake every printable ASCII cell in @p style.
 *
 *  Space is special-cased: it gets an advance and no mask, which is not
 *  the same as a cell that failed to bake. A lone shaped space has no ink,
 *  so the crop-to-ink step would otherwise reduce it to nothing and every
 *  word would run together. @p spaceRatio is its advance as a fraction of
 *  the font size, scaled by the style's horizontal condense. */
PixFont bakeFont(sigil::weave::FontContext& fonts,
                 const sigil::weave::TextStyle& style, Pad pad = {3, 3},
                 float thresholdAt = 0.5f, float spaceRatio = 0.34f);
/** On a partial, resolved against the initial values as every bake is. */
inline PixFont bakeFont(sigil::weave::FontContext& fonts,
                        const sigil::weave::Type& type, Pad pad = {3, 3},
                        float thresholdAt = 0.5f, float spaceRatio = 0.34f) {
  return bakeFont(fonts, sigil::weave::textStyle(type), pad, thresholdAt,
                  spaceRatio);
}

/** How a run is spaced and where it lands. */
struct Blit {
  /** px added after every cell. */
  float track = 1.0f;
  /** Digits take `PixFont::digitAdvance` instead of their own, so a
   *  rolling readout does not shiver. On for a readout, off for prose. */
  bool tabularDigits = true;
  /** Round every pen position to a multiple of this many px (0 = off).
   *  A bitmap face that lands off the device grid is a resampled bitmap
   *  face. Pass the pitch of whatever grid the drawing is on, or `1` for
   *  the device grid. */
  float snap = 0.0f;
};

namespace detail {
inline float snapTo(float v, float grid) {
  return grid > 0 ? std::round(v / grid) * grid : v;
}

/** THE PEN WALK, and the only one. Measuring and drawing must accumulate
 *  the same way or a snapped run is laid out to one width and drawn at
 *  another: `snap` rounds every pen step, so it changes the advance and
 *  not merely where the cells land. @p emit is handed each cell and the
 *  pen position it occupies, relative to a pen that started at 0; the
 *  return is the run's advance.
 *
 *  Starting at 0 loses nothing: a caller's own origin is snapped before
 *  the walk, and a snapped origin plus a snapped offset is already on the
 *  grid, so adding it back per cell rounds to the same place. */
template <typename Emit>
inline float walkRun(const PixFont& f, std::string_view s, const Blit& b,
                     Emit&& emit) {
  float x = 0;
  for (char raw : s) {
    const int i = (int)(unsigned char)raw - 32;
    if (i < 0 || i >= 96) continue;
    const Cell& cell = f.cells[(size_t)i];
    const bool digit = raw >= '0' && raw <= '9';
    emit(cell, x);
    x = snapTo(
        x + (float)(digit && b.tabularDigits ? f.digitAdvance : cell.advance) +
            b.track,
        b.snap);
  }
  return x;
}
}  // namespace detail

/** Advance width of @p s without drawing it — the width `blit` returns for
 *  the same run and the same options, snapping included. */
inline float widthOf(const PixFont& f, std::string_view s, const Blit& b = {}) {
  return detail::walkRun(f, s, b, [](const Cell&, float) {});
}

/** Draw @p s at @p at (top-left of the line box) with @p pen and return
 *  the advance.
 *
 *  Immediate-mode, for inside a `custom()` leaf — which is the whole point:
 *  a live readout reads its live value here and draws the number, with
 *  nothing re-described and nothing reconciled. */
float blit(draw::Pen& pen, const PixFont& f, glm::vec2 at, std::string_view s,
           material::Color colour, const Blit& b = {});

}  // namespace sigil::compose::kit
