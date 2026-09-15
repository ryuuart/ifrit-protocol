#pragma once

/** @file
 * WHAT A GENERATED VALUE HEADER STANDS ON: the readings and the
 * writings every schema needs, with no schema in any of them.
 *
 * A FlatBuffer is read in place, and a generated accessor answers a
 * pointer into the bytes: a string is a `flatbuffers::String*` that may
 * be null, a vector is a `flatbuffers::Vector*` that may be null, and a
 * struct is a pointer at an offset. A consumer that wants a value —
 * something it can copy, hold past the bytes, compare and edit — spells
 * the same three conversions at every field. These are those
 * conversions, written once.
 *
 * ABSENT IS THE EMPTY VALUE for a string and for a vector: the wire
 * carries no difference between a field left out and one written empty,
 * so a reading that answered an optional would invent one. A field the
 * schema declares REQUIRED is the exception, and the generated reading
 * refuses a buffer that left it out rather than answering an empty
 * value for it.
 *
 * THE VERIFIER IS RUN IN ONE PLACE: `rootOf()`, which every generated
 * reading of a whole buffer goes through, so bytes that are not the
 * schema's answer nothing rather than a reading of whatever they were.
 * Reading a table that is already inside a verified buffer runs no
 * second verification.
 *
 * A VALUE'S OWN READING IS NAMED ONCE. `Read<Value>` is the seam a
 * generated header specializes: it says how that value is read out of
 * bytes, so a reader that names the value type reaches the reading
 * without naming it, and a door templated over the value compiles
 * against this header alone.
 *
 * Of flatbuffers this opens the buffer's own header alone — the
 * builder, the verifier, the vector and the string — because a value is
 * read off those and written back through them. Nothing here knows a
 * schema, and nothing here is generated.
 */

#include <flatbuffers/flatbuffers.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::data::values {

/** THE ROOT @p bytes CARRY, verified before a byte of it is read; null
 *  where they are not that root — a buffer cut short, or another
 *  schema's. The pointer is into @p bytes and stands as long as they
 *  do, which is why every generated reading copies out of it. */
template <class Root>
const Root* rootOf(std::span<const std::byte> bytes) {
  if (bytes.empty()) return nullptr;
  const auto* first = reinterpret_cast<const uint8_t*>(bytes.data());
  flatbuffers::Verifier verifier(first, bytes.size());
  if (!verifier.VerifyBuffer<Root>(nullptr)) return nullptr;
  return flatbuffers::GetRoot<Root>(first);
}

/** THE BUFFER @p from HAS FINISHED, as bytes of its own. The builder
 *  owns its memory until it goes, so a writing that hands the buffer
 *  back copies it out. */
inline std::vector<std::byte> bytesOf(flatbuffers::FlatBufferBuilder& from) {
  const auto* first =
      reinterpret_cast<const std::byte*>(from.GetBufferPointer());
  return std::vector<std::byte>(first, first + from.GetSize());
}

/** A string field as a string; absent is empty. */
inline std::string readString(const flatbuffers::String* from) {
  return from ? from->str() : std::string();
}

/** A vector of strings as strings; absent is empty, and so is an entry
 *  the buffer left null. */
inline std::vector<std::string> readStrings(
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* from) {
  std::vector<std::string> out;
  if (!from) return out;
  out.reserve(from->size());
  for (const flatbuffers::String* each : *from) out.push_back(readString(each));
  return out;
}

/** A vector of scalars as a vector; absent is empty. */
template <class Scalar>
std::vector<Scalar> readScalars(const flatbuffers::Vector<Scalar>* from) {
  std::vector<Scalar> out;
  if (!from) return out;
  out.reserve(from->size());
  for (const Scalar each : *from) out.push_back(each);
  return out;
}

/** A vector of bools as bools: the wire holds one byte an entry, which
 *  is why the accessor under this answers bytes. */
inline std::vector<bool> readBools(const flatbuffers::Vector<uint8_t>* from) {
  std::vector<bool> out;
  if (!from) return out;
  out.reserve(from->size());
  for (const uint8_t each : *from) out.push_back(each != 0);
  return out;
}

/** A VECTOR OF ENUMS AS THE ENUM. A vector's entries are the enum's
 *  underlying integer on the wire, so the value names the enumerated
 *  type and this is where the two meet; absent is empty. */
template <class Enumerated, class Stored>
std::vector<Enumerated> readEnums(const flatbuffers::Vector<Stored>* from) {
  std::vector<Enumerated> out;
  if (!from) return out;
  out.reserve(from->size());
  for (const Stored each : *from) out.push_back(static_cast<Enumerated>(each));
  return out;
}

/** ONE VALUE AN ENTRY, for a vector whose entries always read — a
 *  vector of structs, which have no absent form. Absent is empty. */
