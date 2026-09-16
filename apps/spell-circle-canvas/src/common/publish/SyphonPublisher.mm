// The Syphon server behind the publish seam. Every line that talks to
// Objective-C is here: the seam itself is plain C++ over opaque handles,
// so a consumer of it compiles wherever this repository compiles.

#import <Metal/Metal.h>
#import <Syphon/SyphonMetalServer.h>

#include "SyphonPublisher.h"

#include <string>
#include <string_view>
#include <utility>

namespace sigil::publish {

namespace {

class SyphonPublisher final : public Publisher {
 public:
  // Takes the reference alloc/init returned; nothing else holds one.
  SyphonPublisher(std::string name, SyphonMetalServer *server)
      : m_name(std::move(name)), m_server(server) {}
  ~SyphonPublisher() override { stop(); }
  SyphonPublisher(const SyphonPublisher &) = delete;
  SyphonPublisher &operator=(const SyphonPublisher &) = delete;

  void publishFrame(void *nativeTexture, void *nativeCommandBuffer, int width,
                    int height) override {
    // Syphon retains the last image for clients that subscribe later.
    // A static host may publish once and perform no more drawing.
    if (!m_server || !nativeTexture || !nativeCommandBuffer || width <= 0 || height <= 0) return;

    // The handles cross the seam as opaque pointers; __bridge recasts
    // them without moving ownership, which stays with the host that
    // made them.
    id<MTLTexture> texture = (__bridge id<MTLTexture>)nativeTexture;
    id<MTLCommandBuffer> commandBuffer = (__bridge id<MTLCommandBuffer>)nativeCommandBuffer;

    // Syphon appends the copy to the buffer the caller is still filling
    // and the caller commits it.
    //
    // NOT FLIPPED: the flag says whether the texture's rows are upside
    // down for the graphics API it came from, and a canvas drew this one
    // the way its API reads it — first row at the top. Saying otherwise
    // turns every frame over on the way across and costs a redraw where
    // an unturned frame is a straight copy.
    [m_server publishFrameTexture:texture
                  onCommandBuffer:commandBuffer
                      imageRegion:NSMakeRect(0, 0, width, height)
                          flipped:NO];
  }

  std::string_view name() const override { return m_name; }

 private:
  void stop() {
    // This file compiles without automatic reference counting: the
    // server arrived with a reference this object owns, so `= nil`
    // alone would leave the server standing with nobody to stop it.
    [m_server stop];
    [m_server release];
    m_server = nil;
  }

  std::string m_name;
  SyphonMetalServer *m_server = nil;
};

}  // namespace

std::unique_ptr<Publisher> makeSyphonPublisher(std::string name, void *mtlDevice) {
  id<MTLDevice> device = (__bridge id<MTLDevice>)mtlDevice;
  SyphonMetalServer *server = [[SyphonMetalServer alloc] initWithName:@(name.c_str())
                                                               device:device
                                                              options:nil];
  if (!server) return nullptr;
  return std::make_unique<SyphonPublisher>(std::move(name), server);
}

}  // namespace sigil::publish
