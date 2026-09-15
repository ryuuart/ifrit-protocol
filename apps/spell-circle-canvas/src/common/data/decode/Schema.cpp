/** @file
 * What a schema token stands on: the binary schema read once, and the
 * two conversions run through the parser that read it.
 *
 * The parser's headers are opened HERE and nowhere a consumer compiles
 * against: a schema is one pointer to the state below, so the header
 * that declares the token names none of this.
 */

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/idl.h>
#include <flatbuffers/reflection.h>
#include <sigildata/decode/Schema.h>

#include <cstdint>
#include <mutex>
#include <utility>

namespace sigil::data {

/** The deserialized schema behind one pointer, so a copy of the token
 *  is the same schema rather than a second reading of it. */
struct Schema::State {
  /** The binary schema itself, which the reflected pointers below read
   *  in place; it is written once, when the schema is made. */
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

Schema Schema::fromBinarySchema(std::span<const std::byte> bfbs,
                                std::string* why) {
  Schema made;
  if (bfbs.empty()) {
    if (why) *why = "no bytes are no schema";
    return made;
  }
  auto state = std::make_shared<State>();
  const auto* first = reinterpret_cast<const uint8_t*>(bfbs.data());
  state->bytes.assign(first, first + bfbs.size());
  flatbuffers::Verifier verifier(state->bytes.data(), state->bytes.size());
  if (!reflection::VerifySchemaBuffer(verifier)) {
    if (why) *why = "the bytes are no binary schema";
    return made;
  }
  const reflection::Schema* reflected =
      reflection::GetSchema(state->bytes.data());
  const reflection::Object* root = reflected->root_table();
  if (!root || !root->name()) {
    if (why) *why = "the schema declares no root type";
    return made;
  }
  // THE JSON FORM IS ONE A JSON READER READS: field names quoted, no
  // line breaks, and every scalar the schema declares written even
  // where the buffer left it at the default, so a reader indexing a
  // field finds it whatever arrived. The options stand before the
  // schema is read, because they are what both conversions run under.
  state->parser.opts.strict_json = true;
  state->parser.opts.indent_step = -1;
  state->parser.opts.output_default_scalars_in_json = true;
  if (!state->parser.Deserialize(state->bytes.data(), state->bytes.size())) {
    if (why) *why = state->parser.error_;
    return made;
  }
  state->reflected = reflected;
  state->root = root;
  state->rootName = root->name()->str();
  made.m_state = std::move(state);
  return made;
}

std::optional<std::string> Schema::text(std::span<const std::byte> binary,
                                        std::string* why) const {
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

std::optional<std::vector<std::byte>> Schema::binary(std::string_view json,
                                                     std::string* why) const {
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
  if (!flatbuffers::Verify(*m_state->reflected, *m_state->root, first, size)) {
    if (why)
      *why = "what the text made does not verify as " + m_state->rootName;
    return std::nullopt;
  }
  const auto* bytes = reinterpret_cast<const std::byte*>(first);
  return std::vector<std::byte>(bytes, bytes + size);
}

std::string_view Schema::rootName() const {
  return m_state ? std::string_view(m_state->rootName) : std::string_view{};
}

}  // namespace sigil::data
