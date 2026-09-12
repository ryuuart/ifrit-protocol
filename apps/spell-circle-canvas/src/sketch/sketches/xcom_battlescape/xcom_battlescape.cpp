// An X-COM battlescape composed from palette-indexed sprites and tactical
// controls.

// TAGS: Interfaces/Game

#include "Sprites.h"

struct XcomBattlescape : sketch::Sketch {
  using Atlas = instancing::Atlas;
  using Pool = instancing::Pool;

  // ---- the flyweight -------------------------------------------------------
  std::shared_ptr<Atlas> tiles;       // 128x160 cells, (type x shade) + markers
  std::shared_ptr<Atlas> fontAtlas;   // ONE 4x4 white cell, tinted per instance
  std::shared_ptr<Pool> terrain;      // z0 floors + objects, z1 tree tops
  std::shared_ptr<Pool> overlay;      // path arrows + the box selector
  std::shared_ptr<Pool> mapGlyphs;    // fifteen bordered TU markers
  std::shared_ptr<Pool> panelGlyphs;  // stats, ammo, layer, spotted-unit tags

  int cellFloor[3][2][9]{};  // [type][dither variant][shade]
  int cellObj[4][9]{};       // [kNone unused | kBush | kTree | kHullWall]
  int cellHullDeck[9]{};
  int cellArrow[2][3]{};
  int cellCursor = 0;
  int cellFontPx = 0;
  int atlasCells = 0;

  // ---- scenario ------------------------------------------------------------
  std::array<xcom::TileData, (size_t)xcom::kMapSize * xcom::kMapSize> map{};
  std::vector<SkIPoint> path;
  std::vector<int> pathTU;
  std::vector<int> pathBlock;

  // ---- panel data (soldiers.rul ranges; maxima recovered from the capture) --
  static constexpr int kMaxTU = 60, kMaxEnergy = 65, kMaxHealth = 36,
                       kMaxMorale = 100;
  static constexpr int kFiring = 58;     // chosen inside 40-70
  static constexpr int kReserveTU = 15;  // floor(60 * 25/100) — a Snap Shot

  // ---- pixel type ----------------------------------------------------------
  xcom::PixelText nameText, popupRow[3], popupAcc[3], popupTU[3];

  // ---- motion --------------------------------------------------------------
  int animFrame = 0;
  int tagIndex = 32;  // blinkVisibleUnitButtons walks 32..44
  int tagDir = 1;
  sigil::motion::Ticker::FixedStatus fixedStatus{};

  struct Phase {
    int frame = 0;
    int tag = 32;
    bool longPath = false;
    bool popup = false;
    bool fired = false;
    int tags = 3;
    bool operator==(const Phase&) const = default;
  } phase, lastPhase{-1};

  Pattern metalPattern, latticePattern;
  /** The palette shader and its 256-entry table, resolved once per
   *  declaration and held HERE. A function-local static inside a sketch's
   *  dylib outlives the reload that replaced the code around it. */
  sk_sp<SkRuntimeEffect> paletteFx;
  Paint paletteTable;
  bool audited = false;

  /** THE VERIFICATION, as one table. Every row's verdict is COMPUTED from
   *  the two values it reports, so a row that reads PASS cannot disagree
   *  with the measurement beside it, and `failures()` is what puts the
   *  warning on the screen. */
  measure::Table verdict;

  // =========================================================================
  // BAKE

