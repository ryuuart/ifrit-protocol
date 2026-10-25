#pragma once

/** @file
 * @ingroup data-decode
 * A JSON DOCUMENT AS A VALUE, and the rectangle inside one.
 *
 * `Json` is the whole document: nested, ordered, comparable, copyable —
 * what a nested record is read as when it is not a table at all. A
 * document that IS rectangular becomes a `Table` instead, in any of the
 * three shapes data is published in.
 *
 * The parser is somebody else's and stays behind this header: nothing
 * here exposes it, so a consumer compiles against the standard library.
 */

#include <sigilcore/report/Error.h>
#include <sigildata/table/Table.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::data {

/** ONE JSON VALUE, whatever it is. An object keeps its members in the
 *  order the document wrote them, and lookup by key is therefore a
 *  scan, which is what a record of a few fields wants.
 *  @trap A document that writes one key twice keeps BOTH members and a
 *  lookup answers the first, since dropping one would be an edit to
 *  somebody else's document. */
class Json {
 public:
  /** A list's members, in document order. */
  using Array = std::vector<Json>;
  /** A record's members, keyed and in document order. */
  using Object = std::vector<std::pair<std::string, Json>>;
  /** Everything a value can be, in the order `Kind` names them. */
  using Held =
      std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

  /** What a value is, in JSON's own words. The order is the order of
   *  `Held`. */
  enum class Kind { Null, Boolean, Number, String, Array, Object };

  Json() = default;
  /** The null value, written out. */
  Json(std::nullptr_t) {}
  /** A boolean value. */
  Json(bool value) : m_held(value) {}
  /** A number. */
  Json(double value) : m_held(value) {}
  /** A whole number is a number, and keeps its integer reading. Without
   *  this one `Json(1)` is ambiguous: an int converts to bool and to
   *  double at the same rank. */
  template <std::integral T>
    requires(!std::same_as<T, bool>)
  Json(T value) : m_held(static_cast<double>(value)) {
    if (std::in_range<std::int64_t>(value))
      m_integer = static_cast<std::int64_t>(value);
  }
  /** Text. */
  Json(std::string value) : m_held(std::move(value)) {}
  /** A literal is text. Without this one a `const char*` would pick the
   *  boolean constructor and a name would become `true`. */
  Json(const char* value) : m_held(std::string(value)) {}
  /** A list of values. */
  Json(Array value) : m_held(std::move(value)) {}
  /** A record of keyed values, in the order given. */
  Json(Object value) : m_held(std::move(value)) {}

  /** Two values are equal when they hold the same thing; a number
   *  written whole equals the same number written with a fraction, as
   *  `1 == 1.0` in the languages that read JSON. Two integer readings
   *  compare exactly, past where a double tells them apart. */
  bool operator==(const Json& other) const {
    if (m_integer && other.m_integer) return *m_integer == *other.m_integer;
    return m_held == other.m_held;
  }

  /** Which of the six things this value is. */
  Kind kind() const { return static_cast<Kind>(m_held.index()); }
  /** Whether this is the null value. */
  bool null() const { return kind() == Kind::Null; }
  /** What is held, for a caller that dispatches on the type. */
  const Held& held() const { return m_held; }

  /** THE VALUE, or @p fallback when this is something else. Reading the
   *  wrong kind is not an error: a document is somebody else's and a
   *  reader that asked for a number where a string stands wants its own
   *  default, not a throw. */
  bool boolean(bool fallback = false) const;
  /** The number, or @p fallback when this is something else. */
  double number(double fallback = 0.0) const;
  /** THE NUMBER AS THE WHOLE NUMBER IT WAS WRITTEN AS: a document's
   *  number written with no fraction and no exponent, or one made from a
   *  C++ integer, read exactly as a signed 64-bit integer. Nothing for a
   *  number written `1.0` or `1e3`, for one past the signed 64-bit range,
   *  and for anything that is not a number — the distinction a reader
   *  of the same text in another language keeps between an integer and a
   *  float. */
  std::optional<std::int64_t> integer() const { return m_integer; }
  /** The string, or @p fallback when this is something else. */
  std::string_view string(std::string_view fallback = {}) const;

  /** The members of an array, or nothing when this is not one. */
  std::span<const Json> array() const;
  /** The members of an object, keyed and in document order, or nothing
   *  when this is not one. */
  std::span<const std::pair<std::string, Json>> object() const;

  /** How many members an array or an object has; 0 for everything else. */
  size_t size() const;

  /** The member called @p key, or a null value when there is none. The
   *  reference is to a shared null, so a chain of lookups through
   *  members that are not there answers null instead of crashing. */
  const Json& operator[](std::string_view key) const;
  /** The @p index-th member of an array, or null when it is not there. */
  const Json& operator[](size_t index) const;

 private:
  Held m_held;
  /** The integer reading of a number that has one; see `integer()`. */
  std::optional<std::int64_t> m_integer;
};

/** The name a resource hub registers and asks for a document under, so
 *  `hub.load<Json>(uri)` answers wherever it is compiled. */
inline std::string_view meaningName(std::type_identity<Json>) {
  return "data.Json";
}

/** THE RECTANGLE INSIDE @p document — a list of records, a record of
 *  lists, or a list of lists — or the error that says why it holds no
 *  rectangle. A
 *  column's type is the one every present value in it shares.
 *  @trap A cell holding a list or a record of its own has no place in a
 *  rectangle and is MISSING; read that document as a `Json` instead. */
std::expected<Table, core::Error> tableFromJson(const Json& document);

}  // namespace sigil::data
