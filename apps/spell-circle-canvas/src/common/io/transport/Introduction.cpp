/** @file
 * The introductions a signalling door carries, read out of their JSON
 * text and written back into it.
 */

#include "Introduction.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sigil::io::detail {
namespace {

/** The field of @p message that @p name spells, or null where this door
 *  reads no field of that name. */
std::string* fieldOf(Introduction& message, std::string_view name) {
  if (name == "kind") return &message.kind;
  if (name == "room") return &message.room;
  if (name == "sdp") return &message.sdp;
  if (name == "candidate") return &message.candidate;
  if (name == "mid") return &message.mid;
  return nullptr;
}

/** Past every space from @p at: what stands between the pieces of a
 *  JSON text and says nothing. */
size_t pastSpaces(std::string_view text, size_t at) {
  while (at != text.size() && (text[at] == ' ' || text[at] == '\t' ||
                               text[at] == '\r' || text[at] == '\n'))
    ++at;
  return at;
}

/** @p code as the bytes UTF-8 writes it in, appended to @p out. */
void appendUtf8(std::string& out, std::uint32_t code) {
  if (code < 0x80) {
    out.push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else if (code < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (code >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  }
}

/** The four hexadecimal digits at @p at as a number, or nothing where
 *  four of them do not stand there. */
std::optional<std::uint32_t> hexAt(std::string_view text, size_t at) {
  if (at + 4 > text.size()) return std::nullopt;
  std::uint32_t value = 0;
  for (size_t index = at; index != at + 4; ++index) {
    const char digit = text[index];
    value <<= 4;
    if (digit >= '0' && digit <= '9')
      value |= static_cast<std::uint32_t>(digit - '0');
    else if (digit >= 'a' && digit <= 'f')
      value |= static_cast<std::uint32_t>(digit - 'a' + 10);
    else if (digit >= 'A' && digit <= 'F')
      value |= static_cast<std::uint32_t>(digit - 'A' + 10);
    else
      return std::nullopt;
  }
  return value;
}

/** A string read out of a JSON text: what it says, where it ended, and
 *  whether it ended at all. */
struct ReadString {
  std::string value;
  size_t next = 0;
  bool whole = false;
};

/** The string whose opening quote stands at @p at, with every escape
 *  read back, and where the closing quote left off. Not whole where the
 *  closing quote never comes or an escape names nothing: a description
 *  read out of half a text is not the description its sender wrote. */
ReadString readString(std::string_view text, size_t at) {
  ReadString read;
  if (at == text.size() || text[at] != '"') return read;
  ++at;
  while (at != text.size()) {
    const char letter = text[at];
    if (letter == '"') {
      read.next = at + 1;
      read.whole = true;
      return read;
    }
    if (letter != '\\') {
      read.value.push_back(letter);
      ++at;
      continue;
    }
    if (at + 1 == text.size()) return read;
    const char escape = text[at + 1];
    at += 2;
    switch (escape) {
      case '"':
        read.value.push_back('"');
        break;
      case '\\':
        read.value.push_back('\\');
        break;
      case '/':
        read.value.push_back('/');
        break;
      case 'b':
        read.value.push_back('\b');
        break;
      case 'f':
        read.value.push_back('\f');
        break;
      case 'n':
        read.value.push_back('\n');
        break;
      case 'r':
        read.value.push_back('\r');
        break;
      case 't':
        read.value.push_back('\t');
        break;
      case 'u': {
        const std::optional<std::uint32_t> first = hexAt(text, at);
        if (!first) return read;
        at += 4;
        std::uint32_t code = *first;
        // A character past the first sixty-five thousand is written as
        // two escapes, and is the one character they spell together.
        if (code >= 0xD800 && code <= 0xDBFF && at + 1 < text.size() &&
            text[at] == '\\' && text[at + 1] == 'u') {
          if (const std::optional<std::uint32_t> second = hexAt(text, at + 2);
              second && *second >= 0xDC00 && *second <= 0xDFFF) {
            code = 0x10000 + ((code - 0xD800) << 10) + (*second - 0xDC00);
            at += 6;
          }
        }
        appendUtf8(read.value, code);
        break;
      }
      default:
        return read;
    }
  }
  return read;
}

/** Where the value beginning at @p at ends, whatever kind of value it
 *  is: a string, a number, a literal, or an object or an array with
 *  everything nested inside it. Zero where it does not end, which is a
 *  text no field can be trusted to have been read out of. */
size_t pastValue(std::string_view text, size_t at) {
  at = pastSpaces(text, at);
  if (at == text.size()) return 0;
  if (text[at] == '"') {
    const ReadString read = readString(text, at);
    return read.whole ? read.next : 0;
  }
  if (text[at] == '{' || text[at] == '[') {
    int depth = 0;
    while (at != text.size()) {
      const char letter = text[at];
      if (letter == '"') {
        // A brace inside a string is a letter and not a nesting, so the
        // strings are read past rather than scanned through.
        const ReadString read = readString(text, at);
        if (!read.whole) return 0;
        at = read.next;
        continue;
      }
      if (letter == '{' || letter == '[') ++depth;
      if (letter == '}' || letter == ']') {
        --depth;
        if (depth == 0) return at + 1;
      }
      ++at;
    }
    return 0;
  }
  // A number or a literal ends where the object holding it goes on.
  const size_t end = text.find_first_of(",}", at);
  return end == std::string_view::npos ? 0 : end;
}

}  // namespace

std::optional<Introduction> readIntroduction(std::string_view text) {
  size_t at = pastSpaces(text, 0);
  if (at == text.size() || text[at] != '{') return std::nullopt;
  at = pastSpaces(text, at + 1);
  Introduction message;
  if (at != text.size() && text[at] == '}') return message;
  while (at != text.size()) {
    const ReadString name = readString(text, at);
    if (!name.whole) return std::nullopt;
    at = pastSpaces(text, name.next);
    if (at == text.size() || text[at] != ':') return std::nullopt;
    at = pastSpaces(text, at + 1);
    std::string* const field = fieldOf(message, name.value);
    if (field != nullptr && at != text.size() && text[at] == '"') {
      const ReadString value = readString(text, at);
      if (!value.whole) return std::nullopt;
      *field = std::move(value.value);
      at = value.next;
    } else {
      const size_t next = pastValue(text, at);
      if (next == 0) return std::nullopt;
      at = next;
    }
    at = pastSpaces(text, at);
    if (at == text.size()) return std::nullopt;
    if (text[at] == '}') return message;
    if (text[at] != ',') return std::nullopt;
    at = pastSpaces(text, at + 1);
  }
  return std::nullopt;
}

namespace {

/** @p value with every character JSON spells as an escape written as
 *  one. A session description is lines with a carriage return and a
 *  newline between them, so this is what makes one a string at all. */
std::string escaped(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const char letter : value) {
    switch (letter) {
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
        if (static_cast<unsigned char>(letter) < 0x20) {
          static constexpr char kDigits[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(kDigits[(letter >> 4) & 0xF]);
          out.push_back(kDigits[letter & 0xF]);
        } else {
          out.push_back(letter);
        }
    }
  }
  return out;
}

}  // namespace

std::string writeIntroduction(const Introduction& message) {
  std::string text = "{\"kind\":\"" + escaped(message.kind) + "\",\"room\":\"" +
                     escaped(message.room) + "\"";
  if (!message.sdp.empty()) text += ",\"sdp\":\"" + escaped(message.sdp) + "\"";
  if (!message.candidate.empty())
    text += ",\"candidate\":\"" + escaped(message.candidate) + "\"";
  if (!message.mid.empty()) text += ",\"mid\":\"" + escaped(message.mid) + "\"";
  return text + "}";
}

}  // namespace sigil::io::detail
