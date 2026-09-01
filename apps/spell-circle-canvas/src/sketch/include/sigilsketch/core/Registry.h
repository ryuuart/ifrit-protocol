#pragma once

/** @file
 * The registry: every sketch this binary was built with, and the macro a
 * sketch file joins it by.
 */

#include <sigilsketch/core/Kind.h>

#include <concepts>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::sketch {

/** ONE SKETCH, as the registry holds it.
 *
 *  `key` is the file's stem, which is how a sketch is addressed on a
 *  command line and how it is found on disk to hot-reload. `name` is
 *  what it is FILED under: the spelling its plate is written with and
 *  the spelling a byte-identity baseline holds, which is the file's stem
 *  unless the sketch declares otherwise. The two are separate because a
 *  plate's name is a promise to everything that already refers to it,
 *  while a file may be renamed by whoever owns it.
 *
 *  `kind` is a factory rather than a value: registration runs during
 *  static initialization, where an allocation that throws is fatal and
 *  no handler can exist, so the Kind is constructed at the moment a host
 *  actually opens the sketch. */
struct Entry {
  const char* key = "";
  const char* name = "";
  const char* category = "";
  const char* blurb = "";
  Kind (*kind)() = nullptr;

  /** WHETHER THE MACHINE RUNNING THIS BINARY CAN DRAW THE SKETCH, and
   *  what is missing when it cannot.
   *
   *  A sketch written over an optional SDK is compiled in wherever that
   *  SDK was found at build time — and the data the SDK needs at run
   *  time (a resource folder, a plugin registry, the sample archives a
   *  piece draws) can still be absent on the machine that runs the
   *  binary. Such a sketch is UNAVAILABLE rather than broken: a listing
   *  greys it with the reason, and a host that renders passes over it
   *  instead of failing or writing a picture of the failure.
   *
   *  Null for a sketch that needs nothing but this binary. */
  bool (*probe)(std::string* why) = nullptr;

  /** The probe's answer; no probe reads as available. @p why is written
   *  only when the answer is false. */
  [[nodiscard]] bool available(std::string* why = nullptr) const {
    return !probe || probe(why);
  }
};

/** The probe a sketch declares, read off its type: a sketch with a
 *  static `available(std::string* why)` answers for itself, and one
 *  without is available wherever it compiled.
 *
 *  Read off the type rather than passed to the macro so that the
 *  requirement is stated once, in the sketch, beside the code that needs
 *  it — and so a sketch that stops needing an SDK stops declaring one by
 *  deleting a member rather than by editing its registration. */
template <class SketchType>
[[nodiscard]] bool probeOf(std::string* why) {
  if constexpr (requires {
                  { SketchType::available(why) } -> std::same_as<bool>;
                })
    return SketchType::available(why);
  else
    return true;
}

/** Every sketch linked into this binary, ordered by category and then by
 *  name.
 *
 *  ORDERED, not in registration order: registrations run as static
 *  initializers and their sequence is link order, which nothing should
 *  depend on. A stable order is what lets a sketch be selected by index
 *  and a listing be read twice. */
[[nodiscard]] const std::vector<Entry>& registry();

/** Records one sketch. Returns whether it was recorded, so a namespace-
 *  scope initializer can be a bool either way; SIGIL_SKETCH is the only
 *  intended caller.
 *
 *  GENUINELY NON-THROWING, and that is the seam's contract rather than a
 *  hint: the call runs during static initialization, where an unwinding
 *  exception is fatal by definition. The one thing inside that could
 *  throw — the registry growing under memory exhaustion — is answered as
 *  false, because a process that cannot allocate a few pointers before
 *  main() is about to fail loudly anyway. This is what lets every
 *  sketch's registration be non-throwing with no per-file suppression. */
bool add(const char* key, const char* name, const char* category,
         const char* blurb, Kind (*kind)(),
         bool (*probe)(std::string* why) = nullptr) noexcept;

/** The registry index for a decimal index or a name; -1 when nothing
 *  matches. Names match case-insensitively on any unique substring, so
 *  `y2k` and `y2k chrome` land on the same entry, and a sketch answers
 *  to its file stem as well as to the name it is filed under — because
 *  `slitscan_2001` is what you have in front of you when you want to
 *  look at it. An exact match always beats a substring. */
[[nodiscard]] int find(std::string_view query);

/** The display spelling of a filed name: its underscores opened out into
 *  spaces. A sketch is filed under a name a script can pass on a command
 *  line without quoting; this is what a person reads. */
[[nodiscard]] std::string title(std::string_view name);

/** The ABI a sketch dylib must have been built against, bumped whenever
 *  anything crossing the host/dylib line changes shape. A host refuses a
 *  dylib built against another version rather than letting a stale
 *  binary corrupt it. */
inline constexpr unsigned kAbiVersion = 6;

}  // namespace sigil::sketch

#ifdef SIGIL_SKETCH_STATIC

/** Register a sketch under a name of its own.
 *
 *  Use it when the sketch is FILED under something other than its file
 *  stem — a plate name that other things already refer to. Everything
 *  else takes SIGIL_SKETCH below.
 *
 *  The whole expansion is NON-THROWING: the initializer is one call to
 *  the noexcept registration seam, handed the address of a factory — no
 *  lambda body, no construction, nothing that could need the handler
 *  static initialization cannot have. */
#define SIGIL_SKETCH_AS(SketchType, sketchName, sketchCategory, sketchBlurb)  \
  namespace {                                                                 \
  [[maybe_unused]] const bool sigilSketchRegistered =                         \
      ::sigil::sketch::add(SIGIL_SKETCH_STATIC, sketchName, sketchCategory,   \
                           sketchBlurb, &::sigil::sketch::kindOf<SketchType>, \
                           &::sigil::sketch::probeOf<SketchType>);            \
  }

#else

/** Export the sketch's entry points. Exactly one per sketch file. */
#define SIGIL_SKETCH_AS(SketchType, sketchName, sketchCategory, sketchBlurb) \
  extern "C" __attribute__((visibility("default")))                          \
  const ::sigil::sketch::Entry*                                              \
  sigilSketchEntry() {                                                       \
    static const ::sigil::sketch::Entry entry{                               \
        "",                                                                  \
        sketchName,                                                          \
        sketchCategory,                                                      \
        sketchBlurb,                                                         \
        &::sigil::sketch::kindOf<SketchType>,                                \
        &::sigil::sketch::probeOf<SketchType>};                              \
    return &entry;                                                           \
  }                                                                          \
  extern "C" __attribute__((visibility("default"))) unsigned                 \
  sigilSketchAbi() {                                                         \
    return ::sigil::sketch::kAbiVersion;                                     \
  }

#endif

/** Join the registry under the file's own stem. One per sketch file.
 *
 *  @p sketchCategory is the folder it files under — a path, not a flat
 *  label, so a registry this size stays navigable. @p sketchBlurb is one
 *  line on what the sketch is, shown beside it.
 *
 *  Which runtime the sketch draws through is read off the type: the
 *  header a sketch includes declares `kindOf` for the body it also
 *  declares, so a file that includes one of them cannot register for the
 *  other. */
#define SIGIL_SKETCH(SketchType, sketchCategory, sketchBlurb) \
  SIGIL_SKETCH_AS(SketchType, nullptr, sketchCategory, sketchBlurb)
