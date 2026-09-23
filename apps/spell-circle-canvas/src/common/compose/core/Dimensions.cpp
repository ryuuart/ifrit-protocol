/** @file
 * A length written as text, read into a Dimension: the one grammar a
 * rule's declaration and a length handed in from another language are
 * both taken through.
 */

#include <sigilcompose/core/Layout.h>

#include <cctype>
#include <charconv>
#include <cmath>
#include <string>

namespace sigil::compose {
namespace {

/** @p text with leading and trailing blank space cut off. */
std::string_view trimmed(std::string_view text) {
  const auto blank = [](char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
  };
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}

/** Whether @p text is @p word, letter for letter, ignoring case. */
bool sameWord(std::string_view text, std::string_view word) {
  if (text.size() != word.size()) return false;
  for (size_t i = 0; i < text.size(); ++i)
    if (std::tolower((unsigned char)text[i]) != word[i]) return false;
  return true;
}

/** The Dimension @p amount takes in the unit @p suffix spells, or nothing
 *  for a suffix no unit answers to. A suffix of no letters at all is
 *  pixels, which is what a bare number is everywhere else here. */
std::optional<Dimension> inUnit(float amount, std::string_view suffix) {
  if (suffix.empty() || sameWord(suffix, "px")) return Dimension(amount);
  if (sameWord(suffix, "%")) return pct(amount);
  if (sameWord(suffix, "pw")) return pw(amount);
  if (sameWord(suffix, "ph")) return ph(amount);
  if (sameWord(suffix, "em")) return Dimension(sigil::weave::em(amount));
  if (sameWord(suffix, "rem")) return Dimension(sigil::weave::rem(amount));
  if (sameWord(suffix, "lh")) return Dimension(sigil::weave::lh(amount));
  if (sameWord(suffix, "ch")) return Dimension(sigil::weave::ch(amount));
  if (sameWord(suffix, "pt")) return Dimension(sigil::weave::pt(amount));
  return std::nullopt;
}

/** The name inside `var(...)`, or nothing where @p text is not that call.
 *  A leading `--` is CSS's mark for a custom property and names the same
 *  property as the bare name does. */
std::optional<std::string_view> customProperty(std::string_view text) {
  if (text.size() < 5 || text.back() != ')') return std::nullopt;
  if (!sameWord(text.substr(0, 4), "var(")) return std::nullopt;
  std::string_view name = trimmed(text.substr(4, text.size() - 5));
  if (name.starts_with("--")) name.remove_prefix(2);
  return name.empty() ? std::nullopt : std::optional(name);
}

/** THE BODY OF A `calc(...)`: sums of products of lengths, numbers and
 *  bracketed sums, read by recursive descent. A term is a length or a
 *  bare number, and the two are told apart until the end, since CSS
 *  multiplies only by a number — a bare number left standing in a sum
 *  is pixels, as everywhere else here. */
class CalcReader {
 public:
  explicit CalcReader(std::string_view text) : m_text(text) {}

  std::optional<Dimension> read() {
    std::optional<Term> whole = sum(0);
    skipBlank();
    if (!whole || m_at != m_text.size()) return std::nullopt;
    return whole->asLength();
  }

 private:
  /** A value in a calc: a length, or a number waiting to scale one. */
  struct Term {
    bool number = false;
    float amount = 0.0f;
    Dimension length;
    [[nodiscard]] Dimension asLength() const {
      return number ? Dimension(amount) : length;
    }
  };

  /** How deeply brackets may stand inside one another before the text is
   *  refused, so a text cannot drive the reader off the stack. */
  static constexpr int nestingCeiling = 16;

  void skipBlank() {
    while (m_at < m_text.size() &&
           (m_text[m_at] == ' ' || m_text[m_at] == '\t' ||
            m_text[m_at] == '\n' || m_text[m_at] == '\r'))
      ++m_at;
  }

  std::optional<Term> sum(int depth) {
    std::optional<Term> left = product(depth);
    if (!left) return std::nullopt;
    for (;;) {
      skipBlank();
      if (m_at >= m_text.size()) break;
      const char sign = m_text[m_at];
      if (sign != '+' && sign != '-') break;
      ++m_at;
      std::optional<Term> right = product(depth);
      if (!right) return std::nullopt;
      if (left->number && right->number) {
        left->amount += sign == '+' ? right->amount : -right->amount;
        continue;
      }
      const Dimension a = left->asLength();
      const Dimension b = right->asLength();
      const Dimension combined = sign == '+' ? a + b : a - b;
      // A sum the arithmetic refused — a percentage beside another unit —
      // is text this grammar does not read, rather than an auto.
      if (combined.unit == Dimension::Unit::Auto) return std::nullopt;
      left = Term{false, 0.0f, combined};
    }
    return left;
  }

