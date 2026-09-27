// A fact of a library type stated by another image — a sketch the live
// host compiled on its own, hidden, as the guest beside this file is —
// reads back in that type in the library that runs in this one: the pin
// operator hangs what the guest's request asks for.

#include <dlfcn.h>
#include <sigilcompose/kit/Pin.h>

#include <typeinfo>

#include "support/ShapeTestSupport.h"

namespace {

TEST(ComposeAdders, APinRequestStatedByAnotherImageIsHung) {
  void* guest = dlopen(SIGIL_COMPOSE_PIN_GUEST, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(guest, nullptr) << dlerror();
  using StatePin = void (*)(Element*);
  using RequestType = const std::type_info* (*)();
  const auto statePin =
      reinterpret_cast<StatePin>(dlsym(guest, "sigilGuestStatePin"));
  const auto requestType =
      reinterpret_cast<RequestType>(dlsym(guest, "sigilGuestRequestType"));
  ASSERT_NE(statePin, nullptr);
  ASSERT_NE(requestType, nullptr);
  // What makes the case: the guest names the type with an identity of its
  // own, one this image's does not compare equal to.
  ASSERT_STREQ(requestType()->name(), typeid(pin::Request).name());
  ASSERT_FALSE(*requestType() == typeid(pin::Request))
      << "the guest shares this image's identity for the type, so the case "
         "no longer crosses one";

  Element card =
      box().key("card").left(20).top(90).width(20).height(20).fill(green());
  statePin(&card);

  Host host;
  host.composer.render(box().width(200).height(200).children({card}).operators(
      {pin::ByLane{.lane = "label"}}));
  host.frame();
  const auto pinned = host.composer.bounds("card-pin");
  ASSERT_TRUE(pinned.has_value());
  EXPECT_NEAR(pinned->left(), 50, 0.5f);
  EXPECT_NEAR(pinned->centre().y, 100, 0.5f);
  EXPECT_EQ(host.pixel(60, 100), SK_ColorRED);
  // The guest stays loaded: the fact's value and its equality are its.
}

}  // namespace
