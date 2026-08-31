/** @file
 * A Graph's lifetime and its identity: constructed empty for its
 * Package to fill, and its label and url read back.
 */

#include "GraphImpl.h"

namespace sigil::substance {

Graph::Graph() : m_impl(std::make_unique<Impl>()) {}
Graph::~Graph() = default;

const std::string& Graph::label() const { return m_impl->label; }
const std::string& Graph::url() const { return m_impl->url; }

}  // namespace sigil::substance
