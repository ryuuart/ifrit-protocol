#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/kit/Cells.h>

#include <utility>

namespace sigil::sketch::kit {

compose::Element well(const Well& spec, compose::Element surface) {
  const Theme& look = theme();
  return compose::kit::well(
      {.width = spec.width,
       .height = spec.height,
       .ground = spec.ground.value_or(
           compose::Fill::color(look.palette.cellGround)),
       .padding = spec.padding.value_or(look.spacing.wellPadding),
       .clip = spec.clip},
      std::move(surface));
}

compose::Element well(const Well& spec) {
  return well(spec, compose::box());
}

compose::Element caption(float measure, std::u8string label,
                         std::u8string note, compose::Element body) {
  return compose::kit::cell(theme().voice(measure), std::move(label),
                            std::move(note), std::move(body));
}

}  // namespace sigil::sketch::kit
