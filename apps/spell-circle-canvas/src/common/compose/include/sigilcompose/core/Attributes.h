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
      set(name, std::string(std::string_view(value)));
    } else {
      setEntry(name,
               Entry{std::string(), std::make_shared<const T>(std::move(value)),
                     &typeid(T), &equalsAs<T>});
    }
  }

  /** The fact under @p name, read as a @p T — or nothing, when no fact
   *  has that name or it was written as another type. A fact written as
   *  an `int` is not read as a `float`: the type is part of the fact.
   *
   *  A fact reads back in its type WHEREVER THE READER WAS COMPILED: a
   *  sketch built as an image of its own states a fact of a library type
   *  and the library, in the host, reads it. */
  template <typename T>
  std::optional<T> get(std::string_view name) const {
    const Entry* entry = find(name);
    if (!entry || !sameType(*entry->type, typeid(T))) return std::nullopt;
    return *static_cast<const T*>(entry->value.get());
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
  /** One fact: its name, its value — immutable, so copies of a table
   *  share it — the identity of the type it was written in, and the
   *  equality of that type. */
  struct Entry {
    std::string name;
    std::shared_ptr<const void> value;
    const std::type_info* type = nullptr;
    bool (*equals)(const void*, const void*) = nullptr;
  };
  template <typename T>
  static bool equalsAs(const void* a, const void* b) {
    return *static_cast<const T*>(a) == *static_cast<const T*>(b);
  }
  /** WHETHER TWO TYPE IDENTITIES NAME ONE TYPE across images. An image
   *  that keeps a type's identity private to itself — a sketch compiled
   *  with hidden visibility — holds its own copy of the identity of a
   *  type the host also names, and the platform compares a copy it
   *  marked private against the host's shared one by address, so the two
   *  differ. Their mangled names are one spelling, so the names decide —
   *  except for a type in an anonymous namespace, whose spelling repeats
   *  in another translation unit for another type, and which only its
   *  own image can name. */
  static bool sameType(const std::type_info& written,
                       const std::type_info& read);
  const Entry* find(std::string_view name) const;
  void setEntry(std::string_view name, Entry entry);

  std::shared_ptr<const std::vector<Entry>> m_entries;
};

}  // namespace sigil::compose
