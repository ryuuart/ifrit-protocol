#pragma once

/** @file
 * @ingroup io-hub
 * WHAT A HUB KEEPS IN MEMORY: leases that hold a set of resources
 * resident, reading them ahead of the first ask, and letting go of
 * everything no lease holds.
 */

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::io {

class Hub;

namespace detail {
struct Residency;
}  // namespace detail

/** A movable lease that keeps an inspectable set of resource URIs resident.
 *
 * Selectors are snapshots. include() adds a selector and immediately refreshes
 * the union; refresh() reruns every selector so newly created files join and
 * vanished files leave. Multiple leases may retain the same URI independently.
 * Destroying a lease releases only its own claim. The Hub must outlive calls on
 * its leases, but a lease may be destroyed safely after its Hub. */
class ResourceLease {
 public:
  ResourceLease() = default;
  ~ResourceLease();

  /** Takes over @p other's claim, leaving it holding nothing. */
  ResourceLease(ResourceLease&& other) noexcept;
  /** Takes over @p other's claim, releasing this one's. */
  ResourceLease& operator=(ResourceLease&& other) noexcept;
  ResourceLease(const ResourceLease&) = delete;
  ResourceLease& operator=(const ResourceLease&) = delete;

  /** Adds @p selector and refreshes the retained union. Returns its new size.
   */
  size_t include(std::string_view selector);

  /** Reruns every included selector and updates the retained URI snapshot. */
  size_t refresh();

  /** Loads the retained resources into their Hub's byte cache concurrently. */
  size_t preload();

  /** The sorted, duplicate-free URI snapshot this lease currently retains. */
  std::span<const std::string> uris() const { return m_uris; }

 private:
  friend class Hub;
  ResourceLease(Hub& hub, std::shared_ptr<detail::Residency> residency,
                std::vector<std::string> selectors);

  void release();

  Hub* m_hub = nullptr;
  std::weak_ptr<detail::Residency> m_residency;
  std::vector<std::string> m_selectors;
  std::vector<std::string> m_uris;
};

/** Reads the distinct @p uris concurrently into @p hub's byte cache and
 *  returns how many are ready. No decoding is performed. */
size_t preload(Hub& hub, std::span<const std::string_view> uris);

/** The same, over the strings a lease answers with, so a retained set
 *  reaches this call without being copied into a second vector. */
size_t preload(Hub& hub, std::span<const std::string> uris);

/** Selects @p selector and concurrently reads the resulting files. */
size_t preload(Hub& hub, std::string_view selector);

/** An empty resource-retention lease bound to @p hub. */
ResourceLease retain(Hub& hub);

/** A lease retaining the current files selected by @p selector. */
ResourceLease retain(Hub& hub, std::string_view selector);

/** A lease retaining the union of the current selector snapshots. */
ResourceLease retain(Hub& hub, std::span<const std::string_view> selectors);

/** The same from selectors written out at the call site. */
ResourceLease retain(Hub& hub,
                     std::initializer_list<std::string_view> selectors);

/** Discards every cached entry not protected by a resource lease. Values
 *  already returned in shared_ptrs stay alive for their holders. Returns
 *  the number of cache entries discarded. */
size_t discardUnretained(Hub& hub);

}  // namespace sigil::io
