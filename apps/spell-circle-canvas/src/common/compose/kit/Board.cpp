#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Board.h>

#include <utility>

namespace sigil::compose::kit {

Element board(const Board& plate) {
  // Pinned at its parent's origin, and stretched on whichever axis was
  // left open: a size on an axis is the board's own extent, and no size
  // is the parent's far edge, which is the canvas for a root.
  Element root = stack().absolute().left(Dimension(0)).top(Dimension(0));
  if (plate.size.width() > 0)
    root.width(Dimension(plate.size.width()));
  else
    root.right(Dimension(0));
  if (plate.size.height() > 0)
    root.height(Dimension(plate.size.height()));
  else
    root.bottom(Dimension(0));
  if (!plate.ground.none()) root.fill(plate.ground);
  return root;
}

// ---------------------------------------------------------------------------
// The titled region

namespace {

/** One line of the head with @p note standing at the far edge of it: the
 *  space between is what grows, so the note is on the edge however long
 *  the words are. */
Element ranged(Element line, Element note) {
  return box()
      .row()
      .alignItems(Align::Center)
      .children({std::move(line), box().grow(1), std::move(note)});
}

}  // namespace

Element panel(const Panel& region, Element content) {
  const bool hasEyebrow = !region.eyebrow.empty();
  const bool hasTitle = !region.title.empty();
  const bool hasNote = !region.note.empty();
  const auto note = [&] {
    return region.noteLine ? region.noteLine(region.note, region)
                           : captionNote(region.note);
  };
  // The head is the sheet's: its two lines, the gap between them and the
  // rule that bisects the distance to the content are that component's
  // arithmetic, and nothing of it is restated here.
  Sheet head{.title = region.eyebrow, .subtitle = region.title};
  head.marginX = 0;
  head.marginTop = 0;
  head.marginBottom = 0;
  head.subtitleGap = region.titleGap;
  head.contentGap = region.gap;
  head.rule = region.rule;
  head.ruleWidth = region.ruleWidth;
  // A head with no words of its own still carries its note, which is the
  // whole head then.
  if (hasNote && !hasTitle && !hasEyebrow) head.subtitle = region.note;
  head.titleLine = [&](const Utf8& words) {
    Element line = region.eyebrowLine ? region.eyebrowLine(words, region)
                                      : panelEyebrow(words);
    if (hasNote && !hasTitle) return ranged(std::move(line), note());
    return line;
  };
  head.subtitleLine = [&](const Utf8& words) {
    if (!hasTitle) return ranged(box(), note());
    Element line =
        region.titleLine ? region.titleLine(words, region) : sheetTitle(words);
    if (hasNote) return ranged(std::move(line), note());
    return line;
  };
  Element column = sheet(head, std::move(content));
  return region.body ? well(*region.body, std::move(column))
                     : std::move(column);
}

}  // namespace sigil::compose::kit
