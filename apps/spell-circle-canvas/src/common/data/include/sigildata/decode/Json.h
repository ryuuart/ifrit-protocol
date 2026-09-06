#pragma once

/** @file
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

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sigil::data {

/** ONE JSON VALUE, whatever it is.
 *
 *  An object keeps its members in the order the document wrote them,
 *  because that order is the author's and a reader who prints the
 *  document back should not reorder it. Lookup by key is therefore a
 *  scan, which is what a record of a few fields wants.
 *
 *  ```
 *  const Json doc = *decodeJson(text);
 *  for (const Json& node : doc["nodes"].items())
 *    place(node["x"].number(), node["y"].number(), node["name"].text());
 *  ```
 */
class Json {
 public:
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;
  using Held = std::variant<std::nullptr_t, bool, double, std::string, Array,
                            Object>;

  /** What a value is. The order is the order of `Held`. */
  enum class Kind { Null, Boolean, Number, Text, List, Record };

  Json() = default;
  Json(std::nullptr_t) {}
  Json(bool value) : m_held(value) {}
  Json(double value) : m_held(value) {}
  Json(std::string value) : m_held(std::move(value)) {}
  /** A literal is text. Without this one a `const char*` would pick the
   *  boolean constructor and a name would become `true`. */
  Json(const char* value) : m_held(std::string(value)) {}
  Json(Array value) : m_held(std::move(value)) {}
  Json(Object value) : m_held(std::move(value)) {}

  bool operator==(const Json&) const = default;

  Kind kind() const { return static_cast<Kind>(m_held.index()); }
  bool null() const { return kind() == Kind::Null; }
  const Held& held() const { return m_held; }

  /** THE VALUE, or @p fallback when this is something else. Reading the
   *  wrong kind is not an error: a document is somebody else's and a
   *  reader that asked for a number where a string stands wants its own
   *  default, not a throw. */
  bool boolean(bool fallback = false) const;
  double number(double fallback = 0.0) const;
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

/** THE DOCUMENT IN @p text, or nothing when it is not JSON. */
std::optional<Json> decodeJson(std::string_view text);

/** THE RECTANGLE INSIDE @p document, in whichever of the three shapes it
 *  is published in, or nothing when it holds no rectangle:
 *
 *  - a LIST OF RECORDS — one row each, columns being the union of their
 *    keys in first-appearance order, a record missing a key giving a
 *    missing cell;
 *  - a RECORD OF LISTS — one column each, named by its key;
 *  - a LIST OF LISTS — one row each, columns named by their 1-based
 *    position.
 *
 *  A column's type is the one every present value in it shares: numbers
 *  make a number column, booleans a boolean column, strings that are all
 *  instants a time column, and anything else a text column. A cell
 *  holding a list or a record of its own has no place in a rectangle and
 *  is missing; read that document as a `Json` instead. */
std::optional<Table> tableFromJson(const Json& document);

}  // namespace sigil::data
