// The subscription behind both lanes: one name, whatever process is
// publishing under it.

#include "Feed.h"

#include "Servers.h"

#include <utility>

namespace receiver {

Feed::Feed(id<MTLDevice> device, std::string name, std::string app)
    : m_device(device),
      m_name(std::move(name)),
      m_app(std::move(app)),
      m_frames(std::make_shared<std::atomic<unsigned long long>>(0)) {}

Feed::~Feed() {
  // Releasing every reference would stop the client on its own; stopping
  // it says when.
  [m_client stop];
  m_client = nil;
}

bool Feed::open(double withinSeconds) {
  if (standing()) return true;
  [m_client stop];
  m_client = nil;
  m_identity = nil;
  NSDictionary<NSString *, id> *publication =
      publicationNamed(@(m_name.c_str()), @(m_app.c_str()), withinSeconds);
  if (!publication) return false;
  m_identity = publicationIdentity(publication);
  m_publishingApp = publicationApp(publication).UTF8String;
  // The handler holds the counter and nothing else, so a frame that
  // arrives while this feed is being let go increments a number that is
  // still there to increment.
  std::shared_ptr<std::atomic<unsigned long long>> frames = m_frames;
  m_client = [[SyphonMetalClient alloc] initWithServerDescription:publication
                                                           device:m_device
                                                          options:nil
                                                  newFrameHandler:^(SyphonMetalClient *) {
                                                    frames->fetch_add(1);
                                                  }];
  return standing();
}

bool Feed::standing() const {
  return m_client != nil && m_client.isValid && publicationIsListed(m_identity);
}

unsigned long long Feed::frames() const { return m_frames->load(); }

id<MTLTexture> Feed::newestFrame() const { return [m_client newFrameImage]; }

}  // namespace receiver
