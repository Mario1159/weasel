#pragma once

#include "cubemap_loader.hpp"
#include "image_loader.hpp"
#include "model_loader.hpp"
#include "project_loader.hpp"
#include "resource_ids.hpp"
#include "resource_ref.hpp"
#include "scene_loader.hpp"
#include "../comp/component_meta.hpp"
#include "shader_loader.hpp"
#include <SDL3_mixer/SDL_mixer.h>

#include <entt/entt.hpp>
#include <entt/resource/cache.hpp>
#include <entt/resource/resource.hpp>
#include <future>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace wsl
{

namespace comp::singl
{
class runtime_context;
class editor_context;
}

/*!
 * \brief Contains types and classes for managing engine resources, scenes, and
 * project metadata.
 */
namespace rsc
{

/*!
 * \brief Internal implementation details for resource management.
 */
namespace detail
{
}

/*!
 * \brief Core structures for raw, CPU-side asset representation before GPU
 * upload.
 */
namespace raw
{
}

/*!
 * \brief Input/Output and serialization logic for scenes and resource
 * references.
 */
namespace io
{
}

/*!
 * \brief Tools for interacting with the CMake File API and project
 * configuration.
 */
namespace cmake
{
}

using namespace entt::literals;
constexpr entt::id_type builtin_skybox_procedural
    = "builtin/skybox_procedural"_hs;

/*! \brief Represents the current loading state of a 3D model. */
enum class model_state
{
  not_loaded,
  loading_cpu,
  preparing_gpu,
  uploading_gpu,
  loaded
};
/*! \brief Represents the current loading state of an image. */
enum class image_state
{
  not_loaded,
  loading,
  loaded
};
/*! \brief Represents the current loading state of a cubemap. */
enum class cubemap_state
{
  not_loaded,
  loading,
  loaded
};
/*! \brief Represents the current loading state of a scene. */
enum class scene_state
{
  not_loaded,
  loading,
  loaded
};

/*! \brief Represents the current loading state of an audio asset. */
enum class audio_state
{
  not_loaded,
  loading,
  loaded
};

/*! \brief Represents the current loading state of a UI layout. */
enum class ui_layout_state
{
  not_loaded,
  loaded
};

/*! \brief Represents the current loading state of a shader module. */
enum class shader_state
{
  not_loaded,
  loading,
  loaded
};

/*!
 * \brief Metadata for a 3D model resource.
 */
struct model_resource_info
{
  entt::id_type id{};  //!< Unique identifier.
  std::string path;    //!< Path to the model file.
  std::string name;    //!< Human-readable name.
  model_state state{}; //!< Current loading state.
  bool preview_owned
      = false; //!< Whether the model is currently used as a preview.
  bool lowest_lod_only = false; //!< Whether only the lowest LOD is loaded.
};

/*!
 * \brief Metadata for an image resource.
 */
struct image_resource_info
{
  entt::id_type id{};  //!< Unique identifier.
  std::string path;    //!< Path to the image file.
  std::string name;    //!< Human-readable name.
  image_state state{}; //!< Current loading state.
};

/*!
 * \brief Metadata for a cubemap resource.
 */
struct cubemap_resource_info
{
  entt::id_type id{};    //!< Unique identifier.
  std::string path;      //!< Path to the cubemap file.
  std::string name;      //!< Human-readable name.
  cubemap_state state{}; //!< Current loading state.
};

/*!
 * \brief Metadata for a scene resource.
 */
struct scene_resource_info
{
  entt::id_type id{};     //!< Unique identifier.
  std::string path;       //!< Path to the scene file.
  std::string name;       //!< Human-readable name.
  scene_state state{};    //!< Current loading state.
  bool is_prefab = false; //!< Whether the scene is a prefab.
};

/*!
 * \brief Metadata for an audio resource.
 */
struct audio_resource_info
{
  entt::id_type id{};  //!< Unique identifier.
  std::string path;    //!< Path to the audio file.
  std::string name;    //!< Human-readable name.
  audio_state state{}; //!< Current loading state.
};

/*!
 * \brief Metadata for a UI layout resource.
 */
struct ui_layout_resource_info
{
  entt::id_type id{};      //!< Unique identifier.
  std::string path;        //!< Path to the UI layout file.
  std::string name;        //!< Human-readable name.
  ui_layout_state state{}; //!< Current loading state.
};

/*!
 * \brief Metadata for a font resource.
 */
struct font_resource_info
{
  entt::id_type id{}; //!< Unique identifier.
  std::string path;   //!< Path to the font file.
  std::string name;   //!< Human-readable name.
};

/*!
 * \brief Metadata for a shader resource.
 */
struct shader_resource_info
{
  entt::id_type id{};   //!< Unique identifier.
  std::string path;     //!< Path to the shader file.
  std::string name;     //!< Human-readable name.
  shader_state state{}; //!< Current loading state.
};

namespace detail
{

/*!
 * \brief Internal record tracking the state and data of a model resource.
 */
struct model_record
{
  std::string path;                            //!< Path to the model file.
  std::string name;                            //!< Human-readable name.
  model_state state = model_state::not_loaded; //!< Current loading state.
  std::future<std::shared_ptr<raw::cpu_model>>
      job{};                                  //!< Async CPU loading job.
  std::shared_ptr<raw::cpu_model> cpu_data{}; //!< Loaded CPU data.
  std::unique_ptr<model_loader::upload_session>
      upload{}; //!< Active GPU upload session.
};

/*!
 * \brief Internal record tracking the state and data of an image resource.
 */
struct image_record
{
  std::string path;                            //!< Path to the image file.
  std::string name;                            //!< Human-readable name.
  image_state state = image_state::not_loaded; //!< Current loading state.
  std::future<std::shared_ptr<raw::image_cpu>> job{}; //!< Async loading job.
};

/*!
 * \brief Internal record tracking the state and data of a cubemap resource.
 */
struct cubemap_record
{
  std::string path; //!< Path to the cubemap file.
  std::string name; //!< Human-readable name.
  cubemap_state state = cubemap_state::not_loaded;  //!< Current loading state.
  std::future<std::shared_ptr<gfx::cubemap>> job{}; //!< Async loading job.
};

/*!
 * \brief Internal record tracking the state and data of a scene resource.
 */
struct scene_record
{
  std::string path;                            //!< Path to the scene file.
  std::string name;                            //!< Human-readable name.
  scene_state state = scene_state::not_loaded; //!< Current loading state.
  bool is_prefab = false; //!< Whether the scene is a prefab.
  std::future<std::shared_ptr<rsc::scene>> job{}; //!< Async loading job.
};

/*!
 * \brief Internal record tracking the state and data of an audio resource.
 */
struct audio_record
{
  std::string path;                            //!< Path to the audio file.
  std::string name;                            //!< Human-readable name.
  audio_state state = audio_state::not_loaded; //!< Current loading state.
  MIX_Audio *audio = nullptr; //!< Pointer to loaded audio data.
};

/*!
 * \brief Internal record tracking the state of a UI layout resource.
 */
struct ui_layout_record
{
  std::string path; //!< Path to the UI layout file.
  std::string name; //!< Human-readable name.
  ui_layout_state state = ui_layout_state::not_loaded; //!< Current state.
};

/*!
 * \brief Internal record tracking the location of a font resource.
 */
struct font_record
{
  std::string path; //!< Path to the font file.
  std::string name; //!< Human-readable name.
};

/*!
 * \brief Internal record tracking the state of a shader resource.
 */
struct shader_record
{
  std::string path;                              //!< Path to the shader file.
  std::string name;                              //!< Human-readable name.
  shader_state state = shader_state::not_loaded; //!< Current loading state.
};

} // namespace detail

class resource_manager
{
public:
  using model_handle = entt::resource<gfx::model_3d>;
  using image_handle = entt::resource<gfx::image>;
  using cubemap_handle = entt::resource<gfx::cubemap>;
  using scene_handle = entt::resource<rsc::scene>;
  using shader_handle = entt::resource<gfx::shader_module>;

  /*!
   * \brief Construct a resource manager for the runtime.
   * \param runtime_ctx Pointer to the runtime context (non-owning).
   * \param engine_res_path Path to engine-provided resources used as a base for
   * resolution.
   */
  explicit resource_manager (comp::singl::runtime_context *runtime_ctx,
                             const std::string &engine_res_path = ".",
                             bool manages_runtime_state = true);
  /*! \brief Destroy the resource manager and release owned resources. */
  ~resource_manager ();

  /*! \brief Final shutdown used when the owning runtime is being destroyed. */
  void shutdown ();

  /*! \brief Assign an editor context for editor-only behaviors (non-owning).
   * \param editor_ctx Editor context pointer, or nullptr to clear.
   */
  void set_editor_context (comp::singl::editor_context *editor_ctx);

  /*! \brief Poll and finalize any asynchronous upload jobs started for GPU
   * resources. This should be called from the main thread to move resources
   * from CPU-side jobs into GPU upload sessions when ready.
   */
  void update_async_uploads ();

  /*! \brief Register a model path and return its id without loading. */
  model_id register_model (const std::string &path);

  /*! \brief Import a model into the project and optionally request loading.
   * \param path Filesystem path to the model asset.
   * \param request_load If true, start asynchronous loading immediately.
   * \return The assigned model id.
   */
  model_id import_model (const std::string &path, bool request_load = true);

  /*! \brief Load a model resource handle (may start loading if not ready).
   * \param id Model id to load.
   * \return A handle to the model resource.
   */
  model_handle load (model_id id);

  /*! \brief Load a resource given a generic resource_ref. */
  void load (io::resource_ref ref);

  /*! \brief Unload a previously registered model id.
   * \param id Model id to unload.
   */
  void unload (model_id id);

  /*! \brief Unload a resource referenced by a generic resource_ref. */
  void unload (io::resource_ref ref);

  /*! \brief Get a model handle for the given id.
   * \return An entt::resource handle; if the resource is not loaded the
   *         returned handle may be empty or trigger a load when accessed.
   */
  model_handle get (model_id id);

  /*! \brief Query the current loading state of a model id. */
  model_state state (model_id id) const;

  /*! \brief Check whether the manager knows about the given model id. */
  bool contains (model_id id) const;

  /*! \brief Find a registered model id by the original path if present. */
  std::optional<model_id> find_model_by_path (const std::string &path) const;

  /*! \brief Retrieve metadata about a registered model id. */
  std::optional<model_resource_info> info (model_id id) const;

  /*! \brief List metadata for all known models. */
  std::vector<model_resource_info> list_models () const;

  /*! \brief Load a preview model optimized to the lowest LOD for quick display.
   * \param id Model id to load as preview.
   */
  void load_preview_model_low_lod (model_id id);

  /*! \brief Get the currently active preview model id, or entt::null. */
  model_id current_preview_model () const;

  /*! \brief Unload the current preview model if one is active. */
  void unload_preview_model ();

  /*! \brief Release preview ownership for a model if the provided id matches.
   * This clears internal preview ownership flags without unloading the model.
   */
  void release_preview_ownership_if_matches (model_id id);

  /*! \brief Query whether the given model id is currently owned as a preview.
   */
  bool is_preview_owned (model_id id) const;

  /*! \brief Register an image asset path without loading. */
  image_id register_image (const std::string &path);

  /*! \brief Import an image into the project and optionally request loading.
   * \param path Path to the image file.
   * \param request_load If true, start loading immediately.
   */
  image_id import_image (const std::string &path, bool request_load = true);

  /*! \brief Load an image handle for use by the renderer. */
  image_handle load (image_id id);

  /*! \brief Unload the image resource referenced by id. */
  void unload (image_id id);

  /*! \brief Get an image handle for the given id. */
  image_handle get (image_id id);

  /*! \brief Query the loading state of an image id. */
  image_state state (image_id id) const;

  /*! \brief Check if an image id is known to the manager. */
  bool contains (image_id id) const;

  /*! \brief Retrieve metadata for an image id. */
  std::optional<image_resource_info> info (image_id id) const;

  /*! \brief List all registered images and their metadata. */
  std::vector<image_resource_info> list_images () const;

  /*! \brief Register a cubemap asset path. */
  cubemap_id register_cubemap (const std::string &path);

  /*! \brief Import a cubemap and optionally request loading. */
  cubemap_id import_cubemap (const std::string &path, bool request_load = true);

  /*! \brief Load a cubemap handle for rendering. */
  cubemap_handle load (cubemap_id id);

  /*! \brief Unload a cubemap by id. */
  void unload (cubemap_id id);

  /*! \brief Get a cubemap handle for the given id. */
  cubemap_handle get (cubemap_id id);

  /*! \brief Query the load state of a cubemap id. */
  cubemap_state state (cubemap_id id) const;

  /*! \brief Check presence of cubemap id in manager. */
  bool contains (cubemap_id id) const;

  /*! \brief Get metadata for a cubemap id. */
  std::optional<cubemap_resource_info> info (cubemap_id id) const;

  /*! \brief List all registered cubemaps. */
  std::vector<cubemap_resource_info> list_cubemaps () const;

  /*! \brief Register a scene asset path. */
  scene_id register_scene (const std::string &path);

  /*! \brief Import a scene (or prefab) and optionally start loading.
   * \param path Filesystem path to the scene or prefab.
   * \param request_load Start asynchronous load if true.
   */
  scene_id import_scene (const std::string &path, bool request_load = true);

  /*! \brief Load a scene resource handle. */
  scene_handle load (scene_id id);

  /*! \brief Unload a scene resource. */
  void unload (scene_id id);

  /*! \brief Get a scene resource handle for the given id. */
  scene_handle get (scene_id id);

  /*! \brief Return a raw pointer to a loaded scene instance or nullptr if not
   * loaded. The returned pointer is non-owning and valid until the scene is
   * unloaded.
   */
  scene *find_loaded_scene (scene_id id) const;

  /*! \brief Activate the scene with the provided id as the active scene.
   * \return True if activation succeeded.
   */
  bool activate_scene (scene_id id);

  /*! \brief Instantiate a prefab scene under the optional parent entity. */
  void instantiate_prefab (scene_id id, entt::entity parent = entt::null);

  /*! \brief Query the loading state of a scene id. */
  scene_state state (scene_id id) const;

  /*! \brief Check whether the manager knows about the provided scene id. */
  bool contains (scene_id id) const;

  /*! \brief Retrieve metadata for a scene id. */
  std::optional<scene_resource_info> info (scene_id id) const;

  /*! \brief List metadata for all registered scenes. */
  std::vector<scene_resource_info> list_scenes () const;

  /*! \brief Save a scene to disk. \param scene Scene to save. \param path
   * Destination path. \param is_prefab Mark whether the saved scene is a
   * prefab.
   * \return True on success.
   */
  bool save_scene (const rsc::scene &scene, const std::string &path,
                   bool is_prefab = false);

  /*! \brief Register an audio asset path. */
  audio_id register_audio (const std::string &path);

  /*! \brief Import audio and optionally request loading. */
  audio_id import_audio (const std::string &path, bool request_load = true);

  /*! \brief Load audio and return a raw MIX_Audio pointer owned by the mixer.
   * The pointer is non-owning; do not free it manually.
   */
  MIX_Audio *load (audio_id id);

  /*! \brief Unload an audio asset by id. */
  void unload (audio_id id);

  /*! \brief Get the raw MIX_Audio pointer for the given id. */
  MIX_Audio *get (audio_id id);

  /*! \brief Query the state of an audio id. */
  audio_state state (audio_id id) const;

  /*! \brief Check presence of audio id. */
  bool contains (audio_id id) const;

  /*! \brief Get metadata for an audio id. */
  std::optional<audio_resource_info> info (audio_id id) const;

  /*! \brief List all registered audio assets. */
  std::vector<audio_resource_info> list_audio () const;

  /*! \brief Register a UI layout asset path. */
  ui_layout_id register_ui_layout (const std::string &path);

  /*! \brief Get metadata for a UI layout id. */
  std::optional<ui_layout_resource_info> info (ui_layout_id id) const;

  /*! \brief List all registered UI layouts. */
  std::vector<ui_layout_resource_info> list_ui_layouts () const;

  /*! \brief Register a font asset path. */
  font_id register_font (const std::string &path);

  /*! \brief Get metadata for a font id. */
  std::optional<font_resource_info> info (font_id id) const;

  /*! \brief List all registered fonts. */
  std::vector<font_resource_info> list_fonts () const;

  /*! \brief Register a shader asset path. */
  shader_id register_shader (const std::string &path);

  /*! \brief Load a shader module handle for the given id. */
  shader_handle load (shader_id id);

  /*! \brief Unload a shader module. */
  void unload (shader_id id);

  /*! \brief Get a shader handle for the given id. */
  shader_handle get (shader_id id);

  /*! \brief Query shader loading state. */
  shader_state state (shader_id id) const;

  /*! \brief Check whether a shader id is registered. */
  bool contains (shader_id id) const;

  /*! \brief Get metadata for a shader id. */
  std::optional<shader_resource_info> info (shader_id id) const;

  /*! \brief List all registered shaders and metadata. */
  std::vector<shader_resource_info> list_shaders () const;

  /*! \brief Set the base engine resource path used to resolve engine-provided
   * assets. */
  void set_engine_resource_path (const std::string &path);

  /*! \brief Get the configured engine resource base path. */
  std::string
  get_engine_resource_path () const
  {
    return m_wsl_resource_path;
  }

  /*! \brief Create a new project from the provided project descriptor. */
  bool new_project (const rsc::project &proj);

  /*! \brief Load a project file from disk and set it as active. */
  bool load_project (const std::string &path);

  /*! \brief Return the currently active project if any. */
  std::shared_ptr<rsc::project> current_project () const;

  /*! \brief Unload and clear all managed resources, releasing GPU and CPU
   * memory. */
  void clear_all_resources (bool restore_builtin_defaults = true);

  /*! \brief Access to the internal audio mixer instance (non-owning). */
  MIX_Mixer *
  mixer () const
  {
    return m_mixer;
  }

  // Path resolution helpers
  /*! \brief Resolve a possibly-relative asset path to an absolute
   * engine/project path. */
  std::string resolve_path (const std::string &path) const;

  /*! \brief Get the filesystem path for a registered model id. */
  std::string get_resource_path (model_id id) const;

  /*! \brief Get the filesystem path for a registered cubemap id. */
  std::string get_resource_path (cubemap_id id) const;

  /*! \brief Get the filesystem path for a registered audio id. */
  std::string get_resource_path (audio_id id) const;

  /*! \brief Get the filesystem path for a generic resource reference. */
  std::string get_path (io::resource_ref ref) const;

  /*! \brief Helper providing thread-local access to the active resource manager
   * during serialization operations. This is internal and should be set by the
   * manager when performing (de)serialization.
   */
  struct serialization_context
  {
    static resource_manager *&
    get ()
    {
      static thread_local resource_manager *mgr = nullptr;
      return mgr;
    }
  };

private:
  comp::singl::runtime_context *m_runtime_ctx;
  comp::singl::editor_context *m_editor_ctx = nullptr;

  MIX_Mixer *m_mixer = nullptr;

  std::unordered_map<entt::id_type, detail::image_record> m_image_table;
  std::unordered_map<entt::id_type, detail::cubemap_record> m_cubemap_table;
  std::unordered_map<entt::id_type, detail::scene_record> m_scene_table;
  std::unordered_map<entt::id_type, detail::model_record> m_model_table;
  std::unordered_map<entt::id_type, detail::audio_record> m_audio_table;
  std::unordered_map<entt::id_type, detail::ui_layout_record> m_ui_layout_table;
  std::unordered_map<entt::id_type, detail::font_record> m_font_table;
  std::unordered_map<entt::id_type, detail::shader_record> m_shader_table;

  entt::resource_cache<rsc::scene, scene_loader> m_scenes;
  std::unordered_map<entt::id_type, rsc::scene *> m_loaded_scene_instances;
  entt::id_type m_preferred_default_scene_id = entt::null;
  bool m_waiting_for_preferred_default_scene = false;

  entt::resource_cache<gfx::model_3d, model_loader> m_models;
  entt::resource_cache<gfx::cubemap, cubemap_loader> m_cubemaps;
  entt::resource_cache<gfx::image, image_loader> m_images;
  entt::resource_cache<gfx::shader_module, shader_loader> m_shaders;

  std::shared_ptr<rsc::project> m_active_project;
  rsc::project_loader m_project_loader;
  std::unordered_map<std::string, entt::id_type> m_model_ids_by_path;
  std::unordered_map<std::string, entt::id_type> m_audio_ids_by_path;

  // preview temp state
  entt::id_type m_preview_model_id = entt::null;

  // ids that should build/upload only the lowest LOD
  std::unordered_set<entt::id_type> m_low_lod_only_models;

  // cancellation sets (avoid blocking future destructor)
  std::unordered_set<entt::id_type> m_cancel_models;
  std::unordered_set<entt::id_type> m_cancel_images;
  std::unordered_set<entt::id_type> m_cancel_cubemaps;
  std::unordered_set<entt::id_type> m_cancel_scenes;

  std::string m_wsl_resource_path = ".";

  bool m_clearing = false;
  bool m_shutdown = false;
  bool m_manages_runtime_state = true;

  struct project_load_job
  {
    std::shared_ptr<project> project_data;
    std::future<project_assets> assets_job;
    bool has_runtime_code = false;
  };
  std::optional<project_load_job> m_active_project_load;

  /*! \brief Register built-in engine models used as placeholders or defaults.
   */
  void register_builtin_models ();

  /*! \brief Register built-in cubemaps used by engine defaults. */
  void register_builtin_cubemaps ();
};

struct resource_manager_view : comp::singleton_component
{
  resource_manager *manager = nullptr;

  resource_manager_view () = default;
  explicit resource_manager_view (resource_manager *manager) : manager (manager)
  {
  }

  /*! \brief Register reflection metadata used by the editor for this view. */
  static void register_meta ();

  /*! \brief Draw a compact ImGui inspector for the resource manager.
   * \param label Optional UI label.
   * \param runtime_ctx Runtime context used to resolve runtime resources.
   * \return True if interactive UI was drawn.
   */
  bool custom_inspect (const char *label,
                       comp::singl::runtime_context *runtime_ctx) const;
};

} // namespace rsc

} // namespace wsl
