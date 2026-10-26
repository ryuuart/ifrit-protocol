#pragma once

/** @file
 * What a kit component reads the theme's properties as where no sheet in
 * force states them.
 */

#include <sigilcompose/core/Cascade.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/core/Var.h>
#include <sigilsketch/kit/Theme.h>

#include <initializer_list>
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

/** THE STOCK SHEET'S VALUE OF EACH PROPERTY IN @p names — a distance, a
 *  colour or a paint, as the house theme's properties state it — as the
 *  fallbacks a component states with `varDefaults` on the node whose
 *  subtree reads them: a value the sheet in force sets wins over each, and
 *  a tree under no sheet reads the stock sheet's rather than nothing. A
 *  name the stock sheet does not state is left out. */
inline compose::VarTable stockProperties(
    std::initializer_list<std::string_view> names) {
  static const compose::VarTable stock = [] {
    compose::VarTable all;
    for (const compose::Rule& rule : houseTheme().properties().rules())
      all.overlay(rule.vars());
    return all;
  }();
  compose::VarTable picked;
  for (std::string_view name : names)
    if (const compose::VarValue* value = stock.find(compose::var(name)))
      picked.set(compose::var(name), *value);
  return picked;
}

}  // namespace sigil::sketch::kit::detail
