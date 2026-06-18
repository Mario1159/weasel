#include "model_loader.hpp"
#include "fastgltf/types.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>

#include <SDL3_image/SDL_image.h>

#include <fastgltf/util.hpp>
#include <filesystem>
#include <functional>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include "gfx/model_3d.hpp"
#include "rsc/cpu_model.hpp"
#include "wsl/log/log.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "../math/mikktspace_header"

// ---- Tangent generation (MikkTSpace) for any primitive type that has:
// prim.indices (vector<uint32_t>)
// prim.vertices (vector<...>) where vertex has pos, normal, uv, tangent

namespace wsl
{

namespace
{

constexpr std::array<rsc::model_loader::primitive_model_info, 5>
    k_builtin_primitives{ {
        { rsc::model_loader::primitive_model::quad, "builtin://quad", "Quad" },
        { rsc::model_loader::primitive_model::cube, "builtin://cube", "Cube" },
        { rsc::model_loader::primitive_model::prism, "builtin://prism",
          "Prism" },
        { rsc::model_loader::primitive_model::cylinder, "builtin://cylinder",
          "Cylinder" },
        { rsc::model_loader::primitive_model::sphere, "builtin://sphere",
          "Sphere" },
    } };

} // anonymous namespace

namespace rsc
{

template <typename Prim>
void
model_loader::generate_tangents_mikktspace_any (Prim &prim) const
{
  if (prim.indices.empty () || prim.vertices.empty ()) {
    return;
  }

  struct mikkt_ctx
  {
    Prim *p;
  } ctx_data{ &prim };

  SMikkTSpaceInterface iface{};
  iface.m_getNumFaces = [] (const SMikkTSpaceContext *c) -> int {
    auto *p = static_cast<const mikkt_ctx *> (c->m_p_user_data)->p;
    return int (p->indices.size () / 3);
  };

  iface.m_getNumVerticesOfFace
      = [] (const SMikkTSpaceContext *, int) -> int { return 3; };

  iface.m_getPosition
      = [] (const SMikkTSpaceContext *c, float out_pos[3], int face, int vert) {
          auto *p = static_cast<const mikkt_ctx *> (c->m_p_user_data)->p;
          uint32_t const idx = p->indices[(face * 3) + vert];
          const auto &v = p->vertices[idx];
          out_pos[0] = v.pos.x;
          out_pos[1] = v.pos.y;
          out_pos[2] = v.pos.z;
        };

  iface.m_getNormal
      = [] (const SMikkTSpaceContext *c, float out_n[3], int face, int vert) {
          auto *p = static_cast<const mikkt_ctx *> (c->m_p_user_data)->p;
          uint32_t const idx = p->indices[(face * 3) + vert];
          const auto &v = p->vertices[idx];
          out_n[0] = v.normal.x;
          out_n[1] = v.normal.y;
          out_n[2] = v.normal.z;
        };

  iface.m_getTexCoord
      = [] (const SMikkTSpaceContext *c, float out_uv[2], int face, int vert) {
          auto *p = static_cast<const mikkt_ctx *> (c->m_p_user_data)->p;
          uint32_t const idx = p->indices[(face * 3) + vert];
          const auto &v = p->vertices[idx];
          out_uv[0] = v.uv.x;
          out_uv[1] = v.uv.y;
        };

  iface.m_setTSpaceBasic
      = [] (const SMikkTSpaceContext *c, const float tangent[3], float sign,
            int face, int vert) {
          auto *p = static_cast<const mikkt_ctx *> (c->m_p_user_data)->p;
          uint32_t const idx = p->indices[(face * 3) + vert];
          auto &v = p->vertices[idx];
          v.tangent = glm::vec4 (tangent[0], tangent[1], tangent[2], sign);
        };

  SMikkTSpaceContext mctx{};
  mctx.m_p_interface = &iface;
  mctx.m_p_user_data = &ctx_data;

  const bool ok = gen_tang_space_default (&mctx) != 0;
  if (!ok) {
    for (auto &v : prim.vertices) {
      v.tangent = glm::vec4 (1, 0, 0, 1);
    }
  }
}

void
model_loader::generate_tangents_mikktspace (raw::cpu_primitive &prim) const
{
  generate_tangents_mikktspace_any (prim);
}

model_loader::lod_info
model_loader::parse_lod_name (const std::string &name)
{
  lod_info info;

  auto pos = name.rfind ("_LOD");
  if (pos == std::string::npos) {
    return info;
  }

  std::string const base = name.substr (0, pos);
  std::string num = name.substr (pos + 4);

  wsl::log::rsc ()->debug ("MESH NAME: {}, NUM: {}", name, num);

  try {
    int const lod = std::stoi (num);
    info.base = base;
    info.lod = lod;
  } catch (...) {
  }

  return info;
}

glm::mat4
model_loader::fastgltf_mat4_to_glm (const fastgltf::math::fmat4x4 &m)
{
  return glm::mat4 (m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1],
                    m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3],
                    m[3][0], m[3][1], m[3][2], m[3][3]);
}

