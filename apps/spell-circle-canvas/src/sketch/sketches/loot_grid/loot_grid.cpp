// A socketed inventory: item data and reusable slots assembled into a grid.

// TAGS: Interfaces/Game

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include "Inventory.h"

struct LootGrid {
  // The held item's travel, the validity flip, the unique shimmer, and the
  // gold counter's tick.
  choreograph::Output<float> dragX{0}, dragY{0};
  choreograph::Output<float> blockedMix{0}, fitsMix{0};
  // A PHASE in [0,1], not a distance: the call sites shape it into pixels
  // with bind(), so this stays a unit value.
  choreograph::Output<float> shimmer{0};
  choreograph::Output<float> goldFrac{0};
  int gold = 0;

  std::shared_ptr<instancing::Atlas> cellAtlas;
  std::shared_ptr<instancing::Pool> cellPool;

  int shownGold = -1;

  // The drag runs on a 4.4 s cycle, and a still caught mid-slide shows the
  // blocked and fitting footprints blended together — the item claiming to
  // be both at once. This lands in the middle of the blocked rest, which is
  // the authored setup pose and the one that shows the overlap mechanic.
  // Around 7.3 s would show the green "fits" pose instead.

  /** The counter ticks, so the tree re-renders — but only on the frames
   *  where the integer actually changed. That is the reconciler's job and
   *  this is the cheapest honest way to exercise it. */
  void update(double, sketch::SketchContext& ctx) {
    Composer& composer = ctx.composer;
    if (gold != shownGold) {
      shownGold = gold;
      composer.render(describe());
    }
  }

  // Where the dragged shield rests: over the wand+gloves (blocked), then
  // on the hoard's one free 2x3 pocket. Both poses sit ON cells, so the
  // red and the green are each saying something true about the hoard.
  static constexpr int kDragW = 2, kDragH = 3;
  static SkPoint blockedAt() { return {loot::cellX(4), loot::cellY(1)}; }
  static SkPoint freeAt() { return {loot::cellX(7), loot::cellY(1)}; }

  /** The display face an item's NAME is set in. Diablo II names items in
   *  a blackletter-derived type; the fallback chain walks from the
   *  closest thing a system carries down to a serif, and the grotesque
   *  the rest of the sheet is set in is never reached. */
  sk_sp<SkTypeface> displayFace;

  void setup(sketch::SketchContext& ctx) {
    sketch::kit::stage(ctx, {.size = kSceneSize,
                             .captureAt = 5.1,
                             .background = SkColor4f{0, 0, 0, 1}});
    displayFace = weave::ports::face({"Papyrus", "Baskerville", "Hoefler Text",
                                      "Iowan Old Style", "Georgia"},
                                     600);
    Composer& composer = ctx.composer;
    sigil::motion::Ticker& ticker = ctx.ticker;
    namespace lt = loot;
    dragX = blockedAt().x();
    dragY = blockedAt().y();
    blockedMix = 1;
    fitsMix = 0;
    shimmer = 0;
    goldFrac = 0;
    gold = 0;

    // One atlas cell, forty instances: the empty grid is a single
    // drawAtlas stamp, which is the whole point of the flyweight layer.
    cellAtlas = std::make_shared<instancing::Atlas>(2.0f);
    cellAtlas->cell(lt::well(lt::kCell, lt::kCell), {lt::kCell, lt::kCell});
    cellPool = std::make_shared<instancing::Pool>();
    // An instance sits at the centre of its slot; `place::grid` fills the
    // lane row-major off the same `cellRect` the laid-out cells use, so
    // the stamps and the items cannot drift apart.
    instancing::place::grid(*cellPool, (size_t)lt::kCols * (size_t)lt::kRows,
                            lt::kCols, {lt::kCell, lt::kCell}, {0, 0},
                            {lt::kGap, lt::kGap});

    ticker.add([this, &ticker] {
      const double t = ticker.elapsed();
      // 4.4 s round trip: rest blocked, slide, rest free, slide back.
      const double cycle = std::fmod(t, 4.4);
      double u = 0;  // 0 = blocked slot, 1 = free slot
      if (cycle < 1.4)
        u = 0;
      else if (cycle < 2.2)
        u = (cycle - 1.4) / 0.8;
      else if (cycle < 3.6)
        u = 1;
      else
        u = 1.0 - (cycle - 3.6) / 0.8;
      const float ease =
          (float)(u * u * (3.0 - 2.0 * u));  // smoothstep, the UI standard
      const SkPoint a = blockedAt(), b = freeAt();
      dragX = a.x() + (b.x() - a.x()) * ease;
      dragY = a.y() + (b.y() - a.y()) * ease;
      blockedMix = 1.0f - ease;
      fitsMix = ease;
      shimmer = (float)std::fmod(t * 0.42, 1.0);
      const float g = (float)std::min(1.0, t / 1.6);
      goldFrac = g;
      gold = (int)(214860 * g);
    });

    composer.render(describe());
  }

