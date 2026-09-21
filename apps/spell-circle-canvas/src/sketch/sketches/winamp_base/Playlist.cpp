#include "WinampBase.h"

auto WinampBase::playlistWindow() -> Element {
  using namespace wa;
  const float W = 400, H = 377;
  // The scrollbar's two arrow buttons. NOT a stepper at each end of a track:
  // the rail runs the whole height of the well and the two arrows stack INSIDE
  // its bottom 28 px, over it.
  const auto arrow = [this, W](float y, bool up) {
    return key(W - 19, y, 18, 14, box())
        .children({part(6, up ? 5 : 4, 6, 5, upDown(up))});
  };
  // ADD / REM / SEL / MISC sit in the strip's own coordinates, 6 native px
  // clear of the sill — in the real window the row is pinned to the bottom of
  // the playlist, not to a fixed y in the window, so it stays put whatever
  // height the list is resized to.
  const char* kMenus[4] = {"ADD", "REM", "SEL", "MISC"};
  // THE MINI TRANSPORT DOCK: five keys, each over the parts its glyph is drawn
  // from, in that key's own native coordinates.
  struct Mini {
    std::vector<Element> glyph;
  };
  const std::vector<Mini> kMini = {
      {{part(2, 3, 1, 5), part(4, 3, 4, 5, tri(1))}},   // prev
      {{part(3, 2, 5, 6, tri(0))}},                     // play
      {{part(3, 3, 2, 5), part(6, 3, 2, 5)}},           // pause
      {{part(3, 3, 5, 5)}},                             // stop
      {{part(3, 3, 4, 5, tri(0)), part(8, 3, 1, 5)}}};  // next
  return raised(box().width(n(W)).height(n(H)), kWellHi, kWellLo)
      .children(
          {box().inset(0).fill(steel).cache(Cache::Texture),
           titleBar(W, "WINAMP PLAYLIST", true, false, 20.0f),
           // The list well: left rail 12, right rail 20. At this window height
           // it is 319 native px tall, which is 24 whole 13 px rows and half
           // of a twenty-fifth — the list draws all 25 and the clip takes the
           // rest, exactly as the real window does at a size that is not a
           // multiple of the row height. The row backgrounds are one atlas
           // stamp in three tint states.
           sunken(at(box(), 12, 20, W - 32, 319).fill(kPlBg),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.6f),
                  hexColor(0x06060A))
               .children({box().inset(0).children({instancing::instances(
                              rowAtlas, rowPool, instancing::Mode::Live)}),
                          box()
                              .inset(0)
                              .overflow(Overflow::Clip)
                              .children({slot("tracks")})}),
           // the rail, and the grip — a skin's sprite at the one size the skin
           // cut it, so it is not a reading of how much list is showing
           sunken(at(box(), W - 20, 20, 20, 319).fill(hexColor(0x1A1A2A)),
                  mskia::withAlpha(hexColor(0x4A4A70), 0.5f),
                  hexColor(0x0A0A12)),
           raised(at(box(), W - 19, 24, 18, 36).fill(kBtnFace)),
           arrow(310, true), arrow(324, false),
           // the bottom control strip (native y 339..377)
           at(box(), 0, 339, W, 38)
               .children(
                   {box().inset(0).fill(steel).cache(Cache::Texture),
                    at(box(), 0, 0, W, 1)
                        .fill(mskia::withAlpha(hexColor(0x585880), 0.6f)),
                    each(kMenus,
                         [this](const char* words, size_t i) {
                           return textKey(14 + 29 * (float)i, 14, 22, 18, words,
                                          4.0f);
                         }),
                    key(W - 44, 14, 22, 18, box())
                        .column()
                        .justifyContent(Justify::Center)
                        .alignItems(Align::Center)
                        .children({t("LIST", pix(3.8f)), t("OPTS", pix(3.8f))}),
                    // running-time readout
                    at(box(), 132, 13, 62, 7)
                        .alignItems(Align::Center)
                        .children({t(runningTime(), pix(4.0f)).ink(kPlText)}),
                    sunken(at(box(), 132, 22, 62, 12).fill(hexColor(0x12121E)),
                           mskia::withAlpha(hexColor(0x4A4A70), 0.55f),
                           hexColor(0x08080E))
                        .children(each(
                            kMini,
                            [](const Mini& m, size_t i) {
                              return raised(
                                         at(box(), 2 + 12 * (float)i, 1, 11, 10)
                                             .fill(mskia::withAlpha(kBtnFace,
                                                                    0.9f)),
                                         mskia::withAlpha(kBtnHi, 0.8f), kBtnLo)
                                  .children({box().children(m.glyph)});
                            })),
                    // the preview-visualiser swatch (default checkerboard art)
                    sunken(
                        at(box(), W - 88, 20, 38, 14).fill(hexColor(0x000000)),
                        mskia::withAlpha(hexColor(0x4A4A70), 0.5f),
                        hexColor(0x08080E))
                        .children(
                            {box().inset(0).fill(previewCheck.material())})})});
}

auto WinampBase::runningTime() -> std::string {
  int total = 0;
  for (const Track& tr : tracks()) total += tr.seconds;
  return mmss((int)(playPos.value() * tracks()[(size_t)nowPlaying].seconds)) +
         "/" + mmss(total);
}

auto WinampBase::trackList() -> Element {
  using namespace wa;
  const float rowH = 13, listW = 368;
  return box().column().children(each(tracks(), [&](const Track& tr, size_t i) {
    // PLEDIT.TXT Font=Arial, CSS font-size 9px + 0.5px tracking.
    const auto st = type(arial(), n(9) * 0.78f,
                         (int)i == nowPlaying ? kPlNow : kPlText, n(0.5f));
    // Rows reveal in bands of four: 25 rows on an even stagger reads as 25
    // separate animations, where batching reads as a list populating.
    return box()
        .height(n(rowH))
        .row()
        .alignItems(Align::Center)
        .padding(n(3), 0, n(3), 0)
        .opacity(&rowIn[i])
        .translateY(motion::bind(&rowIn[i]).invert().scale(n(2)))
        .children({ellipsized((int)i, std::to_string(i + 1) + ". " + tr.title,
                              st, n(listW - 40)),
                   box().flexGrow(1), text(tr.time, st)});
  }));
}

auto WinampBase::ellipsized(int idx, const std::string& s,
                            const sigil::weave::TextStyle& st, float w)
    -> Element {
  sigil::weave::ParagraphBuilder b(st);
  b.addText(s);
  auto p = std::make_shared<sigil::weave::Paragraph>(b.build());
  if ((int)rowPara.size() <= idx) rowPara.resize((size_t)idx + 1);
  rowPara[(size_t)idx] = p;
  sigil::weave::ParagraphLayoutOptions o;
  o.overflow.ellipsis = u"…";
  o.overflow.maxLines = 1;
  return text(p, o).width(w).flexShrink(0);
}
