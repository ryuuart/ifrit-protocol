#import <Metal/Metal.h>

#include "UltralightMetalDriver.h"
#include "UltralightShaderSource.h"

#include <Ultralight/Matrix.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/ContextOptions.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <include/gpu/graphite/mtl/MtlBackendContext.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <simd/simd.h>

#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ifrit::web {

namespace {

// Mirrors the Uniforms constant buffer in UltralightShaders.metal
// (State = [snap-enabled, screen-width, screen-height, screen-scale]).
struct Uniforms {
  simd::float4 State;
  simd::float4x4 Transform;
  simd::float4 Scalar4[2];
  simd::float4 Vector[8];
  unsigned int ClipSize;
  simd::float4x4 Clip[8];
};

simd::float4x4 toSimd(const ultralight::Matrix4x4 &m) {
  // Matrix4x4::data is column-major: each run of 4 floats is one column.
  return simd_matrix(
      simd_make_float4(m.data[0], m.data[1], m.data[2], m.data[3]),
      simd_make_float4(m.data[4], m.data[5], m.data[6], m.data[7]),
      simd_make_float4(m.data[8], m.data[9], m.data[10], m.data[11]),
      simd_make_float4(m.data[12], m.data[13], m.data[14], m.data[15]));
}

ultralight::Matrix4x4 projectedTransform(const ultralight::GPUState &state) {
  ultralight::Matrix transform;
  transform.Set(state.transform);
  ultralight::Matrix result;
  // No y-flip: Ultralight's texture convention already matches Metal's
  // top-left origin (flipping here double-flips the composited page).
  result.SetOrthographicProjection(state.viewport_width, state.viewport_height,
                                   /*flip_y=*/false);
  result.Transform(transform);
  return result.GetMatrix4x4();
}

} // namespace

struct UltralightMetalDriver::State {
  id<MTLDevice> device = nil;
  id<MTLCommandQueue> queue = nil;
  // Pipeline states indexed [shader type][blending enabled].
  id<MTLRenderPipelineState> pipelines[2][2] = {{nil, nil}, {nil, nil}};

  std::unordered_map<uint32_t, id<MTLTexture>> textures;

  struct RenderBufferEntry {
    uint32_t textureId = 0;
  };
  std::unordered_map<uint32_t, RenderBufferEntry> renderBuffers;

  struct GeometryEntry {
    id<MTLBuffer> vertices = nil;
    id<MTLBuffer> indices = nil;
  };
  std::unordered_map<uint32_t, GeometryEntry> geometry;

  std::vector<ultralight::Command> commands;
  std::unordered_set<uint32_t> pendingClear;

  // The driver's own Graphite context on the shared device/queue, used by
  // paintTexture() so WebImage::paint() needs no caller-side plumbing.
  std::unique_ptr<skgpu::graphite::Context> skiaContext;
  std::unique_ptr<skgpu::graphite::Recorder> skiaRecorder;

  uint32_t nextTextureId = 1;
  uint32_t nextRenderBufferId = 1;
  uint32_t nextGeometryId = 1;
};

