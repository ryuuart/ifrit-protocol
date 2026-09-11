#include <include/core/SkTypes.h>
#include <sigilcompose/core/Grid.h>

#include <algorithm>
#include <string_view>

namespace sigil::compose::layouts {

namespace {

void warnAreaNotRectangular(const std::string& name) {
  // Limit repeated warnings to one per thread.
  static thread_local bool warned = false;
  if (warned) return;
  warned = true;
  SkDebugf(
      "compose: grid area \"%s\" does not cover a rectangle, so it is placed "
      "at the rectangle that bounds it. A name must claim one block of cells "
      "to be addressable as one box.\n",
      name.c_str());
}

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

AreaPicture readAreas(const std::vector<std::string>& rows) {
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
    if (wide * tall != e.cells) warnAreaNotRectangular(name);
    picture.areas.push_back({name, e.left, e.top, wide, tall});
  }
  return picture;
}

struct GridLayout {
  explicit GridLayout(const Grid& grid)
      : columns(grid.columns),
        rows(grid.rows),
        areas(grid.areas),
        gap(grid.gap),
        dense(grid.dense),
        across(grid.across),
        down(grid.down) {}

  const std::vector<Track>& columns;
  const std::vector<Track>& rows;
  const std::vector<std::string>& areas;
  SkSize gap;
  bool dense;
  Align across, down;
  using Resolved = Grid::Resolved;

  Resolved solve(const LayoutInput& in) const {
    const AreaPicture picture = readAreas(areas);
    return solve(in, flowed(in, picture), picture);
  }

  /** Where every child lands, in child order. */
  std::vector<SkRect> place(const LayoutInput& in) const {
    // The picture is read ONCE per call and handed down: parsing it again
    // inside each step would read the same strings three times over for
    // every layout pass.
    const AreaPicture picture = readAreas(areas);
    const std::vector<CellSpan> spans = flowed(in, picture);
    const Resolved grid = solve(in, spans, picture);
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
  Resolved solve(const LayoutInput& in, const std::vector<CellSpan>& spans,
                 const AreaPicture& picture) const {
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

  /** Every child's cells: the name it claimed resolved against the
   *  picture, the numbers it claimed taken as they stand, and the ones
   *  that claimed nothing flowed into what is left. */
  std::vector<CellSpan> flowed(const LayoutInput& in,
                               const AreaPicture& picture) const {
    std::vector<CellSpan> out(in.childSizes.size());
    for (size_t i = 0; i < out.size(); ++i) {
      if (i < in.childCells.size()) out[i] = in.childCells[i];
      if (i >= in.childAreas.size() || in.childAreas[i].empty()) continue;
      if (const NamedArea* found = picture.find(in.childAreas[i])) {
        out[i].column = found->column;
        out[i].row = found->row;
        out[i].columns = found->columns;
        out[i].rows = found->rows;
        out[i].declared = true;
      } else {
        out[i].declared = false;  // an unknown name is silent; the child flows
      }
    }
    // The kernel's flow: the declared spans are claimed, and what said
    // nothing lands in what is left, in this grid's own order.
    flowCells(out, columnCount(out, picture), dense);
    return out;
  }

  int columnCount(const std::vector<CellSpan>& spans,
                  const AreaPicture& picture) const {
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
               const AreaPicture& picture) const {
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

}  // namespace

Grid::Resolved Grid::solve(const LayoutInput& in) const {
  return GridLayout(*this).solve(in);
}

std::vector<SkRect> Grid::place(const LayoutInput& in) const {
  return GridLayout(*this).place(in);
}

}  // namespace sigil::compose::layouts
