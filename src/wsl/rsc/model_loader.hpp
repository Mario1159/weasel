#pragma once

#include <entt/resource/loader.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <fastgltf/types.hpp>
#include <spdlog/spdlog.h>

#include "../gfx/model_3d.hpp"
#include "../gfx/render_context.hpp"
#include "cpu_model.hpp"


namespace wsl
{

namespace rsc
{

/**
 * Resource loader for 3D models (glTF, etc.).
 *
 * This loader handles parsing glTF files into CPU-side data (raw::cpu_model)
 * and managing the asynchronous upload of that data to GPU memory (gfx::model_3d).
 * It also supports generating MikkTSpace tangents and parsing LOD naming conventions.
 */
struct model_loader final : entt::resource_loader<gfx::model_3d>
{
  /** Identifies the type of built-in primitive model. */
  enum class primitive_model
  {
    quad,
    cube,
    prism,
    cylinder,
    sphere,
  };

  /** Contains metadata about a built-in primitive. */
  struct primitive_model_info
  {
    primitive_model primitive{};
    std::string_view path;
    std::string_view display_name;
  };

  /** Represents a single unit of work in the asynchronous GPU upload process. */
  struct upload_task
  {
    enum class type
    {
      primitive_begin,
      vertex_chunk,
      index_chunk,
      texture
    };

    type kind{};
    size_t mesh_index = 0;
    size_t prim_index = 0;
    size_t offset = 0;
    size_t count = 0;
  };

  /** Configuration options for the GPU upload process. */
  struct upload_options
  {
    /** If true, only the lowest detail mesh is uploaded (useful for previews). */
    bool lowest_lod_only;
    /** Number of vertices to upload in a single batch. */
    size_t vertex_chunk_size;
    /** Number of indices to upload in a single batch. */
    size_t index_chunk_size;

    upload_options (bool lowest_lod_only = false,
                    size_t vertex_chunk_size = 512,
                    size_t index_chunk_size = 1024)
        : lowest_lod_only (lowest_lod_only),
          vertex_chunk_size (vertex_chunk_size),
          index_chunk_size (index_chunk_size)
    {
    }
  };

  /** Tracks the state of an active asynchronous GPU upload for one model. */
  struct upload_session
  {
    upload_options options;
    gfx::model_3d gpu_model;
    std::vector<upload_task> tasks;
    size_t next_task = 0;

    upload_session ()  {}
    explicit upload_session (upload_options options) : options (options) {}
  };

  /**
 * Constructs a model loader with a render context.
 * :param ctx: Pointer to the render context (must be valid for GPU uploads).
 */
  explicit model_loader (gfx::render_context *ctx) : m_ctx (ctx) {}

  /** Returns a list of all supported built-in primitives. */
  static std::span<const primitive_model_info> builtin_primitives ();

  /** Attempts to identify a built-in primitive from its engine path. */
  static std::optional<primitive_model>
  primitive_from_path (std::string_view path);

  /** Returns the display name of a built-in primitive. */
  static std::string_view primitive_display_name (std::string_view path);

  /** entt requirement: handle moving a ready model into a resource handle. */
  std::shared_ptr<gfx::model_3d> operator() (gfx::model_3d &&ready_model) const;

  /**
 * Loads or retrieves a model from the specified path.
 *
 * If the path starts with "builtin://", it returns a built-in primitive.
 * Otherwise, it loads a glTF file and performs a synchronous upload.
 */
  std::shared_ptr<gfx::model_3d> operator() (const std::string &path) const;

  /** Directly creates a GPU model for a built-in primitive. */
  static std::shared_ptr<gfx::model_3d>
  load_primitive (primitive_model primitive) ;

  /**
 * Uploads a CPU-side model to the GPU.
 * :param cpu: The source CPU model data.
 * :return: The resulting GPU-side model.
 */
  gfx::model_3d upload_gpu (const raw::cpu_model &cpu) const;

  /**
 * Loads a model from disk into CPU memory.
 * :param path: Path to the model file.
 * :return: Shared pointer to the CPU model data, or nullptr on failure.
 */
  std::shared_ptr<raw::cpu_model> load_cpu (const std::string &path) const;

  /** Begins an asynchronous upload session for a CPU model. */
  upload_session begin_upload (const raw::cpu_model &cpu) const;

  /**
 * Begins an asynchronous upload session with custom options.
 * :param cpu: The source CPU model data.
 * :param options: Custom upload options.
 */
  upload_session begin_upload (const raw::cpu_model &cpu,
                               const upload_options &options) const;

  /**
 * Processes a batch of upload tasks for an active session.
 * :param session: Reference to the upload session.
 * :param cpu: The source CPU model data.
 * :param max_tasks: Maximum number of tasks to process in this batch.
 */
  void upload_next_batch (upload_session &session, const raw::cpu_model &cpu,
                          size_t max_tasks) const;

  /** Returns true if all tasks in the session are complete. */
  static bool is_upload_complete (const upload_session &session) ;

  /**
 * Finalizes an upload session and returns the GPU model.
 * :param session: Reference to the completed upload session.
 * :param cpu: The source CPU model data.
 */
  static gfx::model_3d finish_upload (upload_session &session,
                               const raw::cpu_model &cpu) ;

  /** Helper to create a GPU texture from CPU texture data. */
  SDL_GPUTexture *create_gpu_texture (const raw::cpu_texture &tex, bool srgb) const;

private:
  struct lod_info
  {
    std::string base;
    int lod = -1;
  };

  gfx::render_context *m_ctx;

  template <typename Prim>
  void generate_tangents_mikktspace_any (Prim &prim) const;
  void generate_tangents_mikktspace (raw::cpu_primitive &prim) const;
  static lod_info parse_lod_name (const std::string &name) ;
  static glm::mat4 fastgltf_mat4_to_glm (const fastgltf::math::fmat4x4 &m) ;

  template <typename TexInfo>
  raw::uv_xform get_uv_xform (const TexInfo &info) const;

  void collect_low_lod_meshes_recursive (const raw::cpu_node &node,
                                         std::unordered_set<int> &out) const;
  std::unordered_set<int> collect_low_lod_meshes (const raw::cpu_model &cpu) const;

  glm::mat4 compute_node_local_transform (const fastgltf::Node &node) const;

  static bool extract_image_data (const fastgltf::Asset &asset,
                           const fastgltf::Image &img,
                           std::vector<uint8_t> &out,
                           const std::filesystem::path &base_path) ;

  static bool decode_rgba_image (const std::vector<uint8_t> &bytes, int &w, int &h,
                          std::vector<uint8_t> &pixels) ;
};

} // namespace rsc

} // namespace wsl
