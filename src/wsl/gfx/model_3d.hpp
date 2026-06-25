#pragma once

#include "mesh.hpp"
#include "render_context.hpp"
#include "tracy_gpu_mem.hpp"

#include <SDL3/SDL_gpu.h>
#include <glm/ext/matrix_float4x4.hpp>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace wsl
{

namespace gfx
{

/*!
 * \brief Axis-aligned bounding box.
 */
struct aabb
{
  glm::vec3 min{ std::numeric_limits<float>::max () };
  glm::vec3 max{ -std::numeric_limits<float>::max () };
  bool valid = false;

  void
  expand (const glm::vec3 &p)
  {
    if (!valid) {
      min = p;
      max = p;
      valid = true;
      return;
    }

    min = glm::min (min, p);
    max = glm::max (max, p);
  }

  void
  expand (const aabb &other)
  {
    if (!other.valid) {
      {
        return;
      }
    }
    if (!valid) {
      min = other.min;
      max = other.max;
      valid = true;
      return;
    }

    min = glm::min (min, other.min);
    max = glm::max (max, other.max);
  }
};

/*!
 * \brief Scene graph node used by a model scene.
 */
struct node
{
  glm::mat4 world_transform{ 1.0F };
  glm::mat4 local_transform{ 1.0F };
  std::vector<gfx::mesh *> mesh_lods;
  std::vector<node> children;
};

/*!
 * \brief Renderable scene inside a model resource.
 */
struct scene
{
  std::vector<node> roots;
};

/*!
 * \brief GPU-ready 3D model resource.
 */
struct model_3d
{
  std::vector<mesh> meshes;
  std::vector<scene> scenes;

  /*!
   * \brief Set of meshes representing different LOD levels for a base mesh.
   */
  struct lod_group
  {
    std::string base_name;
    std::vector<mesh *> levels;
  };

  std::vector<lod_group> lod_groups;

  model_3d () = default;
  ~model_3d () { release (); }

  model_3d (const model_3d &) = delete;
  model_3d &operator= (const model_3d &) = delete;

  model_3d (model_3d &&other) noexcept { *this = std::move (other); }

  model_3d &
  operator= (model_3d &&other) noexcept
  {
    if (this != &other) {
      release ();

      meshes = std::move (other.meshes);
      scenes = std::move (other.scenes);
      lod_groups = std::move (other.lod_groups);
      default_scene = other.default_scene;

      m_vertex_buffer = other.m_vertex_buffer;
      m_index_buffer = other.m_index_buffer;
      m_gpu_ready = other.m_gpu_ready;
      m_vertex_count = other.m_vertex_count;
      m_index_count = other.m_index_count;
      m_scene_bounds = std::move (other.m_scene_bounds);
      m_device = other.m_device;

      other.m_vertex_buffer = nullptr;
      other.m_index_buffer = nullptr;
      other.m_gpu_ready = false;
      other.m_vertex_count = 0;
      other.m_index_count = 0;
      other.m_device = nullptr;
    }
    return *this;
  }

  //! Builds GPU buffers for the current mesh data.
  void build_gpu_buffers (gfx::render_context *ctx);
  //! Releases GPU buffers owned by the model.
  void destroy_gpu_buffers (gfx::render_context *ctx);

  //! Creates a unit quad model.
  static std::shared_ptr<model_3d> make_unit_quad ();
  //! Creates a unit cube model.
  static std::shared_ptr<model_3d> make_unit_cube ();
  //! Creates a unit prism model.
  static std::shared_ptr<model_3d> make_unit_prism ();
  //! Creates a unit cylinder model.
  static std::shared_ptr<model_3d> make_unit_cylinder ();
  //! Creates a unit sphere model.
  static std::shared_ptr<model_3d> make_unit_sphere ();

  int default_scene = 0;

  void
  bind (SDL_GPURenderPass *pass) const
  {
    SDL_GPUBufferBinding const vb{ m_vertex_buffer, 0 };
    SDL_GPUBufferBinding const ib{ m_index_buffer, 0 };

    SDL_BindGPUVertexBuffers (pass, 0, &vb, 1);
    SDL_BindGPUIndexBuffer (pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
  }

  void
  ensure_gpu_buffers (render_context *ctx)
  {
    if (m_gpu_ready) {
      return;
    }

    build_gpu_buffers (ctx);
    m_gpu_ready = true;
  }

  //! Returns the world-space bounds for a scene index when available.
  bool get_scene_bounds (size_t scene_index, glm::vec3 &out_min,
                         glm::vec3 &out_max) const;

  //! Recomputes cached scene bounds from mesh data and scene transforms.
  void rebuild_scene_bounds ();

private:
  void
  release ()
  {
    if (m_device != nullptr) {
      if (m_vertex_buffer != nullptr) {
        wsl::gfx::tracy_free_buffer (m_vertex_buffer);
        SDL_ReleaseGPUBuffer (m_device, m_vertex_buffer);
      }
      if (m_index_buffer != nullptr) {
        wsl::gfx::tracy_free_buffer (m_index_buffer);
        SDL_ReleaseGPUBuffer (m_device, m_index_buffer);
      }
    }
    m_vertex_buffer = nullptr;
    m_index_buffer = nullptr;
    m_gpu_ready = false;
    m_device = nullptr;
  }

  SDL_GPUBuffer *m_vertex_buffer = nullptr;
  SDL_GPUBuffer *m_index_buffer = nullptr;

  bool m_gpu_ready = false;
  size_t m_vertex_count = 0;
  size_t m_index_count = 0;

  std::vector<aabb> m_scene_bounds;
  SDL_GPUDevice *m_device = nullptr;
};

} // namespace gfx

} // namespace wsl
