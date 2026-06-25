# Code Style Guide

This project follows the GCC coding style convention for C++ code.
This document defines the style that should be used for new code.
When this document is more specific than generic GCC guidance, this document
takes precedence.

## General Rules

- Use explicit types by default.
- Use `auto` only when the type is excessively verbose and the declaration
  remains clear at a glance.
- Keep formatting consistent and predictable.
- Match the established style of the file when making localized changes.
- Keep comments and documentation synchronized with the code.

## Documentation

This project uses Doxygen with the Qt comment style.

- Use `/*! ... */` for block documentation.
- Use `//!` for short documentation comments.
- Document public classes, structs, enums, free functions, and member
  functions.
- Document internal helpers when their contract or behavior is not obvious from
  the declaration.
- Include parameters, return values, ownership, side effects, and
  preconditions.

### Doxygen Qt-style example

```cpp
/*!
 * \brief Loads a scene from disk.
 * \param path Path to the scene file.
 * \param out_scene Destination scene object.
 * \return `true` if loading succeeded, otherwise `false`.
 */
bool load_scene (const std::filesystem::path &path, scene &out_scene);

/*!
 * \brief Determines how the viewport camera is selected.
 */
enum class camera_mode {
  //! Use the editor camera.
  editor,

  //! Use the assigned entity camera.
  entity
};
```

## Formatting

- Indent with 2 spaces.
- Do not use tabs for indentation.
- Put opening braces on their own line for functions, classes, structs,
  namespaces, and control flow.
- Use a space before `(` in function declarations, function definitions, and
  control statements.
- Keep pointer and reference qualifiers attached to the type.
- Break long declarations and parameter lists across lines in a readable way.
- Use blank lines to separate logical blocks.

## Naming

- Use `snake_case` for classes, structs, functions, methods, variables,
  namespaces, and enum values.
- Use descriptive names.
- Use short names only for conventional cases such as loop indices and template
  parameters.
- Use the `m_` prefix for private data members.
- Use `enum class` for enums.

## Types and `auto`

- Use explicit types for variables, parameters, return types, pointers,
  references, booleans, numeric values, strings, containers, and entity
  identifiers.
- Use `auto` for iterators, `entt` views, `entt` groups, and similarly verbose
  template-heavy types.
- Use explicit types for the values obtained from those views and groups.

## Functions

- Use descriptive parameter names.
- Use `const T &` for large read-only parameters.
- Use values for small inexpensive types.
- Mark single-argument constructors `explicit` unless implicit conversion is
  intended.
- Keep functions focused on a single task.
- Use early returns to keep control flow straightforward.
- Use output parameters only for additional outputs.

## Classes and Structs

- Use `struct` for passive data containers.
- Use `class` for types with invariants, encapsulation, or non-trivial
  behavior.
- Declare copy and move operations explicitly when ownership or lifetime rules
  apply.
- Keep public interfaces compact and clear.

## Ownership and Const Correctness

- Use references for required non-null parameters.
- Use pointers for optional non-owning relationships.
- Use `std::unique_ptr` for exclusive ownership.
- Use `std::shared_ptr` only when shared ownership is required.
- Mark methods `const` when they do not modify observable state.
- Use `const` on references and pointers to read-only data.

## Includes

- Use `#pragma once` in headers.
- Include the matching header first in each `.cpp` file.
- Include only what is required.
- Use forward declarations in headers when a full definition is not needed.
- Keep headers minimal and self-contained.

## Comments

- Use comments for contracts, assumptions, invariants, ordering requirements,
  and other non-obvious details.
- Keep comments concise and factual.
- Keep comments close to the code they describe.

## Example

```cpp
/*!
 * \brief Tracks assets known to the editor.
 */
class asset_database {
public:
  explicit asset_database (std::filesystem::path root);

  /*!
   * \brief Adds an asset path if it is not already registered.
   * \param path Path relative to the asset root.
   * \return `true` if the asset was added, otherwise `false`.
   */
  bool add_asset (const std::string &path);

  std::size_t get_asset_count () const;

private:
  std::filesystem::path m_root;
  std::vector<std::string> m_assets;
};

void
update_cameras (entt::registry &registry)
{
  auto view = registry.view<comp::transform, comp::camera> ();

  for (entt::entity entity : view) {
    comp::transform &transform = view.get<comp::transform> (entity);
    comp::camera &camera = view.get<comp::camera> (entity);

    if (!camera.enabled) {
      continue;
    }

    update_camera_matrices (entity, transform, camera);
  }
}
```

## RenderDoc Integration

The engine exposes the RenderDoc in-application API (1.7.0) through the
`wsl::gfx::rdoc` namespace in `src/wsl/gfx/renderdoc.hpp`. The integration
is header-only and runtime-loaded: `dlopen("librenderdoc.so", RTLD_NOW |
RTLD_NOLOAD)` (or the platform equivalent) is used to detect the module.
No link-time dependency on `librenderdoc` exists; the build only needs
`${CMAKE_DL_LIBS}` for `dlopen`/`dlsym`.

