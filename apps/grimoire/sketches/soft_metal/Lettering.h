#pragma once

#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Transform.h>

#include <string_view>
#include <vector>

namespace title {
namespace path = sigil::geometry::path;

// Hand-drawn extended lettering: flat terminals and rounded machine cuts.
// Each glyph has a five-unit cap height; counters are even-odd contours.
inline path::Outline glyph(char c) {
  std::string_view data;
  switch (c) {
    case 'T':
      data = "M0 0H6V1.1H3.65V5H2.35V1.1H0Z";
      break;
    case 'E':
      data =
          "M6 0H1.25Q0 0 0 1.25V3.75Q0 5 1.25 "
          "5H6V3.9H1.3V3.05H5.65V1.95H1.3V1.1H6Z";
      break;
    case 'R':
      data =
          "M0 0H4.7Q6.2 0 6.2 1.45Q6.2 2.8 4.9 2.95L6.4 5H4.85L3.4 3H1.3V5H0Z "
          "M1.3 1.1V1.9H4.45Q4.9 1.9 4.9 1.5Q4.9 1.1 4.45 1.1Z";
      break;
    case 'M':
      data =
          "M0 5V0.9Q0 0 0.85 0Q1.35 0 1.75 0.55L3.65 3.55L5.55 0.55Q5.95 0 "
          "6.45 0Q7.3 0 7.3 0.9V5H6V2L4.3 4.7Q3.65 5.3 3 4.7L1.3 2V5Z";
      break;
    case 'I':
      data = "M0 0H1.3V5H0Z";
      break;
    case 'N':
      data =
          "M0 5V0.8Q0 0 0.8 0Q1.2 0 1.6 0.4L4.85 3.4V0H6.15V4.2Q6.15 5 5.35 "
          "5Q4.95 5 4.55 4.6L1.3 1.6V5Z";
      break;
    case 'A':
      data =
          "M0 5L2.25 0.8Q2.7 0 3.4 0Q4.1 0 4.55 0.8L6.8 5H5.3L4.65 "
          "3.85H2.15L1.5 5Z M2.65 2.8H4.15L3.4 1.35Z";
      break;
    case 'O':
      data =
          "M1.55 0H4.7Q6.25 0 6.25 1.55V3.45Q6.25 5 4.7 5H1.55Q0 5 0 "
          "3.45V1.55Q0 0 1.55 0Z M1.9 1.15Q1.3 1.15 1.3 1.75V3.25Q1.3 3.85 1.9 "
          "3.85H4.35Q4.95 3.85 4.95 3.25V1.75Q4.95 1.15 4.35 1.15Z";
      break;
    case '2':
      data =
          "M0 0H4.8Q6.15 0 6.15 1.35V1.85Q6.15 3.1 4.8 "
          "3.1H1.3V3.9H6.15V5H0V3.3Q0 2 1.4 2H4.85V1.1H0Z";
      break;
    case 'J':
      data = "M3.7 0H5V3.6Q5 5 3.6 5H0V3.85H3.1Q3.7 3.85 3.7 3.25Z";
      break;
    case 'U':
      data =
          "M0 0H1.3V3.15Q1.3 3.85 2 3.85H4.25Q4.95 3.85 4.95 "
          "3.15V0H6.25V3.5Q6.25 5 4.75 5H1.5Q0 5 0 3.5Z";
      break;
    case 'D':
      data =
          "M0 0H4.6Q6.25 0 6.25 1.65V3.35Q6.25 5 4.6 5H0Z M1.3 "
          "1.15V3.85H4.1Q4.95 3.85 4.95 3V2Q4.95 1.15 4.1 1.15Z";
      break;
    case 'G':
      data =
          "M6.2 0H1.6Q0 0 0 1.6V3.4Q0 5 1.6 5H6.2V2H3.1V3.05H4.9V3.85H1.95Q1.3 "
          "3.85 1.3 3.2V1.8Q1.3 1.15 1.95 1.15H6.2Z";
      break;
    case 'Y':
      data =
          "M0 0H1.3V1.35Q1.3 2 1.95 2H4.25Q4.9 2 4.9 1.35V0H6.2V3.45Q6.2 5 "
          "4.65 5H0.2V3.9H4.25Q4.9 3.9 4.9 3.25V3.1H1.55Q0 3.1 0 1.55Z";
      break;
  }
  return path::Outline::svg(data).withFillRule(path::FillRule::EvenOdd);
}

inline path::Outline inscription(std::string_view words) {
  std::vector<path::Outline> letters;
  float cursor = 0;
  for (char c : words) {
    if (c == ' ') {
      cursor += 2.4f;
      continue;
    }
    const auto shape = glyph(c);
    letters.push_back(
        shape.transformed(path::Transform::translate({cursor, 0})));
    cursor += shape.bounds().width() + 0.68f;
  }
  return path::operations::unite(letters);
}
}  // namespace title
