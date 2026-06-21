#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include "SyphonBridge.h"
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

struct SyphonBridgePrivate {
  SyphonMetalServer *server = nil;
};

SyphonBridge::SyphonBridge(std::string name)
    : m_name(std::move(name)),
      m_syphon_bridge(std::make_unique<SyphonBridgePrivate>()) {}

SyphonBridge::~SyphonBridge() { stop(); }

void SyphonBridge::start(QRhi *rhi) {
  auto *h = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // qrhi_platform.h forward-declares MTLDevice as an opaque C struct; __bridge recasts without touching ARC ownership.
  id<MTLDevice> device = (__bridge id<MTLDevice>)h->dev;

  m_syphon_bridge->server =
      [[SyphonMetalServer alloc] initWithName:@(m_name.c_str())
                                       device:device
                                      options:nil];
}

void SyphonBridge::publishFrame(QRhiTexture *texture, QRhiCommandBuffer *cb,
                                int width, int height) {
  if (!m_syphon_bridge->server || !m_syphon_bridge->server.hasClients)
    return;

  auto nativeTex = texture->nativeTexture();
  // Qt packs the id<MTLTexture> pointer into a quint64 on Metal.
  id<MTLTexture> mtlTex =
      (__bridge id<MTLTexture>)reinterpret_cast<void *>(nativeTex.object);

  auto *cbH = static_cast<const QRhiMetalCommandBufferNativeHandles *>(
      cb->nativeHandles());
  id<MTLCommandBuffer> mtlCb =
      (__bridge id<MTLCommandBuffer>)cbH->commandBuffer;

  // Syphon appends a blit to the still-open command buffer; Qt commits it after render() returns.
  [m_syphon_bridge->server publishFrameTexture:mtlTex
                               onCommandBuffer:mtlCb
                                   imageRegion:NSMakeRect(0, 0, width, height)
                                       flipped:NO];
}

void SyphonBridge::stop() {
  [m_syphon_bridge->server stop];
  m_syphon_bridge->server = nil;
}
