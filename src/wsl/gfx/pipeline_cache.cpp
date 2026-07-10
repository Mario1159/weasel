#include "pipeline_cache.hpp"

namespace wsl
{

namespace gfx
{

bool
pipeline_key::operator== (const pipeline_key &other) const
{
  return shader_program_hash == other.shader_program_hash
         && vertex_layout_hash == other.vertex_layout_hash
         && render_target_hash == other.render_target_hash
         && flags == other.flags;
}

} // namespace gfx

} // namespace wsl

namespace std
{

std::size_t
hash<wsl::gfx::pipeline_key>::operator() (
    const wsl::gfx::pipeline_key &k) const noexcept
{
  std::size_t h = k.shader_program_hash;
  h ^= std::hash<uint32_t>{}(k.vertex_layout_hash) + 0x9e3779b9 + (h << 6)
       + (h >> 2);
  h ^= std::hash<uint32_t>{}(k.render_target_hash) + 0x9e3779b9 + (h << 6)
       + (h >> 2);
  h ^= std::hash<uint32_t>{}(k.flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
  return h;
}

} // namespace std

namespace wsl
{

namespace gfx
{

pipeline_cache::pipeline_cache (SDL_GPUDevice *device) : m_device (device) {}

pipeline_cache::~pipeline_cache () { invalidate_all (); }

void
pipeline_cache::invalidate_all ()
{
  for (auto &kv : m_cache) {
    if (kv.second != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline (m_device, kv.second);
    }
  }
  m_cache.clear ();
}

void
pipeline_cache::invalidate_shader (uint64_t shader_program_hash)
{
  for (auto it = m_cache.begin (); it != m_cache.end ();) {
    if (it->first.shader_program_hash == shader_program_hash) {
      if (it->second != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline (m_device, it->second);
      }
      it = m_cache.erase (it);
    } else {
      ++it;
    }
  }
}

SDL_GPUGraphicsPipeline *
pipeline_cache::acquire (const pipeline_key &key,
                         const SDL_GPUGraphicsPipelineCreateInfo &info)
{
  auto it = m_cache.find (key);
  if (it != m_cache.end ()) {
    return it->second;
  }

  SDL_GPUGraphicsPipeline *pipe
      = SDL_CreateGPUGraphicsPipeline (m_device, &info);
  if (pipe != nullptr) {
    m_cache[key] = pipe;
  }
  return pipe;
}

} // namespace gfx

} // namespace wsl
