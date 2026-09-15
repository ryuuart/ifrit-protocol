#pragma once

/** @file
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
 * AND THE SCHEMA ITSELF IS A VALUE. `schema<Root>()` is that schema as
 * one copyable token, which converts a buffer to its JSON form and a
 * JSON form back to a buffer without naming Root again — what a door
 * reading a wire holds, where the type of the next message is not known
 * at the call site.
 *
 * Speaks io's byte vocabulary and flatbuffers, and nothing else of io:
 * `registerFlatBuffer` is a template over the hub.
 */

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>
#include <flatbuffers/reflection.h>
#include <sigilio/source/Source.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
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
  explicit FlatBuffer(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {}

  /** The root, read in place; every accessor on it reads the bytes. */
  [[nodiscard]] const Root* root() const {
    return flatbuffers::GetRoot<Root>(m_bytes.data());
  }
  const Root* operator->() const { return root(); }

  /** The buffer as it stands: what a sink writes, or a feed sends. */
  [[nodiscard]] std::span<const uint8_t> bytes() const { return m_bytes; }

 private:
  std::vector<uint8_t> m_bytes;
};

/** Whether a generated Root carries its binary schema — a header written
 *  with the embed flag — which is what converting the JSON form needs. */
template <class Root>
concept CarriesSchema = requires {
  { Root::BinarySchema::data() } -> std::convertible_to<const uint8_t*>;
  { Root::BinarySchema::size() } -> std::convertible_to<size_t>;
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
 *  does not fit the schema, and @p why carries the parser's own message
 *  — the line, the column and the field — where it is asked for. */
template <CarriesSchema Root>
std::optional<FlatBuffer<Root>> flatBufferFromJson(std::string_view json,
                                                   std::string* why = nullptr) {
  flatbuffers::Parser parser;
  if (!parser.Deserialize(Root::BinarySchema::data(),
                          Root::BinarySchema::size())) {
    if (why) *why = "the generated schema does not load: " + parser.error_;
    return std::nullopt;
  }
  const std::string text(json);  // the parser reads a terminated string
  if (!parser.Parse(text.c_str())) {
    if (why) *why = parser.error_;
    return std::nullopt;
  }
  return flatBufferFromBytes<Root>(
      {parser.builder_.GetBufferPointer(), parser.builder_.GetSize()}, why);
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

/** A SCHEMA AS ONE VALUE: the two conversions a schema is, with the
 *  root type named once and never again.
 *
 *  `schema<Sky>()` makes one. The binary schema is deserialized once
 *  and held behind a shared pointer, so the token is copied for the
 *  cost of that pointer and every copy is the same schema: a scene
 *  keeps one in a field and hands it to whatever reads a wire.
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

  /** THE SCHEMA IN @p binarySchema — the bytes `flatc -b --schema`
   *  writes, which a generated header embeds. None when they are no
   *  schema, or when the schema declares no root type: a schema with no
   *  root has nothing to read a buffer AS. The bytes are copied, so the
   *  caller keeps nothing for this. */
  explicit Schema(std::span<const uint8_t> binarySchema) {
    auto state = std::make_shared<State>();
    state->bytes.assign(binarySchema.begin(), binarySchema.end());
    flatbuffers::Verifier verifier(state->bytes.data(), state->bytes.size());
    if (!reflection::VerifySchemaBuffer(verifier)) return;
    const reflection::Schema* reflected =
        reflection::GetSchema(state->bytes.data());
    const reflection::Object* root = reflected->root_table();
    if (!root || !root->name()) return;
    // THE JSON FORM IS ONE A JSON READER READS: field names quoted, no
    // line breaks, and every scalar the schema declares written even
    // where the buffer left it at the default, so a reader indexing a
    // field finds it whatever arrived. The options stand before the
    // schema is read, because they are what both conversions run under.
    state->parser.opts.strict_json = true;
    state->parser.opts.indent_step = -1;
    state->parser.opts.output_default_scalars_in_json = true;
    if (!state->parser.Deserialize(state->bytes.data(), state->bytes.size()))
      return;
    state->reflected = reflected;
    state->root = root;
    state->rootName = root->name()->str();
    m_state = std::move(state);
  }

  /** Whether this is a schema at all. */
  explicit operator bool() const { return m_state != nullptr; }

  /** THE BUFFER IN @p binary AS THE SCHEMA'S OWN JSON FORM. The bytes
   *  are verified against the root first, so a buffer of another
   *  schema, or one cut short, answers nothing rather than a reading of
   *  whatever the bytes happened to be; @p why says which where it is
   *  asked for. */
  std::optional<std::string> text(std::span<const std::byte> binary,
                                  std::string* why = nullptr) const {
    if (!m_state) {
      if (why) *why = "there is no schema to read the buffer through";
      return std::nullopt;
    }
    if (binary.empty()) {
      if (why) *why = "no bytes are no buffer";
      return std::nullopt;
    }
    const auto* first = reinterpret_cast<const uint8_t*>(binary.data());
    if (!flatbuffers::Verify(*m_state->reflected, *m_state->root, first,
                             binary.size())) {
      if (why) *why = "the bytes do not verify as " + m_state->rootName;
      return std::nullopt;
    }
    std::string form;
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (const char* trouble =
            flatbuffers::GenText(m_state->parser, first, &form)) {
      if (why) *why = trouble;
      return std::nullopt;
    }
    return form;
  }

  /** THE BUFFER @p json MAKES, read through the schema. Nothing where
   *  the text does not fit it — a field the schema does not declare, a
   *  value of the wrong type, text that is no document — and @p why
   *  carries the parser's own message, the line, the column and the
   *  field, where it is asked for. What comes back verifies as the
   *  root, so whoever is handed it may read it in place. */
  std::optional<std::vector<std::byte>> binary(
      std::string_view json, std::string* why = nullptr) const {
    if (!m_state) {
      if (why) *why = "there is no schema to read the text through";
      return std::nullopt;
    }
    const std::string text(json);  // the parser reads a terminated string
    const std::lock_guard<std::mutex> held(m_state->lock);
    if (!m_state->parser.Parse(text.c_str())) {
      if (why) *why = m_state->parser.error_;
      return std::nullopt;
    }
    const uint8_t* first = m_state->parser.builder_.GetBufferPointer();
    const size_t size = m_state->parser.builder_.GetSize();
    if (!flatbuffers::Verify(*m_state->reflected, *m_state->root, first,
                             size)) {
      if (why)
        *why = "what the text made does not verify as " + m_state->rootName;
      return std::nullopt;
    }
    const auto* bytes = reinterpret_cast<const std::byte*>(first);
    return std::vector<std::byte>(bytes, bytes + size);
  }

  /** THE ROOT BOTH CONVERSIONS GO THROUGH, fully qualified —
   *  `feed_sky.Sky`. Empty for a schema that is none. The view is into
   *  the schema and stands as long as it does. */
  std::string_view rootName() const {
    return m_state ? std::string_view(m_state->rootName) : std::string_view{};
  }

 private:
  /** The deserialized schema behind one pointer, so a copy of the token
   *  is the same schema rather than a second reading of it. */
  struct State {
    /** The binary schema itself, which the reflected pointers below
     *  read in place; it is written once, when the schema is made. */
    std::vector<uint8_t> bytes;
    flatbuffers::Parser parser;
    const reflection::Schema* reflected = nullptr;
    const reflection::Object* root = nullptr;
    std::string rootName;
    /** The parser holds one buffer and one message of its own, so a
     *  reading and a writing never run through it at once, however many
     *  threads hold the token. */
    std::mutex lock;
  };
  std::shared_ptr<State> m_state;
};

/** THE SCHEMA A GENERATED Root CARRIES, as one value:
 *  `schema<feed_sky::Sky>()`. The header the build wrote embeds its
 *  file's whole schema, so this reads no schema file. */
template <CarriesSchema Root>
Schema schema() {
  return Schema(std::span<const uint8_t>(Root::BinarySchema::data(),
                                         Root::BinarySchema::size()));
}

/** BYTES AS A FLATBUFFER OF Root, for a hub: the buffer verified, or the
 *  JSON form converted through the schema the Root carries. */
template <class Root>
struct FlatBufferDecoder {
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
