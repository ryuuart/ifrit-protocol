#pragma once

/** @file
 * THE PANEL AS A CARPENTER CUTS IT: the room's dimensions, the timber the
 * pieces are cut from, one piece of stock with a cut face at each end, the
 * generator that lays out the frame, the register, the jigumi and the
 * seven ha of every cell, and the one element every piece is drawn as.
 * Nothing here knows how the page is set.
 */

#include <choreograph/Easing.h>
#include <sigilmotion/ease/Ease.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilgeometry/path/Operations.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigilgeometry/path/Segments.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Bank.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <vector>

namespace kumiko {

namespace material = sigil::material;
namespace path = sigil::geometry::path;
namespace motion = sigil::motion;
using namespace sigil::compose;
using sigil::material::hexColor;
using glm::vec2;

// ---------------------------------------------------------------------------
// Wood tones, matched by eye.
//
// Hinoki is #E9D3A0 in daylight. This panel is BACKLIT: the wood faces
// away from the lamp, so the body sits a couple of stops under it and only
// the arris reaches the daylight value — otherwise cream wood and cream
// light have no separation and the fretwork stops silhouetting, which is
// the whole point of a ranma.

/** A board's three tones and how strongly its grain and figure read. */
struct Timber {
  material::Color base, light, dark;
  float grain;
  float figure;
};

/** Planed cypress, room side, with its daylight arris and notch shadow. */
const Timber kHinoki{hexColor(0xD6BC89), hexColor(0xF5E6C4),
                     hexColor(0x8E6C3B), 0.19f, 0.26f};
/** The zelkova frame. */
const Timber kKeyaki{hexColor(0x76472A), hexColor(0x9C6B3E),
                     hexColor(0x4B2A12), 0.055f, 0.38f};
/** The room-side members face AWAY from the far room's lamp, so the same
 *  keyaki reads two stops down on the nageshi, the kamoi and the posts. */
const Timber kKeyakiShade{hexColor(0x33200F), hexColor(0x54341B),
                          hexColor(0x140C05), 0.045f, 0.42f};

/** THE JOINT INKS, stated beside the timber they mark because they are
 *  its own shadows and lights rather than the page's: the seam every
 *  abutting piece shows against its neighbour, the shadow a half-lap's
 *  upper edge casts across the member under it, and the lit arris just
 *  outside that shadow. Each is translucent so the timber under it still
 *  shows its grain through the line. */
const material::Color kSeamInk = hexColor(0x4A3620, 0.55f);
const material::Color kLapShadowInk = hexColor(0x472E12, 0.55f);
const material::Color kLapArrisInk = hexColor(0xFFF2D1, 0.30f);

// ---------------------------------------------------------------------------
// The room. The field is FIXED and the pitch is the free constant: change
// kCell alone and the columns and rows re-derive, so the lattice only gets
// denser.

constexpr float kWidth = 1400, kRoom = 1000;
/** The shop drawing's band under the room. */
constexpr float kBandHeight = 364;
constexpr float kHeight = kRoom + kBandHeight;
constexpr vec2 kCentre{700, 500};

/** THE PITCH. 60 gives a 15×9 field, still legal. */
constexpr float kCell = 90;
constexpr float kFieldWidth = 900, kFieldHeight = 540;
// NOLINTBEGIN(bugprone-throwing-static-initialization): arithmetic on constants
// cannot throw
const int kColumns = std::max(2, (int)std::lround(kFieldWidth / kCell));
const int kRows = std::max(2, (int)std::lround(kFieldHeight / kCell));
const float kCellWidth = kFieldWidth / (float)kColumns;
const float kCellHeight = kFieldHeight / (float)kRows;

const SkRect kField = SkRect::MakeXYWH(kCentre.x - kFieldWidth / 2,
                                       kCentre.y - kFieldHeight / 2,
                                       kFieldWidth, kFieldHeight);
/** The register band is exactly HALF the pitch, so its plain cells come
 *  out square (masu, "measuring box") and its members interleave with the
 *  field's own jigumi at a clean 2:1. */
const float kRegisterBand = kCell * 0.5f;
/** The kumiko-buchi, the frame's face. */
constexpr float kBorder = 45;
/** The frame's opening. */
const SkRect kOpening = kField.makeOutset(kRegisterBand, kRegisterBand);
const SkRect kFrameOuter = kOpening.makeOutset(kBorder, kBorder);

// Stock face widths, as fractions of the pitch so they re-derive with it.
// Real stock is 12.7 mm deep by 3.2 mm face, which on a 22 mm pitch is
// about 0.145; the jigumi is drawn a little under that and the ha narrower
// again, because the infill is thinner stock than the framework it seats
// into.
const float kJigumiWidth = std::max(6.0f, 0.125f * kCell);
const float kLeafWidth = std::max(5.0f, 0.096f * kCell);
const float kRegisterWidth = kJigumiWidth * 0.80f;
// NOLINTEND(bugprone-throwing-static-initialization)

/** Where a leg of a right isosceles triangle meets its incircle, as a
 *  fraction of the leg: the incircle's radius is s(2 − √2)/2. */
constexpr float kIncircle = 0.2928932f;
constexpr float kPastIncircle = 1.0f - kIncircle;

// ---------------------------------------------------------------------------
// The assembly, as beats of one loop. Six laws rather than one ladder: the
// frame's four boards, the register's grid, the two jig passes and the
// leaves each open on their own base at their own step, and a leaf's base
// is a function of which CELL it belongs to.

constexpr float kPeriod = 6.4f;
constexpr float kFrameAt = 0.00f, kFrameFor = 0.55f;
constexpr float kRegisterAt = 0.45f, kRegisterFor = 0.55f;
constexpr float kVerticalJigAt = 0.95f, kHorizontalJigAt = 1.28f,
                kJigFor = 0.42f;
constexpr float kLeavesAt = 1.62f, kLeafSweep = 0.62f, kLeafFor = 0.30f;
constexpr float kDiagonalAfter = 0.00f, kFillerAfter = 0.09f,
                kLockAfter = 0.17f;
/** The seating tap: every lap and tenon arrives on one beat. */
constexpr float kSeatAt = 2.62f, kSeatFor = 0.24f;
/** The far room's lamp comes up behind the finished panel. */
constexpr float kLampAt = 2.78f, kLampFor = 0.60f;
/** The shop drawing takes its cell apart once the panel is lit. */
constexpr float kDrawingAt = 3.05f, kDrawingFor = 0.70f;

// ---------------------------------------------------------------------------
// The timber, ONE recipe seeded per piece.
//
// EVERY PIECE IS A BOARD, and `material::kit::timber` is what a board is: a
// flat face between a narrow lit arris and a narrow shadowed one, with
// grain running down the piece and a fine tooth over the whole face.
// `span` is the piece's face width, so the cross-section shading lands
// correctly on a 45 px frame member and a 10 px leaf alike; `flip` picks
// WHICH long edge is lit, from the world light, so a rotated lattice still
// reads under one raking source; `along` turns the piece to run down local
// y, so one recipe boards rails and posts. The bank folds the seed to 24
// buckets, so hundreds of boards cost a bounded number of materials whose
// identity holds across describes.
class TimberBank {
 public:
  material::Paint get(const Timber& timber, float span, bool flip,
                            uint32_t seed, bool along = false) {
    return material::Paint::recipe(m_bank.get(
        material::kit::timberRecipe(),
        material::kit::TimberParameters{.base = timber.base,
                                        .light = timber.light,
                                        .dark = timber.dark,
                                        .span = span,
                                        .flip = flip ? 1.0f : 0.0f,
                                        .along = along ? 1.0f : 0.0f,
                                        .grain = timber.grain,
                                        .figure = timber.figure,
                                        // The tooth is the surface and the
                                        // figure the wood's story; toothScale
                                        // times stretch stays under a tenth
                                        // or the tooth aliases into hash.
                                        .tooth = 0.26f,
                                        .toothScale = 0.045f,
                                        .stretch = 2.0f},
        seed));
  }

