#include <sigilcompose/core/Factories.h>
#include <sigilsketch/kit/Heading.h>

#include <cstddef>
#include <string>
#include <utility>

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dimension;
using compose::Element;
using compose::Fill;
using compose::text;

namespace {

/** One line of a card: its words in @p style, wearing whatever key and
 *  beat the caller gave the line. */
Element spoken(const Line& line, const weave::TextStyle& style,
               const std::string& key) {
  Element el = text(line.words, style);
  if (!key.empty()) el.key(key);
  if (line.opacity) el.opacity(*line.opacity);
  if (line.lift) el.translateY(*line.lift);
  if (line.fx) el.fx(*line.fx);
  return el;
}

}  // namespace

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
  const auto named = [&](const char* which) {
    return card.key.empty() ? std::string() : card.key + "-" + which;
  };
  const auto say = [&](const Line& line, const Register& reg, SkColor4f ink,
                       const char* which, float before) {
    if (line.words.empty()) return;
    place(spoken(line, look.style(reg, line.ink.value_or(Fill::color(ink))),
                 named(which)),
          before);
  };
  say(card.eyebrow, look.type.eyebrow, look.palette.ash, "eyebrow", 0);
  say(card.title, look.type.title, look.palette.ink, "title",
      look.spacing.subtitleGap);
  say(card.subtitle, look.type.subtitle, look.palette.ash, "subtitle",
      look.spacing.subtitleGap);
  if (card.ruled)
    place(box()
              .height(Dimension(1))
              .alignSelf(Align::Stretch)
              .fill(Fill::color(look.palette.rule)),
          look.spacing.contentGap * 0.5f);
  if (card.notes.empty()) return column;

  // With notes the card is a ROW: the lines take the width that is left
  // and the notes range at the far edge, the two ranged against each
  // other at their ENDS so the last note sits on the card's last line.
  Element ranged =
      box().column().gap(look.spacing.rowGap).alignItems(Align::End);
  for (size_t i = 0; i < card.notes.size(); ++i) {
    const Line& note = card.notes[i];
    ranged.child(
        spoken(note,
               look.style(look.type.captionNote,
                          note.ink.value_or(Fill::color(look.palette.ash))),
               card.key.empty() ? std::string()
                                : card.key + "-note" + std::to_string(i)));
  }
  return box()
      .row()
      .alignItems(Align::End)
      .child(std::move(column.grow(1)))
      .child(std::move(ranged));
}

compose::Element sectionHeader(const SectionHeader& header) {
  const Theme& look = theme();
  Element row =
      box().row().alignItems(Align::Center).gap(look.spacing.labelGap);
  if (!header.label.empty())
    row.child(
        text(header.label, look.style(look.type.section, look.palette.ink)));
  // The rule is what GROWS, so the label stays at the left and the note at
  // the right however wide the header is given.
  Element between = box().grow(1).height(Dimension(1)).alignSelf(Align::Center);
  if (header.ruled) between.fill(Fill::color(look.palette.rule));
  row.child(std::move(between));
  if (!header.note.empty())
    row.child(
        text(header.note, look.style(look.type.captionNote, look.palette.ash)));
  return row;
}

}  // namespace sigil::sketch::kit
