/** @file
 * A meaning is one name, whichever image asks: a decoder registered from
 * one translation unit answers a load from another whose type of the
 * same name and layout is a different C++ type identity — the shape a
 * sketch compiled and loaded while its host runs has — and a type that
 * only borrows the name answers nothing.
 */

#include <gtest/gtest.h>
#include <sigilio/advanced/Decoding.h>
#include <sigilio/advanced/Places.h>
#include <sigilio/hub/Hub.h>

#include <string>
#include <type_traits>
#include <typeindex>

#include "MeaningElsewhere.h"
#include "MountedHub.h"

namespace {

/** This image's `Shade`: the same name and layout as the other image's,
 *  and another type. */
struct Shade {
  std::string text;
};

std::string_view meaningName(std::type_identity<Shade>) { return "test.Shade"; }

/** A type that claims the name and is not the type registered under it. */
struct Impostor {
  int value = 0;
};

std::string_view meaningName(std::type_identity<Impostor>) {
  return "test.Shade";
}

}  // namespace

static_assert(sigil::io::Named<Shade>);

TEST_F(IOHub, ADecoderRegisteredInOneImageAnswersALoadFromAnother) {
  // The premise: the two Shades are distinct identities, as a host's type
  // and a live sketch's are, and share only their name.
  ASSERT_NE(std::type_index(typeid(Shade)),
            sigil::io::test::shadeIdentityElsewhere());
  dir.write("dusk.txt", "violet");

  sigil::io::test::registerShadeElsewhere(hub);
  const auto shade = hub.load<Shade>("res://dusk.txt");
  ASSERT_NE(shade, nullptr);
  EXPECT_EQ(shade->text, "violet");
  // One view, whichever image asks: the other image's load is this one.
  EXPECT_EQ(sigil::io::test::loadShadeElsewhere(hub, "res://dusk.txt"),
            std::optional<size_t>(6));
}

TEST_F(IOHub, ADecoderRegisteredHereAnswersALoadFromElsewhere) {
  dir.write("dawn.txt", "amber rose");
  sigil::io::registerDecoder<Shade>(hub, [](const sigil::io::Bytes& bytes) {
    return std::optional<Shade>(Shade{std::string(bytes.asText())});
  });
  EXPECT_EQ(sigil::io::test::loadShadeElsewhere(hub, "res://dawn.txt"),
            std::optional<size_t>(10));
}

TEST_F(IOHub, ATypeBorrowingAMeaningNameAnswersNothing) {
  dir.write("noon.txt", "white");
  sigil::io::test::registerShadeElsewhere(hub);
  ASSERT_NE(hub.load<Shade>("res://noon.txt"), nullptr);
  // The name finds the decoder; the type's own name refuses it, before
  // the view already decoded under the name is handed out as the wrong
  // type.
  EXPECT_EQ(hub.load<Impostor>("res://noon.txt"), nullptr);
}

TEST_F(IOHub, AFileATypedAskFoundMissingIsAChangeWhenItAppears) {
  sigil::io::test::registerShadeElsewhere(hub);
  EXPECT_EQ(hub.load<Shade>("res://later.txt"), nullptr);
  EXPECT_FALSE(sigil::io::poll(hub));
  dir.write("later.txt", "indigo");
  // The poll that sees the file says so once, and the ask then answers.
  EXPECT_TRUE(sigil::io::poll(hub));
  EXPECT_FALSE(sigil::io::poll(hub));
  const auto shade = hub.load<Shade>("res://later.txt");
  ASSERT_NE(shade, nullptr);
  EXPECT_EQ(shade->text, "indigo");
}
