#pragma once

/** @file
 * @ingroup weave-layout
 *
 * `Story` — a text and the block styles it is set under, filled into as
 * many frames as it is given. It holds no layout, no cursor and no
 * frame: every frame of a chain lays the same story out and resumes
 * where the one before it stopped.
 */

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "sigilweave/layout/LayoutOptions.h"
#include "sigilweave/paragraph/RichText.h"
#include "sigilweave/style/TextStyle.h"

namespace sigil::weave {

/** A TEXT AND THE BLOCK STYLES IT IS SET UNDER, filled into as many
 *  frames as it is given, and a VALUE for the reason `RichText` is. THE
 *  BLOCKS ARE NUMBERED FROM THE STORY'S START, so the third block is set
 *  the same way whichever frame it lands in.
 *  @trap Pitch, writing mode and block styles are the story's and a frame
 *  cannot override them: a frame that wants a different pitch is a
 *  different story. */
class Story {
 public:
  Story() = default;
  /** A story over mixed content. */
  explicit Story(RichText content) : m_content(std::move(content)) {}
  /** A story over one styled string. */
  Story(std::u8string utf8, TextStyle style)
      : m_content(RichText(std::move(style))) {
    m_content.add(utf8);
  }
  /** The same story from UTF-8 held as `char`. */
  Story(std::string_view utf8, TextStyle style)
      : m_content(RichText(std::move(style))) {
    m_content.add(utf8);
  }

  /** How each BLOCK of the story is set, in block order — stated once, and
   *  the same for every frame the story runs through. */
  Story& paragraphs(std::vector<ParagraphStyle> blocks) {
    m_blocks = std::move(blocks);
    return *this;
  }

  [[nodiscard]] const RichText& content() const { return m_content; }
  [[nodiscard]] std::span<const ParagraphStyle> blocks() const {
    return m_blocks;
  }
  [[nodiscard]] bool empty() const { return m_content.empty(); }

  bool operator==(const Story& other) const {
    return m_content == other.m_content && m_blocks == other.m_blocks;
  }

 private:
  RichText m_content;
  std::vector<ParagraphStyle> m_blocks;
};

}  // namespace sigil::weave
