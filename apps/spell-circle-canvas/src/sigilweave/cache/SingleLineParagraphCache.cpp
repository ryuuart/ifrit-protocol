/** @file
 * Single-style paragraphs memoized by text, typeface and quantized size in
 * a node-based map, so a returned reference stays valid while other
 * entries are inserted.
 */

#include "sigilweave/cache/SingleLineParagraphCache.h"

#include <unicode/ustring.h>

#include <boost/unordered/unordered_node_map.hpp>
#include <charconv>
#include <cstdint>
#include <string>
#include <utility>

namespace sigil::weave {

/// Private storage: a node-based map so returned Paragraph& stay valid
/// while other entries are inserted (the header's documented contract).
/// Each entry carries the tick it was last asked for, which is what makes
/// room at capacity: one entry goes, and it is the one nothing has asked
/// for in the longest time.
struct SingleLineParagraphCache::Impl {
  explicit Impl(size_t maximumEntryCount) : maximumEntries(maximumEntryCount) {}

  struct Entry {
    Paragraph paragraph;
    uint64_t lastUsed = 0;
  };

  /// Drops the least recently used entry when the cache is full.
  void makeRoom() {
    if (paragraphs.size() < maximumEntries) return;
    auto oldest = paragraphs.begin();
    for (auto entry = paragraphs.begin(); entry != paragraphs.end(); ++entry)
      if (entry->second.lastUsed < oldest->second.lastUsed) oldest = entry;
    if (oldest != paragraphs.end()) paragraphs.erase(oldest);
  }

  boost::unordered_node_map<std::string, Entry> paragraphs;
  size_t maximumEntries;
  uint64_t tick = 0;
};

SingleLineParagraphCache::SingleLineParagraphCache(size_t maximumEntries)
    : m_impl(std::make_unique<Impl>(maximumEntries)) {}
SingleLineParagraphCache::~SingleLineParagraphCache() = default;
SingleLineParagraphCache::SingleLineParagraphCache(
    SingleLineParagraphCache&&) noexcept = default;
SingleLineParagraphCache& SingleLineParagraphCache::operator=(
    SingleLineParagraphCache&&) noexcept = default;

void SingleLineParagraphCache::clear() { m_impl->paragraphs.clear(); }

namespace {

void appendUtf8Key(std::string& key, std::u8string_view utf8) {
  key.append(reinterpret_cast<const char*>(utf8.data()), utf8.size());
}

void appendUtf8Key(std::string& key, std::u16string_view utf16) {
  // Key by UTF-8 so both entry points address the same cache slot.
  const size_t keyStart = key.size();
  key.resize(keyStart + utf16.size() * 3);
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strToUTF8(key.data() + keyStart,
              static_cast<int32_t>(key.size() - keyStart), &written,
              reinterpret_cast<const UChar*>(utf16.data()),
              static_cast<int32_t>(utf16.size()), &status);
  if (U_SUCCESS(status))
    key.resize(keyStart + static_cast<size_t>(written));
  else
    key.resize(keyStart);
}

template <typename Integer>
void appendInteger(std::string& key, Integer value) {
  char digits[16];
  const auto result =
      std::to_chars(std::begin(digits), std::end(digits), value);
  key.append(digits, result.ptr);
}

}  // namespace

template <detail::CacheableTextView View>
Paragraph& SingleLineParagraphCache::paragraphForImpl(
    View text, const sk_sp<SkTypeface>& typeface, float fontSize) {
  std::string key;
  appendUtf8Key(key, text);
  key.push_back('\x1f');
  appendInteger(key, typeface ? typeface->uniqueID() : 0u);
  key.push_back('\x1f');
  appendInteger(key, static_cast<int>(fontSize * 16.0f));
  auto& paragraphs = m_impl->paragraphs;
  auto paragraph = paragraphs.find(key);
  if (paragraph == paragraphs.end()) {
    m_impl->makeRoom();  // scenes cycle labels; don't grow without bound
    TextStyle style;
    style.shaping.typeface = typeface;
    style.shaping.fontSize = fontSize;
    Impl::Entry entry;
    entry.paragraph.appendText(text, style);
    paragraph = paragraphs.emplace(std::move(key), std::move(entry)).first;
  }
  paragraph->second.lastUsed = ++m_impl->tick;
  return paragraph->second.paragraph;
}

Paragraph& SingleLineParagraphCache::paragraphFor(
    std::u8string_view utf8, const sk_sp<SkTypeface>& typeface,
    float fontSize) {
  return paragraphForImpl(utf8, typeface, fontSize);
}

Paragraph& SingleLineParagraphCache::paragraphFor(
    std::u16string_view utf16, const sk_sp<SkTypeface>& typeface,
    float fontSize) {
  return paragraphForImpl(utf16, typeface, fontSize);
}

}  // namespace sigil::weave