template <typename TexInfo>
raw::uv_xform
model_loader::get_uv_xform (const TexInfo &info) const
{
  raw::uv_xform xf{};
  if (!info.has_value ()) {
    return xf;
  }

  if (info->transform) {
    const auto &t = *info->transform;
    xf.offset = { t.uvOffset.x (), t.uvOffset.y () };
    xf.scale = { t.uvScale.x (), t.uvScale.y () };
    xf.rotation = t.rotation; // radians
    xf.valid = true;
  }

  return xf;
}

void
model_loader::collect_low_lod_meshes_recursive (
    const raw::cpu_node &node, std::unordered_set<int> &out) const
{
  if (!node.mesh_lods.empty ()) {
    out.insert (node.mesh_lods.back ());
  }

  for (const auto &child : node.children) {
    collect_low_lod_meshes_recursive (child, out);
  }
}

std::unordered_set<int>
model_loader::collect_low_lod_meshes (const raw::cpu_model &cpu) const
{
  std::unordered_set<int> keep;

  for (const auto &scene : cpu.scenes) {
    for (const auto &root : scene.roots) {
      collect_low_lod_meshes_recursive (root, keep);
    }
  }

  return keep;
}

std::span<const model_loader::primitive_model_info>
model_loader::builtin_primitives ()
{
  return k_builtin_primitives;
}

std::optional<model_loader::primitive_model>
model_loader::primitive_from_path (std::string_view path)
{
  for (const auto &primitive : k_builtin_primitives) {
    if (primitive.path == path) {
      return primitive.primitive;
    }
  }

  return std::nullopt;
}

std::string_view
model_loader::primitive_display_name (std::string_view path)
{
  for (const auto &primitive : k_builtin_primitives) {
    if (primitive.path == path) {
      return primitive.display_name;
    }
  }

  return {};
}

std::shared_ptr<gfx::model_3d>
model_loader::load_primitive (primitive_model primitive)
{
  switch (primitive) {
  case primitive_model::quad:
    return gfx::model_3d::make_unit_quad ();
  case primitive_model::cube:
    return gfx::model_3d::make_unit_cube ();
  case primitive_model::prism:
    return gfx::model_3d::make_unit_prism ();
  case primitive_model::cylinder:
    return gfx::model_3d::make_unit_cylinder ();
  case primitive_model::sphere:
    return gfx::model_3d::make_unit_sphere ();
  }

  return {};
}

glm::mat4
model_loader::compute_node_local_transform (const fastgltf::Node &node) const
{

  if (const auto *trs = std::get_if<fastgltf::TRS> (&node.transform)) {
    glm::vec3 const t (trs->translation.x (), trs->translation.y (),
                       trs->translation.z ());
    glm::vec3 const s (trs->scale.x (), trs->scale.y (), trs->scale.z ());
    glm::quat const r (trs->rotation.w (), trs->rotation.x (),
                       trs->rotation.y (), trs->rotation.z ());

    return glm::translate (glm::mat4 (1.F), t) * glm::mat4_cast (r)
           * glm::scale (glm::mat4 (1.F), s);
  }

  if (const auto *m = std::get_if<fastgltf::math::fmat4x4> (&node.transform)) {
    return fastgltf_mat4_to_glm (*m);
  }

  wsl::log::rsc ()->warn (
      "Node has no TRS or matrix transform, using identity.");
  return glm::mat4 (1.F);
}

