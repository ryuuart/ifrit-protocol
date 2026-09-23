/** @file
 * The family, the italic and the first-line indent a node's layers state,
 * folded weakest first.
 */

#include "LonghandFold.h"

#include "ComposeInternal.h"

namespace sigil::compose::detail {

namespace {

/** Whether a field written as @p keyword takes the parent's value. Every
 *  field of a text style and of a block inherits, so `unset` does too. */
bool takesParent(sigil::weave::Keyword keyword) {
  return keyword != sigil::weave::Keyword::Initial;
}

}  // namespace

void LonghandFold::state(const sigil::weave::Type* font,
                         const sigil::weave::ParagraphBlock* block,
                         const CascadeData* said) {
  if (font != nullptr) {
    // A keyword a layer writes about a field stands over a value the same
    // layer holds for it, as the partial's overlay applies them.
    if (const auto keyword =
            font->keywords.find(sigil::weave::TypeField::Face)) {
      m_family = takesParent(*keyword) ? m_parentFamily : nullptr;
      m_familyNamed = false;
      m_faceStated = false;
    } else if (font->face) {
      m_family = nullptr;
      m_familyNamed = false;
      m_faceStated = true;
    }
    if (const auto keyword =
            font->keywords.find(sigil::weave::TypeField::Slant))
      m_italic = takesParent(*keyword) && m_parentItalic;
    else if (font->slant)
      m_italic = false;
  }
  if (block != nullptr) {
    if (const auto keyword = block->keywords.find(
            sigil::weave::ParagraphField::FirstLineIndent)) {
      m_indent.reset();
      m_indentPercent =
          takesParent(*keyword) ? m_parentIndentPercent : std::nullopt;
    } else if (block->firstLineIndent) {
      m_indent.reset();
      m_indentPercent.reset();
    }
  }
  if (said == nullptr) return;
  // The verbs keep a family and a face, an italic and a lean, an indent in
  // pixels and one in another unit, from standing in one layer together,
  // so what the cascade half says is that layer's last word.
  if (said->fontFamily) {
    m_family = &*said->fontFamily;
    m_familyNamed = true;
    m_faceStated = false;
  }
  if (said->italic) m_italic = said->italic;
  if (said->textIndent) {
    m_indent = said->textIndent;
    m_indentPercent.reset();
  }
}

void LonghandFold::fontKeyword(bool fromParent) {
  m_family = fromParent ? m_parentFamily : nullptr;
  m_familyNamed = false;
  m_faceStated = false;
  m_italic = fromParent && m_parentItalic;
}

void LonghandFold::paragraphKeyword(bool fromParent) {
  m_indent.reset();
  m_indentPercent = fromParent ? m_parentIndentPercent : std::nullopt;
}

}  // namespace sigil::compose::detail
