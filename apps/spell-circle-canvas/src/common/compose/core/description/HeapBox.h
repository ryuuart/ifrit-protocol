#pragma once

/** @file
 * Internal to the kernel — the value-semantic heap box a node keeps each
 * rare block of fields in.
 */

#include <memory>

namespace sigil::compose::detail {

/** Value-semantic heap box for ElementNode's rare-field blocks: absent
 *  costs one null pointer; copying deep-copies a present block (the COW
 *  clone in detail::NodeHandle::operator-> relies on ElementNode's
 *  defaulted copy constructor). ensure() is the builder-side entry. */
template <class T>
class Box {
 public:
  Box() = default;
  Box(const Box& other)
      : m_ptr(other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr) {}
  Box(Box&&) noexcept = default;
  Box& operator=(const Box& other) {
    m_ptr = other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr;
    return *this;
  }
  Box& operator=(Box&&) noexcept = default;

  explicit operator bool() const { return m_ptr != nullptr; }
  T* operator->() { return m_ptr.get(); }
  const T* operator->() const { return m_ptr.get(); }
  T& operator*() { return *m_ptr; }
  const T& operator*() const { return *m_ptr; }
  T& ensure() {
    if (!m_ptr) m_ptr = std::make_unique<T>();
    return *m_ptr;
  }

 private:
  std::unique_ptr<T> m_ptr;
};

}  // namespace sigil::compose::detail
