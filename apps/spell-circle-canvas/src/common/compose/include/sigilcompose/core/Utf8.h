#pragma once

/** @file
 * @ingroup compose-core
 *
 * Utf8 — the value a prop or a parameter that takes TEXT is declared as,
 * so the text may be written either way at the call site.
 */

#include <concepts>
#include <string>
#include <string_view>

namespace sigil::compose {

/** TEXT, SPELLED EITHER WAY.
 *
 *  A prop or a parameter declared as this accepts `"…"`, `u8"…"`, a
 *  `std::string`, a `std::string_view`, a `std::u8string` and a
 *  `std::u8string_view`, so a call site writes `.title = "THE RULE AND
 *  THE STRANDS"` and never a conversion around it. Both spellings mean
 *  the same bytes: a `char` string is read as UTF-8, which is what every
 *  string in this repository holds, and the bytes are kept exactly as
 *  they arrive.
 *
 *  IT HOLDS A `std::u8string`, the type the shaping vocabulary
 *  underneath takes, and `bytes()` is the ONE way to read it out: there
 *  is no implicit conversion out, so a sink spells `value.bytes()` and
 *  every use is visible where it stands. A sink in this library takes THIS
 *  type rather than the bytes — `text`, `ellipsis`, every kit prop — so
 *  `bytes()` at a call site means the caller is reading the bytes
 *  themselves, not fitting a signature.
 *
 *  A VALUE THAT READS ITSELF OUT AS TEXT IS TEXT. Anything answering
 *  `text()` with something a `std::string_view` reads — a node of a
 *  decoded document is the one in this tree — is accepted as well, so a
 *  caller whose words live in a file writes the node where the words
 *  would go. The constraint is what keeps this library from naming the
 *  vocabulary that value belongs to: nothing here includes it and nothing
 *  links it.
 *
 *  Comparable by value, so a props struct that carries one stays
 *  comparable and a description that holds one still prunes. */
class Utf8 {
 public:
  Utf8() = default;
  Utf8(const char* utf8)
      : m_bytes(widen(utf8 != nullptr ? std::string_view(utf8)
                                      : std::string_view())) {}
  Utf8(std::string_view utf8) : m_bytes(widen(utf8)) {}
  Utf8(const std::string& utf8) : m_bytes(widen(utf8)) {}
  Utf8(const char8_t* utf8)
      : m_bytes(utf8 != nullptr ? std::u8string_view(utf8)
                                : std::u8string_view()) {}
  Utf8(std::u8string_view utf8) : m_bytes(utf8) {}
  Utf8(std::u8string utf8) noexcept : m_bytes(std::move(utf8)) {}
  /** A value that reads itself out as text — the document node whose
   *  words a call site would otherwise unwrap by hand. */
  template <class Read>
    requires requires(const Read& value) {
      { value.text() } -> std::convertible_to<std::string_view>;
    }
  Utf8(const Read& value) : m_bytes(widen(std::string_view(value.text()))) {}

  /** The bytes, as the text vocabulary takes them. */
  [[nodiscard]] const std::u8string& bytes() const { return m_bytes; }
  [[nodiscard]] bool empty() const { return m_bytes.empty(); }
  bool operator==(const Utf8&) const = default;

 private:
  static std::u8string widen(std::string_view utf8) {
    return std::u8string(utf8.begin(), utf8.end());
  }

  std::u8string m_bytes;
};

}  // namespace sigil::compose
