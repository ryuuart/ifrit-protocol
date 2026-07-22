#pragma once

/** @file
 * SigilCompose KIT — the aliased bitmap-font bake.
 *
 * ## Why this exists
 *
 * Three studies wrote this from scratch, ~120 lines each, with no
 * knowledge of one another:
 *
 * - `thaumonomicon.cpp:1160-1205` — `bakeText`/`blitText`, a 1-bit A8 mask
 *   per string, presented at 2× with `kNearest`. Minecraft's `ascii.png`.
 * - `vagrant_story_target.cpp:425-520` — `bakeCell`/`bakeFont`/`blit`/
 *   `widthOf`, 96 cells with a tabular digit advance. PS1 cell type.
 * - `xcom_battlescape.cpp:764-818` — `pixelText`, the same bake with a
 *   TWO-level quantisation into palette indices, presented at 4×.
 *
 * That is 240+ duplicated lines and three independent rediscoveries of the
 * same four traps. Whether the *core* should grow a bitmap-font path is a
 * separate question (it should not, on this evidence); a kit component
 * costs nothing and deletes the duplication today.
 *
 * ## The actual hole, which this does not close
 *
 * All three studies bake because **`text()` takes a `std::u8string`, not a
 * `PropValue`** (`Compose.h`, verified) — so a live numeric readout cannot
 * be a text node at all. `vagrant_story_target.cpp:409` says so in as many
 * words. Baking is the workaround, not the goal. That is a ROADMAP entry
 * and a mask helper will bury it if nobody says this out loud, so: **this
 * header is a workaround.** If `text()` ever takes a PropValue, two of the
 * three motivating uses go away.
 *
 * ## The four traps, all of them paid for once already
 *
 * 1. **`measure()` returns the ADVANCE, and glyph ink escapes it.** At a
 *    +2 pad `thaumonomicon`'s longest tooltip line printed "Centrifug"
 *    plus a two-pixel stub where its `e` should be — **at every font size
 *    tried**, which is what gave it away: a rasterisation fault moves with
 *    the size, a surface that is too small does not. Its fix was `+8`.
 *
 *    Measured here across faces, **`+8` is not a fix either, it is a
 *    bigger guess**: Menlo (monospace, boxed by construction) never
 *    overhangs at all, Helvetica Italic overhangs at almost every size and
 *    string, and Zapfino "Wf" at 12 px still reaches the edge at +8. And
 *    all three studies padded only the RIGHT and BOTTOM, so a face with a
 *    negative left side-bearing clips at the left instead and no pad value
 *    reaches it.
 *
 *    So this component does not guess. `Pad` is slack on **each** side,
 *    the run is drawn inset by it, and if the ink still touches any edge
 *    the bake is retried with the pad doubled (`kPadRetries` times). The
 *    mask is cropped to its lit bbox afterwards, so slack costs a larger
 *    scratch surface and nothing else. This is the one place where being a
 *    shared component buys something the three hand-rolls could not
 *    justify individually.
 *
 * 2. **The SIZE is the control, and the threshold is inert.** Under
 *    `ShapingStyle::aliased` Skia lights a pixel iff its centre is inside
 *    the outline, so the coverage handed back is already 0 or 1 and the
 *    threshold reclassifies nothing (`thaumonomicon.cpp:1146-1152`
 *    verified 110 vs 170 as pixel-identical over a whole 1280×800 frame).
 *    What decides legibility is whether the **x-height rounds to the
 *    reference face's**: Menlo at 9 px has a 4.4 px x-height, `e`'s
 *    counter never contains a pixel centre, and every `e` prints as an
 *    `a` — "research" reads "rasaarch". At 10 px the x-height is 4.9,
 *    rounds to 5, and every `e` opens. At 11 `m`'s two gaps close.
 *    **Sweep the size, not the threshold.** The threshold only becomes a
 *    real control with `aliased = false`.
 *
 * 3. **Digits want one tabular advance.** Otherwise a rolling readout
 *    shivers as `1` narrows the string (`vagrant_story_target.cpp:486`).
 *
 * 4. **Present at an INTEGER scale with `kNearest`.** A bitmap face at a
 *    fractional scale is a blurry bitmap face. `Material::image` is the
 *    image path that takes a sampling parameter
 *    (`xcom_battlescape.cpp:822-833`).
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Debug.h"
#include "sigilcompose/Studio.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSamplingOptions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace sigil::compose::kit {

/** Slack around the measured run, in px **on each side**. See trap 1: the
 *  defaults are a starting point, not a guarantee — `coverage()` grows
 *  them until no ink touches an edge. */
