#include <sigilmeasure/check/Check.h>

namespace sigil::measure {

std::vector<std::string> Table::lines(int labelWidth, int valueWidth) const {
  std::vector<std::string> out;
  if (rows.empty()) return out;
  out.reserve(rows.size() + 1);
  for (const Check& c : rows) out.push_back(c.line(labelWidth, valueWidth));
  const int failed = failures();
  const int found = findings();
  char summary[128];
  if (failed == 0)
    std::snprintf(summary, sizeof summary, "  %d checks, all passed", checks());
  else
    std::snprintf(summary, sizeof summary, "  %d checks, %d failed", checks(),
                  failed);
  std::string line = summary;
  if (found > 0) {
    std::snprintf(summary, sizeof summary, ", %d finding%s", found,
                  found == 1 ? "" : "s");
    line += summary;
  }
  out.push_back(std::move(line));
  return out;
}

}  // namespace sigil::measure
