#pragma once

/** @file
 * Where a sketch stands on disk: the file a registry key names, and the
 * translation units an entry file carries with it.
 */

#include <filesystem>
#include <string_view>
#include <vector>

namespace sigil::sketch {

/** THE FILE A KEY NAMES under @p dir.
 *
 *  A sketch is either the file `<key>.cpp` or the directory `<key>/`
 *  with `<key>.cpp` inside it as the entry, and the directory form is
 *  the answer when it stands there. Nothing has to exist: the bare file
 *  is the answer when no directory sketch of that key does, whether or
 *  not the file itself is there. */
[[nodiscard]] std::filesystem::path sourceOf(const std::filesystem::path& dir,
                                             std::string_view key);

/** Whether @p entry is the entry of a DIRECTORY SKETCH: a source standing
 *  in a directory that carries its own stem. `rain/rain.cpp` is one;
 *  `rain.cpp` beside other sketches is not, and neither is
 *  `rain/tables.cpp`, which is a unit of the first. */
[[nodiscard]] bool directorySketch(const std::filesystem::path& entry);

/** Every `.cpp` directly under @p dir, in name order; nothing for a
 *  directory that is not there. */
[[nodiscard]] std::vector<std::filesystem::path> sourcesUnder(
    const std::filesystem::path& dir);

/** EVERY TRANSLATION UNIT A SKETCH IS BUILT FROM: the entry alone for a
 *  bare sketch, and for a directory sketch the entry first and then
 *  every other `.cpp` in its directory in name order. The entry first
 *  because a compiler's output is read from the top, and the entry is
 *  the file being edited. */
[[nodiscard]] std::vector<std::filesystem::path> unitsOf(
    const std::filesystem::path& entry);

}  // namespace sigil::sketch
