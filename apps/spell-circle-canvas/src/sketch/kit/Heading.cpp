#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilsketch/kit/Heading.h>

#include <cstddef>
#include <string>
#include <utility>

#include "DocumentInk.h"

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dimension;
using compose::Element;
using compose::Fill;
namespace document = compose::document;

namespace {

/** A document line wearing the caller's explicit ink, key and beat. */
Element spoken(const Line& line, Element el, const std::string& key) {
  if (line.ink) detail::documentInk(el, *line.ink);
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
  const auto say = [&](const Line& line, Element leaf, const char* which,
                       float before) {
    if (line.words.empty()) return;
    place(spoken(line, std::move(leaf), named(which)), before);
  };
  say(card.eyebrow,
      document::eyebrow(card.eyebrow.words)
          .role(weave::rule("eyebrow").font(
              look.font(look.type.eyebrow, look.palette.ash))),
      "eyebrow", 0);
  say(card.title,
      document::h1(card.title.words)
          .role(weave::rule("h1").font(
              look.font(look.type.title, look.palette.ink))),
      "title", look.spacing.subtitleGap);
  say(card.subtitle,
      document::lead(card.subtitle.words)
          .role(weave::rule("lead").font(
              look.font(look.type.subtitle, look.palette.ash))),
      "subtitle", look.spacing.subtitleGap);
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
                document::caption(note.words)
                    .role(weave::rule("caption").font(
                        look.font(look.type.captionNote, look.palette.ash))),
                card.key.empty() ? std::string()
                                 : card.key + "-note" + std::to_string(i))});
  }
  return box()
      .row()
      .alignItems(Align::End)
      .children({std::move(column.flexGrow(1)), std::move(ranged)});
}

compose::Element sectionHeader(const SectionHeader& header) {
  const Theme& look = theme();
  Element column = box().column().flexShrink(0);
  Element row =
      box().row().alignItems(Align::Center).gap(look.spacing.labelGap);
  if (!header.label.empty())
    row.children({document::h2(header.label)
                      .role(weave::rule("h2").font(
                          look.font(look.type.section, look.palette.ink)))});
  if (header.ruled)
    row.children(
        {box().flexGrow(1).height(1).fill(Fill::color(look.palette.rule))});
  if (!header.label.empty() || header.ruled) column.children({std::move(row)});
  if (!header.note.empty())
    column.children(
        {document::caption(header.note)
             .role(weave::rule("caption").font(
                 look.font(look.type.captionNote, look.palette.ash)))
             .maxWidth(look.type.captionNote.size * 36)
             .margin(0, header.label.empty() ? 0 : look.spacing.captionNoteGap,
                     0, 0)});
  return column;
}

}  // namespace sigil::sketch::kit
