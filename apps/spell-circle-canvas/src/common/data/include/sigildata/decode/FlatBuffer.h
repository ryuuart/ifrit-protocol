#pragma once

/** @file
 * @ingroup data-decode
 * A FLATBUFFER AS A VALUE, and the decoder that puts one on a hub.
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

}  // namespace sigil::data
