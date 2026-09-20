#pragma once

/** @file
 * @ingroup compose-core
 *
 * What a text leaf CARRIES beyond its words: the per-glyph tracks, the
 * siblings anchored to its units, the readings beside it, the frame
 * chain it fills into, the curve it is laid along, the ranges it
 * restyles, and the copy of itself at rest.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilmotion/values/Animatable.h>  // choreograph::Output
#include <sigilweave/style/Style.h>

#include <cstdint>
#include <string_view>

namespace sigil::weave {
class Selector;
}

namespace sigil::compose {

class Element;
struct Track;
struct Annotation;
struct TextPath;

/** THE TEXT CONTENT VERBS. Everything here is about THIS passage —
 *  what is anchored to it, what is set beside it, where it continues,
 *  and which of its own glyphs move or are dressed differently. None
 *  of it is a property another node could inherit or a rule restate,
 *  which is what separates these from the text properties. */
template <class Derived>
class TextContentVerbs {
 public:
  /** APPENDS a text-fx track: which glyphs, what deviation from rest,
   *  how the beats spread, and the master progress that drives it.
   *  Several tracks compose per glyph — dx/dy and rotation ADD, scale
   *  and alpha MULTIPLY — in declaration order, each keeping its own
   *  transition slot. */
  Derived& fx(Track track);
  /** Drive a variable-font axis from a bound output at DRAW time —
   *  paint-only volatility, no reshape, no relayout. An
   *  advance-variant axis is REFUSED with a warning and the text draws
   *  at its shaped coordinates. It takes a bare output, not an
   *  animatable: a drive IS a live binding. */
  Derived& variationDrive(const char (&tag)[5],
                          const choreograph::Output<float>* value);
  /** A SIBLING ANCHORED TO A UNIT OF THE TEXT: a caret, a callout, a
   *  tick, a rule standing at a word's edge. @p what becomes a child of
   *  this node whose PARENT BOX is the rect @p where resolves to. It
   *  reserves nothing — the text is laid out as though the mark were
   *  not there. */
  Derived& mark(sigil::weave::Selector where, Element what);
  /** A READING SET BESIDE THE TYPE — furigana over a compound,
   *  emphasis dots down a column, a gloss under a phrase. A reading is
   *  PART OF THE TEXT: where it reserves, its band goes into the base's
   *  strut before the base is broken. */
  Derived& annotate(Annotation reading);
  /** THE FRAME THIS ONE FILLS INTO — the next link of a chain over one
   *  `weave::Story`. Each frame fills from where the one before it
   *  stopped. A frame that threads somewhere has a remainder by design
   *  and draws no overflow marker; the last frame of a chain keeps
   *  its. Last-wins. */
  Derived& thread(std::string_view key);
  /** THIS FRAME OPENS A BALANCED RUN of its chain — itself and every
   *  frame after it up to the next frame that opens one, or the chain's
   *  end. @p throughLine is what the run must hold, as a story-relative
   *  line number; the default holds all of it. The frames must declare
   *  a depth in pixels. */
  Derived& balanceChain(uint32_t throughLine = ~0u);
  /** Lay the run out along a PATH instead of a line. Single-line runs;
   *  the node's own box still sizes the path, so give it the box the
   *  curve should be inscribed in. */
  Derived& onPath(TextPath spec);
  /** Repaint the range this selector finds — a colour, a shader, an
   *  underline, an added glow pass. PAINT ONLY: it never re-shapes, so
   *  the glyphs are exactly the glyphs the unrestyled text shaped. */
  Derived& spanPaint(sigil::weave::Selector where,
                     sigil::weave::PaintStyle paint);
  /** Restyle the range this selector finds with a complete TextStyle —
   *  a different face, size, weight or tracking as well as paint. It
   *  re-shapes, and only the words the range covers. */
  Derived& spanStyle(sigil::weave::Selector where,
                     sigil::weave::TextStyle style);
  /** Restyle the range with a PARTIAL: the fields it names over the
   *  style the range is set in, the rest standing. One naming no
   *  shaping field is a repaint and never re-shapes. */
  Derived& spanStyle(sigil::weave::Selector where, sigil::weave::Type partial);
  /** THIS LEAF AS IT STANDS AT REST, as a second element that can
   *  stand beside it in one tree: the same content, style, measure and
   *  layout, carrying nothing that deviates or restyles a glyph at
   *  paint time. The key takes `-rest` after it. */
  [[nodiscard]] Element atRest() const;

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  const Derived& self() const { return static_cast<const Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