  // ------------------------------------------------------------------

  /** The hoard: ten by four at the paperdoll's own cell, standing in the
   *  lower half of the one tall panel. */
  Element gridPanel() {
    namespace lt = loot;
    Element grid =
        stack().width(lt::kGridW).height(lt::kGridH).at({16.0f, 516.0f});

    // the empty wells — forty cells, one stamp
    grid.children({instances(cellAtlas, cellPool)});

    // the items, each on its rarity-tinted cell
    for (const auto& item : lt::kItems) {
      const float w = lt::spanW(item.w), h = lt::spanW(item.h);
      const SkColor4f rc = lt::rarityColor(item.rarity);
      const bool lit =
          item.rarity == lt::Rarity::Unique || item.rarity == lt::Rarity::Set;
      Element cell =
          box()
              .width(w)
              .height(h)
              .corners({2})
              .at({lt::cellX(item.col), lt::cellY(item.row)})
              // THE CELL STAYS NEUTRAL. Diablo II reads an item's quality
              // off the NAME in its tooltip and off the glow a set or
              // unique throws; the cell behind it is the same dark well
              // whatever is in it. Tinting the cell makes the ladder the
              // loudest thing on the screen and turns a hoard into a
              // status grid, so what quality carries here is a hairline
              // and, for the two lit ranks, a bloom.
              .fill(Paint::linear({0, 0}, {0, h},
                                  {{0.0f, {0.10f, 0.095f, 0.082f, 0.92f}},
                                   {1.0f, {0.05f, 0.048f, 0.042f, 0.92f}}}))
              .foreground(stroke(
                  1.0f, Fill::color({rc.fR, rc.fG, rc.fB, lit ? 0.8f : 0.34f})))
              .row()
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .children({lt::artwork(item.art, w * 0.76f, h * 0.80f, item.tint,
                                     item.art == lt::Art::Potion)});
      // A set or a unique GLOWS — the one thing besides the tooltip's
      // name that says what a thing is worth.
      if (lit)
        cell.background(
            styles::dropShadow({rc.fR, rc.fG, rc.fB, 0.62f}, {0, 0}, 13));
      // uniques catch a light band that sweeps them
      if (item.rarity == lt::Rarity::Unique)
        cell.children({box().inset(1).clip().children(
            {box()
                 .width(w * 0.30f)
                 .height(h * 1.8f)
                 .at({-w * 0.4f, -h * 0.4f})
                 .translateX(motion::bind(&shimmer).target(-70, 170))
                 .rotate(18.0f)
                 .fill(Paint::linear({0, 0}, {w * 0.35f, 0},
                                     {{0.0f, {1, 1, 1, 0.0f}},
                                      {0.5f, {1, 1, 1, 0.30f}},
                                      {1.0f, {1, 1, 1, 0.0f}}}))
                 .blend(SkBlendMode::kPlus)})});
      grid.children({std::move(cell)});
    }

    // the held item's footprint: green where it fits, red where it does not
    const float dw = lt::spanW(kDragW), dh = lt::spanW(kDragH);
    // FarrokhGames' two cell sprites, cross-faded: green while the
    // footprint is free, red the moment it overlaps something.
    // and the item riding it
    grid.children(
        {box()
             .width(dw)
             .height(dh)
             .corners({2})
             .at({0, 0})
             .translateX(&dragX)
             .translateY(&dragY)
             .fill(Paint::solid({0.16f, 0.80f, 0.24f, 0.26f}))
             .foreground(stroke(1.4f, Fill::color({0.35f, 1.0f, 0.45f, 0.75f})))
             .opacity(&fitsMix)
             .zIndex(5),
         box()
             .width(dw)
             .height(dh)
             .corners({2})
             .at({0, 0})
             .translateX(&dragX)
             .translateY(&dragY)
             .fill(Paint::solid({0.90f, 0.16f, 0.14f, 0.30f}))
             .foreground(stroke(1.4f, Fill::color({1.0f, 0.35f, 0.30f, 0.8f})))
             .opacity(&blockedMix)
             .zIndex(6),
         kit::centred()
             .width(dw)
             .height(dh)
             .at({0, 0})
             .translateX(&dragX)
             .translateY(&dragY)
             .row()

             .zIndex(7)
             .children({lt::artwork(lt::Art::Shield, dw * 0.78f, dh * 0.62f,
                                    hexColor(0x8895A2))})});

    return stack()
        .width(430.0f)
        .height(690.0f)
        .at({30.0f, 96.0f})
        .children(
            {document::h2("HOARD").styleClass("heading").at({16.0f, 492.0f}),
             text("10 ×"
                  " 4")
                 .font({.size = 11, .track = 2.0f})
                 .at({398.0f, 493.0f}),
             std::move(grid)});
  }

