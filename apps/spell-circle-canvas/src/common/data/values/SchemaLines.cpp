#include "SchemaLines.h"

#include <ostream>
#include <string>
#include <vector>

namespace sigil::data::schema {

void writeDocComment(std::ostream& out, const std::vector<std::string>& lines,
                     const std::string& indent) {
  if (lines.empty()) return;
  if (lines.size() == 1) {
    out << indent << "/**" << lines[0] << " */\n";
    return;
  }
  out << indent << "/**" << lines[0] << "\n";
  for (size_t i = 1; i < lines.size(); ++i)
    out << indent << " *" << lines[i] << "\n";
  out << indent << " */\n";
}

void writeCall(std::ostream& out, const std::string& head,
               const std::string& tail, const std::string& indent) {
  if (head.size() + tail.size() <= 80) {
    out << head << tail << "\n";
    return;
  }
  out << head << "\n" << indent << tail << "\n";
}

void writeAssignment(std::ostream& out, const std::string& head,
                     const std::string& tail, const std::string& indent) {
  if (head.size() + 1 + tail.size() <= 80) {
    out << head << " " << tail << "\n";
    return;
  }
  out << head << "\n" << indent << tail << "\n";
}

}  // namespace sigil::data::schema