gfx::model_3d
model_loader::upload_gpu (const raw::cpu_model &cpu) const
{
  auto session = begin_upload (cpu);

  while (!is_upload_complete (session)) {
    upload_next_batch (session, cpu, session.tasks.size ());
  }

  return finish_upload (session, cpu);
}

model_loader::upload_session
model_loader::begin_upload (const raw::cpu_model &cpu) const
{
  return begin_upload (cpu, upload_options{});
}

model_loader::upload_session
model_loader::begin_upload (const raw::cpu_model &cpu,
                            const upload_options &options) const
{
  upload_session session{ options };
  session.gpu_model.meshes.resize (cpu.meshes.size ());

  std::unordered_set<int> keep_meshes;
  if (options.lowest_lod_only) {
    keep_meshes = collect_low_lod_meshes (cpu);
  }

  for (size_t mesh_index = 0; mesh_index < cpu.meshes.size (); ++mesh_index) {
    if (options.lowest_lod_only
        && !keep_meshes.contains (static_cast<int> (mesh_index))) {
      continue;
    }

    const auto &mesh = cpu.meshes[mesh_index];

    for (size_t prim_index = 0; prim_index < mesh.primitives.size ();
         ++prim_index) {
      const auto &prim = mesh.primitives[prim_index];

      session.tasks.push_back ({ .kind = upload_task::type::primitive_begin,
                                 .mesh_index = mesh_index,
                                 .prim_index = prim_index });

      for (size_t offset = 0; offset < prim.vertices.size ();
           offset += options.vertex_chunk_size) {
        session.tasks.push_back (
            { .kind = upload_task::type::vertex_chunk,
              .mesh_index = mesh_index,
              .prim_index = prim_index,
              .offset = offset,
              .count = std::min (options.vertex_chunk_size,
                                 prim.vertices.size () - offset) });
      }

      for (size_t offset = 0; offset < prim.indices.size ();
           offset += options.index_chunk_size) {
        session.tasks.push_back (
            { .kind = upload_task::type::index_chunk,
              .mesh_index = mesh_index,
              .prim_index = prim_index,
              .offset = offset,
              .count = std::min (options.index_chunk_size,
                                 prim.indices.size () - offset) });
      }

      const auto &mat = prim.material;
      if (mat.base_color.tex || mat.metallic_roughness.tex || mat.normal.tex
          || mat.emissive.tex) {
        session.tasks.push_back ({ .kind = upload_task::type::texture,
                                   .mesh_index = mesh_index,
                                   .prim_index = prim_index });
      }
    }
  }

  return session;
}