  Element paperdoll() {
    namespace lt = loot;
    // ONE TALL PANEL. D2 hangs the paperdoll and the ten-by-four hoard
    // in a single stone plate, and the grid is the study's subject: put
    // it in a panel of its own beside the figure and it reads as a
    // second, smaller thing. The panel is as tall as both.
    constexpr float pw = 430, ph = 690, pad = 16;
    struct Slot {
      const char* label;
      lt::Art ghost;
      float x, y;
      int w, h;
    };
    // D2's footprints: helm 2x2, amulet 1x1, armour 2x3, weapon 2x4,
    // shield 2x4, belt 2x1, rings 1x1, gloves 2x2, boots 2x2.
    //
    // The belt row is the tightest one and it fixes every x below: five
    // sockets — weapon, ring, belt, ring, shield — are 78+38+78+38+78 = 310
    // px of the 350 between the outer two, so the four gutters are 10 px
    // each and nothing on the row is free to move. The helm and armour share
    // the belt's column, and the amulet shares the right ring's.
    static const Slot kSlots[] = {
        {"HELM", lt::Art::Helm, 148, 0, 2, 2},
        {"AMULET", lt::Art::Amulet, 236, 20, 1, 1},
        {"WEAPON", lt::Art::Sword, 12, 92, 2, 4},
        {"ARMOUR", lt::Art::Armour, 148, 92, 2, 3},
        {"SHIELD", lt::Art::Shield, 284, 92, 2, 4},
        {"RING", lt::Art::Ring, 100, 216, 1, 1},
        {"BELT", lt::Art::Belt, 148, 216, 2, 1},
        {"RING", lt::Art::Ring, 236, 216, 1, 1},
        {"GLOVES", lt::Art::Gloves, 12, 262, 2, 2},
        {"BOOTS", lt::Art::Boots, 284, 262, 2, 2},
    };
    // Which sockets are filled, and with what.
    struct Worn {
      int slot;
      lt::Rarity rarity;
      SkColor4f tint;
    };
    static const Worn kWorn[] = {
        {0, lt::Rarity::Rare, hexColor(0x9AA0A6)},
        {2, lt::Rarity::Unique, hexColor(0xB9A06A)},
        {3, lt::Rarity::Set, hexColor(0x6E8F63)},
        {6, lt::Rarity::Magic, hexColor(0x7A6A55)},
        {9, lt::Rarity::Normal, hexColor(0x7A6A55)},
    };

    Element body = stack().inset(0);
    body.children({each(kSlots, [&](const Slot& s, size_t i) -> Element {
      const float w = lt::spanW(s.w), h = lt::spanW(s.h);
      const Worn* equipped = nullptr;
      for (const Worn& candidate : kWorn)
        if (candidate.slot == (int)i) equipped = &candidate;

      Element socket =
          stack()
              .rect(SkRect::MakeXYWH(pad + 17 + s.x, pad + 22 + s.y, w, h))
              .children({lt::well(w, h).inset(0)});
      if (equipped) {
        const SkColor4f rc = lt::rarityColor(equipped->rarity);
        socket.children(
            {box()
                 .inset(2)
                 .corners({2})
                 // The same rule as the hoard's cells: the socket is a dark
                 // well whatever is worn in it, and quality is a hairline.
                 .fill(Paint::linear({0, 0}, {0, h},
                                     {{0.0f, {0.10f, 0.095f, 0.082f, 0.95f}},
                                      {1.0f, {0.05f, 0.048f, 0.042f, 0.95f}}}))
                 .foreground(
                     stroke(1.0f, Fill::color({rc.fR, rc.fG, rc.fB, 0.5f}))),
             kit::centred()
                 .inset(0)
                 .row()

                 .children({lt::artwork(s.ghost, w * 0.72f, h * 0.76f,
                                        equipped->tint)})});
        return socket;
      }
      // An empty socket hints at what belongs in it, and names it. The
      // label rides INSIDE the well, so the widest word has to clear the
      // narrowest socket: AMULET on a single cell is 38 px of room. A
      // one-cell socket therefore drops the tracking and condenses, which
      // narrows the run without cutting the cap height the label is read
      // by.
      const bool narrow = s.w < 2;
      socket.children(
          {kit::centred()
               .inset(0)
               .row()

               .opacity(0.13f)
               .children(
                   {lt::artwork(s.ghost, w * 0.64f, h * 0.68f, lt::kParch)}),
           text(s.label)
               .font({.size = 7.0f,
                      .track = narrow ? 0.4f : 1.3f,
                      .condense = narrow ? 0.86f : 1.0f})
               .left(0)
               .right(0)
               .bottom(3)
               .block({.alignment = sigil::weave::TextAlignment::kCenter})});
      return socket;
    })});

    // the stat block D2 puts under the paperdoll: two columns of
    // label-dots-value rows, laid out rather than absolutely stacked
    auto statRow = [&](const char* label, const char* value,
                       SkColor4f valueColor) {
      return box()
          .row()
          .width(166.0f)
          .alignItems(Align::Center)
          .children({text(label).font({.size = 10.5f, .track = 1.1f}),
                     box()
                         .grow(1)
                         .height(1.0f)
                         .margin(6, 0, 6, 0)
                         .fill(Paint::solid({0.42f, 0.38f, 0.31f, 0.28f})),
                     text(value).styleClass("value").ink(valueColor)});
    };
    // The two columns D2 puts under the paperdoll. A reading here is not
    // the kit's: its name and its figure are held apart by a dotted
    // leader, and each figure carries the ink of what it SAYS — a
    // resistance in its own element's colour, a capped stat in the magic
    // blue — which a readout ranges to one register.
    struct Reading {
      const char* name;
      const char* figure;
      SkColor4f ink;
    };
    static const std::array<std::array<Reading, 4>, 2> kColumns{
        {{{{"STRENGTH", "142", lt::kParch},
           {"DEXTERITY", "97", lt::kParch},
           {"VITALITY", "206", lt::rarityColor(lt::Rarity::Magic)},
           {"ENERGY", "68", lt::kParch}}},
         {{{"DEFENCE", "1,884", lt::kParch},
           {"FIRE RES", "+65%", hexColor(0xE07A3C)},
           {"COLD RES", "+41%", hexColor(0x5AA6E0)},
           {"LIGHT RES",
            "\u2212"
            "35%",
            hexColor(0xD04040)}}}}};
    body.children(
        {box()
             .row()
             .gap(20)
             .at({pad + 27, pad + 376})
             .children(
                 {each(kColumns,
                       [&](const std::array<Reading, 4>& column) -> Element {
                         return box().column().gap(4).children(
                             {each(column, [&](const Reading& r) -> Element {
                               return statRow(r.name, r.figure, r.ink);
                             })});
                       })})});

    return stack().width(pw).height(ph).at({30, 96}).children(
        {loot::panel(pw, ph).inset(0), loot::rivets(pw, ph),
         document::h2("EQUIPPED").styleClass("heading").at({pad, pad}),
         std::move(body)});
  }

