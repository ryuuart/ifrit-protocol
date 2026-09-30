#pragma once

// NETSCAPE NAVIGATOR 3.0 on Windows 95, in 1996 pixels: the window, the
// toolbar with its nine buttons and the N throbber, the Location field,
// the directory buttons, the content well and the status bar. Everything
// here is the chrome of the day's system palette — the page is elsewhere.

#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/core/StyleSheet.h>

#include "Artwork.h"

namespace spacejam {

// The window, 650 x 1000, and the rectangle the page is shown in.
constexpr float kWindowWidth = 650, kWindowHeight = 1000;
constexpr float kPageLeft = 5, kPageTop = 134, kPageWidth = 640, kPageHeight = 841;
constexpr float kToolbarTop = 41, kToolbarHeight = 44;
constexpr float kStatusTop = 978;

// The Windows 95 system colours.
const material::Color kFace = material::hexColor(0xC0C0C0);
const material::Color kLight = material::hexColor(0xDFDFDF);
const material::Color kHighlight = material::hexColor(0xFFFFFF);
const material::Color kShadow = material::hexColor(0x808080);
const material::Color kDark = material::hexColor(0x000000);
const material::Color kCaption = material::hexColor(0x000080);

/** THE WINDOWS 95 EDGES. A raised control is white and light grey on the
 *  lit sides over black and dark grey on the far ones; a well is the same
 *  pair the other way up; a status field is one pixel of each. */
inline kit::Bevel raised() {
  return {.light = kHighlight, .shadow = kDark, .antiAlias = false,
          .inner = kit::BevelInner{.gap = 0, .light = kLight, .shadow = kShadow}};
}
inline kit::Bevel sunken() {
  return {.light = kHighlight, .shadow = kShadow, .sunken = true, .antiAlias = false,
          .inner = kit::BevelInner{.gap = 0, .light = kLight, .shadow = kDark}};
}
inline kit::Bevel field() {
  return {.light = kHighlight, .shadow = kShadow, .sunken = true, .antiAlias = false};
}
inline Element dressed(Element element, const kit::Bevel& bevel) {
  kit::bevelled(element, bevel);
  return element;
}

/** THE CHROME'S TYPE: MS Sans Serif at 8 points, which is 11 px and drawn
 *  without smoothing; the caption bold; the page's body text Times, also
 *  unsmoothed, because Windows 95 smoothed nothing. */
inline StyleSheet chromeSheet() {
  const weave::Type system{.face = weave::ports::face({"Tahoma", "Verdana"}, 400), .size = 11,
                           .color = kDark, .aliased = true};
  return StyleSheet{
      rule(":root").font(system),
      rule("caption").fontWeight(700).ink(kHighlight),
      rule("disabled").ink(kShadow),
      rule("copy").font({.face = weave::ports::face({"Times New Roman", "Times"}, 400),
                         .size = 13.33f,
                         .color = material::hexColor(0xFF0000)}),
  };
}

inline Element label(const char* words, const char* role = "label") {
  return text(words).role(role);
}
/** Greyed text as Windows 95 drew it: grey, with a white copy a pixel
 *  down and to the right. */
inline Element greyed(const char* words) {
  return text(words).role("disabled").ink(
      material::from(kShadow).effects(material::Filter::shadow(kHighlight, {.offset = {1, 1}})));
}

// ---------------------------------------------------------------------------
// The toolbar's pictures, each in a 24 x 20 cell. A disabled one is the
// shape in grey over the same shape in white a pixel down and right.

inline Element pictogram(Shape outline, float left, float top, float width, float height,
                         material::Color ink, bool enabled) {
  const Decoration keyline = stroke(1, Fill::color(kDark), PathFormat::Align::Inner);
  if (enabled) return rect(left, top, width, height).shape(outline).fill(ink).stroke(keyline);
  return stack().left(left).top(top).width(width + 1).height(height + 1).children(
      {rect(1, 1, width, height).shape(outline).fill(kHighlight),
       rect(0, 0, width, height).shape(outline).fill(kShadow)});
}

enum class Tool { Back, Forward, Home, Reload, Images, Open, Print, Find, Stop };

inline Element toolPicture(Tool tool, bool enabled) {
  Element cell = stack().width(24).height(20);
  const Decoration keyline = stroke(1, Fill::color(kDark), PathFormat::Align::Inner);
  switch (tool) {
    case Tool::Back:
    case Tool::Forward: {
      std::vector<glm::vec2> arrow{{0, 0.5f}, {0.5f, 0}, {0.5f, 0.28f}, {1, 0.28f},
                                   {1, 0.72f}, {0.5f, 0.72f}, {0.5f, 1}};
      if (tool == Tool::Forward)
        for (glm::vec2& corner : arrow) corner.x = 1 - corner.x;
      cell.children({pictogram(unitPolygon(arrow), 2, 3, 20, 14, C5(0x00A5A5), enabled)});
      break;
    }
    case Tool::Home:
      cell.children({rect(16, 2, 3, 6).fill(C5(0x8C2121)).stroke(keyline),
                     rect(5, 9, 14, 10).fill(C5(0xE7D6A5)).stroke(keyline),
                     rect(10, 12, 4, 7).fill(C5(0x8C5A21)).stroke(keyline),
                     rect(2, 1, 20, 9).shape(unitPolygon({{0, 1}, {0.5f, 0}, {1, 1}}))
                         .fill(C5(0xC62121)).stroke(keyline)});
      break;
    case Tool::Reload:
      cell.children({kit::disc({12, 10}, 7).shape(shapes::arc(-60, 300))
                         .stroke(stroke(2.5f, Fill::color(C5(0x21A521)))),
                     rect(14, 0, 8, 8).shape(unitPolygon({{0, 0}, {1, 0.55f}, {0.2f, 1}}))
                         .fill(C5(0x21A521)).stroke(keyline)});
      break;
    case Tool::Images:
      cell.children({rect(2, 3, 20, 14).fill(material::linearGradient(
                         {0, 0}, {0, 1}, {{0.0f, C5(0x6BB5FF)}, {1.0f, C5(0xFFFFFF)}}))
                         .stroke(keyline),
                     rect(3, 8, 18, 8).shape(unitPolygon({{0, 1}, {0.35f, 0.1f}, {0.6f, 0.6f},
                                                          {0.75f, 0.35f}, {1, 1}}))
                         .fill(C5(0x218439)),
                     kit::dot({16, 7}, 2, Fill::color(C5(0xFFEF00)))});
      break;
    case Tool::Open:
      cell.children({rect(2, 2, 8, 4).fill(C5(0xD6A518)).stroke(keyline),
                     rect(2, 4, 18, 13).fill(C5(0xD6A518)).stroke(keyline),
                     rect(3, 8, 20, 9).shape(unitPolygon({{0.12f, 0}, {1, 0}, {0.88f, 1}, {0, 1}}))
                         .fill(C5(0xFFD65A)).stroke(keyline)});
      break;
    case Tool::Print:
      cell.children({rect(6, 1, 12, 8).fill(kHighlight).stroke(keyline),
                     rect(2, 8, 20, 8).fill(kFace).stroke(keyline),
                     rect(5, 11, 14, 1).fill(kDark),
                     rect(6, 14, 12, 5).fill(kHighlight).stroke(keyline)});
      break;
    case Tool::Find:
      cell.children({rect(13, 12, 9, 3).rotate(40).fill(C5(0x8C5A21)).stroke(keyline),
                     kit::dot({10, 8}, 6.5f, Fill::color(C5(0xB5DEFF)))
                         .stroke(stroke(2, Fill::color(C5(0x21316B)), PathFormat::Align::Inner))});
      break;
    case Tool::Stop:
      cell.children({pictogram(shapes::polygon(8, 22.5f), 3, 1, 18, 18, C5(0xE71010), enabled)});
      if (enabled) cell.children({rect(7, 8.5f, 10, 3).fill(kHighlight)});
      break;
  }
  return cell;
}

/** One toolbar button: the picture over its name, raised; pressed, the
 *  bevel turns over and the contents drop a pixel. */
inline Element toolButton(Tool tool, const char* name, bool enabled, bool pressed = false) {
  Element contents = box().column().alignItems(Align::Center).gap(2).children(
      {toolPicture(tool, enabled), enabled ? label(name) : greyed(name)});
  if (pressed) contents.translateX(1).translateY(1);
  return dressed(box().width(47).height(40).alignItems(Align::Center)
                     .justifyContent(Justify::Center).fill(kFace).children({std::move(contents)}),
                 pressed ? sunken() : raised());
}

// ---------------------------------------------------------------------------
// The N throbber: Netscape's letter standing on a planet's limb in a night
// sky. While a page loads, meteors cross the sky behind it.

constexpr float kThrobberLeft = 596, kThrobberSize = 44;

inline Element throbberSky() {
  Element sky = stack().inset(0).fill(material::linearGradient(
      {0, 0}, {0, 1}, {{0.0f, material::hexColor(0x000021)}, {1.0f, material::hexColor(0x18086B)}}));
  const float stars[6][2] = {{5, 6}, {15, 3}, {34, 8}, {38, 20}, {8, 18}, {27, 4}};
  for (const auto& star : stars)
    sky.children({rect(star[0], star[1], 1, 1).fill(kHighlight)});
  sky.children({kit::disc({22, 70}, 38).shape(shapes::circle())
                     .fill(material::radialGradient({0.5f, 0.0f}, 1.0f,
                                                    {{0.0f, material::hexColor(0x3984C6)},
                                                     {0.35f, material::hexColor(0x10427B)},
                                                     {1.0f, material::hexColor(0x000842)}}))});
  return sky;
}

inline Element throbberLetter() {
  return text("N")
      .font({.face = weave::ports::face({"Times New Roman", "Times"}, 700), .size = 40,
             .aliased = true})
      .ink(material::from(material::hexColor(0xDEE7FF))
               .effects(material::Filter::shadow(material::hexColor(0x000000), {.offset = {1, 1}})))
      .left(9)
      .top(-3);
}

/** Two meteors on the diagonal, as one strip the throbber pans along. */
inline Element meteors() {
  Element strip = stack().width(44).height(44);
  const float heads[2][2] = {{30, 10}, {12, 30}};
  for (const auto& head : heads)
    strip.children({rect(head[0] - 14, head[1] - 1, 16, 2)
                        .fill(material::linearGradient(
                            {0, 0}, {1, 0},
                            {{0.0f, {1, 1, 1, 0}}, {0.8f, {0.8f, 0.9f, 1, 0.8f}}, {1.0f, {1, 1, 1, 1}}}))
                        .rotate(35)});
  return strip;
}

// ---------------------------------------------------------------------------
// The status bar's key: the unlocked-document key, broken in two.

inline Element brokenKey() {
  return stack().width(18).height(14).children(
      {kit::ring({5, 7}, 3.5f, stroke(2, Fill::color(C5(0x8C7318)))),
       rect(8, 6, 3, 2).fill(C5(0xC6A521)),
       rect(12, 5, 6, 2).rotate(-20).fill(C5(0xC6A521)),
       rect(15, 7, 1, 3).fill(C5(0xC6A521))});
}

// ---------------------------------------------------------------------------
// The mouse pointers, as the one-bit masks they were: `#` black, `.`
// white, anything else transparent.

inline constexpr std::array<const char*, 19> kArrowPointer{
    "#",          "##",         "#.#",         "#..#",        "#...#",
    "#....#",     "#.....#",    "#......#",    "#.......#",   "#........#",
    "#.....#####", "#..#..#",   "#.# #..#",    "##  #..#",    "#    #..#",
    "     #..#",  "      #..#", "      #..#",  "       ##"};
inline constexpr std::array<const char*, 21> kHandPointer{
    "     ##",           "    #..#",          "    #..#",          "    #..#",
    "    #..#",          "    #..###",        "    #..#..###",     "    #..#..#..##",
    "##  #..#..#..#.#",  "#..##........#.#", "#...#..........#", " #..#..........#",
    "  #.#..........#",  "  #............#", "   #...........#", "   #..........#",
    "    #.........#",   "    #........#",   "     #.......#",   "     #.......#",
    "     #########"};

}  // namespace spacejam
