#include <sigilsketch/kit/StyleSheets.h>
#include <sigilsketch/kit/Theme.h>

namespace sigil::sketch::kit {

// Each stock stylesheet is its builder's, built once and held, so every
// stage naming one hands the composer the same sheet and a reconcile
// compares it by pointer before it compares it by value.

const compose::StyleSheet& houseStyleSheet() {
  static const compose::StyleSheet house = houseTheme().styleSheet();
  return house;
}

const compose::StyleSheet& studyStyleSheet() {
  static const compose::StyleSheet study = studyTheme().styleSheet();
  return study;
}

const compose::StyleSheet& specimenStyleSheet() {
  static const compose::StyleSheet specimen = specimenTheme().styleSheet();
  return specimen;
}

const compose::StyleSheet& featureStyleSheet(Density density) {
  static const compose::StyleSheet compact =
      featureTheme(Density::Compact).styleSheet();
  static const compose::StyleSheet spacious =
      featureTheme(Density::Spacious).styleSheet();
  return density == Density::Spacious ? spacious : compact;
}

const compose::StyleSheet& darkStyleSheet() {
  static const compose::StyleSheet dark = darkTheme().styleSheet();
  return dark;
}

}  // namespace sigil::sketch::kit
