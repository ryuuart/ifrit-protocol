// Palette-indexed pixel art: the character grid read into marks, what the
// bake puts on an image at one cell size and at another, the index bake a
// shader reads, and the sheet that packs many sprites under their names.

#include <sigilcompose/kit/Sprites.h>

#include "support/ShapeTestSupport.h"

namespace {

using sigil::compose::kit::pixelMap;
using sigil::compose::kit::Sprite;
using sigil::compose::kit::SpriteKey;
using sigil::compose::kit::SpriteSheet;
using sigil::compose::kit::SpriteStyle;

const SkColor4f kBlank{0, 0, 0, 0};
const SkColor4f kRed{1, 0, 0, 1};
const SkColor4f kBlue{0, 0, 1, 1};
const std::vector<SkColor4f> kColours{kBlank, kRed, kBlue};
constexpr std::string_view kChars = " ab";

/** The three-by-three the bake cases read, with a hole in it: the blank is
 *  a palette entry like any other and its alpha is what makes it a hole. */
std::vector<std::string> threeByThree() { return {"ab ", " ab", "b a"}; }

SkColor pixelOf(const sk_sp<SkImage>& image, int x, int y) {
  SkBitmap probe;
  probe.allocPixels(SkImageInfo::MakeN32Premul(1, 1));
  if (!image || !image->readPixels(nullptr, probe.pixmap(), x, y))
    return SK_ColorTRANSPARENT;
  return probe.getColor(0, 0);
}

}  // namespace

TEST(KitSprites, ACharacterGridBakesItsPaletteExactlyAtEveryCellSize) {
  const std::optional<Sprite> sprite =
      pixelMap(threeByThree(), {kChars, kColours});
  ASSERT_TRUE(sprite.has_value());
  EXPECT_EQ(sprite->grid.width(), 3);
  EXPECT_EQ(sprite->grid.height(), 3);

  // The expected colour of each grid cell, read off the rows above.
  const SkColor kWant[3][3] = {
      {SK_ColorRED, SK_ColorBLUE, SK_ColorTRANSPARENT},
      {SK_ColorTRANSPARENT, SK_ColorRED, SK_ColorBLUE},
      {SK_ColorBLUE, SK_ColorTRANSPARENT, SK_ColorRED},
  };

  // At cell 1 the image IS the grid: one pixel per character, the palette
  // entry itself and no other colour.
  const sk_sp<SkImage> one = spriteImage(*sprite);
  ASSERT_TRUE(one);
  EXPECT_EQ(one->width(), 3);
  EXPECT_EQ(one->height(), 3);
  for (int y = 0; y < 3; ++y)
    for (int x = 0; x < 3; ++x) EXPECT_EQ(pixelOf(one, x, y), kWant[y][x]);

  // At cell 4 every character is a four-by-four block of exactly the same
  // colour — magnified, never resampled, so no edge pixel is a blend of
  // two entries.
  const sk_sp<SkImage> four = spriteImage(*sprite, {.cell = 4.0f});
  ASSERT_TRUE(four);
  EXPECT_EQ(four->width(), 12);
  EXPECT_EQ(four->height(), 12);
  for (int y = 0; y < 12; ++y)
    for (int x = 0; x < 12; ++x)
      EXPECT_EQ(pixelOf(four, x, y), kWant[y / 4][x / 4])
          << "at " << x << "," << y;
}

TEST(KitSprites, AGridRefusesRatherThanGuessWhatItCannotRead) {
  // A character the key does not carry. Guessing it — as a blank, say —
  // is a hole in the picture that looks exactly like an authored one.
  const std::vector<std::string> stray{"ab ", " az", "b a"};
  EXPECT_FALSE(pixelMap(stray, {kChars, kColours}));
  // A ragged grid: a row that is short is a typo, not a shape.
  const std::vector<std::string> ragged{"ab ", " a", "b a"};
  EXPECT_FALSE(pixelMap(ragged, {kChars, kColours}));
  // A key whose two sides disagree cannot name its own entries.
  EXPECT_FALSE(pixelMap(threeByThree(), {" ab!", kColours}));
  // …and the grid that CAN be read is read.
  EXPECT_TRUE(pixelMap(threeByThree(), {kChars, kColours}));
}

TEST(KitSprites, NeighbouringCellsOfOneEntryBecomeOneMark) {
  const std::vector<std::string> field{"aaaa", "aaaa", "aaaa"};
  const std::optional<Sprite> flat = pixelMap(field, {kChars, kColours});
  ASSERT_TRUE(flat.has_value());
  ASSERT_EQ(flat->runs.size(), 1u);  // one rectangle, not twelve
  EXPECT_FLOAT_EQ(flat->runs[0].w, 4.0f);
  EXPECT_FLOAT_EQ(flat->runs[0].h, 3.0f);

  // A blank field is no marks at all, and still carries its grid: a
  // sprite occupies the cell it was authored on whether or not it fills it.
  const std::vector<std::string> blank{"   ", "   "};
  const std::optional<Sprite> empty = pixelMap(blank, {kChars, kColours});
  ASSERT_TRUE(empty.has_value());
  EXPECT_TRUE(empty->empty());
  EXPECT_EQ(empty->grid.height(), 2);
}

