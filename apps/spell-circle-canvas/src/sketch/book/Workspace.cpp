#include "Workspace.h"

#include <QtCore/QRegularExpression>
#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <system_error>

namespace sketchbook {
namespace {

namespace fs = std::filesystem;

constexpr size_t historyLimit = 12;
constexpr size_t entryLimit = 20'000;
constexpr size_t sourceLimit = 1024 * 1024;
constexpr size_t sourceBudget = 32 * sourceLimit;
constexpr int depthLimit = 12;
constexpr auto historyKey = "workspace/recent";

fs::path normalized(const fs::path& path) {
  if (path.empty()) return {};
  std::error_code error;
  const fs::path absolute = fs::absolute(path, error);
  if (error) return path.lexically_normal();
  const fs::path canonical = fs::weakly_canonical(absolute, error);
  return error ? absolute.lexically_normal() : canonical;
}

QString text(const fs::path& path) {
  return QString::fromStdString(path.string());
}

WorkspaceLocation normalized(WorkspaceLocation location) {
  location.root = normalized(location.root);
  if (!location.root.empty() && !location.file.empty() &&
      location.file.is_relative())
    location.file = location.root / location.file;
  location.file = normalized(location.file);
  return location;
}

const fs::path& address(const WorkspaceLocation& location) {
  return location.root.empty() ? location.file : location.root;
}

std::vector<WorkspaceLocation> locations(QSettings& settings) {
  settings.sync();
  std::vector<WorkspaceLocation> result;
  std::set<fs::path> seen;
  for (const QVariant& value : settings.value(historyKey).toList()) {
    const QVariantMap row = value.toMap();
    WorkspaceLocation location = normalized(WorkspaceLocation{
        row.value(QStringLiteral("root")).toString().toStdString(),
        row.value(QStringLiteral("file")).toString().toStdString()});
    if (address(location).empty() || !seen.insert(address(location)).second)
      continue;
    result.push_back(std::move(location));
    if (result.size() == historyLimit) break;
  }
  return result;
}

bool excludedDirectory(std::string_view name) {
  if (name.empty() || name.front() == '.') return true;
  static constexpr std::array<std::string_view, 22> excluded{
      "build",         "_build",       "dist",
      "target",        "venv",         "env",
      "__pycache__",   "node_modules", "vendor",
      "vendors",       "third_party",  "third-party",
      "thirdparty",    "external",     "extern",
      "deps",          "dependencies", "vcpkg_installed",
      "site-packages", "CMakeFiles",   "cache",
      "caches"};
  return std::ranges::find(excluded, name) != excluded.end() ||
         name.starts_with("build-") || name.starts_with("build_") ||
         name.starts_with("cmake-build-");
}

bool separateProject(const fs::path& directory) {
  std::error_code error;
  return fs::is_regular_file(directory / "pyproject.toml", error) ||
         fs::is_regular_file(directory / ".venv" / "pyvenv.cfg", error);
}

/** Source discovery has no interpreter or compiler. Blanking comments and
 *  literals keeps declarations in documentation from becoming catalogue rows.
 */
std::string declarations(std::string source, bool python) {
  const auto blank = [&](size_t first, size_t last) {
    for (size_t i = first; i < last; ++i)
      if (source[i] != '\n' && source[i] != '\r') source[i] = ' ';
  };
  size_t i = 0;
  while (i < source.size()) {
    const size_t first = i;
    if ((python && source[i] == '#') ||
        (!python && source.compare(i, 2, "//") == 0)) {
      const size_t end = source.find('\n', i);
      i = end == std::string::npos ? source.size() : end;
    } else if (!python && source.compare(i, 2, "/*") == 0) {
      const size_t end = source.find("*/", i + 2);
      i = end == std::string::npos ? source.size() : end + 2;
    } else if (!python && source.compare(i, 2, "R\"") == 0) {
      const size_t open = source.find('(', i + 2);
      if (open == std::string::npos || open - i > 18) {
        ++i;
        continue;
      }
      const std::string close = ")" + source.substr(i + 2, open - i - 2) + '"';
      const size_t end = source.find(close, open + 1);
      i = end == std::string::npos ? source.size() : end + close.size();
    } else if (!python && source[i] == '\'' && i > 0 &&
               std::isxdigit((unsigned char)source[i - 1]) &&
               i + 1 < source.size() &&
               std::isxdigit((unsigned char)source[i + 1])) {
      ++i;
      continue;
    } else if (source[i] == '\'' || source[i] == '"') {
      const char quote = source[i];
      const bool triple = python && i + 2 < source.size() &&
                          source[i + 1] == quote && source[i + 2] == quote;
      const size_t width = triple ? 3 : 1;
      const std::string closing(width, quote);
      i += width;
      while (i < source.size()) {
        if (source[i] == '\\') {
          i = std::min(i + 2, source.size());
        } else if (source.compare(i, width, closing) == 0) {
          i += width;
          break;
        } else {
          ++i;
        }
      }
    } else {
      ++i;
      continue;
    }
    blank(first, i);
  }
  return source;
}

bool entrySource(const fs::path& path, size_t& bytesLeft) {
  const bool python = path.extension() == ".py";
  if (!python && path.extension() != ".cpp") return false;
  if (path.stem() == path.parent_path().filename()) return true;
  if (bytesLeft == 0) return false;
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return false;
  std::string source(std::min(sourceLimit, bytesLeft), '\0');
  stream.read(source.data(), (std::streamsize)source.size());
  source.resize((size_t)stream.gcount());
  bytesLeft -= source.size();
  const QString code =
      QString::fromStdString(declarations(std::move(source), python));
  static const QRegularExpression cpp(
      QStringLiteral(R"(\bSIGIL_SKETCH(?:_AS)?\s*\()"));
  static const QRegularExpression py(QStringLiteral(
      R"((?m)^[ \t]*@(?:[A-Za-z_]\w*[ \t]*\.[ \t]*)*sketch\b[ \t]*(?:\(|$))"));
  return (python ? py : cpp).match(code).hasMatch();
}

struct Discovery {
  size_t entriesLeft = entryLimit;
  size_t bytesLeft = sourceBudget;
  std::set<fs::path> files;

