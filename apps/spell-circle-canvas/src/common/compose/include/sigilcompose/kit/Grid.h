#pragma once

/** @file
 * The grid: ONE layout value over the LayoutScheme seam that divides a
 * container into sized tracks, names rectangular regions of those tracks,
 * and places a child in a region by name.
 *
 * It is the general form of every arrangement a page is divided into —
 * equal shares, unequal columns sized by what is in them, a fixed rail
 * beside a flexible body, a wrapped run of panels — because a track
 * carries a SIZING FUNCTION rather than a width, and the four functions
 * below cover the lot.
 */

#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Layout.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::compose::layouts {

/** HOW ONE TRACK IS SIZED: a floor and a ceiling, each of which is a
 *  length, the content, or a share of what is left over.
 *
 *  - `px(v)` — v, floor and ceiling both. It neither grows nor shrinks.
 *  - `content()` — the track is as wide as the widest thing in it and no
 *    wider. Free space beside it stays free.
 *  - `fr(w)` — the track takes `w` shares of what is left after the fixed
 *    and content tracks have been served, and nothing else: a share has NO
 *    floor of its own, so a row of equal panels is equal however wide the
 *    things in it are. Put a floor under it explicitly when the content
 *    must not be crushed.
 *  - `minmax(low, high)` — the floor of `low` under the ceiling of
 *    `high`. `minmax(px(180), fr())` is a share that never falls under
 *    180; `minmax(content(), fr())` is one that never falls under what it
 *    holds.
 *
 *  A ceiling under the floor is no ceiling: the track resolves at the
 *  floor. */
struct Track {
  enum class Kind : uint8_t { Fixed, Content, Fraction };

  /** The default is `fr(1)`, one equal share — what an unstated column of
   *  a page is. */
  Kind minKind = Kind::Fixed;
  float minValue = 0.0f;
  Kind maxKind = Kind::Fraction;
  float maxValue = 1.0f;

  bool operator==(const Track&) const = default;

  static constexpr Track px(float v) {
    return {Kind::Fixed, v, Kind::Fixed, v};
  }
  static constexpr Track content() {
    return {Kind::Content, 0.0f, Kind::Content, 0.0f};
  }
  static constexpr Track fr(float weight = 1.0f) {
    return {Kind::Fixed, 0.0f, Kind::Fraction, weight};
  }
  /** The floor of @p low under the ceiling of @p high. A `Fraction` floor
   *  is not a floor — nothing can be a share of the leftover before the
   *  leftover is known — and reads as zero. */
  static constexpr Track minmax(Track low, Track high) {
    Track t;
    t.minKind = low.minKind == Kind::Fraction ? Kind::Fixed : low.minKind;
    t.minValue = low.minKind == Kind::Fraction ? 0.0f : low.minValue;
    t.maxKind = high.maxKind;
    t.maxValue = high.maxValue;
    return t;
  }
};

/** The four sizing functions as free names, so a track list reads the way
 *  it is spoken: `{px(596), fr(1), minmax(px(180), fr(1))}`. They are the
 *  members above under a shorter name and nothing else. */
[[nodiscard]] constexpr Track px(float v) { return Track::px(v); }
[[nodiscard]] constexpr Track content() { return Track::content(); }
[[nodiscard]] constexpr Track fr(float weight = 1.0f) {
  return Track::fr(weight);
}
[[nodiscard]] constexpr Track minmax(Track low, Track high) {
  return Track::minmax(low, high);
}

/** @p count copies of @p track — the spelling for "four equal columns". */
[[nodiscard]] inline std::vector<Track> repeat(int count, Track track) {
  return std::vector<Track>(count > 0 ? (size_t)count : 0u, track);
}

