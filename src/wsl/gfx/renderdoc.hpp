#pragma once

#include "renderdoc_app.h"

#include <cstdint>
#include <string>
#include <string_view>

/**
 * @file renderdoc.hpp
 * @brief C++ wrapper around the RenderDoc in-application API 1.7.0.
 *
 * The engine never links against librenderdoc. The RenderDoc module is
 * resolved at runtime in @ref init() via dlopen / GetModuleHandle; if it
 * is not present (i.e. the program was not launched under renderdocui),
 * every public function in this namespace is a safe no-op.
 *
 * The API is consumed programmatically only. Call sites:
 *
 * @code
 *   wsl::gfx::rdoc::init ();                   // once, before GPU device
 * creation
 *   ...
 *   wsl::gfx::rdoc::trigger_capture ();        // ask RenderDoc to grab next
 * frame wsl::gfx::rdoc::annotate_command ( ctx->main_cmd, "pass.3d",
 * std::string_view ("main"));
 * @endcode
 *
 * The device pointer passed to capture and annotation functions is always
 * @c nullptr (wildcard). RenderDoc's docs explicitly allow this for the
 * single-device single-window case, which matches the engine's reality
 * and avoids reaching into SDL3 GPU's hidden Vulkan handles.
 */
namespace wsl::gfx::rdoc
{

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

/*!
 * \brief Try to load the RenderDoc module and resolve the API.
 *
 * Safe to call multiple times; the second call is a no-op. Must be
 * invoked *before* the engine creates the SDL_GPUDevice for capture
 * options (vsync, callstacks, ...) to take effect.
 *
 * The default capture file path is set to
 * `<user-documents>/weasel_captures/frame_<frame>`.
 *
 * Recognised environment variables (applied after API resolution):
 *   - RENDERDOC_CAPTURE_FILE_TEMPLATE
 *   - RENDERDOC_OPTION_<NAME>=<u32|f32>  (e.g. RENDERDOC_OPTION_AllowVSync=0)
 */
void init ();

/*! \brief Drop the API pointer. Module handle is also released. */
void shutdown ();

// ---------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------

/*! \brief True if the RenderDoc module was loaded and the API resolved. */
[[nodiscard]] bool is_available ();

/*! \brief True if a frame capture is currently in progress. */
[[nodiscard]] bool is_capturing ();

/*! \brief True if the replay UI is currently target-control connected. */
[[nodiscard]] bool is_ui_connected ();

/*! \brief Major / minor / patch of the resolved API implementation. */
void api_version (int &major, int &minor, int &patch);

// ---------------------------------------------------------------------
// Implementation helpers (do not call directly)
// Defined in renderdoc.cpp. Forward-declared here so the templated
// annotation overloads below can see them.
// ---------------------------------------------------------------------

/*! \cond INTERNAL */
void set_object_annotation_impl (void *api_object, const char *key,
                                 const RDAnnotationHelper *value);
void set_command_annotation_impl (void *queue_or_command_buffer,
                                  const char *key,
                                  const RDAnnotationHelper *value);
/*! \endcond */

// ---------------------------------------------------------------------
// Capture control
// ---------------------------------------------------------------------

/*! \brief Begin a frame capture on the active device. */
void start_capture ();

/*! \brief End an in-progress capture. Returns true on success. */
bool end_capture ();

/*! \brief Discard an in-progress capture (cheaper than end_capture). */
bool discard_capture ();

/*! \brief Ask RenderDoc to grab the next presented frame (UI-style). */
void trigger_capture ();

/*! \brief Ask RenderDoc to grab the next @p num_frames presented frames. */
void trigger_multi_frame_capture (uint32_t num_frames);

// ---------------------------------------------------------------------
// Replay UI
// ---------------------------------------------------------------------

/*!
 * \brief Launch the closest matching replay UI.
 * \param connect_target_control If true, the UI attaches to this process.
 * \param cmdline Optional extra command-line args (e.g. a .rdc path).
 * \return PID of the launched UI, or 0 on failure.
 */
uint32_t launch_replay_ui (bool connect_target_control,
                           const char *cmdline = nullptr);

/*! \brief Ask a connected replay UI to raise itself. */
bool show_replay_ui ();

// ---------------------------------------------------------------------
// Capture file metadata
// ---------------------------------------------------------------------

/*! \brief Set the path template for new captures. See
 * SetCaptureFilePathTemplate. */
void set_capture_file_path_template (std::string_view pathtemplate);

/*! \brief Get the current path template. */
[[nodiscard]] std::string get_capture_file_path_template ();

/*! \brief Tag the in-progress capture with a human-readable title. */
void set_capture_title (std::string_view title);

/*!
 * \brief Tag an existing capture file with a free-form comment block.
 *
 * \param file_path Absolute or template-relative path. If empty, the most
 *        recent capture file is used.
 * \param comments Free-form text shown in the UI.
 */
void set_capture_file_comments (std::string_view file_path,
                                std::string_view comments);

/*!
 * \brief Convenience: stamp the most recent capture with engine state.
 *
 * Intended to be called from user code after @ref end_capture() returns.
 * Includes the scene name, frame index, and camera position so the
 * capture is self-describing in the UI.
 */
void stamp_post_capture_comments (std::string_view scene_name,
                                  uint64_t frame_index, float camera_x,
                                  float camera_y, float camera_z);

// ---------------------------------------------------------------------
// Capture options
// ---------------------------------------------------------------------

bool set_option_u32 (RENDERDOC_CaptureOption opt, uint32_t val);
bool set_option_f32 (RENDERDOC_CaptureOption opt, float val);
[[nodiscard]] uint32_t get_option_u32 (RENDERDOC_CaptureOption opt);
[[nodiscard]] float get_option_f32 (RENDERDOC_CaptureOption opt);

// ---------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------

/*! \brief Replace the entire overlay bitmask. */
void set_overlay_bits (uint32_t mask);

/*! \brief Read-modify-write the overlay bitmask. */
void mask_overlay_bits (uint32_t and_mask, uint32_t or_mask);

/*! \brief Current overlay bitmask. */
[[nodiscard]] uint32_t get_overlay_bits ();

/*!
 * \brief Convenience: enable the standard profiling overlay
 *        (frame rate, frame number, capture list).
 */
void enable_profiling_overlay (bool on);

// ---------------------------------------------------------------------
// Hotkeys
// ---------------------------------------------------------------------

void set_capture_keys (const RENDERDOC_InputButton *keys, int num);
void set_focus_toggle_keys (const RENDERDOC_InputButton *keys, int num);

// ---------------------------------------------------------------------
// Annotations (added in API 1.7.0)
// ---------------------------------------------------------------------

//! Maximum supported vector width per the spec.
inline constexpr uint32_t kMaxAnnotationVectorWidth = 4;

/*!
 * \brief Tag a GPU object (texture, buffer, ...) with a key/value pair.
 *
 * Visible in RenderDoc's Resource Inspector as a custom annotation.
 * @p api_object must be a pointer recognised by the captured API. For
 * SDL3 GPU on Vulkan the wrapped VkBuffer / VkImage is not reachable
 * from user code, so object annotations are best-effort. The command
 * annotations (see below) are more reliable through the SDL3 GPU
 * abstraction.
 *
 * The @p value is one of: bool, int32, uint32, int64, uint64, float,
 * double, std::string_view, glm::vec{2,3,4} of float/double/int/uint,
 * or RENDERDOC_ResourceId.
 */
template <typename T>
void
annotate_object (void *api_object, const char *key, const T &value)
{
  RDAnnotationHelper helper (value);
  set_object_annotation_impl (api_object, key, &helper);
}

/*! \brief Convenience overload for null-terminated C strings. */
inline void
annotate_object (void *api_object, const char *key, const char *value)
{
  RDAnnotationHelper helper (value);
  set_object_annotation_impl (api_object, key, &helper);
}

/*! \brief Convenience overload for std::string_view. */
inline void
annotate_object (void *api_object, const char *key, std::string_view value)
{
  // RDAnnotationHelper's const char* ctor stores the pointer; we need a
  // null-terminated copy for the lifetime of the call.
  std::string tmp (value);
  RDAnnotationHelper helper (tmp.c_str ());
  set_object_annotation_impl (api_object, key, &helper);
}

/*! \brief Delete a previously set object annotation (and all its children). */
void delete_object_annotation (void *api_object, const char *key);

/*!
 * \brief Tag the active command buffer / queue with a key/value pair.
 *
 * Visible in RenderDoc's Annotation Viewer as a tree under
 * `command-stream`. Use this from within a render pass to attach
 * structured metadata describing the work being recorded.
 *
 * @p queue_or_command_buffer is passed straight through to
 * SetCommandAnnotation. For SDL3 GPU on Vulkan this is the
 * SDL_GPUCommandBuffer pointer; RenderDoc's layer accepts the value
 * but may not always match it to a captured VkCommandBuffer due to
 * SDL3's handle wrapping. Pass nullptr to wildcard.
 */
template <typename T>
void
annotate_command (void *queue_or_command_buffer, const char *key,
                  const T &value)
{
  RDAnnotationHelper helper (value);
  set_command_annotation_impl (queue_or_command_buffer, key, &helper);
}

/*! \brief Convenience overload for null-terminated C strings. */
inline void
annotate_command (void *queue_or_command_buffer, const char *key,
                  const char *value)
{
  RDAnnotationHelper helper (value);
  set_command_annotation_impl (queue_or_command_buffer, key, &helper);
}

/*! \brief Convenience overload for std::string_view. */
inline void
annotate_command (void *queue_or_command_buffer, const char *key,
                  std::string_view value)
{
  std::string tmp (value);
  RDAnnotationHelper helper (tmp.c_str ());
  set_command_annotation_impl (queue_or_command_buffer, key, &helper);
}

/*! \brief Delete a previously set command annotation. */
void delete_command_annotation (void *queue_or_command_buffer, const char *key);

// ---------------------------------------------------------------------
// RAII scope
// ---------------------------------------------------------------------

/*!
 * \brief Scoped command-stream annotation.
 *
 * Pushes a key with the given value on construction and removes it on
 * destruction, giving you a stack-friendly "with this annotation" block.
 *
 * @code
 *   {
 *     wsl::gfx::rdoc::command_annotation_scope scope (
 *         ctx->main_cmd, "pass.postprocess.bloom",
 *         std::string_view ("downsample"));
 *     // ... draw calls ...
 *   }
 * @endcode
 */
template <typename T> class command_annotation_scope
{
public:
  command_annotation_scope (void *cmd, const char *key, const T &value)
      : m_cmd (cmd), m_key (key)
  {
    annotate_command<T> (cmd, key, value);
  }
  ~command_annotation_scope () { delete_command_annotation (m_cmd, m_key); }

  command_annotation_scope (const command_annotation_scope &) = delete;
  command_annotation_scope &operator= (const command_annotation_scope &)
      = delete;

private:
  void *m_cmd;
  const char *m_key;
};

} // namespace wsl::gfx::rdoc
