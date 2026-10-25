#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Document.h>
#include <sigilsketch/kit/Heading.h>

#include <cstddef>
#include <string>
#include <utility>

#include "DocumentInk.h"
#include "sigilweave/advanced/Skia.h"

namespace sigil::sketch::kit {

using compose::Align;
using compose::box;
using compose::Dimension;
using compose::Element;
using compose::Fill;
using compose::Text;
namespace document = compose::document;

namespace {

/** A document line wearing the caller's explicit ink, key and beat. */
Element spoken(const Line& line, Text el, const std::string& key) {
  if (line.ink) detail::documentInk(el, *line.ink);
  if (!key.empty()) el.key(key);
  if (line.opacity) el.opacity(*line.opacity);
  if (line.lift) el.translateY(*line.lift);
  if (line.textFx) el.textFx(*line.textFx);
  return el;
}

/** A CARD LINE'S ROLE DEFAULT: the house register's size and tracking,
 *  and a face only where the register names one of its own. The theme's
 *  registers and faces are the sheet's to state — a page's role rules
 *  state them — so a family the sheet in force sets, on the root or in a
 *  rule, reaches the card's lines, and a rule's weight resolves in it.
 *  The ink is the sheet's palette property, standing where a colour here
 *  would. */
weave::Type cardLine(const Register& line) {
  weave::Type type{.size = line.size, .track = line.track};
  // Unset rather than null where the register names none: a stated null
  // face is the default family outright, which an inherited one would not
  // reach past.
  if (line.face) type.face = line.face;
  return type;
}

}  // namespace

compose::Element titleCard(const TitleCard& card) {
  const Theme& house = houseTheme();
  using compose::var;
  Element column = box().column().alignItems(card.align);
  // The air above a line is that line's own margin rather than the
  // column's gap, because the two distances differ and a line that is
  // absent must leave no space behind it.
  int placed = 0;
  const auto place = [&](Element line, Dimension before) {
    if (placed > 0) line.margin(before, 0, 0, 0);
    column.children({std::move(line)});
    ++placed;
  };
  const auto named = [&](const char* which) {
    return card.key.empty() ? std::string() : card.key + "-" + which;
  };
  const auto say = [&](const Line& line, Text leaf, const char* which,
                       Dimension before) {
    if (line.words.empty()) return;
    place(spoken(line, std::move(leaf), named(which)), before);
  };
  say(card.eyebrow,
      document::eyebrow(card.eyebrow.words)
          .role("eyebrow", cardLine(house.type.eyebrow), var("palette-ash")),
      "eyebrow", 0.0f);
  say(card.title,
      document::h1(card.title.words)
          .role("h1", cardLine(house.type.title), var("palette-ink")),
      "title", var("spacing-subtitle-gap"));
  say(card.subtitle,
      document::lead(card.subtitle.words)
          .role("lead", cardLine(house.type.subtitle), var("palette-ash")),
      "subtitle", var("spacing-subtitle-gap"));
  if (card.ruled)
    place(box()
              .height(1)
              .alignSelf(Align::Stretch)
              .fill(compose::var("palette-rule")),
          var("spacing-rule-gap"));
  if (card.notes.empty()) return column;

  // With notes the card is a ROW: the lines take the width that is left
  // and the notes range at the far edge, the two ranged against each
  // other at their ENDS so the last note sits on the card's last line.
  Element ranged =
      box().column().gap(var("spacing-row-gap")).alignItems(Align::End);
  for (size_t i = 0; i < card.notes.size(); ++i) {
    const Line& note = card.notes[i];
    ranged.children(
        {spoken(note,
                document::caption(note.words)
                    .role("caption", cardLine(house.type.captionNote),
                          var("palette-ash")),
                card.key.empty() ? std::string()
                                 : card.key + "-note" + std::to_string(i))});
  }
  return box()
      .row()
      .alignItems(Align::End)
      .children({std::move(column.flexGrow(1)), std::move(ranged)});
}

compose::Element sectionHeader(const SectionHeader& header) {
  const Theme& house = houseTheme();
  using compose::var;
  Element column = box().column().flexShrink(0);
  Element row =
      box().row().alignItems(Align::Center).gap(var("spacing-label-gap"));
  if (!header.label.empty())
    row.children(
        {document::h2(header.label)
             .role("h2", house.font(house.type.section), var("palette-ink"))});
  if (header.ruled)
    row.children(
        {box().flexGrow(1).height(1).fill(compose::var("palette-rule"))});
  if (!header.label.empty() || header.ruled) column.children({std::move(row)});
  // The note wraps at thirty-six of its own ems, so its measure follows
  // the size the sheet sets it in.
  if (!header.note.empty())
    column.children(
        {document::caption(header.note)
             .role("caption", house.font(house.type.captionNote),
                   var("palette-ash"))
             .maxWidth(Dimension(weave::em(36)))
             .margin(header.label.empty() ? Dimension(0.0f)
                                          : var("spacing-caption-note-gap"),
                     0, 0, 0)});
  return column;
}

compose::Element section(const SectionHeader& header, Element content) {
  Element group = document::section()
                      .gap(compose::var("spacing-section-gap"))
                      .flexShrink(0);
  if (!header.label.empty() || !header.note.empty() || header.ruled)
    group.children({sectionHeader(header)});
  return group.children({std::move(content.flexShrink(0))});
}

}  // namespace sigil::sketch::kit
