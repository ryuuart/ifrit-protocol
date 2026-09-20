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

#include <sigildata/table/Table.h>

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

  /** What a value is. The order is the order of `Held`. */
  enum class Kind { Null, Boolean, Number, Text, List, Record };

  Json() = default;
  /** The null value, written out. */
  Json(std::nullptr_t) {}
  /** A boolean value. */
  Json(bool value) : m_held(value) {}
  /** A number. */
  Json(double value) : m_held(value) {}
  /** A whole number is a number. Without this one `Json(1)` is
   *  ambiguous: an int converts to bool and to double at the same
   *  rank. */
  template <std::integral T>
    requires(!std::same_as<T, bool>)
  Json(T value) : m_held(static_cast<double>(value)) {}
  /** Text. */
  Json(std::string value) : m_held(std::move(value)) {}
  /** A literal is text. Without this one a `const char*` would pick the
   *  boolean constructor and a name would become `true`. */
  Json(const char* value) : m_held(std::string(value)) {}
  /** A list of values. */
  Json(Array value) : m_held(std::move(value)) {}
  /** A record of keyed values, in the order given. */
  Json(Object value) : m_held(std::move(value)) {}

  bool operator==(const Json&) const = default;

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
  /** The text, or @p fallback when this is something else. */
  std::string_view text(std::string_view fallback = {}) const;

  /** The members of a list, or nothing when this is not one. */
  std::span<const Json> items() const;
  /** The members of a record, or nothing when this is not one. */
  std::span<const std::pair<std::string, Json>> fields() const;

  /** How many members a list or a record has; 0 for everything else. */
  size_t size() const;

  /** The member called @p key, or a null value when there is none. The
   *  reference is to a shared null, so a chain of lookups through
   *  members that are not there answers null instead of crashing. */
  const Json& operator[](std::string_view key) const;
  /** The @p index-th member of a list, or null when it is not there. */
  const Json& operator[](size_t index) const;

 private:
  Held m_held;
};

/** THE DOCUMENT IN @p text, or nothing when it is not JSON. A lone
 *  number, string, boolean or null IS a document, and so answers a
 *  value that is not a list or a record.
 *  @trap Text that is not valid UTF-8, and text nested deeper than the
 *  parser reads, answer nothing rather than the part that parsed. */
std::optional<Json> decodeJson(std::string_view text);

/** @p value AS JSON TEXT, which decodeJson reads back as the same
 *  value. Compact, because what this writes goes on a wire or into a
 *  file rather than in front of an eye; a record keeps the order its
 *  members are in, and a number is written with the fewest digits that
 *  read back as itself.
 *  @trap A number that is not finite has no JSON spelling and is
 *  written null, which is the value a reader gets back for it. */
std::string encodeJson(const Json& value);

/** THE RECTANGLE INSIDE @p document — a list of records, a record of
 *  lists, or a list of lists — or nothing when it holds no rectangle. A
 *  column's type is the one every present value in it shares.
 *  @trap A cell holding a list or a record of its own has no place in a
 *  rectangle and is MISSING; read that document as a `Json` instead. */
std::optional<Table> tableFromJson(const Json& document);

}  // namespace sigil::data