 private:
  material::Bank m_bank{24};
};

// ---------------------------------------------------------------------------
// A piece: a centreline, a face width, and a CUT-FACE DIRECTION at each
// end. The mitre falls straight out of that last field: the two corners of
// an end are where the cut line through the centreline's end meets the two
// edge lines, which in the piece's own frame is a pure shear of
// (w/2)·(cut.x / cut.y).

enum class Role : uint8_t {
  Frame,
  Register,
  VerticalJigumi,
  HorizontalJigumi,
  Diagonal,
  Filler,
  Lock,
};

struct Piece {
  vec2 from{0, 0}, to{0, 0};
  /** Cut-face directions at each end, in canvas space. */
  vec2 cutFrom{0, 0}, cutTo{0, 0};
  float width = 10;
  Role role = Role::VerticalJigumi;
  const Timber* timber = &kHinoki;
  uint32_t seed = 0;
  /** When the piece's entrance starts in the loop, and how long it runs. */
  float enters = 0, entersFor = 0.3f;
  /** Frame members catch the light of the opening instead of the room's. */
  bool litFromOpening = false;
};

inline vec2 perpendicular(vec2 v) { return {-v.y, v.x}; }
inline vec2 direction(vec2 v) {
  const float length = glm::length(v);
  return length > 1e-6f ? v / length : vec2{1, 0};
}
inline uint32_t scatter(uint32_t counter) {
  return counter * 2654435761u >> 13u;
}

/** A piece from @p from to @p to, each end cut square unless it names a
 *  cut face. */
inline Piece cut(vec2 from, vec2 to, float width, Role role,
                 const Timber& timber, uint32_t seed, vec2 cutFrom = {0, 0},
                 vec2 cutTo = {0, 0}) {
  const vec2 along = direction(to - from);
  return Piece{
      .from = from,
      .to = to,
      .cutFrom = glm::length(cutFrom) < 1e-4f ? perpendicular(along)
                                              : direction(cutFrom),
      .cutTo = glm::length(cutTo) < 1e-4f ? perpendicular(along)
                                          : direction(cutTo),
      .width = width,
      .role = role,
      .timber = &timber,
      .seed = scatter(seed),
  };
}

/** THE SEVEN HA OF ONE CELL of @p width by @p height at @p origin, cut from
 *  @p stock of that face width and mirrored
 *  across the cell when @p mirrored so neighbouring cells alternate their
 *  diagonal. Each runs from a vertex of one of the two right triangles the
 *  diagonal makes to that triangle's INCENTER, stopping @p seat short of
 *  the jigumi face it starts against and running @p overlap past the
 *  incenter into the Y-joint. In cutting order: the diagonal, the two
 *  fillers off the right angles, the four locking pieces off the 45°
 *  corners — a locking piece's face is the jigumi it grazes. */
inline std::vector<Piece> cellLeaves(vec2 origin, float width, float height,
                                     float stock, bool mirrored, float seat,
                                     float overlap, uint32_t& seed) {
  const auto at = [&](float x, float y) {
    return origin + vec2{mirrored ? width - x : x, y};
  };
  const vec2 corner = at(0, 0), right = at(width, 0),
             opposite = at(width, height), left = at(0, height);
  const vec2 upperIncentre = at(width * kPastIncircle, height * kIncircle);
  const vec2 lowerIncentre = at(width * kIncircle, height * kPastIncircle);
  const float shallowSeat = seat / 0.3826834f;   // the 22.5° and 67.5° arms
  const float bisectSeat = seat * 1.4142136f;    // the 45° arms, the diagonal

  std::vector<Piece> leaves;
  const auto arm = [&](vec2 from, vec2 incentre, float stop, vec2 face,
                       Role role) {
    const vec2 along = direction(incentre - from);
    leaves.push_back(cut(from + along * stop, incentre + along * overlap,
                         stock, role, kHinoki, seed++, face));
  };
  const vec2 diagonal = direction(opposite - corner);
  leaves.push_back(cut(corner + diagonal * bisectSeat,
                       opposite - diagonal * bisectSeat, stock,
                       Role::Diagonal, kHinoki, seed++));
  arm(right, upperIncentre, bisectSeat, {0, 0}, Role::Filler);
  arm(left, lowerIncentre, bisectSeat, {0, 0}, Role::Filler);
  arm(corner, upperIncentre, shallowSeat, {1, 0}, Role::Lock);
  arm(corner, lowerIncentre, shallowSeat, {0, 1}, Role::Lock);
  arm(opposite, upperIncentre, shallowSeat, {0, 1}, Role::Lock);
  arm(opposite, lowerIncentre, shallowSeat, {1, 0}, Role::Lock);
  return leaves;
}

// ---------------------------------------------------------------------------
// The panel generator.

struct Panel {
  /** Every piece, in the order it is laid: frame, register, jigumi, ha. */
  std::vector<Piece> pieces;
  /** Tenon heads where a jigumi seats into the register's groove. */
  std::vector<Piece> tenons;
  /** The half-lap seams, as two sets of hairlines: the shadow the upper
   *  piece's edge casts across the lower, and the lit arris beside it. */
  SkPath lapShadows, lapHighlights;

