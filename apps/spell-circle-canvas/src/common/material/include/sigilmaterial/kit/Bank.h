#pragma once

/** @file
 * A bounded, seeded BANK of material instances: the N materials a paving
 * of a thousand pieces shares, keyed by the recipe, its parameters and
 * each piece's seed folded into one of N buckets.
 *
 * A field of setts, boards or tesserae wants every piece to differ and
 * cannot afford a material per piece — a material is a program and a
 * resolve, and a thousand of them is a thousand shaders. Folding the
 * seed to `seed % N` bounds the count at N per (recipe, parameters): more
 * variety than a field of two prototiles at ten orientations can show,
 * and a fixed cost whatever the field's size. Because the instance is
 * held here rather than re-minted per describe, its identity is stable,
 * which is what lets a consumer that compares materials prune.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Params.h>
#include <sigilmaterial/core/Recipe.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::material::kit {

/** THE BANK. `get` answers the instance for a (recipe, params, bucket)
 *  triple, building it once through the maker it is handed; the bucket
 *  is `seed % buckets()`. The params' BYTES are their identity — a params
 *  struct is packed floats, which `schema<P>()` proves — so two pieces of
 *  one species at one seed bucket are one material, and a species with a
 *  different tone or scale is another. */
class Bank {
 public:
  explicit Bank(uint32_t buckets = 24) : m_buckets(buckets ? buckets : 1) {}

  uint32_t buckets() const { return m_buckets; }
  /** The bucket @p seed falls in. */
  uint32_t bucket(uint32_t seed) const { return seed % m_buckets; }

  /** The instance for (@p recipe, @p params, the bucket of @p seed),
   *  built by @p make(bucket) the first time that triple is asked for and
   *  answered from the bank thereafter. The maker is where the bucket
   *  becomes what varies the piece — a seed the recipe reads, a jitter on
   *  a tone, a grain's own seed — and it returns the material whole, so a
   *  blend of several recipes is banked exactly as one recipe is. */
  template <class P, std::invocable<uint32_t> Make>
    requires std::convertible_to<std::invoke_result_t<Make, uint32_t>,
                                 Material>
  const Material& get(const std::shared_ptr<const Recipe>& recipe,
                      const P& params, uint32_t seed, Make&& make) {
    (void)schema<P>();  // packed floats, so the bytes are the identity
    const uint32_t b = bucket(seed);
    Key key{recipe.get(), bytesOf(params), b};
    auto it = m_bank.find(key);
    if (it == m_bank.end())
      it = m_bank.emplace(std::move(key), Material(make(b))).first;
    return it->second;
  }

  /** The SEEDED form, for a params struct carrying a `seed` field: the
   *  bucket is written into it and @p recipe instantiated over the result.
   *  Whatever seed the caller left in @p params is ignored — the bucket is
   *  the seed, so pieces in one bucket are one material and a caller's
   *  own seed cannot make the bank unbounded. */
  template <class P>
    requires requires(P& p) {
      { p.seed } -> std::convertible_to<float>;
    }
  const Material& get(const std::shared_ptr<const Recipe>& recipe, P params,
                      uint32_t seed) {
    params.seed = 0.0f;
    return get(recipe, params, seed, [&](uint32_t b) {
      P seeded = params;
      seeded.seed = (float)b;
      return Material(recipe, seeded);
    });
  }

  /** How many instances the bank holds. */
  size_t size() const { return m_bank.size(); }
  void clear() { m_bank.clear(); }

 private:
  struct Key {
    const Recipe* recipe = nullptr;
    std::vector<std::byte> params;
    uint32_t bucket = 0;
    auto operator<=>(const Key&) const = default;
  };
  template <class P>
  static std::vector<std::byte> bytesOf(const P& params) {
    const auto* p = reinterpret_cast<const std::byte*>(&params);
    return std::vector<std::byte>(p, p + sizeof(P));
  }

  uint32_t m_buckets;
  std::map<Key, Material> m_bank;
};

}  // namespace sigil::material::kit
