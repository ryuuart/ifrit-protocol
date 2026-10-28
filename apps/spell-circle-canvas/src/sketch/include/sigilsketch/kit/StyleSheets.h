#pragma once

/** @file
 * @ingroup sketch-kit
 *
 * The stock stylesheets a stage names: the looks this repository's sheets
 * are set in, each one `compose::StyleSheet` of role rules, classes and
 * custom properties, joined to a sketch's own rules with `+`.
 */

#include <sigilcompose/core/StyleSheet.h>

namespace sigil::sketch::kit {

/** The space around a feature's authored figures. Compact retains narrow
 *  specimen margins; Spacious gives multi-section explanations more air. */
enum class Density { Compact, Spacious };

/** THE HOUSE STYLESHEET: the role rules, the classes and the custom
 *  properties most of this repository's specimen sheets are set in.
 *  A stage that names no stylesheet applies its custom properties alone,
 *  and none of its role rules. Built once and held. */
[[nodiscard]] const compose::StyleSheet& houseStyleSheet();

/** THE STUDY STYLESHEET, for comparative studies: a larger heading,
 *  brighter notes and wider margins over the house ground and
 *  components. Built once and held. */
[[nodiscard]] const compose::StyleSheet& studyStyleSheet();

/** THE SPECIMEN STYLESHEET: a readable title and notes for API
 *  comparisons, with the house sheet's compact margins, so fixed specimen
 *  wells keep their measure. Built once and held. */
[[nodiscard]] const compose::StyleSheet& specimenStyleSheet();

/** THE FEATURE STYLESHEET for feature demonstrations: slate grounds,
 *  ivory headings, pale gold readings, interface labels and terminal
 *  controls. `Density::Compact` keeps 24 px side margins and a 22 px
 *  title for fixed specimen grids; `Density::Spacious` uses 40 px side
 *  margins and a 30 px title for pages of comparisons and explanations.
 *  Each density is built once and held. */
[[nodiscard]] const compose::StyleSheet& featureStyleSheet(
    Density density = Density::Compact);

/** THE DARK STYLESHEET in the terminal voice an instrument or a live
 *  demonstration stands on: a near-black ground, a warm off-white ink, a
 *  grey ash, interface headings, and the terminal face as the face a call
 *  is set in. Its `label` class is an instrument's label — the terminal
 *  face at 12 px, tracked 2 px, in the ash — so a dial's caption is
 *  `text(words).styleClass("label")`. Built once and held. */
[[nodiscard]] const compose::StyleSheet& darkStyleSheet();

}  // namespace sigil::sketch::kit