  /** The hover tooltip, D2's stack: name in the rarity colour, base type,
   *  affix lines, then requirements — unmet ones in red. */
  Element tooltip() {
    namespace lt = loot;
    using namespace std::chrono_literals;
    const SkColor4f rc = lt::rarityColor(lt::Rarity::Unique);
    return box()
        .width(300.0f)
        .at({500, 300})
        .column()
        .alignItems(Align::Center)
        .padding(14, 11)
        .gap(2)
        .corners({2})
        .fill(Paint::solid({0.02f, 0.02f, 0.02f, 0.90f}))
        .foreground(stroke(1.0f, Fill::color({rc.fR, rc.fG, rc.fB, 0.45f})))
        .background(styles::dropShadow({0, 0, 0, 0.7f}, {0, 5}, 12))
        .key("tooltip")
        .opacity(animate(motion::from(0.0f).to(1.0f), {380ms}))
        .translateY(animate(motion::from(8.0f).to(0.0f), {460ms}))
        .zIndex(9)
        // Every line under the name is set in this one size and tracking;
        // what each says for itself is its colour.
        .font({.size = 11.5f, .track = 0.2f})
        // THE NAME IS THE ONE PLACE QUALITY IS SPELLED OUT, so it is set
        // in a display face rather than in the sheet's grotesque: Diablo
        // II names items in a blackletter-derived display type and reads
        // everything else in a small serif, and the difference between
        // those two registers is most of what makes a tooltip feel like
        // that game's tooltip.
        .children(
            {text("Doomslinger")
                 .font({.face = displayFace,
                        .size = 17,
                        .color = rc,
                        .track = 1.2f,
                        .weight = 620}),
             text("Colossus Blade")
                 .font({.color = lt::kParch, .track = 0.8f})
                 .margin(0, 0, 0, 6),
             text("189% Enhanced Damage").styleClass("affix"),
             text("+2 to Fire Skills").styleClass("affix"),
             text("Adds 40-92 Fire Damage").styleClass("affix"),
             text("Ignores Target's Defence").styleClass("affix"),
             box()
                 .width(180.0f)
                 .height(1.0f)
                 .margin(0, 7, 0, 5)
                 .fill(Paint::linear({0, 0}, {180, 0},
                                     {{0.0f, {rc.fR, rc.fG, rc.fB, 0.0f}},
                                      {0.5f, {rc.fR, rc.fG, rc.fB, 0.5f}},
                                      {1.0f, {rc.fR, rc.fG, rc.fB, 0.0f}}})),
             text("Required Strength: 189"),
             text("Required Level: 63").ink(hexColor(0xD04040))});
  }

