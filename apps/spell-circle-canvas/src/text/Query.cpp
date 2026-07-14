#include "textflow/Query.h"

#include <unicode/uregex.h>
#include <unicode/ustring.h>

#include <algorithm>

namespace textflow {

namespace {

std::u16string toUtf16(std::u8string_view utf8) {
  if (utf8.empty())
    return {};
  std::u16string utf16;
  utf16.resize(utf8.size());
  UErrorCode status = U_ZERO_ERROR;
  int32_t codeUnitsWritten = 0;
  u_strFromUTF8(reinterpret_cast<UChar *>(utf16.data()),
                static_cast<int32_t>(utf16.size()), &codeUnitsWritten,
                reinterpret_cast<const char *>(utf8.data()),
                static_cast<int32_t>(utf8.size()), &status);
  if (U_FAILURE(status))
    return {};
  utf16.resize(static_cast<size_t>(codeUnitsWritten));
  return utf16;
}

// Marker-adjustment gravity for one recorded edit: positions before the
// replaced region stay, positions after shift, positions inside snap to the
// region's start (range starts) or past the insertion (range ends) so an
// overlapped range absorbs the replacement.
uint32_t mapPosition(uint32_t position, const Paragraph::TextEdit &edit,
                     bool marksRangeEnd) {
  if (position <= edit.start)
    return position;
  if (position >= edit.start + edit.removed)
    return position - edit.removed + edit.inserted;
  return marksRangeEnd ? edit.start + edit.inserted : edit.start;
}

// Clamps a query scope to the paragraph's text.
CharRange clampScope(const Paragraph &paragraph, CharRange scope) {
  const uint32_t textLength = static_cast<uint32_t>(paragraph.text().size());
  scope.start = std::min(scope.start, textLength);
  scope.end = std::min(std::max(scope.end, scope.start), textLength);
  return scope;
}

} // namespace

std::vector<CharRange> findAllOccurrences(const Paragraph &paragraph,
                                          std::u16string_view needle,
                                          CharRange scope) {
  std::vector<CharRange> matches;
  if (needle.empty())
    return matches;
  scope = clampScope(paragraph, scope);
  const std::u16string_view text =
      std::u16string_view(paragraph.text())
          .substr(scope.start, scope.end - scope.start);
  for (size_t matchOffset = text.find(needle);
       matchOffset != std::u16string_view::npos;
       matchOffset = text.find(needle, matchOffset + needle.size())) {
    matches.push_back(
        {scope.start + static_cast<uint32_t>(matchOffset),
         scope.start + static_cast<uint32_t>(matchOffset + needle.size())});
  }
  return matches;
}

std::vector<CharRange> findAllOccurrences(const Paragraph &paragraph,
                                          std::u16string_view needle) {
  return findAllOccurrences(
      paragraph, needle, {0, static_cast<uint32_t>(paragraph.text().size())});
}

std::vector<CharRange> findAllOccurrences(const Paragraph &paragraph,
                                          std::u8string_view utf8Needle,
                                          CharRange scope) {
  return findAllOccurrences(paragraph, std::u16string_view(toUtf16(utf8Needle)),
                            scope);
}

std::vector<CharRange> findAllOccurrences(const Paragraph &paragraph,
                                          std::u8string_view utf8Needle) {
  return findAllOccurrences(paragraph,
                            std::u16string_view(toUtf16(utf8Needle)));
}

std::optional<std::vector<CharRange>>
findRegexMatches(const Paragraph &paragraph, std::u8string_view utf8Pattern,
                 CharRange scope) {
  const std::u16string pattern = toUtf16(utf8Pattern);
  UErrorCode status = U_ZERO_ERROR;
  URegularExpression *regularExpression =
      uregex_open(reinterpret_cast<const UChar *>(pattern.data()),
                  static_cast<int32_t>(pattern.size()), 0, nullptr, &status);
  if (U_FAILURE(status) || !regularExpression) {
    if (regularExpression)
      uregex_close(regularExpression);
    return std::nullopt;
  }
  std::vector<CharRange> matches;
  scope = clampScope(paragraph, scope);
  // The engine sees only the window, so anchors and \b treat its edges as
  // the ends of the text — substring semantics, matching findAllOccurrences.
  const std::u16string &text = paragraph.text();
  uregex_setText(regularExpression,
                 reinterpret_cast<const UChar *>(text.data()) + scope.start,
                 static_cast<int32_t>(scope.end - scope.start), &status);
  while (U_SUCCESS(status) && uregex_findNext(regularExpression, &status)) {
    const int32_t matchStart = uregex_start(regularExpression, 0, &status);
    const int32_t matchEnd = uregex_end(regularExpression, 0, &status);
    if (U_FAILURE(status))
      break;
    matches.push_back({scope.start + static_cast<uint32_t>(matchStart),
                       scope.start + static_cast<uint32_t>(matchEnd)});
  }
  uregex_close(regularExpression);
  return matches;
}

std::optional<std::vector<CharRange>>
findRegexMatches(const Paragraph &paragraph, std::u8string_view utf8Pattern) {
  return findRegexMatches(paragraph, utf8Pattern,
                          {0, static_cast<uint32_t>(paragraph.text().size())});
}

std::vector<CharRange> wordRanges(Paragraph &paragraph,
                                  FontContext &fontContext) {
  paragraph.ensureShaped(fontContext);
  std::vector<CharRange> ranges;
  ranges.reserve(paragraph.words().size());
  for (const Word &word : paragraph.words())
    ranges.push_back({word.textBegin, word.textEnd});
  return ranges;
}

void MarkerSet::setRanges(std::string name, std::vector<CharRange> ranges) {
  m_markers[std::move(name)] = std::move(ranges);
}

const std::vector<CharRange> *
MarkerSet::rangesFor(std::string_view name) const {
  const auto marker = m_markers.find(name);
  return marker == m_markers.end() ? nullptr : &marker->second;
}

void MarkerSet::remove(std::string_view name) {
  const auto marker = m_markers.find(name);
  if (marker != m_markers.end())
    m_markers.erase(marker);
}

bool MarkerSet::synchronize(const Paragraph &paragraph) {
  if (paragraph.revision() == m_revision)
    return true;
  std::vector<Paragraph::TextEdit> edits;
  const bool historyAvailable = paragraph.editsSince(m_revision, edits);
  m_revision = paragraph.revision();
  if (!historyAvailable) {
    for (auto &markerEntry : m_markers)
      markerEntry.second.clear();
    return false;
  }
  for (auto &markerEntry : m_markers) {
    std::vector<CharRange> &ranges = markerEntry.second;
    for (const Paragraph::TextEdit &edit : edits) {
      for (CharRange &range : ranges) {
        range.start = mapPosition(range.start, edit, /*marksRangeEnd=*/false);
        range.end = mapPosition(range.end, edit, /*marksRangeEnd=*/true);
      }
    }
    std::erase_if(ranges, [](const CharRange &range) { return range.empty(); });
  }
  return true;
}

void MarkerSet::applyStyle(Paragraph &paragraph, std::string_view name,
                           const TextStyle &style) {
  (void)synchronize(paragraph);
  if (const std::vector<CharRange> *ranges = rangesFor(name))
    for (const CharRange &range : std::vector<CharRange>(*ranges))
      paragraph.setStyle(range.start, range.end, style);
  m_revision = paragraph.revision(); // style edits don't move text
}

void MarkerSet::applyPaint(Paragraph &paragraph, std::string_view name,
                           const PaintStyle &paint) {
  (void)synchronize(paragraph);
  // Batch: all ranges land in one span-list rebuild, so re-painting a large
  // marker set every frame stays O(spans + ranges).
  if (const std::vector<CharRange> *ranges = rangesFor(name))
    paragraph.setPaint(*ranges, paint);
  m_revision = paragraph.revision();
}

} // namespace textflow
