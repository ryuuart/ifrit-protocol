#pragma once
// Split from GalleryScenes.h — see that header for the registry.

#include "GalleryCore.h"

#include <random>

namespace compose_gallery {

// ---- 1: live scoreboard (reconcile + transitions) ------------------------

struct ScoreboardScene final : Scene {
  struct Row {
    std::string name;
    int score;
    bool highlighted;
    bool operator==(const Row &) const = default;
  };
  std::vector<Row> rows;
  std::mt19937 rng{7};
  double nextMutation = 0.0;

  const char *name() const override { return "scoreboard"; }

  static Element rowElement(const Row &row) {
    return box().row().gap(12).padding(10).corners({8})
        .transition({350ms})
        .fill(Fill::color(row.highlighted
                              ? SkColor4f{0.35f, 0.20f, 0.52f, 1}
                              : SkColor4f{0.10f, 0.11f, 0.16f, 1}))
        .child(text(toU8(row.name), styleAt(20, 0xffe8ecf8)).grow(1))
        .child(text(toU8(std::to_string(row.score)),
                    styleAt(20, 0xff7ee8ff)));
  }

  Element describe() {
    auto list = box().column().gap(8).padding(28)
                    .fill(Fill::color({0.06f, 0.05f, 0.12f, 1}));
    list.child(text(u8"LIVE STANDINGS", styleAt(30, 0xffffb46b)));
    for (const Row &row : rows)
      list.child(memo(row, rowElement).key(row.name));
    return list;
  }

  void setup(Composer &composer, sigil::tick::Ticker &) override {
    rows = {{"ember", 128, false}, {"sigil", 96, true},
            {"cinder", 77, false}, {"ash", 64, false},
            {"flare", 51, false},  {"soot", 12, false}};
    nextMutation = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextMutation)
      return;
    nextMutation = elapsed + 0.9;
    for (Row &row : rows) {
      row.score += (int)(rng() % 9);
      row.highlighted = false;
    }
    rows[rng() % rows.size()].highlighted = true;
    std::stable_sort(rows.begin(), rows.end(),
                     [](const Row &a, const Row &b) {
                       return a.score > b.score;
                     });
    composer.render(describe());
  }
};

// ---- 8: independent data domains via slot() (#4) --------------------------

struct SlotsScene final : Scene {
  int counter = 0;
  double nextTickerUpdate = 0.0;

  const char *name() const override { return "slots"; }

  void setup(Composer &composer, sigil::tick::Ticker &) override {
    counter = 0;
    nextTickerUpdate = 0.0;
    composer.render(
        box().column().padding(48).gap(18)
            .fill(Fill::color({0.06f, 0.05f, 0.12f, 1}))
            .child(text(u8"STATIC POSTER CHROME", styleAt(44, 0xffffb46b)))
            .child(text(u8"everything here keeps its caches while the "
                        u8"ticker below re-renders alone",
                        styleAt(18, 0xff9aa4bb)))
            .child(box().grow(1))
            .child(slot("ticker").height(64)));
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextTickerUpdate)
      return;
    nextTickerUpdate = elapsed + 0.1; // 10 Hz independent domain
    ++counter;
    composer.renderSlot(
        "ticker",
        box().row().gap(10).padding(12).corners({10})
            .fill(Fill::color({0.12f, 0.16f, 0.26f, 1}))
            .child(text(u8"live feed", styleAt(18, 0xff9aa4bb)).grow(1))
            .child(text(toU8("tick " + std::to_string(counter)),
                        styleAt(18, 0xff7ee8ff))));
  }
};

// ---- 9: LayoutScheme grid + query-driven surround (#5, #6) ----------------

struct GridScene final : Scene {
  struct Grid {
    int columns = 4;
    float gap = 12;
    float cellHeight = 120;
    std::vector<SkRect> place(const LayoutInput &in) const {
      std::vector<SkRect> rects;
      const float w =
          (in.container.width() - gap * (float)(columns - 1)) /
          (float)columns;
      for (size_t i = 0; i < in.childSizes.size(); ++i)
        rects.push_back(SkRect::MakeXYWH(
            (w + gap) * (float)((int)i % columns),
            (cellHeight + gap) * (float)((int)i / columns), w,
            cellHeight));
      return rects;
    }
  };
  int highlighted = 0;
  double nextMove = 0.0;
  SkRect ringRect = SkRect::MakeEmpty(); // last frame's bounds() feedback

  const char *name() const override { return "grid_query"; }

