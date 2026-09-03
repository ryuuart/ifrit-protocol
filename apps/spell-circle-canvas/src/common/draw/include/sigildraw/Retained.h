#pragma once

/** @file
 * What a pen keeps between frames on behalf of a guest: a store keyed by
 * the call site that painted the guest.
 */

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <source_location>
#include <string_view>
#include <typeindex>
#include <unordered_map>

namespace sigil::draw {

/** WHAT TELLS ONE RETAINED GUEST FROM ANOTHER: the call site — file,
 *  line and column — and an index the caller adds when one call site
 *  paints several, as a loop over cards does. */
struct Slot {
  const char* file = "";
  uint32_t line = 0;
  uint32_t column = 0;
  int index = 0;

  static Slot at(const std::source_location& where, int index = 0) {
    return {where.file_name(), where.line(), where.column(), index};
  }
  bool operator==(const Slot& o) const {
    return line == o.line && column == o.column && index == o.index &&
           std::string_view(file) == std::string_view(o.file);
  }
};

struct SlotHash {
  size_t operator()(const Slot& s) const {
    size_t h = std::hash<std::string_view>{}(std::string_view(s.file));
    h ^= (size_t)s.line * 0x9e3779b97f4a7c15ull;
    h ^= ((size_t)s.column << 17u) ^ ((size_t)(uint32_t)s.index << 33u);
    return h;
  }
};

/** THE STORE: one value per slot, of whatever type the guest's library
 *  keeps — a composer with its clock, a shaped paragraph, anything that
 *  is worth not rebuilding every frame. The pen owns one and outlives
 *  every frame drawn through it, which is what makes a call site's guest
 *  the same guest next frame. */
class Retained {
 public:
  /** The value at @p slot, made by @p make on the first ask. A slot
   *  asked for as a different type than it holds is remade: a call site
   *  that changed what it paints starts over rather than being handed
   *  the wrong object. */
  template <class T, class Make>
  T& get(const Slot& slot, Make make) {
    auto it = m_entries.find(slot);
    if (it == m_entries.end() ||
        it->second.type != std::type_index(typeid(T))) {
      std::shared_ptr<T> made = make();
      Entry entry{std::type_index(typeid(T)), std::move(made)};
      it = m_entries.insert_or_assign(slot, std::move(entry)).first;
    }
    return *static_cast<T*>(it->second.value.get());
  }

  [[nodiscard]] size_t size() const { return m_entries.size(); }
  void clear() { m_entries.clear(); }

 private:
  struct Entry {
    std::type_index type;
    std::shared_ptr<void> value;
  };
  std::unordered_map<Slot, Entry, SlotHash> m_entries;
};

}  // namespace sigil::draw
