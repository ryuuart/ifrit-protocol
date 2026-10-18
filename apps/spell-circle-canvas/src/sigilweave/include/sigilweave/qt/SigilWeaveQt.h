#pragma once

/** @file
 * @ingroup weave-qt
 *
 * Qt ↔ SigilWeave/Skia bridging. Lives in its own tiny interface target
 * (SigilWeaveQt) so the core SigilWeave library stays Qt-free — link this only
 * from Qt applications.
 *
 * QString and Paragraph both store UTF-16, so text crosses the bridge as a
 * zero-copy view — no transcoding, no allocation.
 */

#include <include/core/SkColor.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/query/Query.h>

#include <QColor>
#include <QFont>
#include <QString>
#include <string_view>

/** QT'S TYPES AS THE ENGINE'S: `QString` as text the engine can shape,
 *  `QFont` as a `TextStyle`, `QColor` as Skia's colour. Text crosses
 *  without copying, both sides storing UTF-16. It is a separate target,
 *  so the engine stays free of Qt and only a Qt application links this. */
namespace sigil::weave::qt {

/** @name Text (zero-copy: QString is UTF-16, and so is Paragraph)
 *  @{
 */

/** Returns a borrowed view of a QString's storage: valid only while `text`
 * is alive and unmodified.
 */
inline std::u16string_view toU16(const QString& text) {
  return {reinterpret_cast<const char16_t*>(text.constData()),
          static_cast<size_t>(text.size())};
}

/** Appends QString text without transcoding its UTF-16 storage. */
inline void appendText(sigil::weave::Paragraph& paragraph, const QString& text,
                       const sigil::weave::TextStyle& style) {
  paragraph.appendText(toU16(text), style);
}

/** Replaces a paragraph range with QString text. */
inline void replaceText(sigil::weave::Paragraph& paragraph, uint32_t start,
                        uint32_t end, const QString& text) {
  // Paragraph::replaceText takes UTF-8; convert once, no extra copies.
  const QByteArray utf8 = text.toUtf8();
  paragraph.replaceText(
      start, end,
      std::u8string_view(reinterpret_cast<const char8_t*>(utf8.constData()),
                         static_cast<size_t>(utf8.size())));
}

/** Finds every occurrence of `needle` without transcoding it. */
inline std::vector<sigil::weave::CharRange> findAllOccurrences(
    const sigil::weave::Paragraph& paragraph, const QString& needle) {
  return sigil::weave::findAllOccurrences(paragraph, toU16(needle));
}

/** @} */

/** @name Color
 *  @{
 */

/** Converts a QColor to its unpremultiplied Skia color value. */
inline SkColor toSkColor(const QColor& color) {
  return SkColorSetARGB(color.alpha(), color.red(), color.green(),
                        color.blue());
}

/** @} */

/** @name Fonts
 *  @{
 */

/** Converts a QFont's weight/slant to an SkFontStyle. QFont::weight() is
 * already on the 1–900 (Thin..Black) scale SkFontStyle uses, so it carries
 * straight through.
 */
inline SkFontStyle toSkFontStyle(const QFont& font) {
  SkFontStyle::Slant slant = SkFontStyle::kUpright_Slant;
  if (font.style() == QFont::StyleItalic)
    slant = SkFontStyle::kItalic_Slant;
  else if (font.style() == QFont::StyleOblique)
    slant = SkFontStyle::kOblique_Slant;
  return SkFontStyle(static_cast<int>(font.weight()),
                     SkFontStyle::kNormal_Width, slant);
}

/** Resolves the QFont's family + style through the given font manager.
 *
 * An empty family yields the platform default. May return null when the
 * manager has no match at all — callers should fall back to their default
 * typeface (e.g. FontContext::defaultTypeface()).
 */
inline sk_sp<SkTypeface> toSkTypeface(SkFontMgr* fontManager,
                                      const QFont& font) {
  if (!fontManager) return nullptr;
  const QByteArray family = font.family().toUtf8();
  return fontManager->matchFamilyStyle(
      family.isEmpty() ? nullptr : family.constData(), toSkFontStyle(font));
}

/** @} */

}  // namespace sigil::weave::qt
