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

/** WHERE GRIMOIRE KEEPS THE KEYS IT HAS RECORDED: `pipelines` under
 *  the process's state root when the run named one, otherwise the
 *  platform cache location, under this app's own name and beside the
 *  thumbnails. The store is the app's alone: a window run replays each
 *  sketch's set before the sketch opens and writes back what its draws
 *  wanted, and a headless sweep on the device writes a sketch's set only
 *  where none stands, so a run that drew a whole selection can seed a
 *  machine but never displace what a window learned. Losing it costs
 *  one open's worth of compiling per sketch. */
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

/** WHAT NAMES ONE SKETCH'S SET under a recorded declaration: @p set, the
 *  name `keySetName` gives the declaration and backend, followed by a
 *  digest of @p sketch — the sketch as the host names it before opening
 *  it, a registry key or a file's path. Each sketch keeps the programs
 *  its own draws wanted, so standing one sketch's set up builds nothing
 *  another sketch needed and the set is never cut to make room. */
std::string sketchKeySetName(std::string_view set, std::string_view sketch);

/** WHETHER ANY SKETCH'S SET is written down under @p set in @p
 *  directory: a store that holds none is one a launch has nothing to
 *  replay from. */
bool holdsAnySketchSet(const std::filesystem::path& directory,
                       std::string_view set);

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

/** WHAT A RUNTIME EFFECT IS FOR, which says how its text is compiled
 *  again: a shader draws, a colour filter maps what is beneath it, and a
 *  blender mixes two colours. */
enum class EffectKind : std::uint8_t {
  Shader = 0,
  ColorFilter = 1,
  Blender = 2
};

/** ONE RUNTIME EFFECT A RUN COMPILED that no declaration named: its kind
 *  and the whole SkSL it was compiled from.
 *
 *  A body's own shader is compiled while the body sets up, after every
 *  device context in the process was made, and a context can only carry
 *  a key for a program over an effect it was told about when it was
 *  made. So such an effect is written down when a run ends and declared
 *  by the next launch before its contexts exist; compiled from the same
 *  text it is the same effect to the backend, and the body's own
 *  compile of it then carries the declared name. */
struct RecordedEffect {
  EffectKind kind = EffectKind::Shader;
  std::string source;
  /** The sketches whose draws went without a key while it was undeclared,
   *  by the names they were drawn under: the ones that stay unkeyed while
   *  it cannot be declared. */
  std::vector<std::string> sketches;
  bool operator==(const RecordedEffect&) const = default;
};

/** THE FILE THE RECORDED EFFECTS STAND IN, under @p directory. It is
 *  one file for the store, never cut or reordered: each effect in it is
 *  declared after the stock ones in the order it was written down, and a
 *  recorded key names it by that place. */
std::filesystem::path effectsFile(const std::filesystem::path& directory);

/** The effects in @p file, in the order they were written; a file that
 *  is missing or not one of these comes back empty, and a truncated one
 *  with the effects that were whole. */
std::vector<RecordedEffect> readEffects(const std::filesystem::path& file);

/** Writes @p effects to @p file whole, beside and renamed over as a key
 *  set is. Returns whether the file landed. */
bool writeEffects(const std::filesystem::path& file,
                  std::span<const RecordedEffect> effects);

/** THE SKETCHES A DECLARATION LEAVES UNKEYED: the backend declares at most
 *  @p room of @p recorded after the stock effects, in the order they were
 *  written down, and every sketch an effect past that room names keeps
 *  programs no key can carry, which build again on every launch. Each
 *  sketch once, in order of name. */
std::vector<std::string> sketchesLeftUnkeyed(
    std::span<const RecordedEffect> recorded, size_t room);

}  // namespace pipelines
