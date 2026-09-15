/** @file
 * The seven readings of a message — the bytes themselves, the text they
 * may be, the document, the packet, the instrument's message or the
 * desk's universe they may be, and the form a schema reads them as —
 * the word a wire's scheme speaks in, and the short spelling of an
 * address.
 */

#include "sigilseer/wire/Rendering.h"

#include <sigildata/decode/ArtNet.h>
#include <sigildata/decode/FlatBuffer.h>
#include <sigildata/decode/Json.h>
#include <sigildata/decode/Midi.h>
#include <sigildata/decode/Osc.h>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::seer {
namespace {

constexpr char kDigits[] = "0123456789abcdef";

/** How deep one step of an indented document is. */
constexpr size_t kIndentWidth = 2;

/** HOW A VALUE IS LAID OUT.
 *
 *  A list of numbers is sometimes a run along a wire rather than a list
 *  of separate values — the bytes of a message, the dimmers of a
 *  universe — and a reader counts through such a run across the page,
 *  not down a column of one number to a line. `numbersPerRow` is how
 *  many of them stand on one line where every member of the list is a
 *  number: a run that fits a line stands whole beside its key, and a
 *  longer one is rows of that many. Zero is one member to a line, which
 *  is every other list and every record. */
struct Layout {
  size_t numbersPerRow = 0;
};

/** How many numbers of a run stand on one line. Sixteen is what a
 *  lighting desk's own channel numbers are counted in and what a row of
 *  levels is read against, and it is narrow enough that a universe is a
 *  block rather than a column. */
constexpr size_t kNumbersPerRow = 16;

/** Whether every member of @p items is a number, which is what makes a
 *  list a run along a wire rather than a list of values of its own. */
bool allNumbers(std::span<const data::Json> items) {
  for (const data::Json& item : items)
    if (item.kind() != data::Json::Kind::Number) return false;
  return true;
}

/** THE CODE POINT AT @p index, and how many bytes it took, or nothing
 *  when what stands there is not well-formed UTF-8. Overlong forms, the
 *  surrogate range and anything above the last code point are refused,
 *  so a decoder that accepted them could not disagree with this about
 *  what the text says. */
std::optional<std::pair<char32_t, size_t>> codePointAt(
    std::span<const std::byte> bytes, size_t index) {
  const auto octet = [&bytes](size_t at) {
    return static_cast<uint8_t>(bytes[at]);
  };
  const uint8_t first = octet(index);
  size_t length = 0;
  char32_t point = 0;
  if (first < 0x80) {
    return std::pair<char32_t, size_t>{first, 1};
  } else if ((first & 0xE0) == 0xC0) {
    length = 2;
    point = first & 0x1F;
  } else if ((first & 0xF0) == 0xE0) {
    length = 3;
    point = first & 0x0F;
  } else if ((first & 0xF8) == 0xF0) {
    length = 4;
    point = first & 0x07;
  } else {
    return std::nullopt;
  }
  if (index + length > bytes.size()) return std::nullopt;
  for (size_t step = 1; step != length; ++step) {
    const uint8_t continuation = octet(index + step);
    if ((continuation & 0xC0) != 0x80) return std::nullopt;
    point = (point << 6) | (continuation & 0x3F);
  }
  const bool overlong = (length == 2 && point < 0x80) ||
                        (length == 3 && point < 0x800) ||
                        (length == 4 && point < 0x10000);
  if (overlong || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
    return std::nullopt;
  return std::pair<char32_t, size_t>{point, length};
}

/** Whether @p point is one a reader can be shown: everything but the
 *  control characters, of which the three that lay a message out are
 *  kept. */
bool showable(char32_t point) {
  if (point == '\t' || point == '\n' || point == '\r') return true;
  if (point < 0x20 || point == 0x7F) return false;
  // The second control block, which is where a byte of some other
  // encoding read as UTF-8 most often lands.
  return !(point >= 0x80 && point <= 0x9F);
}

void appendNumber(std::string& out, double value) {
  char printed[32];
  const std::to_chars_result written =
      std::to_chars(printed, printed + sizeof printed, value);
  if (written.ec != std::errc())
    out += "null";
  else
    out.append(printed, written.ptr);
}

void appendQuoted(std::string& out, std::string_view text) {
  out += '"';
  for (const char character : text) {
    const auto raw = static_cast<uint8_t>(character);
    switch (character) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (raw < 0x20) {
          out += "\\u00";
          out += kDigits[raw >> 4];
          out += kDigits[raw & 0x0F];
        } else {
          out += character;
        }
    }
  }
  out += '"';
}

void appendValue(std::string& out, const data::Json& value, size_t depth,
                 const Layout& layout) {
  const std::string inner((depth + 1) * kIndentWidth, ' ');
  const std::string outer(depth * kIndentWidth, ' ');
  switch (value.kind()) {
    case data::Json::Kind::Null:
      out += "null";
      break;
    case data::Json::Kind::Boolean:
      out += value.boolean() ? "true" : "false";
      break;
    case data::Json::Kind::Number:
      appendNumber(out, value.number());
      break;
    case data::Json::Kind::Text:
      appendQuoted(out, value.text());
      break;
    case data::Json::Kind::List: {
      const std::span<const data::Json> items = value.items();
      if (items.empty()) {
        out += "[]";
        break;
      }
      if (layout.numbersPerRow != 0 && allNumbers(items)) {
        // A run that fits one row is the run, and stands beside the key
        // it belongs to: two bytes of a message broken over four lines
        // are four lines saying what one says.
        if (items.size() <= layout.numbersPerRow) {
          out += '[';
          for (size_t at = 0; at != items.size(); ++at) {
            if (at != 0) out += ", ";
            appendNumber(out, items[at].number());
          }
          out += ']';
          break;
        }
        // A longer run is rows of that many, each row opening at the
        // indent a member would and standing under the row above it, so
        // a reader counts a number's place off its row.
        out += "[\n";
        for (size_t at = 0; at != items.size(); ++at) {
          if (at % layout.numbersPerRow == 0) out += inner;
          appendNumber(out, items[at].number());
          const bool last = at + 1 == items.size();
          if (!last) out += ',';
          if (last || (at + 1) % layout.numbersPerRow == 0)
            out += '\n';
          else
            out += ' ';
        }
        out += outer;
        out += ']';
        break;
      }
      out += "[\n";
      for (size_t at = 0; at != items.size(); ++at) {
        out += inner;
        appendValue(out, items[at], depth + 1, layout);
        out += at + 1 == items.size() ? "\n" : ",\n";
      }
      out += outer;
      out += ']';
      break;
    }
    case data::Json::Kind::Record: {
      const std::span<const std::pair<std::string, data::Json>> fields =
          value.fields();
      if (fields.empty()) {
        out += "{}";
        break;
      }
      out += "{\n";
      for (size_t at = 0; at != fields.size(); ++at) {
        out += inner;
        appendQuoted(out, fields[at].first);
        out += ": ";
        appendValue(out, fields[at].second, depth + 1, layout);
        out += at + 1 == fields.size() ? "\n" : ",\n";
      }
      out += outer;
      out += '}';
      break;
    }
  }
}

/** @p value written out indented, laid out as @p layout says. One
 *  printer serves every reading alike: a reader turning from one to the
 *  other is reading the same kind of value and should not have to read
 *  two layouts to see it. */
std::string indented(const data::Json& value, const Layout& layout = {}) {
  std::string out;
  appendValue(out, value, 0, layout);
  return out;
}

}  // namespace

