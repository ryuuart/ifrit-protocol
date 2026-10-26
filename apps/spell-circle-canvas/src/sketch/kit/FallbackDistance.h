#pragma once

/** @file
 * What a kit component reads one of the theme's distances as where no sheet
 * in force states it.
 */

#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Var.h>

#include <string_view>

namespace sigil::sketch::kit::detail {

/** THE PROPERTY @p name HOLDING @p pixels, as the fallback a component
 *  states with `varDefaults` on the node that reads it: a value the sheet
 *  in force sets wins over it, and a tree under no sheet reads the stock
 *  sheet's distance rather than nothing. */
inline compose::VarTable fallbackDistance(std::string_view name, float pixels) {
  compose::VarTable fallback;
  fallback.set(compose::var(name), compose::Dimension(pixels));
  return fallback;
}

}  // namespace sigil::sketch::kit::detail
