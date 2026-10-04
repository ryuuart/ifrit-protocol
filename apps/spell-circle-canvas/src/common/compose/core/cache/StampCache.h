#pragma once
/** @file
 * The instance-side bake store a stamped brush writes its tile bakes to,
 * handed to a decoration through its paint context.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>

#include <memory>
#include <vector>

namespace sigil::compose {

/** The INSTANCE-SIDE bake store for stamped brushes: tile bakes live with
 *  the NODE, not inside the brush value. A brush value constructed fresh
 *  by every describe would otherwise re-rasterize its art each time — the
 *  one place where re-describing costs raster work rather than a diff.
 *  Keeping the bake on the instance means the rebuilt value finds it.
 *
 *  Keyed on the art Element's node WITH A WEAK GUARD, and the guard is
 *  load-bearing: a plain map on the raw pointer would let a freed node's
 *  recycled address silently inherit the wrong art's bake. Locking the
 *  weak handle and comparing identity makes that impossible — a recycled
 *  key fails the check and re-bakes. Entries carry either a picture
 *  (pattern and scatter tiles) or a rastered image plus its logical size;
 *  each consumer reads only its own kind. */
class StampCache {
 public:
  /** One bake. A consumer stores either a recorded picture or a
   *  rastered image with the logical size it was baked at, and reads
   *  back only the kind it wrote. */
  struct Entry {
    sk_sp<SkPicture> picture;
    sk_sp<SkImage> image;
    SkSize artSize{0, 0};
  };
  /** The entry for `key`, or null — never a recycled address's entry. */
  const Entry* get(const std::shared_ptr<const void>& key) const {
    for (const Row& row : m_entries) {
      if (row.address != key.get()) continue;
      if (row.owner.lock() != key)
        return nullptr;  // the address was recycled: not this art's bake
      return &row.entry;
    }
    return nullptr;
  }
  void put(const std::shared_ptr<const void>& key, Entry entry) {
    // At this size a scan beats a hash, which is why the store is a list
    // and this header needs no map. A key already here is replaced in
    // place: re-baking one art must not cost the other bakes their
    // entries.
    for (Row& row : m_entries)
      if (row.address == key.get()) {
        row.owner = key;
        row.entry = std::move(entry);
        return;
      }
    // A node's stamp arts are few; a store that runs past its capacity
    // means keys churn every frame, and keeping stale bakes alive would
    // pin their nodes' memory. Only a new key can push it there.
    if (m_entries.size() >= kCapacity) m_entries.clear();
    m_entries.push_back({key.get(), key, std::move(entry)});
  }

 private:
  /** How many bakes one node may hold at once. */
  static constexpr size_t kCapacity = 16;
  struct Row {
    const void* address = nullptr;
    std::weak_ptr<const void> owner;
    Entry entry;
  };
  std::vector<Row> m_entries;
};

}  // namespace sigil::compose
