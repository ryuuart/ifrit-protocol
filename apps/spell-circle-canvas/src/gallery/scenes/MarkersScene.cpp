// Scene: query layer — regex markers that follow live edits.
#include "SceneRegistry.h"
#include "SceneSupport.h"

#include <textflow/Query.h>

#include <include/core/SkPaint.h>

#include <cmath>

using namespace textflow;

namespace gallery {

namespace {

QString markersDefaultText() {
  return QStringLiteral(
      "Captain Ada watches the Beacon while Turing recalibrates the "
      "Lattice. Every capitalized Word in this paragraph was found once by "
      "an ICU regex and registered as a named marker set; the highlights "
      "you see are plain setPaint calls over those ranges. The paragraph "
      "keeps editing itself — words swap in and out below the markers — "
      "and the ranges follow the edits through the Paragraph's revision "
      "log, exactly like DOM Range objects. Delete the Beacon and its "
      "marker collapses; retype it and a fresh query picks it up again.");
}

class MarkersScene final : public Scene {
public:
  FrameStats render(SkCanvas *canvas, SkISize size, double elapsedSeconds,
                    int frameNumber, const SceneParams &params,
                    FontContext &fontContext) override {
    if (!m_serif)
      m_serif = defaultSerif(fontContext);
    if (m_body.ensure(params, markersDefaultText(), m_serif)) {
      // Scoped query: only search the window the box can actually place
      // (the frontier of the previous layout). Paste a novel and the regex
      // scans the visible text, not megabytes that will never land — and
      // the marker set stays small, so the per-frame repaint below stays
      // cheap too.
      const uint32_t textLength =
          static_cast<uint32_t>(m_body.paragraph.text().size());
      const CharRange scope{0, m_placedTextEnd > 0
                                   ? std::min(m_placedTextEnd, textLength)
                                   : textLength};
      m_queryWasScoped = scope.end < textLength;
      m_markers = MarkerSet(m_body.paragraph);
      m_markers.setRanges(
          "caps",
          findRegexMatches(m_body.paragraph, u8"\\b\\p{Lu}\\p{Ll}+", scope)
              .value_or(std::vector<CharRange>{}));
    }

    // Scripted live edits: swap a word every ~2.5s; markers ride along.
    static const char8_t *cycle[] = {u8"watches", u8"guards ", u8"studies",
                                     u8"shadows"};
    if (frameNumber > 0 && frameNumber % 150 == 0) {
      for (const char8_t *word : cycle) {
        const size_t textOffset = m_body.paragraph.text().find(
            std::u16string(word, word + 7).c_str());
        if (textOffset != std::u16string::npos) {
          m_body.paragraph.replaceText(static_cast<uint32_t>(textOffset),
                                       static_cast<uint32_t>(textOffset + 7),
                                       cycle[(frameNumber / 150) % 4]);
          break;
        }
      }
    }

    // Hue-cycling highlight over every marker range — paint-only restyle,
    // zero reshaping. The decoration choice rides the same PaintStyle:
    // underline/strikethrough are paint-side too (P8), so toggling them
    // never relayouts either.
    const SkScalar hueSaturationValue[3] = {
        std::fmod(static_cast<float>(elapsedSeconds) * 40.0f, 360.0f), 0.75f,
        0.72f};
    PaintStyle highlight(SkHSVToColor(hueSaturationValue));
    switch (params.intValue(QStringLiteral("decoration"), 1)) {
    case 1:
      highlight.addDecoration({}); // metric underline, ink-skipping
      break;
    case 2:
      highlight.addDecoration({.kind = Decoration::Kind::kStrikethrough});
      break;
    case 3:
      // Highlighter stroke behind the marked words (default translucent
      // tint of the hue-cycling foreground), spanning word gaps.
      highlight.addDecoration({.kind = Decoration::Kind::kHighlight});
      break;
    default:
      break;
    }
    m_markers.applyPaint(m_body.paragraph, "caps", highlight);

    const float canvasWidth = size.width();
    const float canvasHeight = size.height();
    const SkRect box = SkRect::MakeXYWH(canvasWidth * 0.1f, 40,
                                        canvasWidth * 0.8f, canvasHeight - 80);

    // The layout is memoized: the hue cycle repaints the same ranges every
    // frame, which leaves the span structure (and so every run's styleIndex)
    // untouched — draws resolve the new color live from the paragraph, no
    // relayout needed. Only real changes (edits, controls, resize, a marker
    // set whose boundaries moved) re-break lines, so a screenful of 10k
    // words costs its layout once, not 60× a second.
    double layoutMicroseconds = 0;
    m_layoutGuard.ensure(
        m_body.paragraph,
        {size, params.alignment, params.lineBreakStrategy, params.fontSize},
        [&] {
          BlockFlow flow(box);
          ParagraphLayoutOptions options;
          options.alignment = params.alignment;
          options.lineBreakStrategy = params.lineBreakStrategy;
          options.lineMetrics.height = params.fontSize * 1.8f;

          const kit::Stopwatch layoutTime;
          m_layout =
              layoutParagraph(fontContext, m_body.paragraph, flow, options);
          layoutMicroseconds = layoutTime.microseconds();

          // Remember how much text actually landed: the next re-query (text
          // edit) scopes itself to this window.
          m_placedTextEnd =
              m_layout.overflowed()
                  ? m_body.paragraph.words()[m_layout.firstUnplacedWord]
                        .textBegin
                  : static_cast<uint32_t>(m_body.paragraph.text().size());
        });

    canvas->clear(kPaper);
    // The breaker already stops placing words once the box is full, but a
    // hard clip guards the last visible line's descenders/overshoot too —
    // type or paste past capacity and the box still never bleeds into the
    // caption below.
    canvas->save();
    canvas->clipRect(box);
    m_layout.drawBatched(canvas, m_body.paragraph);
    canvas->restore();

    drawCaption(canvas, fontContext,
                m_queryWasScoped
                    ? u8"regex \\b\\p{Lu}\\p{Ll}+ → MarkerSet; query scoped to "
                      "the placed window — overflow text is never scanned"
                    : u8"regex \\b\\p{Lu}\\p{Ll}+ → MarkerSet; ranges follow "
                      "the scripted edits",
                {canvasWidth * 0.1f, canvasHeight - 30}, canvasWidth * 0.8f);
    return {layoutMicroseconds, static_cast<int>(m_layout.runs.size()), 0};
  }

private:
  BodyCache m_body;
  MarkerSet m_markers;
  sk_sp<SkTypeface> m_serif;
  ParagraphLayout m_layout; // memoized across frames (see render)
  kit::LayoutGuard<SkISize, TextAlignment, LineBreakStrategy, float>
      m_layoutGuard;
  uint32_t m_placedTextEnd = 0; // Text frontier of last layout (0 = unknown).
  bool m_queryWasScoped = false;
};

SceneDescriptor makeMarkersDescriptor() {
  SceneDescriptor descriptor;
  descriptor.name = QStringLiteral("Query & markers");
  descriptor.defaultText = markersDefaultText();
  descriptor.displayOrder = 110;
  descriptor.parameters = {
      {QStringLiteral("decoration"), QStringLiteral("Decoration"),
       SceneParameter::Type::kChoice, 1, 0, 3, {},
       {QStringLiteral("None"), QStringLiteral("Underline"),
        QStringLiteral("Strikethrough"), QStringLiteral("Highlight")}},
  };
  descriptor.make = [] { return std::make_unique<MarkersScene>(); };
  return descriptor;
}

} // namespace

REGISTER_GALLERY_SCENE(makeMarkersDescriptor())

} // namespace gallery
