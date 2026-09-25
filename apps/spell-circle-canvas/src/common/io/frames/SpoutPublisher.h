#pragma once

#include <sigilio/frames/Publisher.h>

namespace sigil::io::frames {

std::unique_ptr<Publisher> makeSpoutPublisher(std::string name,
                                              void* nativeDevice);

}  // namespace sigil::io::frames
