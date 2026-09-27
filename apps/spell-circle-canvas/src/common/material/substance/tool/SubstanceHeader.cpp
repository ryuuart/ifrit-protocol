/** @file
 * sigil_substance_header — writes the input struct of a `.sbsar`'s first
 * graph as a C++ header: `sigil_substance_header <archive> -o <header>`.
 * One `std::optional` field per numeric input the author exposed, named
 * by the identifier in camelCase as authored, a combobox or button input
 * as an `enum class` of its labels, a toggle as a `bool`; an unset field
 * keeps the author's or the preset's value. `inputs()` lists the set
 * fields as the keyed values `material::substance()` writes. The
 * engine's own inputs (`$outputsize`, `$randomseed`, `$normalformat`)
 * are the options' fields, and image and text inputs have no field.
 * The header is rewritten only when its text changes, so a rebuild of
 * the archive that changes nothing recompiles nothing.
 */

#include <sigilmaterial/substance/advanced/Archive.h>

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <vector>

using namespace sigil::material;

namespace {

const std::set<std::string>& keywords() {
  static const std::set<std::string> words = {
      "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case",
      "catch", "char", "class", "concept", "const", "constexpr", "continue",
      "default", "delete", "do", "double", "else", "enum", "explicit",
      "export", "extern", "false", "float", "for", "friend", "goto", "if",
      "inline", "int", "long", "namespace", "new", "not", "operator", "or",
      "private", "protected", "public", "register", "requires", "return",
      "short", "signed", "sizeof", "static", "struct", "switch", "template",
      "this", "throw", "true", "try", "typedef", "typename", "union",
      "unsigned", "using", "virtual", "void", "volatile", "while", "inputs",
      "stem", "path"};
  return words;
}

/** The words of an identifier or a label: runs of letters and digits. */
std::vector<std::string> wordsOf(const std::string& text) {
  std::vector<std::string> words;
  std::string word;
  for (const char character : text) {
    if (std::isalnum((unsigned char)character)) {
      word += character;
    } else if (!word.empty()) {
      words.push_back(word);
      word.clear();
    }
  }
  if (!word.empty()) words.push_back(word);
  return words;
}

/** `Autumn_Leaves` → `AutumnLeaves`; each word keeps its own spelling
 *  past its first letter. */
std::string pascal(const std::string& text, const char* fallback) {
  std::string out;
  for (std::string word : wordsOf(text)) {
    word[0] = (char)std::toupper((unsigned char)word[0]);
    out += word;
  }
  if (out.empty()) out = fallback;
  if (std::isdigit((unsigned char)out[0])) out = fallback + out;
  return out;
}

/** `Hue_Shift` → `hueShift`, `LeafType` → `leafType`. */
std::string camel(const std::string& text) {
  std::string out = pascal(text, "Input");
  out[0] = (char)std::tolower((unsigned char)out[0]);
  if (keywords().contains(out)) out += "Value";
  return out;
}

std::string quoted(const std::string& text) {
  std::string out = "\"";
  for (const char character : text) {
    if (character == '"' || character == '\\') out += '\\';
    if (character == '\n') {
      out += "\\n";
      continue;
    }
    out += character;
  }
  return out + "\"";
}

/** A comment line's text: one line, no closing of the comment. */
std::string oneLine(std::string text) {
  for (char& character : text)
    if (character == '\n' || character == '\r') character = ' ';
  size_t at;
  while ((at = text.find("*/")) != std::string::npos) text.replace(at, 2, "* /");
  return text;
}

std::string plain(float value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

int componentsOf(sbsar::InputType type) {
  switch (type) {
    case sbsar::InputType::Float:
    case sbsar::InputType::Integer:
      return 1;
    case sbsar::InputType::Float2:
    case sbsar::InputType::Integer2:
      return 2;
    case sbsar::InputType::Float3:
    case sbsar::InputType::Integer3:
      return 3;
    case sbsar::InputType::Float4:
    case sbsar::InputType::Integer4:
      return 4;
    default:
      return 0;
  }
}

bool isInteger(sbsar::InputType type) {
  return type == sbsar::InputType::Integer ||
         type == sbsar::InputType::Integer2 ||
         type == sbsar::InputType::Integer3 ||
         type == sbsar::InputType::Integer4;
}

std::string header(const std::string& stem, const std::string& path,
                   const sbsar::Description& graph) {
  const std::string name = pascal(stem, "Archive");
  std::ostringstream enums, fields, writes;
  std::set<std::string> taken;
  for (const sbsar::Input& input : graph.inputs) {
    const int components = componentsOf(input.type);
    if (components == 0 || input.name.empty() || input.name[0] == '$')
      continue;
    std::string field = camel(input.name);
    while (taken.contains(field)) field += "Input";
    taken.insert(field);
    std::string summary = input.label.empty() ? input.name : input.label;
    if (!input.minimum.empty() && !input.maximum.empty() && components == 1 &&
        input.choices.empty())
      summary += ", " + plain(input.minimum[0]) + " to " +
                 plain(input.maximum[0]);
    std::string type;
    std::string value;
    if (components == 1 && !input.choices.empty()) {
      type = pascal(input.name, "Choice");
      if (keywords().contains(type) || type == name) type += "Choice";
      enums << "  /** " << oneLine(input.label.empty() ? input.name : input.label)
            << " */\n  enum class " << type << " : int {\n";
      std::set<std::string> labels;
      for (const sbsar::Choice& choice : input.choices) {
        std::string label = pascal(choice.label, "Choice");
        if (labels.contains(label)) label += std::to_string(choice.value);
        labels.insert(label);
        enums << "    " << label << " = " << choice.value << ",\n";
      }
      enums << "  };\n";
      value = "(float)static_cast<int>(*" + field + ")";
    } else if (components == 1 && input.widget == sbsar::Widget::Toggle) {
      type = "bool";
      value = "*" + field + " ? 1.0f : 0.0f";
    } else if (components == 1) {
      type = isInteger(input.type) ? "int" : "float";
      value = "(float)*" + field;
    } else {
      type = std::string("std::array<") +
             (isInteger(input.type) ? "int" : "float") + ", " +
             std::to_string(components) + ">";
      value = "std::vector<float>(" + field + "->begin(), " + field +
              "->end())";
    }
    fields << "  /** " << oneLine(summary) << " */\n  std::optional<" << type
           << "> " << field << ";\n";
    writes << "    if (" << field << ") stated.push_back({"
           << quoted(input.name) << ", " << value << "});\n";
  }

  std::ostringstream out;
  out << "#pragma once\n\n"
      << "/** @file\n"
      << " * The inputs of " << oneLine(stem)
      << ".sbsar's first graph, written by the build from the archive.\n"
      << " * An unset field keeps the author's or the preset's value.\n"
      << " */\n\n"
      << "#include <sigilmaterial/substance/Substance.h>\n\n"
      << "#include <array>\n#include <optional>\n#include <string_view>\n"
      << "#include <vector>\n\n"
      << "/** " << oneLine(graph.graph.empty() ? stem : graph.graph)
      << " */\n"
      << "struct " << name << " {\n"
      << "  /** The archive's file name without its extension. */\n"
      << "  static constexpr std::string_view stem = " << quoted(stem) << ";\n"
      << "  /** Where the archive stood when this was written. */\n"
      << "  static constexpr std::string_view path = " << quoted(path) << ";\n"
      << enums.str() << fields.str()
      << "  /** The set fields, as keyed values by identifier. */\n"
      << "  std::vector<sigil::material::sbsar::InputValue> inputs() const {\n"
      << "    std::vector<sigil::material::sbsar::InputValue> stated;\n"
      << writes.str() << "    return stated;\n  }\n};\n";
  return out.str();
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4 || std::string(argv[2]) != "-o") {
    std::fprintf(stderr, "usage: %s <archive.sbsar> -o <header>\n", argv[0]);
    return 2;
  }
  const std::filesystem::path archivePath = argv[1];
  const std::filesystem::path headerPath = argv[3];
  std::ifstream in(archivePath, std::ios::binary);
  const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
  const std::optional<sbsar::Archive> archive = sbsar::Archive::decode(
      std::as_bytes(std::span<const char>(bytes.data(), bytes.size())));
  if (!archive || archive->graphCount() == 0) {
    std::fprintf(stderr, "%s: %s is not a Substance archive\n", argv[0],
                 archivePath.string().c_str());
    return 1;
  }
  const std::string text =
      header(archivePath.stem().string(),
             std::filesystem::absolute(archivePath).string(), archive->graph(0));
  std::string existing;
  if (std::ifstream current{headerPath}) {
    existing.assign(std::istreambuf_iterator<char>(current),
                    std::istreambuf_iterator<char>());
  }
  if (existing != text) {
    std::filesystem::create_directories(headerPath.parent_path());
    std::ofstream(headerPath) << text;
  }
  return 0;
}
