#include "shader_compiler.hpp"
#include "wsl/log/log.hpp"

#include <SDL3/SDL.h>

#include <nlohmann/json.hpp>

#include <cstring>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#define WEASEL_PATH_SEP '\\'
#else
#include <unistd.h>
#include <sys/wait.h>
#define WEASEL_PATH_SEP '/'
#endif

namespace wsl
{

namespace gfx
{

shader_compiler::shader_compiler ()
{
  // Runtime compilation is performed out-of-process via the `slangc`
  // executable (see compile_stage). This keeps the editor resilient to
  // crashes/heap corruption inside the linked Slang library: any failure is
  // contained in the slangc process and reported as a compile error here.
  if (!init ()) {
    wsl::log::gfx ()->error ("shader_compiler: failed to initialize");
  }
}

shader_compiler::~shader_compiler () {}

bool
shader_compiler::init ()
{
  // No in-process Slang session is required; compilation happens in slangc.
  return true;
}

SDL_GPUShaderFormat
shader_compiler::native_format ()
{
#if defined(__APPLE__)
  return SDL_GPU_SHADERFORMAT_MSL;
#elif defined(_WIN32)
  return SDL_GPU_SHADERFORMAT_DXIL;
#else
  return SDL_GPU_SHADERFORMAT_SPIRV;
#endif
}

namespace
{

// Locate the `slangc` executable used for out-of-process compilation.
// Search order: WEASEL_SLANGC env var, alongside the editor executable,
// then the system PATH.
std::string
find_slangc ()
{
  const char *env = std::getenv ("WEASEL_SLANGC");
  if (env && *env)
    return env;

#ifdef WEASEL_SLANGC_PATH
  if (std::filesystem::exists (WEASEL_SLANGC_PATH))
    return WEASEL_SLANGC_PATH;
#endif

  if (const char *base = SDL_GetBasePath ()) {
    std::string path = std::string (base) + "slangc";
    SDL_free (const_cast<char *> (base));
    if (std::filesystem::exists (path))
      return path;
  }

  // Fall back to PATH lookup (popen resolves it).
  return "slangc";
}

// Run a command and capture its stderr into `diagnostics`. Returns the
// process exit code.
int
run_capture (const std::string &command, std::string &diagnostics)
{
  std::string full = command + " 2>&1";
  FILE *pipe = popen (full.c_str (), "r");
  if (!pipe) {
    diagnostics = "failed to launch slangc";
    return -1;
  }
  char buf[4096];
  while (fgets (buf, sizeof (buf), pipe)) {
    diagnostics += buf;
  }
  int status = pclose (pipe);
  if (status == -1)
    return -1;
#if defined(_WIN32)
  return status;
#else
  if (WIFEXITED (status))
    return WEXITSTATUS (status);
  return -1;
#endif
}

// Mirror the flags used by the build-time shader compilation
// (CMake `compile_shaders` target) so runtime shaders behave identically,
// including `-emit-spirv-via-glsl` for Vulkan 1.0 driver compatibility and
// the explicit `-fvk-*-shift` remapping.
std::string
slangc_flags (SDL_GPUShaderStage stage)
{
  const char *entry
      = (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? "vsMain" : "fsMain";
  std::string f = " -target spirv -entry ";
  f += entry;
  // `-fvk-use-entrypoint-name` is required so the generated SPIR-V keeps the
  // `vsMain`/`fsMain` entry point names. Without it, `-emit-spirv-via-glsl`
  // renames the entry to `main`, and SDL_GPU (which requests `vsMain`/`fsMain`)
  // fails to find the entry point at pipeline creation, crashing Vulkan.
  f += " -emit-spirv-via-glsl -fvk-use-entrypoint-name";
  f += " -fvk-b-shift 0 3 -fvk-s-shift 0 2 -fvk-t-shift 0 2 -fvk-u-shift 0 2";
  return f;
}

} // namespace

// Parse slangc's `-reflection-json` output into the engine's reflection
// structures. Mirrors the layout the previous in-process Slang API produced
// (uniform buffers with members, textures; samplers are implicit).
static void
parse_reflection (const std::filesystem::path &json_path,
                  SDL_GPUShaderStage stage, shader_program &out)
{
  shader_reflection *target = (stage == SDL_GPU_SHADERSTAGE_VERTEX)
                                  ? &out.vertex_reflection
                                  : &out.fragment_reflection;

  std::ifstream is (json_path);
  if (!is) {
    wsl::log::gfx ()->debug ("[shader_compiler] reflection: no JSON file at {}",
                             json_path.string ());
    return;
  }
  nlohmann::json j;
  try {
    is >> j;
  } catch (...) {
    return;
  }

  try {
    auto params = j.find ("parameters");
    if (params == j.end () || !params->is_array ()) {
      wsl::log::gfx ()->debug ("[shader_compiler] reflection: no 'parameters' "
                               "array (top-level type {})",
                               j.type_name ());
      return;
    }

    for (auto &p : *params) {
      if (!p.is_object ())
        continue;
      auto b = p.find ("binding");
      if (b == p.end () || !b->is_object ())
        continue;
      std::string kind = b->value ("kind", "");
      uint32_t set = b->value ("space", 0U);
      uint32_t binding = b->value ("index", 0U);
      std::string name = p.value ("name", "");

      if (kind == "constantBuffer") {
        shader_uniform_buffer ub{};
        ub.name = name;
        ub.set = set;
        ub.binding = binding;
        uint32_t ubo_size = 0;
        auto t = p.find ("type");
        if (t != p.end ()) {
          auto et = t->find ("elementType");
          if (et != t->end ()) {
            auto fields = et->find ("fields");
            if (fields != et->end () && fields->is_array ()) {
              for (auto &f : *fields) {
                if (!f.is_object ())
                  continue;
                auto fb = f.find ("binding");
                if (fb == f.end ())
                  continue;
                shader_buffer_member m{};
                m.name = f.value ("name", "");
                m.offset = fb->value ("offset", 0U);
                m.size = fb->value ("size", 0U);
                ubo_size = std::max (ubo_size, m.offset + m.size);
                ub.members.push_back (std::move (m));
              }
            }
            auto evl = et->find ("elementVarLayout");
            if (evl != et->end ()) {
              auto eb = evl->find ("binding");
              if (eb != evl->end ()) {
                uint32_t s = eb->value ("size", 0U);
                if (s)
                  ubo_size = s;
              }
            }
          }
        }
        ub.size = ubo_size;
        target->uniform_buffers.push_back (std::move (ub));
      } else if (kind == "shaderResource") {
        auto t = p.find ("type");
        bool is_structured = false;
        if (t != p.end () && t->is_object ()) {
          auto base = t->find ("baseShape");
          if (base != t->end () && base->is_string ())
            is_structured = (base->get<std::string> () == "structuredBuffer");
        }
        if (is_structured) {
          // Read-only storage buffers (e.g. clustered light / cluster
          // buffers) are reported as shaderResource with a structuredBuffer
          // base shape. Route them to the storage-buffer list so the
          // pipeline declares the matching binding count.
          shader_storage_buffer sb{};
          sb.name = name;
          sb.set = set;
          sb.binding = binding;
          target->storage_buffers.push_back (std::move (sb));
        } else {
          shader_texture_binding tex{};
          tex.name = name;
          tex.set = set;
          tex.binding = binding;
          if (t != p.end () && t->is_object ()) {
            auto base = t->find ("baseShape");
            if (base != t->end () && base->is_string ())
              tex.is_cube = (base->get<std::string> ().find ("Cube")
                             != std::string::npos);
          }
          target->textures.push_back (std::move (tex));
        }
      } else if (kind == "samplerState") {
        // A `SamplerState` (often `u_Samplers[N]` array) declares N sampler
        // slots. SDL_GPU must declare exactly that many samplers on the
        // pipeline, so record the largest slot count we observe.
        uint32_t count = 1;
        auto t = p.find ("type");
        if (t != p.end ()) {
          auto arr = t->find ("array");
          if (arr != t->end ()) {
            count = arr->value ("elementCount", 1U);
          }
        }
        target->sampler_count
            = std::max (target->sampler_count, binding + count);
      }
    }
  } catch (const std::exception &e) {
    wsl::log::gfx ()->warn ("[shader_compiler] reflection parse aborted "
                            "exceptionally (continuing without reflection): {}",
                            e.what ());
  }

  wsl::log::gfx ()->debug ("[shader_compiler] reflection parsed: {} UBO(s), {} "
                           "texture(s), {} sampler(s)",
                           target->uniform_buffers.size (),
                           target->textures.size (), target->sampler_count);
}

bool
shader_compiler::compile_vertex (const char *source, size_t source_len,
                                 shader_program &out_program)
{
  return compile_stage (source, source_len, SDL_GPU_SHADERSTAGE_VERTEX,
                        out_program);
}

bool
shader_compiler::compile_fragment (const char *source, size_t source_len,
                                   shader_program &out_program)
{
  return compile_stage (source, source_len, SDL_GPU_SHADERSTAGE_FRAGMENT,
                        out_program);
}

bool
shader_compiler::compile_stage (const char *source, size_t /*source_len*/,
                                SDL_GPUShaderStage stage, shader_program &out)
{
  std::string slangc = find_slangc ();
  if (slangc.empty ()) {
    m_last_error = "slangc executable not found (set WEASEL_SLANGC or place "
                   "slangc next to the executable)";
    return false;
  }

  std::error_code ec;
  auto tmp_dir = std::filesystem::temp_directory_path (ec);
  if (ec)
    tmp_dir = std::filesystem::path ("/tmp");

  std::string tag = (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? "vert" : "frag";
  std::string stem = "weasel_rt_" + tag + "_" + std::to_string (getpid ());
  std::filesystem::path src_path = tmp_dir / (stem + ".slang");
  std::filesystem::path spv_path = tmp_dir / (stem + ".spv");
  std::filesystem::path json_path = tmp_dir / (stem + ".json");

  {
    std::ofstream ofs (src_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      m_last_error = "failed to write temporary shader source";
      return false;
    }
    ofs << source;
  }

  std::string command = "\"" + slangc + "\" \"" + src_path.string () + "\""
                        + slangc_flags (stage) + " -o \"" + spv_path.string ()
                        + "\"" + " -reflection-json \"" + json_path.string ()
                        + "\"";

  std::string diagnostics;
  int rc = run_capture (command, diagnostics);
  if (!diagnostics.empty ()) {
    // Slang mixes warnings and errors on stderr; surface them as warnings
    // so benign register/shift warnings don't bury real failures.
    wsl::log::gfx ()->warn ("Slang codegen diagnostics: {}", diagnostics);
  }

  if (rc != 0 || !std::filesystem::exists (spv_path)) {
    m_last_error = "slangc failed to compile shader:\n" + diagnostics;
    std::filesystem::remove (src_path, ec);
    return false;
  }

  std::ifstream is (spv_path, std::ios::binary);
  std::vector<uint8_t> bytes ((std::istreambuf_iterator<char> (is)),
                              std::istreambuf_iterator<char> ());
  if (bytes.empty ()) {
    m_last_error = "slangc produced empty shader output";
    std::filesystem::remove (src_path, ec);
    std::filesystem::remove (spv_path, ec);
    return false;
  }

  if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
    out.vertex_bytecode = std::move (bytes);
  } else {
    out.fragment_bytecode = std::move (bytes);
  }
  out.format = native_format ();

  parse_reflection (json_path, stage, out);

  std::filesystem::remove (src_path, ec);
  std::filesystem::remove (spv_path, ec);
  std::filesystem::remove (json_path, ec);
  return true;
}

bool
shader_compiler::reflect (shader_program & /*program*/)
{
  // Reflection is now extracted automatically during compilation.
  // This method is kept for API compatibility.
  return true;
}

} // namespace gfx

} // namespace wsl
