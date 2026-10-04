# Shared CMake helper fixture

A compiled library and a header-only library each publish C++20 headers to
consumers in the parent directory, whose default language standard is C++14.
Their targets must supply the language requirement to those consumers.

A third library embeds one shader from its `shaders/` directory through
`sigil_shader_sources` and reads it back through the generated accessor,
so the embedding step runs and its table compiles.
