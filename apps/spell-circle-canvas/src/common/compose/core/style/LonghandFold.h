#pragma once

/** @file
 * The three statements the cascade resolves itself rather than by laying
 * one partial over another: a family by NAME, an ITALIC that is a face
 * rather than a lean, and a first-line indent in a unit the node's own
 * font or its passage's measure decides.
 */

#include <sigilweave/layout/ParagraphBlock.h>
#include <sigilweave/style/Type.h>

#include <optional>
#include <string>

#include "sigilcompose/core/Layout.h"

namespace sigil::compose::detail {

struct CascadeData;

/** WHAT A NODE'S LAYERS STATE ABOUT ITS FAMILY, ITS ITALIC AND ITS INDENT,
 *  folded weakest first so the last statement stands — a role's defaults,
 *  each matched rule, then the node's own verbs, each through `state`,
 *  and a whole-property keyword through `fontKeyword` or
 *  `paragraphKeyword` where it falls.
 *
 *  A field written as a keyword in one layer is that layer's statement
 *  about it, over a value the same layer holds, as the partial's own
 *  overlay reads it: `inherit` stands in what the parent had and
 *  `initial` stands in nothing.
 *  @trap The family pointers borrow from the parent's instance and from
 *  the descriptions folded, so the fold lives no longer than one node's
 *  resolve. */
class LonghandFold {
 public:
  /** A fold that has stated nothing: the family, the italic and the
   *  indent percentage in force above. */
  LonghandFold(const std::string* parentFamily, bool parentItalic,
               std::optional<float> parentIndentPercent)
      : m_parentFamily(parentFamily),
        m_parentItalic(parentItalic),
        m_parentIndentPercent(parentIndentPercent),
        m_family(parentFamily),
        m_indentPercent(parentIndentPercent) {}

  /** One layer: its font partial, its paragraph partial and its cascade
   *  half, which holds a family by name, an italic and an indent in
   *  another unit. Any may be null. */
  void state(const sigil::weave::Type* font,
             const sigil::weave::ParagraphBlock* block,
             const CascadeData* said);
  /** The whole font written as a keyword: the family and the italic are
   *  the parent's under @p fromParent, none otherwise. */
  void fontKeyword(bool fromParent);
  /** The whole paragraph setting written as a keyword: the indent is the
   *  parent's under @p fromParent, none otherwise. */
  void paragraphKeyword(bool fromParent);

  /** The family a layer of this node named, or null where none did. */
  [[nodiscard]] const std::string* familyNamed() const {
    return m_familyNamed ? m_family : nullptr;
  }
  /** The family in force by name — named here or above — or null where
   *  the face in force was stated as a face, or none was named. */
  [[nodiscard]] const std::string* familyInForce() const { return m_family; }
  /** Whether a layer of this node stated a face after any family. */
  [[nodiscard]] bool faceStated() const { return m_faceStated; }
  /** Whether the style in force is italic. */
  [[nodiscard]] bool italic() const {
    return m_italic.value_or(m_parentItalic);
  }
  /** The indent the strongest statement gave in a unit the cascade
   *  resolves, or none where it gave pixels, a keyword or nothing. */
  [[nodiscard]] const std::optional<Dimension>& indent() const {
    return m_indent;
  }
  /** The indent in force as a percentage of each passage's measure. */
  [[nodiscard]] std::optional<float>& indentPercent() {
    return m_indentPercent;
  }

 private:
  const std::string* m_parentFamily;
  bool m_parentItalic;
  std::optional<float> m_parentIndentPercent;
  const std::string* m_family;
  bool m_familyNamed = false;
  bool m_faceStated = false;
  std::optional<bool> m_italic;
  std::optional<Dimension> m_indent;
  std::optional<float> m_indentPercent;
};

}  // namespace sigil::compose::detail
