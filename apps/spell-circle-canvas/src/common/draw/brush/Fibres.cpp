/** @file
 * The fibre tip: parallel hairs along the stroke, each intermittently dry.
 */

#include "DabStyle.h"
#include "Executors.h"

#include <sigildraw/Pen.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::brush {

namespace {

struct FibreMark {
  SkPoint position;
  float width;
  float opacity;
};

/** How many marks one stroked run joins before the next begins. */
constexpr size_t kRunLength = 9;

void drawFibreRun(Pen& pen, const Tool& tool, std::span<const FibreMark> marks,
                  float fibreOpacity) {
  if (marks.empty()) return;
  if (marks.size() == 1) {
    pen.stroke(pigment(tool, marks.front().opacity * fibreOpacity));
    pen.strokeWeight(std::max(0.15f, marks.front().width));
    pen.point(marks.front().position.fX, marks.front().position.fY);
    return;
  }

  for (size_t begin = 0; begin + 1 < marks.size();) {
    const size_t end = std::min(marks.size(), begin + kRunLength);
    float width = 0.0f;
    float opacity = 0.0f;
    for (size_t index = begin; index < end; ++index) {
      width += marks[index].width;
      opacity += marks[index].opacity;
    }
    const float count = (float)(end - begin);
    pen.stroke(pigment(tool, opacity / count * fibreOpacity));
    pen.strokeWeight(std::max(0.15f, width / count));
    pen.noFill();
    pen.beginShape();
    for (size_t index = begin; index < end; ++index)
      pen.vertex(marks[index].position.fX, marks[index].position.fY);
    pen.endShape();
    if (end == marks.size()) break;
    begin = end - 1;
  }
}

/** One dab alone: a row of dots across the centreline. */
void depositFibreDab(Pen& pen, const Tool& tool, const Dab& dab,
                     const DabStyle& style) {
  const int fibres = std::max(1, tool.bristles);
  const float normal = dab.direction + HALF_PI;
  pen.noStroke();
  for (int i = 0; i < fibres; ++i) {
    if (pen.random() > std::clamp(tool.density, 0.0f, 1.0f)) continue;
    const float lane = (((float)i + 0.5f) / (float)fibres - 0.5f) * style.size;
    const float radius =
        std::max(0.15f, style.size / (float)fibres * pen.random(0.35f, 0.9f));
    pen.fill(pigment(tool, style.opacity * pen.random(0.5f, 1.1f)));
    pen.circle(style.position.fX + std::cos(normal) * lane,
               style.position.fY + std::sin(normal) * lane, radius);
  }
}

}  // namespace

void depositFibres(Pen& pen, const Tool& tool, std::span<const Dab> dabs) {
  if (dabs.empty()) return;
  std::vector<DabStyle> styles;
  styles.reserve(dabs.size());
  for (const Dab& dab : dabs) styles.push_back(styleDab(pen, tool, dab, false));
  if (dabs.size() == 1) {
    depositFibreDab(pen, tool, dabs.front(), styles.front());
    return;
  }

  pen.strokeCap(ROUND);
  pen.strokeJoin(ROUND);
  const int fibres = std::max(1, tool.bristles);
  const float density = std::clamp(tool.density, 0.0f, 1.0f);
  for (int fibre = 0; fibre < fibres; ++fibre) {
    if (pen.random() > 0.35f + density * 0.65f) continue;
    const float lane = ((float)fibre + 0.5f) / (float)fibres - 0.5f +
                       pen.random(-0.48f, 0.48f) / (float)fibres;
    const float edge = std::abs(lane) * 2.0f;
    const float fibreOpacity =
        pen.random(0.52f, 1.12f) * (1.0f - edge * pen.random(0.0f, 0.22f));
    const float drySeed = pen.random(0.0f, 2048.0f);
    std::vector<FibreMark> run;
    const auto flush = [&] {
      if (!run.empty()) drawFibreRun(pen, tool, run, fibreOpacity);
      run.clear();
    };
    for (size_t index = 0; index < dabs.size(); ++index) {
      const Dab& dab = dabs[index];
      const DabStyle& style = styles[index];
      const float dryBand = pen.noise(dab.distance * 0.0065f, drySeed, 0.31f);
      const float dryThreshold = (1.0f - density) * (0.42f + edge * 0.18f);
      if (!(style.size > 0.0f) || !(style.opacity > 0.0f) ||
          dryBand < dryThreshold || pen.random() < (1.0f - density) * 0.025f) {
        flush();
        continue;
      }
      const float normal = dab.direction + HALF_PI;
      const float sharpness = std::clamp(tool.sharpness, 0.0f, 1.0f);
      const float distribution =
          sharpness +
          (1.0f - sharpness) * (0.35f + pen.noise(dab.distance * 0.031f,
                                                  (float)fibre * 0.23f, 0.79f) *
                                            1.3f);
      const float drift =
          (pen.noise(dab.distance * 0.018f, (float)fibre * 0.37f, 0.41f) -
           0.5f) *
          2.0f * tool.scatter * distribution * (0.55f + edge * 0.8f);
      const float offset = lane * style.size + drift;
      run.push_back({
          .position = {style.position.fX + std::cos(normal) * offset,
                       style.position.fY + std::sin(normal) * offset},
          .width = style.size / (float)fibres * pen.random(0.58f, 1.18f),
          .opacity = style.opacity,
      });
    }
    flush();
  }
}

}  // namespace sigil::draw::brush