void
model_loader::upload_next_batch (upload_session &session,
                                 const raw::cpu_model &cpu,
                                 size_t max_tasks) const
{
  size_t tasks_done = 0;

  while (session.next_task < session.tasks.size () && tasks_done < max_tasks) {
    const auto &task = session.tasks[session.next_task];
    const auto &src_mesh = cpu.meshes[task.mesh_index];
    const auto &src_prim = src_mesh.primitives[task.prim_index];
    auto &dst_mesh = session.gpu_model.meshes[task.mesh_index];

    switch (task.kind) {
    case upload_task::type::primitive_begin: {
      auto &dst = dst_mesh.primitives.emplace_back ();
      dst.vertices.reserve (src_prim.vertices.size ());
      dst.indices.reserve (src_prim.indices.size ());

      const auto &mat = src_prim.material;
      dst.mat.base_color_factor = mat.base_color_factor;
      dst.mat.metallic_factor = mat.metallic_factor;
      dst.mat.roughness_factor = mat.roughness_factor;
      dst.mat.emissive_factor = mat.emissive_factor;
      dst.mat.double_sided = mat.double_sided;
      break;
    }

    case upload_task::type::vertex_chunk: {
      auto &dst_prim = dst_mesh.primitives[task.prim_index];
      const size_t end = task.offset + task.count;

      for (size_t i = task.offset; i < end; ++i) {
        const auto &vertex = src_prim.vertices[i];
        dst_prim.vertices.push_back (
            { vertex.pos, vertex.normal, vertex.uv, vertex.tangent });
      }
      break;
    }

    case upload_task::type::index_chunk: {
      auto &dst_prim = dst_mesh.primitives[task.prim_index];
      const size_t end = task.offset + task.count;

      dst_prim.indices.insert (dst_prim.indices.end (),
                               src_prim.indices.begin () + task.offset,
                               src_prim.indices.begin () + end);
      break;
    }

    case upload_task::type::texture: {
      auto &dst_prim = dst_mesh.primitives[task.prim_index];
      const auto &mat = src_prim.material;

      dst_prim.mat.device = m_ctx->gpu_device;

      if (mat.base_color.tex) {
        dst_prim.mat.base_color_tex
            = create_gpu_texture (*mat.base_color.tex, true);
      }

      if (mat.metallic_roughness.tex) {
        dst_prim.mat.metallic_roughness_tex
            = create_gpu_texture (*mat.metallic_roughness.tex, false);
      }

      if (mat.normal.tex) {
        dst_prim.mat.normal_tex = create_gpu_texture (*mat.normal.tex, false);
      }

      if (mat.emissive.tex) {
        dst_prim.mat.emissive_tex
            = create_gpu_texture (*mat.emissive.tex, true);
      }

      // Create a shared sampler for this material's textures.
      if (dst_prim.mat.sampler == nullptr) {
        SDL_GPUSamplerCreateInfo sampler_info{};
        sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
        sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        sampler_info.max_lod = 1000.0F;
        dst_prim.mat.sampler
            = SDL_CreateGPUSampler (m_ctx->gpu_device, &sampler_info);
      }
      break;
    }
    }

    ++session.next_task;
    ++tasks_done;
  }
}

bool
model_loader::is_upload_complete (const upload_session &session)
{
  return session.next_task >= session.tasks.size ();
}

gfx::model_3d
model_loader::finish_upload (upload_session &session, const raw::cpu_model &cpu)
{
  auto &model = session.gpu_model;
  const bool lowest_lod_only = session.options.lowest_lod_only;

  model.scenes.reserve (cpu.scenes.size ());

  std::function<gfx::node (const raw::cpu_node &)> build_node;
  build_node = [&] (const raw::cpu_node &src) -> gfx::node {
    gfx::node node;
    node.local_transform = src.local_transform;

    if (!src.mesh_lods.empty ()) {
      if (lowest_lod_only) {
        const int lod = src.mesh_lods.back ();
        if (lod >= 0 && lod < static_cast<int> (model.meshes.size ())) {
          node.mesh_lods.push_back (&model.meshes[lod]);
        }
      } else {
        for (const int lod : src.mesh_lods) {
          if (lod >= 0 && lod < static_cast<int> (model.meshes.size ())) {
            node.mesh_lods.push_back (&model.meshes[lod]);
          }
        }
      }
    }

    for (const auto &child : src.children) {
      node.children.push_back (build_node (child));
    }

    return node;
  };

  for (const auto &cpu_scene : cpu.scenes) {
    gfx::scene scene;
    for (const auto &root : cpu_scene.roots) {
      scene.roots.push_back (build_node (root));
    }
    model.scenes.push_back (std::move (scene));
  }

  model.default_scene = cpu.default_scene;
  model.lod_groups.clear ();
  model.lod_groups.resize (cpu.lod_groups.size ());

  for (size_t group_index = 0; group_index < cpu.lod_groups.size ();
       ++group_index) {
    const auto &cpu_group = cpu.lod_groups[group_index];
    auto &gpu_group = model.lod_groups[group_index];

    if (cpu_group.empty ()) {
      continue;
    }

    if (lowest_lod_only) {
      const int mesh_index = cpu_group.back ();
      if (mesh_index >= 0
          && mesh_index < static_cast<int> (model.meshes.size ())) {
        gpu_group.levels.push_back (&model.meshes[mesh_index]);
      }
      continue;
    }

    gpu_group.levels.reserve (cpu_group.size ());
    for (const int mesh_index : cpu_group) {
      if (mesh_index >= 0
          && mesh_index < static_cast<int> (model.meshes.size ())) {
        gpu_group.levels.push_back (&model.meshes[mesh_index]);
      }
    }
  }

  model.rebuild_scene_bounds ();

  wsl::log::rsc ()->debug ("GPU LOD GROUP COUNT: {}", model.lod_groups.size ());

  return std::move (model);
}

