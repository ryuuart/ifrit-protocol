#pragma once

/** @file
 * SigilCompose KIT — keeping type readable where it crosses the drawing.
 *
 * One problem, three mechanisms: an annotation has to sit ON the artwork
 * and the artwork eats it.
 *
 *  - a HALO — a knockout ring in the ground's colour, stroked under the
 *    glyphs;
 *  - a SHADE — a displaced solid copy underneath, for a ground so varied
 *    that a symmetric halo would just grey the glyph;
 *  - a SCRIM — an opaque plate behind the whole run, for a ground a
 *    knockout cannot survive.
 *
 * The first two are text-style transforms built on `weave::PaintStyle::
 * addUnderlay`, so they apply to any text node. There is also an
 * immediate-mode halo at the bottom of this file, because a caption drawn
 * inside a `custom()` leaf has a canvas and no text node, and none of the
 * underlay path is reachable from there.
 *
 * ## A halo makes a CHOICE cheap, so read this first
 *
 * A halo is the answer AFTER the search for a clear band to move the label
 * into has failed — not instead of that search. A drawing where every
 * label is haloed is a drawing whose layout gave up. Move the label first;
 * use this where the label MUST cross the artwork.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkPaint.h>
#include <include/core/SkTextBlob.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>

#include <initializer_list>
#include <string_view>

namespace sigil::compose::kit {

/** A knockout ring around the glyphs, in the GROUND's colour. */
struct Halo {
  /** The colour to knock out in — the ground the type sits on, not a
   *  contrasting outline. A drafting plate knocks out in the paper. */
  SkColor4f colour = {1, 1, 1, 1};
  /** Total stroke width in px, so the visible halo is half this on each
   *  side. The default suits mono type around 7–8 px; below about 1.5 px
   *  total the counters of small type start filling in. */
  float width = 2.2f;
  /** Round is what a knockout wants — a mitred join spikes at every sharp
   *  vertex and reads as a burr. */
  SkPaint::Join join = SkPaint::kRound_Join;
};

/** A displaced solid copy underneath — the game-HUD spelling, for a ground
 *  of arbitrary terrain where a symmetric halo would grey the glyph
 *  instead of separating it. */
struct Shade {
  SkColor4f colour = {0, 0, 0, 0.9f};
  SkVector offset = {1, 1};
};

/** @p style with a halo pass beneath its glyphs. Returns a copy; the
 *  original is untouched, so one base style can spawn haloed and plain
 *  variants without a mutable helper. */
inline sigil::weave::TextStyle haloed(sigil::weave::TextStyle style,
                                      const Halo& halo = {}) {
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
                                      const Shade& shade = {}) {
  sigil::weave::PaintLayer layer;
  layer.paint.setAntiAlias(true);
  layer.paint.setColor4f(shade.colour, nullptr);
  layer.offset = shade.offset;
  style.paint.addUnderlay(std::move(layer));
  return style;
}

/** The same underlay used for weight rather than for separation: a stroke
 *  in the INK's colour thickens the face at the glyph level, which is how
 *  you match an engraved title heavier than any installed digital face.
 *  Same three lines as `haloed`, opposite intent, so it has its own name
 *  rather than a flag — the colour a caller passes is the difference, and
 *  a flag would not make that visible. */
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

/** Wrap @p run in a padded plate.
 *
 *  A halo would be cheaper — this is a second node and a fill. Reach for
 *  it when the ground is BUSY rather than merely crossed: a knockout works
 *  against linework and stops working against texture, such as a star
 *  field or a photograph, where an annotation simply disappears.
 *
 *  `weave::Decoration::Kind::kHighlight` is a third option that lives
 *  inside the text system: a band from ascent to descent drawn beneath
 *  every glyph pass, needing no extra node. It takes no padding, so reach
 *  for this one when the plate has to stand off the type. */
inline Element scrim(Element run, const Scrim& s = {}) {
  Element plate =
      box().padding(s.paddingX, s.paddingY).fill(s.fill).child(std::move(run));
  if (s.radius > 0) plate.corners({s.radius});
  return plate;
}

// ---------------------------------------------------------------------------
// Immediate mode — for a caption inside a PaintProgram.

/** Draw @p s haloed, at an SkFont baseline origin: a stroke pass in the
 *  ground colour, then the same string again in the ink. For a caption
 *  inside a `custom()` leaf, which has a canvas and no text node and so
 *  cannot reach the underlay path above.
 *
 *  @p ink is used exactly as configured, so pass a paint you have already
 *  coloured and blended and a caption inside a `kPlus` program keeps its
 *  blend. */
inline void drawHaloed(SkCanvas& canvas, std::string_view s, SkPoint at,
                       const SkFont& font, const SkPaint& ink,
                       const Halo& halo = {}) {
  SkPaint h;
  h.setAntiAlias(true);
  h.setColor4f(halo.colour, nullptr);
  h.setStyle(SkPaint::kStroke_Style);
  h.setStrokeWidth(halo.width);
  h.setStrokeJoin(halo.join);
  canvas.drawSimpleText(s.data(), s.size(), SkTextEncoding::kUTF8, at.fX, at.fY,
                        font, h);
  canvas.drawSimpleText(s.data(), s.size(), SkTextEncoding::kUTF8, at.fX, at.fY,
                        font, ink);
}

/** The colour spelling, for the common case where the ink is a flat
 *  antialiased fill. */
inline void drawHaloed(SkCanvas& canvas, std::string_view s, SkPoint at,
                       const SkFont& font, SkColor4f ink,
                       const Halo& halo = {}) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(ink, nullptr);
  drawHaloed(canvas, s, at, font, p, halo);
}

/** One line of a haloed block. */
struct Line {
  std::string_view text;
  /** SkFont baseline origin. */
  SkPoint at{0, 0};
};

/** A BLOCK of haloed lines: every halo first, then every ink.
 *
 *  **Not a convenience — pass the whole block.** Calling the single-line
 *  form in a loop draws line 2's knockout AFTER line 1's ink, so the
 *  second line's halo eats the first line's descenders. Any halo wider
 *  than the leading minus the descent has this, which for a tightly-led
 *  note is most of them, and the damage looks like a font bug rather than
 *  an ordering one. */
inline void drawHaloed(SkCanvas& canvas, std::initializer_list<Line> lines,
                       const SkFont& font, const SkPaint& ink,
                       const Halo& halo = {}) {
  SkPaint h;
  h.setAntiAlias(true);
  h.setColor4f(halo.colour, nullptr);
  h.setStyle(SkPaint::kStroke_Style);
  h.setStrokeWidth(halo.width);
  h.setStrokeJoin(halo.join);
  for (const Line& l : lines)
    canvas.drawSimpleText(l.text.data(), l.text.size(), SkTextEncoding::kUTF8,
                          l.at.fX, l.at.fY, font, h);
  for (const Line& l : lines)
    canvas.drawSimpleText(l.text.data(), l.text.size(), SkTextEncoding::kUTF8,
                          l.at.fX, l.at.fY, font, ink);
}

}  // namespace sigil::compose::kit
