#pragma once

/** @file
 * SigilCompose KIT — keeping type readable where it crosses the drawing.
 *
 * Seven sites, six files, three mechanisms, one problem: an annotation has
 * to sit ON the artwork and the artwork eats it.
 *
 * | site | mechanism |
 * |---|---|
 * | `nightingale_coxcomb.cpp:218-231` | stroke underlay, round join (as weight) |
 * | `psx_doom_fire.cpp:365` | `PaintLayer::outline` underlay |
 * | `gallery/ScenesPersona.h:180` | `PaintLayer::outline` underlay |
 * | `gallery/ScenesVeloren.h:106-125` | offset black fill underlay, "not optional" |
 * | `gallery/ScenesY2k.h:77,273,312` | ground underlay + contour underlay |
 * | `slitscan_2001.cpp:1688-1701` | **two `drawString` passes by hand**, 2.2 px |
 * | `astral_tome.cpp:492` | a padded opaque PLATE behind the run |
 *
 * Four of those reach `weave::PaintStyle::addUnderlay` and three do not —
 * and the three that do not are the interesting ones. `slitscan_2001`
 * could not, because its caption lives inside a `PaintProgram` drawing raw
 * `SkFont` strings, so it re-derived the halo with two `drawString` calls
 * and a hand-built stroke paint. `astral_tome` chose a plate instead
 * because its ground is a star field and a 2 px halo would not have been
 * enough. So this header ships all three spellings, and the canvas one is
 * not an afterthought.
 *
 * ## Warning — a halo makes a CHOICE cheap
 *
 * Both researcher retrospectives converged on the same rule: attach a
 * caveat to any component that makes a decision cheap rather than a chore
 * cheap. This one qualifies, and its own author already wrote the caveat
 * at `slitscan_2001.cpp:1685-1690`:
 *
 * > HALOED. The carriage travels the whole rail over tau, so this caption
 * > is printed through by the camera body at some phase of every sweep —
 * > **and there is no clear band to move it to**: above is the clock
 * > block, below is the leader pair, the worm-gear rail and the scale
 * > note. A 2 px knockout in the panel colour is what a drafting plate
 * > does when a note has to cross the drawing.
 *
 * The load-bearing clause is "there is no clear band to move it to". A
 * halo is the answer AFTER that search fails. Made one word long, it
 * becomes the answer instead of the search, and a plate where every label
 * is haloed is a plate whose layout gave up. `astral_tome` is the
 * cautionary case in the other direction: it invented `scrimLabel()` and
 * then failed to use it on its own coordinate annotations, which is what a
 * helper nobody reaches for looks like.
 *
 * Use it where the label MUST cross the drawing. Move the label first.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Studio.h"
#include "sigilcompose/Util.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>

#include <string_view>

namespace sigil::compose::kit {

/** A knockout ring around the glyphs, in the GROUND's colour. */
struct Halo {
  /** The colour to knock out in — the ground the type sits on, not a
   *  contrasting outline. A drafting plate knocks out in the paper. */
  SkColor4f colour = {1, 1, 1, 1};
  /** Total stroke width in px, so the visible halo is half this on each
   *  side. `slitscan_2001` measured 2.2 px as what its 7.5 px mono needed;
   *  below ~1.5 px the counters of small type fill in. */
  float width = 2.2f;
  /** Round is what a knockout wants — a mitred join spikes at every sharp
   *  vertex and reads as a burr. */
  SkPaint::Join join = SkPaint::kRound_Join;
};

/** A displaced solid copy underneath — the game-HUD spelling, where the
 *  ground is arbitrary terrain and a symmetric halo would grey the glyph.
 *  `ScenesVeloren.h:118-124` explains why it is mandatory there. */
struct Shade {
  SkColor4f colour = {0, 0, 0, 0.9f};
  SkVector offset = {1, 1};
};

/** @p style with a halo pass beneath its glyphs. Returns a copy; the
 *  original is untouched, so one base style can spawn haloed and plain
 *  variants without a mutable helper. */
inline sigil::weave::TextStyle haloed(sigil::weave::TextStyle style,
                                      const Halo &halo = {}) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(halo.colour, nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(halo.width);
  p.setStrokeJoin(halo.join);
  style.paint.addUnderlay(sigil::weave::PaintLayer(std::move(p)));
  return style;
}