bool
model_loader::extract_image_data (const fastgltf::Asset &asset,
                                  const fastgltf::Image &img,
                                  std::vector<uint8_t> &out,
                                  const std::filesystem::path &base_path)
{

  auto read_from_buffer = [&] (const auto &buffer_source,
                               const fastgltf::BufferView &view) -> bool {
    const uint8_t *ptr
        = reinterpret_cast<const uint8_t *> (buffer_source.bytes.data ())
          + view.byteOffset;
    out.assign (ptr, ptr + view.byteLength);
    return true;
  };

  auto handle_buffer_view
      = [&] (const fastgltf::sources::BufferView &src) -> bool {
    const auto &view = asset.bufferViews[src.bufferViewIndex];
    const auto &buffer = asset.buffers[view.bufferIndex];

    return std::visit (fastgltf::visitor{

                           [&] (const fastgltf::sources::Vector &v) -> bool {
                             return read_from_buffer (v, view);
                           },

                           [&] (const fastgltf::sources::ByteView &b) -> bool {
                             return read_from_buffer (b, view);
                           },

                           [&] (const fastgltf::sources::Array &a) -> bool {
                             return read_from_buffer (a, view);
                           },

                           [&] (auto &&) -> bool {
                             wsl::log::rsc ()->error ("buffer data = UNKNOWN");
                             return false;
                           } },
                       buffer.data);
  };

  auto handle_uri = [&] (const fastgltf::sources::URI &uri) -> bool {
    std::ifstream file (base_path / uri.uri.fspath (), std::ios::binary);
    if (!file) {
      wsl::log::rsc ()->error ("FAILED to open image file: {}",
                               (base_path / uri.uri.fspath ()).string ());
      return false;
    }

    out.assign (std::istreambuf_iterator<char> (file),
                std::istreambuf_iterator<char> ());
    return true;
  };

  auto handle_embedded_vector
      = [&] (const fastgltf::sources::Vector &v) -> bool {
    out.assign (reinterpret_cast<const uint8_t *> (v.bytes.data ()),
                reinterpret_cast<const uint8_t *> (v.bytes.data ())
                    + v.bytes.size ());
    return true;
  };

  return std::visit (
      fastgltf::visitor{ handle_buffer_view, handle_uri, handle_embedded_vector,
                         [&] (auto &&) -> bool {
                           wsl::log::rsc ()->error (
                               "source = UNKNOWN image source type");
                           return false;
                         } },
      img.data);
}

