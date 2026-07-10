#include "shader_program.hpp"

namespace wsl
{

namespace gfx
{

uint32_t
shader_reflection::num_uniform_buffers () const
{
  return static_cast<uint32_t> (uniform_buffers.size ());
}

uint32_t
shader_reflection::num_samplers () const
{
  return sampler_count != 0 ? sampler_count
                            : static_cast<uint32_t> (textures.size ());
}

uint32_t
shader_reflection::num_storage_buffers () const
{
  return static_cast<uint32_t> (storage_buffers.size ());
}

const shader_uniform_buffer *
shader_reflection::find_uniform_buffer (const std::string &name) const
{
  for (const auto &ub : uniform_buffers) {
    if (ub.name == name) {
      return &ub;
    }
  }
  return nullptr;
}

const shader_texture_binding *
shader_reflection::find_texture (const std::string &name) const
{
  for (const auto &t : textures) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

void
shader_program::clear () noexcept
{
  vertex_bytecode.clear ();
  fragment_bytecode.clear ();
  vertex_reflection = {};
  fragment_reflection = {};
}

} // namespace gfx

} // namespace wsl
