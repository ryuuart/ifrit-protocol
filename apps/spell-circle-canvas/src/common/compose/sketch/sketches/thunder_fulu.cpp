// thunder_fulu.cpp — 斬虹符, a Thunder-Rite command talisman, WRITTEN.
//
// THE ARTEFACT. 《道法會元》 Daofa huiyuan, DZ 1220 — the great Song–Yuan
// compendium of ritual, 268 juan, compiled after 1356 and preserved in the
// 正統道藏 of 1445. 卷四十六《上清神烈飛捷五雷大法》 carries a named, ordered
// set of TWELVE weather-command talismans, and it specifies its own canvas
// verbatim:
//
//   「以火日火時打鐵板一片，長五寸，闊三寸。以朱砂書符畢」
//   On a fire day at a fire hour, beat out ONE IRON PLATE, five cun long,
//   three cun wide. Write the talisman on it in CINNABAR.
//
// So: 5:3 vertical, ≈160 × 96 mm on the Ming 營造尺, vermilion on dark iron —
// not black on paper. The hero is 斬虹符, "sever the rainbow", whose chant
// ends 急急如律令. The other eleven stand on the 步罡踏斗 Dipper-tread.
// Text: Kanripo KR5g0029; ctext.org chapter 878073.
//
// A FU IS NOT AN IMAGE. It is a performance with a fixed score, and the
// artefact is DEFINED by that order: 符頭 (head) → 符竅 (aperture) → 符身
// (body) → 符膽 (gall) → 符腳 (foot), each stroke with a direction, a speed
// and a pressure. This study writes one, in order, at the speeds the sources
// give. Nothing here is a font. Every Han glyph on the plate is drawn as
// GEOMETRY from published stroke data, which is the only reason it can be
// animated at all — and the reason it needs no CJK font: 雲篆 cloud-seal and
// 雷書 thunder-writ are not in Unicode and will not be. (IRGN2522, Andrew
// West, 2022 — 123 Daoist-usage CHARACTERS proposed for Ext. J and objected
// to; talisman GRAPHS are not characters and nobody proposes them.) Every
// caption on this canvas is therefore Latin, on purpose.
//
// THE DATA, AND THREE CORRECTIONS FROM IT.
//   skishore/makemeahanzi `graphics.txt` gives, per character, each stroke's
//   filled outline AND its `medians` — the centreline polyline IN DRAWING
//   DIRECTION — in stroke order on a 1024 em box (y-up; screen y = 900 − y).
//   Eleven characters are embedded below EXACTLY as published: 577 median
//   points, nothing resampled, nothing smoothed on the way in.
//
//   1. 罡 HAS EXACTLY TEN STROKES, so the doctrine that it encodes the ten
//      Heavenly Stems 甲乙丙丁戊己庚辛壬癸 is checkable rather than
//      decorative. Panel A asserts it against the data and prints the count.
//   2. **KanjiVG DOES NOT CONTAIN 罡.** The brief says "makemeahanzi has the
//      coverage; KanjiVG has the taxonomy — use both", and for the other ten
//      characters that is right. But 罡 is not a Japanese kanji:
//      kanji/07f61.svg is a 14-byte 404. The one character whose stroke
//      classes this study most needs has no taxonomy to look up. So the
//      class is RECOVERED FROM THE MEDIAN GEOMETRY — arc length, the body
//      chord angle over the 14–86% span, and the peak turn away from the
//      entry direction — and the recovery is validated against the 54
//      strokes KanjiVG does carry for the seven characters shared with this
//      plate. Panel B prints the agreement and every disagreement.
//   3. The datasets disagree with each other twice, and both disagreements
//      are real: 鬼 is NINE strokes in makemeahanzi and TEN in KanjiVG (the
//      Japanese form carries an extra ノ), and the two sources order the
//      interior of 田 in 雷 the opposite way round — makemeahanzi writes the
//      middle 一 before the middle 丨, KanjiVG the reverse. Panel B prints
//      both, measured off the medians rather than asserted.
//
// THE BRUSH IS THE SKETCH. 起 · 行 · 收 as one width law over arc length:
//
//     w(s)/w₀ = 1.15·e^(−12s) + 0.62 + 0.28·s + 0.55·e^(−90(s−0.88)²)
//
// — 逆鋒起筆 (the tip goes backward first, so the head is blunt and swollen,
// 1.77 w₀), 中鋒行筆 (a thinning belly, 0.73 w₀ at s ≈ 0.28), a 頓 press at
// s = 0.88 (1.42 w₀) and a fast 收筆 cut. w₀ itself is 提按, set per stroke
// CLASS: a 橫 is thin, a 捺 is fat, a 點 is nearly all 頓.
//
//   THE BRIEF PREDICTED brushes::Ribbon takes a taper or a scalar and not an
//   arbitrary w(s), "and if it cannot take a profile callback that is the
//   gap to file". IT CAN — `Ribbon::widthFn` is
//   `std::function<float(const PathSample&)>` and takes exactly this. The
//   trap is one line further down: under `trim()` a decoration receives the
//   ALREADY-TRIMMED outline, so `PathSample::fraction` is a fraction of the
//   REVEALED contour, not of the stroke. Keying the profile to it makes the
//   頓 bulge SLIDE along the stroke as it draws. Key it to
//   `distance / fullLength` — a constant the sketch measures once — and the
//   press stays where the brush puts it. That is the finding, and `widthMax`
//   must be set by hand or the band is silently clipped.
//
// THE FOOT IS ONE CONTOUR. 「符腳是最後步驟…必須聚精會神，一氣立斷，不得遲緩
// 拖滯」 — the foot is the last step; total concentration, cut off in a
// single breath, no slowing or dragging. So 急急如律令 — 9 + 9 + 6 + 9 + 5 =
// THIRTY-EIGHT strokes — is built here as ONE PATH WITH ONE CONTOUR: the 38
// medians chained end to end by 37 ligature spans, one Ribbon, one trim, one
// reveal, no lift anywhere. Its widthFn carries a 75-span table so each
// stroke span gets its class weight and each ligature thins to 0.16 w₀. It
// is the only mark on this plate that is a single contour, and it is written
// at 0.034 s per stroke against the body's 0.240 — 7.1× — which is doctrine
// measured, not taste. 飛白 flying-white comes with the speed: two dashed
// rails at ops::Offset ∓2.6 px, so the dry streaks are longitudinal and
// follow the tangent BY CONSTRUCTION rather than by a canvas-axis shader.
//
// THE LAYOUT IS THE TREAD. 步罡踏斗, 禹步 「三步九跡」 — three steps making
// nine prints. The nine stations are the REAL Big Dipper: J2000 right
// ascension and declination, gnomonically projected about the asterism's own
// centroid (Dubhe, Merak, Phecda, Megrez, Alioth, Mizar, Alkaid, plus 左輔
// Alcor). 左輔 lies 0.008 of the asterism's span from 開陽 on the sky, which
// is why every 步罡 diagram separates them by hand; this one says so in the
// margin and separates them. 右弼 is invisible — its station is doctrine,
// not observation, and it is drawn open and unfilled.
//
// THE PLATE CARRIES 86 MARKS IN 49 NODES: 3 hooks, 1 aperture, 33 cloud-seal
// strokes, 罡's 10, one inverted-brush tap, and the foot's 38 as a single
// contour. Every one is a brushes::Ribbon over a real or derived median.
//
// BUILT FROM (the library, not by hand):
//   brushes::Ribbon + widthFn   every stroke on the plate; 起行收 over
//                        distance/length, w₀ from the recovered class
//   Brush legs + ops::Offset    飛白 as two dashed rails offset off the
//                        stroke's own centreline — tangent-aligned by
//                        construction, no per-stroke shader matrix
//   lines::displace      雲篆: the cloud-wander is a REAL median displaced,
//                        so the graphs are illegible honestly
//   trim(0, bind(&scribe).window(t0,t1))   ONE Output writes the whole
//                        plate; every stroke's own beat is its window
//   PathFormat::trimStart/trimEnd on a SECOND decoration — the wet 頓 pool
//                        riding the head of the self-drawing line, no
//                        second node
//   lines::Rails / hatch / crosshatch   hammer facets, seal ground, tread
//   brushes::ScatterBrush   the nine footprints of 禹步 along the tread
//   brushes::PatternBrush   corner tiles on the plate — corners rounded BY
//                        HAMMERING, not by a radius
//   ops::Sketchy         the beaten iron edge: low frequency, high deviation
//   shapes::chamfered / circle / star / parametric
//   Material::linear + patterns::grain + speckle   the iron, under
//                        Cache::Texture
//   console::LineRing    four panels of checks, printed as they run
//
// Run:
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/thunder_fulu.cpp \
//       --frame /tmp/thunder_fulu.png --at 20.6
//
//    1.9 s  三勾 — the three hooks of the 三清, one chant line per stroke
//    5.1 s  符竅 — the aperture, struck in one revolution, ends not meeting
//    6.4 s  虛書 — three graphs written by NOTHING: left eye, right eye,
//           tongue, visualised blue / red / white (法海遺珠 DZ 1166 卷六)
//    7.9 s  符身 — the cloud-seal command graphs, stroke by stroke
//   16.0 s  符膽 — 罡, ten strokes, the six-phrase chant landing with the last
//   19.7 s  倒頓筆頭 — the brush inverts and taps once
//   20.0 s  符腳 — 急急如律令, one breath, one contour, flying white
//   21.5 s  the 法印 comes down; the eleven other plates rise on the tread
//   27 s loops.

#include <sigilsketch/Sketch.h>

#include <sigilweave/FontContext.h>

#include <sigilcompose/Brushes.h>
#include <sigilcompose/Console.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Lines.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkTypeface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace {

constexpr SkColor4f hex(uint32_t v, float a = 1.0f) {
  return {((v >> 16) & 0xffu) / 255.0f, ((v >> 8) & 0xffu) / 255.0f,
          (v & 0xffu) / 255.0f, a};
}

template <typename... A> std::string fmt(const char *f, A... args) {
  char buf[512];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
  std::snprintf(buf, sizeof(buf), f, args...);
#pragma clang diagnostic pop
  return buf;
}

// ---------------------------------------------------------------------------
// palette — cinnabar on beaten iron under an altar lamp. 朱砂 is mercury
// sulphide ground in glue: a body colour that sits ON the metal, so it is the
// only bright thing here, and where the brush runs dry the IRON shows through
// rather than a paler red.

constexpr SkColor4f kNight = hex(0x08070a);
constexpr SkColor4f kIronDeep = hex(0x232120);
constexpr SkColor4f kIronMid = hex(0x35312c);
constexpr SkColor4f kIronLit = hex(0x4c453b);
constexpr SkColor4f kIronEdge = hex(0x6c6354);
constexpr SkColor4f kCinnabar = hex(0xcf3018);
constexpr SkColor4f kCinnaWet = hex(0xf2542a);
constexpr SkColor4f kCinnaDry = hex(0x8f2313);
constexpr SkColor4f kGold = hex(0xb2914f);
constexpr SkColor4f kGoldDim = hex(0x6d5a33);
constexpr SkColor4f kChalk = hex(0xd8cdb6);
constexpr SkColor4f kVoidBlue = hex(0x4f92d8);
constexpr SkColor4f kVoidRed = hex(0xd8422a);
constexpr SkColor4f kVoidWhite = hex(0xe7e2d6);

// ---------------------------------------------------------------------------
// canvas & the plate. 五寸 × 三寸 — the ratio is the specification, so it is
// the one number nothing else is allowed to round.

constexpr float kW = 1900, kH = 1220;
constexpr float kPW = 624.0f;               // the plate: 三寸
constexpr float kPH = kPW * 5.0f / 3.0f;    // 五寸 = 1040
constexpr float kPL = 76.0f, kPT = 92.0f;   // where it stands on the canvas
constexpr float kCol = 306.0f;              // the spine: every part is strung
                                            // on it, and the asymmetries off
                                            // it are deliberate

// ---------------------------------------------------------------------------
// §1 — THE STROKE DATA, verbatim from makemeahanzi `graphics.txt`.
// Layout: per stroke, [n, x0,y0, x1,y1, … x(n-1),y(n-1)] with the 1024 em box
// already flipped to screen orientation (y = 900 − y_published). In DRAWING
// ORDER and DRAWING DIRECTION — which is the whole point of the file.

struct Glyph {
  const char *id;      // romanised: every caption on this canvas is Latin
  const int16_t *data;
  int strokes;
};

