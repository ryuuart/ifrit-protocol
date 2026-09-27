#pragma once

/** @file
 * @ingroup compose-kit
 *
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
 * addUnderlay`, so they apply to any text node. A caption a pen program
 * draws is a pen's text, stroked in the ground and then filled in the ink.
 *
 * ## A halo makes a CHOICE cheap, so read this first
 *
 * A halo is the answer AFTER the search for a clear band to move the label
 * into has failed — not instead of that search. A drawing where every
 * label is haloed is a drawing whose layout gave up. Move the label first;
 * use this where the label MUST cross the artwork.
 */

#include <glm/vec2.hpp>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Paint.h>
#include <sigilgeometry/path/Stroke.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/style/TextStyle.h>
#include <sigilweave/style/Type.h>

namespace sigil::compose::kit {

/** A knockout ring around the glyphs, in the GROUND's colour. */
struct Halo {
  /** The colour to knock out in — the ground the type sits on, not a
   *  contrasting outline. A drafting plate knocks out in the paper. */
  material::Color colour = {1, 1, 1, 1};
  /** Total stroke width in px, so the visible halo is half this on each
   *  side. The default suits mono type around 7–8 px; below about 1.5 px
   *  total the counters of small type start filling in. */
  float width = 2.2f;
  /** Round is what a knockout wants — a mitred join spikes at every sharp
   *  vertex and reads as a burr. */
  geometry::path::Join join = geometry::path::Join::Round;
};

/** A displaced solid copy underneath — the game-HUD spelling, for a ground
 *  of arbitrary terrain where a symmetric halo would grey the glyph
 *  instead of separating it. */
struct Shade {
  material::Color colour = {0, 0, 0, 0.9f};
  glm::vec2 offset = {1, 1};
};

/** @p style with a halo pass beneath its glyphs. Returns a copy; the
 *  original is untouched, so one base style can spawn haloed and plain
 *  variants without a mutable helper. */
sigil::weave::TextStyle haloed(sigil::weave::TextStyle style,
                                      const Halo& halo = {});

/** @p style with a displaced solid copy beneath its glyphs. */
sigil::weave::TextStyle shaded(sigil::weave::TextStyle style,
                                      const Shade& shade = {});

/** The same two on a PARTIAL: the pass is stated on the partial's own
 *  underlays, which replace whatever the text would have inherited, and a
 *  colour of transparent black is drawn in the colour the text is set in
 *  — a halo that follows the ink wherever the partial lands. */
sigil::weave::Type haloed(sigil::weave::Type type,
                                 const Halo& halo = {});
sigil::weave::Type shaded(sigil::weave::Type type,
                                 const Shade& shade = {});

/** The same underlay used for weight rather than for separation: a stroke
 *  in the INK's colour thickens the face at the glyph level, which is how
 *  you match an engraved title heavier than any installed digital face.
 *  Same three lines as `haloed`, opposite intent, so it has its own name
 *  rather than a flag — the colour a caller passes is the difference, and
 *  a flag would not make that visible. */
sigil::weave::TextStyle emboldened(sigil::weave::TextStyle style,
                                          float width, material::Color colour);
/** On a partial; a @p colour of transparent black is the ink the text is
 *  set in, which is what a weight usually wants. */
sigil::weave::Type emboldened(sigil::weave::Type type, float width,
                                     material::Color colour = {0, 0, 0, 0});

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
  Element plate = box()
                      .padding(s.paddingY, s.paddingX)
                      .fill(s.fill)
                      .children({std::move(run)});
  if (s.radius > 0) plate.borderRadius({s.radius});
  return plate;
}

}  // namespace sigil::compose::kit
