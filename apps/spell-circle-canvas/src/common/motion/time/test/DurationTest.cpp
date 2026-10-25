/** @file
 * The CSS time reader: seconds and milliseconds in any case, the plus
 * and the blank space CSS allows, and the refusal of a time with no unit
 * or another one.
 */

#include <gtest/gtest.h>
#include <sigilmotion/time/Duration.h>

#include <chrono>
#include <optional>

using namespace std::chrono_literals;
using sigil::motion::Duration;
using sigil::motion::readDuration;

TEST(Duration, CssTextIsReadInSecondsAndMilliseconds) {
  EXPECT_EQ(readDuration("0.3s"), Duration(0.3));
  EXPECT_EQ(readDuration("300ms"), Duration(0.3))
      << "three hundred milliseconds is the same number of seconds as 0.3s";
  EXPECT_EQ(readDuration("1.5S"), Duration(1.5));
  EXPECT_EQ(readDuration("250MS"), Duration(250ms));
  EXPECT_EQ(readDuration(" +2s\n"), Duration(2s));
  EXPECT_EQ(readDuration("-0.5s"), Duration(-0.5))
      << "a negative time is read as written; its slot judges it";
  EXPECT_EQ(readDuration("1e3ms"), Duration(1s));
}

TEST(Duration, TextWithoutAUnitOrWithAnotherIsRefused) {
  // A bare number in text is seconds in one convention and milliseconds
  // in another, so the reader names neither.
  for (const char* text :
       {"0.3", "", "s", "ms", "3 s", "300msec", "2min", "infs", "+-1s"})
    EXPECT_EQ(readDuration(text), std::nullopt) << text;
}