std::unique_ptr<UltralightMetalDriver>
UltralightMetalDriver::create(void *mtlDevice, void *mtlCommandQueue) {
  auto state = std::make_unique<State>();
  state->device = (__bridge id<MTLDevice>)mtlDevice;
  state->queue = (__bridge id<MTLCommandQueue>)mtlCommandQueue;

  NSError *error = nil;
  id<MTLLibrary> library = [state->device
      newLibraryWithSource:[NSString stringWithUTF8String:kUltralightShaderSource]
                   options:nil
                     error:&error];
  if (!library) {
    std::fprintf(stderr, "[IfritWeb:error] Ultralight shader compile: %s\n",
                 error.localizedDescription.UTF8String);
    return nullptr;
  }

  struct {
    NSString *vertex;
    NSString *fragment;
  } entryPoints[2] = {
      {@"vertexShader", @"fragmentShader"},         // ShaderType::Fill
      {@"pathVertexShader", @"pathFragmentShader"}, // ShaderType::FillPath
  };

  for (int shader = 0; shader < 2; ++shader) {
    id<MTLFunction> vertexFn =
        [library newFunctionWithName:entryPoints[shader].vertex];
    id<MTLFunction> fragmentFn =
        [library newFunctionWithName:entryPoints[shader].fragment];
    for (int blend = 0; blend < 2; ++blend) {
      MTLRenderPipelineDescriptor *desc =
          [[MTLRenderPipelineDescriptor alloc] init];
      desc.vertexFunction = vertexFn;
      desc.fragmentFunction = fragmentFn;
      MTLRenderPipelineColorAttachmentDescriptor *color =
          desc.colorAttachments[0];
      color.pixelFormat = MTLPixelFormatBGRA8Unorm;
      color.blendingEnabled = blend != 0;
      // Ultralight's premultiplied-alpha blend equation (matches the
      // stock AppCore Metal driver).
      color.rgbBlendOperation = MTLBlendOperationAdd;
      color.alphaBlendOperation = MTLBlendOperationAdd;
      color.sourceRGBBlendFactor = MTLBlendFactorOne;
      color.sourceAlphaBlendFactor = MTLBlendFactorOneMinusDestinationAlpha;
      color.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      color.destinationAlphaBlendFactor = MTLBlendFactorOne;

      state->pipelines[shader][blend] =
          [state->device newRenderPipelineStateWithDescriptor:desc
                                                        error:&error];
      if (!state->pipelines[shader][blend]) {
        std::fprintf(stderr, "[IfritWeb:error] Ultralight pipeline: %s\n",
                     error.localizedDescription.UTF8String);
        return nullptr;
      }
    }
  }

  skgpu::graphite::MtlBackendContext backendContext;
  backendContext.fDevice.reset(CFRetain((__bridge CFTypeRef)state->device));
  backendContext.fQueue.reset(CFRetain((__bridge CFTypeRef)state->queue));
  state->skiaContext =
      skgpu::graphite::ContextFactory::MakeMetal(backendContext, {});
  if (state->skiaContext)
    state->skiaRecorder = state->skiaContext->makeRecorder();
  if (!state->skiaRecorder)
    std::fprintf(stderr, "[IfritWeb:warning] Graphite bring-up for "
                         "WebImage::paint() failed; paint() will no-op\n");

  return std::unique_ptr<UltralightMetalDriver>(
      new UltralightMetalDriver(std::move(state)));
}

UltralightMetalDriver::UltralightMetalDriver(std::unique_ptr<State> state)
    : m_state(std::move(state)) {}

UltralightMetalDriver::~UltralightMetalDriver() = default;

uint32_t UltralightMetalDriver::NextTextureId() {
  return m_state->nextTextureId++;
}

void UltralightMetalDriver::CreateTexture(
    uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
  const bool isRenderTarget = bitmap->IsEmpty();
  MTLPixelFormat format =
      bitmap->format() == ultralight::BitmapFormat::A8_UNORM
          ? MTLPixelFormatA8Unorm
          : MTLPixelFormatBGRA8Unorm;
  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:isRenderTarget
                                             ? MTLPixelFormatBGRA8Unorm
                                             : format
                                   width:bitmap->width()
                                  height:bitmap->height()
                               mipmapped:NO];
  if (isRenderTarget) {
    desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModePrivate;
  } else {
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeShared;
  }

  id<MTLTexture> texture = [m_state->device newTextureWithDescriptor:desc];
  m_state->textures[textureId] = texture;

  if (!isRenderTarget)
    UpdateTexture(textureId, bitmap);
}

void UltralightMetalDriver::UpdateTexture(
    uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
  auto it = m_state->textures.find(textureId);
  if (it == m_state->textures.end())
    return;
  auto pixels = bitmap->LockPixelsSafe();
  if (!pixels || !pixels.data())
    return;
  [it->second replaceRegion:MTLRegionMake2D(0, 0, bitmap->width(),
                                            bitmap->height())
                mipmapLevel:0
                  withBytes:pixels.data()
                bytesPerRow:bitmap->row_bytes()];
}

void UltralightMetalDriver::DestroyTexture(uint32_t textureId) {
  m_state->textures.erase(textureId);
}

uint32_t UltralightMetalDriver::NextRenderBufferId() {
  return m_state->nextRenderBufferId++;
}

void UltralightMetalDriver::CreateRenderBuffer(
    uint32_t renderBufferId, const ultralight::RenderBuffer &buffer) {
  m_state->renderBuffers[renderBufferId] = {buffer.texture_id};
}

void UltralightMetalDriver::DestroyRenderBuffer(uint32_t renderBufferId) {
  m_state->renderBuffers.erase(renderBufferId);
  m_state->pendingClear.erase(renderBufferId);
}

uint32_t UltralightMetalDriver::NextGeometryId() {
  return m_state->nextGeometryId++;
}

void UltralightMetalDriver::CreateGeometry(
    uint32_t geometryId, const ultralight::VertexBuffer &vertices,
    const ultralight::IndexBuffer &indices) {
  UpdateGeometry(geometryId, vertices, indices);
}