template <class Element, class ReadEntry>
auto readEach(const flatbuffers::Vector<Element>* from, ReadEntry read) {
  using Entry = typename flatbuffers::Vector<Element>::return_type;
  std::vector<std::invoke_result_t<ReadEntry, Entry>> out;
  if (!from) return out;
  out.reserve(from->size());
  for (const Entry each : *from) out.push_back(read(each));
  return out;
}

/** ONE VALUE AN ENTRY WHERE AN ENTRY MAY REFUSE — a vector of tables,
 *  one of which may have left a required field out. Nothing when any
 *  entry refuses, because half a vector is not the vector the buffer
 *  claimed to carry; absent is the empty vector. */
template <class Element, class ReadEntry>
auto readEachOrNone(const flatbuffers::Vector<Element>* from, ReadEntry read) {
  using Entry = typename flatbuffers::Vector<Element>::return_type;
  using Answer = std::invoke_result_t<ReadEntry, Entry>;
  using Value = typename Answer::value_type;
  std::optional<std::vector<Value>> out(std::in_place);
  if (!from) return out;
  out->reserve(from->size());
  for (const Entry each : *from) {
    Answer one = read(each);
    if (!one) return std::optional<std::vector<Value>>();
    out->push_back(std::move(*one));
  }
  return out;
}

/** A string into @p into, whose offset a table field is written from. */
inline flatbuffers::Offset<flatbuffers::String> writeString(
    flatbuffers::FlatBufferBuilder& into, const std::string& value) {
  return into.CreateString(value);
}

/** A vector of strings into @p into. */
inline flatbuffers::Offset<
    flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>>
writeStrings(flatbuffers::FlatBufferBuilder& into,
             const std::vector<std::string>& value) {
  return into.CreateVectorOfStrings(value);
}

/** A vector of scalars into @p into. */
template <class Scalar>
flatbuffers::Offset<flatbuffers::Vector<Scalar>> writeScalars(
    flatbuffers::FlatBufferBuilder& into, const std::vector<Scalar>& value) {
  return into.CreateVector(value);
}

/** A vector of bools into @p into, one byte an entry. */
inline flatbuffers::Offset<flatbuffers::Vector<uint8_t>> writeBools(
    flatbuffers::FlatBufferBuilder& into, const std::vector<bool>& value) {
  return into.CreateVector(value);
}

/** A vector of enums into @p into as the integer the wire holds them
 *  in, which is the enum's underlying type. */
template <class Stored, class Enumerated>
flatbuffers::Offset<flatbuffers::Vector<Stored>> writeEnums(
    flatbuffers::FlatBufferBuilder& into,
    const std::vector<Enumerated>& value) {
  std::vector<Stored> each;
  each.reserve(value.size());
  for (const Enumerated one : value) each.push_back(static_cast<Stored>(one));
  return into.CreateVector(each);
}

/** A VECTOR OF STRUCTS into @p into, each value turned into the wire's
 *  struct by @p write. Structs are inline, so they are laid down as one
 *  block rather than as offsets. */
template <class Struct, class Value, class Write>
flatbuffers::Offset<flatbuffers::Vector<const Struct*>> writeStructs(
    flatbuffers::FlatBufferBuilder& into, const std::vector<Value>& value,
    Write write) {
  std::vector<Struct> each;
  each.reserve(value.size());
  for (const Value& one : value) each.push_back(write(one));
  return into.CreateVectorOfStructs<Struct>(each);
}

/** A VECTOR OF TABLES into @p into, each value written by @p write.
 *  Every table is written before the vector that points at them,
 *  because a builder lays its bytes down back to front and a vector
 *  cannot point at what is not there yet. */
template <class Value, class Write>
auto writeEach(flatbuffers::FlatBufferBuilder& into,
               const std::vector<Value>& value, Write write) {
  using Element = std::invoke_result_t<Write, flatbuffers::FlatBufferBuilder&,
                                       const Value&>;
  std::vector<Element> each;
  each.reserve(value.size());
  for (const Value& one : value) each.push_back(write(into, one));
  return into.CreateVector(each);
}

/** HOW ONE VALUE IS READ OUT OF BYTES, for a reader that names the
 *  value and not the reading. A specialization declares
 *
 *      static std::optional<Value> from(std::span<const std::byte> bytes);
 *
 *  which verifies the bytes against that value's root and answers
 *  nothing where they are not it. A generated header writes one for
 *  every table of its schema, beside the reading it stands on.
 *
 *  The primary template is left UNDEFINED, so a value nobody wrote a
 *  reading for is a name that cannot be completed rather than a reading
 *  that always answers nothing. */
template <class Value>
struct Read;

/** Whether Value has a reading of its own, which is what a door asks of
 *  the type it is handed before it reads a message as one. A struct has
 *  none — it travels inline inside a table and is never a root — and
 *  neither has a type from outside a schema. */
template <class Value>
concept Readable = requires(std::span<const std::byte> bytes) {
  { Read<Value>::from(bytes) } -> std::same_as<std::optional<Value>>;
};

}  // namespace sigil::data::values