namespace detail {

/** One name from a `Grid::areas` picture, and the block of cells it
 *  covers. */
struct NamedArea {
  std::string name;
  int column = 0, row = 0, columns = 1, rows = 1;
};

/** The names a picture of a grid declares, and how wide and deep the
 *  picture is.
 *
 *  Each string is one ROW of the grid; the tokens in it, split on spaces,
 *  are one cell each, and `.` is a cell no name claims. A name must cover
 *  a RECTANGLE — that is what makes it addressable as one box. A name
 *  scattered over cells that do not form one is reported once on stderr
 *  and placed at the rectangle that bounds it, because a picture the
 *  author can see is wrong is worth saying so about, and refusing to lay
 *  the page out at all is not. */
struct AreaPicture {
  std::vector<NamedArea> areas;
  int columns = 0;
  int rows = 0;

  const NamedArea* find(std::string_view name) const {
    for (const NamedArea& a : areas)
      if (a.name == name) return &a;
    return nullptr;
  }
};

inline AreaPicture readAreas(const std::vector<std::string>& rows) {
  AreaPicture picture;
  picture.rows = (int)rows.size();
  struct Extent {
    int left = 0, top = 0, right = 0, bottom = 0, cells = 0;
  };
  std::vector<std::pair<std::string, Extent>> seen;
  for (size_t r = 0; r < rows.size(); ++r) {
    int column = 0;
    const std::string& line = rows[r];
    for (size_t at = 0; at < line.size();) {
      while (at < line.size() && (line[at] == ' ' || line[at] == '\t')) ++at;
      const size_t start = at;
      while (at < line.size() && line[at] != ' ' && line[at] != '\t') ++at;
      if (start == at) break;
      const std::string token = line.substr(start, at - start);
      if (token != ".") {
        auto found = std::find_if(seen.begin(), seen.end(), [&](const auto& e) {
          return e.first == token;
        });
        if (found == seen.end()) {
          seen.push_back({token, Extent{column, (int)r, column, (int)r, 1}});
        } else {
          Extent& e = found->second;
          e.left = std::min(e.left, column);
          e.right = std::max(e.right, column);
          e.top = std::min(e.top, (int)r);
          e.bottom = std::max(e.bottom, (int)r);
          ++e.cells;
        }
      }
      ++column;
    }
    picture.columns = std::max(picture.columns, column);
  }
  for (const auto& [name, e] : seen) {
    const int wide = e.right - e.left + 1;
    const int tall = e.bottom - e.top + 1;
    if (wide * tall != e.cells) {
      static thread_local bool warned = false;
      if (!warned) {
        warned = true;
        std::fprintf(stderr,
                     "compose: grid area \"%s\" does not cover a rectangle; "
                     "placed at the rectangle that bounds it\n",
                     name.c_str());
      }
    }
    picture.areas.push_back({name, e.left, e.top, wide, tall});
  }
  return picture;
}

}  // namespace detail

