#include "textflow/SingleLineParagraphCache.h"

#include <absl/strings/str_cat.h>

#include <unicode/ustring.h>

namespace textflow {

namespace {

void appendUtf8Key(std::string &key, std::string_view utf8) { key += utf8; }

void appendUtf8Key(std::string &key, std::u16string_view utf16) {
  // Key by UTF-8 so both entry points address the same cache slot.
  std::string utf8(utf16.size() * 3, '\0');
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strToUTF8(utf8.data(), static_cast<int32_t>(utf8.size()), &written,
              reinterpret_cast<const UChar *>(utf16.data()),
              static_cast<int32_t>(utf16.size()), &status);
  if (U_SUCCESS(status))
    key.append(utf8.data(), static_cast<size_t>(written));
}

} // namespace

template <detail::CacheableTextView View>
Paragraph &SingleLineParagraphCache::paragraphForImpl(
    View text, const sk_sp<SkTypeface> &typeface, float fontSize) {
  std::string key;
  appendUtf8Key(key, text);
  absl::StrAppend(&key, "\x1f", typeface ? typeface->uniqueID() : 0, "\x1f",
                  static_cast<int>(fontSize * 16.0f));
  auto paragraph = m_paragraphs.find(key);
  if (paragraph == m_paragraphs.end()) {
    if (m_paragraphs.size() >= m_maximumEntries)
      m_paragraphs.clear(); // scenes cycle labels; don't grow without bound
    TextStyle style;
    style.shaping.typeface = typeface;
    style.shaping.fontSize = fontSize;
    Paragraph newParagraph;
    newParagraph.appendText(text, style);
    paragraph =
        m_paragraphs.emplace(std::move(key), std::move(newParagraph)).first;
  }
  return paragraph->second;
}

Paragraph &SingleLineParagraphCache::paragraphFor(
    std::string_view utf8, const sk_sp<SkTypeface> &typeface, float fontSize) {
  return paragraphForImpl(utf8, typeface, fontSize);
}

Paragraph &
SingleLineParagraphCache::paragraphFor(std::u16string_view utf16,
                                       const sk_sp<SkTypeface> &typeface,
                                       float fontSize) {
  return paragraphForImpl(utf16, typeface, fontSize);
}

} // namespace textflow