  void bakeAtlas() {
    using namespace xcom;
    paletteFx = paletteEffect();
    paletteTable = sigil::material::skia::paletteLookup(palette());
    tiles = std::make_shared<Atlas>(1.0f);
    // Atlas::filter, and it is the palette's guard rail.
    //
    // At oversample 1.0 with every stamp on an integer pixel, kLinear and
    // kNearest agree exactly: the linear sample lands on the texel centre and
    // the filter is a no-op. So on this frame, as framed, the setting changes
    // nothing.
    //
    // Shift every stamp by HALF A PIXEL and kLinear blends adjacent texels,
    // which invents colours between palette entries — thousands of them,
    // across a large fraction of the frame — while kNearest still emits only
    // table entries. The setting is therefore worth nothing on a static
    // integer-aligned map and worth the entire reconstruction the moment a
    // camera pans by a fraction or a stamp is scaled.
    tiles->filter(SkFilterMode::kNearest);
    const SkSize cell{kCellW, kCellH};

    // THE ART, AUTHORED ONCE EACH, IN INDICES. Thirteen 128 x 160 index
    // rasters — three floors at two dither variants, bush, tree, hull wall,
    // hull deck, two arrow directions and the cursor. Nothing about a shade
    // or a marker colour is committed here.
    sk_sp<SkImage> idxFloor[3][2];
    for (int f = 0; f < 3; ++f)
      for (int v = 0; v < 2; ++v) {
        const Floor kind = (Floor)f;
        idxFloor[f][v] = indexCell(
            [kind, v](kit::Sprite& ink) { paintFloor(ink, kind, v); });
      }
    const sk_sp<SkImage> idxBush = indexCell(paintBush);
    const sk_sp<SkImage> idxTree = indexCell(paintTree);
    const sk_sp<SkImage> idxWall = indexCell(paintHullWall);
    const sk_sp<SkImage> idxDeck = indexCell(paintHullDeck);
    sk_sp<SkImage> idxArrow[2];
    for (int d = 0; d < 2; ++d)
      idxArrow[d] = indexCell([d](kit::Sprite& ink) { paintArrow(ink, d); });
    const sk_sp<SkImage> idxCursor = indexCell(paintCursor);

    // THE CELLS, DERIVED. Ninety-seven sheet frames, every one of them the
    // same handful of drawings read through the table at a different shade
    // or a different marker block. The count is the POOL's, not the paint
    // model's: an instance carries a position, a rotation, a scale, a tint
    // and a FRAME, so a per-instance shade has nowhere to ride but the frame
    // index, and the sheet has to hold what the frame selects.
    for (int shade = 0; shade <= 8; ++shade) {
      for (int f = 0; f < 3; ++f)
        for (int v = 0; v < 2; ++v)
          cellFloor[f][v][shade] =
              tiles->cell(box().fill(paletteLut(paletteFx, paletteTable,
                                                idxFloor[f][v], shade)),
                          cell);
      cellObj[kBush][shade] = tiles->cell(
          box().fill(paletteLut(paletteFx, paletteTable, idxBush, shade)),
          cell);
      cellObj[kTree][shade] = tiles->cell(
          box().fill(paletteLut(paletteFx, paletteTable, idxTree, shade)),
          cell);
      cellObj[kHullWall][shade] = tiles->cell(
          box().fill(paletteLut(paletteFx, paletteTable, idxWall, shade)),
          cell);
      cellHullDeck[shade] = tiles->cell(
          box().fill(paletteLut(paletteFx, paletteTable, idxDeck, shade)),
          cell);
    }
    const int kBlocks[3] = {4, 10, 3};  // Pathfinding green / yellow / red
    for (int d = 0; d < 2; ++d)
      for (int m = 0; m < 3; ++m)
        cellArrow[d][m] =
            tiles->cell(box().fill(paletteLut(paletteFx, paletteTable,
                                              idxArrow[d], 0, kBlocks[m])),
                        cell);
    cellCursor = tiles->cell(
        box().fill(paletteLut(paletteFx, paletteTable, idxCursor, 0)), cell);
    atlasCells = tiles->frameCount();

    fontAtlas = std::make_shared<Atlas>(1.0f);
    fontAtlas->filter(SkFilterMode::kNearest);
    // The font cell is a MASK, not a colour: Pool::tints() MULTIPLIES, so the
    // cell has to be pure white. Filling it with the palette's own white —
    // PAL[1] #FCFCFC, the obvious choice — scales every tinted glyph by
    // 252/255, which lands each one two units below its palette entry: a
    // handful of off-palette colours that are invisible to the eye and caught
    // only by the colour census.
    cellFontPx = fontAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {PX, PX});
  }

  // ---- the font pool: a 4x4 white cell, tinted -----------------------------
  /** value at (x, y) in ORIGINAL px, colour = _color + 1 (NumberText::draw ends
   *  with offset(_color)). `bordered` adds the black surround the map markers
   *  carry. Returns the advance, 4 px per digit, no kerning, ever. */
  int pushNumber(Pool& pool, int value, float x, float y, int colorIdx,
                 bool bordered) {
    const int lit = colorIdx + 1;
    std::string digits = std::to_string(std::max(0, value));
    if (bordered) {
      for (size_t d = 0; d < digits.size(); ++d) {
        const int g = digits[d] - '0';
        for (int r = -1; r <= 5; ++r)
          for (int c = -1; c <= 3; ++c) {
            bool nearLit = false, self = false;
            for (int dr = -1; dr <= 1 && !nearLit; ++dr)
              for (int dc = -1; dc <= 1; ++dc) {
                const int rr = r + dr, cc = c + dc;
                if (rr < 0 || rr > 4 || cc < 0 || cc > 2) continue;
                if ((unsigned)xcom::kDigit[g][rr] & (4u >> (unsigned)cc)) {
                  if (dr == 0 && dc == 0)
                    self = true;
                  else
                    nearLit = true;
                }
              }
            if (nearLit && !self)
              pool.add({(x + (float)d * 4 + (float)c) * PX + PX * 0.5f,
                        (y + (float)r) * PX + PX * 0.5f},
                       cellFontPx, 0.0f, 1.0f, xcom::C(xcom::blk(0, 15)));
          }
      }
    }
    for (size_t d = 0; d < digits.size(); ++d) {
      const int g = digits[d] - '0';
      for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 3; ++c)
          if ((unsigned)xcom::kDigit[g][r] & (4u >> (unsigned)c))
            pool.add({(x + (float)d * 4 + (float)c) * PX + PX * 0.5f,
                      (y + (float)r) * PX + PX * 0.5f},
                     cellFontPx, 0.0f, 1.0f, xcom::C(lit));
    }
    return (int)digits.size() * 4;
  }

  // =========================================================================
  // POOLS

  int terrainZ0 = 0, terrainZ1 = 0;

  void buildTerrain() {
    using namespace xcom;
    terrain->clear();
    terrainZ0 = terrainZ1 = 0;
    const auto visible = [](SkPoint tl) {
      return tl.fX + kCellW > 0 && tl.fX < kMapViewW && tl.fY + kCellH > 0 &&
             tl.fY < kMapViewH;
    };
    // Map::drawTerrain's own nesting: for z, for x, for y. The pool draws in
    // INSERTION order and that order IS the depth sort — do not sort.
    for (int z = 0; z <= 1; ++z)
      for (int mx = 0; mx < kMapSize; ++mx)
        for (int my = 0; my < kMapSize; ++my) {
          const TileData& t = map[(size_t)my * (size_t)kMapSize + (size_t)mx];
          if (!t.seen)
            continue;  // undiscovered tiles are not drawn; PAL[15] shows
          const SkPoint tl = mapToScreen(mx, my, z);
          if (!visible(tl)) continue;
          const int sh = tileShade(mx, my);
          const SkPoint centre = cellCentre(mx, my, z);
          if (z == 0) {
            terrain->add(centre,
                         cellFloor[(int)t.floor][(unsigned)(mx + my) & 1u][sh]);
            ++terrainZ0;
            if (t.object != kNone) {
              terrain->add(centre, cellObj[(int)t.object][sh]);
              ++terrainZ0;
            }
          } else if (t.floor == kHull) {
            terrain->add(centre, cellHullDeck[sh]);
            ++terrainZ1;
          }
        }
    terrain->commit();
  }

  /** previewPath, Pathfinding.cpp:972. Walk the path in order, subtract the
   *  cost, colour by the documented rule. Cardinal steps cost 4 (the .MCD floor
   *  cost is original data; 4 is the engine's own fallback and what open ground
   *  reads as). The marker is a 1-BASED palette block fed to ColorReplace. */
  void computePath(bool longer) {
    using namespace xcom;
    path = pathTiles();
    if (!longer)
      path.resize(11);  // the cursor jumps three tiles further out at t = 3.2 s
    pathTU.clear();
    pathBlock.clear();
    int tus = 58;  // the soldier's current TU, and the number in the recess
    for (size_t i = 0; i < path.size(); ++i) {
      // 4 TU for ordinary open ground (Pathfinding's own fallback value; the
      // real per-terrain costs are .MCD data and are in no repo), plus
      // `wallcost` where the route crosses a fallen tree line every third tile.
      // The costs are a scenario; the COLOURING RULE below is documented.
      tus -= 4 + ((i >= 4 && i % 3 == 1) ? 6 : 0);
      const int marker = tus < 0 ? 3 : (tus >= kReserveTU ? 4 : 10);
      pathTU.push_back(std::max(0, tus));
      pathBlock.push_back(marker);
    }
  }

  void buildOverlay() {
    using namespace xcom;
    overlay->clear();
    for (size_t i = 0; i < path.size(); ++i) {
      const SkIPoint prev =
          i == 0 ? SkIPoint{kSoldierA.mx, kSoldierA.my} : path[i - 1];
      const int dir = path[i].x() != prev.x() ? 1 : 0;
      const int m = pathBlock[i] == 4 ? 0 : (pathBlock[i] == 10 ? 1 : 2);
      overlay->add(cellCentre(path[i].x(), path[i].y(), 0), cellArrow[dir][m]);
    }
    if (!path.empty())
      overlay->add(cellCentre(path.back().x(), path.back().y(), 0), cellCursor);
    overlay->commit();
  }

  void buildMapGlyphs() {
    using namespace xcom;
    mapGlyphs->clear();
    for (size_t i = 0; i < path.size(); ++i) {
      const SkPoint tl = mapToScreen(path[i].x(), path[i].y(), 0);
      const int v = pathTU[i];
      // Map.cpp:944 — x = screenPosition.x + 16 - (tuMarker > 9 ? 5 : 3),
      //               y = screenPosition.y + 29.
      const float ox = tl.fX / PX + 16.0f - (v > 9 ? 5.0f : 3.0f);
      const float oy = tl.fY / PX + 29.0f;
      // UNBOXED. The border is a lit pixel's eight neighbours in palette
      // black, and around a 3x5 glyph that is a 5x7 dark block — the
      // numeral then reads as a filled tile rather than as the tiny
      // NumberText run the reference draws on the ground.
      pushNumber(*mapGlyphs, v, ox, oy, blk(pathBlock[i] - 1, 0), false);
    }
    mapGlyphs->commit();
  }

  void buildPanelGlyphs(const Phase& p) {
    using namespace xcom;
    panelGlyphs->clear();
    const int tu = p.fired ? 43 : 58;
    pushNumber(*panelGlyphs, tu, 136, 186, 64, false);    // numTUs
    pushNumber(*panelGlyphs, 56, 154, 186, 16, false);    // numEnergy
    pushNumber(*panelGlyphs, 36, 136, 194, 32, false);    // numHealth
    pushNumber(*panelGlyphs, 100, 154, 194, 192, false);  // numMorale
    // numLayers declares 15 in interfaces.rul, and offset(_color) would put the
    // lit pixel at 16 — amber. The capture's little `1` is BLACK, so it is
    // drawn here at 14 (lit 15). Noted rather than silently reconciled.
    pushNumber(*panelGlyphs, 1, 232, 150, 14, false);  // numLayers
    pushNumber(*panelGlyphs, p.fired ? 13 : 14, 280, 148, 2, false);  // ammo R
    for (int i = 0; i < p.tags; ++i)
      pushNumber(*panelGlyphs, i + 1, 306 + (i == 9 ? -2 : 0), 132 - 13 * i, 16,
                 false);  // visibleUnits: 16
    panelGlyphs->commit();
  }

  // =========================================================================
  // DESCRIBE

  Element describe(sketch::SketchContext& ctx) {
    using namespace xcom;
    Element root = box().width(kCanvasW).height(kCanvasH).fill(C(blk(0, 15)));

    // ---- z1 the terrain, one atlas stamp ----------------------------------
    root.child(at(0, 0, 320, 144)
                   .key("map")
                   .child(instancing::instances(tiles, terrain)));

    // ---- z2 units and objects: REAL elements, so hitTest can reach them ----
    for (const auto& [u, alien, key] :
         {std::tuple{kSoldierA, false, "unit-a"},
          std::tuple{kSoldierB, false, "unit-b"},
          std::tuple{Soldier{kAlienX, kAlienY}, true, "unit-alien"}}) {
      const SkPoint tl = mapToScreen(u.mx, u.my, 0);
      const int sh = tileShade(u.mx, u.my);
      root.child(
          box()
              .left(tl.fX)
              .top(tl.fY)
              .width(kCellW)
              .height(kCellH)
              .key(key)
              .child(custom(kit::formatted("unit s%d %s", sh,
                                           alien ? "alien" : "soldier"),
                            [sh, alien](SkCanvas& c, const PaintContext&) {
                              paintUnit(c, sh, alien);
                            })));
    }
    {
      const SkPoint tl = mapToScreen(kSoldierA.mx, kSoldierA.my, 0);
      const int frame = phase.frame;
      root.child(box()
                     .left(tl.fX)
                     .top(tl.fY - n(4))
                     .width(kCellW)
                     .height(kCellH)
                     .child(custom(kit::formatted("bob arrow f%d", frame),
                                   [frame](SkCanvas& c, const PaintContext&) {
                                     paintBobArrow(c, frame);
                                   })));
    }

    // ---- z3/z4 path arrows, TU numbers, the box selector -------------------
    root.child(at(0, 0, 320, 144).child(instancing::instances(tiles, overlay)));
    root.child(
        at(0, 0, 320, 144).child(instancing::instances(fontAtlas, mapGlyphs)));

    // ---- z5 the control panel ---------------------------------------------
    root.child(panel(ctx));

    // ---- z6 spotted-enemy tags. visibleUnits pos [300,-16] out of
    //      interfaces.rul, hence the y-16 and the upward stack.
    for (int i = 0; i < phase.tags; ++i) {
      Element tag = box().inset(0);
      tag.child(at(300, 128 - 13 * i, 15, 12).fill(C(blk(0, 15))));
      tag.child(at(301, 129 - 13 * i, 13, 10).fill(C(phase.tag)));
      root.child(tag);
    }

    // ---- panel numbers, above the panel and above the tags ----------------
    root.child(at(0, 0, 320, 200)
                   .child(instancing::instances(fontAtlas, panelGlyphs)));

    // ---- z7 the fire-mode popup, snapped open, never tweened ---------------
    if (phase.popup) root.child(popupEl());

    if (verdict.failures() > 0) root.child(failureCard());
    return root;
  }

  Element panel(sketch::SketchContext&) {
    using namespace xcom;
    // A full-canvas transparent shell, NOT a box at the panel's rect: every
    // widget below is positioned by its BattlescapeState constructor argument,
    // which is a SCREEN coordinate, and Yoga resolves an absolute child
    // against its parent. Nested inside a box at (0,144) every one of those
    // coordinates would be offset a second time and the panel would leave the
    // bottom of the frame.
    Element p = box().inset(0);
    // The metal body: a dithered block-5 field, no gradient, no direction.
    p.child(at(0, 144, 320, 56).fill(metalPattern.material()));
    p.child(at(0, 144, 320, 1).fill(C(blk(5, 1))));
    p.child(at(0, 199, 320, 1).fill(C(blk(5, 13))));

    // Fourteen 32x16 plates, seven columns, two rows. They read in COLUMNS.
    const float bx[7] = {48, 80, 112, 144, 176, 208, 240};
    for (int col = 0; col < 7; ++col)
      for (int row = 0; row < 2; ++row) {
        const int id = col * 2 + row;
        p.child(at(bx[col], 144 + 16 * row, 32, 16)
                    .key("btn" + std::to_string(id))
                    .child(custom("plate 32x16",
                                  [](SkCanvas& c, const PaintContext&) {
                                    paintPlate(c, 32, 16);
                                  })
                               .inset(0))
                    .child(custom(kit::formatted("button glyph %d", id),
                                  [id](SkCanvas& c, const PaintContext&) {
                                    paintButtonGlyph(c, id);
                                  })
                               .inset(0)));
      }

    // The six reserve buttons. buttonReserveNone declares 67, the other three
    // and buttonZeroTUs declare 35 — block 4 step 3 against block 2 step 3.
    const auto reserveBtn = [&](float x, float y, float w, float h, int idx,
                                const char* key) {
      p.child(at(x, y, w, h).fill(C(idx + 4)));
      p.child(at(x + 1, y + 1, w - 2, h - 2).fill(C(idx)).key(key));
    };
    reserveBtn(49, 177, 10, 23, 35, "zeroTUs");
    reserveBtn(60, 177, 17, 11, 67, "resNone");  // the lit one, per the capture
    reserveBtn(78, 177, 17, 11, 35, "resSnap");
    reserveBtn(60, 189, 17, 11, 35, "resAimed");
    reserveBtn(78, 189, 17, 11, 35, "resAuto");
    reserveBtn(96, 177, 10, 23, 35, "resKneel");
    // Glyph strokes on the reserve buttons — a figure with a muzzle flash.
    for (const auto& [x, y] :
         {std::pair{60.0f, 177.0f}, std::pair{78.0f, 177.0f},
          std::pair{60.0f, 189.0f}, std::pair{78.0f, 189.0f}})
      p.child(at(x + 3, y + 3, 11, 5)
                  .child(custom("reserve glyph",
                                [](SkCanvas& c, const PaintContext&) {
                                  const Ink ink{c};
                                  ink.rect(0, 0, 2, 5, blk(0, 15));
                                  ink.rect(2, 2, 5, 1, blk(0, 15));
                                  ink.rect(8, 1, 1, 3, blk(0, 15));
                                  ink.rect(10, 0, 1, 5, blk(0, 15));
                                })));

    // The rank badge, 26x23 — a gold plate, block 9 over block 10.
    p.child(
        at(107, 177, 26, 23)
            .key("rank")
            .child(custom("rank badge", [](SkCanvas& c, const PaintContext&) {
              const Ink ink{c};
              for (int r = 0; r < 23; ++r)
                ink.row(0, (float)r, 26, blk(9, 2 + r / 6));
              ink.row(0, 0, 26, blk(9, 0));
              ink.row(0, 22, 26, blk(10, 6));
              for (int r = 0; r < 23; ++r) {
                ink.px(0, (float)r, blk(9, 1));
                ink.px(25, (float)r, blk(10, 5));
              }
              // A chevron — STR_SQUADDIE.
              for (int k = 0; k < 7; ++k) {
                ink.row((float)(13 - k - 1), (float)(6 + k), 3, blk(10, 8));
                ink.row((float)(13 + k - 1), (float)(6 + k), 3, blk(10, 8));
              }
              for (int k = 0; k < 7; ++k) {
                ink.row((float)(13 - k - 1), (float)(5 + k), 3, blk(9, 0));
                ink.row((float)(13 + k - 1), (float)(5 + k), 3, blk(9, 0));
              }
            })));

    // The stat block sits in a BLACK WELL, not on the metal — measured off the
    // reference, x 132..320, y 175..200. Without it the bars' transparent
    // middle row shows brushed steel and the gauge stops reading as a gauge.
    p.child(at(132, 175, 188, 25).fill(C(blk(0, 15))));
    p.child(at(131, 175, 1, 25).fill(C(blk(5, 12))));

    // The name — textName declares 128, PAL[128] #A8D0F0.
    p.child(pixelTextEl(nameText, n(135), n(176)));

    // Four number recesses: seven-row single-step ramps, one hue each. The TU
    // one uses the GREEN block, not its own yellow-green. Measured, not
    // derived.
    p.child(recess(134, 185, blk(3, 7)));
    p.child(recess(152, 185, blk(1, 5)));
    p.child(recess(134, 193, blk(2, 5)));
    p.child(recess(152, 193, blk(12, 5)));

    // The lattice behind the bars: a 5 px x 2 px pitch, one pitch per axis.
    p.child(at(176, 185, 136, 15).fill(C(blk(0, 15))));
    p.child(at(176, 185, 136, 15).fill(latticePattern.material()));

    // Four bars at 1 px per point. barTUs 64, barEnergy 16, barHealth 32
    // (color2 82), barMorale 192.
    const int tu = phase.fired ? 43 : 58;
    p.child(statBar(170, 185, tu, kMaxTU, 64, "barTU"));
    p.child(statBar(170, 189, 56, kMaxEnergy, 16, "barEnergy"));
    p.child(statBar(170, 193, 36, kMaxHealth, 32, "barHealth"));
    p.child(statBar(170, 197, 100, kMaxMorale, 192, "barMorale"));

    // Two hand wells, 32x48: interior index 15, bevel block 14 232/235.
    for (const auto& [x, right] :
         {std::pair{8.0f, false}, std::pair{280.0f, true}}) {
      const bool holdsRifle = right;
      p.child(
          at(x, 148, 32, 48)
              .key(right ? "handR" : "handL")
              .child(custom(holdsRifle ? "hand well rifle" : "hand well empty",
                            [holdsRifle](SkCanvas& c, const PaintContext&) {
                              const Ink ink{c};
                              for (int r = 0; r < 48; ++r)
                                ink.row(0, (float)r, 32, blk(0, 15));
                              for (int r = 0; r < 48; ++r) {
                                ink.px(0, (float)r, blk(14, 8));
                                ink.px(31, (float)r, blk(14, 11));
                              }
                              ink.row(0, 0, 32, blk(14, 8));
                              ink.row(0, 47, 32, blk(14, 11));
                              if (!holdsRifle) return;
                              // STR_RIFLE, a 32x48 BIGOB reconstruction.
                              ink.rect(14, 5, 4, 26, blk(15, 2));
                              ink.rect(15, 5, 2, 26, blk(15, 0));
                              ink.rect(12, 11, 8, 4, blk(5, 8));
                              ink.rect(13, 12, 6, 2, blk(5, 5));
                              ink.rect(13, 20, 6, 9, blk(2, 6));
                              ink.rect(14, 21, 4, 7, blk(2, 3));
                              ink.rect(11, 30, 10, 4, blk(15, 4));
                              ink.rect(13, 34, 6, 9, blk(5, 9));
                              ink.rect(14, 35, 4, 7, blk(5, 6));
                              ink.rect(12, 43, 8, 2, blk(15, 6));
                            })));
    }
    return p;
  }

  /** ActionMenuState: up to six 272x40 rows stacked UPWARD from
   *  (iconsX + 24, iconsY + 16 - id*40) = (24, 160). Description at (10,13),
   *  accuracy at (140,13), TU at (210,13), FONT_BIG, high contrast, a Frame of
   *  thickness 8 in border colour block 3. It appears instantly; rows do not
   *  stagger, slide or fade. */
  Element popupEl() {
    using namespace xcom;
    Element g = box().inset(0);
    for (int i = 0; i < 3; ++i) {
      const float y = 160.0f - (float)i * 40.0f;
      g.child(at(24, y, 272, 40).fill(C(blk(3, 11))));
      g.child(at(26, y + 2, 268, 36).fill(C(blk(3, 5))));
      g.child(at(30, y + 6, 260, 28).fill(C(blk(3, 13))));
      g.child(pixelTextEl(popupRow[i], n(34), n(y + 13)));
      g.child(pixelTextEl(popupAcc[i], n(164), n(y + 13)));
      g.child(pixelTextEl(popupTU[i], n(234), n(y + 13)));
    }
    return g;
  }

  // =========================================================================
  // AUDIT — the verification protocol, run once

  void runAudit(sketch::SketchContext& ctx) {
    using namespace xcom;
    verdict = {};
    verdict.add(measure::heading("THE SHEET"));
    verdict.add(measure::reading(
        kit::formatted(
            "atlas cells at %.0f\xc3\x97%.0f, sheet 2048\xc3\x97%.0f", kCellW,
            kCellH, (double)(std::ceil((float)atlasCells / 16.0f) * kCellH)),
        (long)atlasCells));
    verdict.add(measure::reading(
        kit::formatted("stamps: terrain %d + overlay %zu + glyphs %zu",
                       terrainZ0 + terrainZ1, overlay->size(),
                       mapGlyphs->size() + panelGlyphs->size()),
        (long)(terrain->size() + overlay->size() + mapGlyphs->size() +
               panelGlyphs->size())));
    verdict.add(measure::heading("THE PROJECTION AND ITS INVERSE"));

    // #1 projection round-trip.
    uint32_t seed = 12345u;
    int fails = 0, boundary = 0;
    for (int i = 0; i < 200; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const float sx = (float)(seed % (uint32_t)kMapViewW);
      seed = seed * 1664525u + 1013904223u;
      const float sy = (float)(seed % (uint32_t)kMapViewH);
      int mx = 0, my = 0;
      screenToMap(sx, sy, 0, &mx, &my);
      if (mx < 0 || my < 0 || mx >= kMapSize || my >= kMapSize) {
        ++boundary;
        continue;
      }
      const SkPoint tl = mapToScreen(mx, my, 0);
      // The inverse answers with the tile whose COLUMN the point is in, which
      // is the 128-wide cell, and the y band is the 32-px screen diagonal.
      if (sx < tl.fX || sx >= tl.fX + kCellW) ++fails;
    }
    verdict.add(measure::check(
        "#1  of 200 screen points, round-trips that missed", 0, fails));
    verdict.add(measure::reading("     clamped at the Clamp(-1, size) boundary",
                                 (long)boundary));

    {  // panel widgets: bounds() -> hitTest() -> the same key, round-trip
      int ok = 0, total = 0;
      for (const char* k : {"map", "btn0", "btn13", "rank", "handR", "handL",
                            "resNone", "barTU", "barMorale", "zeroTUs"}) {
        ++total;
        const auto b = ctx.composer.bounds(k);
        const auto h = b ? ctx.composer.hitTest(b->center()) : std::nullopt;
        if (h && *h == k) ++ok;
      }
      verdict.add(measure::check(
          "#2a panel widgets surviving bounds() \xe2\x86\x92 hitTest()", total,
          ok));
    }
    // #2 hitTest against the same inverse.
    int agree = 0, checked = 0;
    for (const auto& [key, mx, my] :
         {std::tuple{"unit-a", kSoldierA.mx, kSoldierA.my},
          std::tuple{"unit-b", kSoldierB.mx, kSoldierB.my},
          std::tuple{"unit-alien", kAlienX, kAlienY}}) {
      const SkPoint tl = mapToScreen(mx, my, 0);
      const SkPoint probe{tl.fX + kCellW * 0.5f, tl.fY + n(30)};
      const auto hit = ctx.composer.hitTest(probe);
      int qx = 0, qy = 0;
      screenToMap(probe.fX, probe.fY, 0, &qx, &qy);
      ++checked;
      if (hit && *hit == key && qx == mx && qy == my) ++agree;
      verdict.add(measure::check(
          kit::formatted("     hitTest(%s) names it, and the inverse "
                         "answers (%d,%d)",
                         key, mx, my),
          std::string_view(key),
          std::string_view(hit && qx == mx && qy == my ? hit->c_str()
                                                       : "(no)")));
    }
    // Pool tiles are unreachable on purpose — instances() is one custom()
    // leaf, so a hit lands on the pool node rather than on a tile.
    verdict.add(measure::check("#2  keyed units agreeing with the inverse",
                               checked, agree));

    // #3 the light radius is exactly 8. Off by one means floor() where
    // addLight uses Round().
    {
      std::string walk;
      int firstConstant = -1;
      for (int k = 0; k <= 11; ++k) {
        const int shade = tileShade(kSoldierA.mx - k, kSoldierA.my);
        if (firstConstant < 0 && k > 0 &&
            shade == tileShade(kSoldierA.mx - k + 1, kSoldierA.my))
          firstConstant = k - 1;
        walk += (k ? "," : "") + std::to_string(shade);
      }
      verdict.add(
          measure::reading("#3  shade walking \xe2\x88\x92x from "
                           "the selected soldier",
                           walk));
      verdict.add(
          measure::check("     first constant at tile", 8, firstConstant));
    }

    // #4 the bars read back — from the RESOLVED geometry, the same
    // measurement that recovered the maxima off the reference capture:
    // fill length / PX is the value, outline length / PX - 1 is the maximum.
    // bounds() reads the keyed rects statBar actually drew, so a bar drawn at
    // the wrong length changes this output, and the expected TU follows
    // phase.fired — the value the panel is drawing, not a fixed 58.
    {
      const int wantVal[4] = {phase.fired ? 43 : 58, 56, 36, 100};
      const int wantMax[4] = {kMaxTU, kMaxEnergy, kMaxHealth, kMaxMorale};
      const char* nm[4] = {"TU", "Energy", "Health", "Morale"};
      const char* keys[4] = {"barTU", "barEnergy", "barHealth", "barMorale"};
      verdict.add(measure::heading("THE BARS, READ BACK OFF THE DRAWN RECTS"));
      for (int i = 0; i < 4; ++i) {
        const auto fill = ctx.composer.bounds(keys[i]);
        const auto line = ctx.composer.bounds(std::string(keys[i]) + "-max");
        const int value = fill ? (int)std::lround(fill->width() / PX) : -1;
        const int maxv = line ? (int)std::lround(line->width() / PX) - 1 : -1;
        verdict.add(measure::check(
            kit::formatted("#4 %-6s fill %.0f px / %.0f", nm[i],
                           fill ? (double)fill->width() : -1.0, (double)PX),
            wantVal[i], value));
        verdict.add(measure::check(
            kit::formatted("       outline %.0f px / %.0f \xe2\x88\x92 1",
                           line ? (double)line->width() : -1.0, (double)PX),
            wantMax[i], maxv));
      }
    }

    // The selector's ten-pixel fudge, both answers.
    const SkPoint cur = mapToScreen(path.back().x(), path.back().y(), 0);
    const SkPoint probe{cur.fX + kCellW * 0.5f, cur.fY + n(30)};
    int hx = 0, hy = 0, bx = 0, by = 0;
    screenToMap(probe.fX, probe.fY, 0, &hx, &hy);
    screenToMap(probe.fX, probe.fY + n(10), 0, &bx, &by);  // spriteHeight/4
    verdict.add(measure::reading(
        kit::formatted("     selector: honest inverse (%d,%d); the shipped "
                       "+10 px bias",
                       hx, hy),
        kit::formatted("(%d,%d)", bx, by)));
  }

  /** THE FAILING CLAIMS, PAINTED — and only when there are any. The screen
   *  is the artefact and carries no drafting chrome, so a reconstruction
   *  whose projection, queries and bars all read back shows the screen and
   *  nothing else. */
  Element failureCard() const {
    using namespace xcom;
    sketch::kit::Theme look;
    look.palette.ash = C(blk(0, 1));
    look.palette.figure = C(blk(2, 3));
    look.type.captionNote = {n(4.5f), 0.1f};
    look.type.captionLabel = {n(4.5f), 0.1f, true};
    look.spacing.rowGap = n(2);
    std::vector<sketch::kit::Row> rows;
    for (const measure::Check& c : verdict.rows) {
      if (!c.judged() || c.pass) continue;
      rows.push_back(
          {{toUtf8(c.label), toUtf8(c.actual), toUtf8("want " + c.expected)},
           Fill::color(C(blk(2, 3)))});
    }
    sketch::kit::Provide bound(look);
    return box()
        .left(n(12))
        .top(n(18))
        .width(n(296))
        .height(n(12) + n(8) * (float)rows.size())
        .fill(Fill::color(C(blk(2, 12))))
        .foreground(
            stroke(PX, Fill::color(C(blk(2, 3))), PathFormat::Align::Inner))
        .column()
        .padding(n(6))
        .gap(n(4))
        .child(sketch::kit::table(std::move(rows),
                                  {.columns = {{n(150)}, {n(24), true}, {}},
                                   .gap = n(4),
                                   .swatchSide = n(3)}));
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    using namespace xcom;
    // The still is captured inside the 9.6 s state cycle below, at the phase
    // that matches the reference screen: the 14-tile path preview with TU 58
    // intact, which runs [3.2, 4.8). 4.0 s is its midpoint. Later phases open
    // the fire-mode popup, which covers most of the battlescape.
    // The plate at exactly 2x. One 1994 pixel is four canvas px, so eight
    // device px in every column and every row, and an integer downsample of
    // the capture lays it over the reference.
    sketch::kit::stage(ctx, {.size = SkSize::Make(kCanvasW, kCanvasH),
                             .captureAt = 4.0,
                             .background = C(blk(0, 15)),
                             .oversample = 2});

    bakeAtlas();
    terrain = std::make_shared<Pool>();
    overlay = std::make_shared<Pool>();
    mapGlyphs = std::make_shared<Pool>();
    panelGlyphs = std::make_shared<Pool>();

    map = buildMap();
    buildTerrain();

    // The panel's brushed metal: a dithered scatter of block 5 indices 83-94,
    // no gradient, no direction — measured off the capture. Baked once,
    // nearest.
    metalPattern =
        Pattern::tile({n(8), n(8)}, [](SkCanvas& c, SkSize, uint32_t) {
          const Ink ink{c};
          for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
              const uint32_t h = hash3(x, y, 404);
              int step = 6 + (int)(h % 2u);
              if ((h >> 4u) % 9u == 0u)
                step = 4 + (int)((h >> 8u) % 2u);
              else if ((h >> 4u) % 13u == 0u)
                step = 9 + (int)((h >> 8u) % 2u);
              ink.px((float)x, (float)y, blk(5, step));
            }
        });
    metalPattern.sampling(SkSamplingOptions(SkFilterMode::kNearest));
    // patterns::gridLines(spacingX, spacingY, width, colour) — the 5 x 2 pitch,
    // exactly. It takes ONE colour, so the capture's 136/137 verticals and its
    // 138 horizontals collapse to a single palette step here.
    latticePattern = patterns::gridLines(n(5), n(2), PX, toColor(C(137)));
    latticePattern.sampling(SkSamplingOptions(SkFilterMode::kNearest));

    // Type. FONT_BIG substitute at 1x, quantised into block 8 by coverage.
    weave::TextStyle big;
    big.shaping.typeface =
        weave::ports::face({"Arial Narrow"}, SkFontStyle::kBold_Weight);
    big.shaping.fontSize = 12.0f;
    big.shaping.letterSpacing = 0.6f;
    if (ctx.fonts) {
      nameText =
          pixelText(u8"Anders Holmgren", big, *ctx.fonts, blk(8, 0), blk(8, 5));
      // items.rul + BattleUnit::getFiringAccuracy / getActionTUs, recomputed:
      //   firing 58, maxTU 60, rifle two-handed with an empty left hand.
      //   Auto  58*35/100 = 20   TU floor(60*35/100) = 21
      //   Snap  58*60/100 = 34   TU floor(60*25/100) = 15
      //   Aimed 58*110/100 = 63  TU floor(60*80/100) = 48
      const char* names[3] = {"Auto Shot", "Snap Shot", "Aimed Shot"};
      const int accPct[3] = {35, 60, 110};
      const int tuPct[3] = {35, 25, 80};
      for (int i = 0; i < 3; ++i) {
        const int acc = kFiring * accPct[i] / 100;
        const int tus = kMaxTU * tuPct[i] / 100;
        popupRow[i] = pixelText(std::u8string((const char8_t*)names[i]), big,
                                *ctx.fonts, blk(0, 1), blk(0, 5));
        popupAcc[i] = pixelText(
            std::u8string(
                (const char8_t*)("Acc>" + std::to_string(acc) + "%").c_str()),
            big, *ctx.fonts, blk(0, 1), blk(0, 5));
        popupTU[i] = pixelText(
            std::u8string(
                (const char8_t*)("TU>" + std::to_string(tus)).c_str()),
            big, *ctx.fonts, blk(0, 1), blk(0, 5));
      }
    }

    // DEFAULT_ANIM_SPEED = 100 (BattlescapeState.h:103) — a 10 Hz clock. The
    // alpha interpolant is deliberately IGNORED: a 10 Hz UI that interpolates
    // is wrong, and every motion on this screen is an integer sequence.
    ctx.ticker.addFixed(
        10.0,
        [this] {
          animFrame = (int)((unsigned)(animFrame + 1) & 7u);
          // blinkVisibleUnitButtons: +1 up, -2 down, turning at 44 and 32.
          tagIndex += tagDir > 0 ? 1 : -2;
          if (tagIndex >= 44) {
            tagIndex = 44;
            tagDir = -1;
          } else if (tagIndex <= 32) {
            tagIndex = 32;
            tagDir = 1;
          }
          return true;
        },
        8, nullptr, &fixedStatus);

    phase = Phase{animFrame, tagIndex, false, false, false, 3};
    computePath(false);
    buildOverlay();
    buildMapGlyphs();
    buildPanelGlyphs(phase);
    lastPhase = phase;
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The verification runs BEFORE this frame's render(). bounds() and
    // hitTest() read the resolved Yoga layout, which the composer computes
    // inside draw(), so a query issued straight after a render() in the same
    // update sees a dirtied tree and every rect comes back nan. There is no
    // way to ask the composer to lay out without painting, so the queries have
    // to be one frame behind the description they are asking about.
    if (!audited && elapsed > 0.25) {
      audited = true;
      runAudit(ctx);
      // A claim that did not hold has to reach the screen, and the phase
      // below may not change for seconds.
      if (verdict.failures() > 0) ctx.composer.render(describe(ctx));
    }
    // The documented 9.6 s loop, twelve 800 ms cycles. Every transition below
    // is INSTANT — the path recomputes in one frame, the popup snaps open, the
    // TU bar drops 15 points in one frame. Resist every instinct to tween it.
    const double t = std::fmod(elapsed, 9.6);
    Phase p;
    p.frame = animFrame;
    p.tag = tagIndex;
    p.longPath = t >= 3.2;
    p.popup = t >= 4.8 && t < 6.4;
    p.fired = t >= 6.4;
    p.tags = t >= 8.0 ? 4 : 3;
    if (p == lastPhase) return;
    const bool pathChanged = p.longPath != lastPhase.longPath;
    const bool panelChanged =
        p.fired != lastPhase.fired || p.tags != lastPhase.tags;
    phase = p;
    if (pathChanged) {
      computePath(p.longPath);
      buildOverlay();
      buildMapGlyphs();
    }
    if (panelChanged || lastPhase.frame < 0) buildPanelGlyphs(p);
    lastPhase = p;
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    XcomBattlescape, "Study \xc2\xb7 Game UI",
    "X-COM: UFO Defense (1994) at 4\xc3\x97 \xe2\x80\x94 115 colours, all of "
    "them in the palette")