struct Pad {
  int x = 8;
  int y = 4;
  bool operator==(const Pad &) const = default;
};

/** How many times `coverage()` doubles the pad before giving up. Four
 *  doublings take the default 8 to 128 px a side, which is past any face
 *  probed here. */
inline constexpr int kPadRetries = 4;

/** The raw bake: a coverage plane and the box its ink actually occupies.
 *
 *  This is the step all three studies share, and it stops before the step
 *  where they diverge — `thaumonomicon` and `vagrant_story_target`
 *  threshold to 1-bit A8, `xcom_battlescape` quantises to two levels and
 *  looks each up in a 256-entry VGA palette. Handing back coverage rather
 *  than a finished mask serves all three; a component that only emitted
 *  A8 would have served two. */
struct Coverage {
  /** F16 premultiplied, `pad` bigger than the measured advance. Read it
   *  with `alphaAt`, or directly for a colour-aware classification. */
  SkBitmap plane;
  /** The bbox of pixels with any coverage at all. Empty when nothing lit
   *  (a space, an unmapped codepoint). */
  SkIRect ink = SkIRect::MakeEmpty();
  /** What `measure()` reported — the ADVANCE, which is what a layout
   *  wants and is NOT the ink extent. */
  SkSize advance = {0, 0};

  bool valid() const { return !plane.isNull(); }
  int width() const { return plane.width(); }
  int height() const { return plane.height(); }
  float alphaAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= plane.width() || y >= plane.height())
      return 0.0f;
    return plane.getColor4f(x, y).fA;
  }
};

/** Shape @p run in @p style, rasterise it, and hand back the coverage.
 *
 *  Built on `debug::rasterize`, which already carries the other trap in
 *  this area: `snapshot()` sizes by the root's CHILDREN, so the wrapper
 *  must carry explicit dims or an absolutely-placed child resolves against
 *  nothing. Reusing it is deliberate — a second copy of that code is
 *  exactly what this header exists to stop. */
inline Coverage coverage(std::u8string_view run, sigil::weave::FontContext &fonts,
                         const sigil::weave::TextStyle &style, Pad pad = {}) {
  Coverage out;
  const std::u8string text8(run);
  const SkSize sz = measure(box().child(text(text8, style)), fonts);
  out.advance = sz;
  const int advW = std::max(1, (int)std::ceil(sz.width()));
  const int advH = std::max(1, (int)std::ceil(sz.height()));

  for (int attempt = 0;; ++attempt) {
    const int w = advW + 2 * std::max(0, pad.x);
    const int h = advH + 2 * std::max(0, pad.y);
    // padding() rather than an absolute offset: the run is inset by `pad`
    // on EVERY side, so a negative left side-bearing has somewhere to go.
    // The three hand-rolled versions all padded right and bottom only.
    debug::Raster r = debug::rasterize(
        box()
            .padding((float)std::max(0, pad.x), (float)std::max(0, pad.y))
            .child(text(text8, style)),
        fonts, {w, h});
    if (!r.valid())
      return out;
    out.plane = std::move(r.bitmap);
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x)
        if (out.plane.getColor4f(x, y).fA > 0.0f) {
          x0 = std::min(x0, x);
          y0 = std::min(y0, y);
          x1 = std::max(x1, x);
          y1 = std::max(y1, y);
        }
    out.ink = x1 < 0 ? SkIRect::MakeEmpty()
                     : SkIRect::MakeLTRB(x0, y0, x1 + 1, y1 + 1);
    const bool clipped = !out.ink.isEmpty() &&
                         (out.ink.fLeft == 0 || out.ink.fTop == 0 ||
                          out.ink.fRight == w || out.ink.fBottom == h);
    if (!clipped || attempt >= kPadRetries || (pad.x <= 0 && pad.y <= 0))
      return out;
    pad.x = std::max(1, pad.x * 2);
    pad.y = std::max(1, pad.y * 2);
  }
}

/** A baked run: a 1-bit A8 image plus the numbers a caller needs to place
 *  and advance past it. */