/** THE GRID.
 *
 *      layout(layouts::Grid{
 *          .columns = {layouts::px(596), layouts::fr(1), layouts::px(700)},
 *          .rows    = {layouts::px(40), layouts::content(), layouts::fr(1)},
 *          .areas   = {"status status status",
 *                      "audio  nav    mast",
 *                      "main   main   feature"},
 *          .gap     = {8, 8}})
 *          .child(navBar().area("nav"))
 *          .child(masthead().area("mast"))
 *
 *  **The track sizing rule, which is the whole of the value.** Per axis,
 *  in this order:
 *
 *  1. **Initialize.** Every track starts at its floor — a `Fixed` floor is
 *     the length, anything else is zero — and gets a ceiling: a `Fixed`
 *     ceiling is the length, a `Content` ceiling is not known yet, and a
 *     `Fraction` ceiling is the floor, because a share is handed out in
 *     step 4 and nowhere else.
 *  2. **Resolve the content.** Each child raises the floors of the
 *     `Content`-floored tracks it spans to hold its minimum, and the
 *     ceilings of the `Content`-ceilinged ones to hold its measured size.
 *     Children are taken NARROWEST SPAN FIRST, so a child across four
 *     tracks sees what the single-track children already asked for
 *     instead of paying for them twice, and a span's deficit is shared in
 *     proportion to what each of its tracks already holds.
 *  3. **Maximize.** Free space grows the floors toward the ceilings, an
 *     equal share at a time, each track stopping at its own ceiling.
 *  4. **Expand the shares.** What is still free is divided among the
 *     `Fraction` tracks by weight. TWO RULES make a share behave: when
 *     the weights sum to LESS THAN ONE each track takes only
 *     `weight × free` — so `fr(0.5)` is half a share and not all of it —
 *     and a track whose share would fall UNDER ITS OWN FLOOR freezes
 *     there, leaves the division, and the division re-runs without it,
 *     which is what keeps a squeezed container from resolving negative
 *     widths.
 *
 *  Whatever is left after step 4 stays free: a row of `content()` tracks
 *  packs at the start of its container and does not stretch to fill it.
 *
 *  **The two axes run the same rule.** A row span's deficit is shared
 *  across its rows in proportion exactly as a column span's is — which is
 *  where a grid parts company with the auto table beside it, whose row
 *  spans drop their whole deficit on their last row because that is what
 *  a page of HTML tables has always done. What differs here is only the
 *  DEFAULT track: an unstated column is a share of the width, an unstated
 *  row is as tall as what is in it, so a grid fills its container across
 *  and grows down the page.
 *
 *  **Where a child goes.** `Element::area("nav")` names a region of the
 *  picture; `Element::cells(c, r, w, h)` names the block by number and is
 *  what `area` resolves to. A child that says neither flows into the next
 *  free cell — left to right and then down, never backtracking past the
 *  cursor, unless `dense` is set, in which case each child fills the
 *  first hole that will take it. A name the picture does not carry is
 *  silent, and the child flows.
 *
 *  **When a list is empty.** No `columns` makes every column an equal
 *  share; no `rows` makes every row as tall as its content. Tracks past
 *  the end of a given list are content-sized, which is CSS's implicit
 *  row.
 *
 *  **THE ONE THING A CONTENT TRACK NEEDS FROM ITS CONTAINER.** A child is
 *  measured before it is placed, and a container that stretches its
 *  children measures every one of them at ITS OWN width — so a content
 *  track would be sized by the container the content is about to be
 *  fitted into rather than by the content, and the two would chase each
 *  other. Range the container's children at their own size
 *  (`.alignItems(Align::Start)`) whenever a track is sized by what is in
 *  it; the grid does its own stretching, through `across` and `down`. */
struct Grid {
  std::vector<Track> columns;
  std::vector<Track> rows;
  /** The picture: one string per row, one token per cell, `.` a cell no
   *  name claims. Empty leaves the grid addressed by number alone. */
  std::vector<std::string> areas;
  /** Between tracks: x across, y down. */
  SkSize gap = {0.0f, 0.0f};
  /** Fill the earliest hole a flowing child fits rather than never
   *  backtracking past the cursor. */
  bool dense = false;
  /** How a child sits in the box its cells make, when the child itself
   *  did not say with `Element::cellAlign`. `Stretch` sizes it to the box. */
  Align across = Align::Stretch;
  Align down = Align::Stretch;

  /** This scheme reads `LayoutInput::childMinSizes`: a `Content` floor is
   *  a child's minimum, not its measured size. */
  static constexpr bool readsChildMinSizes = true;

  bool operator==(const Grid&) const = default;

  /** THE RESOLVED TRACKS: what the sizing rule arrived at, and the origin
   *  of each. Exposed for the same reason the auto table exposes its own —
   *  a study that reproduces a printed page has to be able to print the
   *  grid it resolved and diff it against the original's measurements, and
   *  reading the numbers back off the placed rects cannot do it, because a
   *  track nothing fills leaves no trace at all. */
  struct Resolved {
    std::vector<float> columnWidths, rowHeights;
    std::vector<float> columnX, rowY;
  };

  Resolved solve(const LayoutInput& in) const {
    const std::vector<CellSpan> spans = flowed(in);
    const detail::AreaPicture picture = detail::readAreas(areas);
    const int cols = columnCount(spans, picture);
    const int lines = rowCount(spans, picture);

    Resolved out;
    // A column list that was never given is equal shares of the width; a
    // row list that was never given is a page that grows down, so its rows
    // are as tall as what is in them.
    out.columnWidths =
        resolve(columns, Track::fr(1.0f), cols, in.container.width(),
                gap.width(), spans, in, /*horizontal=*/true);
    out.rowHeights =
        resolve(rows, Track::content(), lines, in.container.height(),
                gap.height(), spans, in, /*horizontal=*/false);
    out.columnX = origins(out.columnWidths, gap.width());
    out.rowY = origins(out.rowHeights, gap.height());
    return out;
  }

