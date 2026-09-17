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
    column.children({std::move(line)});
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
              .height(1)
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
    ranged.children(
        {spoken(note,
                look.style(look.type.captionNote,
                           note.ink.value_or(Fill::color(look.palette.ash))),
                card.key.empty() ? std::string()
                                 : card.key + "-note" + std::to_string(i))});
  }
  return box()
      .row()
      .alignItems(Align::End)
      .children({std::move(column.grow(1)), std::move(ranged)});
}

compose::Element sectionHeader(const SectionHeader& header) {
  const Theme& look = theme();
  Element column = box().column().shrink(0);
  Element row =
      box().row().alignItems(Align::Center).gap(look.spacing.labelGap);
  if (!header.label.empty())
    row.children(
        {text(header.label, look.style(look.type.section, look.palette.ink))});
  if (header.ruled)
    row.children(
        {box().grow(1).height(1).fill(Fill::color(look.palette.rule))});
  if (!header.label.empty() || header.ruled) column.children({std::move(row)});
  if (!header.note.empty())
    column.children(
        {text(header.note, look.style(look.type.captionNote, look.palette.ash))
             .maxWidth(look.type.captionNote.size * 36)
             .margin(0, header.label.empty() ? 0 : look.spacing.captionNoteGap,
                     0, 0)});
  return column;
}

}  // namespace sigil::sketch::kit
