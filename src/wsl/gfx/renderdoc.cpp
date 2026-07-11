#include "renderdoc.hpp"

#include "wsl/log/log.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace wsl::gfx::rdoc
{

namespace
{

// Resolved API pointer. Set in init(), cleared in shutdown(). All public
// functions check this for null before dereferencing.
const RENDERDOC_API_1_7_0 *s_api = nullptr;

// Opaque module handle returned by the platform loader. Held until
// shutdown so the API pointer stays valid.
void *s_module = nullptr;

// True once init() has completed (successfully or not).
bool s_initialised = false;

const char *
capture_option_name (RENDERDOC_CaptureOption opt)
{
  switch (opt) {
  case eRENDERDOC_Option_AllowVSync:
    return "AllowVSync";
  case eRENDERDOC_Option_AllowFullscreen:
    return "AllowFullscreen";
  case eRENDERDOC_Option_APIValidation:
    return "APIValidation";
  case eRENDERDOC_Option_CaptureCallstacks:
    return "CaptureCallstacks";
  case eRENDERDOC_Option_CaptureCallstacksOnlyActions:
    return "CaptureCallstacksOnlyActions";
  case eRENDERDOC_Option_DelayForDebugger:
    return "DelayForDebugger";
  case eRENDERDOC_Option_VerifyBufferWrites:
    return "VerifyBufferWrites";
  case eRENDERDOC_Option_HookIntoChildren:
    return "HookIntoChildren";
  case eRENDERDOC_Option_RefAllResources:
    return "RefAllResources";
  case eRENDERDOC_Option_CaptureAllCmdLists:
    return "CaptureAllCmdLists";
  case eRENDERDOC_Option_DebugOutputMute:
    return "DebugOutputMute";
  case eRENDERDOC_Option_AllowUnsupportedCPUVendor:
    return "AllowUnsupportedCPUVendor";
  }
  return "??";
}

void
apply_env_options (const RENDERDOC_API_1_7_0 *api)
{
  // 1. RENDERDOC_CAPTURE_FILE_TEMPLATE
  if (const char *tpl = std::getenv ("RENDERDOC_CAPTURE_FILE_TEMPLATE")) {
    api->SetCaptureFilePathTemplate (tpl);
    wsl::log::gfx ()->info ("RenderDoc: capture file template = '{}'", tpl);
  }

  // 2. RENDERDOC_OPTION_<NAME>=<value>
  char **env = SDL_GetEnvironmentVariables (nullptr);
  if (env == nullptr) {
    return;
  }
  static const char kPrefix[] = "RENDERDOC_OPTION_";
  size_t const kPrefixLen = sizeof (kPrefix) - 1;
  for (char **e = env; *e != nullptr; ++e) {
    const char *kv = *e;
    if (std::strncmp (kv, kPrefix, kPrefixLen) != 0) {
      continue;
    }
    const char *name = kv + kPrefixLen;
    const char *eq = std::strchr (name, '=');
    if (eq == nullptr) {
      continue;
    }
    std::string option_name (name, static_cast<size_t> (eq - name));
    std::string raw (eq + 1);
    if (raw.empty ()) {
      continue;
    }

    // Match the option by name. u32 first (covers the integer options),
    // then f32 (covers the float options). A misspelled name is a
    // silent no-op rather than a crash.
    for (uint32_t opt = 0; opt <= eRENDERDOC_Option_DebugOutputMute; ++opt) {
      auto eopt = static_cast<RENDERDOC_CaptureOption> (opt);
      if (capture_option_name (eopt) != option_name) {
        continue;
      }
      char *end = nullptr;
      uint32_t const u = std::strtoul (raw.c_str (), &end, 10);
      if (end != raw.c_str () && *end == '\0') {
        if (api->SetCaptureOptionU32 (eopt, u)) {
          wsl::log::gfx ()->info ("RenderDoc: option {} = {}", option_name, u);
        } else {
          wsl::log::gfx ()->warn (
              "RenderDoc: SetCaptureOptionU32 rejected {} = {}", option_name,
              u);
        }
      } else {
        char *end = nullptr;
        float const f = std::strtof (raw.c_str (), &end);
        if (end != raw.c_str () && *end == '\0') {
          if (api->SetCaptureOptionF32 (eopt, f)) {
            wsl::log::gfx ()->info ("RenderDoc: option {} = {}", option_name,
                                    f);
          } else {
            wsl::log::gfx ()->warn (
                "RenderDoc: SetCaptureOptionF32 rejected {} = {}", option_name,
                f);
          }
        } else {
          wsl::log::gfx ()->warn (
              "RenderDoc: could not parse {}='{}' as u32 or f32", option_name,
              raw);
        }
      }
      break;
    }
  }
  for (char **e = env; *e != nullptr; ++e) {
    SDL_free (*e);
  }
  SDL_free (env);
}

std::filesystem::path
default_capture_directory ()
{
  // SDL_GetUserFolder returns the user's documents directory. On Linux
  // this is $XDG_DOCUMENTS_DIR or $HOME/Documents; on Windows it is the
  // CSIDL_PROFILE + Documents shell folder; on macOS it is ~/Documents.
  const char *docs = SDL_GetUserFolder (SDL_FOLDER_DOCUMENTS);
  std::filesystem::path base = (docs != nullptr)
                                   ? std::filesystem::path (docs)
                                   : std::filesystem::current_path ();
  return base / "weasel_captures";
}

} // namespace

// ---------------------------------------------------------------------
// Platform loaders (resolved out-of-line; one TU per OS)
// ---------------------------------------------------------------------
namespace detail
{
void *try_load_module (void *&out_module);
void release_module (void *module);
} // namespace detail

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

void
init ()
{
  if (s_initialised) {
    return;
  }
  s_initialised = true;

  if (s_module != nullptr) {
    // Already loaded by a previous init() — shouldn't happen, but be
    // defensive.
    return;
  }

  void *module = nullptr;
  pRENDERDOC_GetAPI get_api
      = reinterpret_cast<pRENDERDOC_GetAPI> (detail::try_load_module (module));
  if (get_api == nullptr) {
    wsl::log::gfx ()->trace (
        "RenderDoc: module not loaded, in-application API disabled");
    return;
  }

  // Ask for the highest version we know about; the loader may return a
  // newer minor/patch that is backwards compatible.
  RENDERDOC_API_1_7_0 *api = nullptr;
  int const rc = get_api (eRENDERDOC_API_Version_1_7_0,
                          reinterpret_cast<void **> (&api));
  if (rc != 1 || api == nullptr) {
    wsl::log::gfx ()->warn (
        "RenderDoc: RENDERDOC_GetAPI returned {}, in-application API disabled",
        rc);
    detail::release_module (module);
    return;
  }

  s_api = api;
  s_module = module;

  int major = 0;
  int minor = 0;
  int patch = 0;
  api->GetAPIVersion (&major, &minor, &patch);
  wsl::log::gfx ()->info (
      "RenderDoc: API {}.{}.{} loaded, capture support active", major, minor,
      patch);

  // Apply RENDERDOC_CAPTURE_FILE_TEMPLATE and RENDERDOC_OPTION_* from
  // the environment before any GPU device is created.
  apply_env_options (api);

  // Set a sensible default capture path. The user's
  // RENDERDOC_CAPTURE_FILE_TEMPLATE (if any) is already applied above; this
  // only runs when the env var was absent so it is not a silent override.
  if (std::getenv ("RENDERDOC_CAPTURE_FILE_TEMPLATE") == nullptr) {
    std::filesystem::path const dir = default_capture_directory ();
    std::error_code ec;
    std::filesystem::create_directories (dir, ec);
    std::string tpl = (dir / "frame_<capture>").string ();
    api->SetCaptureFilePathTemplate (tpl.c_str ());
    wsl::log::gfx ()->info ("RenderDoc: default capture path template = '{}'",
                            tpl);
  }
}

void
shutdown ()
{
  s_api = nullptr;
  if (s_module != nullptr) {
    detail::release_module (s_module);
    s_module = nullptr;
  }
  s_initialised = false;
}

// ---------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------

bool
is_available ()
{
  return s_api != nullptr;
}

bool
is_capturing ()
{
  return s_api != nullptr && s_api->IsFrameCapturing () != 0;
}

bool
is_ui_connected ()
{
  return s_api != nullptr && s_api->IsTargetControlConnected () != 0;
}

void
api_version (int &major, int &minor, int &patch)
{
  if (s_api != nullptr) {
    s_api->GetAPIVersion (&major, &minor, &patch);
  } else {
    major = minor = patch = 0;
  }
}

// ---------------------------------------------------------------------
// Capture control
// ---------------------------------------------------------------------

void
start_capture ()
{
  if (s_api != nullptr) {
    s_api->StartFrameCapture (nullptr, nullptr);
  }
}

bool
end_capture ()
{
  return s_api != nullptr && s_api->EndFrameCapture (nullptr, nullptr) != 0;
}

bool
discard_capture ()
{
  return s_api != nullptr && s_api->DiscardFrameCapture (nullptr, nullptr) != 0;
}

void
trigger_capture ()
{
  if (s_api != nullptr) {
    s_api->TriggerCapture ();
  }
}

void
trigger_multi_frame_capture (uint32_t num_frames)
{
  if (s_api != nullptr) {
    s_api->TriggerMultiFrameCapture (num_frames);
  }
}

// ---------------------------------------------------------------------
// Replay UI
// ---------------------------------------------------------------------

uint32_t
launch_replay_ui (bool connect_target_control, const char *cmdline)
{
  if (s_api == nullptr) {
    return 0;
  }
  return s_api->LaunchReplayUI (connect_target_control ? 1U : 0U, cmdline);
}

bool
show_replay_ui ()
{
  return s_api != nullptr && s_api->ShowReplayUI () != 0;
}

// ---------------------------------------------------------------------
// Capture file metadata
// ---------------------------------------------------------------------

void
set_capture_file_path_template (std::string_view pathtemplate)
{
  if (s_api == nullptr) {
    return;
  }
  // RenderDoc stores a pointer to the string, so keep a copy alive.
  static thread_local std::string s_buf;
  s_buf.assign (pathtemplate.data (), pathtemplate.size ());
  s_api->SetCaptureFilePathTemplate (s_buf.c_str ());
}

std::string
get_capture_file_path_template ()
{
  if (s_api == nullptr) {
    return {};
  }
  return s_api->GetCaptureFilePathTemplate ();
}

void
set_capture_title (std::string_view title)
{
  if (s_api == nullptr) {
    return;
  }
  static thread_local std::string s_buf;
  s_buf.assign (title.data (), title.size ());
  s_api->SetCaptureTitle (s_buf.c_str ());
}

void
set_capture_file_comments (std::string_view file_path,
                           std::string_view comments)
{
  if (s_api == nullptr) {
    return;
  }
  static thread_local std::string s_path;
  static thread_local std::string s_comments;
  s_path.assign (file_path.data (), file_path.size ());
  s_comments.assign (comments.data (), comments.size ());
  s_api->SetCaptureFileComments (s_path.empty () ? nullptr : s_path.c_str (),
                                 s_comments.c_str ());
}

void
stamp_post_capture_comments (std::string_view scene_name, uint64_t frame_index,
                             float camera_x, float camera_y, float camera_z)
{
  if (s_api == nullptr) {
    return;
  }
  static thread_local std::string s_comments;
  s_comments.clear ();
  s_comments.reserve (128);
  s_comments.append ("scene: ");
  s_comments.append (scene_name.data (), scene_name.size ());
  s_comments.append ("\nframe: ");
  s_comments.append (std::to_string (frame_index));
  s_comments.append ("\ncamera: (");
  s_comments.append (std::to_string (camera_x));
  s_comments.append (", ");
  s_comments.append (std::to_string (camera_y));
  s_comments.append (", ");
  s_comments.append (std::to_string (camera_z));
  s_comments.append (")");
  s_api->SetCaptureFileComments (nullptr, s_comments.c_str ());
}

// ---------------------------------------------------------------------
// Capture options
// ---------------------------------------------------------------------

bool
set_option_u32 (RENDERDOC_CaptureOption opt, uint32_t val)
{
  return s_api != nullptr && s_api->SetCaptureOptionU32 (opt, val) != 0;
}

bool
set_option_f32 (RENDERDOC_CaptureOption opt, float val)
{
  return s_api != nullptr && s_api->SetCaptureOptionF32 (opt, val) != 0;
}

uint32_t
get_option_u32 (RENDERDOC_CaptureOption opt)
{
  return s_api != nullptr ? s_api->GetCaptureOptionU32 (opt) : 0xFFFFFFFFU;
}

float
get_option_f32 (RENDERDOC_CaptureOption opt)
{
  return s_api != nullptr ? s_api->GetCaptureOptionF32 (opt) : -FLT_MAX;
}

// ---------------------------------------------------------------------
// Overlay
// ---------------------------------------------------------------------

void
set_overlay_bits (uint32_t mask)
{
  if (s_api == nullptr) {
    return;
  }
  s_api->MaskOverlayBits (0, mask);
}

void
mask_overlay_bits (uint32_t and_mask, uint32_t or_mask)
{
  if (s_api == nullptr) {
    return;
  }
  s_api->MaskOverlayBits (and_mask, or_mask);
}

uint32_t
get_overlay_bits ()
{
  return s_api != nullptr ? s_api->GetOverlayBits () : 0;
}

void
enable_profiling_overlay (bool on)
{
  if (s_api == nullptr) {
    return;
  }
  uint32_t const kProf
      = eRENDERDOC_Overlay_FrameRate | eRENDERDOC_Overlay_FrameNumber
        | eRENDERDOC_Overlay_CaptureList | eRENDERDOC_Overlay_Enabled;
  if (on) {
    s_api->MaskOverlayBits (~0U, kProf);
  } else {
    s_api->MaskOverlayBits (~kProf, 0);
  }
}

// ---------------------------------------------------------------------
// Hotkeys
// ---------------------------------------------------------------------

void
set_capture_keys (const RENDERDOC_InputButton *keys, int num)
{
  if (s_api == nullptr) {
    return;
  }
  s_api->SetCaptureKeys (const_cast<RENDERDOC_InputButton *> (keys), num);
}

void
set_focus_toggle_keys (const RENDERDOC_InputButton *keys, int num)
{
  if (s_api == nullptr) {
    return;
  }
  s_api->SetFocusToggleKeys (const_cast<RENDERDOC_InputButton *> (keys), num);
}

// ---------------------------------------------------------------------
// Annotations (1.7.0)
// ---------------------------------------------------------------------

void
set_object_annotation_impl (void *api_object, const char *key,
                            const RDAnnotationHelper *value)
{
  if (s_api == nullptr || api_object == nullptr || key == nullptr) {
    return;
  }
  s_api->SetObjectAnnotation (nullptr, api_object, key, value->type, 0,
                              &value->value);
}

void
set_command_annotation_impl (void *queue_or_command_buffer, const char *key,
                             const RDAnnotationHelper *value)
{
  if (s_api == nullptr || key == nullptr) {
    return;
  }
  s_api->SetCommandAnnotation (nullptr, queue_or_command_buffer, key,
                               value->type, 0, &value->value);
}

void
delete_object_annotation (void *api_object, const char *key)
{
  if (s_api == nullptr || api_object == nullptr || key == nullptr) {
    return;
  }
  RENDERDOC_AnnotationValue v{};
  s_api->SetObjectAnnotation (nullptr, api_object, key, eRENDERDOC_Empty, 0,
                              &v);
}

void
delete_command_annotation (void *queue_or_command_buffer, const char *key)
{
  if (s_api == nullptr || key == nullptr) {
    return;
  }
  RENDERDOC_AnnotationValue v{};
  s_api->SetCommandAnnotation (nullptr, queue_or_command_buffer, key,
                               eRENDERDOC_Empty, 0, &v);
}

} // namespace wsl::gfx::rdoc
