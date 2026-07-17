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
      m_private(std::make_unique<SyphonBridgePrivate>()) {}

SyphonBridge::~SyphonBridge() { stop(); }

void SyphonBridge::start(QRhi *rhi) {
  if (!rhi || rhi->backend() != QRhi::Metal)
    return;
  stop(); // restarting: never orphan a previous server

  const auto *nativeHandles =
      static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
  // QRhi declares MTLDevice as opaque; __bridge recasts it without changing
  // ARC ownership.
  id<MTLDevice> device = (__bridge id<MTLDevice>)nativeHandles->dev;

  m_private->server = [[SyphonMetalServer alloc] initWithName:@(m_name.c_str())
                                                       device:device
                                                      options:nil];
}

void SyphonBridge::publishFrame(QRhiTexture *texture,
                                QRhiCommandBuffer *commandBuffer, int width,
                                int height) {
  if (!m_private->server || !m_private->server.hasClients)
    return;

  auto nativeTex = texture->nativeTexture();
  // Qt packs the id<MTLTexture> pointer into a quint64 on Metal.
  id<MTLTexture> metalTexture =
      (__bridge id<MTLTexture>)reinterpret_cast<void *>(nativeTex.object);

  const auto *commandBufferHandles =
      static_cast<const QRhiMetalCommandBufferNativeHandles *>(
          commandBuffer->nativeHandles());
  id<MTLCommandBuffer> metalCommandBuffer =
      (__bridge id<MTLCommandBuffer>)commandBufferHandles->commandBuffer;

  // Syphon appends a blit to the still-open command buffer. Qt commits it
  // after render() returns.
  [m_private->server publishFrameTexture:metalTexture
                         onCommandBuffer:metalCommandBuffer
                             imageRegion:NSMakeRect(0, 0, width, height)
                                 flipped:YES];
}

void SyphonBridge::stop() {
  // This file compiles without ARC: alloc/init in start() left us a +1
  // reference, so a plain `= nil` here would leak the server on every
  // stop/restart cycle. Release explicitly.
  [m_private->server stop];
  [m_private->server release];
  m_private->server = nil;
}

std::unique_ptr<TexturePublisher> createTexturePublisher(QRhi *rhi,
                                                         std::string name) {
  if (!rhi || rhi->backend() != QRhi::Metal)
    return nullptr;
  auto bridge = std::make_unique<SyphonBridge>(std::move(name));
  bridge->start(rhi);
  return bridge;
}
