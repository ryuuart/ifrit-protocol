#pragma once

/** @file
 * @ingroup compose-core
 *
 * What a text leaf CARRIES beyond its words: the per-glyph tracks, the
 * siblings anchored to its units, the readings beside it, the frame
 * chain it fills into, the curve it is laid along, the spans it
 * restyles, and the copy of itself at rest.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/SpanDeclarations.h>
#include <sigilcompose/core/verbs/Node.h>
#include <sigilcompose/core/verbs/TextStyle.h>
#include <sigilmotion/values/Animatable.h>  // choreograph::Output
#include <sigilweave/style/Style.h>

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

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
  Derived& textAttach(sigil::weave::Selector where, Element what);
  /** A READING SET BESIDE THE TYPE — furigana over a compound,
   *  emphasis dots down a column, a gloss under a phrase. A reading is
   *  PART OF THE TEXT: where it reserves, its band goes into the base's
   *  strut before the base is broken. */
  Derived& textAnnotation(Annotation reading);
  /** THE FRAME THIS ONE FILLS INTO — the next link of a chain over one
   *  `weave::Story`. Each frame fills from where the one before it
   *  stopped. A frame that threads somewhere has a remainder by design
   *  and draws no overflow marker; the last frame of a chain keeps
   *  its. Last-wins. */
  Derived& textThreadTo(std::string_view key);
  /** THIS FRAME OPENS A BALANCED RUN of its chain — itself and every
   *  frame after it up to the next frame that opens one, or the chain's
   *  end. @p throughLine is what the run must hold, as a story-relative
   *  line number; the default holds all of it. The frames must declare
   *  a depth in pixels. */
  Derived& textThreadBalance(uint32_t throughLine = ~0u);
  /** Lay the run out along a PATH instead of a line. Single-line runs;
   *  the node's own box still sizes the path, so give it the box the
   *  curve should be inscribed in. */
  Derived& textOnPath(TextPath spec);
  /** RESTYLE THE RANGE @p where FINDS with @p declarations: the font
   *  fields and the ink they state, over the style the range is set in,
   *  the rest standing. It re-shapes only where a shaping field was
   *  declared — a face, a size, a weight, a tracking — and only the
   *  words the range covers; a colour, a decoration or a paint alone is
   *  a repaint. Where two spans overlap the later wins, field by field
   *  for the paint. */
  Derived& span(sigil::weave::Selector where, SpanDeclarations declarations);
  /** THIS LEAF AS IT STANDS AT REST, as a second leaf that can stand
   *  beside it in one tree: the same content, style, measure and
   *  layout, carrying nothing that deviates or restyles a glyph at
   *  paint time. The key takes `-rest` after it. */
  [[nodiscard]] Derived atRest() const;

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  const Derived& self() const { return static_cast<const Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

/** A TEXT LEAF: a passage, and everything a passage alone can say. It
 *  is a node like any other — it lays out, takes a fill, a mask and a
 *  transform — and it adds the text properties a rule could restate
 *  and the content verbs nothing else could. It CONVERTS to `Element`,
 *  so a leaf drops into any `children({…})` block and any container
 *  that takes a node; hold it as a `Text` for as long as the text
 *  verbs are still to be written. */
class Text : public detail::Declaring,
             public NodeVerbs<Text>,
             public TextStyleVerbs<Text>,
             public TextContentVerbs<Text> {
 public:
  /** @private the factories' door */
  explicit Text(std::shared_ptr<detail::ElementNode> n)
      : detail::Declaring(std::move(n)) {}

  operator Element() const;  // NOLINT: implicit by design (a leaf is a node)

 private:
  friend struct detail::NodeAccess;
};

}  // namespace sigil::compose
