#pragma once

/** @file
 * Program — a compiled recipe as a type-erased handle — and the one cache
 * that builds and keeps them, keyed by recipe identity, target and
 * variant, through the compilers renderers register per target.
 */

#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/Target.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace sigil::material {

class Material;

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

/** A per-target compiler: hands back the program for a recipe and
 *  variant, or null with @p error filled. Registered once per target by
 *  the backend that can run it. */
using Compiler = std::function<std::shared_ptr<Program>(
    std::shared_ptr<const Recipe> recipe, Variant variant, std::string& error)>;

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
 *  however many materials instantiate it. Thread-safe.
 *
 *  A request that cannot be met — no compiler registered for the target,
 *  no body in the recipe for it, or a body that fails to compile — returns
 *  null and reports once per (recipe, target), naming both, so the error
 *  surfaces at the first describe and does not scroll past every frame.
 *
 *  A program that COMPILED is checked the same way and reported the same
 *  once: a params field the body never reads is discarded by the shader
 *  compiler and uploads nothing, so the field is dead weight in the ABI
 *  and every value written to it is lost in silence. The check names the
 *  recipe and each unread field. */
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
   *  first use. */
  std::shared_ptr<Program> program(std::shared_ptr<const Recipe> recipe,
                                   Target target, Variant variant = {});

  /** Compiles the distinct requests concurrently and populates this cache. */
  WarmupResult warmup(std::span<const WarmupRequest> requests);

  /** How many programs are held. */
  size_t size() const;
  /** Drops every program; compilers stay registered. The next request for
   *  a dropped key recompiles, and a failure that was reported once is
   *  reported again. */
  void clear();

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** `ProgramCache::shared().registerCompiler(...)`. */
void registerCompiler(Target target, Compiler compiler);
/** `ProgramCache::shared().program(...)`. */
std::shared_ptr<Program> program(std::shared_ptr<const Recipe> recipe,
                                 Target target, Variant variant = {});
/** `ProgramCache::shared().warmup(requests)`. */
WarmupResult warmup(std::span<const WarmupRequest> requests);
/** Compiles the distinct recipes instantiated by @p materials. */
WarmupResult warmup(std::span<const Material> materials, Target target,
                    Variant variant = {});

/** Writes @p message to the diagnostic stream the first time @p key is
 *  seen and never again, so a per-frame path can report a mistake once. */
void reportOnce(const std::string& key, const std::string& message);

}  // namespace sigil::material