// 罡  U+7F61 — 10 strokes, 50 median points
const int16_t k_gang[] = {
    3, 228, 155, 262, 181, 315, 389, 10, 296, 162, 312, 173, 577, 126, 660,
    115, 715, 115, 730, 123, 752, 152, 710, 287, 692, 324, 692, 350, 4, 401,
    183, 424, 283, 432, 307, 444, 320, 4, 543, 154, 570, 176, 541, 292, 527,
    298, 3, 335, 366, 354, 356, 661, 305, 6, 320, 461, 350, 469, 404, 470,
    610, 438, 640, 436, 665, 444, 6, 471, 490, 499, 507, 510, 522, 501, 749,
    503, 763, 520, 779, 4, 540, 636, 559, 625, 657, 607, 711, 605, 4, 281,
    606, 325, 652, 340, 783, 327, 795, 6, 108, 839, 179, 860, 402, 816, 610,
    803, 785, 806, 894, 849,
};
// 急  U+6025 — 9 strokes, 56 median points
const int16_t k_ji[] = {
    7, 466, 54, 485, 79, 492, 104, 467, 143, 378, 251, 319, 307, 268, 343,
    9, 458, 209, 483, 196, 562, 177, 605, 176, 624, 184, 621, 206, 555, 291,
    513, 338, 494, 348, 8, 297, 396, 351, 399, 534, 363, 620, 339, 663, 340,
    702, 376, 645, 538, 618, 554, 4, 306, 508, 339, 510, 552, 478, 585, 485,
    5, 290, 618, 321, 625, 608, 585, 654, 586, 682, 595, 3, 229, 704, 210,
    777, 164, 874, 13, 330, 702, 343, 720, 366, 783, 389, 817, 418, 844,
    460, 871, 505, 889, 584, 905, 714, 906, 750, 893, 767, 880, 741, 826,
    686, 749, 3, 477, 674, 536, 722, 563, 761, 4, 725, 645, 833, 689, 860,
    709, 892, 752,
};
// 如  U+5982 — 6 strokes, 41 median points
const int16_t k_ru[] = {
    9, 338, 103, 362, 123, 383, 160, 325, 408, 261, 612, 278, 631, 475, 757,
    494, 782, 504, 811, 9, 450, 365, 468, 379, 486, 410, 442, 569, 392, 675,
    354, 727, 300, 776, 241, 815, 164, 846, 5, 78, 472, 133, 495, 217, 469,
    424, 423, 441, 406, 4, 558, 443, 582, 464, 595, 487, 636, 720, 9, 613,
    446, 624, 454, 640, 455, 793, 412, 822, 418, 844, 434, 854, 446, 820,
    592, 792, 617, 5, 656, 687, 674, 670, 779, 650, 830, 648, 858, 656,
};
// 律  U+5F8B — 9 strokes, 49 median points
const int16_t k_lv[] = {
    6, 317, 113, 335, 138, 340, 160, 299, 220, 203, 319, 145, 359, 7, 306,
    296, 335, 335, 337, 345, 325, 368, 220, 518, 110, 634, 68, 667, 4, 268,
    498, 290, 560, 278, 805, 282, 866, 7, 441, 228, 503, 230, 719, 181, 752,
    187, 780, 215, 731, 374, 704, 394, 5, 404, 344, 449, 351, 833, 289, 872,
    288, 930, 302, 5, 432, 463, 469, 466, 695, 423, 738, 422, 762, 429, 5,
    447, 579, 469, 584, 516, 581, 718, 538, 768, 543, 5, 370, 708, 425, 714,
    676, 671, 839, 652, 920, 669, 5, 548, 64, 573, 70, 609, 104, 600, 286,
    595, 942,
};
// 令  U+4EE4 — 5 strokes, 34 median points
const int16_t k_ling[] = {
    9, 461, 67, 484, 88, 494, 118, 456, 198, 388, 302, 302, 407, 231, 476,
    142, 547, 76, 586, 6, 510, 181, 509, 194, 608, 304, 733, 426, 783, 464,
    975, 496, 3, 411, 408, 486, 460, 508, 491, 12, 261, 613, 286, 626, 323,
    631, 540, 572, 579, 567, 604, 571, 632, 595, 637, 603, 634, 612, 515,
    772, 512, 784, 502, 783, 4, 397, 743, 505, 842, 530, 884, 537, 922,
};
// 雨  U+96E8 — 8 strokes, 44 median points
const int16_t k_yu[] = {
    6, 312, 205, 343, 213, 396, 214, 520, 199, 698, 166, 747, 170, 6, 162,
    394, 196, 429, 209, 475, 217, 597, 218, 754, 232, 811, 14, 232, 443,
    246, 425, 259, 421, 510, 377, 813, 343, 845, 360, 855, 370, 858, 389,
    840, 629, 826, 726, 808, 796, 780, 831, 679, 791, 661, 777, 6, 465, 230,
    498, 249, 511, 275, 507, 588, 499, 690, 502, 764, 3, 318, 500, 372, 519,
    405, 543, 3, 310, 631, 373, 661, 396, 682, 3, 598, 466, 670, 485, 697,
    502, 3, 598, 608, 663, 634, 687, 654,
};
// 五  U+4E94 — 4 strokes, 26 median points
const int16_t k_wu[] = {
    5, 304, 206, 343, 215, 400, 214, 704, 165, 753, 167, 6, 460, 237, 489,
    267, 483, 313, 391, 705, 375, 726, 362, 732, 8, 273, 465, 308, 472, 345,
    470, 616, 424, 654, 433, 685, 466, 619, 700, 592, 721, 7, 63, 782, 138,
    803, 419, 751, 654, 739, 855, 747, 905, 762, 966, 790,
};
// 鬼  U+9B3C — 9 strokes, 60 median points
const int16_t k_gui[] = {
    5, 434, 51, 447, 59, 467, 90, 373, 219, 350, 228, 4, 242, 254, 271, 276,
    286, 305, 343, 537, 8, 296, 255, 329, 263, 617, 205, 680, 202, 702, 208,
    735, 248, 699, 340, 658, 481, 4, 387, 381, 534, 346, 578, 340, 600, 345,
    4, 364, 514, 380, 503, 613, 465, 635, 455, 10, 456, 258, 494, 282, 497,
    296, 472, 445, 453, 520, 407, 629, 348, 719, 274, 791, 214, 832, 139,
    870, 14, 506, 539, 529, 561, 546, 591, 536, 658, 533, 740, 541, 809,
    559, 840, 598, 863, 674, 876, 762, 876, 835, 869, 912, 846, 932, 832,
    924, 674, 8, 696, 521, 721, 551, 695, 617, 652, 686, 643, 720, 676, 720,
    794, 686, 807, 690, 3, 774, 603, 832, 675, 844, 720,
};
// 雲  U+96F2 — 12 strokes, 62 median points
const int16_t k_yun[] = {
    5, 387, 133, 409, 139, 475, 138, 623, 105, 684, 106, 5, 224, 251, 233,
    269, 235, 299, 188, 397, 183, 450, 10, 266, 286, 286, 295, 312, 294,
    442, 268, 747, 225, 806, 223, 821, 228, 845, 243, 857, 267, 769, 358, 4,
    480, 162, 512, 184, 518, 202, 509, 497, 3, 343, 345, 385, 361, 417, 385,
    3, 328, 452, 385, 472, 410, 490, 3, 618, 319, 679, 337, 702, 353, 3,
    613, 424, 676, 446, 702, 464, 4, 358, 576, 394, 576, 612, 536, 657, 542,
    5, 213, 683, 263, 692, 498, 655, 725, 631, 807, 650, 13, 485, 695, 439,
    713, 407, 760, 356, 812, 351, 820, 367, 830, 363, 867, 580, 816, 597,
    817, 607, 809, 619, 813, 636, 803, 657, 808, 4, 623, 723, 726, 844, 747,
    886, 753, 925,
};
// 雷  U+96F7 — 13 strokes, 63 median points
const int16_t k_lei[] = {
    4, 363, 125, 429, 132, 641, 96, 683, 98, 5, 204, 248, 216, 277, 214,
    299, 176, 377, 168, 436, 8, 248, 280, 260, 290, 282, 290, 446, 260, 783,
    219, 824, 235, 842, 261, 766, 345, 5, 479, 152, 513, 181, 513, 346, 502,
    482, 505, 532, 3, 313, 371, 365, 383, 400, 403, 3, 306, 497, 363, 507,
    382, 519, 3, 593, 325, 663, 343, 699, 370, 3, 589, 444, 667, 465, 694,
    494, 5, 250, 602, 274, 619, 299, 655, 325, 832, 347, 907, 12, 296, 602,
    307, 611, 346, 619, 527, 596, 660, 570, 698, 570, 717, 579, 741, 607,
    740, 618, 707, 833, 684, 875, 683, 934, 4, 401, 723, 460, 729, 574, 703,
    620, 704, 5, 476, 620, 503, 640, 510, 663, 508, 803, 493, 821, 3, 364,
    870, 375, 857, 639, 831,
};
// 號  U+865F — 13 strokes, 92 median points
const int16_t k_hao[] = {
    4, 167, 150, 192, 168, 201, 183, 236, 313, 9, 224, 150, 234, 157, 326,
    130, 343, 128, 357, 134, 377, 155, 375, 163, 356, 219, 330, 236, 4, 253,
    290, 263, 279, 325, 261, 381, 264, 6, 70, 447, 87, 452, 131, 449, 317,
    388, 344, 382, 376, 384, 12, 186, 463, 200, 470, 207, 486, 185, 563,
    221, 565, 276, 551, 306, 558, 324, 578, 294, 723, 261, 794, 227, 819,
    155, 772, 4, 601, 54, 641, 91, 637, 231, 620, 246, 3, 677, 138, 749,
    116, 801, 112, 7, 476, 293, 498, 303, 750, 237, 810, 227, 827, 235, 842,
    253, 787, 321, 9, 414, 280, 443, 309, 449, 339, 441, 491, 424, 600, 407,
    672, 382, 742, 343, 820, 287, 899, 4, 511, 426, 573, 425, 702, 384, 733,
    383, 10, 582, 316, 604, 342, 605, 463, 613, 486, 636, 508, 694, 521,
    739, 515, 767, 503, 785, 486, 779, 409, 8, 532, 608, 550, 621, 563, 641,
    554, 701, 537, 760, 517, 795, 481, 836, 409, 881, 12, 660, 587, 688,
    623, 679, 749, 686, 824, 694, 844, 723, 867, 767, 880, 826, 883, 881,
    875, 921, 854, 933, 838, 944, 712,
};

enum G { GANG, JI, RU, LV, LING, YU, WU, GUI, YUN, LEI, HAO, GCOUNT };
const Glyph kGlyphs[GCOUNT] = {
    {"GANG U+7F61", k_gang, 10}, {"JI   U+6025", k_ji, 9},
    {"RU   U+5982", k_ru, 6},    {"LU   U+5F8B", k_lv, 9},
    {"LING U+4EE4", k_ling, 5},  {"YU   U+96E8", k_yu, 8},
    {"WU   U+4E94", k_wu, 4},    {"GUI  U+9B3C", k_gui, 9},
    {"YUN  U+96F2", k_yun, 12},  {"LEI  U+96F7", k_lei, 13},
    {"HAO  U+865F", k_hao, 13},
};

// KanjiVG kvg:type, mapped to the six classes, in KanjiVG's stroke order.
// 罡 is ABSENT from KanjiVG entirely (kanji/07f61.svg → 404). That absence
// is why this file has a classifier at all.
enum Cls { HENG, SHU, PIE, NA, DIAN, TURN, CLSN };
const char *kClsName[CLSN] = {"HENG", "SHU ", "PIE ", "NA  ", "DIAN", "TURN"};

const int8_t kKvg_ji[] = {2, 5, 5, 0, 0, 4, 5, 4, 4};
const int8_t kKvg_ru[] = {5, 2, 0, 1, 5, 0};
const int8_t kKvg_lv[] = {2, 2, 1, 5, 0, 0, 0, 0, 1};
const int8_t kKvg_ling[] = {2, 3, 4, 5, 4};
const int8_t kKvg_yu[] = {0, 1, 5, 1, 4, 4, 4, 4};
const int8_t kKvg_wu[] = {0, 1, 5, 0};
const int8_t kKvg_lei[] = {0, 4, 5, 1, 4, 4, 4, 4, 1, 5, 1, 0, 0};
const int8_t kKvg_gui[] = {2, 1, 5, 1, 0, 0, 2, 5, 5, 4};
struct KvgRow {
  int glyph;
  const int8_t *cls;
  int n;
};
const KvgRow kKvg[] = {{JI, kKvg_ji, 9},   {RU, kKvg_ru, 6},
                       {LV, kKvg_lv, 9},   {LING, kKvg_ling, 5},
                       {YU, kKvg_yu, 8},   {WU, kKvg_wu, 4},
                       {LEI, kKvg_lei, 13}, {GUI, kKvg_gui, 10}};

// ---------------------------------------------------------------------------
// §2 — MEDIANS → GEOMETRY.

using Poly = std::vector<SkPoint>;

/** One character's medians, in em units [0,1]², y down, drawing order. */
std::vector<Poly> medians(int glyph) {
  const Glyph &g = kGlyphs[glyph];
  std::vector<Poly> out;
  const int16_t *p = g.data;
  for (int s = 0; s < g.strokes; ++s) {
    const int n = *p++;
    Poly poly;
    poly.reserve((size_t)n);
    for (int i = 0; i < n; ++i, p += 2)
      poly.push_back({(float)p[0] / 1024.0f, (float)p[1] / 1024.0f});
    out.push_back(std::move(poly));
  }
  return out;
}

