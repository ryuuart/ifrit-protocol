#pragma once

/** @file
 * The device programs this machine has already built, written down so
 * the next launch does not build them again: where the file stands,
 * what names one set of keys apart from another, and the reader and
 * writer of one.
 */

#include <include/core/SkRefCnt.h>

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class SkData;
class SkRuntimeEffect;

/** WHAT A PIPELINE COST LAST TIME, KEPT. A backend builds one device
 *  program per distinct draw and the thread that recorded the draw
 *  waits for it, and it hands out a key each program can be rebuilt
 *  from. Recorded while a run draws and read back at the next launch,
 *  those keys are how a program is built before anything waits on it.
 *  Nothing here talks to a device: these are bytes on a disk. */
namespace pipelines {

/** WHERE SKETCHBOOK KEEPS THE KEYS IT HAS RECORDED. An environment
 *  variable names one for a test; otherwise the platform cache
 *  location, under this app's own name and beside the thumbnails. The
 *  store is the app's alone: a window run replays it and writes back
 *  what its own draws wanted, and a headless sweep on the device writes
 *  into it only where it stands empty, so a run that drew a whole
 *  selection can seed a machine but never displace what a window
 *  learned. Losing it costs one launch's worth of compiling. */
std::filesystem::path storeDirectory();

/** THE FILE A SET RECORDED AGAINST @p name STANDS IN, under @p
 *  directory. */
std::filesystem::path keySetFile(const std::filesystem::path& directory,
                                 std::string_view name);

/** WHAT NAMES ONE RECORDED SET APART FROM ANOTHER: a digest of the SkSL
 *  of every declared runtime effect, in the order they were declared,
 *  and @p backend, the name of the backend the programs were built for.
 *
 *  A key names an effect by its place in that list, so a set recorded
 *  against a different list — one effect added, removed or moved, or
 *  one body edited — describes different programs, and replaying it
 *  would build the wrong ones under the right names. Keying the file on
 *  the list is how such a set is thrown away instead of believed. */
std::string keySetName(std::span<const sk_sp<SkRuntimeEffect>> effects,
                       std::string_view backend);

/** ONE RECORDED PROGRAM: what rebuilds it, and what it was called when
 *  it was written down.
 *
 *  The description is kept because it is the only thing that says a key
 *  still MEANS what it meant. A key names the pieces a program is
 *  inlined out of by number, and a piece the reading run cannot put a
 *  name to reads back as a hole — a description that has lost a name is
 *  a key that would build the wrong program, or none. Reading the
 *  description back and comparing is how such a key is dropped. */
struct RecordedPipeline {
  sk_sp<SkData> key;
  std::string description;
};

/** The programs in @p file, in the order they were written.
 *
 *  A file that is missing, truncated, or not one of these at all comes
 *  back EMPTY rather than as an error: a recorded set is an
 *  optimisation, and all a lost one costs is the compiling it would
 *  have saved. */
std::vector<RecordedPipeline> readKeySet(const std::filesystem::path& file);

/** Writes @p recorded to @p file, making the directory it stands in.
 *
 *  Written beside and renamed over, so a run that dies partway through
 *  leaves the last whole set rather than half of a new one. An entry
 *  with no key is dropped: a backend hands one back for a program it
 *  cannot serialise, and there is nothing to write down. Returns
 *  whether the set landed. */
bool writeKeySet(const std::filesystem::path& file,
                 std::span<const RecordedPipeline> recorded);

}  // namespace pipelines
