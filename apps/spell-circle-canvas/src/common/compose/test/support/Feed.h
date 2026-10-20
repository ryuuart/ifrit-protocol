#pragma once
// The feed window the feed cases share: plain white rows on the Host's
// black ground, so a row's ink reads as brightness.

#include <sigilcompose/kit/Feed.h>

#include <cstddef>

#include "Host.h"

namespace {

/** Plain rows, white on the Host's black ground so ink reads as brightness. */
feed::TextOptions feedOptions(size_t visible, float size = 12.0f) {
  feed::TextOptions options;
  options.styles.base(whiteStyle(size));
  options.window.visible = visible;
  options.window.gap = 2.0f;
  return options;
}

}  // namespace
