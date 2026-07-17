#pragma once

/** @file
 * @ingroup document
 *
 * Minimal inline-storage vector used by TextFlow's public value types
 * (Word::segments). The first `InlineCapacity` elements live inside the
 * object; growth beyond that moves to the heap. Deliberately tiny — only
 * the operations the layout pipeline uses — so the public headers carry no
 * third-party container dependency.
 */

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace textflow {

/**
 * A vector whose first `InlineCapacity` elements need no allocation.
 *
 * Semantics follow std::vector where implemented: contiguous storage,
 * amortized push_back, value copy/move of the whole container. clear()
 * keeps the current capacity (inline or heap) so rebuild loops stay
 * allocation-free. Unlike std::vector, moved-from containers are empty with
 * inline capacity.
 */
template <typename T, size_t InlineCapacity> class InlineVector {
  static_assert(InlineCapacity > 0, "inline capacity must be non-zero");

public:
  InlineVector() = default;

  /** Copies `other`'s elements; small counts land in inline storage. */
  InlineVector(const InlineVector &other) { appendCopyOf(other); }

  /** Steals heap storage, or moves elements when `other` is inline; the
   *  source is left empty with inline capacity. */
  InlineVector(InlineVector &&other) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    stealFrom(std::move(other));
  }

  /** Replaces the contents with a copy of `other`'s elements. */
  InlineVector &operator=(const InlineVector &other) {
    if (this != &other) {
      reset();
      appendCopyOf(other);
    }
    return *this;
  }

  /** Replaces the contents by stealing from `other`, which is left empty
   *  with inline capacity. */
  InlineVector &
  operator=(InlineVector &&other) noexcept(std::is_nothrow_move_constructible_v<T>) {
    if (this != &other) {
      reset();
      stealFrom(std::move(other));
    }
    return *this;
  }

  ~InlineVector() { reset(); }

  /** Returns a pointer to the first element of the contiguous storage. */
  T *data() noexcept { return m_heap ? m_heap : inlineData(); }
  /** Returns a const pointer to the first element of the storage. */
  const T *data() const noexcept { return m_heap ? m_heap : inlineData(); }

  /** Returns an iterator (plain pointer) to the first element. */
  T *begin() noexcept { return data(); }
  /** Returns the past-the-end iterator. */
  T *end() noexcept { return data() + m_size; }
  /** Returns a const iterator to the first element. */
  const T *begin() const noexcept { return data(); }
  /** Returns the past-the-end const iterator. */
  const T *end() const noexcept { return data() + m_size; }

  /** Returns the number of live elements. */
  [[nodiscard]] size_t size() const noexcept { return m_size; }
  /** Returns whether the container holds no elements. */
  [[nodiscard]] bool empty() const noexcept { return m_size == 0; }
  /** Returns the element count storable without another allocation. */
  [[nodiscard]] size_t capacity() const noexcept { return m_capacity; }

  /** Returns the element at `index`; no bounds checking. */
  T &operator[](size_t index) noexcept { return data()[index]; }
  /** Returns the element at `index` (const); no bounds checking. */
  const T &operator[](size_t index) const noexcept { return data()[index]; }
  /** Returns the first element; undefined when empty. */
  T &front() noexcept { return data()[0]; }
  /** Returns the first element (const); undefined when empty. */
  const T &front() const noexcept { return data()[0]; }
  /** Returns the last element; undefined when empty. */
  T &back() noexcept { return data()[m_size - 1]; }
  /** Returns the last element (const); undefined when empty. */
  const T &back() const noexcept { return data()[m_size - 1]; }

  /** Destroys every element but keeps the current storage, so refill loops
   *  (Paragraph re-derives segments on every analysis pass) never
   *  re-allocate. */
  void clear() noexcept {
    destroyElements();
    m_size = 0;
  }

  /** Ensures capacity for at least `requestedCapacity` elements. */
  void reserve(size_t requestedCapacity) {
    if (requestedCapacity > m_capacity)
      grow(requestedCapacity);
  }

  /** Appends a copy of `value`, growing storage when full. */
  void push_back(const T &value) { emplace_back(value); }
  /** Appends by moving `value`, growing storage when full. */
  void push_back(T &&value) { emplace_back(std::move(value)); }

  /** Constructs an element in place at the end and returns it. */
  template <typename... Args> T &emplace_back(Args &&...args) {
    if (m_size == m_capacity)
      grow(m_capacity * 2);
    T *slot = data() + m_size;
    ::new (static_cast<void *>(slot)) T(std::forward<Args>(args)...);
    ++m_size;
    return *slot;
  }

  /** Element-wise equality (requires T::operator==). */
  bool operator==(const InlineVector &other) const {
    if (m_size != other.m_size)
      return false;
    for (size_t index = 0; index < m_size; ++index)
      if (!(data()[index] == other.data()[index]))
        return false;
    return true;
  }

private:
  T *inlineData() noexcept {
    return std::launder(reinterpret_cast<T *>(m_inlineStorage));
  }
  const T *inlineData() const noexcept {
    return std::launder(reinterpret_cast<const T *>(m_inlineStorage));
  }

  void destroyElements() noexcept {
    for (size_t index = 0; index < m_size; ++index)
      data()[index].~T();
  }

  /** Destroys elements and returns to the empty, inline-capacity state. */
  void reset() noexcept {
    destroyElements();
    if (m_heap)
      std::allocator<T>().deallocate(m_heap, m_capacity);
    m_heap = nullptr;
    m_size = 0;
    m_capacity = InlineCapacity;
  }

  void appendCopyOf(const InlineVector &other) {
    reserve(other.m_size);
    for (size_t index = 0; index < other.m_size; ++index)
      ::new (static_cast<void *>(data() + index)) T(other.data()[index]);
    m_size = other.m_size;
  }

  void stealFrom(InlineVector &&other) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    if (other.m_heap) {
      // Heap storage transfers by pointer; the source reverts to inline.
      m_heap = other.m_heap;
      m_capacity = other.m_capacity;
      m_size = other.m_size;
      other.m_heap = nullptr;
      other.m_size = 0;
      other.m_capacity = InlineCapacity;
    } else {
      for (size_t index = 0; index < other.m_size; ++index)
        ::new (static_cast<void *>(inlineData() + index))
            T(std::move(other.inlineData()[index]));
      m_size = other.m_size;
      other.clear();
    }
  }

  void grow(size_t minimumCapacity) {
    const size_t newCapacity =
        minimumCapacity > m_capacity * 2 ? minimumCapacity : m_capacity * 2;
    T *newStorage = std::allocator<T>().allocate(newCapacity);
    T *source = data();
    for (size_t index = 0; index < m_size; ++index) {
      ::new (static_cast<void *>(newStorage + index))
          T(std::move(source[index]));
      source[index].~T();
    }
    if (m_heap)
      std::allocator<T>().deallocate(m_heap, m_capacity);
    m_heap = newStorage;
    m_capacity = newCapacity;
  }

  alignas(T) std::byte m_inlineStorage[InlineCapacity * sizeof(T)];
  T *m_heap = nullptr;
  size_t m_size = 0;
  size_t m_capacity = InlineCapacity;
};

} // namespace textflow
