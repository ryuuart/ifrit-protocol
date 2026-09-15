#include <sigildata/decode/Decoders.h>

namespace sigil::data {

namespace {

bool endsWith(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         text.substr(text.size() - suffix.size()) == suffix;
}

/** Whether these bytes are a JSON document. The resource's name decides
 *  where it has one; otherwise the first character that is not a space
 *  does, since a JSON document begins with a bracket or a brace and a
 *  row of fields does not. */
bool looksLikeJson(std::string_view text, std::string_view hint) {
  if (endsWith(hint, ".json") || endsWith(hint, ".geojson")) return true;
  if (endsWith(hint, ".csv") || endsWith(hint, ".tsv") ||
      endsWith(hint, ".tab"))
    return false;
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    return c == '[' || c == '{';
  }
  return false;
}

std::string_view withoutMark(std::string_view text) {
  return text.starts_with("\xEF\xBB\xBF") ? text.substr(3) : text;
}

/** Whether a resource's name says its bytes are an OSC packet: the
 *  scheme a desk is reached at, or the extension a recorded packet is
 *  filed under. */
bool looksLikeOsc(std::string_view hint) {
  return hint.starts_with("osc://") || endsWith(hint, ".osc");
}

}  // namespace

std::optional<Table> TableDecoder::decode(const io::Bytes& bytes,
                                          std::string_view hint) const {
  const std::string_view text = withoutMark(bytes.asText());
  if (looksLikeJson(text, hint)) {
    const std::optional<Json> document = decodeJson(text);
    if (!document) return std::nullopt;
    return tableFromJson(*document);
  }
  return decodeCsv(text, csv, hint);
}

std::optional<Json> JsonDecoder::decode(const io::Bytes& bytes,
                                        std::string_view hint) const {
  // A packet is bytes and a document is text, and no reading of the
  // first byte tells the two apart, so the name is the whole of what
  // there is to go on.
  if (looksLikeOsc(hint)) return decodeOsc(bytes.bytes);
  return decodeJson(withoutMark(bytes.asText()));
}

}  // namespace sigil::data
