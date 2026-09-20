#pragma once

/** @file
 * @ingroup data-decode
 * A SCHEMA AS ONE VALUE, and the generated root that carries one. A
 * FlatBuffer says nothing about itself: the names of its fields are in
 * the schema and nowhere in the message. A Schema is that schema as one
 * copyable token, which converts a buffer to its JSON form and a JSON
 * form back to a buffer without naming the root type again — what a
 * door reading a wire holds, where the type of the next message is not
 * known at the call site.
 *
 * NOTHING OF THE READER UNDER IT IS NAMED HERE, so a consumer that
 * holds a schema compiles against the standard library alone.
 */

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::data {

/** Whether a generated Root carries its binary schema — a header written
 *  with the embed flag — which is what converting the JSON form needs. */
template <class Root>
concept CarriesSchema = requires {
  { Root::BinarySchema::data() } -> std::convertible_to<const uint8_t*>;
  { Root::BinarySchema::size() } -> std::convertible_to<size_t>;
};

/** A SCHEMA AS ONE VALUE: the two conversions a schema is, with the
 *  root type named once and never again. `schema<Sky>()` makes one, and
 *  the binary schema behind it is read once and shared, so a copy of
 *  the token is the same schema. Both conversions refuse what does not
 *  fit rather than answering part of it, so a reader holding one knows
 *  that what it got back means what the schema says it means.
 *  @trap THE ROOT IS THE ONE THE SCHEMA FILE DECLARES, not the type
 *  named: a generated header embeds its file's whole schema beside
 *  every type in it, so naming another type of the same file makes the
 *  same schema. `rootName()` says which. */
class Schema {
 public:
  /** A SCHEMA THAT IS NONE: it converts nothing, and every reading
   *  below says so. */
  Schema() = default;

  /** THE SCHEMA A `.bfbs` FILE HOLDS — the bytes `flatc -b --schema`
   *  writes, which a generated header embeds. Every way of making a
   *  schema comes through here. None where the bytes are no schema, or
   *  where the schema declares no root type; @p why says which where it
   *  is asked for. The bytes are copied, so the caller keeps nothing
   *  for this. */
  static Schema fromBinarySchema(std::span<const std::byte> bfbs,
                                 std::string* why = nullptr);

  /** Whether this is a schema at all. */
  explicit operator bool() const { return m_state != nullptr; }

  /** WHETHER @p binary IS THE ROOT: the check both conversions make,
   *  for a holder that wants the answer and not the reading. False for
   *  a buffer of another schema, one cut short, and for a schema that
   *  is none; @p why says which where it is asked for. */
  bool verifies(std::span<const std::byte> binary,
                std::string* why = nullptr) const;

  /** THE BUFFER IN @p binary AS THE SCHEMA'S OWN JSON FORM. The bytes
   *  are verified against the root first, so a buffer of another
   *  schema, or one cut short, answers nothing rather than a reading of
   *  whatever the bytes happened to be; @p why says which where it is
   *  asked for. */
  std::optional<std::string> text(std::span<const std::byte> binary,
                                  std::string* why = nullptr) const;

  /** THE BUFFER @p json MAKES, read through the schema. Nothing where
   *  the text does not fit it — a field the schema does not declare, a
   *  value of the wrong type, text that is no document — and @p why
   *  carries the parser's own message, the line, the column and the
   *  field. What comes back verifies as the root. */
  std::optional<std::vector<std::byte>> binary(
      std::string_view json, std::string* why = nullptr) const;

  /** THE ROOT BOTH CONVERSIONS GO THROUGH, fully qualified —
   *  `feed_sky.Sky`. Empty for a schema that is none. The view is into
   *  the schema and stands as long as it does. */
  std::string_view rootName() const;

 private:
  /** What the token stands on: the schema as the reader under it holds
   *  it, named here and defined where that reader's headers are opened,
   *  so this header spells none of them. One pointer, so a copy of the
   *  token is the same schema rather than a second reading of it. */
  struct State;
  std::shared_ptr<State> m_state;
};

/** THE SCHEMA A GENERATED Root CARRIES, as one value:
 *  `schema<feed_sky::Sky>()`. The header the build wrote embeds its
 *  file's whole schema, so this reads no schema file. */
template <CarriesSchema Root>
Schema schema() {
  return Schema::fromBinarySchema(
      {reinterpret_cast<const std::byte*>(Root::BinarySchema::data()),
       Root::BinarySchema::size()});
}

}  // namespace sigil::data