  /** Where every child lands, in child order. */
  std::vector<SkRect> place(const LayoutInput& in) const {
    const std::vector<CellSpan> spans = flowed(in);
    const Resolved grid = solve(in);
    std::vector<SkRect> rects(in.childSizes.size());
    for (size_t i = 0; i < spans.size() && i < rects.size(); ++i) {
      const CellSpan& s = spans[i];
      if (s.column < 0 || s.row < 0 ||
          (size_t)s.column >= grid.columnX.size() ||
          (size_t)s.row >= grid.rowY.size())
        continue;  // outside the grid it was given; placed nowhere
      const SkSize box{
          extent(grid.columnWidths, s.column, s.columns, gap.width()),
          extent(grid.rowHeights, s.row, s.rows, gap.height())};
      const Align a = s.alignDeclared ? s.across : across;
      const Align d = s.alignDeclared ? s.down : down;
      const SkSize size{
          a == Align::Stretch ? box.width() : in.childSizes[i].width(),
          d == Align::Stretch ? box.height() : in.childSizes[i].height()};
      rects[i] = SkRect::MakeXYWH(
          grid.columnX[(size_t)s.column] + slack(a, box.width(), size.width()),
          grid.rowY[(size_t)s.row] + slack(d, box.height(), size.height()),
          size.width(), size.height());
    }
    return rects;
  }

 private:
  /** Every child's cells: the name it claimed resolved against the
   *  picture, the numbers it claimed taken as they stand, and the ones
   *  that claimed nothing flowed into what is left. */
  std::vector<CellSpan> flowed(const LayoutInput& in) const {
    const detail::AreaPicture picture = detail::readAreas(areas);
    std::vector<CellSpan> out(in.childSizes.size());
    for (size_t i = 0; i < out.size(); ++i) {
      if (i < in.childCells.size()) out[i] = in.childCells[i];
      if (i >= in.childAreas.size() || in.childAreas[i].empty()) continue;
      if (const detail::NamedArea* found = picture.find(in.childAreas[i])) {
        out[i].column = found->column;
        out[i].row = found->row;
        out[i].columns = found->columns;
        out[i].rows = found->rows;
        out[i].declared = true;
      } else {
        out[i].declared = false;  // an unknown name is silent; the child flows
      }
    }
    const int cols = std::max(columnCount(out, picture), 1);
    std::vector<char> taken;
    const auto held = [&](int column, int row) {
      if (column < 0 || column >= cols || row < 0) return true;
      const size_t at = (size_t)row * (size_t)cols + (size_t)column;
      return at < taken.size() && taken[at] != 0;
    };
    const auto claim = [&](int column, int row, int wide, int tall) {
      for (int r = row; r < row + tall; ++r)
        for (int c = column; c < column + wide; ++c) {
          if (c < 0 || c >= cols || r < 0) continue;
          const size_t at = (size_t)r * (size_t)cols + (size_t)c;
          if (taken.size() <= at) taken.resize(at + 1, 0);
          taken[at] = 1;
        }
    };
    for (const CellSpan& s : out)
      if (s.declared) claim(s.column, s.row, s.columns, s.rows);
    size_t cursor = 0;
    for (CellSpan& s : out) {
      if (s.declared) continue;
      const int wide = std::max(s.columns, 1);
      const int tall = std::max(s.rows, 1);
      // Sparse flow never looks back past the cursor, so a child cannot
      // land on a cell an explicit span already claimed and the run stays
      // in declaration order. Dense flow starts every search at cell zero,
      // which fills the holes a wide span left beside it — and is the one
      // difference between the two.
      size_t at = dense ? 0 : cursor;
      for (;; ++at) {
        const int c = (int)(at % (size_t)cols);
        const int r = (int)(at / (size_t)cols);
        if (c + wide > cols) continue;  // a span may not straddle the edge
        bool free = true;
        for (int dr = 0; dr < tall && free; ++dr)
          for (int dc = 0; dc < wide && free; ++dc)
            free = !held(c + dc, r + dr);
        if (!free) continue;
        s.column = c;
        s.row = r;
        claim(c, r, wide, tall);
        if (!dense) cursor = at + 1;
        break;
      }
    }
    return out;
  }

