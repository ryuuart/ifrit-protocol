#include "TexturePreview.h"
#include "texture/Capture.h"

#import <Metal/Metal.h>
#include <QtQuick/qsgtexture_platform.h>
#include <sigilio/publish/Subscription.h>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>
#include <QtQuick/QSGSimpleTextureNode>
#include <memory>
#include <utility>

namespace {
// The scene graph owns all native references, including the frame borrowed
// from the subscription. The texture wrapper is destroyed before its image.
class TextureNode final : public QSGNode {
 public:
  ~TextureNode() override {
    if (image) {
      delete image->texture();
      delete image;
    }
  }
  QSGSimpleTextureNode* image = nullptr;
  QString source;
  QString application;
  std::unique_ptr<sigil::io::publish::Subscription> subscription;
  id<MTLTexture> frame = nil;
  id<MTLCommandQueue> queue = nil;
  uint64_t generation = 0;
  void replace(QSGTexture* next) {
    if (!image) {
      image = new QSGSimpleTextureNode;
      image->setFiltering(QSGTexture::Linear);
      // A RECEIVED FRAME IS THE CARRIED SURFACE ITSELF, whose first row
      // is the bottom of the picture, and this item's own space has the
      // first row at the top. Nothing is copied to turn it: the node
      // samples the same texture the other way up.
      image->setTextureCoordinatesTransform(QSGSimpleTextureNode::MirrorVertically);
      appendChildNode(image);
    }
    auto* previous = image->texture();
    image->setTexture(next);
    delete previous;
  }
};
}  // namespace

QSGNode* TexturePreview::updatePaintNode(QSGNode* old, UpdatePaintNodeData*) {
  @autoreleasepool {
    auto* node = static_cast<TextureNode*>(old);
    if (source().isEmpty()) {
      delete node;
      return nullptr;
    }
    auto* renderer = window()->rendererInterface();
    if (renderer->graphicsApi() != QSGRendererInterface::Metal) {
      delete node;
      reportFrame(false, {}, 0,
                  QStringLiteral("Select the Metal graphics backend to receive textures."));
      return nullptr;
    }
    if (node && (node->source != source() || node->application != application())) {
      delete node;
      node = nullptr;
    }
    if (!node) {
      node = new TextureNode;
      node->source = source();
      node->application = application();
      void* device = renderer->getResource(window(), QSGRendererInterface::DeviceResource);
      node->subscription = sigil::io::publish::subscribe(source().toStdString(),
                                                         application().toStdString(), device);
      node->queue = [(__bridge id<MTLDevice>)device newCommandQueue];
    }
    if (!node->subscription) {
      delete node;
      reportFrame(false, {}, 0, QStringLiteral("The texture subscription could not start."));
      return nullptr;
    }
    if (!paused() || !node->frame) {
      id<MTLTexture> next = (__bridge id<MTLTexture>)node->subscription->newestFrame();
      const auto generation = node->subscription->generation();
      if (next && (generation != node->generation || !node->frame)) {
        auto* texture = QNativeInterface::QSGMetalTexture::fromNative(
            next, window(), QSize(static_cast<int>(next.width), static_cast<int>(next.height)),
            QQuickWindow::TextureHasAlphaChannel);
        if (texture) {
          node->replace(texture);
          node->frame = next;
          node->generation = generation;
        }
      }
    }
    const bool connected = node->subscription->standing();
    const QSize size = node->frame ? QSize(static_cast<int>(node->frame.width),
                                           static_cast<int>(node->frame.height))
                                   : QSize{};
    reportFrame(
        connected, size, node->generation,
        connected ? QStringLiteral("Receiving texture") : QStringLiteral("Waiting for publisher"));
    if (!m_capturePath.isEmpty()) {
      const auto path = std::exchange(m_capturePath, {});
      const bool success =
          node->frame && seer::texture::writeTexturePng(node->frame, node->queue,
                                                        path.toStdString(),
                                                        seer::texture::Rows::BottomFirst);
      QMetaObject::invokeMethod(
          this, [this, path, success] { emit saved(path, success); }, Qt::QueuedConnection);
    }
    if (node->image) node->image->setRect(boundingRect());
    return node;
  }
}