  uint32_t seed = 1;

  void add(Piece piece, float enters, float entersFor,
           bool litFromOpening = false) {
    piece.enters = enters;
    piece.entersFor = entersFor;
    piece.litFromOpening = litFromOpening;
    pieces.push_back(piece);
  }

  void build() {
    frame();
    reg();
    jigumi();
    leaves();
    laps();
  }

  /** The mitred kumiko-buchi: four members, each cut 45° into the corner
   *  diagonals, laid clockwise from the top. */
  void frame() {
    const SkRect& outer = kFrameOuter;
    const float half = kBorder * 0.5f;
    const vec2 falling{0.7071f, 0.7071f}, rising{-0.7071f, 0.7071f};
    const vec2 topLeft{outer.left() + half, outer.top() + half},
        topRight{outer.right() - half, outer.top() + half},
        bottomRight{outer.right() - half, outer.bottom() - half},
        bottomLeft{outer.left() + half, outer.bottom() - half};
    const vec2 corners[] = {topLeft, topRight, bottomRight, bottomLeft,
                            topLeft};
    for (int side = 0; side < 4; ++side) {
      const bool even = side % 2 == 0;
      add(cut(corners[side], corners[side + 1], kBorder, Role::Frame, kKeyaki,
              seed++, even ? falling : rising, even ? rising : falling),
          kFrameAt + 0.10f * (float)side, kFrameFor, true);
    }
  }

