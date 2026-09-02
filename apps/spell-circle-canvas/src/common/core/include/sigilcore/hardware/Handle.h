#pragma once
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace sigil::core::hardware {

/**
 * A name for a device resource: a slot index and the generation of that
 * slot when the name was issued. A slot is reused after its resource is
 * released, but with a higher generation, so a name kept past its
 * resource's death compares unequal to the name of whatever now lives
 * there and is rejected as stale. Generation zero is reserved for the
 * null handle, which every default-constructed handle is.
 */
struct Handle {
  uint32_t index = 0;
  uint32_t generation = 0;

  /** True for a handle that was issued at some point — not proof the
   *  resource still lives; that is the table's answer. */
  explicit operator bool() const { return generation != 0; }
  friend bool operator==(const Handle& a, const Handle& b) {
    return a.index == b.index && a.generation == b.generation;
  }
  friend bool operator!=(const Handle& a, const Handle& b) { return !(a == b); }
};

/** A handle that can only name one kind of resource: a TextureHandle is
 *  not a BufferHandle even when the bits agree. */
template <typename Tag>
struct TypedHandle : Handle {};

struct TextureTag;
struct BufferTag;
struct FenceTag;
using TextureHandle = TypedHandle<TextureTag>;
using BufferHandle = TypedHandle<BufferTag>;
using FenceHandle = TypedHandle<FenceTag>;

/**
 * The slot store behind a handle type: allocate a value and get its
 * handle, find a value by handle, release it. Released slots go on a
 * free list and are reused with a bumped generation, so every handle
 * ever issued for a slot stays distinguishable from every later one
 * until the 32-bit generation wraps. Lookup by a stale or null handle
 * yields null and releases nothing.
 */
template <typename T, typename H>
class HandleTable {
 public:
  /** Issues a handle for @p value, reusing a released slot if one waits. */
  H allocate(T value) {
    uint32_t index;
    if (!m_free.empty()) {
      index = m_free.back();
      m_free.pop_back();
    } else {
      index = static_cast<uint32_t>(m_slots.size());
      m_slots.push_back(Slot{});
    }
    Slot& slot = m_slots[index];
    slot.value = std::move(value);
    slot.live = true;
    ++m_live;
    H handle;
    handle.index = index;
    handle.generation = slot.generation;
    return handle;
  }

  /** The value behind a live handle; null for a stale or null one. */
  T* find(H handle) {
    Slot* slot = live(handle);
    return slot ? &slot->value : nullptr;
  }
  const T* find(H handle) const {
    return const_cast<HandleTable*>(this)->find(handle);
  }

  /** True while @p handle names a live value. */
  bool contains(H handle) const { return find(handle) != nullptr; }

  /** Releases the value behind a live handle and returns it; a stale or
   *  null handle releases nothing and returns the default value. The
   *  slot's generation advances, skipping zero, so the released handle
   *  is stale from here on. */
  T release(H handle) {
    Slot* slot = live(handle);
    if (!slot) return T{};
    T value = std::move(slot->value);
    slot->value = T{};
    slot->live = false;
    if (++slot->generation == 0) slot->generation = 1;
    --m_live;
    m_free.push_back(handle.index);
    return value;
  }

  /** Releases every live value and hands them over, oldest slot first;
   *  every outstanding handle is stale afterwards. */
  std::vector<T> drain() {
    std::vector<T> values;
    for (uint32_t index = 0; index < m_slots.size(); ++index) {
      Slot& slot = m_slots[index];
      if (!slot.live) continue;
      H handle;
      handle.index = index;
      handle.generation = slot.generation;
      values.push_back(release(handle));
    }
    return values;
  }

  /** Live values. */
  size_t size() const { return m_live; }
  /** Slots ever created, live or waiting for reuse. */
  size_t capacity() const { return m_slots.size(); }

 private:
  struct Slot {
    T value{};
    uint32_t generation = 1;
    bool live = false;
  };

  Slot* live(H handle) {
    if (!handle || handle.index >= m_slots.size()) return nullptr;
    Slot& slot = m_slots[handle.index];
    if (!slot.live || slot.generation != handle.generation) return nullptr;
    return &slot;
  }

  std::vector<Slot> m_slots;
  std::vector<uint32_t> m_free;
  size_t m_live = 0;
};

}  // namespace sigil::core::hardware