  /** THE BELT: four quick-slots on the same cell, which is the one place
   *  Diablo II lets an item be USED rather than carried. It is here
   *  because a grid study wants more than one construction over the same
   *  well and the same footprint rules. */
  Element beltRack() {
    namespace lt = loot;
    // Three potions and a ring, on the keys they are drunk with.
    static const std::array<lt::Art, 4> kHeld{lt::Art::Potion, lt::Art::Potion,
                                              lt::Art::Potion, lt::Art::Ring};
    return box()
        .column()
        .gap(7)
        .at({500, 150})
        .children(
            {document::h2("BELT").styleClass("heading"),
             box().row().gap(lt::kGap).key("belt").children(
                 {each(kHeld, [](lt::Art held, size_t i) -> Element {
                   const bool potion = held == lt::Art::Potion;
                   return stack().width(lt::kCell).height(lt::kCell).children(
                       {lt::well(lt::kCell, lt::kCell).inset(0),
                        kit::centred()
                            .inset(0)
                            .row()

                            .children({lt::artwork(held, lt::kCell * 0.6f,
                                                   lt::kCell * 0.7f,
                                                   potion ? hexColor(0xC24040)
                                                          : hexColor(0xB9A06A),
                                                   potion)}),
                        text(std::to_string(i + 1))
                            .font({.size = 8, .track = 0.4f})
                            .at({3, 2})});
                 })})});
  }