bool
model_loader::decode_rgba_image (const std::vector<uint8_t> &bytes, int &w,
                                 int &h, std::vector<uint8_t> &pixels)
{

  SDL_IOStream *rw = SDL_IOFromConstMem (bytes.data (), (int)bytes.size ());
  if (rw == nullptr) {
    wsl::log::rsc ()->error ("SDL_IOFromConstMem FAILED: {}", SDL_GetError ());
    return false;
  }

  SDL_Surface *surf = IMG_Load_IO (rw, 1);
  if (surf == nullptr) {
    wsl::log::rsc ()->error ("IMG_Load_IO FAILED: {}", SDL_GetError ());
    return false;
  }

  SDL_Surface *rgba = SDL_ConvertSurface (surf, SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface (surf);

  if (rgba == nullptr) {
    wsl::log::rsc ()->error ("SDL_ConvertSurface FAILED: {}", SDL_GetError ());
    return false;
  }

  w = rgba->w;
  h = rgba->h;

  pixels.resize (static_cast<size_t> (w * h * 4));
  std::memcpy (pixels.data (), rgba->pixels, pixels.size ());

  SDL_DestroySurface (rgba);
  return true;
}

std::shared_ptr<raw::cpu_model>
model_loader::load_cpu (const std::string &path) const
{
  auto cpu = std::make_shared<raw::cpu_model> ();

  std::filesystem::path const p (path);

  fastgltf::Parser parser (
      fastgltf::Extensions::KHR_mesh_quantization
      | fastgltf::Extensions::KHR_texture_transform
      | fastgltf::Extensions::KHR_materials_variants
      | fastgltf::Extensions::KHR_materials_unlit
      | fastgltf::Extensions::KHR_materials_emissive_strength);

  auto file = fastgltf::MappedGltfFile::FromPath (p);

  constexpr auto options = fastgltf::Options::LoadExternalBuffers
                           | fastgltf::Options::LoadExternalImages
                           | fastgltf::Options::AllowDouble
                           | fastgltf::Options::GenerateMeshIndices;

  auto asset = parser.loadGltf (file.get (), p.parent_path (), options);
  if (!asset) {
    wsl::log::rsc ()->error ("Failed to load glTF: {}", path);
    return {};
  }

  const auto &gltf = asset.get ();
  const std::filesystem::path base_path = p.parent_path ();

  cpu->meshes.resize (gltf.meshes.size ());

  // -------- meshes --------
  for (size_t mi = 0; mi < gltf.meshes.size (); ++mi) {
    const fastgltf::Mesh &gltf_mesh = gltf.meshes[mi];
    raw::cpu_mesh &out_mesh = cpu->meshes[mi];

    for (const fastgltf::Primitive &prim : gltf_mesh.primitives) {
      raw::cpu_primitive &out = out_mesh.primitives.emplace_back ();

      // ---- positions ----
      const auto &pos_acc
          = gltf.accessors[prim.findAttribute ("POSITION")->accessorIndex];

      out.vertices.resize (pos_acc.count);

      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3> (
          gltf, pos_acc, [&] (auto p, size_t i) {
            out.vertices[i].pos = { p.x (), p.y (), p.z () };
          });

      // ---- normals ----
      if (const auto *n = prim.findAttribute ("NORMAL");
          n != prim.attributes.end ()) {
        const auto &acc = gltf.accessors[n->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3> (
            gltf, acc, [&] (auto v, size_t i) {
              out.vertices[i].normal
                  = glm::normalize (glm::vec3 (v.x (), v.y (), v.z ()));
            });
      }

      // ---- uvs ----
      if (const auto *t = prim.findAttribute ("TEXCOORD_0");
          t != prim.attributes.end ()) {
        const auto &acc = gltf.accessors[t->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2> (
            gltf, acc, [&] (auto uv, size_t i) {
              out.vertices[i].uv = { uv.x (), uv.y () };
            });
      } else {
        for (auto &v : out.vertices) {
          v.uv = glm::vec2 (0.0F);
        }
      }

      // ---- indices ----
      if (prim.indicesAccessor.has_value ()) {
        const auto &acc = gltf.accessors[*prim.indicesAccessor];
        out.indices.resize (acc.count);
        fastgltf::iterateAccessorWithIndex<uint32_t> (
            gltf, acc, [&] (uint32_t idx, size_t i) { out.indices[i] = idx; });
      } else {
        out.indices.resize (out.vertices.size ());
        for (size_t i = 0; i < out.indices.size (); ++i) {
          out.indices[i] = uint32_t (i);
        }
      }

      if (prim.materialIndex.has_value ()) {
        const auto &gltf_mat = gltf.materials[*prim.materialIndex];
        raw::cpu_material &mat = out.material;

        mat.base_color_factor = glm::vec4 (gltf_mat.pbrData.baseColorFactor[0],
                                           gltf_mat.pbrData.baseColorFactor[1],
                                           gltf_mat.pbrData.baseColorFactor[2],
                                           gltf_mat.pbrData.baseColorFactor[3]);

        mat.metallic_factor = gltf_mat.pbrData.metallicFactor;
        mat.roughness_factor = gltf_mat.pbrData.roughnessFactor;
        mat.double_sided = gltf_mat.doubleSided;
        mat.emissive_factor
            = glm::vec3 (gltf_mat.emissiveFactor[0], gltf_mat.emissiveFactor[1],
                         gltf_mat.emissiveFactor[2]);
        mat.emissive_strength = gltf_mat.emissiveStrength;
        mat.emissive_factor *= mat.emissive_strength;

        auto load_tex = [&] (const auto &tex_info, raw::cpu_tex_slot &slot) {
          slot = {};
          if (!tex_info.has_value ()) {
            return;
          }

          slot.uv = get_uv_xform (tex_info);

          const auto &tex = gltf.textures[tex_info->textureIndex];
          if (!tex.imageIndex.has_value ()) {
            return;
          }

          const auto &img = gltf.images[*tex.imageIndex];

          std::vector<uint8_t> image_bytes;
          std::vector<uint8_t> pixels;
          int w = 0;
          int h = 0;

          if (!extract_image_data (gltf, img, image_bytes, base_path)) {
            return;
          }
          if (!decode_rgba_image (image_bytes, w, h, pixels)) {
            return;
          }

          auto cpu_tex = std::make_shared<raw::cpu_texture> ();
          cpu_tex->width = w;
          cpu_tex->height = h;
          cpu_tex->pixels = std::move (pixels);

          slot.tex = std::move (cpu_tex);
        };

        load_tex (gltf_mat.pbrData.baseColorTexture, mat.base_color);
        load_tex (gltf_mat.pbrData.metallicRoughnessTexture,
                  mat.metallic_roughness);
        load_tex (gltf_mat.normalTexture, mat.normal);
        load_tex (gltf_mat.emissiveTexture, mat.emissive);
      }
      generate_tangents_mikktspace (out);
    }
  }

  // -------- build LOD groups --------
  std::unordered_map<std::string, std::vector<std::pair<int, int>>> groups;

  for (size_t i = 0; i < gltf.meshes.size (); ++i) {
    const auto &m = gltf.meshes[i];
    if (m.name.empty ()) {
      continue;
    }

    lod_info const info = parse_lod_name ((std::string)m.name);
    if (info.lod >= 0) {
      groups[info.base].push_back ({ info.lod, (int)i });
    }
  }

  for (auto &[base, lods] : groups) {
    std::sort (lods.begin (), lods.end (),
               [] (auto &a, auto &b) { return a.first < b.first; });

    std::vector<int> indices;
    for (auto &[lod, idx] : lods) {
      indices.push_back (idx);
    }

    cpu->lod_groups.push_back (std::move (indices));
  }

  wsl::log::rsc ()->debug ("LOD GROUP COUNT: {}", cpu->lod_groups.size ());

  // -------- scenes --------
  cpu->scenes.reserve (gltf.scenes.size ());

  for (const auto &gltf_scene : gltf.scenes) {
    raw::cpu_scene scn;

    for (size_t const node_index : gltf_scene.nodeIndices) {
      std::function<raw::cpu_node (size_t)> build_node;
      build_node = [&] (size_t idx) -> raw::cpu_node {
        const auto &src = gltf.nodes[idx];

        raw::cpu_node n;
        n.local_transform = compute_node_local_transform (src);

        if (src.meshIndex.has_value ()) {
          int const mesh_index = static_cast<int> (*src.meshIndex);

          bool assigned_group = false;

          for (const auto &group : cpu->lod_groups) {
            for (int const idx : group) {
              if (idx == mesh_index) {
                n.mesh_lods = group; // assign full LOD chain
                assigned_group = true;
                break;
              }
            }
            if (assigned_group) {
              break;
            }
          }

          if (!assigned_group) {
            n.mesh_lods.push_back (mesh_index);
          }
        }

        for (size_t const child : src.children) {
          n.children.push_back (build_node (child));
        }

        return n;
      };

      scn.roots.push_back (build_node (node_index));
    }

    cpu->scenes.push_back (std::move (scn));
  }

  // default scene
  if (gltf.defaultScene.has_value ()) {
    cpu->default_scene = *gltf.defaultScene;
  } else {
    cpu->default_scene = 0;
  }

  return cpu;
}

std::shared_ptr<gfx::model_3d>
model_loader::operator() (const std::string &path) const
{
  if (auto primitive = primitive_from_path (path)) {
    return load_primitive (*primitive);
  }

  if (path.rfind ("builtin://", 0) == 0) {
    wsl::log::rsc ()->warn ("Unknown builtin model path '{}'", path);
    return {};
  }

  auto cpu = load_cpu (path);
  if (!cpu) {
    return {};
  }

  auto gpu = upload_gpu (*cpu);
  return std::make_shared<gfx::model_3d> (std::move (gpu));
}

std::shared_ptr<gfx::model_3d>
model_loader::operator() (gfx::model_3d &&ready_model) const
{
  return std::make_shared<gfx::model_3d> (std::move (ready_model));
}

SDL_GPUTexture *
model_loader::create_gpu_texture (const raw::cpu_texture &tex, bool srgb) const
{

  if ((m_ctx == nullptr) || (m_ctx->gpu_device == nullptr)) {
    wsl::log::rsc ()->critical ("GPU context not initialized.");
    return nullptr;
  }

  auto mip_count_2d = [] (uint32_t w, uint32_t h) {
    uint32_t levels = 1;
    while (w > 1 || h > 1) {
      w = (w > 1) ? (w >> 1) : 1;
      h = (h > 1) ? (h >> 1) : 1;
      ++levels;
    }
    return levels;
  };

  SDL_GPUTextureCreateInfo ti{};
  ti.type = SDL_GPU_TEXTURETYPE_2D;
  ti.format = srgb ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB
                   : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  ti.width = tex.width;
  ti.height = tex.height;
  ti.layer_count_or_depth = 1;
  ti.num_levels = mip_count_2d (tex.width, tex.height);
  ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

  SDL_GPUTexture *gpu_tex = SDL_CreateGPUTexture (m_ctx->gpu_device, &ti);
  if (gpu_tex == nullptr) {
    wsl::log::rsc ()->error ("SDL_CreateGPUTexture failed.");
    return nullptr;
  }

  SDL_GPUTransferBufferCreateInfo tbi{};
  tbi.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tbi.size = tex.pixels.size ();

  SDL_GPUTransferBuffer *upload
      = SDL_CreateGPUTransferBuffer (m_ctx->gpu_device, &tbi);

  void *mapped = SDL_MapGPUTransferBuffer (m_ctx->gpu_device, upload, false);
  memcpy (mapped, tex.pixels.data (), tex.pixels.size ());
  SDL_UnmapGPUTransferBuffer (m_ctx->gpu_device, upload);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer (m_ctx->gpu_device);
  SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass (cmd);

  SDL_GPUTextureTransferInfo const src{ upload, 0, (Uint32)tex.width,
                                        (Uint32)tex.height };

  SDL_GPUTextureRegion const dst{ .texture = gpu_tex,
                                  .mip_level = 0,
                                  .layer = 0,
                                  .x = 0,
                                  .y = 0,
                                  .z = 0,
                                  .w = (Uint32)tex.width,
                                  .h = (Uint32)tex.height,
                                  .d = 1 };

  SDL_UploadToGPUTexture (cp, &src, &dst, true);
  SDL_EndGPUCopyPass (cp);

  if (ti.num_levels > 1) {
    SDL_GenerateMipmapsForGPUTexture (cmd, gpu_tex);
  }

  SDL_SubmitGPUCommandBuffer (cmd);

  SDL_ReleaseGPUTransferBuffer (m_ctx->gpu_device, upload);

  return gpu_tex;
}

} // namespace rsc

} // namespace wsl
