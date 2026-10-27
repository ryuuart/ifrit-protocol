#pragma once

/** @file
 * @ingroup material-advanced
 *
 * Program — a compiled recipe as a type-erased handle — and the one cache
 * that builds and keeps them, keyed by recipe identity, target and
 * variant, through the compilers renderers register per target.
 */

#include <sigilcore/report/Error.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/core/Target.h>

#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::material {

class Material;
namespace detail {
struct CompilerDefaults;
}  // namespace detail

/** A recipe compiled for one target and variant. The base carries what
 *  every backend shares; a backend's compiler returns a subclass holding
 *  its compiled object, which a renderer reaches through `as<T>()`. */
class Program {
 public:
  Program(std::shared_ptr<const Recipe> recipe, Target target, Variant variant)
      : m_recipe(std::move(recipe)), m_target(target), m_variant(variant) {}
  virtual ~Program() = default;

  const Recipe& recipe() const { return *m_recipe; }
  Target target() const { return m_target; }
  Variant variant() const { return m_variant; }
  /** Whether the compiled program still has the uniform @p name. A
   *  shading language whose compiler discards a uniform no body reads
   *  answers false for it, and nothing is uploaded for that field however
   *  the material sets it; a backend that keeps every declared uniform
   *  leaves this at yes. */
  virtual bool keeps(std::string_view name) const {
    (void)name;
    return true;
  }
  /** The backend's handle, or null when this program was compiled by a
   *  different backend than @p T expects. */
  template <class T>
  const T* as() const {
    return dynamic_cast<const T*>(this);
  }

 private:
  std::shared_ptr<const Recipe> m_recipe;
  Target m_target;
  Variant m_variant;
};

/** A per-target compiler: answers the program for a recipe and variant,
 *  or the error that says why it did not compile. Registered once per
 *  target by the backend that can run it. */
using Compiler =
    std::function<std::expected<std::shared_ptr<Program>, core::Error>(
        std::shared_ptr<const Recipe> recipe, Variant variant)>;

/** One program a loading phase wants resident before the first frame. */
struct WarmupRequest {
  std::shared_ptr<const Recipe> recipe;
  Target target;
  Variant variant{};
};

/** What a warm-up submitted after duplicate keys were folded together. */
struct WarmupResult {
  size_t requested = 0;
  size_t unique = 0;
  size_t ready = 0;
};

/** THE program cache: every compiled program in the process, keyed by
 *  (recipe identity, target, variant), so a recipe compiles once per key
 *  however many materials instantiate it. Thread-safe. A request it
 *  cannot meet returns null and reports once per (recipe, target), and a
 *  program that compiled is checked the same way for parameter fields no
 *  body reads.
 *  @trap A field the body never reads uploads nothing, so every value
 *  written to it is lost in silence. */
class ProgramCache {
 public:
  ProgramCache();
  ~ProgramCache();
  ProgramCache(const ProgramCache&) = delete;
  ProgramCache& operator=(const ProgramCache&) = delete;

  /** The one cache. */
  static ProgramCache& shared();

  void registerCompiler(Target target, Compiler compiler);
  bool hasCompiler(Target target) const;

  /** The program for @p recipe on @p target in @p variant, compiled on
   *  first use; or the error that stopped it — the compiler's message, or
   *  why no compiler was asked. A caller that shows a failure to a person
   *  reads it here rather than from the diagnostic stream, which says it
   *  once. */
  std::expected<std::shared_ptr<Program>, core::Error> program(
      std::shared_ptr<const Recipe> recipe, Target target,
      Variant variant = {});

  /** WHETHER THE PROGRAM CAN BE HAD WITHOUT WAITING: true when it is
   *  held, or when asking for it would not compile — no compiler, no body,
   *  a failure already reported — so a synchronous ask answers at once.
   *  Otherwise false, with the compile started on a worker of its own if
   *  none is under way; a later ask finds it held. What a frame that will
   *  not wait for a compile asks before it draws.
   *  @trap Only the shared cache keeps a worker's compile: one this asks
   *  of another cache must outlive the compile. */
  bool prepare(std::shared_ptr<const Recipe> recipe, Target target,
               Variant variant = {});
  /** How many compiles `prepare` started that have not finished. */
  size_t compiling() const;

  /** Compiles the distinct requests concurrently and populates this cache. */
  WarmupResult warmup(std::span<const WarmupRequest> requests);

  /** How many programs are held. */
  size_t size() const;
  /** EVERY PROGRAM HELD for @p target, in no stated order: what a host
   *  that writes down the programs a run compiled reads, a body's own
   *  shaders among them. A compile still under way is not held yet. */
  std::vector<std::shared_ptr<Program>> programs(Target target) const;
  /** Drops every program; compilers stay registered. The next request for
   *  a dropped key recompiles, and a failure that was reported once is
   *  reported again. */
  void clear();

 private:
  friend struct detail::CompilerDefaults;
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** `ProgramCache::shared().registerCompiler(...)`. */
void registerCompiler(Target target, Compiler compiler);
/** `ProgramCache::shared().program(...)`. */
std::expected<std::shared_ptr<Program>, core::Error> program(
    std::shared_ptr<const Recipe> recipe, Target target, Variant variant = {});
/** Compiles the distinct recipes instantiated by @p materials into the
 *  shared cache; `ProgramCache::warmup` takes the requests themselves. */
WarmupResult warmup(std::span<const Material> materials, Target target,
                    Variant variant = {});

/** Says @p message on behalf of SigilMaterial through
 *  `sigil::core::report`: to the reporter a host installed, or to the
 *  standard error as `[SigilMaterial] message` when none is. */
void report(std::string_view message);

/** Says @p message as `report` does the first time @p key is seen and
 *  never again, so a per-frame path can report a mistake once. */
void reportOnce(const std::string& key, const std::string& message);

}  // namespace sigil::material
