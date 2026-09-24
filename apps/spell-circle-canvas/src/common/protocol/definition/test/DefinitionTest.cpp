/** The reflected definition, judged as the generator reads it: every
 *  domain is a service with an events service beside it, every part of
 *  it carries the documentation written over it, the marks the parser
 *  cannot know survive, and the bytes a client is served are the bytes
 *  flatc wrote.
 */

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/reflection.h>
#include <flatbuffers/util.h>
#include <gtest/gtest.h>
#include <sigilprotocol/definition/Definition.h>

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <set>
#include <span>
#include <string>

namespace {

using Attributes =
    flatbuffers::Vector<flatbuffers::Offset<reflection::KeyValue>>;
using Lines = flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>;

const reflection::Schema& reflected() {
  const std::span<const std::byte> bytes = sigil::protocol::definition();
  return *reflection::GetSchema(bytes.data());
}

std::optional<std::string> attributeOf(const Attributes* attributes,
                                       const std::string& key) {
  if (!attributes) return std::nullopt;
  for (const reflection::KeyValue* each : *attributes)
    if (each->key()->str() == key)
      return each->value() ? each->value()->str() : std::string();
  return std::nullopt;
}

bool documented(const Lines* lines) { return lines && lines->size() > 0; }

const reflection::Service* serviceNamed(const std::string& name) {
  for (const reflection::Service* each : *reflected().services())
    if (each->name()->str() == name) return each;
  return nullptr;
}

TEST(ProtocolDefinition, TheBytesAreAReflectedSchema) {
  const std::span<const std::byte> bytes = sigil::protocol::definition();
  ASSERT_FALSE(bytes.empty());
  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  EXPECT_TRUE(reflection::VerifySchemaBuffer(verifier));
  ASSERT_NE(nullptr, reflected().root_table());
  EXPECT_EQ("sigil.protocol.Error", reflected().root_table()->name()->str());
}

TEST(ProtocolDefinition, TheBytesServedAreTheBytesFlatcWrote) {
  std::string written;
  ASSERT_TRUE(flatbuffers::LoadFile(SIGIL_PROTOCOL_BFBS, true, &written))
      << SIGIL_PROTOCOL_BFBS;
  const std::span<const std::byte> bytes = sigil::protocol::definition();
  ASSERT_EQ(written.size(), bytes.size());
  EXPECT_EQ(0, std::memcmp(written.data(), bytes.data(), bytes.size()));
}

TEST(ProtocolDefinition, EveryDomainIsAServiceWithItsEventsBesideIt) {
  // The reflection keeps rpc_service blocks, which is what the whole
  // generator stands on: were they dropped, there would be no domains.
  ASSERT_NE(nullptr, reflected().services());
  for (const std::string domain : {"host", "clock", "session", "registry"}) {
    std::string service = domain;
    service[0] = static_cast<char>(std::toupper(service[0]));
    const std::string name = "sigil.protocol." + domain + "." + service;
    EXPECT_NE(nullptr, serviceNamed(name)) << name;
    EXPECT_NE(nullptr, serviceNamed(name + "Events")) << name << "Events";
  }
  EXPECT_EQ(8u, reflected().services()->size());
}

TEST(ProtocolDefinition, EveryPartCarriesItsDocumentation) {
  for (const reflection::Service* service : *reflected().services()) {
    EXPECT_TRUE(documented(service->documentation())) << service->name()->str();
    for (const reflection::RPCCall* call : *service->calls())
      EXPECT_TRUE(documented(call->documentation()))
          << service->name()->str() << "." << call->name()->str();
  }
  for (const reflection::Object* table : *reflected().objects()) {
    EXPECT_TRUE(documented(table->documentation())) << table->name()->str();
    for (const reflection::Field* field : *table->fields())
      EXPECT_TRUE(documented(field->documentation()))
          << table->name()->str() << "." << field->name()->str();
  }
  for (const reflection::Enum* enumeration : *reflected().enums()) {
    EXPECT_TRUE(documented(enumeration->documentation()))
        << enumeration->name()->str();
    for (const reflection::EnumVal* value : *enumeration->values())
      EXPECT_TRUE(documented(value->documentation()))
          << enumeration->name()->str() << "." << value->name()->str();
  }
}

TEST(ProtocolDefinition, TheMarksTheParserCannotKnowSurvive) {
  std::set<std::string> experimental;
  std::set<std::string> asynchronous;
  for (const reflection::Service* service : *reflected().services()) {
    const std::string name = service->name()->str();
    const bool events = name.size() > 6 &&
                        name.compare(name.size() - 6, 6, "Events") == 0;
    for (const reflection::RPCCall* call : *service->calls()) {
      const std::string method = name + "." + call->name()->str();
      // Every event, and nothing else, streams from the server: the
      // builtin mark is kept only with --bfbs-builtins.
      EXPECT_EQ(events ? std::optional<std::string>("server") : std::nullopt,
                attributeOf(call->attributes(), "streaming"))
          << method;
      if (attributeOf(call->attributes(), "experimental"))
        experimental.insert(call->name()->str());
      if (attributeOf(call->attributes(), "asynchronous"))
        asynchronous.insert(call->name()->str());
    }
  }
  // A user attribute is kept whatever the flags.
  EXPECT_EQ((std::set<std::string>{"catalog", "measured", "profile"}),
            experimental);
  EXPECT_EQ((std::set<std::string>{"compositeCounts", "open", "profile",
                                   "sequence", "step", "still"}),
            asynchronous);
}

}  // namespace