  std::optional<Term> product(int depth) {
    std::optional<Term> left = factor(depth);
    if (!left) return std::nullopt;
    for (;;) {
      skipBlank();
      if (m_at >= m_text.size()) break;
      const char operation = m_text[m_at];
      if (operation != '*' && operation != '/') break;
      ++m_at;
      std::optional<Term> right = factor(depth);
      if (!right) return std::nullopt;
      if (operation == '/') {
        // Only a number divides, and never by nothing.
        if (!right->number || right->amount == 0.0f) return std::nullopt;
        if (left->number)
          left->amount /= right->amount;
        else
          left->length = left->length / right->amount;
        continue;
      }
      if (left->number && right->number) {
        left->amount *= right->amount;
      } else if (left->number) {
        left = Term{false, 0.0f, left->amount * right->length};
      } else if (right->number) {
        left->length = left->length * right->amount;
      } else {
        return std::nullopt;  // a length times a length is an area
      }
    }
    return left;
  }

  std::optional<Term> factor(int depth) {
    skipBlank();
    if (m_at >= m_text.size()) return std::nullopt;
    if (m_text[m_at] == '(' || startsWord("calc(")) {
      if (depth >= nestingCeiling) return std::nullopt;
      m_at += m_text[m_at] == '(' ? 1 : 5;
      std::optional<Term> inner = sum(depth + 1);
      skipBlank();
      if (!inner || m_at >= m_text.size() || m_text[m_at] != ')')
        return std::nullopt;
      ++m_at;
      return inner;
    }
    if (startsWord("var(")) {
      const size_t close = m_text.find(')', m_at);
      if (close == std::string_view::npos) return std::nullopt;
      const std::optional<std::string_view> name =
          customProperty(m_text.substr(m_at, close + 1 - m_at));
      if (!name) return std::nullopt;
      m_at = close + 1;
      return Term{false, 0.0f, Dimension(var(*name))};
    }
    // A number and the unit glued to it; a sign in front is the number's.
    size_t end = m_at;
    if (end < m_text.size() && (m_text[end] == '+' || m_text[end] == '-'))
      ++end;
    while (end < m_text.size() &&
           (std::isdigit((unsigned char)m_text[end]) || m_text[end] == '.'))
      ++end;
    const size_t numberEnd = end;
    while (end < m_text.size() &&
           (std::isalpha((unsigned char)m_text[end]) || m_text[end] == '%'))
      ++end;
    if (numberEnd == m_at) return std::nullopt;
    std::string_view number = m_text.substr(m_at, numberEnd - m_at);
    if (number.front() == '+') number.remove_prefix(1);
    float amount = 0;
    const std::from_chars_result read =
        std::from_chars(number.data(), number.data() + number.size(), amount);
    if (read.ec != std::errc{} ||
        read.ptr != number.data() + number.size() || !std::isfinite(amount))
      return std::nullopt;
    const std::string_view suffix = m_text.substr(numberEnd, end - numberEnd);
    m_at = end;
    if (suffix.empty()) return Term{true, amount, {}};
    const std::optional<Dimension> length = inUnit(amount, suffix);
    if (!length) return std::nullopt;
    return Term{false, 0.0f, *length};
  }

  bool startsWord(std::string_view word) const {
    return m_text.size() - m_at >= word.size() &&
           sameWord(m_text.substr(m_at, word.size()), word);
  }

  std::string_view m_text;
  size_t m_at = 0;
};

}  // namespace

std::optional<Dimension> parseDimension(std::string_view text) {
  const std::string_view body = trimmed(text);
  if (body.empty()) return std::nullopt;
  if (sameWord(body, "auto")) return autoDimension();
  if (body.size() > 6 && sameWord(body.substr(0, 5), "calc(") &&
      body.back() == ')')
    return CalcReader(body.substr(5, body.size() - 6)).read();
  if (const std::optional<std::string_view> name = customProperty(body))
    return Dimension(var(*name));
  // A LEADING SIGN is the number's, and from_chars reads only the minus,
  // so the plus CSS allows is stepped over here.
  const std::string_view digits =
      body.front() == '+' ? body.substr(1) : body;
  if (digits.empty()) return std::nullopt;
  // from_chars reads the number and stops at the unit, which is the whole
  // of the split: a length is one number and one suffix with nothing
  // between them.
  float amount = 0;
  const char* const first = digits.data();
  const char* const last = first + digits.size();
  const std::from_chars_result read = std::from_chars(first, last, amount);
  if (read.ec != std::errc{}) return std::nullopt;
  // The general format from_chars reads accepts "inf" and "nan", which
  // name no distance. A length that is not a finite number of anything is
  // refused here rather than handed on to a layout that cannot lay it out.
  if (!std::isfinite(amount)) return std::nullopt;
  return inUnit(amount, std::string_view(read.ptr, (size_t)(last - read.ptr)));
}

}  // namespace sigil::compose
