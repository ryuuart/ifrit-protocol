/** A door's vitals as one value: taken before and after each arrival of
 *  a recording, it compares unequal exactly when a field moved, and every
 *  field is the reading of the same name.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

#include "ScratchDir.h"

using sigil::data::Connection;
using sigil::io::Bytes;
using sigil::io::Hub;
using sigil::test::ScratchDir;

namespace {

std::shared_ptr<const Bytes> bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return std::make_shared<const Bytes>(std::span(first, text.size()));
}

/** Each field of @p vitals is the reading of that name on @p door. */
void expectReadings(const Connection& door, const Connection::Vitals& vitals) {
  EXPECT_EQ(vitals.revision, door.revision());
  EXPECT_EQ(vitals.dropped, door.dropped());
  EXPECT_EQ(vitals.undecodable, door.undecodable());
  EXPECT_EQ(vitals.closed, door.closed());
  EXPECT_EQ(vitals.localAddress, door.localAddress());
  EXPECT_EQ(vitals.sender, door.sender());
  EXPECT_EQ(vitals.error, door.error());
}

TEST(DataVitals, TheValueMovesExactlyWhenAFieldDid) {
  const ScratchDir scratch("data_connection_vitals");
  const std::filesystem::path path = scratch.path / "scene.feed";
  {
    sigil::io::RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    writer.append(sigil::io::Message(bytesOf(R"({"kind":"gust","strength":1})"), {}, std::chrono::duration<double>(0.0), 1));
    writer.append(sigil::io::Message(bytesOf("this is no document at all"), {}, std::chrono::duration<double>(1.0), 2));
  }
  Hub hub;
  hub.replay("ws://:8848/scene", path.string());
  Connection scene(hub, "ws://:8848/scene");

  const Connection::Vitals opened = scene.vitals();
  expectReadings(scene, opened);
  EXPECT_EQ(opened.revision, 0u);
  EXPECT_FALSE(opened.closed);
  // Asked again with nothing dispatched between, it is the same value.
  EXPECT_EQ(scene.vitals(), opened);

  hub.advance(std::chrono::duration<double>(0.0));
  const Connection::Vitals arrived = scene.vitals();
  expectReadings(scene, arrived);
  EXPECT_NE(arrived, opened);
  EXPECT_EQ(arrived.revision, 1u);
  EXPECT_EQ(arrived.undecodable, 0u);

  // Time moves and nothing arrives: nothing moved, so the value did not.
  hub.advance(std::chrono::duration<double>(0.5));
  EXPECT_EQ(scene.vitals(), arrived);

  // The last arrival is no message and the recording runs out: the
  // revision, the undecodable count and the door's state all move.
  hub.advance(std::chrono::duration<double>(1.0));
  const Connection::Vitals ended = scene.vitals();
  expectReadings(scene, ended);
  EXPECT_NE(ended, arrived);
  EXPECT_EQ(ended.revision, 2u);
  EXPECT_EQ(ended.undecodable, 1u);
  EXPECT_TRUE(ended.closed);

  // One field alone is enough to make two values unequal.
  Connection::Vitals oneField = ended;
  oneField.sender = "ws://127.0.0.1:52341";
  EXPECT_NE(oneField, ended);
}

TEST(DataVitals, AConnectionOntoNothingIsClosedWithNothingArrived) {
  const Connection none;
  const Connection::Vitals vitals = none.vitals();
  expectReadings(none, vitals);
  EXPECT_EQ(vitals, Connection::Vitals{});
  EXPECT_TRUE(vitals.closed);
}

}  // namespace
