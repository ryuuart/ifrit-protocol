/** @file Resource leases: selector unions and URI residency claims. */

#include <algorithm>
#include <iterator>
#include <utility>

#include "Residency.h"
#include "sigilio/hub/Hub.h"

namespace sigil::io {

namespace {

void releaseUris(detail::Residency& residency,
                 std::span<const std::string> uris) {
  const std::lock_guard lock(residency.mutex);
  for (const std::string& uri : uris) {
    const auto found = residency.pins.find(uri);
    if (found == residency.pins.end()) continue;
    if (--found->second == 0) residency.pins.erase(found);
  }
}

void replaceUris(detail::Residency& residency,
                 std::span<const std::string> previous,
                 std::span<const std::string> current) {
  const std::lock_guard lock(residency.mutex);
  for (const std::string& uri : previous)
    if (!std::ranges::binary_search(current, uri)) {
      const auto found = residency.pins.find(uri);
      if (found != residency.pins.end() && --found->second == 0)
        residency.pins.erase(found);
    }
  for (const std::string& uri : current)
    if (!std::ranges::binary_search(previous, uri)) ++residency.pins[uri];
}

}  // namespace

ResourceLease::ResourceLease(Hub& hub,
                             std::shared_ptr<detail::Residency> residency,
                             std::vector<std::string> selectors)
    : m_hub(&hub), m_residency(residency), m_selectors(std::move(selectors)) {
  std::ranges::sort(m_selectors);
  m_selectors.erase(std::unique(m_selectors.begin(), m_selectors.end()),
                    m_selectors.end());
  refresh();
}

ResourceLease::~ResourceLease() { release(); }

ResourceLease::ResourceLease(ResourceLease&& other) noexcept
    : m_hub(std::exchange(other.m_hub, nullptr)),
      m_residency(std::move(other.m_residency)),
      m_selectors(std::move(other.m_selectors)),
      m_uris(std::move(other.m_uris)) {
  other.m_uris.clear();
}

ResourceLease& ResourceLease::operator=(ResourceLease&& other) noexcept {
  if (this == &other) return *this;
  release();
  m_hub = std::exchange(other.m_hub, nullptr);
  m_residency = std::move(other.m_residency);
  m_selectors = std::move(other.m_selectors);
  m_uris = std::move(other.m_uris);
  other.m_uris.clear();
  return *this;
}

size_t ResourceLease::include(std::string_view selector) {
  if (!m_hub || m_residency.expired()) return m_uris.size();
  const auto where = std::ranges::lower_bound(m_selectors, selector);
  if (where == m_selectors.end() || *where != selector)
    m_selectors.insert(where, std::string(selector));
  return refresh();
}

size_t ResourceLease::refresh() {
  const std::shared_ptr<detail::Residency> residency = m_residency.lock();
  if (!m_hub || !residency) return m_uris.size();

  std::vector<std::string> selected;
  for (const std::string& selector : m_selectors) {
    std::vector<std::string> matches = m_hub->select(selector);
    selected.insert(selected.end(), std::make_move_iterator(matches.begin()),
                    std::make_move_iterator(matches.end()));
  }
  std::ranges::sort(selected);
  selected.erase(std::unique(selected.begin(), selected.end()), selected.end());
  replaceUris(*residency, m_uris, selected);
  m_uris = std::move(selected);
  return m_uris.size();
}

size_t ResourceLease::preload() {
  if (!m_hub || m_residency.expired()) return 0;
  std::vector<std::string_view> uris;
  uris.reserve(m_uris.size());
  for (const std::string& uri : m_uris) uris.push_back(uri);
  return m_hub->preload(uris);
}

void ResourceLease::release() {
  if (const std::shared_ptr<detail::Residency> residency = m_residency.lock())
    releaseUris(*residency, m_uris);
  m_hub = nullptr;
  m_residency.reset();
  m_selectors.clear();
  m_uris.clear();
}

ResourceLease Hub::retain() { return ResourceLease(*this, residency(), {}); }

ResourceLease Hub::retain(std::string_view selector) {
  return ResourceLease(*this, residency(), {std::string(selector)});
}

ResourceLease Hub::retain(std::span<const std::string_view> selectors) {
  std::vector<std::string> owned;
  owned.reserve(selectors.size());
  for (std::string_view selector : selectors) owned.emplace_back(selector);
  return ResourceLease(*this, residency(), std::move(owned));
}

ResourceLease Hub::retain(std::initializer_list<std::string_view> selectors) {
  return retain(
      std::span<const std::string_view>(selectors.begin(), selectors.size()));
}

std::shared_ptr<detail::Residency> Hub::residency() {
  const std::lock_guard lock(m_synchronization->mutex);
  if (!m_residency) m_residency = std::make_shared<detail::Residency>();
  return m_residency;
}

}  // namespace sigil::io