/** @p style with a displaced solid copy beneath its glyphs. */
inline sigil::weave::TextStyle shaded(sigil::weave::TextStyle style,
                                      const Shade &shade = {}) {
  sigil::weave::PaintLayer layer;
  layer.paint.setAntiAlias(true);
  layer.paint.setColor4f(shade.colour, nullptr);
  layer.offset = shade.offset;
  style.paint.addUnderlay(std::move(layer));
  return style;
}

/** The same trick with the STROKE pass on top of the fill — which is not a
 *  halo but the other thing `nightingale_coxcomb.cpp:218-231` used it for:
 *  thickening a face at the glyph level, because the engraved title on the
 *  1858 plate is heavier than any installed digital face. Same three lines,
 *  opposite intent, so it gets its own name rather than a bool. */
inline sigil::weave::TextStyle emboldened(sigil::weave::TextStyle style,
                                          float width, SkColor4f colour) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(colour, nullptr);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setStrokeJoin(SkPaint::kRound_Join);
  style.paint.addUnderlay(sigil::weave::PaintLayer(std::move(p)));
  return style;
}

// ---------------------------------------------------------------------------
// The plate, for a ground a halo cannot survive.

/** An opaque sill behind a whole run. */
struct Scrim {
  Fill fill = Fill::color({0.02f, 0.02f, 0.035f, 0.74f});
  float paddingX = 3.0f;
  float paddingY = 3.0f;
  /** Corner radius; 0 is the drafting-plate square. */
  float radius = 0.0f;
};

/** Wrap @p run in a padded plate. This is `astral_tome.cpp:492
 *  scrimLabel()`, whose own doc explains the case: the tome's ground is a
 *  star field with links crossing it, and *"without one, an annotation
 *  lands on a link and is gone"*.
 *
 *  A halo would be cheaper — this is a second node and a fill. Reach for
 *  it when the ground is BUSY rather than merely crossed: a knockout works
 *  against linework, and stops working against texture.
 *
 *  Note `weave::Decoration::Kind::kHighlight` is the third option and
 *  lives inside the text system — a band from ascent to descent drawn
 *  beneath every glyph pass, which needs no extra node. It takes no
 *  padding, which is exactly why `astral_tome` did not use it. */
inline Element scrim(Element run, const Scrim &s = {}) {
  Element plate = box().padding(s.paddingX, s.paddingY).fill(s.fill).child(
      std::move(run));
  if (s.radius > 0)
    plate.corners({s.radius});
  return plate;
}

// ---------------------------------------------------------------------------
// Immediate mode — for a caption inside a PaintProgram.

/** Draw @p s haloed, at an SkFont baseline origin.
 *
 *  This is `slitscan_2001.cpp:1690-1701` exactly: build a stroke paint in
 *  the ground colour, draw the string with it, then draw it again in the
 *  ink. It exists because a `custom()` leaf has a canvas and no text node,
 *  and the four `addUnderlay` sites above are unreachable from there.
 *
 *  @p ink is used as configured — pass a paint you have already coloured
 *  and blended, so a caption inside a `kPlus` program keeps its blend. */
inline void drawHaloed(SkCanvas &canvas, std::string_view s, SkPoint at,
                       const SkFont &font, const SkPaint &ink,
                       const Halo &halo = {}) {
  SkPaint h;
  h.setAntiAlias(true);
  h.setColor4f(halo.colour, nullptr);
  h.setStyle(SkPaint::kStroke_Style);
  h.setStrokeWidth(halo.width);
  h.setStrokeJoin(halo.join);
  canvas.drawSimpleText(s.data(), s.size(), SkTextEncoding::kUTF8, at.fX,
                        at.fY, font, h);
  canvas.drawSimpleText(s.data(), s.size(), SkTextEncoding::kUTF8, at.fX,
                        at.fY, font, ink);
}

/** The colour spelling, for the common case where the ink is a flat
 *  antialiased fill. */
inline void drawHaloed(SkCanvas &canvas, std::string_view s, SkPoint at,
                       const SkFont &font, SkColor4f ink,
                       const Halo &halo = {}) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(ink, nullptr);
  drawHaloed(canvas, s, at, font, p, halo);
}

} // namespace sigil::compose::kit
