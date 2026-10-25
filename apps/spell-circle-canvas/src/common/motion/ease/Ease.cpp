/** @file
 * `easeEqual`: the one rule every curve slot in the library compares by;
 * and `readEasing`, a CSS timing function read from its text.
 */

#include "sigilmotion/ease/Ease.h"

#include "sigilmotion/values/Oscillator.h"

#include <cctype>
#include <charconv>
#include <string>
#include <vector>

namespace sigil::motion {

bool easeEqual(const Easing& left, const Easing& right) {
  const bool leftSet = (bool)left, rightSet = (bool)right;
  if (leftSet != rightSet) return false;
  if (!leftSet) return true;
  using Pointer = float (*)(float);
  if (const Pointer* leftPointer = left.target<Pointer>(); leftPointer) {
    const Pointer* rightPointer = right.target<Pointer>();
    return rightPointer && *leftPointer == *rightPointer;
  }
  // A shaped curve keeps its shape and its numbers where they can be read
  // back, and so does an oscillator; anything else is a lambda and stays
  // unequal.
  if (const ease::Curve* leftCurve = left.target<ease::Curve>(); leftCurve) {
    const ease::Curve* rightCurve = right.target<ease::Curve>();
    return rightCurve && *leftCurve == *rightCurve;
  }
  if (const Oscillator* leftWave = left.target<Oscillator>(); leftWave) {
    const Oscillator* rightWave = right.target<Oscillator>();
    return rightWave && *leftWave == *rightWave;
  }
  return false;
}

namespace {

/** @p text without its surrounding spaces, in lower case. */
std::string folded(std::string_view text) {
  size_t first = 0, last = text.size();
  while (first < last && std::isspace((unsigned char)text[first])) ++first;
  while (last > first && std::isspace((unsigned char)text[last - 1])) --last;
  std::string result(text.substr(first, last - first));
  for (char& letter : result) letter = (char)std::tolower((unsigned char)letter);
  return result;
}

/** The comma-separated arguments of `name(…)` in @p text, each folded;
 *  empty when @p text is not a call of @p name. */
std::optional<std::vector<std::string>> argumentsOf(const std::string& text,
                                                    std::string_view name) {
  if (text.size() < name.size() + 2 || text.compare(0, name.size(), name) != 0)
    return std::nullopt;
  std::string_view rest = std::string_view(text).substr(name.size());
  size_t open = 0;
  while (open < rest.size() && std::isspace((unsigned char)rest[open])) ++open;
  if (open >= rest.size() || rest[open] != '(' || rest.back() != ')')
    return std::nullopt;
  rest = rest.substr(open + 1, rest.size() - open - 2);
  std::vector<std::string> arguments;
  size_t start = 0;
  for (size_t at = 0; at <= rest.size(); ++at) {
    if (at == rest.size() || rest[at] == ',') {
      arguments.push_back(folded(rest.substr(start, at - start)));
      start = at + 1;
    }
  }
  return arguments;
}

std::optional<float> number(const std::string& text) {
  float value = 0;
  const char* end = text.data() + text.size();
  const auto [stop, error] = std::from_chars(text.data(), end, value);
  if (error != std::errc() || stop != end || text.empty()) return std::nullopt;
  return value;
}

}  // namespace

std::optional<Easing> readEasing(std::string_view written) {
  const std::string text = folded(written);
  if (text == "linear") return Easing(ease::linear);
  if (text == "ease") return Easing(ease::cubicBezier(0.25f, 0.1f, 0.25f, 1.0f));
  if (text == "ease-in") return Easing(ease::cubicBezier(0.42f, 0, 1, 1));
  if (text == "ease-out") return Easing(ease::cubicBezier(0, 0, 0.58f, 1));
  if (text == "ease-in-out")
    return Easing(ease::cubicBezier(0.42f, 0, 0.58f, 1));
  if (text == "step-start")
    return Easing(ease::steps(1, ease::StepPosition::JumpStart));
  if (text == "step-end") return Easing(ease::steps(1));
  if (const auto arguments = argumentsOf(text, "cubic-bezier")) {
    if (arguments->size() != 4) return std::nullopt;
    float control[4];
    for (size_t index = 0; index < 4; ++index) {
      const std::optional<float> value = number((*arguments)[index]);
      if (!value) return std::nullopt;
      control[index] = *value;
    }
    // CSS keeps both control points' x inside [0, 1], so the curve is a
    // function of time.
    if (control[0] < 0 || control[0] > 1 || control[2] < 0 || control[2] > 1)
      return std::nullopt;
    return Easing(
        ease::cubicBezier(control[0], control[1], control[2], control[3]));
  }
  if (const auto arguments = argumentsOf(text, "steps")) {
    if (arguments->empty() || arguments->size() > 2) return std::nullopt;
    int count = 0;
    const std::string& written = arguments->front();
    const auto [stop, error] = std::from_chars(
        written.data(), written.data() + written.size(), count);
    if (error != std::errc() || stop != written.data() + written.size() ||
        count < 1)
      return std::nullopt;
    ease::StepPosition position = ease::StepPosition::JumpEnd;
    if (arguments->size() == 2) {
      const std::string& term = (*arguments)[1];
      if (term == "jump-start" || term == "start")
        position = ease::StepPosition::JumpStart;
      else if (term == "jump-none")
        position = ease::StepPosition::JumpNone;
      else if (term == "jump-both")
        position = ease::StepPosition::JumpBoth;
      else if (term != "jump-end" && term != "end")
        return std::nullopt;
    }
    // A staircase that jumps at neither end needs two steps to have both.
    if (position == ease::StepPosition::JumpNone && count < 2)
      return std::nullopt;
    return Easing(ease::steps(count, position));
  }
  return std::nullopt;
}

}  // namespace sigil::motion
