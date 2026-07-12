#pragma once

// Optional convenience layer over Paragraph — nothing in the core pipeline
// depends on it, and applications with their own selection/annotation
// systems can ignore it entirely. It exists for the HTML/JS-flavoured
// workflow: find ranges by substring, word, or ICU regular expression,
// style them, or register them in a MarkerSet that follows subsequent
// edits the way DOM Range objects do.

#include "Paragraph.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace textflow {

class FontContext;

// UTF-16 code-unit range into a Paragraph's text, end exclusive.
struct CharRange {
  uint32_t start = 0;
  uint32_t end = 0;
  bool empty() const { return end <= start; }
  bool operator==(const CharRange &) const = default;
};

// Every non-overlapping occurrence of `needle`, left to right.
std::vector<CharRange> findAll(const Paragraph &para,
                               std::u16string_view needle);
std::vector<CharRange> findAll(const Paragraph &para,
                               std::string_view utf8Needle);

// Every match of an ICU regular expression (full Unicode semantics; the
// pattern is UTF-8). std::nullopt when the pattern does not compile.
std::optional<std::vector<CharRange>> findRegex(const Paragraph &para,
                                                std::string_view utf8Pattern);

// The content range of every word — the line-break segments the layout
// itself uses, trailing whitespace excluded. Shapes on demand (cache-hot).
std::vector<CharRange> wordRanges(Paragraph &para, FontContext &ctx);

// Named range sets that follow edits. Ranges are adjusted by replaying the
// paragraph's recorded edit ops:
//   - text inserted/removed before a range shifts it;
//   - a replacement overlapping a range is absorbed into it (the range
//     grows to cover the inserted text);
//   - ranges that collapse to empty are dropped.
// An insertion exactly at a range's start joins the range; one exactly at
// its (exclusive) end does not.
class MarkerSet {
public:
  MarkerSet() = default;
  explicit MarkerSet(const Paragraph &para) : m_revision(para.revision()) {}

  // Ranges must be valid for the paragraph revision the set was last
  // synced/constructed against.
  void set(std::string name, std::vector<CharRange> ranges);
  const std::vector<CharRange> *get(std::string_view name) const;
  void erase(std::string_view name);

  // Replays edits since the last sync. Returns false when the paragraph's
  // bounded edit history no longer reaches back that far — every range is
  // cleared and callers must re-query (findAll/findRegex) and set() again.
  bool sync(const Paragraph &para);

  // sync() + Paragraph::setStyle / setPaint over every range of `name`.
  void applyStyle(Paragraph &para, std::string_view name,
                  const TextStyle &style);
  void applyPaint(Paragraph &para, std::string_view name,
                  const PaintStyle &paint);

private:
  uint64_t m_revision = 0;
  std::map<std::string, std::vector<CharRange>, std::less<>> m_marks;
};

} // namespace textflow