void UltralightMetalDriver::UpdateGeometry(
    uint32_t geometryId, const ultralight::VertexBuffer &vertices,
    const ultralight::IndexBuffer &indices) {
  // Fresh buffers every update: in-flight command buffers retain the old
  // ones until the GPU is done with them, so this is race-free without a
  // ring allocator.
  State::GeometryEntry entry;
  entry.vertices = [m_state->device newBufferWithBytes:vertices.data
                                                length:vertices.size
                                               options:MTLResourceStorageModeShared];
  entry.indices = [m_state->device newBufferWithBytes:indices.data
                                               length:indices.size
                                              options:MTLResourceStorageModeShared];
  m_state->geometry[geometryId] = entry;
}

void UltralightMetalDriver::DestroyGeometry(uint32_t geometryId) {
  m_state->geometry.erase(geometryId);
}

void UltralightMetalDriver::UpdateCommandList(
    const ultralight::CommandList &list) {
  m_state->commands.insert(m_state->commands.end(), list.commands,
                           list.commands + list.size);
}

std::unordered_set<uint32_t> UltralightMetalDriver::flush() {
  std::unordered_set<uint32_t> dirtyRenderBuffers;
  if (m_state->commands.empty())
    return dirtyRenderBuffers;

  @autoreleasepool {
    id<MTLCommandBuffer> commandBuffer = [m_state->queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = nil;
    uint32_t encoderRenderBuffer = 0;

    auto endEncoder = [&] {
      if (encoder) {
        [encoder endEncoding];
        encoder = nil;
        encoderRenderBuffer = 0;
      }
    };

    for (const ultralight::Command &command : m_state->commands) {
      const uint32_t renderBufferId = command.gpu_state.render_buffer_id;
      auto renderBufferIt = m_state->renderBuffers.find(renderBufferId);
      if (renderBufferIt == m_state->renderBuffers.end())
        continue;
      dirtyRenderBuffers.insert(renderBufferId);

      if (command.command_type == ultralight::CommandType::ClearRenderBuffer) {
        // Realized lazily as the load action of the next draw pass.
        m_state->pendingClear.insert(renderBufferId);
        if (encoderRenderBuffer == renderBufferId)
          endEncoder();
        continue;
      }

      auto textureIt =
          m_state->textures.find(renderBufferIt->second.textureId);
      auto geometryIt = m_state->geometry.find(command.geometry_id);
      if (textureIt == m_state->textures.end() ||
          geometryIt == m_state->geometry.end())
        continue;
      id<MTLTexture> target = textureIt->second;

      if (!encoder || encoderRenderBuffer != renderBufferId) {
        endEncoder();
        MTLRenderPassDescriptor *pass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction =
            m_state->pendingClear.erase(renderBufferId) ? MTLLoadActionClear
                                                        : MTLLoadActionLoad;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
        encoderRenderBuffer = renderBufferId;
      }

      const ultralight::GPUState &gpu = command.gpu_state;
      const int shaderIndex =
          gpu.shader_type == ultralight::ShaderType::FillPath ? 1 : 0;
      [encoder setRenderPipelineState:m_state->pipelines[shaderIndex]
                                                        [gpu.enable_blend ? 1
                                                                          : 0]];

      [encoder setViewport:MTLViewport{0.0, 0.0,
                                       (double)gpu.viewport_width,
                                       (double)gpu.viewport_height, 0.0,
                                       1.0}];

      MTLScissorRect scissor;
      if (gpu.enable_scissor) {
        const ultralight::IntRect &r = gpu.scissor_rect;
        NSUInteger x = std::max(0, r.left);
        NSUInteger y = std::max(0, r.top);
        scissor.x = std::min(x, target.width - 1);
        scissor.y = std::min(y, target.height - 1);
        scissor.width =
            std::min((NSUInteger)std::max(0, r.right - (int)scissor.x),
                     target.width - scissor.x);
        scissor.height =
            std::min((NSUInteger)std::max(0, r.bottom - (int)scissor.y),
                     target.height - scissor.y);
      } else {
        scissor = {0, 0, target.width, target.height};
      }
      [encoder setScissorRect:scissor];

      Uniforms uniforms;
      std::memset(&uniforms, 0, sizeof(uniforms));
      uniforms.State = simd_make_float4(0.0f, (float)gpu.viewport_width,
                                        (float)gpu.viewport_height, 1.0f);
      uniforms.Transform = toSimd(projectedTransform(gpu));
      std::memcpy(uniforms.Scalar4, gpu.uniform_scalar, sizeof(float) * 8);
      std::memcpy(uniforms.Vector, gpu.uniform_vector,
                  sizeof(uniforms.Vector));
      uniforms.ClipSize = gpu.clip_size;
      for (uint8_t i = 0; i < gpu.clip_size && i < 8; ++i)
        uniforms.Clip[i] = toSimd(gpu.clip[i]);

      [encoder setVertexBuffer:geometryIt->second.vertices offset:0 atIndex:0];
      [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
      [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];

      auto bindTexture = [&](uint32_t textureId, NSUInteger slot) {
        if (!textureId)
          return;
        auto it = m_state->textures.find(textureId);
        if (it != m_state->textures.end())
          [encoder setFragmentTexture:it->second atIndex:slot];
      };
      bindTexture(gpu.texture_1_id, 0);
      bindTexture(gpu.texture_2_id, 1);

      [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                          indexCount:command.indices_count
                           indexType:MTLIndexTypeUInt32
                         indexBuffer:geometryIt->second.indices
                   indexBufferOffset:command.indices_offset *
                                     sizeof(ultralight::IndexType)];
    }

    endEncoder();
    [commandBuffer commit];
  }

  m_state->commands.clear();
  return dirtyRenderBuffers;
}

void *UltralightMetalDriver::createPublishTexture(int width, int height) {
  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:width
                                  height:height
                               mipmapped:NO];
  desc.usage = MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModePrivate;
  id<MTLTexture> texture = [m_state->device newTextureWithDescriptor:desc];
  return (__bridge_retained void *)texture;
}

void UltralightMetalDriver::releaseTexture(void *mtlTexture) {
  if (mtlTexture)
    CFRelease(mtlTexture);
}

void *UltralightMetalDriver::createImageTexture(int width, int height) {
  MTLTextureDescriptor *desc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                   width:width
                                  height:height
                               mipmapped:NO];
  desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
  // Shared storage: render-target capable on Apple GPUs while still
  // accepting replaceRegion uploads from the CPU.
  desc.storageMode = MTLStorageModeShared;
  id<MTLTexture> texture = [m_state->device newTextureWithDescriptor:desc];
  return (__bridge_retained void *)texture;
}