  /** The plain masu register. Its inner boundary IS the field's outermost
   *  jigumi — in real work one member serves both — so this lays only the
   *  outer ring that seats into the frame's groove and the half-pitch ties
   *  that square the band's cells, one per field cell, landing exactly
   *  between the jigumi already running through the band. */
  void reg() {
    const SkRect& opening = kOpening;
    const float half = kRegisterWidth * 0.5f;
    int laid = 0;
    const auto lay = [&](vec2 from, vec2 to) {
      add(cut(from, to, kRegisterWidth, Role::Register, kHinoki, seed++),
          kRegisterAt + 0.008f * (float)laid++, kRegisterFor);
    };
    lay({opening.left(), opening.top() + half},
        {opening.right(), opening.top() + half});
    lay({opening.left(), opening.bottom() - half},
        {opening.right(), opening.bottom() - half});
    lay({opening.left() + half, opening.top()},
        {opening.left() + half, opening.bottom()});
    lay({opening.right() - half, opening.top()},
        {opening.right() - half, opening.bottom()});
    for (int column = 0; column < kColumns; ++column) {
      const float x = kField.left() + kCellWidth * ((float)column + 0.5f);
      lay({x, opening.top() + half}, {x, kField.top()});
      lay({x, kField.bottom()}, {x, opening.bottom() - half});
    }
    for (int row = 0; row < kRows; ++row) {
      const float y = kField.top() + kCellHeight * ((float)row + 0.5f);
      lay({opening.left() + half, y}, {kField.left(), y});
      lay({kField.right(), y}, {opening.right() - half, y});
    }
  }

  /** The structural jigumi, running the whole opening. A jigumi member is
   *  never a strip sliced mid-length: every one runs groove to groove and
   *  carries a tenon head where it seats. */
  void jigumi() {
    const SkRect& opening = kOpening;
    for (int column = 0; column <= kColumns; ++column) {
      const float x = kField.left() + kCellWidth * (float)column;
      add(cut({x, opening.top()}, {x, opening.bottom()}, kJigumiWidth,
              Role::VerticalJigumi, kHinoki, seed++),
          kVerticalJigAt + 0.012f * (float)column, kJigFor);
      tenon({x, opening.top() + 2.5f}, {0, 1});
      tenon({x, opening.bottom() - 2.5f}, {0, 1});
    }
    for (int row = 0; row <= kRows; ++row) {
      const float y = kField.top() + kCellHeight * (float)row;
      add(cut({opening.left(), y}, {opening.right(), y}, kJigumiWidth,
              Role::HorizontalJigumi, kHinoki, seed++),
          kHorizontalJigAt + 0.016f * (float)row, kJigFor);
      tenon({opening.left() + 2.5f, y}, {1, 0});
      tenon({opening.right() - 2.5f, y}, {1, 0});
    }
  }

