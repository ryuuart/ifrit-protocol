/** @file URI selection over mounted directories and file URLs. */

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Fetch.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

namespace {

bool hasWildcard(std::string_view pattern) {
  bool quoted = false;
  for (char c : pattern) {
    if (quoted) {
      quoted = false;
      continue;
    }
    if (c == '\\') {
      quoted = true;
      continue;
    }
    if (c == '*' || c == '?') return true;
  }
  return false;
}

std::string unquote(std::string_view value) {
  std::string unquoted;
  unquoted.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i + 1 < value.size()) ++i;
    unquoted += value[i];
  }
  return unquoted;
}

/** Segment-aware glob matching. A run of two or more stars crosses URI path
 * separators; a globstar followed by a slash also matches zero complete
 * directory segments. */
bool matches(std::string_view pattern, std::string_view text) {
  const size_t columns = text.size() + 1;
  std::vector<int8_t> memo((pattern.size() + 1) * columns, -1);
  std::function<bool(size_t, size_t)> at = [&](size_t p, size_t t) -> bool {
    int8_t& remembered = memo[p * columns + t];
    if (remembered >= 0) return remembered != 0;

    bool result = false;
    if (p == pattern.size()) {
      result = t == text.size();
    } else if (pattern[p] == '\\' && p + 1 < pattern.size()) {
      result = t < text.size() && pattern[p + 1] == text[t] && at(p + 2, t + 1);
    } else if (pattern[p] == '?') {
      result = t < text.size() && text[t] != '/' && at(p + 1, t + 1);
    } else if (pattern[p] == '*') {
      size_t after = p;
      while (after < pattern.size() && pattern[after] == '*') ++after;
      if (after - p == 1) {
        result =
            at(after, t) || (t < text.size() && text[t] != '/' && at(p, t + 1));
      } else if (after < pattern.size() && pattern[after] == '/') {
        // With the slash, a globstar consumes whole segments or none. Trying
        // the remainder after each separator covers any number of segments.
        result = at(after + 1, t);
        for (size_t i = t; !result && i < text.size(); ++i)
          if (text[i] == '/') result = at(after + 1, i + 1);
      } else {
        result = at(after, t) || (t < text.size() && at(p, t + 1));
      }
    } else {
      result = t < text.size() && pattern[p] == text[t] && at(p + 1, t + 1);
    }
    remembered = result ? 1 : 0;
    return result;
  };
  return at(0, 0);
}

bool regularFile(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

bool directory(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_directory(path, error) && !error;
}

template <class Add>
void walk(const std::filesystem::path& root, Add&& add) {
  std::error_code error;
  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, error);
  const std::filesystem::recursive_directory_iterator end;
  for (; !error && it != end; it.increment(error)) {
    std::error_code typeError;
    if (it->is_regular_file(typeError) && !typeError) add(it->path());
  }
}

std::string directoryPrefix(std::string_view selector) {
  std::string prefix(selector);
  if (!prefix.empty() && !prefix.ends_with('/')) prefix += '/';
  return prefix;
}

