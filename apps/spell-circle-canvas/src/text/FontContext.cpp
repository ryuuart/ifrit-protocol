#include "FontContextImpl.h"

#include <include/core/SkData.h>
#include <include/core/SkFontStyle.h>

namespace textflow {

namespace {

// hb_face table provider backed by SkTypeface::copyTableData. hb_tag_t and
// SkFontTableTag share the same big-endian four-cc encoding.
hb_blob_t *getTable(hb_face_t *, hb_tag_t tag, void *ctx) {
  auto *typeface = static_cast<SkTypeface *>(ctx);
  sk_sp<SkData> data = typeface->copyTableData(tag);
  if (!data)
    return nullptr;
  SkData *raw = data.release();
  return hb_blob_create(static_cast<const char *>(raw->data()), raw->size(),
                        HB_MEMORY_MODE_READONLY, raw,
                        [](void *p) { static_cast<SkData *>(p)->unref(); });
}

} // namespace

FontContext::Impl::FaceRec &
FontContext::Impl::faceFor(const sk_sp<SkTypeface> &typeface) {
  auto [it, inserted] = faces.try_emplace(typeface->uniqueID());
  FaceRec &rec = it->second;
  if (!inserted)
    return rec;

  rec.typeface = typeface;
  rec.face = hb_face_create_for_tables(getTable, typeface.get(), nullptr);
  rec.upem = typeface->getUnitsPerEm();
  if (rec.upem <= 0)
    rec.upem = 1000;
  hb_face_set_upem(rec.face, rec.upem);
  rec.font = hb_font_create(rec.face);
  hb_font_set_scale(rec.font, rec.upem, rec.upem);
  return rec;
}

FontContext::Impl::~Impl() {
  for (auto &[id, rec] : faces) {
    if (rec.font)
      hb_font_destroy(rec.font);
    if (rec.face)
      hb_face_destroy(rec.face);
  }
  if (buffer)
    hb_buffer_destroy(buffer);
  if (lineBreaker)
    ubrk_close(lineBreaker);
}

FontContext::FontContext(sk_sp<SkFontMgr> fontMgr,
                         sk_sp<SkTypeface> defaultTypeface)
    : m_impl(std::make_unique<Impl>()) {
  m_impl->fontMgr = std::move(fontMgr);
  m_impl->defaultTypeface = std::move(defaultTypeface);
  m_impl->buffer = hb_buffer_create();
}

FontContext::~FontContext() = default;

SkFontMgr *FontContext::fontMgr() const { return m_impl->fontMgr.get(); }

const sk_sp<SkTypeface> &FontContext::defaultTypeface() const {
  if (!m_impl->defaultTypeface && m_impl->fontMgr) {
    m_impl->defaultTypeface =
        m_impl->fontMgr->matchFamilyStyle(nullptr, SkFontStyle());
    if (!m_impl->defaultTypeface)
      m_impl->defaultTypeface =
          m_impl->fontMgr->legacyMakeTypeface(nullptr, SkFontStyle());
  }
  return m_impl->defaultTypeface;
}

sk_sp<SkTypeface> FontContext::resolveTypeface(const sk_sp<SkTypeface> &primary,
                                               int32_t codepoint,
                                               const char *language) {
  const sk_sp<SkTypeface> &base = primary ? primary : defaultTypeface();
  if (!base)
    return nullptr;

  // ASCII fast path: direct-mapped table per primary, memoizing the last
  // primary used. Entries borrow the sk_sp held by the `fallback` map (which
  // is never pruned), so the raw pointers stay valid.
  const bool ascii = codepoint >= 0 && codepoint < 128;
  if (ascii) {
    if (m_impl->lastAsciiID != base->uniqueID() || !m_impl->lastAsciiTable) {
      m_impl->lastAsciiID = base->uniqueID();
      m_impl->lastAsciiTable = &m_impl->asciiFallback[base->uniqueID()];
    }
    if (SkTypeface *hit = (*m_impl->lastAsciiTable)[codepoint])
      return sk_ref_sp(hit);
  }

  const uint64_t key = (static_cast<uint64_t>(base->uniqueID()) << 32) |
                       static_cast<uint32_t>(codepoint);
  auto it = m_impl->fallback.find(key);
  if (it != m_impl->fallback.end())
    return it->second;

  m_impl->stats.coverageQueries++;
  sk_sp<SkTypeface> resolved = base;
  if (base->unicharToGlyph(codepoint) == 0) {
    m_impl->stats.fallbackQueries++;
    const char *bcp47[1] = {language};
    const int bcp47Count = (language && *language) ? 1 : 0;
    sk_sp<SkTypeface> match = m_impl->fontMgr->matchFamilyStyleCharacter(
        nullptr, base->fontStyle(), bcp47Count ? bcp47 : nullptr, bcp47Count,
        codepoint);
    if (match && match->unicharToGlyph(codepoint) != 0)
      resolved = std::move(match);
  }
  auto [inserted, _] = m_impl->fallback.emplace(key, resolved);
  if (ascii)
    (*m_impl->lastAsciiTable)[codepoint] = inserted->second.get();
  return resolved;
}

void FontContext::purgeShapeCache() { m_impl->shapeCache.clear(); }

const FontContext::Stats &FontContext::stats() const { return m_impl->stats; }

void FontContext::resetStats() { m_impl->stats = {}; }

} // namespace textflow
