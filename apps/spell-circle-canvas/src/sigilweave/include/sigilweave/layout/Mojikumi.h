#pragma once

/** @file
 * @ingroup weave-layout
 *
 * JAPANESE COMPOSITION: the class a character is set by, and the table
 * that says how much space stands between two of them.
 */

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace sigil::weave {

/** WHICH KIND OF CHARACTER A MOJIKUMI RULE IS ABOUT. Japanese setting
 * spaces full-width characters by the CLASS of the two either side of a
 * gap rather than by the characters themselves. Which characters are of
 * which class is a decision, so it is data a house sets; whether a
 * character stands in a full-width cell at all is a property of the
 * character and the engine answers it.
 */
enum class MojikumiClass : uint8_t {
  kOther,      ///< not full-width, or not a class the table names
  kIdeograph,  ///< a full-width character the table gives no other class
  kOpening,    ///< an opening bracket or quote
  kClosing,    ///< a closing bracket or quote
  kFullStop,   ///< a sentence mark
  kComma,      ///< a reading mark
  kMiddleDot,  ///< an interpunct
  kCount
};

/** HOW MUCH ROOM STANDS BETWEEN TWO ADJACENT FULL-WIDTH CHARACTERS.
 * `members` names the characters of each class, one per entry; `room` is
 * read by the class before the gap and the class after it, as a fraction
 * of the em, negative closing the gap up.
 * @silent the two characters were shaped inside ONE WORD: a table is
 * applied across a break opportunity, and the face sets the rest.
 */
struct MojikumiTable {
  static constexpr size_t kClasses = static_cast<size_t>(MojikumiClass::kCount);
  std::u16string members[kClasses];
  float room[kClasses][kClasses] = {};

  /** Returns whether any class has members. */
  [[nodiscard]] bool empty() const {
    for (const std::u16string& entry : members)
      if (!entry.empty()) return false;
    return true;
  }
  /** The class the table gives `character`, or kOther when it names none.
   *  A full-width character the table does not name is kIdeograph, which
   *  the caller decides by asking the character and not this table. */
  [[nodiscard]] MojikumiClass classOf(char16_t character) const {
    for (size_t index = 0; index < kClasses; ++index)
      if (members[index].find(character) != std::u16string::npos)
        return static_cast<MojikumiClass>(index);
    return MojikumiClass::kOther;
  }
  bool operator==(const MojikumiTable& other) const {
    for (size_t index = 0; index < kClasses; ++index) {
      if (members[index] != other.members[index]) return false;
      for (size_t column = 0; column < kClasses; ++column)
        if (room[index][column] != other.room[index][column]) return false;
    }
    return true;
  }
};

}  // namespace sigil::weave
