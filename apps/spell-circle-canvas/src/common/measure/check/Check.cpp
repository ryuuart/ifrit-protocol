#include <sigilmeasure/advanced/CheckFormat.h>
#include <sigilmeasure/check/Check.h>

#include <cstdio>

namespace sigil::measure {

std::string line(const Check& row, CheckColumns columns) {
  if (row.standing == Standing::Heading) return row.label;
  std::string out = "  " + row.label;
  if ((int)row.label.size() < columns.labelWidth)
    out.append((size_t)columns.labelWidth - row.label.size(), ' ');
  out += ' ';
  if ((int)row.actual.size() < columns.valueWidth)
    out.append((size_t)columns.valueWidth - row.actual.size(), ' ');
  out += row.actual;
  if (row.standing == Standing::Reading) return out;
  out += row.pass ? "   PASS" : "   FAIL want " + row.expected;
  return out;
}

int failures(std::span<const Check> rows) {
  int failed = 0;
  for (const Check& row : rows)
    failed += (!row.pass && row.standing == Standing::Claim) ? 1 : 0;
  return failed;
}

int findings(std::span<const Check> rows) {
  int found = 0;
  for (const Check& row : rows)
    found += (!row.pass && row.standing == Standing::Finding) ? 1 : 0;
  return found;
}

int CheckTable::failures() const { return measure::failures(rows); }

int CheckTable::findings() const { return measure::findings(rows); }

int CheckTable::checks() const {
  int judged = 0;
  for (const Check& row : rows) judged += row.judged() ? 1 : 0;
  return judged;
}

std::vector<std::string> CheckTable::lines(CheckColumns columns) const {
  std::vector<std::string> out;
  if (rows.empty()) return out;
  out.reserve(rows.size() + 1);
  for (const Check& row : rows) out.push_back(line(row, columns));
  const int failed = failures();
  const int found = findings();
  char summary[128];
  if (failed == 0)
    std::snprintf(summary, sizeof summary, "  %d checks, all passed", checks());
  else
    std::snprintf(summary, sizeof summary, "  %d checks, %d failed", checks(),
                  failed);
  std::string text = summary;
  if (found > 0) {
    std::snprintf(summary, sizeof summary, ", %d finding%s", found,
                  found == 1 ? "" : "s");
    text += summary;
  }
  out.push_back(std::move(text));
  return out;
}

}  // namespace sigil::measure
