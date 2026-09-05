#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Heading.h>

#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dim;
using compose::Element;
using compose::Fill;
using compose::text;

compose::Element titleCard(const TitleCard& card) {
  const Theme& look = theme();
  Element column = box().column().alignItems(card.align);
  // The air above a line is that line's own margin rather than the
  // column's gap, because the two distances differ and a line that is
  // absent must leave no space behind it.
  int placed = 0;
  const auto place = [&](Element line, float before) {
    if (placed > 0) line.margin(0, before, 0, 0);
    column.child(std::move(line));
    ++placed;
  };
  if (!card.eyebrow.empty())
    place(text(card.eyebrow, look.style(look.type.eyebrow, look.palette.ash)),
          0);
  if (!card.title.empty())
    place(text(card.title, look.style(look.type.title, look.palette.ink)),
          look.spacing.subtitleGap);
  if (!card.subtitle.empty())
    place(
        text(card.subtitle, look.style(look.type.subtitle, look.palette.ash)),
        look.spacing.subtitleGap);
  if (card.ruled)
    place(box()
              .height(Dim(1))
              .alignSelf(Align::Stretch)
              .fill(Fill::color(look.palette.rule)),
          look.spacing.contentGap * 0.5f);
  return column;
}

compose::Element sectionHeader(const SectionHeader& header) {
  const Theme& look = theme();
  Element row = box().row().alignItems(Align::Center).gap(
      look.spacing.labelGap);
  if (!header.label.empty())
    row.child(
        text(header.label, look.style(look.type.section, look.palette.ink)));
  // The rule is what GROWS, so the label stays at the left and the note at
  // the right however wide the header is given.
  Element between = box().grow(1).height(Dim(1)).alignSelf(Align::Center);
  if (header.ruled) between.fill(Fill::color(look.palette.rule));
  row.child(std::move(between));
  if (!header.note.empty())
    row.child(text(header.note,
                   look.style(look.type.captionNote, look.palette.ash)));
  return row;
}

}  // namespace sigil::sketch::kit
