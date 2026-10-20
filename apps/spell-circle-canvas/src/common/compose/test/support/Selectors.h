#pragma once
// The selector cases' one claim about the two front doors: a selector
// written as CSS text parses to exactly the typed selector beside it.

#include <gtest/gtest.h>
#include <sigilcompose/core/Selector.h>

#include <string_view>

namespace {

void expectReads(std::string_view cssText,
                 const sigil::compose::ElementSelector& typed) {
  const sigil::compose::ElementSelector parsed =
      sigil::compose::selector(cssText);
  EXPECT_FALSE(parsed.matchesNothing()) << cssText;
  EXPECT_TRUE(parsed == typed) << cssText;
}

}  // namespace
