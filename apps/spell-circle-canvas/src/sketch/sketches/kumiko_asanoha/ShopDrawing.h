#pragma once

/** @file
 * THE SHOP DRAWING under the room: one cell of the field taken apart, the
 * three jigs its pieces are cut on, the reading of the rule they all come
 * from, and the drawing's title block — and the sheet it is set in.
 *
 * The field's whole argument is a rule about ONE cell, and on the panel it
 * is invisible: seven pieces per jigumi cell, each running from a vertex
 * of one of the two right isosceles triangles the diagonal makes to that
 * triangle's INCENTER. The jig angles, the three-way bisection of every
 * right angle and the piece count all follow from the incircle, so the
 * cell is drawn again here with the two incircles struck, their centres
 * marked, the cutting angles arced at the corner they are cut from, and
 * the seven pieces pushed off their seats. The pieces come from the SAME
 * rule the field is built by and are drawn by the same element, so the
 * drawing cannot drift out of agreement with the panel above it.
 */

#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Board.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilsketch/kit/Document.h>
#include <sigilweave/kit/LineTables.h>
#include <sigilweave/query/Selector.h>

#include <array>
#include <string>

#include "Joinery.h"

namespace kumiko {

namespace document = sigil::compose::document;
namespace shapes = sigil::geometry::shapes;
namespace weave = sigil::weave;
namespace data = sigil::data;
namespace sketch = sigil::sketch;
using namespace sigil::weave::literals;

/** HOW THE DRAWING IS SET. Its colours are tokens on the band's root; the
 *  type is three voices — a drafting sans for every label and figure, a
 *  book serif for the reading, and a mincho for the title the drawing is
 *  filed under — at one root size with every step a multiple of it. */
inline StyleSheet drawingSheet() {
  const std::string drafting = "Avenir Next, Helvetica Neue, sans-serif";
  const std::string book = "Iowan Old Style, Palatino, Georgia, serif";
  const std::string mincho = "Hiragino Mincho ProN, YuMincho, serif";
  return StyleSheet{
      rule(":root")
          .var("ground", hexColor(0x120C07))
          .var("rule", hexColor(0x4A3620, 0.9f))
          .var("ink", hexColor(0xE4D5B2, 0.9f))
          .var("body", hexColor(0xC9B78F, 0.9f))
          .var("muted", hexColor(0xB7A281, 0.85f))
          .var("accent", hexColor(0xC79A57, 0.6f))
          .var("wedge", hexColor(0xC79A57, 0.16f))
          .var("cell", hexColor(0x8E6C3B, 0.85f))
          .var("centre", hexColor(0xF4E3B8, 0.9f))
          .fontFamily(drafting)
          .fontSize(12)
          .letterSpacing(0.02_em)
          .ink(var("body")),
      rule("eyebrow")
          .fontSize(1_rem)
          .fontWeight(600)
          .letterSpacing(0.16_em)
          .ink(var("ink")),
      rule("caption").fontSize(0.95_rem).ink(var("muted")),
      rule("label").fontSize(1.05_rem).textAlign(weave::TextAlignment::kCenter),
      rule("paragraph")
          .fontFamily(book)
          .fontSize(1.1_rem)
          .letterSpacing(0)
          .lineHeight(weave::Leading::absolute(21))
          .textWrap(TextWrap::Pretty)
          .paragraph({.hanging = weave::kit::hanging::latin()}),
      rule(".formula")
          .fontFamily(drafting)
          .fontSize(1_rem)
          .lineHeight(weave::Leading::absolute(16))
          .letterSpacing(0.02_em)
          .ink(var("muted")),
      rule("footer").fontSize(0.9_rem).letterSpacing(0.14_em).ink(var("muted")),
      rule(".title")
          .fontFamily(mincho)
          .fontSize(2.2_rem)
          .letterSpacing(0.3_em)
          .writingMode(weave::WritingMode::kVerticalRL)
          .ink(var("ink")),
  };
}

/** A column of the drawing: its heading over what it announces, at a
 *  width of its own. */
inline Element column(Utf8 heading, float width, Element body) {
  return kit::panel({.eyebrow = std::move(heading), .gap = 18}, std::move(body))
      .width(width)
      .flexShrink(0);
}

/** THE CELL, TAKEN APART, in a @p size square box: its four jigumi and
 *  its seven ha at the panel's proportions, drawn larger. On the drawing's
 *  beat every piece moves away from the cell's centre in proportion to its
 *  distance from it — an exploded view, so every joint opens by the same
 *  share — while the construction the ha were cut to is struck. */
inline Element explodedCell(float size, TimberBank& bank,
                            const choreograph::Output<float>* seconds) {
  constexpr float side = 124, explode = 0.22f;
  const float enlarged = side / kCellWidth;
  const float jigumi = kJigumiWidth * enlarged;
  const vec2 origin{(size - side) * 0.5f, (size - side) * 0.5f};
  const vec2 centre = origin + vec2{side, side} * 0.5f;
  const std::array<vec2, 2> incentres{
      origin + vec2{side * kPastIncircle, side * kIncircle},
      origin + vec2{side * kIncircle, side * kPastIncircle}};
  const auto beat = [&](float delay) {
    return motion::bind(seconds)
        .window(kDrawingAt + delay, kDrawingAt + delay + kDrawingFor)
        .map(choreograph::easeOutCubic);
  };
  const auto point = [](vec2 at) { return SkPoint{at.x, at.y}; };
  const auto exploded = [&](const Piece& piece) {
    const vec2 away = ((piece.from + piece.to) * 0.5f - centre) * explode;
    return pieceElement(piece, bank, nullptr)
        .translateX(beat(0).target(0, away.x))
        .translateY(beat(0).target(0, away.y));
  };

  // The jigumi the cell stands in, each running a little past the
  // crossing it laps at, the horizontals over the verticals.
  const float reach = jigumi;
  uint32_t seed = 101;
  std::vector<Piece> frame;
  for (const float x : {origin.x, origin.x + side})
    frame.push_back(cut({x, origin.y - reach}, {x, origin.y + side + reach},
                        jigumi, Role::VerticalJigumi, kHinoki, seed++));
  for (const float y : {origin.y, origin.y + side})
    frame.push_back(cut({origin.x - reach, y}, {origin.x + side + reach, y},
                        jigumi, Role::HorizontalJigumi, kHinoki, seed++));

  Element art = box().width(size).height(size);
  // The two incircles, struck on the beat, and the incenters they are
  // struck about: the construction the whole pattern is derived from and
  // the only circles anywhere in a kumiko panel.
  art.children({each(incentres, [&](vec2 incentre) {
    return kit::disc(point(incentre), side * kIncircle)
        .shape(shapes::circle())
        .stroke(spans::upTo(beat(0.1f)),
                PathFormat{.width = 0.9f,
                           .strokeFill = Fill::var("accent"),
                           .dashIntervals = {3.0f, 3.0f}});
  })});
  for (const Piece& piece :
       cellLeaves(origin, side, side, kLeafWidth * enlarged, false,
                  jigumi * 0.5f + enlarged, kLeafWidth * enlarged * 0.55f,
                  seed))
    art.children({exploded(piece)});
  for (const Piece& piece : frame) art.children({exploded(piece)});
  art.children({each(incentres, [&](vec2 incentre) {
    return kit::dot(point(incentre), 2.2f, Fill::var("centre"))
        .opacity(beat(0.3f));
  })});
  // The two jig angles a locking piece is cut at, arced at the corner it
  // leaves: 22.5° off the top edge and 22.5° off the side.
  for (const float start : {0.0f, 67.5f})
    art.children({kit::disc(point(origin), 56)
                      .shape(shapes::arc(start, 22.5f))
                      .stroke(spans::upTo(beat(0.2f)),
                              PathFormat{.width = 1.0f,
                                         .strokeFill = Fill::var("accent")})});
  return art;
}

/** THE THREE JIGS, each showing its own cutting angle against the same
 *  vertical fence, with the angle named under it. */
inline Element jigs(const data::Json& angles, float height) {
  return box()
      .row()
      .height(height)
      .justifyContent(Justify::SpaceBetween)
      .alignItems(Align::Center)
      .children({each(angles.items(), [](const data::Json& angle) {
        const float degrees = (float)angle.number();
        return box().column().gap(16).alignItems(Align::Center).children(
            {box()
                 .width(88)
                 .height(88)
                 .shape(shapes::sector(-90.0f, degrees, 0.0f))
                 .fill(Fill::var("wedge"))
                 .stroke(stroke(0.9f, Fill::var("accent"),
                                PathFormat::Align::Inner)),
             document::label(kit::formatted("%g°", degrees))});
      })});
}

/** The title the drawing is filed under, set down the page, each word
 *  with its reading beside it. */
inline Element title(const data::Json& words) {
  std::u8string base;
  for (const data::Json& word : words.items())
    base += Utf8(word["base"].text()).bytes();
  Text column =
      text(base).styleClass("title").height(230).width(90).flexShrink(0);
  for (const data::Json& word : words.items())
    column.textAnnotation(kit::ruby(
        weave::selectors::text(Utf8(word["base"].text()).bytes()),
        weave::Unit::Selection, {Utf8(word["reading"].text()).bytes()},
        weave::Type{.size = 0.34_em}));
  return column;
}

/** THE WHOLE DRAWING: the band's four columns on the frame's own edges,
 *  every heading on one line and both figures' notes on another, and the
 *  title block's rule and caption along its foot. */
inline Element shopDrawing(const sketch::kit::Document& doc, TimberBank& bank,
                           const choreograph::Output<float>* seconds) {
  constexpr float figure = 160;
  const float left = kFrameOuter.left(), width = kFrameOuter.width();
  return kit::at(0, kRoom, kWidth, kBandHeight)
      .fill(Fill::var("ground"))
      .applyStyleSheet(drawingSheet())
      .children({
          // the drawing's own ground: a hairline ruled off the room above it
          kit::at(0, 0, kWidth, 1).fill(Fill::var("rule")),
          box()
              .absolute()
              .left(left)
              .top(36)
              .width(width)
              .row()
              .gap(40)
              .children({
                  column(doc["cell"]["heading"].text(), 230,
                         document::figure(explodedCell(figure, bank, seconds),
                                          doc["cell"]["note"].text())),
                  column(doc["jigs"]["heading"].text(), 270,
                         document::figure(jigs(doc["jigs"]["angles"], figure),
                                          doc["jigs"]["note"].text())),
                  kit::panel(
                      {.eyebrow = doc["reading"]["title"].text(), .gap = 18},
                      box().column().gap(18).children(
                          {document::paragraph(
                               doc.phrase(doc["reading"]["prose"])),
                           box().column().gap(4).children({each(
                               doc.run(doc["reading"]["formulas"]),
                               [](const sketch::kit::Document::Line& line) {
                                 return document::paragraph(line.words)
                                     .styleClass("formula");
                               })})}))
                      .flexGrow()
                      .flexShrink(1),
                  title(doc["title"]),
              }),
          box()
              .absolute()
              .left(left)
              .bottom(24)
              .width(width)
              .column()
              .gap(10)
              .children({
                  box().height(1).fill(Fill::var("rule")),
                  box().row().justifyContent(Justify::SpaceBetween).children(
                      {document::footer(doc["sheet"].text()),
                       document::footer(doc["caption"].text())}),
              }),
      });
}

}  // namespace kumiko
