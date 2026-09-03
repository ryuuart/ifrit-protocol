/** @file
 * The program cache: compiler registry, compile-on-first-use, and the
 * once-per-key reporting of what could not be built.
 */

#include "sigilmaterial/core/Program.h"

#include <cstdio>
#include <unordered_set>

namespace sigil::material {

void reportOnce(const std::string& key, const std::string& message) {
  static std::mutex mutex;
  static std::unordered_set<std::string> seen;
  std::lock_guard lock(mutex);
  if (!seen.insert(key).second) return;
  std::fprintf(stderr, "[sigil::material] %s\n", message.c_str());
}

ProgramCache& ProgramCache::shared() {
  static ProgramCache cache;
  return cache;
}

void ProgramCache::registerCompiler(Target target, Compiler compiler) {
  std::lock_guard lock(m_mutex);
  m_compilers[target] = std::move(compiler);
}

bool ProgramCache::hasCompiler(Target target) const {
  std::lock_guard lock(m_mutex);
  return m_compilers.contains(target);
}

std::shared_ptr<Program> ProgramCache::program(
    std::shared_ptr<const Recipe> recipe, Target target, Variant variant) {
  if (!recipe) return nullptr;
  const Key key{recipe.get(), target, variant};
  {
    std::lock_guard lock(m_mutex);
    auto it = m_programs.find(key);
    if (it != m_programs.end()) return it->second;
  }
  // A failure is reported once per (recipe, target) — the variant does
  // not change whether a body exists or compiles, and a renderer asking
  // for several variants would otherwise say the same thing several times.
  const auto report = [&](const std::string& what) {
    std::lock_guard lock(m_mutex);
    if (!m_reported.insert({recipe.get(), target}).second) return;
    std::fprintf(stderr, "[sigil::material] recipe \"%s\": %s\n",
                 recipe->name().c_str(), what.c_str());
  };
  Compiler compiler;
  {
    std::lock_guard lock(m_mutex);
    auto it = m_compilers.find(target);
    if (it != m_compilers.end()) compiler = it->second;
  }
  if (!compiler) {
    report("no compiler is registered for " + std::string(name(target)));
    return nullptr;
  }
  if (!recipe->has(target)) {
    report("no " + std::string(name(target)) + " body");
    return nullptr;
  }
  std::string error;
  std::shared_ptr<Program> built = compiler(recipe, variant, error);
  if (!built) {
    report(std::string(name(target)) + " failed to compile: " + error);
    return nullptr;
  }
  // A field the compiled body never reads: whatever the material writes
  // there has no effect on the picture, which reads at a call site as a
  // dial that does nothing. Named once per (recipe, target), like every
  // other thing this cache has to say about a definition.
  std::string unread;
  for (const Field& f : recipe->params().fields) {
    if (built->keeps(f.name)) continue;
    if (!unread.empty()) unread += ", ";
    unread += f.name;
  }
  if (!unread.empty()) {
    bool first = false;
    {
      std::lock_guard lock(m_mutex);
      first = m_unread.insert({recipe.get(), target}).second;
    }
    if (first) {
      const std::string what =
          "the " + std::string(name(target)) + " body never reads " + unread +
          " — whatever is written to those fields has no effect";
      std::fprintf(stderr, "[sigil::material] recipe \"%s\": %s\n",
                   recipe->name().c_str(), what.c_str());
    }
  }
  std::lock_guard lock(m_mutex);
  auto [it, inserted] = m_programs.try_emplace(key, built);
  return it->second;
}

size_t ProgramCache::size() const {
  std::lock_guard lock(m_mutex);
  return m_programs.size();
}

void ProgramCache::clear() {
  std::lock_guard lock(m_mutex);
  m_programs.clear();
  m_reported.clear();
  m_unread.clear();
}

void registerCompiler(Target target, Compiler compiler) {
  ProgramCache::shared().registerCompiler(target, std::move(compiler));
}

std::shared_ptr<Program> program(std::shared_ptr<const Recipe> recipe,
                                 Target target, Variant variant) {
  return ProgramCache::shared().program(std::move(recipe), target, variant);
}

}  // namespace sigil::material
