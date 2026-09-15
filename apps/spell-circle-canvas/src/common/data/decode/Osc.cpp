#include <sigildata/decode/Osc.h>

#include <bit>
#include <cstdint>
#include <string>
#include <utility>

namespace sigil::data {

namespace {

/** The fractions of a second a timetag counts in: a timetag is a 64-bit
 *  count of two to the minus thirty-second of a second since the start
 *  of 1900. */
constexpr double timeTagTicksPerSecond = 4294967296.0;

/** The one timetag the protocol spends on a word rather than a time: a
 *  bundle carrying it is for now. */
constexpr uint64_t immediateTimeTag = 1;

/** The whole numbers a 32-bit integer holds. */
constexpr double smallestInt32 = -2147483648.0;
constexpr double largestInt32 = 2147483647.0;

/** The word a bundle opens with, which is what tells one from a
 *  message. */
constexpr std::string_view bundleMark = "#bundle";

/** The bytes an OSC string of @p length characters takes: its
 *  characters, the null that ends it, and nulls up to the next multiple
 *  of four. */
size_t paddedLength(size_t length) { return (length + 4) & ~size_t{3}; }

/** @p length rounded up to the multiple of four the wire aligns to. */
size_t roundedUp(size_t length) { return (length + 3) & ~size_t{3}; }

/** @p value held inside [@p low, @p high]. A number outside what its
 *  place on the wire holds is written at the nearer end, since wrapping
 *  it would put a value there that nobody meant; a number that is not a
 *  number lands at the low end, having no side to be on. */
double clamped(double value, double low, double high) {
  if (!(value > low)) return low;
  return value > high ? high : value;
}

/** The four bytes of a packed argument, most significant first, which
 *  is the order the wire writes them in. */
Json fourBytes(uint32_t packed) {
  return Json(Json::Array{Json((packed >> 24) & 0xFFu),
                          Json((packed >> 16) & 0xFFu),
                          Json((packed >> 8) & 0xFFu), Json(packed & 0xFFu)});
}

/** @p bytes, a list of four byte members, packed back into the number
 *  the wire carries. A member that is not there is zero, so a record
 *  written short still names a colour. */
uint32_t packedBytes(const Json& bytes) {
  uint32_t packed = 0;
  for (size_t at = 0; at < 4; ++at)
    packed = (packed << 8) |
             static_cast<uint32_t>(clamped(bytes[at].number(), 0.0, 255.0));
  return packed;
}

// Reading the wire. Every length on it is a claim the bytes make about
// themselves, so each reading answers nothing rather than reaching past
// what arrived.

/** The four bytes at @p at, most significant first, or nothing when
 *  they are not all inside @p bytes. */
std::optional<uint32_t> wordAt(std::span<const std::byte> bytes, size_t at) {
  if (bytes.size() < 4 || at > bytes.size() - 4) return std::nullopt;
  uint32_t word = 0;
  for (size_t step = 0; step < 4; ++step)
    word = (word << 8) | std::to_integer<uint32_t>(bytes[at + step]);
  return word;
}

/** The eight bytes at @p at, most significant first, or nothing when
 *  they are not all inside @p bytes. */
std::optional<uint64_t> giantAt(std::span<const std::byte> bytes, size_t at) {
  if (bytes.size() < 8 || at > bytes.size() - 8) return std::nullopt;
  uint64_t giant = 0;
  for (size_t step = 0; step < 8; ++step)
    giant = (giant << 8) | std::to_integer<uint64_t>(bytes[at + step]);
  return giant;
}

/** The OSC string at @p at — its characters up to the null that ends
 *  it, with @p at left past the nulls that pad it out to a multiple of
 *  four — or nothing when no null stands before the end of @p bytes,
 *  since a string that never ends is a length @p bytes does not hold.
 *  The characters are a view onto @p bytes and live as long as it. */
std::optional<std::string_view> stringAt(std::span<const std::byte> bytes,
                                         size_t& at) {
  for (size_t end = at; end < bytes.size(); ++end) {
    if (bytes[end] != std::byte{0}) continue;
    const size_t after = at + paddedLength(end - at);
    if (after > bytes.size()) return std::nullopt;
    const std::string_view text(
        reinterpret_cast<const char*>(bytes.data()) + at, end - at);
    at = after;
    return text;
  }
  return std::nullopt;
}

/** Whether @p element opens with the word a bundle is named by. */
bool marksBundle(std::span<const std::byte> element) {
  if (element.size() <= bundleMark.size()) return false;
  for (size_t step = 0; step < bundleMark.size(); ++step)
    if (std::to_integer<char>(element[step]) != bundleMark[step]) return false;
  return element[bundleMark.size()] == std::byte{0};
}

/** A four-byte argument as the value its type tag says it is. */
Json wordAsJson(char tag, uint32_t word) {
  switch (tag) {
    case 'i':
      return Json(static_cast<int32_t>(word));
    case 'f':
      return Json(static_cast<double>(std::bit_cast<float>(word)));
    case 'c':
      // A character rides in a word, and its code is what a drawing can
      // do anything with.
      return Json(word & 0xFFu);
    case 'r':
      return Json(Json::Object{{"rgba", fourBytes(word)}});
    case 'm':
      return Json(Json::Object{{"midi", fourBytes(word)}});
    default:
      return Json();
  }
}

/** An eight-byte argument as the value its type tag says it is. */
Json giantAsJson(char tag, uint64_t giant) {
  switch (tag) {
    case 'h':
      return Json(static_cast<int64_t>(giant));
    case 'd':
      return Json(std::bit_cast<double>(giant));
    case 't':
      return Json(static_cast<double>(giant) / timeTagTicksPerSecond);
    default:
      return Json();
  }
}

/** The arguments @p tags names, read from @p at in @p message, or
 *  nothing when a tag has no argument under it, when an array does not
 *  close where it opened, or when a tag names a length this cannot
 *  measure and so cannot step over. */
std::optional<Json::Array> argumentsFrom(std::span<const std::byte> message,
                                         std::string_view tags, size_t at) {
  // The arrays the arguments are nested in: an argument joins the
  // innermost one, and closing an array makes it a member of the one
  // outside. The outermost is the message's own argument list.
  std::vector<Json::Array> open(1);
  for (const char tag : tags) {
    Json value;
    switch (tag) {
      case '[':
        if (open.size() > static_cast<size_t>(maxOscBundleDepth))
          return std::nullopt;
        open.emplace_back();
        continue;
      case ']': {
        if (open.size() == 1) return std::nullopt;
        Json closed(std::move(open.back()));
        open.pop_back();
        open.back().push_back(std::move(closed));
        continue;
      }
      case 'T':
      case 'I':
        // An impulse carries nothing but the fact that it arrived, and
        // the plainest reading of that is the true it stands for.
        value = Json(true);
        break;
      case 'F':
        value = Json(false);
        break;
      case 'N':
        break;
      case 'i':
      case 'f':
      case 'c':
      case 'r':
      case 'm': {
        const std::optional<uint32_t> word = wordAt(message, at);
        if (!word) return std::nullopt;
        at += 4;
        value = wordAsJson(tag, *word);
        break;
      }
      case 'h':
      case 'd':
      case 't': {
        const std::optional<uint64_t> giant = giantAt(message, at);
        if (!giant) return std::nullopt;
        at += 8;
        value = giantAsJson(tag, *giant);
        break;
      }
      case 's':
      case 'S': {
        const std::optional<std::string_view> text = stringAt(message, at);
        if (!text) return std::nullopt;
        value = Json(std::string(*text));
        break;
      }
      case 'b': {
        const std::optional<uint32_t> length = wordAt(message, at);
        if (!length) return std::nullopt;
        at += 4;
        const size_t span = roundedUp(*length);
        if (span > message.size() - at) return std::nullopt;
        Json::Array bytes;
        bytes.reserve(*length);
        for (uint32_t step = 0; step < *length; ++step)
          bytes.push_back(Json(std::to_integer<uint8_t>(message[at + step])));
        at += span;
        value = Json(Json::Object{{"blob", Json(std::move(bytes))}});
        break;
      }
      default:
        // A tag this does not know names a length this cannot measure,
        // so every argument after it stands somewhere unknown and the
        // message cannot be read at all.
        return std::nullopt;
    }
    open.back().push_back(std::move(value));
  }
  if (open.size() != 1) return std::nullopt;
  return std::move(open.front());
}

/** A message as its address and its arguments, or nothing when @p
 *  message is not one. */
std::optional<Json> messageFrom(std::span<const std::byte> message) {
  size_t at = 0;
  const std::optional<std::string_view> address = stringAt(message, at);
  // An address pattern opens with a slash, which is the one mark that
  // tells a message from a run of bytes that happens to hold a null.
  if (!address || !address->starts_with('/')) return std::nullopt;

  Json::Array arguments;
  // A message may stop after its address: a sender that writes no type
  // tag string means a message with no arguments.
  if (at < message.size()) {
    const std::optional<std::string_view> tags = stringAt(message, at);
    if (!tags || !tags->starts_with(',')) return std::nullopt;
    std::optional<Json::Array> read =
        argumentsFrom(message, tags->substr(1), at);
    if (!read) return std::nullopt;
    arguments = std::move(*read);
  }
  return Json(Json::Object{{"address", Json(std::string(*address))},
                           {"arguments", Json(std::move(arguments))}});
}

/** A bundle as its elements and the time they are for, its own bundles
 *  nested inside it. @p depth is how many bundles stand above this one.
 */
std::optional<Json> bundleFrom(std::span<const std::byte> bundle, int depth) {
  if (depth >= maxOscBundleDepth) return std::nullopt;
  size_t at = 0;
  const std::optional<std::string_view> mark = stringAt(bundle, at);
  if (!mark || *mark != bundleMark) return std::nullopt;
  const std::optional<uint64_t> tag = giantAt(bundle, at);
  if (!tag) return std::nullopt;
  at += 8;

  Json::Array elements;
  while (at < bundle.size()) {
    const std::optional<uint32_t> length = wordAt(bundle, at);
    if (!length) return std::nullopt;
    at += 4;
    // An element's length is the packet's own claim about itself, and
    // the protocol aligns every element to four, so a length that is
    // not a multiple of four is no length the packet meant.
    if (*length == 0 || *length % 4 != 0 || *length > bundle.size() - at)
      return std::nullopt;
    const std::span<const std::byte> element = bundle.subspan(at, *length);
    at += *length;
    std::optional<Json> read = marksBundle(element)
                                   ? bundleFrom(element, depth + 1)
                                   : messageFrom(element);
    if (!read) return std::nullopt;
    elements.push_back(std::move(*read));
  }
  return Json(Json::Object{
      {"bundle", Json(std::move(elements))},
      {"at", *tag == immediateTimeTag
                 ? Json()
                 : Json(static_cast<double>(*tag) / timeTagTicksPerSecond)}});
}

// Writing the wire.

/** @p value's four bytes onto @p bytes, most significant first. */
void appendWord(std::vector<std::byte>& bytes, uint32_t value) {
  for (int shift = 24; shift >= 0; shift -= 8)
    bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
}

/** @p value's eight bytes onto @p bytes, most significant first. */
void appendGiant(std::vector<std::byte>& bytes, uint64_t value) {
  appendWord(bytes, static_cast<uint32_t>(value >> 32));
  appendWord(bytes, static_cast<uint32_t>(value));
}

/** @p text onto @p bytes as an OSC string: its characters, the null
 *  that ends it, and nulls up to the next multiple of four. */
void appendString(std::vector<std::byte>& bytes, std::string_view text) {
  for (const char letter : text)
    bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
  for (size_t pad = paddedLength(text.size()) - text.size(); pad > 0; --pad)
    bytes.push_back(std::byte{0});
}

/** @p blob's byte members onto @p bytes as an OSC blob: its own length,
 *  its bytes, and nulls up to the next multiple of four. */
void appendBlob(std::vector<std::byte>& bytes, const Json& blob) {
  appendWord(bytes, static_cast<uint32_t>(blob.size()));
  for (const Json& value : blob.items())
    bytes.push_back(static_cast<std::byte>(
        static_cast<unsigned char>(clamped(value.number(), 0.0, 255.0))));
  for (size_t pad = roundedUp(blob.size()) - blob.size(); pad > 0; --pad)
    bytes.push_back(std::byte{0});
}

/** A record as the argument its one key names. A record with none of
 *  those keys has no type on the wire and writes as nil, so the
 *  arguments keep their count and a reader's positions do not shift. */
void writeRecord(const Json& record, std::string& tags,
                 std::vector<std::byte>& content) {
  if (const Json& blob = record["blob"]; blob.kind() == Json::Kind::List) {
    tags.push_back('b');
    appendBlob(content, blob);
    return;
  }
  if (const Json& colour = record["rgba"]; colour.kind() == Json::Kind::List) {
    tags.push_back('r');
    appendWord(content, packedBytes(colour));
    return;
  }
  if (const Json& midi = record["midi"]; midi.kind() == Json::Kind::List) {
    tags.push_back('m');
    appendWord(content, packedBytes(midi));
    return;
  }
  if (const Json& whole = record["int"]; whole.kind() == Json::Kind::Number) {
    tags.push_back('i');
    appendWord(content, static_cast<uint32_t>(static_cast<int32_t>(clamped(
                            whole.number(), smallestInt32, largestInt32))));
    return;
  }
  if (const Json& wide = record["double"]; wide.kind() == Json::Kind::Number) {
    tags.push_back('d');
    appendGiant(content, std::bit_cast<uint64_t>(wide.number()));
    return;
  }
  tags.push_back('N');
}

/** @p arguments onto @p tags and @p content, which are built together
 *  because the type tag string goes on the wire before the arguments it
 *  names and neither is known until every argument is classified. False
 *  when the nesting runs deeper than a packet may, which is no packet
 *  at all rather than a packet with its innermost list dropped. */
bool writeArguments(std::span<const Json> arguments, int depth,
                    std::string& tags, std::vector<std::byte>& content) {
  if (depth >= maxOscBundleDepth) return false;
  for (const Json& argument : arguments) {
    switch (argument.kind()) {
      case Json::Kind::Null:
        tags.push_back('N');
        break;
      case Json::Kind::Boolean:
        tags.push_back(argument.boolean() ? 'T' : 'F');
        break;
      case Json::Kind::Number:
        // A number carries no memory of the width it arrived at, and
        // what a performance tool expects under a continuous control is
        // a float; a value wanting another width asks for it by name.
        tags.push_back('f');
        appendWord(content, std::bit_cast<uint32_t>(
                                static_cast<float>(argument.number())));
        break;
      case Json::Kind::Text:
        tags.push_back('s');
        appendString(content, argument.text());
        break;
      case Json::Kind::List:
        tags.push_back('[');
        if (!writeArguments(argument.items(), depth + 1, tags, content))
          return false;
        tags.push_back(']');
        break;
      case Json::Kind::Record:
        writeRecord(argument, tags, content);
        break;
    }
  }
  return true;
}

}  // namespace

std::optional<Json> decodeOsc(std::span<const std::byte> packet) {
  // Every element on the wire is aligned to four, the packet itself
  // included, so a length that is not is not a packet.
  if (packet.empty() || packet.size() % 4 != 0) return std::nullopt;
  if (marksBundle(packet)) return bundleFrom(packet, 0);
  return messageFrom(packet);
}

std::vector<std::byte> encodeOsc(std::string_view address,
                                 const Json& arguments) {
  // An address pattern opens with a slash, and a message without one is
  // addressed to nothing.
  if (!address.starts_with('/')) return {};
  // A value that is not a list is the one argument it is.
  const std::span<const Json> list = arguments.kind() == Json::Kind::List
                                         ? arguments.items()
                                         : std::span<const Json>(&arguments, 1);
  std::string tags(1, ',');
  std::vector<std::byte> content;
  if (!writeArguments(list, 0, tags, content)) return {};

  const size_t size =
      paddedLength(address.size()) + paddedLength(tags.size()) + content.size();
  if (size > maxOscPacketBytes) return {};
  std::vector<std::byte> packet;
  packet.reserve(size);
  appendString(packet, address);
  appendString(packet, tags);
  packet.insert(packet.end(), content.begin(), content.end());
  return packet;
}

std::vector<std::byte> encodeOsc(const Json& message) {
  const std::string_view address = message["address"].text();
  const Json& arguments = message["arguments"];
  // A message with no arguments member is a message with no arguments,
  // where a lone value that is not a list would be one argument.
  if (arguments.null()) return encodeOsc(address, Json(Json::Array{}));
  return encodeOsc(address, arguments);
}

}  // namespace sigil::data