uint32_t UltralightMetalDriver::registerExternalTexture(void *mtlTexture) {
  uint32_t textureId = m_state->nextTextureId++;
  m_state->textures[textureId] = (__bridge id<MTLTexture>)mtlTexture;
  return textureId;
}

void UltralightMetalDriver::unregisterExternalTexture(uint32_t textureId) {
  m_state->textures.erase(textureId);
}

bool UltralightMetalDriver::paintTexture(
    void *mtlTexture, int width, int height,
    const std::function<void(SkCanvas &)> &painter) {
  if (!mtlTexture || !m_state->skiaRecorder)
    return false;

  skgpu::graphite::BackendTexture backendTexture =
      skgpu::graphite::BackendTextures::MakeMetal(
          SkISize::Make(width, height), (__bridge CFTypeRef)(
              (__bridge id<MTLTexture>)mtlTexture));
  sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
      m_state->skiaRecorder.get(), backendTexture, SkColorSpace::MakeSRGB(),
      nullptr);
  if (!surface)
    return false;

  painter(*surface->getCanvas());
  surface.reset();

  std::unique_ptr<skgpu::graphite::Recording> recording =
      m_state->skiaRecorder->snap();
  if (!recording)
    return false;
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = recording.get();
  m_state->skiaContext->insertRecording(info);
  m_state->skiaContext->submit();
  return true;
}

void UltralightMetalDriver::uploadToTexture(void *mtlTexture,
                                            const void *pixels, int width,
                                            int height, size_t rowBytes) {
  if (!mtlTexture)
    return;
  id<MTLTexture> texture = (__bridge id<MTLTexture>)mtlTexture;
  [texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
             mipmapLevel:0
               withBytes:pixels
             bytesPerRow:rowBytes];
}

void UltralightMetalDriver::copyTexture(uint32_t srcTextureId,
                                        void *dstMtlTexture, int width,
                                        int height) {
  auto it = m_state->textures.find(srcTextureId);
  if (it == m_state->textures.end() || !dstMtlTexture)
    return;
  id<MTLTexture> src = it->second;
  id<MTLTexture> dst = (__bridge id<MTLTexture>)dstMtlTexture;
  NSUInteger copyWidth = std::min<NSUInteger>(width, src.width);
  copyWidth = std::min(copyWidth, dst.width);
  NSUInteger copyHeight = std::min<NSUInteger>(height, src.height);
  copyHeight = std::min(copyHeight, dst.height);

  @autoreleasepool {
    id<MTLCommandBuffer> commandBuffer = [m_state->queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [commandBuffer blitCommandEncoder];
    [blit copyFromTexture:src
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(copyWidth, copyHeight, 1)
                toTexture:dst
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    [commandBuffer commit];
  }
}

} // namespace ifrit::web
