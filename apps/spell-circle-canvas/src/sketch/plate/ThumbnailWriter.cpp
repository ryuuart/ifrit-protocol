/** @file
 * The still writer: one worker, one encode in flight, and the asks
 * waiting behind it.
 */

#include <include/core/SkData.h>
#include <sigilimage/encode/Encode.h>
#include <sigilio/source/Sink.h>
#include <sigilsketch/plate/ThumbnailWriter.h>
#include <sigilsketch/plate/Thumbnails.h>

#include <system_error>
#include <utility>

namespace sigil::sketch {

ThumbnailWriter::ThumbnailWriter(Wrote wrote)
    : m_wrote(std::move(wrote)), m_worker(&ThumbnailWriter::loop, this) {}

ThumbnailWriter::~ThumbnailWriter() {
  {
    const std::lock_guard lock(m_mutex);
    m_stop = true;
    m_pending.clear();
  }
  m_wake.notify_all();
  if (m_worker.joinable()) m_worker.join();
}

void ThumbnailWriter::write(int index, SkBitmap still,
                            std::filesystem::path outputPath,
                            std::filesystem::path storeDirectory,
                            std::string stem) {
  if (still.isNull()) return;
  {
    const std::lock_guard lock(m_mutex);
    if (m_stop) return;
    m_pending.push_back({index, std::move(still), std::move(outputPath),
                         std::move(storeDirectory), std::move(stem)});
  }
  m_wake.notify_one();
}

std::size_t ThumbnailWriter::pending() const {
  const std::lock_guard lock(m_mutex);
  return m_pending.size();
}

bool ThumbnailWriter::writing() const {
  const std::lock_guard lock(m_mutex);
  return m_writing;
}

void ThumbnailWriter::drain() {
  std::unique_lock lock(m_mutex);
  m_idle.wait(lock, [this] { return m_pending.empty() && !m_writing; });
}

void ThumbnailWriter::loop() {
  for (;;) {
    Ask ask;
    {
      std::unique_lock lock(m_mutex);
      m_wake.wait(lock, [this] { return m_stop || !m_pending.empty(); });
      if (m_stop) return;
      ask = std::move(m_pending.front());
      m_pending.pop_front();
      m_writing = true;
    }

    std::error_code code;
    std::filesystem::create_directories(ask.outputPath.parent_path(), code);
    const sk_sp<SkData> png =
        image::encodeImage(ask.still.pixmap(), image::Format::Png);
    const bool wrote =
        png && io::writeBytes(ask.outputPath, png->data(), png->size());
    if (wrote) pruneThumbnails(ask.storeDirectory, ask.stem, ask.outputPath);

    {
      const std::lock_guard lock(m_mutex);
      m_writing = false;
    }
    m_idle.notify_all();
    // Reported after the file is on disk, so a listener that reads it
    // finds it there. A still that could not be written says nothing
    // about the sketch and nothing is claimed for it.
    if (wrote && m_wrote) m_wrote(ask.index);
  }
}

}  // namespace sigil::sketch
