/** @file
 * The program cache: compiler registry, compile-on-first-use, and the
 * once-per-key reporting of what could not be built.
 */

#include "sigilmaterial/core/Program.h"

#include <sigilcore/schedule/Parallel.h>

#include <atomic>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <cstdio>
#include <exception>
#include <future>
#include <mutex>
#include <vector>

#include "sigilmaterial/core/Material.h"

namespace sigil::material {

struct ProgramCache::Impl {
  struct Key {
    const Recipe* recipe;
    Target target;
    Variant variant;
    auto operator<=>(const Key&) const = default;
  };
  struct InFlight {
    size_t generation;
    std::shared_future<std::shared_ptr<Program>> result;
  };

  mutable std::mutex mutex;
  boost::container::flat_map<Target, Compiler> compilers;
  boost::container::flat_map<Key, std::shared_ptr<Program>> programs;
  boost::container::flat_map<Key, InFlight> inFlight;
  boost::container::flat_set<std::pair<const Recipe*, Target>> reported;
  // Kept apart from reported so that a variant which compiled and one
  // which did not each still say their piece once.
  boost::container::flat_set<std::pair<const Recipe*, Target>> unread;
  size_t generation = 0;
};

ProgramCache::ProgramCache() : m_impl(std::make_unique<Impl>()) {}
ProgramCache::~ProgramCache() = default;

void reportOnce(const std::string& key, const std::string& message) {
  static std::mutex mutex;
  static boost::unordered_flat_set<std::string> seen;
  std::lock_guard lock(mutex);
  if (!seen.insert(key).second) return;
  std::fprintf(stderr, "[sigil::material] %s\n", message.c_str());
}

ProgramCache& ProgramCache::shared() {
  static ProgramCache cache;
  return cache;
}

void ProgramCache::registerCompiler(Target target, Compiler compiler) {
  std::lock_guard lock(m_impl->mutex);
  m_impl->compilers[target] = std::move(compiler);
}

bool ProgramCache::hasCompiler(Target target) const {
  std::lock_guard lock(m_impl->mutex);
  return m_impl->compilers.contains(target);
}

std::shared_ptr<Program> ProgramCache::program(
    std::shared_ptr<const Recipe> recipe, Target target, Variant variant) {
  if (!recipe) return nullptr;
  const Impl::Key key{recipe.get(), target, variant};
  std::shared_future<std::shared_ptr<Program>> waiting;
  std::shared_ptr<std::promise<std::shared_ptr<Program>>> promise;
  size_t generation = 0;
  {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->programs.find(key);
    if (it != m_impl->programs.end()) return it->second;
    auto flight = m_impl->inFlight.find(key);
    if (flight != m_impl->inFlight.end()) {
      waiting = flight->second.result;
    } else {
      promise = std::make_shared<std::promise<std::shared_ptr<Program>>>();
      waiting = promise->get_future().share();
      generation = m_impl->generation;
      m_impl->inFlight.emplace(key, Impl::InFlight{generation, waiting});
    }
  }
  if (!promise) return waiting.get();

  const auto finish = [&](std::shared_ptr<Program> built) {
    std::shared_ptr<Program> result = built;
    {
      std::lock_guard lock(m_impl->mutex);
      const auto flight = m_impl->inFlight.find(key);
      if (flight != m_impl->inFlight.end() &&
          flight->second.generation == generation)
        m_impl->inFlight.erase(flight);
      if (built && generation == m_impl->generation) {
        auto [it, inserted] =
            m_impl->programs.try_emplace(key, std::move(built));
        result = it->second;
      }
    }
    promise->set_value(result);
    return result;
  };
  // A failure is reported once per (recipe, target) — the variant does
  // not change whether a body exists or compiles, and a renderer asking
  // for several variants would otherwise say the same thing several times.
  const auto report = [&](const std::string& what) {
    std::lock_guard lock(m_impl->mutex);
    if (!m_impl->reported.insert({recipe.get(), target}).second) return;
    std::fprintf(stderr, "[sigil::material] recipe \"%s\": %s\n",
                 recipe->name().c_str(), what.c_str());
  };
  Compiler compiler;
  {
    std::lock_guard lock(m_impl->mutex);
    auto it = m_impl->compilers.find(target);
    if (it != m_impl->compilers.end()) compiler = it->second;
  }
  if (!compiler) {
    report("no compiler is registered for " + std::string(name(target)));
    return finish(nullptr);
  }
  if (!recipe->has(target)) {
    report("no " + std::string(name(target)) + " body");
    return finish(nullptr);
  }
  std::string error;
  std::shared_ptr<Program> built;
  try {
    built = compiler(recipe, variant, error);
  } catch (const std::exception& exception) {
    error = exception.what();
  } catch (...) {
    error = "the compiler raised an unknown exception";
  }
  if (!built) {
    report(std::string(name(target)) + " failed to compile: " + error);
    return finish(nullptr);
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
      std::lock_guard lock(m_impl->mutex);
      first = m_impl->unread.insert({recipe.get(), target}).second;
    }
    if (first) {
      const std::string what =
          "the " + std::string(name(target)) + " body never reads " + unread +
          " — whatever is written to those fields has no effect";
      std::fprintf(stderr, "[sigil::material] recipe \"%s\": %s\n",
                   recipe->name().c_str(), what.c_str());
    }
  }
  return finish(std::move(built));
}

WarmupResult ProgramCache::warmup(std::span<const WarmupRequest> requests) {
  boost::container::flat_map<Impl::Key, WarmupRequest> unique;
  for (const WarmupRequest& request : requests) {
    if (!request.recipe) continue;
    unique.try_emplace(
        Impl::Key{request.recipe.get(), request.target, request.variant},
        request);
  }
  std::vector<WarmupRequest> work;
  work.reserve(unique.size());
  for (const auto& [key, request] : unique) work.push_back(request);

  std::atomic_size_t ready = 0;
  // One key is one whole program compiled, so a worker takes one at a
  // time: nothing about a second key makes the first one cheaper, and a
  // warm-up of two keys is worth dividing.
  sigil::core::schedule::parallelForEach(
      work, 1, [&](const WarmupRequest& request) {
        if (program(request.recipe, request.target, request.variant)) ++ready;
      });
  return {.requested = requests.size(),
          .unique = work.size(),
          .ready = ready.load()};
}

size_t ProgramCache::size() const {
  std::lock_guard lock(m_impl->mutex);
  return m_impl->programs.size();
}

void ProgramCache::clear() {
  std::lock_guard lock(m_impl->mutex);
  ++m_impl->generation;
  m_impl->programs.clear();
  m_impl->inFlight.clear();
  m_impl->reported.clear();
  m_impl->unread.clear();
}

void registerCompiler(Target target, Compiler compiler) {
  ProgramCache::shared().registerCompiler(target, std::move(compiler));
}

std::shared_ptr<Program> program(std::shared_ptr<const Recipe> recipe,
                                 Target target, Variant variant) {
  return ProgramCache::shared().program(std::move(recipe), target, variant);
}

WarmupResult warmup(std::span<const WarmupRequest> requests) {
  return ProgramCache::shared().warmup(requests);
}

WarmupResult warmup(std::span<const Material> materials, Target target,
                    Variant variant) {
  std::vector<WarmupRequest> requests;
  requests.reserve(materials.size());
  for (const Material& material : materials)
    requests.push_back({material.recipePtr(), target, variant});
  return warmup(requests);
}

}  // namespace sigil::material