  /** THE CUBE: three by four, the transmutation grid. The same well, the
   *  same footprint arithmetic and a different rule about what may sit in
   *  it — which is the argument for a grid being a LAYOUT and not a
   *  widget. */
  Element cubePanel() {
    namespace lt = loot;
    // Twelve wells on the hoard's own cell, and the one ring standing in
    // the middle of them.
    return box()
        .column()
        .gap(7)
        .at({500, 560})
        .children(
            {document::h2("HORADRIC CUBE · 3 ×"
                          " 4")
                 .styleClass("heading"),
             stack()
                 .width(3 * lt::kCell + 2 * lt::kGap)
                 .height(4 * lt::kCell + 3 * lt::kGap)
                 .children(
                     {each(12,
                           [](int i) -> Element {
                             return lt::well(lt::kCell, lt::kCell)
                                 .key("cube" + std::to_string(i))
                                 .at({lt::cellX(i % 3), lt::cellY(i / 3)});
                           }),
                      kit::centred()
                          .rect(SkRect::MakeXYWH(lt::cellX(1), lt::cellY(1),
                                                 lt::kCell, lt::kCell))
                          .row()

                          .children({lt::artwork(
                              lt::Art::Ring, lt::kCell * 0.6f, lt::kCell * 0.6f,
                              hexColor(0xB9A06A))})})});
  }

  /** THE SHEET'S OWN VOICE. The kit's components read a theme, and the
   *  one register this hoard's keys are set in is its own: 10.5 px at
   *  0.8 tracking, which is what was measured off the reference. Bound
   *  where the tree is DESCRIBED, because that is the scope a component
   *  four levels down reads. */
  static sketch::kit::Theme sheetTheme() {
    sketch::kit::Theme look = sketch::kit::houseTheme();
    look.type.captionNote = {10.5f, 0.8f};
    look.palette.ink = loot::kAsh;
    return look;
  }

  /** THE CLASSES PAST THE REGISTERS: a panel's bronze heading, the figure
   *  a stat row answers with, and a tooltip's affix line, which is set in
   *  the magic colour. Every other line is the root's ink at a size of
   *  its own. */
  static weave::StyleSheet classes(const sketch::kit::Theme& look) {
    weave::StyleSheet sheet = look.styleSheet();
    sheet.set(
        "heading",
        {.size = 12, .color = loot::kBronzeLit, .track = 4.5f, .weight = 650});
    sheet.set("value", {.size = 12, .track = 0.5f, .weight = 620});
    sheet.set("affix", {.color = loot::rarityColor(loot::Rarity::Magic)});
    return sheet;
  }