struct Mask {
  sk_sp<SkImage> image;
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
 *  @p threshold is in coverage units [0, 1] and is **inert under
 *  `aliased`** — see trap 2. It matters only when you deliberately shape
 *  antialiased and quantise afterwards, which `xcom_battlescape` does with
 *  two levels rather than one. */
inline Mask threshold(const Coverage &cov, float threshold = 0.5f,
                      bool cropToInk = true) {
  Mask m;
  if (!cov.valid() || cov.ink.isEmpty())
    return m;
  const SkIRect r = cropToInk ? cov.ink
                              : SkIRect::MakeWH(cov.width(), cov.height());
  SkBitmap a8;
  a8.allocPixels(SkImageInfo::MakeA8(r.width(), r.height()));
  a8.eraseColor(SK_ColorTRANSPARENT);
  for (int y = 0; y < r.height(); ++y)
    for (int x = 0; x < r.width(); ++x)
      *a8.getAddr8(x, y) =
          cov.alphaAt(r.fLeft + x, r.fTop + y) >= threshold ? 255 : 0;
  a8.setImmutable();
  m.image = a8.asImage();
  m.w = r.width();
  m.h = r.height();
  m.inkX = r.fLeft;
  m.inkY = r.fTop;
  m.advance = cov.advance.width();
  return m;
}

/** The whole bake in one call — what `thaumonomicon::bakeText` is. */
inline Mask bakeRun(std::u8string_view run, sigil::weave::FontContext &fonts,
                    const sigil::weave::TextStyle &style, Pad pad = {},
                    float thresholdAt = 0.5f) {
  return threshold(coverage(run, fonts, style, pad), thresholdAt);
}

/** How a baked mask is presented. */
struct Present {
  SkColor4f colour = {1, 1, 1, 1};
  /** INTEGER, please — trap 4. A bitmap face at 1.5× is a blurry bitmap
   *  face. */
  float scale = 1.0f;
  /** A second pass underneath, offset by this many DESTINATION px, with
   *  the colour's RGB multiplied by `shadowMul`. Minecraft's own
   *  `FontRenderer` offsets by +1 GUI px and multiplies by 0.25
   *  (`(color & 16579836) >> 2`), which is `thaumonomicon.cpp:1218-1233`.
   *  Zero offset = no shadow pass. */
  SkVector shadowOffset = {0, 0};
  float shadowMul = 0.25f;
};

/** Draw a baked mask at @p at (top-left), immediate mode.
 *
 *  An A8 image drawn through `drawImageRect` modulates the PAINT's colour,
 *  which is why all three studies blit rather than fill: a mask carries
 *  coverage, and the colour stays a paint property so one bake serves
 *  every tint the plate needs. */
inline void draw(SkCanvas &canvas, const Mask &m, SkPoint at,
                 const Present &p = {}) {
  if (!m.image)
    return;
  const SkRect dst = SkRect::MakeXYWH(at.fX, at.fY, (float)m.w * p.scale,
                                      (float)m.h * p.scale);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  SkPaint paint;
  paint.setAntiAlias(false);
  if (p.shadowOffset.fX != 0 || p.shadowOffset.fY != 0) {
    paint.setColor4f({p.colour.fR * p.shadowMul, p.colour.fG * p.shadowMul,
                      p.colour.fB * p.shadowMul, p.colour.fA},
                     nullptr);
    canvas.drawImageRect(m.image,
                         dst.makeOffset(p.shadowOffset.fX, p.shadowOffset.fY),
                         nearest, &paint);
  }
  paint.setColor4f(p.colour, nullptr);
  canvas.drawImageRect(m.image, dst, nearest, &paint);
}

/** The same as a retained leaf sized to the mask, for a STATIC run. The
 *  node is `m.w × scale` by `m.h × scale`; place it with `.at()`/`.rect()`
 *  like any other absolute node.
 *
 *  Note this is a `custom()` leaf, so it never records a picture and its
 *  program runs every frame it is visible — cheap here (one
 *  `drawImageRect`) but not free. A static run that never changes colour
 *  is better as `.cache(Cache::Texture)` on its parent. */
inline Element masked(const Mask &m, const Present &p = {}) {
  if (!m.image)
    return box().width(0).height(0);
  return custom([m, p](SkCanvas &canvas, const PaintContext &) {
           draw(canvas, m, {0, 0}, p);
         })
      .width((float)m.w * p.scale)
      .height((float)m.h * p.scale);
}

// ---------------------------------------------------------------------------
// The 96-cell font — what a LIVE readout needs.

/** One baked cell. */
struct Cell {
  sk_sp<SkImage> mask;
  int w = 0, h = 0;
  /** The shaped advance, rounded — NOT the ink width. */
  int advance = 0;
};

/** ASCII 32..127, baked once. */
struct PixFont {
  std::array<Cell, 96> cells{};
  /** The tallest cell — a line box for the caller. */
  int lineHeight = 0;
  /** The widest DIGIT advance, shared by all ten. Trap 3. */
  int digitAdvance = 0;

