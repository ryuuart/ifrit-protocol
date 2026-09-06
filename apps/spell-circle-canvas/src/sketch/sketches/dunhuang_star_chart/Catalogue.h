#pragma once
// Catalogue.h — the catalogue dunhuang_star_chart is derived from, in a
// unit of its own beside the sketch, so that an edit to the plate never
// reads the tables again.
//
// THREE DATA FILES under res://data/dunhuang/, read through the sketch's
// own resource hub:
//
//   stars.csv      1,460 HIP stars from Stellarium's chinese_chenzhuo
//                  joined to HYG v4.1 — RA, Dec, magnitude — with proper
//                  motion carried back 1,300 years and the positions
//                  still on the J2000 EQUINOX, so the precession in the
//                  sketch is the only rotation applied;
//   asterisms.csv  the 317 asterisms — id, pinyin, native name, English,
//                  how many distinct stars each has, and its line art as
//                  a run of star indices, polyline after polyline with a
//                  kVSep word between them;
//   mansions.csv   the 28 lunar mansions in order — native name, pinyin,
//                  and the star index of the determinative star as
//                  Stellarium's lunar_system.defining_stars gives it.
//
// Nothing here draws, and the numbers are the join's: the sketch reads
// these and states beside each use what it makes of them.

#include <cstdint>
#include <string>
#include <vector>

namespace sigil::io {
class Hub;
}

namespace dunhuang {

/** One asterism, and where its vertex words stand in Catalogue::verts. */
struct AstRec {
  std::string id;
  std::string pinyin;
  std::string native;
  std::string english;
  uint16_t first = 0;
  uint16_t words = 0;
  uint16_t stars = 0;
};

/** One lunar mansion, and the star its position is taken from. */
struct XiuRec {
  std::string native;
  std::string pinyin;
  int star = 0;
};

/** Ends one polyline in Catalogue::verts. */
constexpr uint16_t kVSep = 0xFFFF;

/** The three tables as one value, in the shape the sketch walks them:
 *  the stars flat, three floats each, so a walk down them is one span. */
struct Catalogue {
  std::vector<float> star;
  std::vector<uint16_t> verts;
  std::vector<AstRec> asterism;
  std::vector<XiuRec> mansion;

  int stars() const { return (int)(star.size() / 3); }
  int asterisms() const { return (int)asterism.size(); }

  const AstRec& ast(int i) const { return asterism[(size_t)i]; }
  const XiuRec& xiu(int i) const { return mansion[(size_t)i]; }

  float ra(int i) const { return star[(size_t)i * 3 + 0]; }
  float dec(int i) const { return star[(size_t)i * 3 + 1]; }
  /** Read by nothing that draws: the chart does not encode magnitude,
   *  and the column stands so the catalogue is the catalogue. */
  float mag(int i) const { return star[(size_t)i * 3 + 2]; }
};

/** Reads the three files off @p hub. A file the hub cannot answer leaves
 *  its table empty rather than half-built, so a sketch missing its data
 *  draws nothing instead of drawing a fragment. */
Catalogue catalogue(sigil::io::Hub& hub);

}  // namespace dunhuang