  /** A tenon head, so a terminated member reads as SEATED into a milled
   *  groove rather than sliced off by a rectangle. */
  void tenon(vec2 at, vec2 along) {
    const vec2 across = perpendicular(direction(along)) * (kJigumiWidth * 0.8f);
    tenons.push_back(cut(at - across, at + across, kRegisterWidth * 0.5f,
                         Role::Register, kHinoki, seed++));
  }

  /** The ha, seven per cell, the diagonal alternating cell to cell. A
   *  leaf's base is where its cell stands on the sweep from the top-left
   *  corner to the bottom-right. */
  void leaves() {
    const float seat = kJigumiWidth * 0.5f + 1.0f;
    const float overlap = kLeafWidth * 0.55f;
    const float sweep = (float)std::max(1, kColumns + kRows - 2);
    constexpr float after[] = {kDiagonalAfter,     kFillerAfter,
                               kFillerAfter + 0.03f, kLockAfter,
                               kLockAfter + 0.02f, kLockAfter + 0.04f,
                               kLockAfter + 0.06f};
    for (int row = 0; row < kRows; ++row)
      for (int column = 0; column < kColumns; ++column) {
        const float base =
            kLeavesAt + kLeafSweep * (float)(row + column) / sweep;
        const std::vector<Piece> cell = cellLeaves(
            {kField.left() + kCellWidth * (float)column,
             kField.top() + kCellHeight * (float)row},
            kCellWidth, kCellHeight, kLeafWidth, ((row + column) & 1) != 0,
            seat, overlap, seed);
        for (size_t index = 0; index < cell.size(); ++index)
          add(cell[index], base + after[index], kLeafFor);
      }
  }

  /** Which layer of the lattice a role is cut into, the ha on top. Only
   *  the lattice laps: a leaf sits on the face of what it crosses, and two
   *  members of one layer butt instead of lapping. */
  static int layer(Role role) {
    switch (role) {
      case Role::HorizontalJigumi:
        return 1;
      case Role::VerticalJigumi:
        return 2;
      case Role::Register:
        return 3;
      default:
        return 0;
    }
  }

