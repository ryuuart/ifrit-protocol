#include "textflow/Query.h"

#include <unicode/uregex.h>
#include <unicode/ustring.h>

#include <algorithm>

namespace textflow {

namespace {

std::u16string toUtf16(std::string_view utf8) {
  if (utf8.empty())
    return {};
  std::u16string out;
  out.resize(utf8.size());
  UErrorCode status = U_ZERO_ERROR;
  int32_t written = 0;
  u_strFromUTF8(reinterpret_cast<UChar *>(out.data()),
                static_cast<int32_t>(out.size()), &written, utf8.data(),
                static_cast<int32_t>(utf8.size()), &status);
  if (U_FAILURE(status))
    return {};
  out.resize(static_cast<size_t>(written));
  return out;
}

// Marker-adjustment gravity for one recorded edit: positions before the
// replaced region stay, positions after shift, positions inside snap to the
// region's start (range starts) or past the insertion (range ends) so an
// overlapped range absorbs the replacement.
uint32_t mapPos(uint32_t pos, const Paragraph::EditOp &op, bool isEnd) {
  if (pos <= op.start)
    return pos;
  if (pos >= op.start + op.removed)
    return pos - op.removed + op.inserted;
  return isEnd ? op.start + op.inserted : op.start;
}

} // namespace

std::vector<CharRange> findAll(const Paragraph &para,
                               std::u16string_view needle) {
  std::vector<CharRange> out;
  if (needle.empty())
    return out;
  const std::u16string &text = para.text();
  for (size_t pos = text.find(needle); pos != std::u16string::npos;
       pos = text.find(needle, pos + needle.size())) {
    out.push_back({static_cast<uint32_t>(pos),
                   static_cast<uint32_t>(pos + needle.size())});
  }
  return out;
}

std::vector<CharRange> findAll(const Paragraph &para,
                               std::string_view utf8Needle) {
  return findAll(para, std::u16string_view(toUtf16(utf8Needle)));
}

std::optional<std::vector<CharRange>> findRegex(const Paragraph &para,
                                                std::string_view utf8Pattern) {
  const std::u16string pattern = toUtf16(utf8Pattern);
  UErrorCode status = U_ZERO_ERROR;
  URegularExpression *re =
      uregex_open(reinterpret_cast<const UChar *>(pattern.data()),
                  static_cast<int32_t>(pattern.size()), 0, nullptr, &status);
  if (U_FAILURE(status) || !re) {
    if (re)
      uregex_close(re);
    return std::nullopt;
  }
  std::vector<CharRange> out;
  const std::u16string &text = para.text();
  uregex_setText(re, reinterpret_cast<const UChar *>(text.data()),
                 static_cast<int32_t>(text.size()), &status);
  while (U_SUCCESS(status) && uregex_findNext(re, &status)) {
    const int32_t s = uregex_start(re, 0, &status);
    const int32_t e = uregex_end(re, 0, &status);
    if (U_FAILURE(status))
      break;
    out.push_back({static_cast<uint32_t>(s), static_cast<uint32_t>(e)});
  }
  uregex_close(re);
  return out;
}

std::vector<CharRange> wordRanges(Paragraph &para, FontContext &ctx) {
  para.ensureShaped(ctx);
  std::vector<CharRange> out;
  out.reserve(para.words().size());
  for (const Word &word : para.words())
    out.push_back({word.textBegin, word.textEnd});
  return out;
}

void MarkerSet::set(std::string name, std::vector<CharRange> ranges) {
  m_marks[std::move(name)] = std::move(ranges);
}

const std::vector<CharRange> *MarkerSet::get(std::string_view name) const {
  const auto it = m_marks.find(name);
  return it == m_marks.end() ? nullptr : &it->second;
}

void MarkerSet::erase(std::string_view name) {
  const auto it = m_marks.find(name);
  if (it != m_marks.end())
    m_marks.erase(it);
}

bool MarkerSet::sync(const Paragraph &para) {
  if (para.revision() == m_revision)
    return true;
  std::vector<Paragraph::EditOp> ops;
  const bool reachable = para.editsSince(m_revision, ops);
  m_revision = para.revision();
  if (!reachable) {
    for (auto &[name, ranges] : m_marks)
      ranges.clear();
    return false;
  }
  for (auto &[name, ranges] : m_marks) {
    for (const Paragraph::EditOp &op : ops) {
      for (CharRange &range : ranges) {
        range.start = mapPos(range.start, op, /*isEnd=*/false);
        range.end = mapPos(range.end, op, /*isEnd=*/true);
      }
    }
    ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
                                [](const CharRange &r) { return r.empty(); }),
                 ranges.end());
  }
  return true;
}

void MarkerSet::applyStyle(Paragraph &para, std::string_view name,
                           const TextStyle &style) {
  sync(para);
  if (const std::vector<CharRange> *ranges = get(name))
    for (const CharRange &range : std::vector<CharRange>(*ranges))
      para.setStyle(range.start, range.end, style);
  m_revision = para.revision(); // style edits don't move text
}

void MarkerSet::applyPaint(Paragraph &para, std::string_view name,
                           const PaintStyle &paint) {
  sync(para);
  if (const std::vector<CharRange> *ranges = get(name))
    for (const CharRange &range : std::vector<CharRange>(*ranges))
      para.setPaint(range.start, range.end, paint);
  m_revision = para.revision();
}

} // namespace textflow
