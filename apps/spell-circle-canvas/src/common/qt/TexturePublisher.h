#pragma once

#include <sigilio/publish/Publisher.h>

#include <QtCore/QSize>
#include <memory>
#include <string>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

namespace ifrit::qt {

/** Opens the native publisher supported by this QRhi backend, or returns
 * null. The publisher must be destroyed before QRhi's device. */
std::unique_ptr<sigil::io::publish::Publisher> createPublisher(
    QRhi* rhi, std::string name);

/** Publishes a texture from the QRhi that created the publisher. Drawing
 * must already be submitted; Qt commits the still-open command buffer.
 * The newest image remains available to clients that subscribe later. */
void publishFrame(sigil::io::publish::Publisher& publisher,
                  QRhiTexture* texture, QRhiCommandBuffer* commandBuffer,
                  QSize size);

}  // namespace ifrit::qt
