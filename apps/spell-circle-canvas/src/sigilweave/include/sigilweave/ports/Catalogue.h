#pragma once

/** @file
 * @ingroup weave-ports
 *
 * THE TYPE A PROGRAM CAN SET IN, as values to look through: the families
 * the platform's font manager has, each with its styles — weight, width,
 * slant and the font's own name for the style — and the variation axes
 * its faces carry, and the face a family names at a style, asked either
 * by CSS's weight, width and slant or by the font's own name for the
 * style. Asked with a font context, the faces that context adopted stand
 * in the same catalogue, marked as bundled and carrying the resource each
 * came from.
 *
 *     for (const std::string& name : weave::fonts::families()) …
 *     auto avenir = weave::fonts::family("Avenir");  // its styles and axes
 *     weave::Face bold = weave::fonts::find("Avenir", {.weight = 700});
 *     weave::Face book = weave::fonts::find("Avenir", "Book");
 */

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sigilweave/fonts/FamilyName.h"
#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/style/Face.h"

namespace sigil::weave::fonts {

/** ONE FACE OF A FAMILY, as a specimen sheet names it. */
struct Style {
  /** The CSS weight: 100 thin, 400 regular, 700 bold, 900 black. */
  int weight = 400;
  /** CSS's name for the weight, `weightName(weight)`: "Semi Bold". */
  std::string weightName;
  /** The width class: 1 ultra-condensed, 5 normal, 9 ultra-expanded. */
  int width = 5;
  FaceSlant slant = FaceSlant::Upright;
  /** The font's own name for the style: "Semibold Italic". */
  std::string name;
  /** The resource an adopted face was read from; empty for an installed
   *  face. */
  std::string uri;
  bool operator==(const Style&) const = default;
};

/** ONE FAMILY: its name, every style it has, and the variation axes its
 *  faces carry. */
struct Family {
  std::string name;
  std::vector<Style> styles;
  /** The axes the family's faces vary along, each once, in the order the
   *  first face to carry it lists them. */
  std::vector<VariationAxis> axes;
  /** Whether the family's faces are ones a font context adopted rather
   *  than the font manager's. */
  bool bundled = false;
  /** The style the font itself names @p name — "Book", "Roman",
   *  "Demi Bold Italic" — compared with ASCII case and spaces aside, so
   *  "demibold italic" finds it too; null where no style goes by it. */
  [[nodiscard]] const Style* style(std::string_view name) const;
  bool operator==(const Family&) const = default;
};

/** The family names the platform's font manager has, in its order. */
[[nodiscard]] std::vector<std::string> families();
/** The family names @p context answers: the families it adopted first,
 *  then its font manager's, each name once. */
[[nodiscard]] std::vector<std::string> families(const FontContext& context);

/** The family the platform's font manager has under @p name, with its
 *  styles and axes; nothing when it has none. */
[[nodiscard]] std::optional<Family> family(std::string_view name);
/** The family @p context answers under @p name: its adopted faces, marked
 *  bundled, where it adopted any of that family, else its manager's. */
[[nodiscard]] std::optional<Family> family(std::string_view name,
                                           const FontContext& context);

/** The face the platform's font manager finds for @p family nearest
 *  @p style; no face where it knows no family of that name. */
[[nodiscard]] Face find(std::string_view family, FaceStyle style = {});
/** The face @p context answers for @p family at @p style — its adopted
 *  faces first — which is the face `familyTypeface` answers. */
[[nodiscard]] Face find(std::string_view family, FaceStyle style,
                        FontContext& context);

/** The face of @p family whose font names its style @p styleName — the
 *  name `Family::style` matches, with ASCII case and spaces aside — from
 *  the platform's font manager; no face where the family has no style of
 *  that name. Where two faces share the name, the first the family lists
 *  answers. */
[[nodiscard]] Face find(std::string_view family, std::string_view styleName);
/** The face of @p family that @p context answers under @p styleName — of
 *  its adopted faces where it adopted any of that family, as
 *  `family(name, context)` lists them, else of its font manager's — which
 *  is the face `familyTypeface(family, styleName)` answers. */
[[nodiscard]] Face find(std::string_view family, std::string_view styleName,
                        FontContext& context);

}  // namespace sigil::weave::fonts
