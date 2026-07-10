#pragma once

#include "shader_program.hpp"
#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace wsl
{

namespace gfx
{

class render_context;
class render_window;

/*!
 * \brief Key for a cached graphics pipeline.
 *
 * Combines shader program identity with the fixed-function state needed to
 * create an SDL_GPU pipeline.
 */
struct pipeline_key
{
  uint64_t shader_program_hash = 0;
  uint32_t vertex_layout_hash = 0;
  uint32_t render_target_hash = 0;
  uint32_t flags = 0; //!< double-sided, blend mode, depth state, etc.

  bool operator== (const pipeline_key &other) const;
};

} // namespace gfx

} // namespace wsl

namespace std
{
template <> struct hash<wsl::gfx::pipeline_key>
{
  std::size_t operator() (const wsl::gfx::pipeline_key &k) const noexcept;
};
} // namespace std

namespace wsl
{

namespace gfx
{

/*!
 * \brief Cache for SDL_GPU graphics pipelines.
 *
 * Because custom materials can produce many shader permutations,
 * we cache pipelines by their full state key to avoid redundant
 * SDL_CreateGPUGraphicsPipeline calls.
 */
class pipeline_cache
{
public:
  explicit pipeline_cache (SDL_GPUDevice *device);
  ~pipeline_cache ();

  pipeline_cache (const pipeline_cache &) = delete;
  pipeline_cache &operator= (const pipeline_cache &) = delete;

  /*! \brief Invalidate all cached pipelines. Call when a shader is recompiled.
   */
  void invalidate_all ();

  /*! \brief Invalidate only pipelines referencing a given shader program hash.
   */
  void invalidate_shader (uint64_t shader_program_hash);

  /*! \brief Acquire or create a pipeline for the given key and description. */
  SDL_GPUGraphicsPipeline *
  acquire (const pipeline_key &key,
           const SDL_GPUGraphicsPipelineCreateInfo &info);

private:
  SDL_GPUDevice *m_device = nullptr;
  std::unordered_map<pipeline_key, SDL_GPUGraphicsPipeline *> m_cache;
};

} // namespace gfx

} // namespace wsl
