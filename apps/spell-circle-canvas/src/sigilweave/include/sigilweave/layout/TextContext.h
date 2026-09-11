#pragma once

/** @file
 * Single-style text measured and laid out through a per-thread service.
 * Paragraph reuse is configured on the service; callers supply content,
 * style and geometry and receive a result that owns its text.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>

#include "sigilweave/fonts/FontContext.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

/** Resource limits for one text context. */
struct TextContextOptions {
  /// Maximum retained paragraphs. Zero disables paragraph retention.
  /// Least recently used entries are released first. Results held by a
  /// caller remain alive independently of this limit.
  size_t paragraphCacheEntries = 1024;
};

/** A layout and the immutable text it was laid out from.
 * Copies share the text. Eviction, subsequent layout calls and destruction
 * of the context cannot invalidate this result. Link SigilWeavePaint to
 * use the draw members, as with ParagraphLayout.
 */
class TextLayout {
 public:
  [[nodiscard]] const Paragraph& paragraph() const { return *m_paragraph; }
  [[nodiscard]] const ParagraphLayout& layout() const { return m_layout; }

  /** Draws with the supplied style, or an optional paint override. */
  void draw(SkCanvas* canvas, const PaintStyle* overridePaint = nullptr) const {
    m_layout.draw(canvas, *m_paragraph, overridePaint);
  }
  /** Batches compatible runs, retaining the same paint behavior as draw(). */
  void drawBatched(SkCanvas* canvas,
                   const PaintStyle* overridePaint = nullptr) const {
    m_layout.drawBatched(canvas, *m_paragraph, overridePaint);
  }

 private:
  friend class TextContext;
  TextLayout(std::shared_ptr<Paragraph> paragraph, ParagraphLayout layout)
      : m_paragraph(std::move(paragraph)), m_layout(std::move(layout)) {}

  std::shared_ptr<const Paragraph> m_paragraph;
  ParagraphLayout m_layout;
};

/** Per-thread text service with internally owned paragraph reuse.
 * One instance serves any number of labels or single-style passages.
 * Exact text and shaping style identify an entry; paint changes reuse its
 * analysis. Geometry is queried on every layout call. A result still held
 * by a caller is isolated before another call changes its paragraph.
 *
 * Editable or mixed-style documents use Paragraph and layoutParagraph
 * directly. This context owns no canonical layout of such a document.
 */
class TextContext {
 public:
  /** Owns a font context using the supplied manager and fallback policy. */
  explicit TextContext(sk_sp<SkFontMgr> fontManager,
                       TextContextOptions options = {},
                       sk_sp<SkTypeface> defaultTypeface = nullptr,
                       FontContext::FallbackResolver fallbackResolver = {});
  /** Reuses an existing font service, which must outlive this context. */
  explicit TextContext(FontContext& fonts, TextContextOptions options = {});
  ~TextContext();
  TextContext(const TextContext&) = delete;
  TextContext& operator=(const TextContext&) = delete;

  /** The lower-level font service used by every operation here. */
  [[nodiscard]] FontContext& fonts();

  /** Measures unwrapped horizontal text, excluding trailing whitespace. */
  [[nodiscard]] float naturalWidth(std::u8string_view text,
                                   const TextStyle& style);
  [[nodiscard]] float naturalWidth(std::u16string_view text,
                                   const TextStyle& style);

  /** Lays horizontal text into any compatible flow, including a contour. */
  [[nodiscard]] TextLayout layout(std::u8string_view text,
                                  const TextStyle& style, FlowGeometry& flow,
                                  const ParagraphLayoutOptions& options = {});
  [[nodiscard]] TextLayout layout(std::u16string_view text,
                                  const TextStyle& style, FlowGeometry& flow,
                                  const ParagraphLayoutOptions& options = {});

  /** Places one unconstrained horizontal line at the supplied baseline. */
  [[nodiscard]] TextLayout singleLine(std::u8string_view text,
                                      const TextStyle& style, SkPoint baseline,
                                      const PathTextOptions& options = {});
  [[nodiscard]] TextLayout singleLine(std::u16string_view text,
                                      const TextStyle& style, SkPoint baseline,
                                      const PathTextOptions& options = {});

  /** Releases retained paragraphs; outstanding results remain valid.
   * Font caches are independent and are controlled through fonts(). */
  void purgeParagraphs();

  /** Paragraph reuse counters for tests and benchmarks. */
  struct Stats {
    uint64_t paragraphBuilds = 0;
    uint64_t paragraphCacheHits = 0;
    size_t paragraphEntries = 0;
  };
  [[nodiscard]] Stats stats() const;
  /** Resets cumulative counters without releasing retained paragraphs. */
  void resetStats();

 private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::weave
