/** @file
 * What the settled layout says rather than keeps quiet about: a flex
 * container whose children run past its box where no clip answers for
 * the spill, and a text leaf a stated minimum lets a column shrink below
 * its lines. Both are told through the reporting door, once when they
 * start and not again while they stand, so a still tree and a relayout
 * that leaves them as they were say nothing.
 */

#include <sigilweave/paragraph/Paragraph.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "layout/FlexItems.h"
#include "runtime/ComposeRuntime.h"
#include "runtime/Place.h"
#include "runtime/Warning.h"

namespace sigil::compose {

using namespace detail;

namespace {

/** HOW FAR PAST ITS ROOM A BOX MAY RUN BEFORE IT IS SAID TO OVERFLOW: Yoga
 *  rounds every edge to its pixel grid, so a line that exactly fits can
 *  come out a fraction over and is not an overflow. */
constexpr float kOverflowTolerance = 1.0f;

/** Appends the UTF-8 form of one code point to @p into. */
void appendCodePoint(std::string& into, char32_t point) {
  if (point < 0x80) {
    into += (char)point;
  } else if (point < 0x800) {
    into += (char)(0xC0 | (point >> 6));
    into += (char)(0x80 | (point & 0x3F));
  } else if (point < 0x10000) {
    into += (char)(0xE0 | (point >> 12));
    into += (char)(0x80 | ((point >> 6) & 0x3F));
    into += (char)(0x80 | (point & 0x3F));
  } else {
    into += (char)(0xF0 | (point >> 18));
    into += (char)(0x80 | ((point >> 12) & 0x3F));
    into += (char)(0x80 | ((point >> 6) & 0x3F));
    into += (char)(0x80 | (point & 0x3F));
  }
}

/** THE FIRST WORDS OF A TEXT LEAF, as a reader would find them on the page:
 *  its opening few words with every run of white space as one space, and an
 *  ellipsis where the text goes on. Empty for a leaf with no paragraph. */
std::string firstWordsOf(const Instance& inst) {
  constexpr size_t kWords = 6;
  constexpr size_t kCodePoints = 48;
  if (!inst.paragraph) return {};
  const std::u16string& text = inst.paragraph->text();
  std::string words;
  size_t wordCount = 0;
  size_t codePoints = 0;
  bool inSpace = true;
  bool truncated = false;
  for (size_t index = 0; index < text.size(); ++index) {
    char32_t point = text[index];
    // A surrogate pair is one code point.
    if (point >= 0xD800 && point < 0xDC00 && index + 1 < text.size() &&
        text[index + 1] >= 0xDC00 && text[index + 1] < 0xE000) {
      point = 0x10000 + ((point - 0xD800) << 10) + (text[index + 1] - 0xDC00);
      ++index;
    }
    const bool space = point == u' ' || point == u'\n' || point == u'\t' ||
                       point == u'\r' || point == 0x2028 || point == 0x2029;
    if (space) {
      inSpace = true;
      continue;
    }
    if (inSpace) {
      if (wordCount == kWords || codePoints >= kCodePoints) {
        truncated = true;
        break;
      }
      if (wordCount > 0) words += ' ';
      ++wordCount;
      inSpace = false;
    }
    if (codePoints == kCodePoints) {
      truncated = true;
      break;
    }
    appendCodePoint(words, point);
    ++codePoints;
  }
  if (truncated) words += "…";
  return words;
}

/** A NODE AS AN AUTHOR WOULD FIND IT: the text it sets, where it sets one,
 *  else its role, else where it stands. */
std::string describe(const Instance& inst) {
  std::string place = placeOf(inst);
  if (inst.description->kind == Kind::Text) {
    const std::string words = firstWordsOf(inst);
    if (!words.empty()) return "the text \"" + words + "\" at " + place;
    return "the text at " + place;
  }
  const ElementNode& node = *inst.description;
  if (node.cascadeData && node.cascadeData->role)
    return "the " + node.cascadeData->role->name + " at " + place;
  return "the box at " + place;
}

bool runsDown(const Instance& container) {
  return !mainAxisHorizontal(container.computed.layout.direction);
}

/** The in-flow items of @p container's flex line, in child order: a node
 *  with no box hands its children to the line, and an absolute child and
 *  a node with no box at all stand out of it. */
void gatherItems(const Instance& container,
                 std::vector<const Instance*>& items) {
  for (const auto& child : container.children) {
    if (!child || !child->yoga) continue;
    const Display display = child->computed.layout.display;
    if (display == Display::None) continue;
    if (display == Display::Contents) {
      gatherItems(*child, items);
      continue;
    }
    if (YGNodeStyleGetPositionType(child->yoga) == YGPositionTypeAbsolute)
      continue;
    items.push_back(child.get());
  }
}

struct MainAxis {
  YGEdge start, end;
  bool down;
};

MainAxis mainAxisOf(const Instance& container) {
  const bool down = runsDown(container);
  return {down ? YGEdgeTop : YGEdgeLeft, down ? YGEdgeBottom : YGEdgeRight,
          down};
}

float positionAlong(const Instance& inst, const MainAxis& axis) {
  return axis.down ? YGNodeLayoutGetTop(inst.yoga)
                   : YGNodeLayoutGetLeft(inst.yoga);
}

float sizeAlong(const Instance& inst, const MainAxis& axis) {
  return axis.down ? YGNodeLayoutGetHeight(inst.yoga)
                   : YGNodeLayoutGetWidth(inst.yoga);
}

/** Whether @p inst is a flex container whose line the report reads: one
 *  Yoga lays out, in one line, that lets what spills show. */
bool readsOverflow(const Instance& inst) {
  if (!inst.yoga || inst.description->kind == Kind::Text) return false;
  const LayoutProps& layout = inst.computed.layout;
  return layout.display == Display::Flex && layout.wrap == FlexWrap::NoWrap &&
         !inst.description->arranges() && !inst.computed.clipContent &&
         childrenCarryYoga(inst);
}

/** WHETHER A SPILL IS SAID: @p spill, in @p container's layout space, is
 *  said unless a clipping ancestor answers for it. One whose box holds the
 *  whole spill bounds it, so it stays inside the region its author closed;
 *  and a spill the clips above it cut away entirely draws nothing. A spill
 *  a clip cuts in part is said, as the cut shows. Each ancestor's box is
 *  brought into the container's space by the offsets Yoga placed the boxes
 *  between them at; a node with no box of its own sits at no offset and
 *  clips nothing. */
bool spillIsSaid(const Instance& container, const SkRect& spill) {
  SkRect showing = spill;
  float left = 0.0f;
  float top = 0.0f;
  for (const Instance* node = &container; node->parent && node->yoga;
       node = node->parent) {
    left -= YGNodeLayoutGetLeft(node->yoga);
    top -= YGNodeLayoutGetTop(node->yoga);
    const Instance& ancestor = *node->parent;
    if (!ancestor.yoga) break;
    if (!ancestor.computed.clipContent ||
        ancestor.computed.layout.display == Display::Contents)
      continue;
    SkRect box =
        SkRect::MakeXYWH(left, top, YGNodeLayoutGetWidth(ancestor.yoga),
                         YGNodeLayoutGetHeight(ancestor.yoga));
    if (box.makeOutset(kOverflowTolerance, kOverflowTolerance).contains(spill))
      return false;
    if (!showing.intersect(box)) return false;
  }
  return true;
}

}  // namespace

void Composer::Impl::reportLayoutOverflow(Instance& inst) {
  if (inst.computed.layout.display == Display::None) return;
  // AN OVER-FULL LINE. CSS lets a sized box's children spill out of it
  // when its overflow is visible, so a spill is not an error; it is
  // reported because what it draws — children overlapping the next box
  // or running off the page — is rarely what was meant, and nothing else
  // in the tree says it happened.
  bool overflowing = false;
  if (readsOverflow(inst)) {
    std::vector<const Instance*> items;
    gatherItems(inst, items);
    if (!items.empty()) {
      const MainAxis axis = mainAxisOf(inst);
      const float roomStart = YGNodeLayoutGetPadding(inst.yoga, axis.start) +
                              YGNodeLayoutGetBorder(inst.yoga, axis.start);
      const float roomEnd = sizeAlong(inst, axis) -
                            YGNodeLayoutGetPadding(inst.yoga, axis.end) -
                            YGNodeLayoutGetBorder(inst.yoga, axis.end);
      float lineStart = INFINITY;
      float lineEnd = -INFINITY;
      float crossStart = INFINITY;
      float crossEnd = -INFINITY;
      const Instance* firstPast = nullptr;
      for (const Instance* item : items) {
        const float start = positionAlong(*item, axis) -
                            YGNodeLayoutGetMargin(item->yoga, axis.start);
        const float end = positionAlong(*item, axis) + sizeAlong(*item, axis) +
                          YGNodeLayoutGetMargin(item->yoga, axis.end);
        lineStart = std::min(lineStart, start);
        lineEnd = std::max(lineEnd, end);
        const float across = axis.down ? YGNodeLayoutGetLeft(item->yoga)
                                       : YGNodeLayoutGetTop(item->yoga);
        const float acrossSize = axis.down ? YGNodeLayoutGetWidth(item->yoga)
                                           : YGNodeLayoutGetHeight(item->yoga);
        crossStart = std::min(crossStart, across);
        crossEnd = std::max(crossEnd, across + acrossSize);
        if (!firstPast && (start < roomStart - kOverflowTolerance ||
                           end > roomEnd + kOverflowTolerance))
          firstPast = item;
      }
      const float room = std::max(roomEnd - roomStart, 0.0f);
      const float line = lineEnd - lineStart;
      // What the line holds past each end of the room, across the band
      // its items stand in; a clipping ancestor may answer for it.
      const auto said = [&](float from, float to) {
        if (to - from <= kOverflowTolerance) return false;
        return spillIsSaid(
            inst, axis.down ? SkRect::MakeLTRB(crossStart, from, crossEnd, to)
                            : SkRect::MakeLTRB(from, crossStart, to, crossEnd));
      };
      overflowing = line > room + kOverflowTolerance &&
                    (said(lineStart, std::min(roomStart, lineEnd)) ||
                     said(std::max(roomEnd, lineStart), lineEnd));
      if (overflowing && !inst.overflowReported) {
        const char* across = axis.down ? "tall" : "wide";
        const std::string container = (axis.down ? std::string("the column at ")
                                                 : std::string("the row at ")) +
                                      placeOf(inst);
        const std::string past =
            firstPast
                ? ", the first past its edge being " + describe(*firstPast)
                : std::string();
        reportWarning(
            "%s holds children %.0f pixels %s in %.0f pixels of room, so "
            "they run %.0f pixels past its box and overlap what follows or "
            "spill out of it%s. Give it the room, let a child shrink, or "
            "state overflow(Overflow::Clip) on it where the spill is meant.",
            container.c_str(), std::round(line), across, std::round(room),
            std::round(line - room), past.c_str());
      }
    }
  }
  inst.overflowReported = overflowing;

  // A TEXT LEAF A STATED MINIMUM LETS A COLUMN SHRINK. Unstated, its
  // minimum is CSS's automatic one, which holds the leaf at its lines; a
  // stated minimum height or basis lets a column short of room shrink it
  // below them. The lines are drawn whole whatever box the leaf was given,
  // so a shrunk leaf paints over what the column placed after it.
  bool squashed = false;
  if (linesRunAcrossAColumn(inst) && !keepsItsLines(inst) &&
      inst.measuredRev == inst.contentRev) {
    const Instance& container = *flexContainerOf(inst);
    const Insets pad = paddingOf(inst);
    const float borderAcross = YGNodeLayoutGetBorder(inst.yoga, YGEdgeLeft) +
                               YGNodeLayoutGetBorder(inst.yoga, YGEdgeRight);
    const float borderDown = YGNodeLayoutGetBorder(inst.yoga, YGEdgeTop) +
                             YGNodeLayoutGetBorder(inst.yoga, YGEdgeBottom);
    const float width = YGNodeLayoutGetWidth(inst.yoga);
    const float height = YGNodeLayoutGetHeight(inst.yoga);
    const float measure = width - pad.across() - borderAcross;
    // The lines held are the ones the box breaks: they were set at a
    // measure no narrower than the box and fit inside it, so the box would
    // break them where they already broke.
    const bool linesFitTheBox = inst.measuredSize.width <= measure + 0.5f &&
                                measure <= inst.measuredForWidth + 0.5f;
    if (linesFitTheBox) {
      // A stated height or ceiling is the author's own floor: a leaf set
      // shorter than its lines on purpose has not been shrunk.
      float floor = inst.measuredSize.height + pad.down() + borderDown;
      const float containerHeight = YGNodeLayoutGetHeight(container.yoga);
      const auto bound = [&](YGValue value) {
        if (value.unit == YGUnitPoint) floor = std::min(floor, value.value);
        if (value.unit == YGUnitPercent)
          floor = std::min(floor, value.value * containerHeight * 0.01f);
      };
      bound(YGNodeStyleGetHeight(inst.yoga));
      bound(YGNodeStyleGetMaxHeight(inst.yoga));
      squashed = height < floor - kOverflowTolerance;
      if (squashed && !inst.squashReported) {
        const char* cause =
            inst.computed.layout.minHeight.unit != Dimension::Unit::Auto
                ? "its stated minimum height lets the column shrink it; "
                  "leave the minimum unstated and it keeps the height of "
                  "its lines"
                : "its stated basis lets the column shrink it; leave the "
                  "basis unstated and it keeps the height of its lines";
        reportWarning(
            "%s is laid out %.0f pixels shorter than its lines in the column "
            "at %s, so they paint over what the column places after it: %s.",
            describe(inst).c_str(), std::round(floor - height),
            placeOf(container).c_str(), cause);
      }
    }
  }
  inst.squashReported = squashed;

  for (const auto& child : inst.children)
    if (child) reportLayoutOverflow(*child);
}

}  // namespace sigil::compose
