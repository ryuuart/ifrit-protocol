/** @file
 * The one rule for where a sketch stands on disk, applied to a key and
 * to an entry file.
 */

#include "sigilsketch/core/Sources.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <system_error>
#include <utility>

namespace sigil::sketch {

namespace {

std::string trimmed(const std::string& text) {
  const size_t from = text.find_first_not_of(" \t\r");
  if (from == std::string::npos) return {};
  return text.substr(from, text.find_last_not_of(" \t\r") + 1 - from);
}

/** A line of nothing but rule characters, which authors use as a
 *  section break where a blank line would be too quiet. */
bool isRule(const std::string& text) {
  return text.size() >= 3 && text.find_first_not_of("=-") == std::string::npos;
}

/** One header line with its comment marker taken off, or nothing when
 *  the line carries no text of its own. Leading indentation SURVIVES:
 *  the knobs an author lists are indented under their heading, and a
 *  list flattened to the left margin stops reading as one. */
std::string undecorate(std::string text) {
  const std::string bare = trimmed(text);
  if (bare.starts_with("*/")) return {};
  if (bare.rfind("///", 0) == 0)
    text = text.substr(text.find("///") + 3);
  else if (bare.rfind("//", 0) == 0)
    text = text.substr(text.find("//") + 2);
  else if (bare.rfind("/**", 0) == 0)
    text = text.substr(text.find("/**") + 3);
  else if (bare.rfind("/*", 0) == 0)
    text = text.substr(text.find("/*") + 2);
  else if (bare.rfind('*', 0) == 0)
    text = text.substr(text.find('*') + 1);
  if (const size_t close = text.find("*/"); close != std::string::npos)
    text = text.substr(0, close);
  if (!text.empty() && text.front() == ' ') text = text.substr(1);
  const std::string content = trimmed(text);
  if (content.empty() || content == "@file" || isRule(content)) return {};
  return text;
}

/** Prose wraps in a header and does not wrap in a panel, so a paragraph
 *  of prose comes back as one line. */
std::string unwrap(const std::vector<std::string>& paragraph) {
  std::string out;
  for (const std::string& line : paragraph) {
    if (!out.empty()) out += ' ';
    out += trimmed(line);
  }
  return out;
}

/** A LIST OF KNOBS, one line each. It keeps its own line breaks, unlike
 *  prose — and loses both the indentation that put it under a heading it
 *  is no longer under and the wrapping the file's own margin forced on
 *  it: a line indented deeper than the first is an entry that ran long,
 *  so it rejoins the line above rather than reading as a knob of its
 *  own. */
std::string knobs(const std::vector<std::string>& paragraph) {
  size_t base = std::string::npos;
  for (const std::string& line : paragraph) {
    const size_t indent = line.find_first_not_of(" \t");
    if (indent != std::string::npos) base = std::min(base, indent);
  }
  std::string out;
  for (const std::string& line : paragraph) {
    const size_t indent = line.find_first_not_of(" \t");
    if (indent == std::string::npos) continue;
    if (!out.empty() && indent > base)
      out += ' ' + trimmed(line);
    else
      out += (out.empty() ? "" : "\n") + trimmed(line);
  }
  return out;
}

/** A paragraph of one line that ends no sentence: a heading over the
 *  paragraph below it rather than a statement of its own. */
bool isHeading(const std::vector<std::string>& paragraph) {
  if (paragraph.size() != 1) return false;
  const std::string line = trimmed(paragraph.front());
  return !line.empty() && line.back() != '.';
}

void appendTags(SourceMetadata& header, std::string_view text) {
  while (!text.empty()) {
    const size_t comma = text.find(',');
    const std::string_view tag = text.substr(0, comma);
    std::string path;
    size_t start = 0;
    while (start < tag.size()) {
      const size_t slash = tag.find('/', start);
      const auto part = trimmed(std::string(tag.substr(start, slash - start)));
      if (!part.empty()) {
        if (!path.empty()) path += '/';
        path += part;
      }
      if (slash == std::string_view::npos) break;
      start = slash + 1;
    }
    if (!path.empty() && std::find(header.tags.begin(), header.tags.end(),
                                   path) == header.tags.end())
      header.tags.push_back(std::move(path));
    if (comma == std::string_view::npos) break;
    text.remove_prefix(comma + 1);
  }
}

}  // namespace

SourceMetadata sourceMetadata(const std::filesystem::path& file) {
  SourceMetadata header;
  std::ifstream stream(file);
  if (!stream) return header;

  std::vector<std::vector<std::string>> paragraphs;
  std::vector<std::string> current;
  bool inHeader = true;
  bool inBlock = false;
  std::string line;
  while (std::getline(stream, line)) {
    ++header.lines;
    if (!inHeader) continue;
    const std::string bare = trimmed(line);
    std::string text;
    if (inBlock) {
      text = undecorate(line);
      if (bare.find("*/") != std::string::npos) inBlock = false;
    } else if (bare.empty()) {
      text = {};
    } else if (bare.rfind("//", 0) == 0) {
      text = undecorate(line);
    } else if (bare.rfind("/*", 0) == 0) {
      text = undecorate(line);
      inBlock = bare.find("*/") == std::string::npos;
    } else {
      inHeader = false;  // the first line of code closes the header
      continue;
    }
    const auto content = trimmed(text);
    if (content.starts_with("TAGS:")) {
      appendTags(header, std::string_view(content).substr(5));
      continue;
    }
    if (text.empty()) {
      if (!current.empty()) paragraphs.push_back(std::exchange(current, {}));
    } else {
      current.push_back(text);
    }
  }
  if (!current.empty()) paragraphs.push_back(current);

  // The subject: past the title, then headings until something says
  // something.
  for (size_t i = 1; i < paragraphs.size(); ++i) {
    if (!header.subject.empty()) header.subject += '\n';
    header.subject += unwrap(paragraphs[i]);
    if (!isHeading(paragraphs[i])) break;
  }
  for (const std::vector<std::string>& paragraph : paragraphs) {
    if (trimmed(paragraph.front()) != "EDIT THESE FIRST") continue;
    header.editFirst = knobs({paragraph.begin() + 1, paragraph.end()});
    break;
  }
  return header;
}

std::filesystem::path sourceOf(const std::filesystem::path& dir,
                               std::string_view key) {
  const std::filesystem::path name(std::string(key) + ".cpp");
  const std::filesystem::path entry = dir / std::filesystem::path(key) / name;
  std::error_code ec;
  if (std::filesystem::is_regular_file(entry, ec)) return entry;
  return dir / name;
}

bool directorySketch(const std::filesystem::path& entry) {
  return entry.extension() == ".cpp" && entry.has_parent_path() &&
         entry.parent_path().filename() == entry.stem();
}

std::vector<std::filesystem::path> sourcesUnder(
    const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> sources;
  std::error_code ec;
  for (auto it = std::filesystem::directory_iterator(dir, ec);
       !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
    if (it->path().extension() == ".cpp") sources.push_back(it->path());
  }
  std::sort(sources.begin(), sources.end());
  return sources;
}

std::vector<std::filesystem::path> unitsOf(const std::filesystem::path& entry) {
  std::vector<std::filesystem::path> units{entry};
  if (!directorySketch(entry)) return units;
  for (std::filesystem::path& source : sourcesUnder(entry.parent_path()))
    if (source != entry) units.push_back(std::move(source));
  return units;
}

std::vector<std::filesystem::path> headersOf(
    const std::filesystem::path& entry) {
  std::vector<std::filesystem::path> pending = unitsOf(entry);
  std::set<std::filesystem::path> visited;
  std::set<std::filesystem::path> headers;
  while (!pending.empty()) {
    const auto file = pending.back().lexically_normal();
    pending.pop_back();
    if (!visited.insert(file).second) continue;
    std::ifstream stream(file);
    std::string line;
    while (std::getline(stream, line)) {
      line = trimmed(line);
      if (line.empty() || line.front() != '#') continue;
      line = trimmed(line.substr(1));
      if (!line.starts_with("include")) continue;
      line = trimmed(line.substr(7));
      if (line.empty() || line.front() != '"') continue;
      const auto end = line.find('"', 1);
      if (end == std::string::npos) continue;
      auto included =
          (file.parent_path() / line.substr(1, end - 1)).lexically_normal();
      if (headers.insert(included).second)
        pending.push_back(std::move(included));
    }
  }
  return {headers.begin(), headers.end()};
}

}  // namespace sigil::sketch