### Enabling

`WEASEL_ENABLE_RENDERDOC` (CMake option, default ON in Debug, OFF in
Release). When OFF, the `renderdoc_*.cpp` sources are filtered out of the
`wsl` target's source list and the include path is not added. Code that
calls into `wsl::gfx::rdoc` must be wrapped in
`#ifdef WEASEL_ENABLE_RENDERDOC` to keep the OFF build compiling.

### Lifecycle

`wsl::gfx::rdoc::init()` is called from `wsl::app::app` before the
`runtime_context` constructor runs, so capture options take effect
before `SDL_CreateGPUDevice`. `wsl::gfx::rdoc::shutdown()` is called from
`~app`.

When the RenderDoc module is not loaded, every public function in the
namespace is a safe no-op (`is_available()` returns `false`). Programs
launched under `renderdocui` will see `is_available() == true` and have
the full API surface available.

### Environment variables (read at `init` time)

- `RENDERDOC_CAPTURE_FILE_TEMPLATE` - applied via
  `SetCaptureFilePathTemplate` before the GPU device is created.
- `RENDERDOC_OPTION_<Name>=<value>` - applied via
  `SetCaptureOptionU32` / `SetCaptureOptionF32`. Names match the C
  identifiers: `AllowVSync`, `AllowFullscreen`, `APIValidation`,
  `CaptureCallstacks`, `CaptureCallstacksOnlyActions`, `DelayForDebugger`,
  `VerifyBufferWrites`, `HookIntoChildren`, `RefAllResources`,
  `CaptureAllCmdLists`, `DebugOutputMute`,
  `AllowUnsupportedCPUVendor`. Values are parsed as `uint32_t` first,
  then `float`.

### Default capture path

When `RENDERDOC_CAPTURE_FILE_TEMPLATE` is unset, the engine sets the
template to `<user-documents>/weasel_captures/frame_<capture>`. The
documents directory comes from `SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)`
which maps to `$XDG_DOCUMENTS_DIR` (or `~/Documents`) on Linux,
`{FOLDERID_Documents}` on Windows, and `~/Documents` on macOS. The
directory is created if it does not exist.

### Annotations

Frame-scope annotations are emitted automatically by the renderer. The
following tree appears in RenderDoc's Annotation Viewer when the module
is loaded:

```
frame.index          (uint64_t)
frame.ticks_ms       (uint64_t)
pass.3d              ("main")
pass.postprocess     ("bloom_tonemap")
  pass.postprocess.bloom  ("downsample" | "blur_h" | "blur_v")
  pass.postprocess.tonemap ("swapchain" | "present_tex")
pass.ui              ("ui")
```

Game / system code can add its own annotations with
`wsl::gfx::rdoc::annotate_command` (templated; supports `bool`, integer,
floating point, and `std::string_view`) or the
`command_annotation_scope<T>` RAII helper. The `queueOrCommandBuffer`
parameter is the SDL3 `SDL_GPUCommandBuffer*` (passed verbatim through
to the API).

### Timing / profiling

The in-app API does not surface GPU timestamps directly. For per-pass
GPU timings, capture a frame programmatically and inspect it in the
Replay UI. The `enable_profiling_overlay(bool)` helper toggles the
in-window frame-rate / frame-number / capture-list overlay
(`eRENDERDOC_Overlay_FrameRate | FrameNumber | CaptureList`).

### Programmatic capture

```cpp
// At any point in the main loop:
wsl::gfx::rdoc::trigger_capture ();
// or for a multi-frame burst:
wsl::gfx::rdoc::trigger_multi_frame_capture (3);
// or manual start/stop:
wsl::gfx::rdoc::start_capture ();
// ... render one or more frames ...
wsl::gfx::rdoc::end_capture ();
wsl::gfx::rdoc::stamp_post_capture_comments ("main_scene", frame_idx, x, y, z);
```

### Editor UI

The `Debug` menu in the editor's main menu bar contains a `RenderDoc`
submenu with `Capture Next Frame`, `Capture Next 3 Frames`, `Launch /
Show Replay UI`, and a `Profiling Overlay` toggle. All entries are
disabled when `is_available()` returns false, and the submenu shows a
"RenderDoc module not loaded" hint. The submenu is only compiled into
the editor when `WEASEL_ENABLE_RENDERDOC` is on.

### Tracy telemetry

Tracy v0.13.1 is integrated via the standard client API (no LTO /
GlibC mismatch — Tracy is loaded as a shared library, the engine
links `TracyClient`). The integration is split across three
small modules: `sys/tracy_telemetry`, `log/tracy_sink`, and
`gfx/tracy_gpu_mem`.

