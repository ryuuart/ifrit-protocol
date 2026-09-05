/** @file The stock catalogue: three feature catalogues, read side by side. */

#include "sigilmaterial/stock/Stock.h"

#include <sigilcore/schedule/ConcurrentIo.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Recipes.h>
#include <sigilmaterial/sdf/Sdf.h>

#include <array>
#include <utility>

namespace sigil::material::stock {

std::vector<Material> everyRecipe() {
  using Catalogue = std::vector<Material> (*)();
  constexpr std::array<Catalogue, 3> catalogues{
      &field::everyRecipe, &sdf::everyRecipe, &kit::everyRecipe};

  // Each catalogue discovers its own shader directory and reads the files
  // in it, so the three spend their time waiting on a disk rather than on
  // a core — and each writes only its own element.
  std::array<std::vector<Material>, catalogues.size()> found;
  core::schedule::concurrentIo(catalogues.size(), [&](size_t index) {
    found[index] = catalogues[index]();
  });

  std::vector<Material> recipes;
  size_t total = 0;
  for (const std::vector<Material>& part : found) total += part.size();
  recipes.reserve(total);
  for (std::vector<Material>& part : found)
    for (Material& item : part) recipes.push_back(std::move(item));
  return recipes;
}

WarmupResult warmup(Target target) {
  const std::vector<Material> recipes = everyRecipe();
  return material::warmup(recipes, target);
}

}  // namespace sigil::material::stock