void sortUnique(std::vector<std::string>& values) {
  std::ranges::sort(values);
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

std::filesystem::path globRoot(std::string_view pathPattern) {
  size_t meta = pathPattern.size();
  bool quoted = false;
  for (size_t i = 0; i < pathPattern.size(); ++i) {
    if (quoted) {
      quoted = false;
      continue;
    }
    if (pathPattern[i] == '\\') {
      quoted = true;
      continue;
    }
    if (pathPattern[i] == '*' || pathPattern[i] == '?') {
      meta = i;
      break;
    }
  }
  const size_t slash = pathPattern.substr(0, meta).rfind('/');
  if (slash == std::string_view::npos) return ".";
  if (slash == 0) return "/";
  return unquote(pathPattern.substr(0, slash));
}

/** The literal directory prefix before a selector's first wildcard. It ends
 * at a separator so a partial filename never becomes a traversal root. */
std::string globDirectoryPrefix(std::string_view pattern) {
  size_t meta = pattern.size();
  bool quoted = false;
  for (size_t i = 0; i < pattern.size(); ++i) {
    if (quoted) {
      quoted = false;
      continue;
    }
    if (pattern[i] == '\\') {
      quoted = true;
      continue;
    }
    if (pattern[i] == '*' || pattern[i] == '?') {
      meta = i;
      break;
    }
  }
  const size_t slash = pattern.substr(0, meta).rfind('/');
  if (slash == std::string_view::npos) return {};
  return unquote(pattern.substr(0, slash + 1));
}

bool safeRelative(std::string_view value) {
  const std::filesystem::path relative{std::string(value)};
  if (relative.has_root_path()) return false;
  for (const std::filesystem::path& component : relative)
    if (component == "..") return false;
  return true;
}

struct MountedWalk {
  std::filesystem::path root;
  std::string uriPrefix;
};

/** Narrows a mounted traversal to the overlap between the mount's namespace
 * and a known literal directory prefix. */
std::optional<MountedWalk> mountedWalk(std::string_view mountPrefix,
                                       const std::filesystem::path& mountRoot,
                                       std::string_view literalDirectory) {
  if (literalDirectory.empty())
    return MountedWalk{mountRoot, std::string(mountPrefix)};
  if (literalDirectory.starts_with(mountPrefix)) {
    const std::string_view relative =
        literalDirectory.substr(mountPrefix.size());
    if (!safeRelative(relative)) return std::nullopt;
    return MountedWalk{mountRoot / std::string(relative),
                       std::string(literalDirectory)};
  }
  if (mountPrefix.starts_with(literalDirectory))
    return MountedWalk{mountRoot, std::string(mountPrefix)};
  return std::nullopt;
}

/** Plain paths and file URLs have no mounted namespace to reconstruct, so
 * candidates are formed from their filesystem names directly. */
std::vector<std::string> selectFilesystem(std::string_view selector,
                                          bool fileUrl) {
  const std::string_view pathSelector = fileUrl ? selector.substr(7) : selector;
  const bool glob = hasWildcard(pathSelector);
  const std::string literalSelector =
      glob ? std::string(selector) : unquote(selector);
  const std::filesystem::path selectedPath =
      fileUrl ? literalSelector.substr(7) : literalSelector;
  if (!glob && regularFile(selectedPath)) return {literalSelector};

  std::vector<std::string> found;
  if (!glob) {
    if (!directory(selectedPath)) return found;
    const std::string base = directoryPrefix(literalSelector);
    walk(selectedPath, [&](const std::filesystem::path& path) {
      const std::filesystem::path relative =
          path.lexically_relative(selectedPath);
      if (!relative.empty()) found.push_back(base + relative.generic_string());
    });
  } else {
    const std::filesystem::path root = globRoot(pathSelector);
    if (!directory(root)) return found;
    const bool absolute = selectedPath.is_absolute();
    walk(root, [&](const std::filesystem::path& path) {
      std::string candidate;
      if (fileUrl) {
        candidate = "file://" + path.lexically_normal().generic_string();
      } else if (absolute) {
        candidate = path.lexically_normal().generic_string();
      } else {
        std::error_code error;
        const std::filesystem::path absolutePath =
            std::filesystem::absolute(path, error).lexically_normal();
        if (error) return;
        const std::filesystem::path relative = absolutePath.lexically_relative(
            std::filesystem::current_path(error).lexically_normal());
        if (error || relative.empty()) return;
        candidate = relative.generic_string();
      }
      if (matches(selector, candidate)) found.push_back(std::move(candidate));
    });
  }
  sortUnique(found);
  return found;
}

}  // namespace

std::vector<std::string> Hub::select(std::string_view selector) const {
  if (selector.empty() || detail::isNetworkUri(selector)) return {};
  if (selector.starts_with("file://")) return selectFilesystem(selector, true);

  const bool glob = hasWildcard(selector);
  const std::string literalSelector =
      glob ? std::string(selector) : unquote(selector);
  const bool uri = selector.find("://") != std::string_view::npos;
  if (!uri) return selectFilesystem(selector, false);

  if (!glob) {
    const std::filesystem::path exact = resolve(literalSelector);
    if (regularFile(exact)) return {literalSelector};
  }

  const std::string beneath =
      glob ? globDirectoryPrefix(selector) : directoryPrefix(literalSelector);
  std::vector<std::string> found;
  for (const auto& [prefix, root] : m_mounts) {
    const std::optional<MountedWalk> traversal =
        mountedWalk(prefix, root, beneath);
    if (!traversal || !directory(traversal->root)) continue;
    walk(traversal->root, [&](const std::filesystem::path& path) {
      const std::filesystem::path relative =
          path.lexically_relative(traversal->root);
      if (relative.empty()) return;
      std::string candidate = traversal->uriPrefix + relative.generic_string();
      if (glob && !matches(selector, candidate)) return;

      // A more-specific mount hides the broad mount's file at this URI. Only
      // expose candidates contributed by the prefix an ordinary fetch wins.
      for (const auto& mount : m_mounts) {
        const std::string& otherPrefix = mount.first;
        if (otherPrefix.size() > prefix.size() &&
            candidate.starts_with(otherPrefix))
          return;
      }
      found.push_back(std::move(candidate));
    });
  }
  sortUnique(found);
  return found;
}

size_t Hub::preload(std::string_view selector) {
  const std::vector<std::string> selected = select(selector);
  std::vector<std::string_view> uris;
  uris.reserve(selected.size());
  for (const std::string& uri : selected) uris.push_back(uri);
  return preload(uris);
}

size_t Hub::preloadDirectory(std::string_view uriPrefix) {
  return preload(uriPrefix);
}

}  // namespace sigil::io
