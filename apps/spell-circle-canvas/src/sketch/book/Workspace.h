#pragma once

/** @file
 * Source discovery and remembered locations for a Sketchbook window. A folder
 * owns a fixed list of entry files for the life of that window.
 */

#include <QtCore/QVariantList>
#include <filesystem>
#include <vector>

class QSettings;

namespace sketchbook {

/** A nonempty root denotes a folder workspace; file is its last selection.
 *  With no root, file denotes a standalone sketch. Empty paths remain empty. */
struct WorkspaceLocation {
  std::filesystem::path root;
  std::filesystem::path file;
};

/** Finds C++ and Python entry files without executing source. Entries declare
 *  SIGIL_SKETCH or a sketch decorator, or share their enclosing folder's stem.
 *  Results are unique canonical paths in lexical order. Child symlinks, hidden
 *  folders, build/dependency trees and nested Python projects are skipped.
 *  A nested pyproject.toml or .venv/pyvenv.cfg marks a separate project.
 *  The scan is bounded to 12 directory levels, 20,000 entries, 32 MiB of source
 *  and 1 MiB per source file;
 *  a missing or unreadable root produces an empty list. */
std::vector<std::filesystem::path> workspaceFiles(
    const std::filesystem::path& root);

/** The supplied settings outlive this object. Writes preserve unrelated keys
 *  and are flushed before returning. At most 12 locations are retained. */
class WorkspaceHistory {
 public:
  explicit WorkspaceHistory(QSettings& settings);

  /** Moves a location to the front. A folder remembered without a file keeps
   *  its previous selection. Paths are canonical where they exist and absolute
   *  and lexical where they do not. An empty location has no effect. */
  void remember(const WorkspaceLocation& location);

  /** Maps contain path, url, name, kind (folder or file), and exists. Missing
   *  locations remain in the list; existence is checked each time it is read.
   */
  QVariantList recent() const;
  WorkspaceLocation restore() const;
  std::filesystem::path forRoot(const std::filesystem::path& root) const;
  void clear();

 private:
  QSettings& m_settings;
};

}  // namespace sketchbook
