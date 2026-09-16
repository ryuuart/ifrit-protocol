#pragma once

#include <sigilpublish/Publisher.h>

namespace sigil::publish {

std::unique_ptr<Publisher> makeSpoutPublisher(std::string name,
                                              void* nativeDevice);

}  // namespace sigil::publish