  Element describe() {
    namespace lt = loot;
    const sketch::kit::Theme look = sheetTheme();
    const sketch::kit::Provide bound(look);
    const std::string goldText = kit::formatted("%d", gold);

    // THE ROOT OF THE CASCADE: ash is the ink every line that names no
    // colour of its own is set in — the keys, the labels, the remarks.
    auto root = stack()
                    .styleSheet(classes(look))
                    .ink(lt::kAsh)
                    .fill(Paint::linear({0, 0}, {0, lt::kH},
                                        {{0.0f, hexColor(0x0D0C0A)},
                                         {0.5f, hexColor(0x14120F)},
                                         {1.0f, hexColor(0x080706)}}));

    // THE GROUND IS TOOLED LEATHER, and it is tiled rather than painted:
    // two patterns over one dark ramp, each a repeating tile the
    // renderer resolves rather than an image anyone fetched. The grain is
    // the hide and the wide gold rule grid is
    // the blind tooling a bound cover carries. A panel needs something
    // under it that is not a gradient, or its bevel has no ground to be a
    // bevel against.
    //
    // IT IS ONE BAKED NODE, AT HALF SCALE. Four full-canvas shaders
    // re-evaluated per frame cost this scene forty milliseconds for a
    // picture that never changes; under one `Cache::Texture` the ground is
    // rasterised once and blitted after. The bake itself is the only
    // expensive frame in the piece, so it is taken at a third of the canvas and
    // scaled up — a hide's grain and a blind rule have nothing in them
    // that a coarse raster loses, and the bake costs a ninth.
    // gold, on its own little plaque
    root.children(
        {box()
             .key("ground")
             .inset(0)
             .cache(Cache::Texture)
             .bakeScale(0.34f)
             .children(
                 {box()
                      .inset(0)
                      .fill(Paint::recipe(field::noise(0.9f, 4, 3.0f)))
                      .opacity(0.34f)
                      .blend(SkBlendMode::kOverlay),
                  box().inset(0).fill(
                      Pattern(mpattern::gridLines(96.0f, 1.0f,
                                                  {0.62f, 0.50f, 0.26f, 0.10f}))
                          .material()),
                  box()
                      .inset(0)
                      .fill(Pattern(mpattern::gridLines(
                                        96.0f, 1.0f, {0.0f, 0.0f, 0.0f, 0.22f}))
                                .material())
                      .translateX(1.0f)
                      .translateY(1.0f)}),
         box().column().at({30, 34}).children(
             {text("HOARD OF THE HORADRIM")
                  .font({.size = 23,
                         .color = lt::kParch,
                         .track = 3.4f,
                         .weight = 640}),
              text("grid inventory — generated "
                   "materials, no sprites")
                  .font({.size = 12, .track = 1.0f})
                  .margin(0, 5, 0, 0)}),
         paperdoll(), gridPanel(), tooltip(), beltRack(), cubePanel(),
         box()
             .row()
             .alignItems(Align::Center)
             .gap(9)
             .right(30)
             .top(38)
             .padding(13, 7)
             .corners({3})
             .fill(Paint::linear({0, 0}, {0, 32},
                                 {{0.0f, lt::kStoneHi}, {1.0f, lt::kStoneLo}}))
             .foreground(stroke(1.0f, Fill::color(lt::kBronzeDim)))
             .children({box().width(13.0f).height(13.0f).corners({6.5f}).fill(
                            Paint::radial({5, 4}, 9,
                                          {{0.0f, hexColor(0xFFE9A8)},
                                           {0.6f, hexColor(0xD8A93C)},
                                           {1.0f, hexColor(0x7A5C15)}})),
                        text(goldText).font({.size = 17,
                                             .color = hexColor(0xD8B95C),
                                             .track = 1.6f,
                                             .weight = 620}),
                        text("GOLD").font({.size = 10, .track = 2.2f})})});

    // The two keys, bottom left and bottom right. Both are
    // `sketch::kit::legend` under the sheet's own theme: a dim body
    // inside a bright edge is the entry's `keyline`, and the rarity
    // ladder's words are set in what they name through its `ink`.
    auto tier = [](lt::Rarity r, const char* label) {
      const SkColor4f c = lt::rarityColor(r);
      return sketch::kit::LegendEntry{
          Fill::color({c.fR * 0.35f, c.fG * 0.35f, c.fB * 0.35f, 1}),
          label,
          {},
          Fill::color(c),
          Fill::color(c)};
    };
    // The occupancy key. Its two entries stand further apart than the
    // ladder's, because they name two answers to one question rather
    // than five steps of one scale.
    root.children(
        {sketch::kit::legend({.entries = {tier(lt::Rarity::Normal, "normal"),
                                          tier(lt::Rarity::Magic, "magic"),
                                          tier(lt::Rarity::Rare, "rare"),
                                          tier(lt::Rarity::Set, "set"),
                                          tier(lt::Rarity::Unique, "unique")},
                              .column = false,
                              .swatchSide = 9.0f,
                              .gap = 15.0f,
                              .corners = 1.5f,
                              .labelGap = 6.0f})
             .right(30)
             .bottom(26),
         sketch::kit::legend(
             {.entries = {{Fill::color({0.16f, 0.80f, 0.24f, 0.30f}),
                           "fits",
                           {},
                           Fill::color({0.35f, 1.0f, 0.45f, 0.8f}),
                           Fill::color(lt::kAsh)},
                          {Fill::color({0.90f, 0.16f, 0.14f, 0.34f}),
                           "blocked",
                           {},
                           Fill::color({1.0f, 0.35f, 0.30f, 0.8f}),
                           Fill::color(lt::kAsh)}},
              .column = false,
              .swatchSide = 11.0f,
              .gap = 18.0f,
              .corners = 2.0f,
              .labelGap = 8.0f})
             .left(30)
             .bottom(26)});
    return root;
  }
};

}  // namespace

SIGIL_SKETCH_AS(LootGrid, "loot grid", "Catalog · Game UI",
                "D2 hoard — generated materials, instances()")