  void visit(const fs::path& directory, int depth) {
    std::error_code error;
    fs::directory_iterator iterator(
        directory, fs::directory_options::skip_permission_denied, error);
    std::vector<fs::directory_entry> entries;
    while (!error && iterator != fs::directory_iterator{} && entriesLeft > 0) {
      entries.push_back(*iterator);
      --entriesLeft;
      iterator.increment(error);
    }
    std::ranges::sort(entries, {}, &fs::directory_entry::path);
    for (const auto& entry : entries) {
      const fs::file_status status = entry.symlink_status(error);
      if (error || fs::is_symlink(status)) continue;
      const std::string name = entry.path().filename().string();
      if (name.empty() || name.front() == '.') continue;
      if (fs::is_directory(status)) {
        if (depth < depthLimit && !excludedDirectory(name) && entriesLeft > 0 &&
            !separateProject(entry.path()))
          visit(entry.path(), depth + 1);
      } else if (fs::is_regular_file(status) &&
                 entrySource(entry.path(), bytesLeft)) {
        files.insert(normalized(entry.path()));
      }
    }
  }
};

}  // namespace

std::vector<fs::path> workspaceFiles(const fs::path& root) {
  if (root.empty()) return {};
  Discovery discovery;
  discovery.visit(normalized(root), 0);
  return {discovery.files.begin(), discovery.files.end()};
}

WorkspaceHistory::WorkspaceHistory(QSettings& settings)
    : m_settings(settings) {}

void WorkspaceHistory::remember(const WorkspaceLocation& value) {
  WorkspaceLocation location = normalized(value);
  if (address(location).empty()) return;
  if (!location.root.empty() && location.file.empty())
    location.file = forRoot(location.root);
  auto previous = locations(m_settings);
  std::erase_if(previous, [&](const WorkspaceLocation& old) {
    return address(old) == address(location);
  });
  previous.insert(previous.begin(), std::move(location));
  if (previous.size() > historyLimit) previous.resize(historyLimit);
  QVariantList stored;
  for (const WorkspaceLocation& item : previous) {
    stored.push_back(QVariantMap{{QStringLiteral("root"), text(item.root)},
                                 {QStringLiteral("file"), text(item.file)}});
  }
  m_settings.setValue(historyKey, stored);
  m_settings.sync();
}

QVariantList WorkspaceHistory::recent() const {
  QVariantList result;
  for (const WorkspaceLocation& location : locations(m_settings)) {
    const fs::path& path = address(location);
    const bool folder = !location.root.empty();
    std::error_code error;
    const bool exists = folder ? fs::is_directory(path, error)
                               : fs::is_regular_file(path, error);
    result.push_back(
        QVariantMap{{QStringLiteral("path"), text(path)},
                    {QStringLiteral("url"), QUrl::fromLocalFile(text(path))},
                    {QStringLiteral("name"),
                     text(path.filename().empty() ? path : path.filename())},
                    {QStringLiteral("kind"), folder ? QStringLiteral("folder")
                                                    : QStringLiteral("file")},
                    {QStringLiteral("exists"), exists && !error}});
  }
  return result;
}

WorkspaceLocation WorkspaceHistory::restore() const {
  const auto stored = locations(m_settings);
  return stored.empty() ? WorkspaceLocation{} : stored.front();
}

fs::path WorkspaceHistory::forRoot(const fs::path& root) const {
  const fs::path wanted = normalized(root);
  if (wanted.empty()) return {};
  for (const WorkspaceLocation& location : locations(m_settings))
    if (location.root == wanted) return location.file;
  return {};
}

void WorkspaceHistory::clear() {
  m_settings.remove(historyKey);
  m_settings.sync();
}

}  // namespace sketchbook