/** Resample a polyline to n stations by arc length — the classifier's own
 *  measure, and hanzi-writer/animCJK do exactly this before animating. */
Poly resample(const Poly &p, int n) {
  Poly out;
  if (p.size() < 2)
    return p;
  std::vector<float> seg(p.size() - 1);
  float total = 0;
  for (size_t i = 0; i + 1 < p.size(); ++i) {
    seg[i] = std::hypot(p[i + 1].fX - p[i].fX, p[i + 1].fY - p[i].fY);
    total += seg[i];
  }
  if (total < 1e-6f)
    return Poly((size_t)n, p.front());
  for (int k = 0; k < n; ++k) {
    const float d = total * (float)k / (float)(n - 1);
    float acc = 0;
    for (size_t i = 0; i < seg.size(); ++i) {
      if (acc + seg[i] >= d || i + 1 == seg.size()) {
        const float t = seg[i] > 0 ? (d - acc) / seg[i] : 0;
        out.push_back({p[i].fX + (p[i + 1].fX - p[i].fX) * t,
                       p[i].fY + (p[i + 1].fY - p[i].fY) * t});
        break;
      }
      acc += seg[i];
    }
  }
  return out;
}

struct Feat {
  float length = 0;  // em
  float angle = 0;   // deg, body chord, +x right, +y DOWN
  float turn = 0;    // deg, peak deviation from the entry direction
};

/** The three numbers the taxonomy actually turns on. The 起筆 and 收筆
 *  flicks live in the outer 14% at each end and swing the chord by up to
 *  90°, so the body span — and only the body span — is measured. */
Feat features(const Poly &m) {
  Feat f;
  const Poly r = resample(m, 33);
  if (r.size() < 3)
    return f;
  for (size_t i = 0; i + 1 < m.size(); ++i)
    f.length += std::hypot(m[i + 1].fX - m[i].fX, m[i + 1].fY - m[i].fY);
  const size_t a = (size_t)(0.14f * 32), b = (size_t)(0.86f * 32);
  const Poly body(r.begin() + (long)a, r.begin() + (long)b + 1);
  const float a0 = std::atan2(body[1].fY - body[0].fY, body[1].fX - body[0].fX);
  float turn = 0;
  for (size_t i = 1; i + 1 < body.size(); ++i) {
    const float dx = body[i + 1].fX - body[i].fX;
    const float dy = body[i + 1].fY - body[i].fY;
    if (std::hypot(dx, dy) < 1e-9f)
      continue;
    float t = std::atan2(dy, dx) - a0;
    while (t > SK_ScalarPI)
      t -= 2 * SK_ScalarPI;
    while (t < -SK_ScalarPI)
      t += 2 * SK_ScalarPI;
    turn = std::max(turn, std::abs(t));
  }
  f.turn = turn * 57.29578f;
  f.angle = std::atan2(body.back().fY - body.front().fY,
                       body.back().fX - body.front().fX) *
            57.29578f;
  return f;
}

/** The recovered stroke class. Validated against KanjiVG in panel B. */
int classify(const Poly &m) {
  const Feat f = features(m);
  if (f.turn > 62.0f)
    return TURN;
  if (f.length < 0.20f)
    return DIAN;
  if (std::abs(f.angle) < 27.0f)
    return HENG;
  if (f.angle >= 27.0f && f.angle <= 72.0f)
    return NA;
  if (f.angle > 100.0f || f.angle < -100.0f)
    return PIE;
  return SHU;
}

/** 提按 — lift and press. w₀ as a fraction of the glyph's em box: a 橫 is
 *  thin, a 捺 is fat, a 點 is nearly all 頓. */
float w0ForClass(int cls) {
  static const float k[CLSN] = {0.0255f, 0.0310f, 0.0292f,
                                0.0440f, 0.0470f, 0.0318f};
  return k[cls];
}

/** 起 · 行 · 收 — the width law, normalised to w₀. Flagged as a MODEL: the
 *  shape is what the robotic-calligraphy literature fits (two symmetric
 *  cubics either side of the median, B-BSMG 2024); the constants are chosen
 *  so 逆鋒 reads at 1.77, the belly bottoms at 0.73 and the 頓 press peaks
 *  at 1.42 — the ratios a loaded wolf-hair brush actually leaves. */
float widthLaw(float s) {
  s = std::clamp(s, 0.0f, 1.0f);
  const float d = s - 0.88f;
  return 1.15f * std::exp(-12.0f * s) + 0.62f + 0.28f * s +
         0.55f * std::exp(-90.0f * d * d);
}

/** Polyline → a smooth path through it (quads through the midpoints). The
 *  medians are sparse on purpose — 3 to 14 points for a whole stroke — so
 *  the smoothing is not decoration, it is what makes the centreline a
 *  centreline rather than a chain of chords. */
SkPath smoothPath(const Poly &p) {
  SkPathBuilder b;
  if (p.size() < 2)
    return b.detach();
  b.moveTo(p[0]);
  if (p.size() == 2) {
    b.lineTo(p[1]);
    return b.detach();
  }
  for (size_t i = 1; i + 1 < p.size(); ++i)
    b.quadTo(p[i], {(p[i].fX + p[i + 1].fX) * 0.5f,
                    (p[i].fY + p[i + 1].fY) * 0.5f});
  b.lineTo(p.back());
  return b.detach();
}

float pathLength(const SkPath &p) {
  float total = 0;
  SkContourMeasureIter it(p, false);
  while (sk_sp<SkContourMeasure> c = it.next())
    total += c->length();
  return total;
}

/** Place an em-unit polyline into a box. */
Poly place(const Poly &m, SkPoint origin, float sx, float sy) {
  Poly out;
  out.reserve(m.size());
  for (const SkPoint &q : m)
    out.push_back({origin.fX + q.fX * sx, origin.fY + q.fY * sy});
  return out;
}

// ---------------------------------------------------------------------------
// §3 — THE BIG DIPPER, and the tread on it.
//
// J2000 RA/Dec, gnomonically projected about the asterism's own centroid and
// normalised so the seven stars span 1.0 in x. 左輔 Alcor comes out 0.0079
// from 開陽 Mizar, which is the true sky and useless as a station — every
// 步罡 plate separates the pair by hand, and so does the margin note here.

struct Star {
  float x, y;
  const char *name;
  const char *ritual;
};
const Star kDipper[9] = {
    {0.9886f, 0.0000f, "Dubhe", "TIAN SHU"},
    {1.0000f, 0.2288f, "Merak", "TIAN XUAN"},
    {0.6862f, 0.3434f, "Phecda", "TIAN JI"},
    {0.5559f, 0.2011f, "Megrez", "TIAN QUAN"},
    {0.3229f, 0.2469f, "Alioth", "YU HENG"},
    {0.1425f, 0.2909f, "Mizar", "KAI YANG"},
    {0.0000f, 0.5302f, "Alkaid", "YAO GUANG"},
    // 左輔 Alcor: measured at (0.1347, 0.2883) — 0.0079 from Mizar. Pushed
    // out along the handle's normal to be a station, and said so.
    {0.0640f, 0.4020f, "Alcor", "ZUO FU"},
    // 右弼 is invisible. Its station is doctrine, not observation.
    {0.2180f, 0.4390f, "(invisible)", "YOU BI"},
};

// The twelve talismans of 卷四十六, verbatim and in the order they appear.
// 斬虹符 is the hero; the other eleven stand on the tread.
struct Fu {
  const char *pinyin;
  const char *gloss;
};
const Fu kOthers[11] = {
    {"QI FENG FU", "raise wind"},       {"QI YUN FU", "raise cloud"},
    {"QI YU FU", "pray for rain"},      {"QI LEI FU", "raise thunder"},
    {"QI DIAN FU", "raise lightning"},  {"QI XUE FU", "pray for snow"},
    {"JIANG BAO FU", "bring down hail"},{"JUAN WU FU", "roll up fog"},
    {"QI QING FU", "pray for clear"},   {"ZHAO LONG FU", "summon the dragon"},
    {"JIANG XUE FU", "bring down snow"},
};

// ---------------------------------------------------------------------------
// §4 — THE SCORE. Seconds. The tempo marks are quotations, not choices.

constexpr float tPlate = 0.05f, tPlateEnd = 1.15f;
constexpr float tLoad = 1.20f;
constexpr float tHead = 1.90f, tHeadEach = 1.00f, tHeadGap = 0.05f;
constexpr float tRing = 5.05f, tRingDur = 1.15f;
constexpr float tVoid = 6.35f, tVoidDur = 1.40f;
constexpr float tBody = 7.85f, tBodyEach = 0.240f; // 33 strokes → 7.92 s
constexpr float tGall = 15.95f, tGallEach = 0.360f; // 10 strokes → 3.60 s
constexpr float tTap = 19.65f, tTapDur = 0.25f;
constexpr float tFoot = 20.00f, tFootEach = 0.034f; // 38 strokes → 1.292 s
constexpr float tSeal = 21.45f;
constexpr float tStars = 13.10f;
constexpr float kLoop = 27.0f;

