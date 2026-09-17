#include "TextureSources.h"

#include <sigilio/publish/Subscription.h>

#include <algorithm>
#include <tuple>

TextureSources::TextureSources(QObject* parent) : QObject(parent) {
  connect(&m_discovery, &QTimer::timeout, this, &TextureSources::refresh);
  m_discovery.start(500);
  refresh();
}

bool TextureSources::supported() const {
#ifdef Q_OS_MACOS
  return true;
#else
  return false;
#endif
}

void TextureSources::refresh() {
  auto sources = sigil::io::publish::publications();
  std::sort(sources.begin(), sources.end(), [](const auto& a, const auto& b) {
    return std::tie(a.application, a.name) < std::tie(b.application, b.name);
  });
  QVariantList next;
  for (const auto& source : sources)
    next.append(QVariantMap{
        {"name", QString::fromStdString(source.name)},
        {"application", QString::fromStdString(source.application)}});
  if (m_publications == next) return;
  m_publications = std::move(next);
  emit changed();
}
