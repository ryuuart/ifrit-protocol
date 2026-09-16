/** @file
 * The handler search every guard in this binary stands on: a catch of a
 * base class matches an exception thrown as a derived one.
 *
 * An archive compiled without run-time type information that throws or
 * catches a standard exception emits a private, non-unique copy of that
 * type's typeinfo, and the linker satisfies every other object's
 * reference to the name with that copy. The search compares a typeinfo
 * by address, so one such archive anywhere in the link leaves every
 * base-class catch in the binary matching nothing the standard library
 * throws; the case below then ends the process rather than failing.
 */

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

namespace sigil::weave::ports {
namespace {

TEST(StandardExceptions, ABaseClassCatchMatchesWhatTheStandardLibraryThrows) {
  const std::string message = "thrown as a derived type";
  bool caught = false;
  try {
    throw std::runtime_error(message);
  } catch (const std::exception& error) {
    caught = true;
    EXPECT_EQ(std::string(error.what()), message);
  }
  EXPECT_TRUE(caught) << "a std::runtime_error is a std::exception, and the "
                         "typeinfo the handler search compares it by is the "
                         "one the standard library threw it with";
}

}  // namespace
}  // namespace sigil::weave::ports
