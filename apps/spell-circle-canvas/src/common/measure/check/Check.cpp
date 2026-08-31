#include <sigilmeasure/check/Check.h>

namespace sigil::measure {

std::vector<std::string> Table::lines(int labelWidth, int valueWidth) const {
  std::vector<std::string> out;
  if (rows.empty()) return out;
  out.reserve(rows.size() + 1);
  for (const Check& c : rows) out.push_back(c.line(labelWidth, valueWidth));
  const int failed = failures();
  char summary[96];
  if (failed == 0)
    std::snprintf(summary, sizeof summary, "  %zu checks, all passed",
                  rows.size());
  else
    std::snprintf(summary, sizeof summary, "  %zu checks, %d failed",
                  rows.size(), failed);
  out.emplace_back(summary);
  return out;
}

}  // namespace sigil::measure
