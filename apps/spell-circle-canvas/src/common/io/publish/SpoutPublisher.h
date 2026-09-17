#pragma once

#include <sigilio/publish/Publisher.h>

namespace sigil::io::publish {

std::unique_ptr<Publisher> makeSpoutPublisher(std::string name,
                                              void* nativeDevice);

}  // namespace sigil::io::publish