std::string hexadecimal(const io::Bytes& bytes, size_t limit) {
  const size_t shown = bytes.bytes.size() < limit ? bytes.bytes.size() : limit;
  std::string out;
  out.reserve(shown * 3);
  for (size_t at = 0; at != shown; ++at) {
    if (at != 0) out += ' ';
    const auto octet = static_cast<uint8_t>(bytes.bytes[at]);
    out += kDigits[octet >> 4];
    out += kDigits[octet & 0x0F];
  }
  if (shown < bytes.bytes.size()) out += " …";
  return out;
}

std::string printableText(const io::Bytes& bytes) {
  const std::span<const std::byte> span(bytes.bytes);
  for (size_t at = 0; at < span.size();) {
    const std::optional<std::pair<char32_t, size_t>> read =
        codePointAt(span, at);
    if (!read || !showable(read->first)) return {};
    at += read->second;
  }
  return std::string(bytes.asText());
}

std::string indentedJson(const io::Bytes& bytes) {
  const std::optional<data::Json> document = data::decodeJson(bytes.asText());
  if (!document) return {};
  return indented(*document);
}

std::string oscReading(const io::Bytes& bytes) {
  const std::optional<data::Json> packet = data::decodeOsc(bytes.bytes);
  if (!packet) return {};
  return indented(*packet);
}

std::string midiReading(const io::Bytes& bytes) {
  const std::optional<data::Json> message = data::decodeMidi(bytes.bytes);
  if (!message) return {};
  return indented(*message, {kNumbersPerRow});
}

std::string dmxReading(const io::Bytes& bytes) {
  const std::optional<data::Json> packet = data::decodeArtNet(bytes.bytes);
  if (!packet) return {};
  return indented(*packet, {kNumbersPerRow});
}

std::string schemaReading(const io::Bytes& bytes, const data::Schema& schema,
                          std::string* why) {
  if (why) why->clear();
  if (!schema) {
    if (why) *why = "no schema is loaded to read the message through";
    return {};
  }
  // A buffer and the schema's own form share no first byte, so which of
  // them arrived is knowable before either is read — and a message that
  // is neither is left with the sentence of the reading it was nearest,
  // which is the one that names what is wrong with it.
  std::string trouble;
  std::optional<std::string> form;
  if (data::flatBufferLooksLikeJson(bytes.asText(), {})) {
    if (const std::optional<std::vector<std::byte>> built =
            schema.binary(bytes.asText(), &trouble))
      form = schema.text(*built, &trouble);
  } else {
    form = schema.text(bytes.bytes, &trouble);
  }
  if (!form) {
    if (why) *why = trouble;
    return {};
  }
  // The schema writes its form on one line. It is laid out here through
  // the same printer the document and the packet go through, so a reader
  // turning from one reading to another reads what differs rather than
  // how each was printed.
  const std::optional<data::Json> document = data::decodeJson(*form);
  return document ? indented(*document) : *form;
}

std::string_view dialect(std::string_view uri) {
  const size_t mark = uri.find("://");
  if (mark == std::string_view::npos) return {};
  const std::string_view scheme = uri.substr(0, mark);
  // What the scheme SAYS its messages are, which is the whole of what
  // is known before one arrives. Two schemes carry anything a sender
  // writes and say so; the rest name a format, and a socket under two
  // names is two wires speaking two things.
  if (scheme == "osc") return "osc";
  if (scheme == "midi") return "midi";
  if (scheme == "artnet") return "dmx";
  if (scheme == "serial") return "lines";
  if (scheme == "ws" || scheme == "wss") return "json";
  if (scheme == "udp" || scheme == "shm") return "bytes";
  return {};
}

std::string hostAndPort(std::string_view uri) {
  const size_t mark = uri.find("://");
  if (mark == std::string_view::npos) return std::string(uri);
  return std::string(uri.substr(mark + 3));
}

}  // namespace sigil::seer
