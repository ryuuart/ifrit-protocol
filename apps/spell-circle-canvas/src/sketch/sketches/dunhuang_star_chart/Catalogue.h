#pragma once
// Catalogue.h — the catalogue dunhuang_star_chart is derived from, in a
// unit of its own beside the sketch, so that an edit to the plate never
// compiles the tables again.
//
// FOUR TABLES, generated once from the join and frozen:
//
//   kStarData   1,460 HIP stars from Stellarium's chinese_chenzhuo joined
//               to HYG v4.1 — RA, Dec, magnitude, three floats per star —
//               with proper motion carried back 1,300 years and the
//               positions still on the J2000 EQUINOX, so the precession
//               in the sketch is the only rotation applied;
//   kVerts      every asterism's line vertices as star indices, polyline
//               after polyline with kVSep between them, each asterism
//               reading its own run from `first` for `words` entries;
//   kAst        the 317 asterisms — id, pinyin, native name, English —
//               each with where its run of vertex words starts and how
//               many distinct stars it has;
//   kXiuIndex   the 28 lunar mansions as the star index of each one's
//               determinative star, in mansion order.
//
// Nothing here draws, and nothing here is edited by hand: the sketch
// reads these and states beside each use what it makes of them.

#include <cstdint>

namespace dunhuang {

struct AstRec {
  const char* id;
  const char* pinyin;
  const char* native;
  const char* english;
  uint16_t first;
  uint16_t words;
  uint16_t stars;
};

constexpr int kStarCount = 1460;
constexpr int kAstCount = 317;
/** Ends one polyline in kVerts. */
constexpr uint16_t kVSep = 0xFFFF;

extern const float kStarData[];
extern const uint16_t kVerts[];
extern const AstRec kAst[];
extern const int kXiuIndex[28];

inline float starRa(int i) { return kStarData[i * 3 + 0]; }
inline float starDec(int i) { return kStarData[i * 3 + 1]; }
inline float starMag(int i) { return kStarData[i * 3 + 2]; }

}  // namespace dunhuang
