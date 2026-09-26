/** A door's state as one value: taken before and after each arrival of
 *  a recording, it compares unequal exactly when a field moved — IO's
 *  feed state with the count of arrivals no reader saw.
 */

#include <gtest/gtest.h>
#include <sigildata/connection/Connection.h>
#include <sigilio/advanced/Time.h>
#include <sigilio/hub/Hub.h>
#include <sigilio/hub/Recording.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string_view>

#include "ScratchDir.h"

using sigil::data::Connection;
using sigil::data::ConnectionState;
using sigil::io::Bytes;
using sigil::io::Hub;
using sigil::io::ReadyState;
using sigil::test::ScratchDir;

namespace {

std::shared_ptr<const Bytes> bytesOf(std::string_view text) {
  const auto* first = reinterpret_cast<const std::byte*>(text.data());
  return std::make_shared<const Bytes>(std::span(first, text.size()));
}

TEST(DataConnectionState, TheValueMovesExactlyWhenAFieldDid) {
  const ScratchDir scratch("data_connection_state");
  const std::filesystem::path path = scratch.path / "scene.feed";
  {
    sigil::io::RecordingWriter writer(path);
    ASSERT_TRUE(writer.good());
    writer.append(sigil::io::Message(bytesOf(R"({"kind":"gust","strength":1})"), {}, std::chrono::duration<double>(0.0), 1));
    writer.append(sigil::io::Message(bytesOf("this is no document at all"), {}, std::chrono::duration<double>(1.0), 2));
  }
  Hub hub;
  Connection scene = sigil::data::replay(hub, "ws://:8848/scene", path.string());

  const ConnectionState opened = scene.state();
  EXPECT_EQ(opened.revision, 0u);
  EXPECT_NE(opened.readiness, ReadyState::Closed);
  // Asked again with nothing advanced between, it is the same value.
  EXPECT_EQ(scene.state(), opened);

  sigil::io::advance(hub, std::chrono::duration<double>(0.0));
  const ConnectionState arrived = scene.state();
  EXPECT_NE(arrived, opened);
  EXPECT_EQ(arrived.revision, 1u);
  EXPECT_EQ(arrived.undecodable, 0u);

  // Time moves and nothing arrives: nothing moved, so the value did not.
  sigil::io::advance(hub, std::chrono::duration<double>(0.5));
  EXPECT_EQ(scene.state(), arrived);

  // The last arrival is no message and the recording runs out: the
  // revision, the undecodable count and the readiness all move.
  sigil::io::advance(hub, std::chrono::duration<double>(1.0));
  const ConnectionState ended = scene.state();
  EXPECT_NE(ended, arrived);
  EXPECT_EQ(ended.revision, 2u);
  EXPECT_EQ(ended.undecodable, 1u);
  EXPECT_EQ(ended.readiness, ReadyState::Closed);

  // Data's own field alone is enough to make two values unequal.
  ConnectionState oneField = ended;
  oneField.undecodable = 0;
  EXPECT_NE(oneField, ended);
}

TEST(DataConnectionState, AConnectionOntoNothingIsClosedWithNothingArrived) {
  const ConnectionState state = Connection().state();
  EXPECT_EQ(state.readiness, ReadyState::Closed);
  EXPECT_EQ(state.revision, 0u);
  EXPECT_EQ(state.undecodable, 0u);
  EXPECT_TRUE(state.error.empty());
}

}  // namespace