TEST(KitSprites, TheIndexBakeCarriesIndicesAndKeepsEntryZeroAHole) {
  const std::optional<Sprite> sprite =
      pixelMap(threeByThree(), {kChars, kColours});
  ASSERT_TRUE(sprite.has_value());
  const sk_sp<SkImage> indices = indexImage(*sprite);
  ASSERT_TRUE(indices);
  // The VALUE, not the colour: entry 1 is red in the palette and reads as
  // 1 here, which is what lets a shader do index arithmetic on it.
  EXPECT_EQ(SkColorGetR(pixelOf(indices, 0, 0)), 1u);
  EXPECT_EQ(SkColorGetR(pixelOf(indices, 1, 0)), 2u);
  EXPECT_EQ(SkColorGetA(pixelOf(indices, 0, 0)), 255u);
  // Entry 0 is the hole an 8-bit sheet reserves, so "no sprite here" is
  // readable from the alpha alone.
  EXPECT_EQ(SkColorGetA(pixelOf(indices, 2, 0)), 0u);
}

TEST(KitSprites, MarksKeepTheirOrderSoAnOverlapCompositesAsAuthored) {
  // What a character grid cannot state: a translucent mark over an opaque
  // one. The runs are a list rather than a grid exactly so this holds.
  Sprite sprite;
  sprite.grid = {2, 1};
  sprite.rect(0, 0, 2, 1, kRed);
  sprite.px(1, 0, SkColor4f{0, 0, 1, 0.5f});
  const sk_sp<SkImage> image = spriteImage(sprite);
  ASSERT_TRUE(image);
  EXPECT_EQ(pixelOf(image, 0, 0), SK_ColorRED);
  const SkColor mixed = pixelOf(image, 1, 0);
  EXPECT_GT(SkColorGetR(mixed), 100u);
  EXPECT_GT(SkColorGetB(mixed), 100u);
  // The alpha prop multiplies each mark's OWN alpha, so the half-covered
  // pixel fades further than the opaque one under the same fade.
  const sk_sp<SkImage> faded = spriteImage(sprite, {.alpha = 0.5f});
  EXPECT_EQ(SkColorGetA(pixelOf(faded, 0, 0)), 128u);
  EXPECT_LT(SkColorGetB(pixelOf(faded, 1, 0)), SkColorGetB(mixed));
}

TEST(KitSprites, TheNodeFormPaintsWhatTheBakedFormDoes) {
  const std::optional<Sprite> sprite =
      pixelMap(threeByThree(), {kChars, kColours});
  ASSERT_TRUE(sprite.has_value());
  Host host;
  host.composer.render(box().absolute().inset(0).child(
      sigil::compose::kit::pixelSprite(*sprite, {.cell = 10.0f})));
  host.frame();
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);     // a
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLUE);   // b
  EXPECT_EQ(host.pixel(25, 5), SK_ColorBLACK);  // the hole, over the ground
  EXPECT_EQ(host.pixel(15, 15), SK_ColorRED);
}

TEST(KitSprites, ASheetPacksEverySpriteUnderItsNameWithoutOverlap) {
  SpriteSheet sheet;
  const std::vector<std::string> flat{"aa", "aa"};
  for (int i = 0; i < 16; ++i)
    sheet.add("icon" + std::to_string(i),
              *pixelMap(flat, {kChars, i % 2 ? kColours
                                             : std::vector<SkColor4f>{
                                                   kBlank, kBlue, kRed}}));
  ASSERT_EQ(sheet.size(), 16u);
  ASSERT_TRUE(sheet.bake({.cell = 3.0f}));
  ASSERT_TRUE(sheet.image());

  const SkRect bounds = SkRect::MakeWH((float)sheet.image()->width(),
                                       (float)sheet.image()->height());
  std::vector<SkRect> cells;
  for (std::string_view name : sheet.names()) {
    const SkRect cell = sheet.rect(name);
    EXPECT_FALSE(cell.isEmpty()) << name;
    EXPECT_TRUE(bounds.contains(cell)) << name;
    EXPECT_FLOAT_EQ(cell.width(), 6.0f);  // a two-pixel sprite at cell 3
    for (const SkRect& other : cells)
      EXPECT_FALSE(SkRect::Intersects(cell, other));
    cells.push_back(cell);
  }
  // Each name's rectangle holds that name's own drawing, so a caller can
  // trust the window it is handed.
  EXPECT_EQ(pixelOf(sheet.image(), (int)sheet.rect("icon1").centerX(),
                    (int)sheet.rect("icon1").centerY()),
            SK_ColorRED);
  EXPECT_EQ(pixelOf(sheet.image(), (int)sheet.rect("icon2").centerX(),
                    (int)sheet.rect("icon2").centerY()),
            SK_ColorBLUE);
}

TEST(KitSprites, ASheetAnswersByNameAndGivesNothingForOneItDoesNotHold) {
  SpriteSheet sheet;
  const std::vector<std::string> small{"ab", "ba"};
  const std::vector<std::string> larger{"aaa", "aaa", "aaa"};
  sheet.add("folder", *pixelMap(small, {kChars, kColours}));
  ASSERT_TRUE(sheet.find("folder"));
  EXPECT_EQ(sheet.find("folder")->grid.width(), 2);
  EXPECT_FALSE(sheet.find("trash"));
  EXPECT_TRUE(sheet.rect("trash").isEmpty());
  // Before a bake there is no sheet and therefore no rectangle — an
  // answer of "somewhere" would be worse than none.
  EXPECT_FALSE(sheet.image());
  EXPECT_TRUE(sheet.rect("folder").isEmpty());
  // Adding replaces rather than duplicates, and drops the stale bake.
  ASSERT_TRUE(sheet.bake());
  sheet.add("folder", *pixelMap(larger, {kChars, kColours}));
  EXPECT_EQ(sheet.size(), 1u);
  EXPECT_EQ(sheet.find("folder")->grid.width(), 3);
  EXPECT_FALSE(sheet.image());
}