  const Cell &cell(char c) const {
    const int i = (int)(unsigned char)c - 32;
    static const Cell empty{};
    return (i < 0 || i >= 96) ? empty : cells[(size_t)i];
  }
};

/** Bake every printable ASCII cell in @p style.
 *
 *  Space is given an advance and no mask, which is not the same as a cell
 *  that failed to bake — `vagrant_story_target.cpp:471` sets it to
 *  `size * 0.34 * condense` because a shaped lone space measures zero ink
 *  and the loop would otherwise crop it to nothing. @p spaceRatio is that
 *  number; the default matches. */
inline PixFont bakeFont(sigil::weave::FontContext &fonts,
                        const sigil::weave::TextStyle &style, Pad pad = {3, 3},
                        float thresholdAt = 0.5f, float spaceRatio = 0.34f) {
  PixFont f;
  const float size = style.shaping.fontSize;
  const float condense = style.shaping.scaleX;
  for (int i = 0; i < 96; ++i) {
    const char32_t ch = (char32_t)(32 + i);
    if (ch == U' ') {
      f.cells[(size_t)i].advance =
          std::max(1, (int)std::lround(size * spaceRatio * condense));
      continue;
    }
    const char c = (char)ch;
    const std::u8string one(1, (char8_t)c);
    const Coverage cov = coverage(one, fonts, style, pad);
    const Mask m = threshold(cov, thresholdAt);
    Cell &cell = f.cells[(size_t)i];
    cell.mask = m.image;
    cell.w = m.w;
    cell.h = m.h;
    cell.advance = std::max(1, (int)std::lround(cov.advance.width()));
    f.lineHeight = std::max(f.lineHeight, cell.h);
  }
  for (int d = 0; d < 10; ++d)
    f.digitAdvance =
        std::max(f.digitAdvance, f.cells[(size_t)('0' - 32 + d)].advance);
  return f;
}

/** How a run is spaced and where it lands. */
struct Blit {
  /** px added after every cell. */
  float track = 1.0f;
  /** Digits take `PixFont::digitAdvance` instead of their own — trap 3.
   *  On for a readout, off for prose. */
  bool tabularDigits = true;
  /** Round every pen position to a multiple of this many px (0 = off).
   *  A bitmap face that lands off the device grid is a resampled bitmap
   *  face. `vagrant_story_target` snaps to its own 512-grid; pass the
   *  grid pitch, or `1` for the device grid. */
  float snap = 0.0f;
};

namespace detail {
inline float snapTo(float v, float grid) {
  return grid > 0 ? std::round(v / grid) * grid : v;
}
} // namespace detail

/** Advance width of @p s without drawing it. */
inline float widthOf(const PixFont &f, std::string_view s, const Blit &b = {}) {
  float w = 0;
  for (char raw : s) {
    const int i = (int)(unsigned char)raw - 32;
    if (i < 0 || i >= 96)
      continue;
    const bool digit = raw >= '0' && raw <= '9';
    w += (float)(digit && b.tabularDigits ? f.digitAdvance
                                          : f.cells[(size_t)i].advance) +
         b.track;
  }
  return w;
}

/** Draw @p s at @p at (top-left of the line box) and return the advance.
 *
 *  Immediate-mode, for inside a `custom()` leaf — which is the whole point:
 *  a live readout reads its bound `Output` here and draws the number, with
 *  nothing re-described and nothing reconciled. */
inline float blit(SkCanvas &canvas, const PixFont &f, SkPoint at,
                  std::string_view s, SkColor4f colour, const Blit &b = {}) {
  SkPaint p;
  p.setAntiAlias(false);
  p.setColor4f(colour, nullptr);
  const SkSamplingOptions nearest(SkFilterMode::kNearest);
  const float x0 = detail::snapTo(at.fX, b.snap);
  float x = x0;
  const float y = detail::snapTo(at.fY, b.snap);
  for (char raw : s) {
    const int i = (int)(unsigned char)raw - 32;
    if (i < 0 || i >= 96)
      continue;
    const Cell &cell = f.cells[(size_t)i];
    const bool digit = raw >= '0' && raw <= '9';
    if (cell.mask)
      canvas.drawImageRect(
          cell.mask, SkRect::MakeXYWH(x, y, (float)cell.w, (float)cell.h),
          nearest, &p);
    x = detail::snapTo(
        x + (float)(digit && b.tabularDigits ? f.digitAdvance : cell.advance) +
            b.track,
        b.snap);
  }
  return x - x0;
}

} // namespace sigil::compose::kit
