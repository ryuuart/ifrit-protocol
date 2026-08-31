#pragma once

/** @file
 * SigilCompose one-shot verbs — a tree taken without a live composer:
 * `snapshot` bakes it to a picture, `measure` answers its intrinsic size,
 * `metrics` reads a face's vertical metrics, and `measureRun` / `runPens`
 * shape one run into per-glyph advances and pen positions.
 */

#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilweave/style/Style.h>

#include <string_view>
#include <vector>

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

/** One-shot element render: reconciles, lays out, and records the
 *  paint into a picture. With an empty @p maxSize the tree takes its
 *  intrinsic (content) size; a non-empty one bounds it (root max
 *  dims). Bindings are sampled at their current values; transitions
 *  don't run — there is no live timeline. This is the bake primitive
 *  behind ContourWalk element stamps, and generally "an element tree
 *  as a brush".
 *
 *  THE INTRINSIC SIZE COMES FROM THE ROOT'S CHILDREN, not from the root's
 *  own dims, and this catches people out: `snapshot(box().width(32).
 *  fill(…))` bakes at CONTENT size and quietly ignores the 32. Wrap the
 *  sized tree in a plain `box().child(...)` and the dims are honoured,
 *  because they now belong to a child. */
sk_sp<SkPicture> snapshot(const Element& root, sigil::weave::FontContext& fonts,
                          SkSize maxSize = SkSize::MakeEmpty());

/** A face's vertical metrics at a given size, without laying anything out.
 *
 *  A compose text node's top is the LINE BOX top, while type is usually
 *  positioned against its CAP TOP — so aligning text to a coordinate taken
 *  from a design or a reference image needs the slack between the two, and
 *  `measure()` returns only an `SkSize`. `capSlack()` below is that
 *  number.
 *
 *  `capHeight` and `xHeight` are what the face itself reports; both fall
 *  back to a fraction of the ascent when a face reports zero, which some
 *  do. All values are positive distances in px, with `ascent` measured
 *  above the baseline. */
struct TextMetrics {
  float ascent = 0;      ///< baseline to the top of the em box (positive)
  float descent = 0;     ///< baseline to the bottom (positive)
  float leading = 0;     ///< the face's recommended extra line gap
  float capHeight = 0;   ///< baseline to the top of a flat capital
  float xHeight = 0;     ///< baseline to the top of a lowercase x
  float lineHeight = 0;  ///< ascent + descent + leading
  /** How far the line box's top sits above the cap top — the number that
   *  turns "place this at the reference's y" into a coordinate. */
  float capSlack() const { return ascent - capHeight; }
};

TextMetrics metrics(const sigil::weave::TextStyle& style,
                    sigil::weave::FontContext& fonts);

/** Shape ONE RUN without building an Element: per-glyph advances in px, in
 *  visual order, through the same shaping path a text() leaf takes, so
 *  kerning and ligatures are real. The result's length is the GLYPH count,
 *  which is neither the byte nor the code-point count.
 *
 *  Pen positions are the running prefix sums, so hand-placing N glyphs
 *  costs one layout here rather than N text() leaves and N `measure()`
 *  calls. A space between two words is a gap the flow leaves rather than a
 *  glyph, so it rides the advance of the glyph before it and the sums stay
 *  true across a whole sentence; the sums therefore add up to the run's
 *  laid-out extent, not to the ink alone. The pen starts at the FIRST
 *  GLYPH, so leading whitespace is no part of the run.
 *
 *  Single style, no wrapping: the run is laid on one unbounded line. A
 *  '\n' starts a new line and resets the positions after it, so pass a
 *  RUN and not a paragraph.
 *
 *  This is the STATIC answer, for a run that is not in the tree. For a
 *  MOUNTED, animated run — one a `text()` leaf is drawing and an `fx()`
 *  track is cascading — `Composer::beatsOf` is the answer instead: it
 *  reports the rect the layout actually placed each unit in, which follows
 *  a wrap, a mixed-style run and a path baseline that no single-style
 *  measurement can see. */
std::vector<float> measureRun(std::u8string_view utf8,
                              const sigil::weave::TextStyle& style,
                              sigil::weave::FontContext& fonts);

/** WHERE THE LETTERS SIT: `measureRun`'s advances already summed. Entry i
 *  is glyph i's pen x, measured from the first glyph's pen, and there is
 *  ONE PAST-THE-END ENTRY, so `runPens(...).back()` is the run's whole
 *  laid-out width and `pens[i + 1] - pens[i]` is glyph i's advance. `n`
 *  glyphs give `n + 1` entries, and an empty run gives the single entry 0.
 *
 *  THE ONE RULE TO KNOW, which is `measureRun`'s and is stated here because
 *  this is the form that gets read: A SPACE RIDES THE PREVIOUS ADVANCE.
 *  An inter-word space is a gap the flow leaves between positioned runs
 *  rather than a glyph, so it is no entry of its own; whatever the layout
 *  left between one glyph's pen end and the next one's origin is folded
 *  into the advance of the glyph BEFORE it. That is exactly what makes
 *  these sums reproduce the pen positions the layout used, across a whole
 *  sentence and not only inside one word. Two steps are deliberately not
 *  folded: a '\n' restarts the pen, and a BACKWARDS step between two words
 *  is bidi reordering, which visual-order prefix sums cannot express (a
 *  backwards step INSIDE a word is ordinary kerning and does count). The
 *  pen starts at the first glyph, so leading whitespace is no part of the
 *  run and entry 0 is always 0.
 *
 *  Same shaping path, same single style, same unbounded line as
 *  `measureRun` — and the same division of labour: this is the STATIC
 *  answer for an unmounted run, `Composer::beatsOf` is the answer for a
 *  mounted, cascading one. */
std::vector<float> runPens(std::u8string_view utf8,
                           const sigil::weave::TextStyle& style,
                           sigil::weave::FontContext& fonts);

/** One-shot intrinsic measurement: what size would this element take?
 *  Runs the same reconcile+layout as snapshot() and returns the root's
 *  resolved size without painting. The sizing primitive behind
 *  content-fit chrome (marquees, tooltips, badges): measure the content,
 *  then describe the real tree with the answer. Same sampling rules as
 *  snapshot() — bindings at current values, no transitions. */
SkSize measure(const Element& root, sigil::weave::FontContext& fonts,
               SkSize maxSize = SkSize::MakeEmpty());

}  // namespace sigil::compose