  /** The half-lap seams, read off the crossing graph rather than authored:
   *  where two members of different layers cross, the upper one's two
   *  edges show across the lower as a shadowed hairline with a lit one
   *  just outside it. The tolerance is the distance at which a crossing
   *  is a piece landing on another's face — a butt joint, which shows no
   *  lap. */
  void laps() {
    std::vector<path::operations::Strip> stock;
    stock.reserve(pieces.size());
    for (const Piece& piece : pieces)
      stock.push_back({piece.from, piece.to, piece.width});
    std::vector<path::SegmentContour> shadows, highlights;
    const auto line = [](vec2 from, vec2 to) {
      return path::SegmentContour{
          .segments = {{.kind = path::SegmentKind::Line, .points = {from, to}}}};
    };
    for (const path::operations::StripLap& lap : path::operations::stripLaps(
             stock, {.tolerance = 2.5f, .lapLimit = 3.0f})) {
      const Piece& first = pieces[(size_t)lap.pieces[0]];
      const Piece& second = pieces[(size_t)lap.pieces[1]];
      const int firstLayer = layer(first.role), secondLayer = layer(second.role);
      if (firstLayer == 0 || secondLayer == 0 || firstLayer == secondLayer)
        continue;
      const int upper = firstLayer > secondLayer ? 0 : 1;
      const vec2 along = lap.along[upper];
      const vec2 across = perpendicular(along);
      const vec2 reach = along * lap.halfSpan[upper];
      const float half = pieces[(size_t)lap.pieces[upper]].width * 0.5f;
      for (const float side : {-1.0f, 1.0f}) {
        const vec2 edge = lap.at + across * (side * half);
        shadows.push_back(line(edge - reach, edge + reach));
        const vec2 arris = edge + across * (side * 0.9f);
        highlights.push_back(line(arris - reach, arris + reach));
      }
    }
    lapShadows = path::toPath(shadows);
    lapHighlights = path::toPath(highlights);
  }
};

// ---------------------------------------------------------------------------
// One piece as an element. The mitre becomes the node's outline; the timber
// becomes its fill; the arris becomes a counter-rotated bevel so the light
// stays fixed in the room across hundreds of differently-angled boards.

/** @p piece, entering on the loop @p seconds is read from when one is
 *  given: it fades up and settles into its seat across its own beat.
 *  The settle starts at four fifths of full size and never overshoots,
 *  because the piece's face is a bake taken at the scale it is drawn at:
 *  a swell that passes through full size and back asks for a fresh bake
 *  of every board at each step of scale it crosses, where a settle that
 *  stays inside one step is one bake and then a blit. */
inline Element pieceElement(const Piece& piece, TimberBank& bank,
                            const choreograph::Output<float>* seconds) {
  const vec2 span = piece.to - piece.from;
  const float length = glm::length(span);
  const float angle = std::atan2(span.y, span.x);
  const float cosine = std::cos(-angle), sine = std::sin(-angle);
  const auto shear = [&](vec2 face) {
    const float x = face.x * cosine - face.y * sine;
    const float y = face.x * sine + face.y * cosine;
    return std::abs(y) < 0.02f ? 0.0f : piece.width * 0.5f * (x / y);
  };
  const float shearFrom = shear(piece.cutFrom), shearTo = shear(piece.cutTo);
  const float pad = std::max(std::abs(shearFrom), std::abs(shearTo)) + 0.5f;
  const float start = pad, end = pad + length;
  const SkPath outline = path::toPath(path::Polyline{
      .points = {{start - shearFrom, 0},
                 {end - shearTo, 0},
                 {end + shearTo, piece.width},
                 {start + shearFrom, piece.width}},
      .closed = true});

  // Which long edge catches the light: lattice pieces take one raking
  // source from the upper left, frame members the light of the opening.
  const vec2 outward{direction(span).y, -direction(span).x};
  const vec2 middle = (piece.from + piece.to) * 0.5f;
  const bool lit =
      piece.litFromOpening
          ? glm::dot(outward, direction(kCentre - middle)) > 0
          : glm::dot(outward, vec2{-0.45f, -0.89f}) > 0;

  // The timber already paints the arris. A bevel sized for a 45 px frame
  // member on an 8 px leaf double-counts it and the piece becomes a length
  // of rope, so the bevel scales with the stock and stays a hint on the
  // thin stuff.
  const bool heavy = piece.width > 20.0f;
  const float bevelAlpha = heavy ? 0.42f : 0.26f;
  const float degrees = angle * 57.29578f;
  const float width = length + 2 * pad;

  Element element =
      kit::at(middle.x - width * 0.5f, middle.y - piece.width * 0.5f, width,
              piece.width)
          .rotate(degrees)
          .shape(heldPath(outline))
          .fill(bank.get(*piece.timber, piece.width, !lit, piece.seed))
          .foreground(styles::BevelEmboss{heavy ? piece.width * 0.09f : 0.7f,
                                          heavy ? piece.width * 0.14f : 1.0f,
                                          120.0f + degrees,
                                          {1, 0.96f, 0.86f, bevelAlpha},
                                          {0.14f, 0.09f, 0.03f, bevelAlpha}})
          // The seam every abutting piece shows against its neighbour.
          .stroke(stroke(0.6f, Fill::color(kSeamInk),
                         PathFormat::Align::Inner))
          // A PIECE IS A PICTURE OF A PIECE. Neither the grain nor the arris
          // changes once the board is cut: the entrance moves where the
          // board is and how present it is, never what is on its face, so
          // the face is resolved once and the entrance is a blit.
          .cache(Cache::Texture);
  if (seconds != nullptr) {
    const float from = piece.enters, until = piece.enters + piece.entersFor;
    element
        .opacity(motion::bind(seconds)
                     .window(from, until)
                     .map(motion::ease::outCubic)
                     .scale(1.35f)
                     .clamp(0, 1))
        .scale(motion::bind(seconds)
                   .window(from, until)
                   .map(motion::ease::outCubic)
                   .target(0.8f, 1));
  }
  return element;
}

}  // namespace kumiko
