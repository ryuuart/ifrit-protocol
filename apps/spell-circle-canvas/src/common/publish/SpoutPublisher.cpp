#include "SpoutPublisher.h"

#include <SpoutDX.h>
#include <d3d11.h>

#include <utility>

namespace sigil::publish {
namespace {

class SpoutPublisher final : public Publisher {
 public:
  explicit SpoutPublisher(std::string name) : m_name(std::move(name)) {}
  ~SpoutPublisher() override {
    if (!m_open) return;
    m_sender.ReleaseSender();
    m_sender.CloseDirectX11();
  }

  bool open(ID3D11Device* device) {
    m_sender.SetSenderName(m_name.c_str());
    m_open = m_sender.OpenDirectX11(device);
    return m_open;
  }

  void publishFrame(void* texture, void*, int width, int height) override {
    if (!m_open || !texture || width <= 0 || height <= 0) return;
    // Spout copies through the device's immediate context after the host's
    // draw, recreating its shared texture when the source extent changes.
    m_sender.SendTexture(static_cast<ID3D11Texture2D*>(texture));
  }

  std::string_view name() const override { return m_name; }

 private:
  std::string m_name;
  spoutDX m_sender;
  bool m_open = false;
};

}  // namespace

std::unique_ptr<Publisher> makeSpoutPublisher(std::string name,
                                              void* nativeDevice) {
  auto publisher = std::make_unique<SpoutPublisher>(std::move(name));
  if (!publisher->open(static_cast<ID3D11Device*>(nativeDevice))) return {};
  return publisher;
}

}  // namespace sigil::publish