**Frame markers** — the main loop calls `tracy_telemetry_frame_mark`
once per iteration, which pushes `FrameMarkNamed("Frame N")`. Tracy
reads the per-frame time from the gap between consecutive frame
marks; the label just makes the Frame view self-describing. Two
**secondary frame sets** wrap the work breakdown of each loop:
`FrameMarkStart("Update")` / `FrameMarkEnd("Update")` covers ECS
and physics, and `FrameMarkStart("Render")` / `FrameMarkEnd("Render")`
covers the GPU submission. They appear as two extra rows in the
Frame view so the engine shows "Update took X, Render took Y,
frame took Z" at a glance.

**Memory tab** — engine GPU resources are reported to named memory
pools via `TracyAllocN` / `TracyFreeN` (the proper Tracy memory API,
not simple plots). The pools are:
- `wsl.gfx.textures` — every `SDL_GPUTexture` (HDR scene / bloom /
  depth / present tex). The size is computed from the
  `SDL_GPUTextureCreateInfo` (format byte width × mip chain ×
  sample count).
- `wsl.gfx.buffers`, `wsl.gfx.transfer`, `wsl.gfx.imgui`,
  `wsl.gfx.cluster` — reserved for `SDL_GPUBuffer`,
  `SDL_GPUTransferBuffer`, the ImGui vertex/index uploads, and the
  cluster light buffer, respectively. The texture pool is wired in
  `render_window.cpp`; the others are declared as named constants
  in `tracy_gpu_mem.hpp` and are ready to be hooked into their
  respective creation sites.

**Trace description** — `TracyAppInfo("Weasel Engine", 14)` is
pushed once from the background telemetry thread. It shows up in
the trace's Information panel.

**Runtime parameters** — `TracyParameterSetup` registers three
runtime-tunable values: `frame` (int), `is_running` (int), and
`in_play_session` (int). The background thread refreshes them
every 250 ms, so the connection popup's parameter panel always
reflects the live engine state.

**Playback plots** — the only plots left in the engine, since the
Memory tab supersedes the previous RSS/virt plots. `playback.fps`
is an EWMA of the per-frame instantaneous FPS, and `playback.running`
is a 0/1 step plot driven by `runtime_context::is_running`.

**Frame images** — the engine's `present_tex` (the tonemapped,
bloom-augmented LDR output) is captured to a 320x180 RGBA thumbnail
and forwarded to Tracy's Frame view via `FrameImage`. The flow:

1. `render_window::frame_image_issue_copy()` records a copy pass on
   the **main** command buffer from `present_tex` into a single
   transfer buffer allocated at startup
   (`SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD`). The copy is the **last**
   command in the frame so the captured pixels include the UI pass
   on top of the postprocess pass.
2. `render_context::end_cmd()` submits and acquires a fence for the
   current frame; `render_context::current_fence()` exposes it.
3. `render_window::frame_image_submit(fence)` waits on the fence,
   maps the transfer buffer, box-filter downsamples BGRA → RGBA
   on the CPU (no compute pipeline needed), and calls `FrameImage`
   with `offset=0, flip=false`. The downscaled vector is alive
   until after the call returns; Tracy copies internally so the
   transfer buffer is reusable next frame.

The per-frame image path adds one fence-wait and a small amount of
CPU downscaling work (320×180 with box filter, no per-pixel SIMD).
Tracy's client compresses the image on a background thread, so the
main thread cost is bounded by the fence wait. The capture is
**always issued**; if no profiler is attached, `FrameImage` resolves
to a no-op and the GPU copy / CPU downsample still run, which is a
small constant overhead per frame. Set `m_fi_dst_w = 0` in
`frame_image_init` to disable the entire pipeline (skip the copy
pass, the downsample, and the call).

**spdlog → Tracy messages** — every spdlog logger (core, gfx, rsc,
sys, editor, cli, phys, net, cmake) attaches a `wsl::log::tracy_sink`
alongside the stdout colour sink. Each log entry is forwarded as
`TracyMessageLC` with a colour mapped from the spdlog level: blue-gray
for trace, gray for debug, green for info, orange for warn, red for
error, dark red for critical. The Messages column in Tracy shows
`[LEVEL][LOGGER] payload` so a `gfx::error` line becomes
`[ERROR][gfx] vkCreateBuffer failed: ...`.

> The vendored Tracy is v0.13.1, which does **not** ship the newer
> `TracyLogString` (with a proper `MessageSeverity` enum) or the
> `TracyParamType*` enums. The `tracy_sink::sink_it_` and the
> `tracy_telemetry::publish_parameters` functions each carry a
> comment noting the exact call to swap in once the engine upgrades
> to a newer Tracy.

The telemetry thread is started in `app::app()` (right after the
`runtime_context` is created) and joined in `~app()` before the
context is destroyed, so the snapshot function never dereferences
a freed `runtime_context`.
