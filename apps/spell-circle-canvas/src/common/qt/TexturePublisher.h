#pragma once

/** @file
 * Publishing a texture out of the Qt renderer: the publisher for the
 * backend a QRhi is running, and the one call that offers a frame it
 * has just drawn.
 */

#include <sigilio/frames/Publisher.h>
#include <sigilio/hub/Hub.h>

#include <QtCore/QSize>
#include <memory>
#include <string>
#include <string_view>

class QRhi;
class QRhiTexture;
class QRhiCommandBuffer;

namespace ifrit::qt {

/** Publishes under @p name through @p hub on the device this QRhi
 * backend draws on — Syphon over Metal, Spout over Direct3D11 — or
 * returns an empty handle. The publisher must be destroyed before QRhi's
 * device. */
sigil::io::frames::Publisher createPublisher(sigil::io::Hub& hub, QRhi* rhi,
                                             std::string_view name);

/** Publishes a texture from the QRhi that created the publisher. Drawing
 * must already be submitted; Qt commits the still-open command buffer.
 * The newest image remains available to clients that subscribe later. */
void publishFrame(sigil::io::frames::Publisher& publisher,
                  QRhiTexture* texture, QRhiCommandBuffer* commandBuffer,
                  QSize size);

}  // namespace ifrit::qt
