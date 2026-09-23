/** @file
 * The CSS front door: the text a selector is written as, read once
 * into the value the cascade matches with.
 *
 * The grammar accepted is a selector list of complex selectors; a
 * complex selector is compounds joined by `>`, `+`, `~` or a space; a
 * compound is `*`, a bare word (a role), `.name` (a class) and the
 * structural pseudo-classes, in any order; `:has()` takes RELATIVE
 * selectors, each opening with `>`, `+`, `~` or nothing. Anything else
 * is a parse error, which warns once and matches nothing.
 */

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "SelectorInternal.h"

namespace sigil::compose {
namespace {

using detail::Combinator;
using detail::SelectorAccess;
using detail::Simple;
using detail::SimpleKind;

bool isIdentifierStart(char letter) {
  return (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
         letter == '_';
}
bool isIdentifierChar(char letter) {
  return isIdentifierStart(letter) || (letter >= '0' && letter <= '9') ||
         letter == '-';
}
bool isDigit(char letter) { return letter >= '0' && letter <= '9'; }

class Parser {
 public:
  explicit Parser(std::string_view text) : m_text(text) {}

  ElementSelector parse() {
    ElementSelector list = parseList();
    if (m_reason.empty()) {
      skipSpace();
      if (m_at != m_text.size()) m_reason = "there is text left over after it";
    }
    if (!m_reason.empty()) {
      detail::warnBadSelector(m_text, m_reason);
      return {};
    }
    return list;
  }

 private:
  bool skipSpace() {
    const size_t start = m_at;
    while (m_at < m_text.size() && (m_text[m_at] == ' ' || m_text[m_at] == '\t' ||
                                    m_text[m_at] == '\n' || m_text[m_at] == '\r'))
      ++m_at;
    return m_at != start;
  }
  bool nextIs(char letter) const {
    return m_at < m_text.size() && m_text[m_at] == letter;
  }
  bool take(char letter) {
    skipSpace();
    if (!nextIs(letter)) {
      m_reason = "a bracket or a separator is missing";
      return false;
    }
    ++m_at;
    return true;
  }
  std::string_view readIdentifier() {
    const size_t start = m_at;
    if (nextIs('-') && m_at + 1 < m_text.size() &&
        isIdentifierStart(m_text[m_at + 1]))
      ++m_at;
    if (m_at >= m_text.size() || !isIdentifierStart(m_text[m_at])) {
      m_at = start;
      return {};
    }
    while (m_at < m_text.size() && isIdentifierChar(m_text[m_at])) ++m_at;
    return m_text.substr(start, m_at - start);
  }
  bool startsCompound() const {
    if (m_at >= m_text.size()) return false;
    const char letter = m_text[m_at];
    return letter == '*' || letter == '.' || letter == ':' ||
           isIdentifierStart(letter) || letter == '-';
  }

  /** The digit run at the cursor, refused where it does not fit: a
   *  count no child list could ever reach is a misprint, and a number
   *  that wrapped around would name a position nobody wrote. */
  bool readCount(int& value, bool& anyDigit) {
    value = 0;
    anyDigit = false;
    while (m_at < m_text.size() && isDigit(m_text[m_at])) {
      value = value * 10 + (m_text[m_at] - '0');
      if (value > countCeiling) {
        m_reason = "a count larger than any child list can reach";
        return false;
      }
      ++m_at;
      anyDigit = true;
    }
    return true;
  }

  /** A selector list inside brackets — the argument of `:is`, `:where`
   *  or `:not`, or an `of` filter — refused past a ceiling so that a
   *  text cannot drive the parser off the stack. */
  ElementSelector parseNestedList() {
    if (m_depth >= nestingCeiling) {
      m_reason = "selectors nested deeper than this library reads";
      return {};
    }
    ++m_depth;
    ElementSelector nested = parseList();
    --m_depth;
    return nested;
  }

  /** The relative selectors of a `:has()`: each a complex selector,
   *  optionally opened by the combinator that reaches its first compound
   *  from the element the `:has()` stands on. */
  ElementSelector parseRelativeList() {
    if (m_depth >= nestingCeiling) {
      m_reason = "selectors nested deeper than this library reads";
      return {};
    }
    ++m_depth;
    ++m_hasDepth;
    ElementSelector list;
    for (;;) {
      skipSpace();
      Combinator relation = Combinator::Descendant;
      if (nextIs('>')) {
        relation = Combinator::Child;
        ++m_at;
      } else if (nextIs('+')) {
        relation = Combinator::Next;
        ++m_at;
      } else if (nextIs('~')) {
        relation = Combinator::Sibling;
        ++m_at;
      }
      skipSpace();
      if (!startsCompound()) {
        m_reason = "a relative selector with no element in it";
        break;
      }
      const ElementSelector one = parseComplex();
      if (!m_reason.empty()) break;
      list = list | SelectorAccess::relative(relation, one);
      skipSpace();
      if (!nextIs(',')) break;
      ++m_at;
    }
    --m_hasDepth;
    --m_depth;
    if (!m_reason.empty()) return {};
    return list;
  }

  ElementSelector parseList() {
    ElementSelector list = parseComplex();
    if (!m_reason.empty()) return {};
    for (;;) {
      skipSpace();
      if (!nextIs(',')) break;
      ++m_at;
      skipSpace();
      const ElementSelector alternative = parseComplex();
      if (!m_reason.empty()) return {};
      list = list | alternative;
    }
    return list;
  }

  ElementSelector parseComplex() {
    skipSpace();
    ElementSelector chain = parseCompound();
    if (!m_reason.empty()) return {};
    for (;;) {
      const size_t before = m_at;
      const bool spaced = skipSpace();
      if (m_at >= m_text.size() || nextIs(',') || nextIs(')')) {
        m_at = before;
        break;
      }
      Combinator combinator = Combinator::Descendant;
      if (nextIs('>')) {
        combinator = Combinator::Child;
        ++m_at;
      } else if (nextIs('+')) {
        combinator = Combinator::Next;
        ++m_at;
      } else if (nextIs('~')) {
        combinator = Combinator::Sibling;
        ++m_at;
      } else if (!spaced) {
        m_at = before;
        break;
      }
      skipSpace();
      if (!startsCompound()) {
        m_reason = "a combinator with no element after it";
        return {};
      }
      const ElementSelector subject = parseCompound();
      if (!m_reason.empty()) return {};
      switch (combinator) {
        case Combinator::Child:
          chain = chain.child(subject);
          break;
        case Combinator::Next:
          chain = chain.next(subject);
          break;
        case Combinator::Sibling:
          chain = chain.sibling(subject);
          break;
        case Combinator::Descendant:
          chain = chain.descendant(subject);
          break;
      }
    }
    return chain;
  }

  ElementSelector parseCompound() {
    std::vector<Simple> simples;
    for (;;) {
      if (m_at >= m_text.size()) break;
      const char letter = m_text[m_at];
      if (letter == '*') {
        ++m_at;
        simples.push_back(Simple{});
        continue;
      }
      if (letter == '.') {
        ++m_at;
        const std::string_view name = readIdentifier();
        if (name.empty()) {
          m_reason = "a dot with no class name after it";
          return {};
        }
        Simple simple;
        simple.kind = SimpleKind::StyleClass;
        simple.name = std::string(name);
        simples.push_back(std::move(simple));
        continue;
      }
      if (letter == ':') {
        if (!parsePseudo(simples)) return {};
        continue;
      }
      if (isIdentifierStart(letter) || letter == '-') {
        const std::string_view name = readIdentifier();
        if (name.empty()) break;
        Simple simple;
        simple.kind = SimpleKind::Role;
        simple.name = std::string(name);
        simples.push_back(std::move(simple));
        continue;
      }
      break;
    }
    if (simples.empty()) {
      m_reason = "an element was expected here";
      return {};
    }
    return SelectorAccess::fromSimples(std::move(simples));
  }

  /** `an+b`, and CSS's `odd` and `even` words for it. */
  bool parseCount(Simple& simple) {
    skipSpace();
    const size_t save = m_at;
    const std::string_view word = readIdentifier();
    if (word == "odd") {
      simple.step = 2;
      simple.offset = 1;
      return true;
    }
    if (word == "even") {
      simple.step = 2;
      simple.offset = 0;
      return true;
    }
    m_at = save;
    int sign = 1;
    if (nextIs('+'))
      ++m_at;
    else if (nextIs('-')) {
      sign = -1;
      ++m_at;
    }
    int value = 0;
    bool anyDigit = false;
    if (!readCount(value, anyDigit)) return false;
    if (nextIs('n') || nextIs('N')) {
      ++m_at;
      simple.step = sign * (anyDigit ? value : 1);
      skipSpace();
      int offsetSign = 0;
      if (nextIs('+')) {
        offsetSign = 1;
        ++m_at;
      } else if (nextIs('-')) {
        offsetSign = -1;
        ++m_at;
      }
      if (offsetSign == 0) {
        simple.offset = 0;
        return true;
      }
      skipSpace();
      int offset = 0;
      bool anyOffsetDigit = false;
      if (!readCount(offset, anyOffsetDigit)) return false;
      if (!anyOffsetDigit) {
        m_reason = "a count whose sign has no number after it";
        return false;
      }
      simple.offset = offsetSign * offset;
      return true;
    }
    if (!anyDigit) {
      m_reason = "a count that is not a number, an+b, odd or even";
      return false;
    }
    simple.step = 0;
    simple.offset = sign * value;
    return true;
  }

  bool parsePseudo(std::vector<Simple>& simples) {
    ++m_at;
    if (nextIs(':')) {
      m_reason = "a pseudo-element, which this library does not read";
      return false;
    }
    const std::string_view name = readIdentifier();
    Simple simple;
    const auto plain = [&](SimpleKind kind) {
      simple.kind = kind;
      simples.push_back(std::move(simple));
      return true;
    };
    if (name == "first-child") return plain(SimpleKind::FirstChild);
    if (name == "last-child") return plain(SimpleKind::LastChild);
    if (name == "only-child") return plain(SimpleKind::OnlyChild);
    if (name == "first-of-type") return plain(SimpleKind::FirstOfType);
    if (name == "last-of-type") return plain(SimpleKind::LastOfType);
    if (name == "only-of-type") return plain(SimpleKind::OnlyOfType);
    if (name == "empty") return plain(SimpleKind::Empty);
    if (name == "root") return plain(SimpleKind::Root);

    const bool counted = name == "nth-child" || name == "nth-last-child" ||
                         name == "nth-of-type" || name == "nth-last-of-type";
    if (counted) {
      const bool ofType = name == "nth-of-type" || name == "nth-last-of-type";
      simple.kind = name == "nth-child"          ? SimpleKind::NthChild
                    : name == "nth-last-child"   ? SimpleKind::NthLastChild
                    : name == "nth-of-type"      ? SimpleKind::NthOfType
                                                 : SimpleKind::NthLastOfType;
      if (!take('(')) return false;
      if (!parseCount(simple)) return false;
      skipSpace();
      const size_t save = m_at;
      if (readIdentifier() == "of") {
        if (ofType) {
          m_reason = "an of-type count, which takes no `of` filter";
          return false;
        }
        const ElementSelector filter = parseNestedList();
        if (!m_reason.empty()) return false;
        simple.arguments = SelectorAccess::alternatives(filter);
        if (simple.arguments.empty()) {
          m_reason = "an `of` filter with no selector in it";
          return false;
        }
      } else {
        m_at = save;
      }
      if (!take(')')) return false;
      simples.push_back(std::move(simple));
      return true;
    }

    if (name == "has") {
      if (m_hasDepth > 0) {
        m_reason = "a :has() inside another :has(), which CSS refuses too";
        return false;
      }
      simple.kind = SimpleKind::Has;
      if (!take('(')) return false;
      const ElementSelector relatives = parseRelativeList();
      if (!m_reason.empty()) return false;
      if (!take(')')) return false;
      simple.arguments = SelectorAccess::alternatives(relatives);
      simples.push_back(std::move(simple));
      return true;
    }

    if (name == "is" || name == "where" || name == "not") {
      simple.kind = name == "is"      ? SimpleKind::Is
                    : name == "where" ? SimpleKind::Where
                                      : SimpleKind::Not;
      if (!take('(')) return false;
      const ElementSelector arguments = parseNestedList();
      if (!m_reason.empty()) return false;
      if (!take(')')) return false;
      simple.arguments = SelectorAccess::alternatives(arguments);
      if (simple.arguments.empty()) {
        m_reason = "a selector list with nothing in it";
        return false;
      }
      simples.push_back(std::move(simple));
      return true;
    }

    m_reason = name.empty() ? "a colon with no pseudo-class name after it"
                            : "a pseudo-class this library does not read";
    return false;
  }

  /** The largest an+b step or offset a count may name. A child list
   *  this long cannot be built, so a longer digit run is a misprint
   *  rather than a number to carry. */
  static constexpr int countCeiling = 1000000;
  /** How deeply bracketed selector lists may stand inside one another
   *  before the text is refused. */
  static constexpr int nestingCeiling = 16;

  std::string_view m_text;
  size_t m_at = 0;
  int m_depth = 0;
  /** How many `:has()` the cursor stands inside, which may be one at
   *  most. */
  int m_hasDepth = 0;
  std::string m_reason;
};

}  // namespace

ElementSelector selector(std::string_view cssText) {
  return Parser(cssText).parse();
}

}  // namespace sigil::compose
