#pragma once

/** @file
 * @ingroup sketch-core
 *
 * What a browser knows about every sketch before one is opened, as rows:
 * the registry's entries and the files a session was pointed at, each
 * with what its own source says about it.
 */

#include <sigilsketch/core/Registry.h>
#include <sigilsketch/core/Sources.h>

#include <filesystem>
#include <string>
#include <vector>

namespace sigil::sketch {

/** Where a catalog's rows come from. */
struct CatalogSources {
  /** The directory a registry entry's file stands in, by its key. */
  std::filesystem::path sketchDirectory;
  /** Files opened by path, anywhere on disk, listed after the registry. */
  std::vector<std::filesystem::path> files;
  /** The folder those files were found in, when a workspace was opened: it
   *  groups them by the directories under it and roots their entry paths. */
  std::filesystem::path workspaceRoot;
};

/** ONE SKETCH AS A BROWSER SHOWS IT BEFORE IT IS OPENED: its registry row,
 *  where its file stands and what that file says at the top of itself.
 *
 *  A file opened by path is `external`: its key and name are its stem, its
 *  category is the workspace group it stands in and its blurb the
 *  directory holding it, since two drafts may share a stem and where they
 *  stand is what tells them apart. Such a file is compiled when it is
 *  opened, so a C++ one has NO kind until it has been built — an empty
 *  kind, not a guess from the folder it stands in — while a Python file
 *  always draws through a canvas.
 *
 *  What is not here is the canvas: a sketch declares its size, its ground
 *  and its moment from inside its own setup, which only a running session
 *  can answer. */
struct CatalogRow : RegistryRow {
  /** Where the row stands in the catalog: registry entries first, in
   *  registry order, then the files. */
  int index = 0;
  /** The display spelling of the name. */
  std::string title;
  /** The bare file, or the entry of a sketch that is a directory. */
  std::filesystem::path path;
  /** …relative to the workspace root, or to the sketch directory when no
   *  workspace is open, and the whole path when it stands outside it. */
  std::filesystem::path entryPath;
  bool external = false;
  /** Whether a montage can carry it: a registry entry can, a file opened
   *  by path is not in the registry a montage walks. */
  bool videoExportable = true;
  /** The lines, subject, first knobs and tags its source states. */
  SourceMetadata source;
};

/** EVERY ROW THE SOURCES NAME: every registry entry, then every file, in
 *  that order. */
[[nodiscard]] std::vector<CatalogRow> catalog(const CatalogSources& sources);

}  // namespace sigil::sketch