  int columnCount(const std::vector<CellSpan>& spans,
                  const detail::AreaPicture& picture) const {
    if (!columns.empty()) return (int)columns.size();
    if (picture.columns > 0) return picture.columns;
    int most = 0;
    for (const CellSpan& s : spans)
      if (s.declared) most = std::max(most, s.column + s.columns);
    // With no tracks, no picture and nothing claimed, the grid has only its
    // children to go on and is one row of them.
    return most > 0 ? most : std::max((int)spans.size(), 1);
  }
  int rowCount(const std::vector<CellSpan>& spans,
               const detail::AreaPicture& picture) const {
    if (!rows.empty()) return (int)rows.size();
    int most = std::max(picture.rows, 1);
    for (const CellSpan& s : spans) most = std::max(most, s.row + s.rows);
    return most;
  }

  /** The track at @p index of a list that may be shorter than the grid:
   *  a list that was never given is all equal shares, and a track past the
   *  end of one that was is sized by its content. */
  static Track trackAt(const std::vector<Track>& list, Track fallback,
                       int index) {
    if (list.empty()) return fallback;
    if ((size_t)index < list.size()) return list[(size_t)index];
    return Track::content();
  }

  /** THE SIZING RULE, over one axis. The four steps in the class comment,
   *  in that order. */
  static std::vector<float> resolve(const std::vector<Track>& list,
                                    Track fallback, int count, float container,
                                    float gap,
                                    const std::vector<CellSpan>& spans,
                                    const LayoutInput& in, bool horizontal) {
    const size_t n = count > 0 ? (size_t)count : 0u;
    std::vector<float> base(n, 0.0f), limit(n, 0.0f);
    std::vector<Track> tracks(n);
    for (size_t i = 0; i < n; ++i) {
      tracks[i] = trackAt(list, fallback, (int)i);
      base[i] =
          tracks[i].minKind == Track::Kind::Fixed ? tracks[i].minValue : 0.0f;
      // A Fraction ceiling is the floor here: a share is dealt out in the
      // expansion step and must not be pre-spent by the maximize step.
      limit[i] = tracks[i].maxKind == Track::Kind::Fixed ? tracks[i].maxValue
                                                         : base[i];
    }

    // 2. The content, narrowest span first.
    const auto axisOf = [&](const SkSize& s) {
      return horizontal ? s.width() : s.height();
    };
    const auto firstOf = [&](const CellSpan& s) {
      return horizontal ? s.column : s.row;
    };
    const auto spanOf = [&](const CellSpan& s) {
      return std::max(horizontal ? s.columns : s.rows, 1);
    };
    const auto share = [&](std::vector<float>& values, int first, int k,
                           float need, bool wantsContent(const Track&)) {
      float have = (float)(k - 1) * gap;
      float pool = 0.0f;
      int accepted = 0;
      for (int j = 0; j < k; ++j) {
        const size_t at = (size_t)(first + j);
        if (first + j < 0 || at >= values.size()) return;
        have += values[at];
        if (wantsContent(tracks[at])) {
          pool += values[at];
          ++accepted;
        }
      }
      const float deficit = need - have;
      if (deficit <= 0 || accepted == 0) return;
      for (int j = 0; j < k; ++j) {
        const size_t at = (size_t)(first + j);
        if (!wantsContent(tracks[at])) continue;
        values[at] +=
            pool > 0 ? deficit * values[at] / pool : deficit / (float)accepted;
      }
    };
    const auto flooredByContent = [](const Track& t) {
      return t.minKind == Track::Kind::Content;
    };
    const auto ceilingedByContent = [](const Track& t) {
      return t.maxKind == Track::Kind::Content;
    };
    for (int k = 1; k <= count; ++k)
      for (size_t i = 0; i < spans.size(); ++i) {
        if (spanOf(spans[i]) != k) continue;
        if (i >= in.childSizes.size()) continue;
        const float measured = axisOf(in.childSizes[i]);
        const float minimum = i < in.childMinSizes.size()
                                  ? axisOf(in.childMinSizes[i])
                                  : measured;
        share(base, firstOf(spans[i]), k, minimum, flooredByContent);
        share(limit, firstOf(spans[i]), k, measured, ceilingedByContent);
      }
    for (size_t i = 0; i < n; ++i) limit[i] = std::max(limit[i], base[i]);

    const float available = container - (float)(n > 0 ? n - 1 : 0) * gap;

    // 3. Maximize: grow the floors toward the ceilings, equally.
    float free = available;
    for (float w : base) free -= w;
    while (free > 0.01f) {
      int growable = 0;
      for (size_t i = 0; i < n; ++i)
        if (limit[i] - base[i] > 0.01f) ++growable;
      if (growable == 0) break;
      const float each = free / (float)growable;
      bool progressed = false;
      for (size_t i = 0; i < n && free > 0.01f; ++i) {
        const float room = limit[i] - base[i];
        if (room <= 0.01f) continue;
        const float grew = std::min(each, room);
        base[i] += grew;
        free -= grew;
        progressed = true;
      }
      if (!progressed) break;
    }

    // 4. Expand the shares.
    std::vector<char> flexible(n, 0);
    bool anyFlexible = false;
    for (size_t i = 0; i < n; ++i)
      if (tracks[i].maxKind == Track::Kind::Fraction &&
          tracks[i].maxValue > 0) {
        flexible[i] = 1;
        anyFlexible = true;
      }
    while (anyFlexible) {
      float leftover = available;
      float weights = 0.0f;
      for (size_t i = 0; i < n; ++i) {
        if (flexible[i])
          weights += tracks[i].maxValue;
        else
          leftover -= base[i];
      }
      if (leftover <= 0 || weights <= 0) break;
      // Weights that sum to less than one hand out only their own share of
      // the free space, so fr(0.5) is half a share and the rest stays free.
      const float unit = leftover / std::max(weights, 1.0f);
      bool froze = false;
      for (size_t i = 0; i < n; ++i)
        if (flexible[i] && base[i] > unit * tracks[i].maxValue) {
          flexible[i] = 0;  // freezes at its own floor and leaves the division
          froze = true;
        }
      if (froze) {
        anyFlexible = false;
        for (size_t i = 0; i < n; ++i) anyFlexible = anyFlexible || flexible[i];
        continue;
      }
      for (size_t i = 0; i < n; ++i)
        if (flexible[i]) base[i] = unit * tracks[i].maxValue;
      break;
    }
    return base;
  }

  static std::vector<float> origins(const std::vector<float>& tracks,
                                    float gap) {
    std::vector<float> at(tracks.size(), 0.0f);
    for (size_t i = 1; i < at.size(); ++i)
      at[i] = at[i - 1] + tracks[i - 1] + gap;
    return at;
  }

  /** How far @p count tracks from @p first reach, the gaps between them
   *  included. */
  static float extent(const std::vector<float>& tracks, int first, int count,
                      float gap) {
    const int wide = std::max(count, 1);
    float total = (float)(wide - 1) * gap;
    for (int j = 0; j < wide; ++j) {
      const size_t at = (size_t)(first + j);
      if (first + j >= 0 && at < tracks.size()) total += tracks[at];
    }
    return total;
  }

  /** Where a `size` sits in a `box` under one alignment. A cell has no run
   *  of siblings to share a baseline with, so `Auto` and `Baseline` read
   *  as `Start`. */
  static float slack(Align how, float box, float size) {
    switch (how) {
      case Align::Center:
        return (box - size) * 0.5f;
      case Align::End:
        return box - size;
      default:
        return 0.0f;
    }
  }
};

}  // namespace sigil::compose::layouts
