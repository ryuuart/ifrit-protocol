/** @file
 * The registry and the files beside it, read into the rows a browser
 * shows.
 */

#include "sigilsketch/core/Catalog.h"

#include <string>
#include <vector>

namespace sigil::sketch {

namespace {

namespace fs = std::filesystem;

/** How a group path's parts are joined, a middle dot between spaces. */
constexpr std::string_view kGroupSeparator = " \xc2\xb7 ";

/** Everything the file can say, filled into a row the caller has named. */
void describeFile(CatalogRow& row, const fs::path& file,
                  const CatalogSources& sources) {
  row.title = title(row.name);
  row.path = file;
  const fs::path& root = sources.workspaceRoot.empty() ? sources.sketchDirectory
                                                       : sources.workspaceRoot;
  const fs::path relative = file.lexically_relative(root);
  row.entryPath =
      !relative.empty() && *relative.begin() != ".." ? relative : file;
  if (file.extension() == ".cpp" || file.extension() == ".py")
    row.source = sourceMetadata(file);
}

/** The group a file opened by path stands under: the workspace, and the
 *  directories between its root and the file. */
std::string groupOf(const fs::path& file, const fs::path& workspaceRoot) {
  std::string group = "Workspace";
  if (workspaceRoot.empty()) return group;
  group += kGroupSeparator;
  group += workspaceRoot.filename().string();
  const fs::path relative =
      file.parent_path().lexically_relative(workspaceRoot);
  if (!relative.empty() && relative != "." && *relative.begin() != "..")
    for (const fs::path& part : relative) {
      group += kGroupSeparator;
      group += part.string();
    }
  return group;
}

}  // namespace

std::vector<CatalogRow> catalog(const CatalogSources& sources) {
  const std::vector<Entry>& entries = registry();
  std::vector<CatalogRow> rows;
  rows.reserve(entries.size() + sources.files.size());
  for (const Entry& entry : entries) {
    CatalogRow& row = rows.emplace_back();
    static_cast<RegistryRow&>(row) = registryRow(entry);
    row.index = (int)rows.size() - 1;
    describeFile(row, sourceOf(sources.sketchDirectory, entry.key), sources);
  }
  for (const fs::path& file : sources.files) {
    CatalogRow& row = rows.emplace_back();
    row.index = (int)rows.size() - 1;
    row.name = file.stem().string();
    row.key = row.name;
    row.category = groupOf(file, sources.workspaceRoot);
    row.blurb = file.parent_path().string();
    row.kind = file.extension() == ".py" ? "canvas" : "";
    row.external = true;
    row.videoExportable = false;
    describeFile(row, file, sources);
  }
  return rows;
}

}  // namespace sigil::sketch
