#pragma once

/** @file
 * @ingroup compose-core
 *
 * THE TYPED FACTS AN ELEMENT CARRIES: `Attributes`, a table of named
 * values a node states about itself and says nothing more about. Who
 * reads a fact, and what it means, is the reader's business — a layout
 * scheme placing children by a lane, an operator building elements from
 * one — and a fact nothing reads does nothing.
 */

#include <any>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace sigil::compose {

/** WHAT A FACT'S VALUE MAY BE: anything that can be copied and compared,
 *  because a description is a value the reconciler compares to the one
 *  before it, and a fact that could not compare would keep its node from
 *  ever pruning. A string literal is stored as a `std::string`. */
template <typename T>
concept AttributeValue = std::equality_comparable<T> && std::copyable<T>;

/** THE FACTS ON ONE NODE, as one immutable value: each a name and a typed
 *  value, read back with the type it was written in. Copies share their
 *  storage, so a node carrying a table costs a pointer, and writing to a
 *  shared table copies it first.
 *
 *  Two tables are equal when they carry the same names with equal values
 *  of the same type, in any order: the order facts were stated in is not
 *  a fact. */
class Attributes {
 public:
  Attributes() = default;

  /** States @p value under @p name, replacing a fact of that name. */
  template <AttributeValue T>
  void set(std::string_view name, T value) {
    if constexpr (std::is_convertible_v<T, std::string_view> &&
                  !std::same_as<T, std::string>) {
      setEntry(name, std::any(std::string(std::string_view(value))),
               &equalsAs<std::string>);
    } else {
      setEntry(name, std::any(std::move(value)), &equalsAs<T>);
    }
  }

  /** The fact under @p name, read as a @p T — or nothing, when no fact
   *  has that name or it was written as another type. A fact written as
   *  an `int` is not read as a `float`: the type is part of the fact. */
  template <typename T>
  std::optional<T> get(std::string_view name) const {
    const Entry* entry = find(name);
    if (!entry) return std::nullopt;
    if (const T* value = std::any_cast<T>(&entry->value)) return *value;
    return std::nullopt;
  }

  /** Whether a fact stands under @p name, whatever its type. */
  bool has(std::string_view name) const { return find(name) != nullptr; }

  /** Every fact of @p other laid over this table, same names replaced. */
  void merge(const Attributes& other);

  /** The names stated, in the order they were first stated. */
  std::vector<std::string> names() const;

  size_t size() const { return m_entries ? m_entries->size() : 0; }
  bool empty() const { return size() == 0; }

  bool operator==(const Attributes& other) const;

 private:
  struct Entry {
    std::string name;
    std::any value;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
  };
  template <typename T>
  static bool equalsAs(const std::any& a, const std::any& b) {
    const T* left = std::any_cast<T>(&a);
    const T* right = std::any_cast<T>(&b);
    return left && right && *left == *right;
  }
  const Entry* find(std::string_view name) const;
  void setEntry(std::string_view name, std::any value,
                bool (*equals)(const std::any&, const std::any&));

  std::shared_ptr<const std::vector<Entry>> m_entries;
};

}  // namespace sigil::compose
