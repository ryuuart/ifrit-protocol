/** @file
 * The one instrument primitive that cannot be written against the public
 * seam: building a copy of a text element WITHOUT its tracks means reading
 * the description, and a description is internal. The instrument itself
 * (`kit::restGhost`) is the kit's; this is what it reads.
 */

#include <sigilcompose/core/Element.h>

#include <string>
#include <utility>

#include "ComposeInternal.h"

namespace sigil::compose::detail {

namespace nodes = ::sigil::compose::detail;

namespace {

void warnGhostOfNonText() {
  static bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "[compose] restGhost() on an element that is not text() hands "
      "the element back unchanged: a rest pose is what an fx() track's "
      "per-glyph deviation is measured against, and every other element "
      "already draws where its layout put it.\n");
}

}  // namespace

Element textAtRest(Element moving, SkColor4f colour) {
  const std::shared_ptr<nodes::ElementNode>& node = moving.node();
  if (node->kind != nodes::Kind::Text || !node->textData) {
    warnGhostOfNonText();
    return moving;
  }
  // A COPY OF THE DESCRIPTION, not a re-description: the ghost has to be
  // the same paragraph, laid out the same way, at the same width, or the
  // two copies disagree about where a letter belongs and the comparison is
  // worthless. Everything that could shift a glyph is therefore carried
  // over untouched and only two things are changed.
  auto ghostNode = std::make_shared<nodes::ElementNode>(*node);
  nodes::TextData& text = ghostNode->textData.ensure();
  // 1. NO TRACKS. This is the whole point — a ghost carrying the effect
  //    would deform with it and measure nothing.
  text.tracks.clear();
  // …AND NO CHILDREN, which is the same rule read twice. A text node's
  // children are its marks and its slot mounts, and both are already on
  // screen once: copying them would draw each of them twice under a
  // duplicate key, which the composer's key index cannot answer for. The
  // slot RUNS stay — they are content, and they reserve the same space in
  // the ghost's paragraph, which is what keeps the two copies' letters in
  // the same places.
  ghostNode->children.clear();
  text.marks.clear();
  // 2. ONE INK. The style's own foreground where the content carries a
  //    style to edit, and a whole-text repaint where it does not: the
  //    shared_ptr<Paragraph> overload's paragraph belongs to the caller and
  //    must not be written through. spanPaint is paint-only and re-shapes
  //    nothing, so the ghost's glyphs stay the moving copy's glyphs either
  //    way.
  sigil::weave::PaintStyle paint;
  paint.foreground.setColor4f(colour, nullptr);
  if (text.paragraphOverride) {
    nodes::SpanRestyle repaint;
    repaint.style.paint = std::move(paint);
    repaint.paintOnly = true;
    text.spanRestyles.push_back(std::move(repaint));
  } else {
    text.style.paint = paint;
    text.rich = RichText();
    // A rich() value's runs each carry their own resolved style, so the
    // base alone would repaint only the undressed ones. Rebuild it with
    // every run set in the ghost's ink and the run boundaries — which are
    // shaping boundaries — kept exactly where they were.
    if (!node->textData->rich.empty()) {
      RichText ghostRich(text.style);
      for (const RichText::Run& run : node->textData->rich.runs()) {
        if (!run.slotKey.empty()) {
          ghostRich.slot(run.slotKey, run.slotSize, run.slotBaselineDrop);
          continue;
        }
        sigil::weave::TextStyle runStyle = run.style;
        runStyle.paint = paint;
        ghostRich.add(run.utf8, runStyle);
      }
      text.rich = std::move(ghostRich);
    }
    text.hasTextStroke = false;
    text.metricFill.reset();
  }
  if (!ghostNode->key.empty()) ghostNode->key += "-rest";
  // Pinned at the origin so the two copies share one origin, and absolute
  // so the MOVING copy is what sizes the box around them.
  Element ghost{std::move(ghostNode)};
  ghost.left(0.0f).top(0.0f);
  return box().child(std::move(ghost)).child(std::move(moving));
}

}  // namespace sigil::compose::detail