  Element describe() {
    auto grid = layout(Grid{}).width(pct(100)).grow(1);
    for (int i = 0; i < 12; ++i)
      grid.child(box().key("cell" + std::to_string(i)).corners({10})
                     .fill(Fill::color(i == highlighted
                                           ? SkColor4f{0.35f, 0.2f, 0.52f, 1}
                                           : SkColor4f{0.1f, 0.11f, 0.16f, 1}))
                     .child(text(toU8("cell " + std::to_string(i)),
                                 styleAt(16, 0xff9aa4bb))
                                .absolute().inset(10, 10, 10, 80)));
    SkRect ring = ringRect;
    return stack().fill(Fill::color({0.05f, 0.06f, 0.1f, 1}))
        .child(box().inset(30, 30, 30, 30).child(std::move(grid)))
        .child(custom([ring](SkCanvas &c, const PaintContext &) {
                 if (ring.isEmpty())
                   return;
                 SkPaint p;
                 p.setAntiAlias(true);
                 p.setStyle(SkPaint::kStroke_Style);
                 p.setStrokeWidth(3);
                 p.setColor(0xffffb46b);
                 c.drawRoundRect(ring.makeOutset(6, 6), 14, 14, p);
               })
                   .inset(0).cache(Cache::None).zIndex(1));
  }

  void setup(Composer &composer, sigil::tick::Ticker &) override {
    highlighted = 0;
    ringRect = SkRect::MakeEmpty();
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed >= nextMove) {
      nextMove = elapsed + 0.8;
      highlighted = (highlighted + 1) % 12;
      composer.render(describe());
    }
    // Query-driven surround: this frame reads resolved bounds, next
    // frame's describe draws the ring (cross-frame feedback — the
    // forward-only law in practice).
    auto b = composer.bounds("cell" + std::to_string(highlighted));
    if (b && *b != ringRect) {
      // ring is drawn in stack-local coords; bounds are composer-space
      ringRect = *b;
      composer.render(describe());
    }
  }
};

// ---- 12: transition choreography — insert/remove/reorder (#18) ------------

struct TransitionScene final : Scene {
  std::vector<int> ids;
  std::mt19937 rng{5};
  int nextId = 0;
  double nextOp = 0.0;

  const char *name() const override { return "transitions"; }

  Element describe() {
    auto list = box().column().gap(8).padding(36)
                    .fill(Fill::color({0.06f, 0.05f, 0.12f, 1}));
    list.child(text(u8"insert / remove / reorder", styleAt(24, 0xffffb46b)));
    for (int id : ids)
      list.child(box().key("item" + std::to_string(id))
                     .row().padding(10).corners({8})
                     .transition({400ms, &choreograph::easeOutQuint})
                     .fill(Fill::color(
                         {0.10f + 0.02f * (float)(id % 5), 0.12f,
                          0.20f + 0.02f * (float)(id % 3), 1}))
                     .child(text(toU8("item " + std::to_string(id)),
                                 styleAt(18, 0xffdde4f2))));
    return list;
  }

  void setup(Composer &composer, sigil::tick::Ticker &) override {
    ids = {0, 1, 2, 3, 4};
    nextId = 5;
    nextOp = 0.0;
    composer.render(describe());
  }

  void update(double elapsed, Composer &composer) override {
    if (elapsed < nextOp)
      return;
    nextOp = elapsed + 0.8;
    switch (rng() % 3) {
    case 0:
      if (ids.size() < 10)
        ids.insert(ids.begin() + (long)(rng() % (ids.size() + 1)), nextId++);
      break;
    case 1:
      if (ids.size() > 3)
        ids.erase(ids.begin() + (long)(rng() % ids.size()));
      break;
    default:
      std::shuffle(ids.begin(), ids.end(), rng);
      break;
    }
    composer.render(describe());
  }
};

// ---- 13: sustained load — cached cards + bound movers (#21) ---------------

struct LoadScene final : Scene {
  std::vector<std::unique_ptr<choreograph::Output<float>>> movers;

  const char *name() const override { return "load"; }

  void setup(Composer &composer, sigil::tick::Ticker &ticker) override {
    movers.clear();
    auto root = stack().fill(Fill::color({0.04f, 0.04f, 0.08f, 1}));
    std::mt19937 rng{3};
    // 300 static cached cards.
    for (int i = 0; i < 300; ++i) {
      const float x = (float)(rng() % 860), y = (float)(rng() % 600);
      root.child(box().width(34).height(22).corners({4})
                     .inset(x, y, 0, 0).absolute()
                     .fill(Fill::color({0.09f, 0.10f, 0.16f, 1})));
    }
    // 24 binding-driven movers over them (only these paint live).
    for (int i = 0; i < 24; ++i) {
      auto out = std::make_unique<choreograph::Output<float>>(0.0f);
      const float y = 20.0f + 25.0f * (float)i;
      const float phase = (float)i * 0.7f;
      root.child(box().width(46).height(18).corners({4})
                     .inset(0, y, 0, 0).absolute()
                     .translateX(out.get())
                     .fill(Fill::color({0.49f, 0.91f, 1.0f, 0.8f})));
      movers.push_back(std::move(out));
      ticker.add([o = movers.back().get(), phase, t = 0.0](double dt) mutable {
        t += dt;
        *o = 430.0f + 410.0f * (float)std::sin(t * 0.9 + phase);
        return true;
      });
    }
    composer.render(std::move(root));
  }
};


} // namespace compose_gallery
