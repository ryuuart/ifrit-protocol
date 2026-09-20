#pragma once

/** @file
 * @ingroup data-decode
 * A FLATBUFFER AS A VALUE, and the decoder that puts one on a hub.
 *
 * There are two of them, for the two things a holder may know about the
 * message: `FlatBuffer<Root>` where a generated header names the root,
 * and `SchemaBuffer` where a schema token names it instead. The first
 * reads fields through the generated accessors; the second reads the
 * schema's own JSON form, which is all a holder with no generated
 * header can name a field by.
 *
 * A FlatBuffer is read in place: its root is a pointer into its bytes,
 * so the value IS the bytes, verified once against the schema and read
 * through the generated accessors from then on. The decoder answers a
 * Root for the bytes a hub hands it whether they are the buffer itself
 * or the schema's own JSON form — a scene written by hand, or sent by
 * something that speaks JSON — which the schema converts. After
 * `registerFlatBuffer<Root>(hub)`, `hub.load<FlatBuffer<Root>>(uri)` answers,
 * cached and reloaded like any resource the hub holds.
 *
 * THE SCHEMA COMES FROM THE GENERATED CODE. A header written with
 * `flatc --cpp -b --schema --bfbs-gen-embed` carries the binary schema
 * beside every root, as `Root::BinarySchema`, so nothing here reads a
 * schema file. A header written without it still decodes the buffer
 * itself; only the JSON form needs the schema, and a Root that carries
 * none refuses that form.
 *
 * Speaks io's byte vocabulary and flatbuffers, and nothing else of io:
 * `registerFlatBuffer` is a template over the hub. Of flatbuffers it
 * opens the buffer's own header alone — the verifier that checks bytes
 * and the accessor that reads a root out of them — because that is what
 * this value IS. The reader that converts the JSON form is named
 * nowhere here: the conversion goes through a schema token, which is one
 * pointer to a state defined out of sight.
 */

#include <flatbuffers/flatbuffers.h>
#include <sigildata/decode/Schema.h>
#include <sigilio/source/Source.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::data {

/** A FLATBUFFER OF Root, holding the bytes its root is read from. */
template <class Root>
class FlatBuffer {
 public:
  /** Takes ownership of @p bytes, which are read as they stand: verify
   *  them first with `flatBufferFromBytes`. */
  explicit FlatBuffer(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {}

  /** The root, read in place; every accessor on it reads the bytes. */
  [[nodiscard]] const Root* root() const {
    return flatbuffers::GetRoot<Root>(m_bytes.data());
  }
  /** The root, so a field reads straight off the value. */
  const Root* operator->() const { return root(); }

  /** The buffer as it stands: what a sink writes, or a feed sends. */
  [[nodiscard]] std::span<const uint8_t> bytes() const { return m_bytes; }

 private:
  std::vector<uint8_t> m_bytes;
};

/** @p bytes as a FlatBuffer of Root, verified before any of it is read.
 *  Nothing when they are not one — a truncated buffer, another schema —
 *  and @p why says so where it is asked for. */
template <class Root>
std::optional<FlatBuffer<Root>> flatBufferFromBytes(
    std::span<const uint8_t> bytes, std::string* why = nullptr) {
  flatbuffers::Verifier verifier(bytes.data(), bytes.size());
  if (!verifier.VerifyBuffer<Root>(nullptr)) {
    if (why) *why = "the bytes do not verify as the schema's root";
    return std::nullopt;
  }
  return FlatBuffer<Root>(std::vector<uint8_t>(bytes.begin(), bytes.end()));
}

/** @p json, the schema's own JSON form, converted through the schema the
 *  generated Root carries and verified as one. Nothing when the text
 *  does not fit the schema, and @p why carries the reader's own message
 *  — the line, the column and the field — where it is asked for. */
template <CarriesSchema Root>
std::optional<FlatBuffer<Root>> flatBufferFromJson(std::string_view json,
                                                   std::string* why = nullptr) {
  const std::optional<std::vector<std::byte>> buffer =
      schema<Root>().binary(json, why);
  if (!buffer) return std::nullopt;
  return flatBufferFromBytes<Root>(
      {reinterpret_cast<const uint8_t*>(buffer->data()), buffer->size()}, why);
}

/** Whether @p text is the JSON form rather than a buffer: read from the
 *  resource's name where it has one, and otherwise from its first byte
 *  that is not a space, since the JSON form opens with a brace or a
 *  bracket and a buffer does not. */
inline bool flatBufferLooksLikeJson(std::string_view text,
                                    std::string_view hint) {
  if (hint.size() >= 5 && hint.substr(hint.size() - 5) == ".json") return true;
  for (const char c : text) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
    return c == '{' || c == '[';
  }
  return false;
}

/** BYTES AS A FLATBUFFER OF Root, for a hub: the buffer verified, or the
 *  JSON form converted through the schema the Root carries. */
template <class Root>
struct FlatBufferDecoder {
  /** Reads @p bytes as a buffer or as the schema's JSON form, @p hint
   *  naming the resource. */
  std::optional<FlatBuffer<Root>> decode(const io::Bytes& bytes,
                                         std::string_view hint) const {
    if constexpr (CarriesSchema<Root>) {
      if (flatBufferLooksLikeJson(bytes.asText(), hint))
        return flatBufferFromJson<Root>(bytes.asText());
    }
    return flatBufferFromBytes<Root>(
        {reinterpret_cast<const uint8_t*>(bytes.bytes.data()),
         bytes.bytes.size()});
  }
};

