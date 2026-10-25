/** @file
 * The error a refusing door answers: a message that reads on its own, the
 * line and the code each unset until a door states one, compared as a
 * value, and carried as the unexpected side of `std::expected`.
 */

#include <gtest/gtest.h>
#include <sigilcore/report/Error.h>

#include <expected>
#include <string>

namespace {

using sigil::core::Error;

std::expected<int, Error> digitOf(char character) {
  if (character < '0' || character > '9')
    return std::unexpected(Error{.message = "not a digit", .code = 1});
  return character - '0';
}

TEST(CoreError, StatesNoLineAndNoCodeUntilADoorDoes) {
  const Error error{.message = "the bytes are not a table"};
  EXPECT_EQ(error.message, "the bytes are not a table");
  EXPECT_FALSE(error.line);
  EXPECT_FALSE(error.code);
}

TEST(CoreError, ComparesEveryField) {
  EXPECT_EQ((Error{"expected ';'", 5, {}}), (Error{"expected ';'", 5, {}}));
  EXPECT_NE((Error{"expected ';'", 5, {}}), (Error{"expected ';'", 6, {}}));
  EXPECT_NE((Error{"refused", {}, 2}), (Error{"refused", {}, {}}));
}

TEST(CoreError, IsTheRefusingSideOfAnExpectedAnswer) {
  EXPECT_EQ(digitOf('7').value(), 7);
  const std::expected<int, Error> refused = digitOf('x');
  ASSERT_FALSE(refused);
  EXPECT_EQ(refused.error().message, "not a digit");
  EXPECT_EQ(refused.error().code, 1);
}

}  // namespace
