#include "ProtocolWriting.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace sigil::protocol::generator {

std::string cppSpace(const std::string& dotted) {
  std::string out;
  for (const char each : dotted) {
    if (each == '.')
      out += "::";
    else
      out += each;
  }
  return out;
}

std::string valueType(const std::string& table, const std::string& from) {
  const size_t dot = table.rfind('.');
  const std::string space = dot == std::string::npos ? "" : table.substr(0, dot);
  const std::string name = dot == std::string::npos ? table : table.substr(dot + 1);
  if (cppSpace(space) == from) return "values::" + name;
  return "::" + cppSpace(space) + "::values::" + name;
}

std::string raised(const std::string& name) {
  std::string out = name;
  if (!out.empty())
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
  return out;
}

void writeDocComment(std::ostream& out, const Documented& part,
                     const std::string& indent, const std::string& more) {
  std::vector<std::string> lines = part.documentation;
  if (part.experimental) {
    lines.push_back("");
    lines.push_back("EXPERIMENTAL: its shape may still change.");
  }
  if (!more.empty()) {
    lines.push_back("");
    std::istringstream paragraph(more);
    std::string line;
    while (std::getline(paragraph, line)) lines.push_back(line);
  }
  if (lines.empty()) return;
  if (lines.size() == 1) {
    out << indent << "/** " << lines[0] << " */\n";
    return;
  }
  out << indent << "/** " << lines[0] << "\n";
  for (size_t i = 1; i < lines.size(); ++i) {
    if (lines[i].empty())
      out << indent << " *\n";
    else
      out << indent << " *  " << lines[i] << "\n";
  }
  out << indent << " */\n";
}

void writeBanner(std::ostream& out, const std::string& what) {
  out << "// " << what << ".\n"
      << "// Written from protocol.fbs by sigil_protocol: edit the definition;\n"
      << "// this is a build artefact and never hand-edited.\n\n";
}

bool writeFile(const std::string& path, const std::string& text,
               std::string* why) {
  std::error_code trouble;
  std::filesystem::create_directories(
      std::filesystem::path(path).parent_path(), trouble);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file << text;
  file.close();
  if (!file) {
    if (why) *why = "cannot write " + path;
    return false;
  }
  return true;
}

}  // namespace sigil::protocol::generator
