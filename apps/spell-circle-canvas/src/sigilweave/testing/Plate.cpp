#include "sigilweave/advanced/Skia.h"
#include "sigilweave/testing/Plate.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkData.h>
#include <include/core/SkFontArguments.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRect.h>
#include <include/core/SkSerialProcs.h>
#include <include/core/SkString.h>
#include <include/core/SkTypeface.h>
#include <include/utils/SkNWayCanvas.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include <utility>

namespace sigil::weave::testing {

namespace {

/// The revision a face's 'head' table states, as the table writes it:
/// a 16.16 fixed-point number. Absent when the face carries no such
/// table.
std::string headRevision(const SkTypeface& face) {
  uint8_t head[8] = {};
  if (face.getTableData(SkSetFourByteTag('h', 'e', 'a', 'd'), 0, sizeof head,
                        head) != sizeof head)
    return "none";
  const uint32_t fixed = (uint32_t(head[4]) << 24) | (uint32_t(head[5]) << 16) |
                         (uint32_t(head[6]) << 8) | uint32_t(head[7]);
  char text[32];
  std::snprintf(text, sizeof text, "%.5f", double(fixed) / 65536.0);
  return text;
}

/// The variation position a face is drawn at, axis by axis, or empty for
/// a face with none.
std::string variationPosition(const SkTypeface& face) {
  const int count = face.getVariationDesignPosition({});
  if (count <= 0) return {};
  std::vector<SkFontArguments::VariationPosition::Coordinate> coordinates(
      static_cast<size_t>(count));
  if (face.getVariationDesignPosition(coordinates) != count) return {};
  std::string text;
  for (const auto& coordinate : coordinates) {
    const SkFourByteTag tag = coordinate.axis;
    const char name[5] = {char(tag >> 24), char(tag >> 16), char(tag >> 8),
                          char(tag), 0};
    char value[32];
    std::snprintf(value, sizeof value, "%.3f", double(coordinate.value));
    text += std::string(text.empty() ? "" : " ") + name + "=" + value;
  }
  return text;
}

/// One face as the line a plate lists it by.
std::string identity(const SkTypeface& face) {
  SkString family;
  face.getFamilyName(&family);
  SkString postScript;
  if (!face.getPostScriptName(&postScript)) postScript = "?";
  const SkFontStyle style = face.fontStyle();
  std::string line = std::string(family.c_str()) + " | " + postScript.c_str() +
                     " | weight " + std::to_string(style.weight()) + " width " +
                     std::to_string(style.width()) + " slant " +
                     std::to_string(int(style.slant())) + " | revision " +
                     headRevision(face);
  if (const std::string position = variationPosition(face); !position.empty())
    line += " | " + position;
  return line;
}

/// Collects every typeface a picture names as its serialization asks
/// for them, and writes none of them.
SkSerialReturnType collectFace(SkTypeface* face, void* context) {
  if (face)
    static_cast<std::set<std::string>*>(context)->insert(identity(*face));
  return SkData::MakeEmpty();
}

SkRect boundsOf(SkISize size) {
  return SkRect::MakeIWH(std::max(size.width(), 1), std::max(size.height(), 1));
}

}  // namespace

Plate::Plate(SkISize size, SkColor ground)
    : m_surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size))),
      m_witness(std::make_unique<SkPictureRecorder>()),
      m_canvas(std::make_unique<SkNWayCanvas>(size.width(), size.height())),
      m_faces(std::make_unique<std::set<std::string>>()) {
  if (m_surface) {
    m_surface->getCanvas()->clear(ground);
    m_canvas->addCanvas(m_surface->getCanvas());
  }
  m_canvas->addCanvas(m_witness->beginRecording(boundsOf(size)));
}

Plate::~Plate() = default;
Plate::Plate(Plate&&) noexcept = default;
Plate& Plate::operator=(Plate&&) noexcept = default;

SkCanvas* Plate::canvas() const { return m_canvas.get(); }

void Plate::draw(const Passage& passage) const {
  passage.layout.draw(canvas(), passage.paragraph);
}

SkPixmap Plate::pixels() const {
  SkPixmap pixmap;
  if (m_surface) m_surface->peekPixels(&pixmap);
  return pixmap;
}

sk_sp<SkImage> Plate::image() const {
  return m_surface ? m_surface->makeImageSnapshot() : nullptr;
}

SkISize Plate::size() const {
  return m_surface ? SkISize{m_surface->width(), m_surface->height()}
                   : SkISize{0, 0};
}

std::vector<std::string> Plate::faces() const {
  // What was witnessed so far is folded into the faces already known and
  // the witness starts again empty, so asking twice answers the same
  // faces and a draw after the question is still witnessed.
  const sk_sp<SkPicture> witnessed = m_witness->finishRecordingAsPicture();
  if (witnessed) {
    SkSerialProcs procs;
    procs.fTypefaceProc = collectFace;
    procs.fTypefaceCtx = m_faces.get();
    witnessed->serialize(&procs);
  }
  m_canvas->removeAll();
  if (m_surface) m_canvas->addCanvas(m_surface->getCanvas());
  m_canvas->addCanvas(m_witness->beginRecording(boundsOf(size())));
  return {m_faces->begin(), m_faces->end()};
}

sk_sp<SkImage> render(const Passage& passage, SkISize size, SkColor ground) {
  Plate plate(size, ground);
  plate.draw(passage);
  return plate.image();
}

}  // namespace sigil::weave::testing
