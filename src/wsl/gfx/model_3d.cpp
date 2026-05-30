#include "model_3d.hpp"
#include "wsl/log/log.hpp"

#include "gfx/mesh.hpp"
#include "render_context.hpp"

#include <SDL3/SDL_gpu.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/geometric.hpp>
#include <memory>
#include <numbers>
#include <utility>
#include <vector>

namespace wsl
{

namespace
{

constexpr float k_half_extent = 0.5F;

gfx::vertex
make_vertex (const glm::vec3 &pos, const glm::vec3 &normal, const glm::vec2 &uv,
             const glm::vec3 &tangent)
{
  gfx::vertex vertex{};
  vertex.pos = pos;
  vertex.normal = glm::normalize (normal);
  vertex.uv = uv;

  glm::vec3 safe_tangent = tangent;
  if (glm::dot (safe_tangent, safe_tangent) <= 0.0F) {
    safe_tangent = glm::vec3 (1.0F, 0.0F, 0.0F);
  }

  vertex.tangent = glm::vec4 (glm::normalize (safe_tangent), 1.0F);
  return vertex;
}

void
append_quad (gfx::primitive &prim, const glm::vec3 &center,
             const glm::vec3 &u_axis, const glm::vec3 &v_axis)
{
  const glm::vec3 normal = glm::normalize (glm::cross (u_axis, v_axis));
  const glm::vec3 tangent = glm::normalize (u_axis);
  const uint32_t base = static_cast<uint32_t> (prim.vertices.size ());

  prim.vertices.push_back (
      make_vertex (center - u_axis - v_axis, normal, { 0.0F, 1.0F }, tangent));
  prim.vertices.push_back (
      make_vertex (center + u_axis - v_axis, normal, { 1.0F, 1.0F }, tangent));
  prim.vertices.push_back (
      make_vertex (center + u_axis + v_axis, normal, { 1.0F, 0.0F }, tangent));
  prim.vertices.push_back (
      make_vertex (center - u_axis + v_axis, normal, { 0.0F, 0.0F }, tangent));

  prim.indices.insert (prim.indices.end (),
                       { base, base + 1, base + 2, base, base + 2, base + 3 });
}

std::shared_ptr<gfx::model_3d>
make_single_primitive_model (gfx::primitive primitive)
{
  std::shared_ptr<gfx::model_3d> model = std::make_shared<gfx::model_3d> ();

  gfx::mesh &mesh = model->meshes.emplace_back ();
  mesh.primitives.push_back (std::move (primitive));

  gfx::scene scene;
  gfx::node root;
  root.mesh_lods.push_back (model->meshes.data ());
  scene.roots.push_back (std::move (root));

  model->scenes.push_back (std::move (scene));
  model->default_scene = 0;
  model->rebuild_scene_bounds ();
  return model;
}

gfx::primitive
build_quad_primitive ()
{
  gfx::primitive prim{};
  append_quad (prim, glm::vec3 (0.0F), glm::vec3 (0.0F, 0.0F, k_half_extent),
               glm::vec3 (k_half_extent, 0.0F, 0.0F));
  return prim;
}

gfx::primitive
build_cube_primitive ()
{
  gfx::primitive prim{};

  append_quad (prim, glm::vec3 (0.0F, 0.0F, k_half_extent),
               glm::vec3 (k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, k_half_extent, 0.0F));
  append_quad (prim, glm::vec3 (0.0F, 0.0F, -k_half_extent),
               glm::vec3 (-k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, k_half_extent, 0.0F));
  append_quad (prim, glm::vec3 (k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, 0.0F, -k_half_extent),
               glm::vec3 (0.0F, k_half_extent, 0.0F));
  append_quad (prim, glm::vec3 (-k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, 0.0F, k_half_extent),
               glm::vec3 (0.0F, k_half_extent, 0.0F));
  append_quad (prim, glm::vec3 (0.0F, k_half_extent, 0.0F),
               glm::vec3 (k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, 0.0F, -k_half_extent));
  append_quad (prim, glm::vec3 (0.0F, -k_half_extent, 0.0F),
               glm::vec3 (k_half_extent, 0.0F, 0.0F),
               glm::vec3 (0.0F, 0.0F, k_half_extent));

  return prim;
}

gfx::primitive
build_cylinder_primitive (uint32_t radial_segments = 32)
{
  gfx::primitive prim{};
  radial_segments = std::max<uint32_t> (radial_segments, 3);

  const float radius = k_half_extent;
  const float half_height = k_half_extent;
  const float tau = std::numbers::pi_v<float> * 2.0F;

  for (uint32_t i = 0; i <= radial_segments; ++i) {
    const float u = static_cast<float> (i) / radial_segments;
    const float angle = u * tau;
    const float x = std::cos (angle) * radius;
    const float z = std::sin (angle) * radius;
    const glm::vec3 normal = glm::normalize (glm::vec3 (x, 0.0F, z));
    const glm::vec3 tangent (-std::sin (angle), 0.0F, std::cos (angle));

    prim.vertices.push_back (
        make_vertex ({ x, -half_height, z }, normal, { u, 1.0F }, tangent));
    prim.vertices.push_back (
        make_vertex ({ x, half_height, z }, normal, { u, 0.0F }, tangent));
  }

  for (uint32_t i = 0; i < radial_segments; ++i) {
    const uint32_t base = i * 2;
    prim.indices.insert (prim.indices.end (), { base, base + 1, base + 3, base,
                                                base + 3, base + 2 });
  }

  const uint32_t top_center = static_cast<uint32_t> (prim.vertices.size ());
  prim.vertices.push_back (make_vertex ({ 0.0F, half_height, 0.0F },
                                        { 0.0F, 1.0F, 0.0F }, { 0.5F, 0.5F },
                                        { 1.0F, 0.0F, 0.0F }));

  for (uint32_t i = 0; i <= radial_segments; ++i) {
    const float u = static_cast<float> (i) / radial_segments;
    const float angle = u * tau;
    const float x = std::cos (angle) * radius;
    const float z = std::sin (angle) * radius;
    const glm::vec2 uv (((x / radius) * 0.5F) + 0.5F,
                        ((z / radius) * 0.5F) + 0.5F);

    prim.vertices.push_back (make_vertex (
        { x, half_height, z }, { 0.0F, 1.0F, 0.0F }, uv, { 1.0F, 0.0F, 0.0F }));
  }

  for (uint32_t i = 0; i < radial_segments; ++i) {
    prim.indices.insert (prim.indices.end (), { top_center, top_center + 1 + i,
                                                top_center + 2 + i });
  }

  const uint32_t bottom_center = static_cast<uint32_t> (prim.vertices.size ());
  prim.vertices.push_back (make_vertex ({ 0.0F, -half_height, 0.0F },
                                        { 0.0F, -1.0F, 0.0F }, { 0.5F, 0.5F },
                                        { 1.0F, 0.0F, 0.0F }));

  for (uint32_t i = 0; i <= radial_segments; ++i) {
    const float u = static_cast<float> (i) / radial_segments;
    const float angle = u * tau;
    const float x = std::cos (angle) * radius;
    const float z = std::sin (angle) * radius;
    const glm::vec2 uv (((x / radius) * 0.5F) + 0.5F,
                        ((z / radius) * 0.5F) + 0.5F);

    prim.vertices.push_back (make_vertex ({ x, -half_height, z },
                                          { 0.0F, -1.0F, 0.0F }, uv,
                                          { 1.0F, 0.0F, 0.0F }));
  }

  for (uint32_t i = 0; i < radial_segments; ++i) {
    prim.indices.insert (
        prim.indices.end (),
        { bottom_center, bottom_center + 2 + i, bottom_center + 1 + i });
  }

  return prim;
}

gfx::primitive
build_uv_sphere_primitive (uint32_t radial_segments = 32, uint32_t rings = 16)
{
  gfx::primitive prim{};
  radial_segments = std::max<uint32_t> (radial_segments, 3);
  rings = std::max<uint32_t> (rings, 2);

  const float radius = k_half_extent;
  const float tau = std::numbers::pi_v<float> * 2.0F;

  for (uint32_t ring = 0; ring <= rings; ++ring) {
    const float v = static_cast<float> (ring) / rings;
    const float phi = v * std::numbers::pi_v<float>;
    const float y = std::cos (phi) * radius;
    const float ring_radius = std::sin (phi) * radius;

    for (uint32_t seg = 0; seg <= radial_segments; ++seg) {
      const float u = static_cast<float> (seg) / radial_segments;
      const float theta = u * tau;

      const float x = std::cos (theta) * ring_radius;
      const float z = std::sin (theta) * ring_radius;

      const glm::vec3 pos (x, y, z);
      glm::vec3 tangent (-std::sin (theta), 0.0F, std::cos (theta));
      if (glm::dot (tangent, tangent) <= 0.0F) {
        tangent = glm::vec3 (1.0F, 0.0F, 0.0F);
      }

      prim.vertices.push_back (
          make_vertex (pos, glm::normalize (pos), { u, v }, tangent));
    }
  }

  const uint32_t row_stride = radial_segments + 1;
  for (uint32_t ring = 0; ring < rings; ++ring) {
    for (uint32_t seg = 0; seg < radial_segments; ++seg) {
      const uint32_t a = (ring * row_stride) + seg;
      const uint32_t b = a + row_stride;

      prim.indices.insert (prim.indices.end (),
                           { a, a + 1, b, a + 1, b + 1, b });
    }
  }

  return prim;
}

gfx::aabb
compute_mesh_local_bounds (const gfx::mesh &mesh)
{
  gfx::aabb box;

  for (const auto &prim : mesh.primitives) {
    for (const auto &vertex : prim.vertices) {
      box.expand (vertex.pos);
    }
  }

  return box;
}

void
transform_aabb_local (const gfx::aabb &src, const glm::mat4 &transform,
                      gfx::aabb &dst)
{
  if (!src.valid) {
    return;
  }

  const glm::vec3 corners[8] = {
    { src.min.x, src.min.y, src.min.z }, { src.max.x, src.min.y, src.min.z },
    { src.min.x, src.max.y, src.min.z }, { src.max.x, src.max.y, src.min.z },
    { src.min.x, src.min.y, src.max.z }, { src.max.x, src.min.y, src.max.z },
    { src.min.x, src.max.y, src.max.z }, { src.max.x, src.max.y, src.max.z },
  };

  for (const glm::vec3 &corner : corners) {
    dst.expand (glm::vec3 (transform * glm::vec4 (corner, 1.0F)));
  }
}

gfx::aabb
compute_node_bounds_recursive (const gfx::node &node)
{
  gfx::aabb out;

  if (!node.mesh_lods.empty () && (node.mesh_lods[0] != nullptr)) {
    gfx::aabb const mesh_box = compute_mesh_local_bounds (*node.mesh_lods[0]);
    gfx::aabb transformed;
    transform_aabb_local (mesh_box, node.local_transform, transformed);
    out.expand (transformed);
  }

  for (const gfx::node &child : node.children) {
    gfx::aabb const child_box = compute_node_bounds_recursive (child);
    gfx::aabb transformed_child;
    transform_aabb_local (child_box, node.local_transform, transformed_child);
    out.expand (transformed_child);
  }

  return out;
}

} // namespace

void
gfx::model_3d::build_gpu_buffers (gfx::render_context *ctx)
{
  std::vector<vertex> vertices;
  std::vector<uint32_t> indices;

  vertices.reserve (4096);
  indices.reserve (4096);

  for (gfx::mesh &mesh : meshes) {
    for (gfx::primitive &prim : mesh.primitives) {
      const uint32_t base_vertex = static_cast<uint32_t> (vertices.size ());

      vertices.insert (vertices.end (), prim.vertices.begin (),
                       prim.vertices.end ());

      prim.first_index = static_cast<uint32_t> (indices.size ());
      for (uint32_t const idx : prim.indices) {
        indices.push_back (base_vertex + idx);
      }
    }
  }

  m_vertex_count = static_cast<uint32_t> (vertices.size ());
  m_index_count = static_cast<uint32_t> (indices.size ());

  if (m_vertex_count == 0 || m_index_count == 0) {
    wsl::log::gfx ()->error ("Empty model in build_gpu_buffers");
    return;
  }

  wsl::log::gfx ()->debug ("Building model: {} vertices, {} indices",
                           m_vertex_count, m_index_count);

  destroy_gpu_buffers (ctx);
  m_device = ctx->gpu_device;

  SDL_GPUBufferCreateInfo vb{};
  vb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  vb.size = m_vertex_count * sizeof (vertex);

  m_vertex_buffer = SDL_CreateGPUBuffer (ctx->gpu_device, &vb);

  SDL_GPUBufferCreateInfo ib{};
  ib.usage = SDL_GPU_BUFFERUSAGE_INDEX;
  ib.size = m_index_count * sizeof (uint32_t);

  m_index_buffer = SDL_CreateGPUBuffer (ctx->gpu_device, &ib);

  SDL_GPUTransferBufferCreateInfo tvb{};
  tvb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tvb.size = vb.size;

  SDL_GPUTransferBufferCreateInfo tib{};
  tib.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tib.size = ib.size;

  SDL_GPUTransferBuffer *v_upload
      = SDL_CreateGPUTransferBuffer (ctx->gpu_device, &tvb);
  SDL_GPUTransferBuffer *i_upload
      = SDL_CreateGPUTransferBuffer (ctx->gpu_device, &tib);

  void *vmap = SDL_MapGPUTransferBuffer (ctx->gpu_device, v_upload, false);
  std::memcpy (vmap, vertices.data (), vb.size);
  SDL_UnmapGPUTransferBuffer (ctx->gpu_device, v_upload);

  void *imap = SDL_MapGPUTransferBuffer (ctx->gpu_device, i_upload, false);
  std::memcpy (imap, indices.data (), ib.size);
  SDL_UnmapGPUTransferBuffer (ctx->gpu_device, i_upload);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (ctx->gpu_device);
  SDL_GPUCopyPass *pass = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTransferBufferLocation const vloc{ v_upload, 0 };
  SDL_GPUBufferRegion const vreg{ m_vertex_buffer, 0, vb.size };
  SDL_UploadToGPUBuffer (pass, &vloc, &vreg, true);

  SDL_GPUTransferBufferLocation const iloc{ i_upload, 0 };
  SDL_GPUBufferRegion const ireg{ m_index_buffer, 0, ib.size };
  SDL_UploadToGPUBuffer (pass, &iloc, &ireg, true);

  SDL_EndGPUCopyPass (pass);
  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (ctx->gpu_device, v_upload);
  SDL_ReleaseGPUTransferBuffer (ctx->gpu_device, i_upload);
}

void
gfx::model_3d::destroy_gpu_buffers (gfx::render_context *ctx)
{
  if (m_vertex_buffer != nullptr) {
    SDL_ReleaseGPUBuffer (ctx->gpu_device, m_vertex_buffer);
    m_vertex_buffer = nullptr;
  }

  if (m_index_buffer != nullptr) {
    SDL_ReleaseGPUBuffer (ctx->gpu_device, m_index_buffer);
    m_index_buffer = nullptr;
  }
}

std::shared_ptr<gfx::model_3d>
gfx::model_3d::make_unit_quad ()
{
  return make_single_primitive_model (build_quad_primitive ());
}

std::shared_ptr<gfx::model_3d>
gfx::model_3d::make_unit_cube ()
{
  return make_single_primitive_model (build_cube_primitive ());
}

std::shared_ptr<gfx::model_3d>
gfx::model_3d::make_unit_prism ()
{
  return make_unit_cube ();
}

std::shared_ptr<gfx::model_3d>
gfx::model_3d::make_unit_cylinder ()
{
  return make_single_primitive_model (build_cylinder_primitive ());
}

std::shared_ptr<gfx::model_3d>
gfx::model_3d::make_unit_sphere ()
{
  return make_single_primitive_model (build_uv_sphere_primitive ());
}

bool
gfx::model_3d::get_scene_bounds (size_t scene_index, glm::vec3 &out_min,
                                 glm::vec3 &out_max) const
{
  if (scene_index >= m_scene_bounds.size ()) {
    return false;
  }

  const aabb &bounds = m_scene_bounds[scene_index];
  if (!bounds.valid) {
    return false;
  }

  out_min = bounds.min;
  out_max = bounds.max;
  return true;
}

void
gfx::model_3d::rebuild_scene_bounds ()
{
  m_scene_bounds.clear ();
  m_scene_bounds.resize (scenes.size ());

  for (size_t i = 0; i < scenes.size (); ++i) {
    aabb scene_box;

    for (const gfx::node &root : scenes[i].roots) {
      scene_box.expand (compute_node_bounds_recursive (root));
    }

    m_scene_bounds[i] = scene_box;
  }
}

} // namespace wsl