/** Puts the decoder for Root on @p hub, so `load<FlatBuffer<Root>>` answers.
 *  Registering a Root again replaces the decoder later asks run. */
template <class Root, typename Hub>
void registerFlatBuffer(Hub& hub) {
  hub.template registerDecoder<FlatBuffer<Root>>(FlatBufferDecoder<Root>{});
}

/** A FLATBUFFER WHOSE ROOT A SCHEMA NAMES rather than a generated C++
 *  type: the same bytes, verified against that root through the schema
 *  token and read back through it.
 *
 *  It is what a holder that has no generated header carries — a tool
 *  handed a `.bfbs`, a process that compiles no header at all — and it
 *  answers the buffer's own JSON form rather than typed accessors,
 *  since the names of the fields are in the schema and nowhere in the
 *  message. Where the generated root IS in reach, `FlatBuffer<Root>`
 *  reads the same bytes field by field and converts nothing.
 *
 *  The value is the bytes, as a FlatBuffer is; the schema rides along
 *  because reading a field needs it, and costs the one pointer a
 *  token is. */
class SchemaBuffer {
 public:
  /** Takes ownership of @p bytes, which are read through @p schema as
   *  they stand: verify them first with `schemaBufferFromBytes`. */
  SchemaBuffer(Schema schema, std::vector<std::byte> bytes)
      : m_schema(std::move(schema)), m_bytes(std::move(bytes)) {}

  /** The schema the bytes are read through, to hand to whatever reads
   *  the next message. */
  [[nodiscard]] const Schema& schema() const { return m_schema; }

  /** The buffer as it stands: what a sink writes, or a feed sends. */
  [[nodiscard]] std::span<const std::byte> bytes() const { return m_bytes; }

  /** THE BUFFER AS THE SCHEMA'S OWN JSON FORM, which is how a holder
   *  with no generated accessors reads a field. Nothing where the bytes
   *  are not the root after all, and @p why says so where it is asked
   *  for. The form is made on each ask rather than held, the value
   *  being the bytes. */
  [[nodiscard]] std::optional<std::string> text(
      std::string* why = nullptr) const {
    return m_schema.text(m_bytes, why);
  }

  /** The same bytes read through a schema of the same root. The roots
   *  are compared and not the tokens, so one schema read twice out of a
   *  `.bfbs` is one schema here. */
  bool operator==(const SchemaBuffer& other) const {
    return m_bytes == other.m_bytes &&
           m_schema.rootName() == other.m_schema.rootName();
  }

 private:
  Schema m_schema;
  std::vector<std::byte> m_bytes;
};

/** @p bytes as a SchemaBuffer of @p schema's root, verified before any
 *  of it is read. Nothing when they are not one — a truncated buffer,
 *  another schema, a schema that is none — and @p why says so where it
 *  is asked for. */
inline std::optional<SchemaBuffer> schemaBufferFromBytes(
    Schema schema, std::span<const std::byte> bytes,
    std::string* why = nullptr) {
  if (!schema.verifies(bytes, why)) return std::nullopt;
  return SchemaBuffer(std::move(schema),
                      std::vector<std::byte>(bytes.begin(), bytes.end()));
}

/** @p json, @p schema's own JSON form, converted through it. Nothing
 *  when the text does not fit the schema, and @p why carries the
 *  reader's own message — the line, the column and the field — where it
 *  is asked for. What comes back has verified as the root, the
 *  conversion refusing anything that did not. */
inline std::optional<SchemaBuffer> schemaBufferFromJson(
    Schema schema, std::string_view json, std::string* why = nullptr) {
  std::optional<std::vector<std::byte>> buffer = schema.binary(json, why);
  if (!buffer) return std::nullopt;
  return SchemaBuffer(std::move(schema), std::move(*buffer));
}

/** BYTES AS A SCHEMABUFFER, for a hub: the buffer verified against the
 *  schema it carries, or that schema's own JSON form converted through
 *  it, which is the reading FlatBufferDecoder makes for a generated
 *  root. */
struct SchemaBufferDecoder {
  /** The schema every resource this decoder reads is read through. */
  Schema schema;

  /** Reads @p bytes as a buffer or as the schema's JSON form, @p hint
   *  naming the resource. */
  std::optional<SchemaBuffer> decode(const io::Bytes& bytes,
                                     std::string_view hint) const {
    if (flatBufferLooksLikeJson(bytes.asText(), hint))
      return schemaBufferFromJson(schema, bytes.asText());
    return schemaBufferFromBytes(schema, bytes.bytes);
  }
};

/** Puts the decoder for @p schema on @p hub, so `load<SchemaBuffer>`
 *  answers. A hub holds one decoder per type and a SchemaBuffer is one
 *  type however many schemas there are, so a second call replaces the
 *  schema later asks are read through: a hub reading two wires of
 *  different schemas at once wants a hub for each. */
template <typename Hub>
void registerSchemaBuffer(Hub& hub, Schema schema) {
  hub.template registerDecoder<SchemaBuffer>(
      SchemaBufferDecoder{std::move(schema)});
}

}  // namespace sigil::data
