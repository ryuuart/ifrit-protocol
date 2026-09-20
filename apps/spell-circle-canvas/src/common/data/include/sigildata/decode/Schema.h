#pragma once

/** @file
 * @ingroup data-decode
 * A SCHEMA AS ONE VALUE, and the generated root that carries one.
 *
 * A FlatBuffer says nothing about itself: the names of its fields are in
 * the schema and nowhere in the message. A Schema is that schema as one
 * copyable token, which converts a buffer to its JSON form and a JSON
 * form back to a buffer without naming the root type again — what a door
 * reading a wire holds, where the type of the next message is not known
 * at the call site.
 *
 * `schema<Root>()` makes one out of the schema a generated header
 * embeds, so nothing here reads a schema file.
 * `Schema::fromBinarySchema()` makes the same token out of a schema
 * file's own bytes, for a tool that has no generated header for what it
 * is looking at and was handed the schema instead.
 *
 * NOTHING OF THE READER UNDER IT IS NAMED HERE. The schema is read, and
 * both conversions are run, by a parser whose headers are opened in the
 * one translation unit that defines what the token holds. So a consumer
 * that holds a schema compiles against the standard library alone.
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
 *  root type named once and never again.
 *
 *  `schema<Sky>()` makes one. The binary schema is read once and held
 *  behind a shared pointer, so the token is copied for the cost of that
 *  pointer and every copy is the same schema: a scene keeps one in a
 *  field and hands it to whatever reads a wire.
 *
 *      const Schema sky = schema<feed_sky::Sky>();
 *      const std::optional<std::string> form = sky.text(arrived);
 *      const std::optional<std::vector<std::byte>> out = sky.binary(text);
 *
 *  THE ROOT IS THE ONE THE SCHEMA FILE DECLARES. A generated header
 *  embeds its file's whole schema beside every type in it, so naming
 *  another type of the same file makes the same schema; `rootName()`
 *  says which root both conversions go through.
 *
 *  Both refuse what does not fit rather than answering part of it: a
 *  buffer is verified against that root before a byte of it is read,
 *  and text carrying a field the schema does not declare is no buffer.
 *  So a reader holding a Schema knows that what it got back means what
 *  the schema says it means. */
class Schema {
 public:
  /** A SCHEMA THAT IS NONE: it converts nothing, and every reading
   *  below says so. */
  Schema() = default;

  /** THE SCHEMA A `.bfbs` FILE HOLDS — the bytes `flatc -b --schema`
   *  writes, which a generated header embeds. Every way of making a
   *  schema comes through here, so a schema made from a file and one
   *  made from a generated type are one schema made one way. None where
   *  the bytes are no schema, or where the schema declares no root
   *  type: a schema with no root has nothing to read a buffer AS; @p why
   *  says which where it is asked for. The bytes are copied, so the
   *  caller keeps nothing for this. */
  static Schema fromBinarySchema(std::span<const std::byte> bfbs,
                                 std::string* why = nullptr);

  /** Whether this is a schema at all. */
  explicit operator bool() const { return m_state != nullptr; }

  /** WHETHER @p binary IS THE ROOT: the check both conversions make,
   *  for a holder that wants the answer and not the reading. False for
   *  a buffer of another schema, one cut short, and for a schema that
   *  is none; @p why says which where it is asked for. So bytes that
   *  are kept and read field by field later are proved once, here,
   *  rather than converted whole to prove them. */
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
   *  field, where it is asked for. What comes back verifies as the
   *  root, so whoever is handed it may read it in place. */
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
