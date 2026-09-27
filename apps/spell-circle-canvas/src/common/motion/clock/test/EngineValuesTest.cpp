/** @file
 * One engine over every animated value: an animation, a timeline item and
 * a collective run on a float, a `glm::vec2`, a `glm::vec3`, a `Duration`
 * and a value that is not additive and states its own line — all through
 * `interpolate()`, with no path of their own. Every frame is a stated one.
 */

#include "support/StandsAlone.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilmotion/schedule/Stagger.h>
#include <sigilmotion/values/Animatable.h>
#include <sigilmotion/values/Interpolate.h>
#include <sigilmotion/values/Tween.h>

#include <chrono>
#include <vector>

using namespace sigil::motion;
using namespace std::chrono_literals;

namespace swatch {
/** A VALUE THAT DOES NOT ADD — a colour-like pair whose line between two
 *  values is its own, found by argument-dependent lookup the way
 *  `material::interpolate` is for a colour: the level walks straight and
 *  the name switches half way. */
struct Swatch {
  float level = 0.0f;
  int name = 0;
  bool operator==(const Swatch&) const = default;
};
Swatch interpolate(const Swatch& start, const Swatch& end, float amount) {
  return {start.level + (end.level - start.level) * amount,
          amount < 0.5f ? start.name : end.name};
}
}  // namespace swatch

namespace {

template <typename T>
Tween<T> linearTo(T to, Duration length = 1s) {
  return {.to = to, .duration = length, .ease = ease::linear};
}

}  // namespace

TEST(EngineValues, TheSeamIsTheStraightLineForAValueThatAdds) {
  EXPECT_FLOAT_EQ(interpolate(2.0f, 4.0f, 0.25f), 2.5f);
  const glm::vec2 point = interpolate(glm::vec2(0, 10), glm::vec2(4, 20), 0.5f);
  EXPECT_EQ(point, glm::vec2(2, 15));
  EXPECT_EQ(interpolate(Duration(1s), Duration(3s), 0.5f), Duration(2s));
  static_assert(Additive<glm::vec3>);
  static_assert(!Additive<swatch::Swatch>);
  static_assert(Interpolable<swatch::Swatch>);
}

TEST(EngineValues, AVectorAnimatesOnTheEngineThroughTheSameBody) {
  Engine engine;
  Animatable<glm::vec2> at = animatable(glm::vec2(0, 0));
  engine.animate(at, linearTo(glm::vec2(10, -20)));
  engine.advance(500ms);
  EXPECT_NEAR(at.value().x, 5.0f, 1e-4f);
  EXPECT_NEAR(at.value().y, -10.0f, 1e-4f);
  engine.advance(2s);
  EXPECT_EQ(at.value(), glm::vec2(10, -20));
  EXPECT_FALSE(engine.isRunning());

  Animatable<glm::vec3> tint = glm::vec3(1, 1, 1);  // a constant, made live
  engine.animate(tint, {.from = glm::vec3(0), .to = glm::vec3(1, 0.5f, 0),
                        .duration = 1s, .ease = ease::linear});
  engine.advance(2500ms);
  EXPECT_NEAR(tint.value().y, 0.25f, 1e-4f);
}

TEST(EngineValues, AValueThatDoesNotAddMovesOnItsOwnLine) {
  Engine engine;
  Animatable<swatch::Swatch> value = animatable(swatch::Swatch{0.0f, 1});
  engine.animate(value, linearTo(swatch::Swatch{1.0f, 2}));
  engine.advance(250ms);
  EXPECT_NEAR(value.value().level, 0.25f, 1e-4f);
  EXPECT_EQ(value.value().name, 1);
  engine.advance(750ms);
  EXPECT_EQ(value.value().name, 2);
}

TEST(EngineValues, BlendOnAValueThatDoesNotAddStartsAgainFromWhereItStands) {
  Engine engine;
  Animatable<swatch::Swatch> value = animatable(swatch::Swatch{0.0f, 1});
  engine.animate(value, linearTo(swatch::Swatch{1.0f, 1}));
  engine.advance(500ms);
  Tween<swatch::Swatch> change = linearTo(swatch::Swatch{0.0f, 1});
  change.composition = Composition::Blend;
  engine.animate(value, change);
  EXPECT_NEAR(value.value().level, 0.5f, 1e-4f);  // no jump
  engine.advance(1000ms);
  EXPECT_NEAR(value.value().level, 0.25f, 1e-4f);  // half way back from 0.5
}

TEST(EngineValues, ATimelinePlacesAnyValueType) {
  Engine engine;
  Animatable<glm::vec2> at = animatable(glm::vec2(0));
  Animatable<float> fade = animatable(0.0f);
  engine.timeline()
      .add(at, linearTo(glm::vec2(4, 8)))
      .add(fade, linearTo(1.0f), afterPrevious());
  engine.advance(1500ms);
  EXPECT_EQ(at.value(), glm::vec2(4, 8));
  EXPECT_NEAR(fade.value(), 0.5f, 1e-4f);
}

TEST(EngineValues, ACollectiveResolvesAStaggerFromEachTargetsPlace) {
  Engine engine;
  std::vector<Animatable<float>> letters(3, Animatable<float>(0.0f));
  engine.animate(letters, {.to = stagger({10.0f, 30.0f}), .duration = 1s,
                           .delay = stagger(500ms), .ease = ease::linear});
  engine.advance(1s);
  EXPECT_NEAR(letters[0].value(), 10.0f, 1e-4f);  // done: its own `to`
  EXPECT_NEAR(letters[1].value(), 10.0f, 1e-4f);  // half of 20
  EXPECT_NEAR(letters[2].value(), 0.0f, 1e-4f);   // still waiting
  engine.advance(3s);
  EXPECT_NEAR(letters[2].value(), 30.0f, 1e-4f);
  EXPECT_FALSE(engine.isRunning());
}

TEST(EngineValues, AnItemAfterACollectiveFollowsItsLastMember) {
  Engine engine;
  std::vector<Animatable<glm::vec2>> dots(2, Animatable<glm::vec2>(glm::vec2(0)));
  Animatable<float> rule = animatable(0.0f);
  engine.timeline()
      .add(dots, {.to = glm::vec2(1), .duration = 1s, .delay = stagger(1s),
                  .ease = ease::linear})
      .add(rule, linearTo(1.0f));
  engine.advance(2500ms);  // the second dot ends at 2s
  EXPECT_NEAR(rule.value(), 0.5f, 1e-4f);
}