sigil::weave::TextStyle type(sk_sp<SkTypeface> face, float size, SkColor4f c,
                             float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = std::move(face);
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor4f(c, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

// the 踏符頭 chant: one line said silently per hook, as the hook goes down
const char *kHeadChant[3] = {
    "1st stroke  the world moves",
    "2nd stroke  the patriarch's sword",
    "3rd stroke  malign gods, a thousand li hence",
};
// the six phrases sung WHILE 罡 is drawn — the chant must finish exactly as
// the tenth stroke lands
const char *kGallChant[6] = {
    "open the gate of heaven", "kill the ghost-road",
    "open the earth-prison",   "bar the road of men",
    "slay the ghost-troops",   "break the ghost's belly",
};

} // namespace

// ===========================================================================

struct ThunderFulu : sigil::compose::sketch::Sketch {
  sk_sp<SkTypeface> faceSerif, faceItalic, faceMono, faceDisplay, faceSmall;

  // ONE Output writes the entire plate: it is the score position in seconds,
  // and every stroke's beat is a window() on it.
  ch::Output<float> scribe{0.0f};
  double clockT = 0;

  console::LineRing logA{64}, logB{64}, logC{64}, logD{64};

  Material ironGrain;
  Pattern ironSpeck;
  Element footPrint; // ScatterBrush art, held for pointer stability
  Element hammerTile, hammerCorner;

  // --- the ink ------------------------------------------------------------
  struct Stroke {
    SkPath path;      // plate-local, already cloud-wandered where it should be
    SkRect frame;     // the node's own box — sized to content, never to plate
    float len = 1;
    float w0 = 6;
    int cls = TURN;
    float t0 = 0, t1 = 1;
    bool dry = false; // 飛白
    SkColor4f ink = kCinnabar;
    std::string key;
    // the foot is ONE contour of 75 spans: 38 strokes and 37 ligatures
    std::vector<std::array<float, 3>> spans; // {d0, d1, w0}  (w0 = 0 → nothing)
  };
  std::vector<Stroke> strokes;
  int nHead = 0, nBody = 0, nGall = 0, nFootStrokes = 0;

  // --- verification -------------------------------------------------------
  int kvgAgree = 0, kvgTotal = 0;
  std::vector<std::string> kvgMiss;
  std::array<int, 10> gangCls{};

  // =========================================================================
  // BUILDING THE INK

  /** One stroke → its node. The whole brush model is here. */
  Element inkStroke(const Stroke &s) const {
    brushes::Ribbon rib;
    rib.fill = Fill::color(s.ink);
    rib.step = 2.2f;
    rib.widthMax = s.w0 * 2.0f + 2.0f;

    if (s.spans.empty()) {
      const float len = s.len, w0 = s.w0;
      // KEYED TO distance/len, NOT PathSample::fraction. Under trim() the
      // decoration is handed the REVEALED contour, so `fraction` is a
      // fraction of what has been drawn so far and the 頓 press would slide
      // down the stroke as it writes.
      rib.widthFn = [len, w0](const PathSample &p) {
        return w0 * widthLaw(len > 0 ? p.distance / len : 0.0f);
      };
    } else {
      // 一氣立斷 — the foot. One contour, 75 spans, no lift.
      const std::vector<std::array<float, 3>> spans = s.spans;
      const float w0 = s.w0;
      rib.widthFn = [spans, w0](const PathSample &p) {
        for (const std::array<float, 3> &sp : spans)
          if (p.distance >= sp[0] && p.distance <= sp[1]) {
            if (sp[2] <= 0.0f)
              return w0 * 0.16f; // the ligature: the brush never leaves the
                                 // plate, it only lightens
            const float u = (p.distance - sp[0]) / std::max(1e-3f, sp[1] - sp[0]);
            return w0 * sp[2] * widthLaw(u);
          }
        return w0 * 0.16f;
      };
      rib.widthMax = s.w0 * 2.4f;
    }

    Brush brush;
    brush.leg(rib);
    if (s.dry) {
      // 飛白. At speed a dry brush's hair bundles separate and the ground
      // shows through in LONGITUDINAL streaks. Two dashed rails offset off
      // the stroke's own centreline are tangent-aligned by construction —
      // no per-stroke shader matrix, which is what a canvas-axis grain
      // would have needed.
      PathFormat hair;
      hair.width = 1.3f;
      hair.strokeFill = Fill::color(hex(0x231f1c));
      hair.cap = SkPaint::kButt_Cap;
      hair.dashIntervals = {5.0f, 3.4f};
      hair.dashPhase = 2.0f;
      brush.leg(hair, {ops::Offset{2.2f, 3.0f}});
      PathFormat hair2 = hair;
      hair2.width = 1.0f;
      hair2.dashIntervals = {3.6f, 5.2f};
      hair2.dashPhase = 5.5f;
      brush.leg(hair2, {ops::Offset{-2.4f, 3.0f}});
    }

    const SkRect f = s.frame;
    const SkPath local = s.path.makeOffset(-f.left(), -f.top());
    Element e = box()
                    .left(f.left())
                    .top(f.top())
                    .width(Dim(f.width()))
                    .height(Dim(f.height()))
                    .outline([local](SkSize) { return local; })
                    .fill(Fill::none())
                    .stroke(std::move(brush))
                    .key(s.key);
    // The wet 頓 pool riding the head of the self-drawing line. A decoration
    // receives the ALREADY-trimmed outline, so its own window is a fraction
    // of the revealed part — this needs no second node.
    PathFormat wet;
    wet.width = s.w0 * 0.30f;
    wet.strokeFill = Fill::color({kCinnaWet.fR, kCinnaWet.fG, kCinnaWet.fB, 0.7f});
    wet.cap = SkPaint::kButt_Cap;
    wet.trimStart = 0.93f;
    wet.trimEnd = 1.0f;
    e.foreground(wet);
    e.trim(0.0f, bind(&scribe).window(s.t0, s.t1));
    return e;
  }

  /** Register a stroke: measure it, size its box to its own content. */
  void push(SkPath p, float w0, int cls, float t0, float t1, SkColor4f ink,
            std::string key, bool dry = false,
            std::vector<std::array<float, 3>> spans = {}) {
    Stroke s;
    s.len = pathLength(p);
    s.w0 = w0;
    s.cls = cls;
    s.t0 = t0;
    s.t1 = t1;
    s.ink = ink;
    s.dry = dry;
    s.spans = std::move(spans);
    const float pad = w0 * 1.4f + 6.0f;
    s.frame = p.getBounds().makeOutset(pad, pad);
    s.path = std::move(p);
    s.key = std::move(key);
    strokes.push_back(std::move(s));
  }

  /** 雲篆. Strokes 「盤曲如雲」 — twisted like winding cloud. The wander is a
   *  REAL median displaced, never an invented squiggle: lines::displace
   *  offsets perpendicular to the tangent, so the graph keeps its topology
   *  and loses its legibility, which is exactly the intent of the script. */
  static SkPath cloud(const SkPath &p, float amp, float wl) {
    return lines::displace(p, amp, wl, false);
  }

  /** A composite cloud graph: components stacked vertically in one box, each
   *  component's real medians, the whole column wandered. */
  void cloudGraph(const std::vector<std::pair<int, float>> &parts, SkPoint at,
                  float size, float t, float each, float amp, int tag) {
    float y = at.fY;
    float total = 0;
    for (const auto &[g, h] : parts)
      total += h;
    int idx = 0;
    for (const auto &[g, share] : parts) {
      const std::vector<Poly> ms = medians(g);
      const float bh = size * share / total;
      for (size_t i = 0; i < ms.size(); ++i) {
        const int cls = classify(ms[i]);
        Poly q = place(ms[i], {at.fX - size * 0.5f, y}, size, bh);
        SkPath sp = cloud(smoothPath(q), amp, size * 0.42f);
        push(sp, w0ForClass(cls) * size * 0.92f, cls, t + (float)idx * each,
             t + (float)(idx + 1) * each - 0.03f, kCinnabar,
             fmt("body%d_%d", tag, (int)idx));
        ++idx;
      }
      y += bh;
    }
  }

  // =========================================================================
  // THE PLATE

  Element ironGround() {
    auto g = box().inset(0);

    // the plate stands off the altar cloth
    g.child(box()
                .left(-16)
                .top(-8)
                .width(Dim(kPW + 46))
                .height(Dim(kPH + 44))
                .outline(shapes::chamfered(26.0f))
                .fill(Material::radialUnit({0.5f, 0.5f}, 0.78f,
                                           {{0.0f, hex(0x000000, 0.66f)},
                                            {0.72f, hex(0x000000, 0.40f)},
                                            {1.0f, hex(0x000000, 0.0f)}}))
                .key("ironshadow"));

    // the plate itself: hammered iron, warm under an altar lamp. The edge is
    // NOT a radius — it is what a hammer leaves.
    g.child(box()
                .inset(0)
                .outline([](SkSize s) {
                  return ops::sketchy(46.0f, 2.6f, 1356)(
                      shapes::chamfered(17.0f)(s));
                })
                // linearUnit, not linear: linear() is in NODE PIXELS, so
                // a {0.1,0} -> {0.9,1} ramp is one pixel wide at the corner
                // and clamps the whole plate to its last stop.
                .fill(Material::linearUnit({0.10f, -0.06f}, {0.96f, 1.0f},
                                           {{0.0f, hex(0x736a5b)},
                                            {0.18f, hex(0x4f4840)},
                                            {0.46f, kIronMid},
                                            {0.78f, hex(0x201e1d)},
                                            {1.0f, hex(0x161514)}}))
                .foreground(lines::hatch(Fill::color(hex(0xa79a83, 0.075f)),
                                         13.0f, 1.6f, -18.0f))
                .foreground(lines::hatch(Fill::color(hex(0x000000, 0.13f)),
                                         31.0f, 3.4f, 24.0f))
                .foreground(Wash{.material = ironGrain,
                                 .blend = SkBlendMode::kOverlay,
                                 .amount = 0.30f})
                .foreground(Wash{.material = ironSpeck.material(),
                                 .blend = SkBlendMode::kMultiply,
                                 .amount = 0.85f})
                .cache(Cache::Texture)
                .key("iron"));

    // the beaten edge, and the corners rounded BY HAMMERING. PatternBrush
    // corner tiles: a facet, not a fillet.
    //
    // `cornerAlign` is spelled out below and the value is the DEFAULT, which
    // is the point. It defaults to Bisector only since f706f5d (12:03);
    // before that every corner behaved as Outgoing, and a study written
    // before that hour has art in the other frame and no way to say so — one
    // did, and its corners silently turned 45 degrees. This plate was
    // written at 12:36 against the new header, so the facets were drawn and
    // approved on the bisector, and on a bisector is where they belong: a
    // hammer lands on the CORNER, and the flat it leaves straddles both legs
    // instead of lying along one of them. Audited by rendering the same
    // frame with Outgoing forced — the six facets rotate 13 to 35 degrees
    // (half the chamfer's own turn, perturbed by ops::sketchy) and nothing
    // snaps into or out of alignment, because a stubby lozenge at elongation
    // 1.4 has no strong axis to align. So the value stands, and now it is
    // stated rather than inherited.
    g.child(box()
                .inset(0)
                .outline([](SkSize s) {
                  return ops::sketchy(38.0f, 3.1f, 46)(
                      shapes::chamfered(17.0f)(s));
                })
                .fill(Fill::none())
                .stroke(Brush{}
                            .leg(lines::rails(
                                {{.offset = 0.0f,
                                  .width = 3.0f,
                                  .fill = Fill::color(hex(0x5d564a, 0.85f))},
                                 {.offset = 5.5f,
                                  .width = 1.1f,
                                  .fill = Fill::color(hex(0x0a0909, 0.75f))}}))
                            .leg(brushes::PatternBrush{
                                .side = hammerTile,
                                .corner = hammerCorner,
                                .advance = 26.0f,
                                .cornerAngleDeg = 30.0f,
                                .cornerLength = 34.0f,
                                .cornerAlign = brushes::PatternBrush::
                                    CornerAlign::Bisector,
                                .reach = 22.0f}))
                .cache(Cache::Texture)
                .key("ironedge"));
    return g;
  }

  /** The grain wash that unifies ink and iron — the flying-white streaks are
   *  painted in the iron's own mid-tone, and this is what stops them reading
   *  as paint. Explicitly baked: the library refuses to promote an
   *  opacity+blend leaf because compositing a bake rounds twice, and it is
   *  right to refuse and I am right to accept it. */
  Element ironWash() {
    return box()
        .inset(0)
        .outline(shapes::chamfered(17.0f))
        .fill(ironGrain)
        .opacity(0.085f)
        .blend(SkBlendMode::kSoftLight)
        .cache(Cache::Texture)
        .key("wash");
  }

  // =========================================================================
  // THE INK, ASSEMBLED IN ORDER

  void buildStrokes() {
    strokes.clear();

    // --- 符頭 三勾 ------------------------------------------------------
    // Three hooks for the 三清 — 元始天尊, 靈寶天尊, 道德天尊 — written
    // FIRST, one silent chant line per stroke: 踏符頭, "treading the head".
    // A descending stair, each starting further LEFT and LOWER, not three
    // parallel marks.
    for (int k = 0; k < 3; ++k) {
      const float sx = 246.0f - (float)k * 32.0f;
      const float sy = 42.0f + (float)k * 47.0f;
      const float w = 150.0f, h = 44.0f;
      Poly p = {{sx, sy},
                {sx + 0.34f * w, sy + 0.09f * h},
                {sx + 0.66f * w, sy + 0.30f * h},
                {sx + 0.88f * w, sy + 0.66f * h},
                {sx + 0.78f * w, sy + 1.00f * h},
                {sx + 0.56f * w, sy + 0.88f * h}};
      const float t0 = tHead + (float)k * (tHeadEach + tHeadGap);
      push(smoothPath(p), 9.6f - (float)k * 0.6f, TURN, t0, t0 + tHeadEach,
           kCinnabar, fmt("hook%d", k));
    }
    nHead = (int)strokes.size();

    // --- 符竅 -----------------------------------------------------------
    // A plain circle between head and foot, struck in ONE revolution — the
    // ends do not meet, because that is what a one-stroke ring looks like.
    // 《帝令寶珠五雷祈禱大法》 draws it exactly so, with a spirit visualised
    // inside it.
    {
      const float r = 58.0f;
      SkPathBuilder b;
      const int n = 72;
      for (int i = 0; i <= n; ++i) {
        const float a = -1.35f + 6.02f * (float)i / (float)n;
        const SkPoint q{kCol + r * std::cos(a) * 1.02f,
                        242.0f + r * std::sin(a) * 0.96f};
        i == 0 ? b.moveTo(q) : b.lineTo(q);
      }
      push(b.detach(), 7.4f, TURN, tRing, tRing + tRingDur, kCinnabar, "ring");
    }

    // --- 符身 ------------------------------------------------------------
    // The cloud-seal command graphs. No reader can read these, and that is
    // the point: they are real medians run through the cloud-wander, so the
    // illegibility is derived rather than invented.
    //   G1  YU over WU — 雨 (the 青城 符座) over 五, for 五雷      8 + 4 = 12
    //   G2  YUN — 雲, wandered hardest                                    12
    //   G3  GUI — 鬼, the 神霄 符座, SET ON THE LEFT per 《高上神霄玉清真王
    //       紫書大法》卷六                                                  9
    const int before = (int)strokes.size();
    cloudGraph({{YU, 0.58f}, {WU, 0.42f}}, {kCol + 6, 300.0f}, 158.0f, tBody,
               tBodyEach, 3.4f, 1);
    cloudGraph({{YUN, 1.0f}}, {kCol + 30, 430.0f}, 150.0f,
               tBody + 12 * tBodyEach, tBodyEach, 5.2f, 2);
    cloudGraph({{GUI, 1.0f}}, {kCol - 44, 556.0f}, 148.0f,
               tBody + 24 * tBodyEach, tBodyEach, 4.2f, 3);
    nBody = (int)strokes.size() - before;

    // --- 符膽 罡 ---------------------------------------------------------
    // The soul of the fu; without it the sheet is 空符, waste paper. TEN
    // strokes for the ten Heavenly Stems, and larger than anything else on
    // the plate. NOT wandered: its ten strokes are the argument, so it has
    // to stay countable.
    {
      const std::vector<Poly> ms = medians(GANG);
      const float size = 196.0f;
      for (size_t i = 0; i < ms.size(); ++i) {
        const int cls = classify(ms[i]);
        gangCls[i] = cls;
        Poly q = place(ms[i], {kCol - size * 0.5f, 574.0f}, size, size);
        const float t0 = tGall + (float)i * tGallEach;
        push(smoothPath(q), w0ForClass(cls) * size, cls, t0, t0 + tGallEach - 0.04f,
             kCinnabar, fmt("gang%d", (int)i));
      }
      nGall = 10;
    }

    // --- 倒頓筆頭 --------------------------------------------------------
    // Invert the brush and tap the butt-end once. 「若對象為孕子或患有眼疾
    // 者，則萬萬不可倒頓筆頭」 — never for a pregnant woman or a patient with
    // eye disease. Drawn, and noted in the margin.
    {
      SkPathBuilder b;
      b.moveTo(kCol + 96, 790);
      b.lineTo(kCol + 99, 795);
      push(b.detach(), 12.0f, DIAN, tTap, tTap + tTapDur, kCinnaWet, "tap");
    }

    // --- 符腳 急急如律令 --------------------------------------------------
    // ONE PATH, ONE CONTOUR. 38 strokes chained by 37 ligatures — the brush
    // does not leave the plate between them, it only lightens. Written at
    // 0.034 s a stroke against the body's 0.240.
    {
      const int seq[5] = {JI, JI, RU, LV, LING};
      const float sizes[5] = {76.0f, 71.0f, 68.0f, 68.0f, 92.0f};
      const float ys[5] = {816.0f, 858.0f, 897.0f, 934.0f, 982.0f};
      const float xs[5] = {kCol - 18, kCol + 24, kCol - 22, kCol + 20, kCol - 4};
      SkPathBuilder b;
      std::vector<std::array<float, 3>> spans;
      std::vector<SkPoint> flat;
      std::vector<std::pair<size_t, size_t>> strokeRanges;
      std::vector<float> strokeW;
      for (int c = 0; c < 5; ++c) {
        const std::vector<Poly> ms = medians(seq[c]);
        for (const Poly &m : ms) {
          Poly q = place(m, {xs[c] - sizes[c] * 0.5f, ys[c] - sizes[c] * 0.5f},
                         sizes[c], sizes[c]);
          const size_t start = flat.size();
          for (const SkPoint &pt : q)
            flat.push_back(pt);
          strokeRanges.push_back({start, flat.size()});
          strokeW.push_back(w0ForClass(classify(m)) / w0ForClass(SHU));
        }
      }
      nFootStrokes = (int)strokeRanges.size();
      // chain: every stroke, then a ligature to the next stroke's head
      b.moveTo(flat[0]);
      for (size_t i = 1; i < flat.size(); ++i)
        b.lineTo(flat[i]);
      SkPath chained = b.detach();
      // measure the span table on the CHAINED contour
      SkContourMeasureIter it(chained, false);
      sk_sp<SkContourMeasure> cm = it.next();
      const float total = cm ? cm->length() : 1.0f;
      float d = 0;
      for (size_t s = 0; s < strokeRanges.size(); ++s) {
        const auto [a, bIdx] = strokeRanges[s];
        float run = 0;
        for (size_t i = a; i + 1 < bIdx; ++i)
          run += SkPoint::Distance(flat[i], flat[i + 1]);
        spans.push_back({d, d + run, strokeW[s]});
        d += run;
        if (bIdx < flat.size()) { // the ligature to the next head
          const float lig = SkPoint::Distance(flat[bIdx - 1], flat[bIdx]);
          spans.push_back({d, d + lig, 0.0f});
          d += lig;
        }
      }
      (void)total;
      push(chained, 5.9f, SHU, tFoot, tFoot + (float)nFootStrokes * tFootEach,
           kCinnaDry, "foot", true, std::move(spans));
    }
  }

  Element inkLayer() {
    auto g = box().inset(0).key("ink");
    for (const Stroke &s : strokes)
      g.child(inkStroke(s));
    return g;
  }

  // =========================================================================
  // 虛書 — the graphs written by NOTHING. 《法海遺珠》 DZ 1166 卷六:
  // "with the left eye's light write one talismanic graph; with the right
  // eye's light write one; with the tongue write one; visualise the three
  // graphs as blue, red and white." No brush touches the plate, so they are
  // drawn as light and they fade into the aperture.

  Element voidWriting() {
    auto g = box().inset(0).key("xushu");
    const SkColor4f cols[3] = {kVoidBlue, kVoidRed, kVoidWhite};
    const char *how[3] = {"left eye", "right eye", "tongue"};
    const SkPoint at[3] = {{kCol - 116, 214}, {kCol + 116, 214}, {kCol, 330}};
    const int src[3] = {WU, GANG, LING};
    for (int k = 0; k < 3; ++k) {
      const std::vector<Poly> ms = medians(src[k]);
      SkPathBuilder b;
      for (size_t i = 0; i < ms.size(); ++i) {
        Poly q = place(ms[i], {at[k].fX - 30.0f, at[k].fY - 30.0f}, 60.0f, 60.0f);
        SkPath sp = cloud(smoothPath(q), 2.2f, 22.0f);
        b.addPath(sp);
      }
      auto hump = [](float t) { return std::sin(t * SK_ScalarPI); };
      g.child(box()
                  .left(at[k].fX - 46)
                  .top(at[k].fY - 46)
                  .width(92)
                  .height(92)
                  .outline([p = b.detach().makeOffset(-(at[k].fX - 46),
                                                      -(at[k].fY - 46))](SkSize) {
                    return p;
                  })
                  .fill(Fill::none())
                  .stroke(brushes::taper(3.6f, 1.0f, Fill::color(cols[k])))
                  .foreground(PathFormat{.width = 7.0f,
                                         .strokeFill = Fill::color(
                                             {cols[k].fR, cols[k].fG,
                                              cols[k].fB, 0.20f})})
                  .opacity(bind(&scribe)
                               .window(tVoid + (float)k * 0.16f,
                                       tVoid + tVoidDur + (float)k * 0.16f)
                               .map(hump)
                               .scale(0.92f))
                  .key(fmt("void%d", k)));
      g.child(text(toU8(fmt("%s  \xe2\x80\x94  no mark", how[k])),
                   type(faceMono, 10.0f, {cols[k].fR, cols[k].fG, cols[k].fB,
                                          0.85f}))
                  .left(at[k].fX - 72)
                  .top(at[k].fY + 50)
                  .width(168)
                  .opacity(bind(&scribe)
                               .window(tVoid + (float)k * 0.16f,
                                       tVoid + tVoidDur + (float)k * 0.16f)
                               .map(hump))
                  .key(fmt("voidlbl%d", k)));
    }
    return g;
  }

  // =========================================================================
  // 法印 — the master's seal, square, in relief, vermilion. The seal script
  // deforms a graph to FILL its cell: 五 and 雷 stretched into two halves of
  // a square, cut in relief, with the negative space crosshatched the way a
  // stone impression breaks up.

  Element sealBlock() {
    const float S = 104.0f, x = 474.0f, y = 786.0f;
    auto g = box()
                 .left(x)
                 .top(y)
                 .width(Dim(S))
                 .height(Dim(S))
                 .rotate(-6.0f)
                 .transformOrigin(0.5f, 0.5f)
                 .opacity(bind(&scribe).window(tSeal, tSeal + 0.45f))
                 .scale(bind(&scribe).window(tSeal, tSeal + 0.45f).to(1.5f, 1.0f))
                 .key("seal");
    g.child(box()
                .inset(0)
                .outline(shapes::chamfered(6.0f))
                .fill(Fill::color(hex(0xb52a17, 0.90f)))
                .foreground(lines::crosshatch(Fill::color(hex(0x6d1409, 0.25f)),
                                              5.0f, 0.8f, 18.0f))
                .stroke(PathFormat{.width = 5.0f,
                                   .strokeFill = Fill::color(hex(0xc23520, 1.0f)),
                                   .align = PathFormat::Align::Inner})
                .key("sealground"));
    // 五 above, 雷 below — each STRETCHED to fill its half cell, which is
    // what 篆書 does inside a seal.
    const int half[2] = {WU, LEI};
    for (int k = 0; k < 2; ++k) {
      const std::vector<Poly> ms = medians(half[k]);
      SkPathBuilder b;
      for (const Poly &m : ms) {
        Poly q = place(m, {12.0f, 11.0f + (float)k * 41.0f}, S - 24.0f, 40.0f);
        b.addPath(smoothPath(q));
      }
      g.child(box()
                  .inset(0)
                  .outline([p = b.detach()](SkSize) { return p; })
                  .fill(Fill::none())
                  .stroke(PathFormat{.width = 4.4f,
                                     .strokeFill = Fill::color(hex(0xf2e2cf, 0.95f)),
                                     .cap = SkPaint::kSquare_Cap,
                                     .join = SkPaint::kMiter_Join})
                  .key(fmt("sealglyph%d", k)));
    }
    return g;
  }

  // =========================================================================
  // THE PLATE, ASSEMBLED

  Element plate() {
    auto g = box()
                 .left(kPL)
                 .top(kPT)
                 .width(Dim(kPW))
                 .height(Dim(kPH))
                 .opacity(bind(&scribe).window(tPlate, tPlateEnd))
                 .key("plate");
    g.child(ironGround());

    // the spine every component is strung on — a fu is a COLUMN
    g.child(box()
                .inset(0)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(kCol, 28);
                  b.lineTo(kCol, kPH - 24);
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(PathFormat{.width = 0.8f,
                                   .strokeFill = Fill::color(hex(0x0e0d0c, 0.26f)),
                                   .dashIntervals = {2.0f, 9.0f}})
                .key("spine"));

    // the four registers, ruled faintly in the margin the way a plate is
    // laid out before it is written
    struct Reg {
      float y;
      const char *label;
    };
    const Reg regs[5] = {{34, "FU TOU  head  \xc2\xb7 3 hooks, 3 Pure Ones"},
                         {184, "FU QIAO  aperture \xc2\xb7 one revolution"},
                         {296, "FU SHEN  body \xc2\xb7 cloud-seal, 33 strokes"},
                         {566, "FU DAN  gall \xc2\xb7 GANG, 10 = 10 stems"},
                         {782, "FU JIAO  foot \xc2\xb7 one breath, 38 strokes"}};
    for (int i = 0; i < 5; ++i) {
      g.child(box()
                  .left(18)
                  .top(regs[i].y)
                  .width(Dim(kPW - 36))
                  .height(1)
                  .outline([w = kPW - 36](SkSize) {
                    SkPathBuilder b;
                    b.moveTo(0, 0.5f);
                    b.lineTo(w, 0.5f);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(PathFormat{.width = 0.7f,
                                     .strokeFill = Fill::color(hex(0x0e0d0c, 0.28f)),
                                     .dashIntervals = {1.5f, 6.0f}})
                  .key(fmt("reg%d", i)));
      g.child(text(toU8(regs[i].label), type(faceMono, 8.5f, hex(0x0b0a09, 0.60f)))
                  .left(20)
                  .top(regs[i].y + 3)
                  .width(360)
                  .key(fmt("reglbl%d", i)));
    }

    g.child(inkLayer());
    g.child(voidWriting());
    g.child(sealBlock());
    g.child(ironWash());
    return g;
  }

  // =========================================================================
  // THE TREAD — 步罡踏斗 on the real Dipper.

  Element tread() {
    const float x0 = 1012, y0 = 108, W = 738, H = 738 * 0.5302f;
    // where each station's plate stands relative to it — five step right,
    // two step ABOVE, so the tread's own line stays legible where the
    // handle folds back on itself
    // JIANG BAO (6) hangs 24 lower than its neighbours: at the common 26 its
    // plate top landed inside JUAN WU's caption band and swallowed the first
    // two letters of "roll up fog" against its own lit top edge.
    static const float kOffX[9] = {-56, 48, 48, -56, 48, -58, 50, 54, 52};
    static const float kOffY[9] = {26, 26, 26, 26, 26, -104, 50, 26, 34};
    auto g = box()
                 .left(x0)
                 .top(y0)
                 .width(Dim(W))
                 .height(Dim(H + 130))
                 .key("tread");

    auto S = [&](int i) {
      return SkPoint{kDipper[i].x * W, kDipper[i].y * W};
    };

    // the priest's path between the stations, drawn as LINE ART — the
    // connective tissue is the walk, not a gap. 禹步 「三步九跡」.
    SkPathBuilder walk;
    walk.moveTo(S(0));
    for (int i = 1; i < 7; ++i)
      walk.lineTo(S(i));
    walk.lineTo(S(7));
    walk.lineTo(S(8));
    const SkPath walkPath = walk.detach();

    g.child(box()
                .inset(0)
                .outline([walkPath](SkSize) { return walkPath; })
                .fill(Fill::none())
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 2.6f,
                                       .fill = Fill::color(hex(0x8b6f36, 0.50f))},
                                       {.offset = 0.0f,
                                        .width = 1.0f,
                                        .fill = Fill::color(hex(0xd8bd7c, 0.75f)),
                                        .dash = {2.0f, 7.0f}}}))
                .foreground(brushes::ScatterBrush{.art = footPrint,
                                                  .spacing = 46.0f,
                                                  .alignToPath = true,
                                                  .reach = 18.0f})
                .opacity(bind(&scribe).window(tStars - 0.4f, tStars + 0.5f))
                .key("walkpath"));

    // the nine stations
    for (int i = 0; i < 9; ++i) {
      const SkPoint p = S(i);
      const bool invisible = i == 8;
      const float t = tStars + (float)i * 0.16f;
      auto st = box()
                    .left(p.fX - 13)
                    .top(p.fY - 13)
                    .width(26)
                    .height(26)
                    .opacity(bind(&scribe).window(t, t + 0.45f))
                    .key(fmt("star%d", i));
      st.child(box()
                   .inset(0)
                   .outline(shapes::star(6, 0.30f))
                   .fill(invisible ? Fill::none() : Fill::color(hex(0xe4c98a, 0.92f)))
                   .stroke(PathFormat{
                       .width = 1.0f,
                       .strokeFill = Fill::color(hex(0xe4c98a, invisible ? 0.7f : 0.4f)),
                       .dashIntervals = invisible ? std::vector<SkScalar>{2.0f, 2.6f}
                                                  : std::vector<SkScalar>{}}));
      g.child(std::move(st));
      // 天璇 is the one station the walk reaches from straight overhead — the
      // Dubhe-Merak leg is 8 px of run over 169 of rise — so its ritual name,
      // hung 44 px left of the star, had the tread coming down through the X
      // of "XUAN". It moves to the star's right instead, where that leg has
      // already stopped. Every other ritual name clears its own segments.
      static const float kRitualDodge[9] = {0, 54, 0, 0, 0, 0, 0, 0, 0};
      g.child(text(toU8(fmt("%d %s", i + 1, kDipper[i].ritual)),
                   type(faceMono, 9.5f, hex(0xa48c5c, 0.9f)))
                  .left(p.fX - 44 + kRitualDodge[i])
                  .top(p.fY - 34)
                  .width(140)
                  .opacity(bind(&scribe).window(t, t + 0.45f))
                  .key(fmt("starlbl%d", i)));
      // THE BAYER NAME DODGES THE WALK. It hangs 12 px under its own star,
      // which is fine for the five stations the tread leaves sideways and
      // wrong for the three it leaves downward: the walk ran straight through
      // "Dubhe", "Mizar" and "Alcor" at their x-height. A plate would set the
      // name clear of the line, not on it, so each name carries its own dodge
      // — left at 天樞 and 左輔, right at 開陽, where the segment falls the
      // other way. The other six are 0 and stay under their star.
      // 左輔's dodge is 30 and not more: the margin column's right edge is at
      // x = 1000 and "Alcor" is only 38 px wide, so a bigger left dodge walks
      // it into the SIX CLASSES rule.
      static const float kNameDodge[9] = {-34, 0, 0, 0, 0, 30, 0, -30, 0};
      g.child(text(toU8(kDipper[i].name), type(faceItalic, 9.0f, hex(0x6f6047, 0.85f)))
                  .left(p.fX - 22 + kNameDodge[i])
                  .top(p.fY + 12)
                  .width(140)
                  .opacity(bind(&scribe).window(t, t + 0.45f))
                  .key(fmt("starnm%d", i)));
    }

    // the eleven other plates. Nine stand on stations; TWO bleed off the
    // canvas — the tread does not stop at the frame.
    // the tread does not stop at the sheet: one plate walks off the right
    // edge and one off the top. Neither is labelled — a caption cut mid-word
    // reads as an accident, a plate cut mid-edge reads as a frame.
    const SkPoint bleed[2] = {{W + 150, H * 0.16f}, {W * 0.52f, -150.0f}};
    for (int i = 0; i < 11; ++i) {
      SkPoint at = i < 9 ? S(i) : bleed[i - 9];
      if (i < 9) {
        at.fX += kOffX[i];
        at.fY += kOffY[i];
      }
      const float pw = 52.0f, ph = pw * 5.0f / 3.0f;
      const float t = tStars + (float)i * 0.15f + 0.2f;
      auto mp = box()
                    .left(at.fX - pw * 0.5f)
                    .top(at.fY)
                    .width(Dim(pw))
                    .height(Dim(ph))
                    .rotate(((i * 37) % 11 - 5) * 0.62f)
                    .transformOrigin(0.5f, 0.0f)
                    .opacity(bind(&scribe).window(t, t + 0.5f))
                    .key(fmt("mini%d", i));
      mp.child(box()
                   .inset(0)
                   .outline([](SkSize s) {
                     return ops::sketchy(14.0f, 1.4f, 7)(shapes::chamfered(5.0f)(s));
                   })
                   .fill(Material::linearUnit(
                       {0.18f, 0.0f}, {0.88f, 1.0f},
                       {{0.0f, hex(0x4a443b)}, {0.55f, hex(0x272522)},
                        {1.0f, hex(0x161514)}}))
                   .stroke(PathFormat{.width = 1.2f,
                                      .strokeFill = Fill::color(hex(0x5b5449, 0.85f)),
                                      .align = PathFormat::Align::Inner}));
      // a miniature fu: the same grammar, four marks, generated
      SkPathBuilder b;
      const int srcs[4] = {YU, WU, LEI, YUN};
      const std::vector<Poly> ms = medians(srcs[i % 4]);
      const int take = 3 + (i % 3);
      // The rows must FIT the plate, and a fixed 17 px pitch did not: the
      // fourth row hung 6 px past the bottom edge and the FIFTH landed
      // entirely below it, on the plate's own caption — which is why QI YU,
      // QI XUE and QI QING (the three plates where i % 3 == 2) each carried a
      // red mark struck through their gloss. Pitch the band between the head
      // hook and the foot tick instead, so any `take` is contained.
      const float rowTop = 24.0f, rowBot = ph - 19.0f;
      const float rowH = std::min(16.0f, (rowBot - rowTop) / (float)take);
      const float pitch =
          take > 1 ? (rowBot - rowTop - rowH) / (float)(take - 1) : 0.0f;
      for (int k = 0; k < take && k < (int)ms.size(); ++k) {
        Poly q = place(ms[(size_t)((k * 5 + i) % ms.size())],
                       {9.0f, rowTop + (float)k * pitch}, pw - 18.0f, rowH);
        b.addPath(cloud(smoothPath(q), 1.8f, 13.0f));
      }
      // the head: one hook. the foot: one tick.
      b.moveTo(14, 12);
      b.quadTo(pw * 0.6f, 10, pw - 16, 20);
      b.moveTo(pw * 0.30f, ph - 16);
      b.quadTo(pw * 0.5f, ph - 8, pw * 0.72f, ph - 18);
      mp.child(box()
                   .inset(0)
                   .outline([p = b.detach()](SkSize) { return p; })
                   .fill(Fill::none())
                   .stroke(brushes::taper(3.6f, 1.4f, Fill::color(kCinnaWet))));
      if (i < 9) {
        mp.child(text(toU8(kOthers[i].pinyin),
                      type(faceMono, 8.0f, hex(0xa89264, 0.95f)))
                     .left(-16)
                     .top(ph + 10)
                     .width(124));
        mp.child(text(toU8(kOthers[i].gloss),
                      type(faceItalic, 8.0f, hex(0x776953, 0.9f)))
                     .left(-16)
                     .top(ph + 22)
                     .width(124));
      }
      g.child(std::move(mp));
    }
    return g;
  }

  // =========================================================================
  // MARGINALIA

  Element chantPanel() {
    auto g = box().left(718).top(796).width(474).key("chant");
    g.child(text(toU8("ZHAN HONG FU \xc2\xb7 SEVER THE RAINBOW"),
                 type(faceDisplay, 17.0f, kGold, 1.4f))
                .left(0)
                .top(0)
                .width(468));
    g.child(box()
                .left(0)
                .top(24)
                .width(468)
                .height(3)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(0, 1.5f);
                  b.lineTo(468, 1.5f);
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 1.6f,
                                       .fill = Fill::color(hex(0xb2914f, 0.55f))},
                                      {.offset = 4.0f,
                                       .width = 0.7f,
                                       .fill = Fill::color(hex(0xb2914f, 0.30f)),
                                       .dash = {1.4f, 4.6f}}})));
    const char *lines_[6] = {
        "SHANG DI YOU LING      The High Emperor has commanded:",
        "YAO NI MIE XING        demon-rainbow, annihilate its form.",
        "FENG DAO CUN ZHAN      The wind-blade cuts it inch by inch.",
        "QING KE LIU XING       In an instant, a shooting star.",
        "JI JI RU LU LING       Swiftly, swiftly, per the statutes.",
        "",
    };
    for (int i = 0; i < 5; ++i) {
      const float t = tGall + (float)i * 0.7f;
      g.child(text(toU8(lines_[i]), type(faceMono, 11.5f, kChalk))
                  .left(0)
                  .top(36 + (float)i * 19)
                  .width(468)
                  .opacity(bind(&scribe).window(t, t + 0.4f).to(0.22f, 1.0f))
                  .key(fmt("chant%d", i)));
    }
    g.child(text(toU8("\xe2\x80\x9c\xe6\x80\xa5\xe6\x80\xa5\xe5\xa6\x82\xe5\xbe"
                      "\x8b\xe4\xbb\xa4\xe2\x80\x9d is the Han imperial-document "
                      "closing formula, borrowed whole. It ends"),
                 type(faceItalic, 10.5f, hex(0x7d6f52)))
                .left(0)
                .top(136)
                .width(468));
    g.child(text(toU8("almost every fu, and it goes at the FOOT."),
                 type(faceItalic, 10.5f, hex(0x7d6f52)))
                .left(0)
                .top(150)
                .width(468));
    return g;
  }

  console::Style logStyle() {
    console::Style s;
    s.text = type(faceMono, 9.6f, hex(0x9a8a68));
    s.palette = {type(faceMono, 9.6f, hex(0x6d6249)),  // 0 dim
                 type(faceMono, 9.6f, kGold),          // 1 heading
                 type(faceMono, 9.6f, hex(0x5fae7f)),  // 2 PASS
                 type(faceMono, 9.6f, hex(0xcf6a4a)),  // 3 number
                 type(faceMono, 9.6f, hex(0xc4483a))}; // 4 FAIL
    s.gap = 1.0f;
    s.visibleLines = 13;
    return s;
  }

  Element consolePanel() {
    return console::panel({.rings = {&logA, &logB, &logC},
                           .style = logStyle(),
                           .column = true,
                           .paddingX = 11,
                           .paddingY = 8,
                           .gap = 7,
                           .fill = Fill::color(hex(0x131215, 0.88f)),
                           .border = Fill::color(hex(0xb2914f, 0.20f)),
                           .divider = Fill::color(hex(0xb2914f, 0.14f))})
        .rect(SkRect::MakeXYWH(1228, 768, 638, 420))
        .key("console");
  }

  Element tempoPanel() {
    auto g = box().left(718).top(972).width(474).key("tempo");
    g.child(text(toU8("YI QI LI DUAN \xc2\xb7 CUT OFF IN ONE BREATH"),
                 type(faceDisplay, 13.0f, kGold, 1.2f))
                .left(0)
                .top(0)
                .width(468));
    const char *rows[5] = {
        "FU TOU   head        3 str   1.000 s/stroke   tap fu tou",
        "FU QIAO  aperture    1 rev   1.150 s          ends not meeting",
        "FU SHEN  body       33 str   0.240 s/stroke   cloud-seal",
        "FU DAN   gall       10 str   0.360 s/stroke   chant lands on 10",
        "FU JIAO  foot       38 str   0.034 s/stroke   NO LIFT, FLYING WHITE",
    };
    for (int i = 0; i < 5; ++i)
      g.child(text(toU8(rows[i]),
                   type(faceMono, 10.5f, i == 4 ? hex(0xcf6a4a) : hex(0x9a8a68)))
                  .left(0)
                  .top(20 + (float)i * 15)
                  .width(468));
    g.child(text(toU8("the foot is 7.1x the body \xe2\x80\x94 doctrine, measured: "
                      "\"the foot is the last"),
                 type(faceItalic, 10.5f, hex(0x7d6f52)))
                .left(0)
                .top(100)
                .width(468));
    g.child(text(toU8("step; total concentration, cut off in a single breath, "
                      "no slowing"),
                 type(faceItalic, 10.5f, hex(0x7d6f52)))
                .left(0)
                .top(114)
                .width(468));
    g.child(text(toU8("or dragging.\"  A fu written at one tempo is not a fu."),
                 type(faceItalic, 10.5f, hex(0x7d6f52)))
                .left(0)
                .top(128)
                .width(468));
    return g;
  }

  /** The chant that is sung WHILE the stroke goes down — head and gall. The
   *  六句 of 罡 must finish exactly as the tenth stroke lands, so each line
   *  is windowed on the same Output as the strokes. */
  /** The margin column: the chants that are said WHILE the stroke goes down,
   *  and the two things that decide how every stroke on this plate is drawn —
   *  the width law, plotted, and the six recovered classes as specimens. */
  Element marginColumn() {
    const float X = 718, Wc = 282;
    auto g = box().left(X).top(0).width(Dim(Wc)).key("margin");

    auto rule = [&](float y, float w) {
      return box()
          .left(0)
          .top(y)
          .width(Dim(w))
          .height(3)
          .outline([w](SkSize) {
            SkPathBuilder b;
            b.moveTo(0, 1.5f);
            b.lineTo(w, 1.5f);
            return b.detach();
          })
          .fill(Fill::none())
          .stroke(lines::rails({{.offset = 0.0f,
                                 .width = 1.3f,
                                 .fill = Fill::color(hex(0xb2914f, 0.48f))},
                                {.offset = 3.4f,
                                 .width = 0.6f,
                                 .fill = Fill::color(hex(0xb2914f, 0.26f)),
                                 .dash = {1.3f, 4.2f}}}));
    };

    // --- 踏符頭: one chant line per hook, as the hook goes down -----------
    g.child(text(toU8("TA FU TOU \xc2\xb7 TREADING THE HEAD"),
                 type(faceDisplay, 11.5f, kGold, 1.1f))
                .left(0)
                .top(126)
                .width(Dim(Wc)));
    g.child(rule(144, Wc));
    for (int k = 0; k < 3; ++k) {
      const float t = tHead + (float)k * (tHeadEach + tHeadGap);
      g.child(text(toU8(kHeadChant[k]), type(faceItalic, 11.0f, kChalk))
                  .left(0)
                  .top(154 + (float)k * 26)
                  .width(Dim(Wc))
                  .opacity(bind(&scribe).window(t, t + 0.3f).to(0.14f, 0.98f))
                  .key(fmt("hc%d", k)));
    }

    // --- the width law, PLOTTED. 起 · 行 · 收 as one curve ---------------
    const float py = 260, ph = 126, pw = Wc;
    g.child(text(toU8("QI / XING / SHOU \xc2\xb7 w(s) OVER ARC LENGTH"),
                 type(faceDisplay, 11.5f, kGold, 1.1f))
                .left(0)
                .top(py - 22)
                .width(Dim(Wc)));
    g.child(rule(py - 5, Wc));
    // STRIP 1 — the band the law actually paints, by the same Ribbon that
    // paints the plate. This is the specimen, not an illustration of one.
    {
      const float bh = 46.0f;
      SkPathBuilder axis;
      axis.moveTo(2, bh * 0.5f);
      axis.lineTo(pw - 2, bh * 0.5f);
      g.child(box()
                  .left(0)
                  .top(py + 6)
                  .width(Dim(pw))
                  .height(Dim(bh))
                  .outline([p = axis.detach()](SkSize) { return p; })
                  .fill(Fill::none())
                  .stroke(brushes::Ribbon{
                      .fill = Fill::color(hex(0xcf3018, 0.92f)),
                      .step = 1.5f,
                      .widthFn = [](const PathSample &s) {
                        return 21.0f * widthLaw(s.fraction);
                      },
                      .widthMax = 42.0f})
                  .key("lawband"));
      // STRIP 2 — w(s) plotted from a baseline, with the 1.0 reference
      const float cy = py + 6 + bh + 12, chh = ph - bh - 18;
      const float sc = chh / 2.0f;
      g.child(box()
                  .left(0)
                  .top(cy)
                  .width(Dim(pw))
                  .height(Dim(chh))
                  .outline([pw, chh, sc](SkSize) {
                    SkPathBuilder b;
                    b.moveTo(0, chh - sc);
                    b.lineTo(pw, chh - sc);
                    b.moveTo(0, chh);
                    b.lineTo(pw, chh);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(PathFormat{.width = 0.7f,
                                     .strokeFill = Fill::color(hex(0x8b7f66, 0.5f)),
                                     .dashIntervals = {1.6f, 4.4f}})
                  .key("lawaxis"));
      g.child(box()
                  .left(0)
                  .top(cy)
                  .width(Dim(pw))
                  .height(Dim(chh))
                  // shapes::parametric returns UNIT coordinates (+-1
                  // spans the box), so with the baseline at the box's
                  // bottom and the 1.0 reference at its middle the plot is
                  // exactly v = 1 - w(s). Returning pixels here drew a
                  // 40 000 px diagonal across the whole sheet.
                  .outline(shapes::parametric(
                      [](float t) { return SkPoint{2.0f * t - 1.0f,
                                                   1.0f - widthLaw(t)}; },
                      0.0f, 1.0f, 180))
                  .fill(Fill::none())
                  .stroke(lines::rails(
                      {{.offset = 0.0f,
                        .width = 1.5f,
                        .fill = Fill::color(hex(0xe6d7ae, 0.95f))},
                       {.offset = 3.0f,
                        .width = 0.6f,
                        .fill = Fill::color(hex(0xcf3018, 0.55f))}}))
                  .key("lawcurve"));
      const char *marks[3] = {"ni feng 1.77", "belly 0.73", "dun 1.42"};
      const float mx[3] = {0.0f, 0.28f, 0.88f};
      const float off[3] = {2, -18, -52};
      for (int i = 0; i < 3; ++i)
        g.child(text(toU8(marks[i]), type(faceMono, 8.5f, hex(0xa89778)))
                    .left(mx[i] * pw + off[i])
                    .top(cy + chh - widthLaw(mx[i]) * sc + (i == 1 ? 4 : -13))
                    .width(120)
                    .key(fmt("lawmk%d", i)));
      g.child(text(toU8("s = distance / fullLength, NOT PathSample::fraction"),
                   type(faceMono, 8.5f, hex(0x6f6047)))
                  .left(0)
                  .top(cy + chh + 4)
                  .width(Dim(pw)));
    }

    // --- the six recovered classes, as specimens -------------------------
    // 412, and the chant below moved to 646. The grid is three rows at a 62
    // pitch with each caption hung at cell + 56, so the last caption's
    // BASELINE landed on 636 — which was the chant heading's own top. DIAN and
    // TURN's captions and "SUNG WHILE GANG IS DRAWN" printed 3 px apart with
    // the section rule below both, so the specimen key read as the first line
    // of the next section. The grid cannot compress (SHU's specimen starts at
    // cell y = 2 and the caption above already sits over that), so the fix is
    // 8 px off the top of the block and 10 px onto the heading below it.
    const float ky = 412;
    g.child(text(toU8("SIX CLASSES \xc2\xb7 w0, RECOVERED"),
                 type(faceDisplay, 11.5f, kGold, 1.1f))
                .left(0)
                .top(ky)
                .width(Dim(Wc)));
    g.child(rule(ky + 18, Wc));
    // Each specimen runs in its OWN class's direction, at the class's own
    // w₀, so the key reads as the taxonomy and not as six copies of one
    // curve. Two columns of three; the cell is a 120 × 54 em window.
    static const Poly kSpec[CLSN] = {
        {{4, 30}, {60, 22}, {118, 28}},                // HENG
        {{58, 2}, {64, 26}, {57, 52}},                 // SHU
        {{112, 4}, {74, 22}, {6, 52}},                 // PIE
        {{6, 4}, {50, 22}, {118, 52}},                 // NA
        {{42, 8}, {58, 22}, {72, 38}},                 // DIAN
        {{6, 8}, {96, 5}, {106, 16}, {100, 52}},       // TURN
    };
    for (int c = 0; c < CLSN; ++c) {
      const float cx = (c % 2 == 0) ? 0.0f : 148.0f;
      const float y = ky + 28 + (float)(c / 2) * 62;
      const float w0 = w0ForClass(c) * 128.0f;
      g.child(box()
                  .left(cx + 4)
                  .top(y)
                  .width(126)
                  .height(56)
                  .outline([p = smoothPath(kSpec[c])](SkSize) { return p; })
                  .fill(Fill::none())
                  .stroke(brushes::Ribbon{
                      .fill = Fill::color(kCinnabar),
                      .step = 1.2f,
                      .widthFn = [w0](const PathSample &s) {
                        return w0 * widthLaw(s.fraction);
                      },
                      .widthMax = w0 * 2.0f})
                  .key(fmt("spec%d", c)));
      g.child(text(toU8(fmt("%s  %.3f em", kClsName[c], (double)w0ForClass(c))),
                   type(faceMono, 9.0f, hex(0xa48c5c)))
                  .left(cx)
                  .top(y + 56)
                  .width(140)
                  .key(fmt("speclbl%d", c)));
    }

    // --- the six phrases sung while 罡 is drawn ---------------------------
    const float gy = 646; // see the note above ky
    g.child(text(toU8("SUNG WHILE GANG IS DRAWN"),
                 type(faceDisplay, 11.5f, kGold, 1.1f))
                .left(0)
                .top(gy)
                .width(Dim(Wc)));
    g.child(rule(gy + 18, Wc));
    for (int k = 0; k < 6; ++k) {
      // the chant must finish exactly as the tenth stroke lands
      const float t = tGall + (float)k * (10.0f * tGallEach / 6.0f);
      g.child(text(toU8(fmt("%d/6  %s", k + 1, kGallChant[k])),
                   type(faceItalic, 11.0f, k == 5 ? hex(0xe07a52) : kChalk))
                  .left(0)
                  .top(gy + 28 + (float)k * 18)
                  .width(Dim(Wc))
                  .opacity(bind(&scribe).window(t, t + 0.28f).to(0.14f, 0.98f))
                  .key(fmt("gc%d", k)));
    }
    g.child(text(toU8("the sixth phrase lands on the tenth stroke"),
                 type(faceMono, 9.0f, hex(0x6f6047)))
                .left(0)
                .top(gy + 132)
                .width(Dim(Wc)));
    return g;
  }

  Element furniture() {
    auto g = box().inset(0).key("furn");
    // title block
    g.child(text(toU8("WU LEI HAO LING \xc2\xb7 A THUNDER-RITE COMMAND TALISMAN, "
                      "WRITTEN"),
                 type(faceDisplay, 22.0f, kChalk, 2.6f))
                .left(76)
                .top(34)
                .width(1400));
    g.child(text(toU8("DAOFA HUIYUAN DZ 1220, juan 46 \xc2\xb7 iron plate, five "
                      "cun by three \xc2\xb7 written in cinnabar \xc2\xb7 stroke "
                      "medians from makemeahanzi, classes recovered from "
                      "geometry"),
                 type(faceMono, 10.5f, kGoldDim))
                .left(76)
                .top(62)
                .width(1500));
    // registration marks at the four corners of the sheet
    for (int i = 0; i < 4; ++i) {
      const float rx = (i & 1) ? kW - 46 : 46;
      const float ry = (i & 2) ? kH - 46 : 46;
      g.child(box()
                  .left(rx - 11)
                  .top(ry - 11)
                  .width(22)
                  .height(22)
                  .outline([](SkSize s) {
                    SkPathBuilder b;
                    b.moveTo(s.width() * 0.5f, 0);
                    b.lineTo(s.width() * 0.5f, s.height());
                    b.moveTo(0, s.height() * 0.5f);
                    b.lineTo(s.width(), s.height() * 0.5f);
                    b.addCircle(s.width() * 0.5f, s.height() * 0.5f,
                                s.width() * 0.30f);
                    return b.detach();
                  })
                  .fill(Fill::none())
                  .stroke(PathFormat{.width = 0.8f,
                                     .strokeFill = Fill::color(hex(0xb2914f, 0.42f))})
                  .key(fmt("reg%d", i + 10)));
    }
    // tick ladder down the plate's left margin — cun and fen
    g.child(box()
                .left(kPL - 26)
                .top(kPT)
                .width(20)
                .height(Dim(kPH))
                .outline([](SkSize s) {
                  SkPathBuilder b;
                  for (int i = 0; i <= 50; ++i) {
                    const float y = s.height() * (float)i / 50.0f;
                    const float len = (i % 10 == 0) ? 17.0f : (i % 5 == 0 ? 10.0f : 5.0f);
                    b.moveTo(s.width(), y);
                    b.lineTo(s.width() - len, y);
                  }
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(PathFormat{.width = 0.9f,
                                   .strokeFill = Fill::color(hex(0xb2914f, 0.40f))})
                .key("ladder"));
    for (int i = 0; i <= 5; ++i)
      g.child(text(toU8(fmt("%d", i)), type(faceMono, 8.5f, hex(0x8b7644)))
                  .left(kPL - 46)
                  .top(kPT + kPH * (float)i / 5.0f - 5)
                  .width(16)
                  .key(fmt("ladlbl%d", i)));
    g.child(text(toU8("CUN"), type(faceMono, 8.0f, hex(0x8b7644)))
                .left(kPL - 52)
                .top(kPT + kPH + 8)
                .width(40));
    // colophon
    g.child(text(toU8("BU GANG TA DOU \xc2\xb7 THE TREAD, ON THE REAL DIPPER"),
                 type(faceDisplay, 12.0f, kGold, 1.1f))
                .left(1046)
                .top(706)
                .width(830));
    g.child(box()
                .left(1046)
                .top(724)
                .width(830)
                .height(3)
                .outline([](SkSize) {
                  SkPathBuilder b;
                  b.moveTo(0, 1.5f);
                  b.lineTo(830, 1.5f);
                  return b.detach();
                })
                .fill(Fill::none())
                .stroke(lines::rails({{.offset = 0.0f,
                                       .width = 1.3f,
                                       .fill = Fill::color(hex(0xb2914f, 0.48f))},
                                      {.offset = 3.4f,
                                       .width = 0.6f,
                                       .fill = Fill::color(hex(0xb2914f, 0.26f)),
                                       .dash = {1.3f, 4.2f}}})));
    g.child(text(toU8("Nine stations: J2000 right ascension and declination, "
                      "gnomonically projected about the asterism's own "
                      "centroid. Yu bu is \"three steps, nine prints\"."),
                 type(faceItalic, 10.0f, hex(0x8d7f60)))
                .left(1046)
                .top(734)
                .width(830));
    g.child(text(toU8("Zuo Fu (Alcor) lies 0.008 of the asterism's span from "
                      "Kai Yang (Mizar) on the real sky \xe2\x80\x94 every bu "
                      "gang plate separates the pair by hand, and so does this "
                      "one. You Bi is invisible: its station is doctrine, and "
                      "it is drawn open."),
                 type(faceItalic, 10.0f, hex(0x6d6047)))
                .left(1046)
                .top(752)
                .width(830));
    g.child(text(toU8("Never invert the brush and tap for a pregnant woman or "
                      "a patient with eye disease. \xc2\xb7 SigilCompose study "
                      "\xc2\xb7 no CJK font is loaded: every Han glyph here is "
                      "stroke geometry"),
                 type(faceMono, 9.5f, hex(0x5d5341)))
                .left(76)
                .top(kH - 34)
                .width(1600));
    return g;
  }

  // =========================================================================

  Element describe(sketch::SketchContext &) {
    auto root = box().inset(0);
    root.child(plate());
    root.child(tread());
    root.child(marginColumn());
    root.child(chantPanel());
    root.child(tempoPanel());
    root.child(consolePanel());
    root.child(furniture());
    return root;
  }

  // =========================================================================
  // THE CHECKS

  void runChecks() {
    // --- panel A: the stroke data -----------------------------------------
    logA.append(toU8("makemeahanzi graphics.txt \xe2\x80\x94 MEDIANS, 1024 em, "
                     "DRAWING ORDER"),
                1);
    int totalPts = 0, totalStrokes = 0;
    for (const Glyph &g : kGlyphs) {
      const int16_t *p = g.data;
      for (int s = 0; s < g.strokes; ++s) {
        totalPts += *p;
        p += 1 + 2 * (*p);
      }
      totalStrokes += g.strokes;
    }
    logA.append(toU8(fmt("  11 characters, %d strokes, %d median points, "
                         "verbatim",
                         totalStrokes, totalPts)),
                0);
    logA.append(toU8(fmt("  GANG U+7F61 has %d strokes", kGlyphs[GANG].strokes)),
                kGlyphs[GANG].strokes == 10 ? 2 : 4);
    logA.append(toU8("  doctrine: 10 strokes = the ten Heavenly Stems"), 0);
    logA.append(toU8("  JIA YI BING DING WU JI GENG XIN REN GUI \xe2\x80\x94 "
                     "the count MATCHES"),
                kGlyphs[GANG].strokes == 10 ? 2 : 4);
    logA.append(toU8(fmt("  foot JI+JI+RU+LU+LING = %d+%d+%d+%d+%d = %d strokes",
                         kGlyphs[JI].strokes, kGlyphs[JI].strokes,
                         kGlyphs[RU].strokes, kGlyphs[LV].strokes,
                         kGlyphs[LING].strokes, nFootStrokes)),
                nFootStrokes == 38 ? 2 : 4);
    logA.append(toU8(fmt("    drawn as ONE contour: %d spans = %d strokes + "
                         "%d ligatures",
                         2 * nFootStrokes - 1, nFootStrokes, nFootStrokes - 1)),
                2);
    logA.append(toU8(fmt("  body YU+WU / YUN / GUI = %d cloud-seal strokes", nBody)),
                nBody == 33 ? 2 : 4);
    logA.append(toU8(fmt("  ink strokes on the plate: %d nodes",
                         (int)strokes.size())),
                3);

    // --- panel B: the taxonomy --------------------------------------------
    logB.append(toU8("KanjiVG kvg:type \xe2\x80\x94 THE STROKE CLASS, FOR w0"), 1);
    logB.append(toU8("  kanji/07f61.svg (GANG) is a 14-byte 404. NOT A KANJI."), 4);
    logB.append(toU8("  recovered instead from the median geometry: length,"), 0);
    logB.append(toU8("  body chord angle over 14-86%, peak turn from entry"), 0);
    logB.append(toU8(fmt("  vs KanjiVG over the %d strokes it does carry: %d/%d",
                         kvgTotal, kvgAgree, kvgTotal)),
                kvgAgree * 10 >= kvgTotal * 8 ? 2 : 4);
    for (size_t i = 0; i < kvgMiss.size() && i < 3; ++i)
      logB.append(toU8("    " + kvgMiss[i]), 0);
    logB.append(toU8("  GUI U+9B3C: mmah 9, KanjiVG 10 \xe2\x80\x94 the Japanese"), 0);
    logB.append(toU8("    form carries an extra PIE; and the two sources order"), 0);
    logB.append(toU8("    TIAN's interior in LEI U+96F7 opposite ways"), 3);
    {
      std::string row = "  GANG recovered: ";
      for (int i = 0; i < 10; ++i) {
        std::string n = kClsName[gangCls[(size_t)i]];
        while (!n.empty() && n.back() == ' ')
          n.pop_back();
        row += n.substr(0, 2) + (i < 9 ? " " : "");
      }
      logB.append(toU8(row), 3);
    }

    // --- panel C: the brush ------------------------------------------------
    logC.append(toU8("THE BRUSH \xc2\xb7 QI / XING / SHOU, one law over arc "
                     "length"),
                1);
    logC.append(toU8("  w(s)/w0 = 1.15e^-12s + 0.62 + 0.28s + 0.55e^-90(s-.88)^2"),
                0);
    logC.append(toU8(fmt("    ni feng (reverse entry) w(0)    = %.2f w0",
                         (double)widthLaw(0.0f))),
                3);
    {
      float minS = 0, minW = 1e9f;
      for (int i = 0; i <= 100; ++i) {
        const float s = (float)i / 100.0f, w = widthLaw(s);
        if (w < minW) {
          minW = w;
          minS = s;
        }
      }
      logC.append(toU8(fmt("    zhong feng belly  min %.2f w0 at s = %.2f",
                           (double)minW, (double)minS)),
                  3);
    }
    logC.append(toU8(fmt("    dun (the press)   w(0.88) = %.2f w0",
                         (double)widthLaw(0.88f))),
                3);
    logC.append(toU8(fmt("    shou (the cut)    w(1)    = %.2f w0",
                         (double)widthLaw(1.0f))),
                3);
    logC.append(toU8("  brushes::Ribbon::widthFn takes exactly this law."), 2);
    logC.append(toU8("  BUT under trim() a decoration is handed the REVEALED"), 0);
    logC.append(toU8("  contour: PathSample::fraction is a fraction of what is"), 0);
    logC.append(toU8("  drawn SO FAR, so key the law to distance/fullLength or"), 0);
    logC.append(toU8("  the dun press SLIDES down the stroke as it writes."), 2);
    logC.append(toU8(fmt("  tempo: foot %.3f s/stroke vs body %.3f = %.1fx",
                         (double)tFootEach, (double)tBodyEach,
                         (double)(tBodyEach / tFootEach))),
                3);
  }

  void validateClassifier() {
    kvgAgree = kvgTotal = 0;
    kvgMiss.clear();
    for (const KvgRow &row : kKvg) {
      const std::vector<Poly> ms = medians(row.glyph);
      if ((int)ms.size() != row.n) // GUI: 9 vs 10 — reported, not compared
        continue;
      for (size_t i = 0; i < ms.size(); ++i) {
        const int got = classify(ms[i]);
        ++kvgTotal;
        if (got == row.cls[i])
          ++kvgAgree;
        else
          kvgMiss.push_back(fmt("%s #%d  kvg %s  geo %s", kGlyphs[row.glyph].id,
                                (int)i + 1, kClsName[row.cls[i]], kClsName[got]));
      }
    }
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kNight);

    auto family = [&](const char *name, SkFontStyle st) -> sk_sp<SkTypeface> {
      if (!ctx.fonts || !ctx.fonts->fontManager())
        return nullptr;
      return ctx.fonts->fontManager()->matchFamilyStyle(name, st);
    };
    faceSerif = family("Hoefler Text", SkFontStyle::Normal());
    faceItalic = family("Hoefler Text", SkFontStyle::Italic());
    faceMono = family("Menlo", SkFontStyle::Normal());
    faceDisplay = family("Optima", SkFontStyle::Bold());
    faceSmall = faceMono;
    if (!faceSerif)
      faceSerif = family("Baskerville", SkFontStyle::Normal());
    if (!faceItalic)
      faceItalic = faceSerif;
    if (!faceMono)
      faceMono = family("Courier New", SkFontStyle::Normal());
    if (!faceDisplay)
      faceDisplay = faceSerif;

    ironGrain = patterns::grain(2.2f, 4, 1356.0f, 0.55f, 2.6f);
    ironSpeck = patterns::speckle(420, 26, 0.7f, 2.6f,
                                  {hex(0x7c7263, 0.10f), hex(0x000000, 0.16f)});
    ironSpeck.seed(1220);

    // ScatterBrush / PatternBrush art: held as MEMBERS. Built inside a
    // describe they would re-bake their tile every frame — the snapshot
    // cache lives in the value.
    footPrint = box()
                    .width(7)
                    .height(11)
                    .outline(shapes::squircle(2.6f))
                    .fill(Fill::color(hex(0xb2914f, 0.42f)));
    hammerTile = box().width(22).height(3).fill(Fill::none());
    hammerCorner = box()
                       .width(19)
                       .height(19)
                       .outline([](SkSize s) {
                         SkPathBuilder b;
                         const float w = s.width(), h = s.height();
                         b.moveTo(0, h * 0.5f);
                         b.lineTo(w * 0.42f, h * 0.16f);
                         b.lineTo(w, h * 0.44f);
                         b.lineTo(w * 0.58f, h * 0.88f);
                         b.close();
                         return b.detach();
                       })
                       .fill(Fill::color(hex(0x6b6355, 0.55f)))
                       .stroke(PathFormat{
                           .width = 0.9f,
                           .strokeFill = Fill::color(hex(0x100f0f, 0.7f))});

    validateClassifier();
    buildStrokes();
    runChecks();

    ctx.ticker.add([this](double dt) {
      clockT += dt;
      scribe = (float)std::fmod(clockT, (double)kLoop);
      return true;
    });

    ctx.composer.render(describe(ctx));
  }

  void update(double, sketch::SketchContext &) override {}
};

SIGIL_SKETCH(ThunderFulu)

// ---------------------------------------------------------------------------
// WHAT IT COST, AND WHAT DID NOT PAY. 1900 × 1220, `--bench`, worst moment
// (t = 20.6, the whole plate written and the foot flying): p50 8.57 ms /
// p99 9.45 ms. At t = 3.0 it is 4.37 / 4.87.
//
// THE SHAPE THAT MADE IT CHEAP. 86 marks in 49 nodes (the foot's 38 strokes
// are one node and one contour), every one carrying a Ribbon with a widthFn
// and every one bound to the SAME Output through
// `window(t0, t1)`, and the frame is still under 10 ms — because a stroke
// that has not started trims to nothing and a stroke that is finished trims
// to a constant, so almost every node is a plain cached fill on almost every
// frame. There is no re-describe anywhere in the score: `update()` is empty
// and the whole 27 s performance runs off one `ch::Output<float>` stepped in
// the ticker. That is also why each node is sized to its OWN bounds and not
// to the plate — 49 full-plate boxes would have made the culling and the
// layer composites the cost instead of the ink.
//
// Two things measured that did NOT pay, both worth the next study's time:
//   1. `bakeScale(0.5f)` on the 624 × 1040 grain wash made it WORSE, 2.9 ms
//      → 5.0 ms and the frame 8.6 → 10.9. The upscale resample on composite
//      costs more than the half-area bake saves; a full-canvas soft wash is
//      already the cheapest thing a texture bake can hold.
//   2. removing the plate group's BOUND OPACITY — a 624 × 1040 saveLayer
//      every frame — bought 0.1 ms, not the several-millisecond win the same
//      change bought another study. The layer is only expensive when what it
//      wraps is expensive; here its whole subtree is already two texture
//      bakes and a pile of tiny cached fills, so the layer is nearly free.
//      The plate's arrival is part of the score, so the binding stayed.
// The one node that does cost is the grain wash at 2.9 ms — an
// opacity + kSoftLight leaf the library refuses to promote (a bake would
// round twice) and which therefore has to be asked for by name.
