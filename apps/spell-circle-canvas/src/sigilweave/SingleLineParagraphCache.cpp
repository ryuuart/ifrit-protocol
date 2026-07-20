#include "sigilweave/SingleLineParagraphCache.h"

#include <absl/container/node_hash_map.h>
#include <absl/strings/str_cat.h>

#include <unicode/ustring.h>

#include <string>

namespace sigil::weave {

/// Private storage: a node-based map so returned Paragraph& stay valid
/// while other entries are inserted (the header's documented contract).
struct SingleLineParagraphCache::Impl {
  explicit Impl(size_t maximumEntryCount) : maximumEntries(maximumEntryCount) {}
  absl::node_hash_map<std::string, Paragraph> paragraphs;
  size_t maximumEntries;
};

SingleLineParagraphCache::SingleLineParagraphCache(size_t maximumEntries)
    : m_impl(std::make_unique<Impl>(maximumEntries)) {}
SingleLineParagraphCache::~SingleLineParagraphCache() = default;
SingleLineParagraphCache::SingleLineParagraphCache(
    SingleLineParagraphCache &&) noexcept = default;
SingleLineParagraphCache &SingleLineParagraphCache::operator=(
    SingleLineParagraphCache &&) noexcept = default;

void SingleLineParagraphCache::clear() { m_impl->paragraphs.clear(); }

namespace {

void appendUtf8Key(std::string &key, std::u8string_view utf8) {
  key.append(reinterpret_cast<const char *>(utf8.data()), utf8.size());
}

void appendUtf8Key(std::string &key, std::u16string_view utf16) {
  // Key by UTF-8 so both entry points address the same cache slot.
  const size_t keyStart = key.size();
  key.resize(keyStart + utf16.size() * 3);
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strToUTF8(key.data() + keyStart,
              static_cast<int32_t>(key.size() - keyStart), &written,
              reinterpret_cast<const UChar *>(utf16.data()),
              static_cast<int32_t>(utf16.size()), &status);
  if (U_SUCCESS(status))
    key.resize(keyStart + static_cast<size_t>(written));
  else
    key.resize(keyStart);
}

} // namespace

template <detail::CacheableTextView View>
Paragraph &SingleLineParagraphCache::paragraphForImpl(
    View text, const sk_sp<SkTypeface> &typeface, float fontSize) {
  std::string key;
  appendUtf8Key(key, text);
  absl::StrAppend(&key, "\x1f", typeface ? typeface->uniqueID() : 0, "\x1f",
                  static_cast<int>(fontSize * 16.0f));
  auto &paragraphs = m_impl->paragraphs;
  auto paragraph = paragraphs.find(key);
  if (paragraph == paragraphs.end()) {
    if (paragraphs.size() >= m_impl->maximumEntries)
      paragraphs.clear(); // scenes cycle labels; don't grow without bound
    TextStyle style;
    style.shaping.typeface = typeface;
    style.shaping.fontSize = fontSize;
    Paragraph newParagraph;
    newParagraph.appendText(text, style);
    paragraph =
        paragraphs.emplace(std::move(key), std::move(newParagraph)).first;
  }
  return paragraph->second;
}

Paragraph &
SingleLineParagraphCache::paragraphFor(std::u8string_view utf8,
                                       const sk_sp<SkTypeface> &typeface,
                                       float fontSize) {
  return paragraphForImpl(utf8, typeface, fontSize);
}

Paragraph &
SingleLineParagraphCache::paragraphFor(std::u16string_view utf16,
                                       const sk_sp<SkTypeface> &typeface,
                                       float fontSize) {
  return paragraphForImpl(utf16, typeface, fontSize);
}

} // namespace sigil::weave
