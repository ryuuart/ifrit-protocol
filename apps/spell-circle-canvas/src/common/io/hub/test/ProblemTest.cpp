/** @file
 * The hub's problems: a reader says what it could not make of a URI, the
 * hub lists it once per URI with the newest word standing, and the word
 * is taken back by its reader, by a host clearing the list, or by a load
 * of the URI that succeeds. A text read that found nothing is watched as
 * a typed ask is, so the poll that sees the file appear says so.
 */

#include <gtest/gtest.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/advanced/Problems.h>
#include <sigilio/hub/Hub.h>

#include <optional>
#include <string>
#include <type_traits>

#include "MountedHub.h"

namespace {

struct Verse {
  std::string text;
};

std::string_view meaningName(std::type_identity<Verse>) {
  return "test.Verse";
}

}  // namespace

TEST_F(IOHub, AProblemIsListedOncePerUriAndTheNewestWordStands) {
  EXPECT_TRUE(hub.problems().empty());
  sigil::io::reportProblem(hub, {"res://a.sksl", "unknown identifier", 3});
  sigil::io::reportProblem(hub, {"res://b.sksl", "could not be read", {}});
  sigil::io::reportProblem(hub, {"res://a.sksl", "expected ';'", 5});
  const std::vector<sigil::io::Problem> said = hub.problems();
  ASSERT_EQ(said.size(), 2u);
  EXPECT_EQ(said[0], (sigil::io::Problem{"res://a.sksl", "expected ';'", 5}));
  EXPECT_EQ(said[1].uri, "res://b.sksl");
  EXPECT_FALSE(said[1].line);

  sigil::io::clearProblem(hub, "res://a.sksl");
  ASSERT_EQ(hub.problems().size(), 1u);
  EXPECT_EQ(hub.problems()[0].uri, "res://b.sksl");
  sigil::io::clearProblems(hub);
  EXPECT_TRUE(hub.problems().empty());
}

TEST_F(IOHub, ALoadThatSucceedsTakesBackItsUrisProblem) {
  sigil::io::registerDecoder<Verse>(hub, [](const sigil::io::Bytes& bytes) {
    if (bytes.asText().empty()) return std::optional<Verse>();
    return std::optional<Verse>(Verse{std::string(bytes.asText())});
  });
  sigil::io::reportProblem(hub, {"res://verse.txt", "no such file", {}});
  sigil::io::reportProblem(hub, {"res://other.txt", "no such file", {}});
  dir.write("verse.txt", "a line");
  ASSERT_NE(hub.load<Verse>("res://verse.txt"), nullptr);
  ASSERT_EQ(hub.problems().size(), 1u);
  EXPECT_EQ(hub.problems()[0].uri, "res://other.txt");
}

TEST_F(IOHub, AFileATextReadFoundMissingIsAChangeWhenItAppears) {
  EXPECT_FALSE(hub.text("res://later.sksl"));
  EXPECT_FALSE(sigil::io::poll(hub));
  dir.write("later.sksl", "half4 main(float2 p) { return half4(1); }");
  EXPECT_TRUE(sigil::io::poll(hub));
  EXPECT_FALSE(sigil::io::poll(hub));
  EXPECT_TRUE(hub.text("res://later.sksl"));
}
