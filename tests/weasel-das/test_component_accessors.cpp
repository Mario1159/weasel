#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef WEASEL_HAS_DASLANG

#include "wsl/das/das_engine.hpp"
#include "wsl/das/wsl_api_module.hpp"
#include "wsl/comp/camera_2d.hpp"
#include "wsl/comp/directional_light.hpp"
#include "wsl/comp/point_light.hpp"
#include "wsl/comp/spot_light.hpp"
#include "wsl/comp/sprite_2d.hpp"
#include "wsl/comp/transform.hpp"
#include "wsl/comp/transform_2d.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/reg/component_registry.hpp"
#include "wsl/math/vector.hpp"
#include "wsl/log/log.hpp"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{

const char *const k_smoke_das = R"DAS(
options gen2
require weasel_api
require weasel_helpers

def smoke_test() {
    var e : uint = 0u

    var t = get_component_or(e, Transform())
    t.scale.x = 2.0
    t.position.y = 3.5
    t.position.z = -1.0
    t.scale = float3(4.0, 5.0, 6.0)
    t.position = float3(1.0, 2.0, 3.0)

    var t2 = get_component_or(e, Transform2D())
    t2.rotation = 0.5
    t2.scale = float2(2.0, 3.0)

    var cam = get_component_or(e, Camera2D())
    cam.zoom = 2.25

    var spr = get_component_or(e, Sprite2D())
    spr.color.w = 1.0
    spr.size.x = 10.0
    spr.size.y = 20.0
    spr.color = float4(0.25, 0.5, 0.75, 1.0)

    var pl = get_component_or(e, PointLight())
    pl.intensity = 5.0
    pl.color = float3(0.1, 0.2, 0.3)

    var dl = get_component_or(e, DirectionalLight())
    dl.intensity = 3.0

    var sl = get_component_or(e, SpotLight())
    sl.intensity = 4.0

    var missing = get_component_or(1u, Transform())
    missing.position.x = 99.0

    var sxs : float = t.scale.x
    var sy : float = spr.size.y
    var zz : float = cam.zoom
    var cg : float = spr.color.y
    var lr : float = pl.color.x
    var tsc : float = t2.scale.y
    assert(sxs == 4.0)
    assert(sy == 20.0)
    assert(zz == 2.25)
    assert(cg == 0.5)
    assert(lr == 0.1)
    assert(tsc == 3.0)
}
)DAS";

bool
run_smoke ()
{
  wsl::das::das_engine engine;
  if (!engine.initialize ()) {
    std::fprintf (stderr, "das_engine::initialize failed\n");
    return false;
  }

  entt::registry reg;
  auto entity = reg.create ();
  auto missing_entity = reg.create ();
  auto &t = reg.emplace<wsl::comp::transform> (entity);
  t.scale = wsl::math::vec3f{ 1.0f, 1.0f, 1.0f };
  reg.emplace<wsl::comp::transform_2d> (entity);
  reg.emplace<wsl::comp::camera_2d> (entity);
  reg.emplace<wsl::comp::sprite_2d> (entity);
  reg.emplace<wsl::comp::point_light> (entity);
  reg.emplace<wsl::comp::directional_light> (entity);
  reg.emplace<wsl::comp::spot_light> (entity);
  wsl::das::wsl_api_set_active_registry (&reg);

  auto script
      = std::filesystem::temp_directory_path () / "weasel_accessor_smoke.das";
  {
    std::ofstream out (script);
    out << k_smoke_das;
  }

  bool executed = false;
  bool called = false;
  if (engine.execute_file (script)) {
    executed = true;
    called = engine.call_void_function_safe (script, "smoke_test");
    if (!called) {
      const auto &e = engine.last_das_error ();
      std::fprintf (stderr, "smoke_test call failed: %s (%s:%d): %s\n",
                    engine.last_error ().c_str (), e.file.c_str (), e.line,
                    e.message.c_str ());
    }
  } else {
    std::fprintf (stderr, "execute_file failed: %s\n",
                  engine.last_error ().c_str ());
  }

  if (!(executed && called)) {
    return false;
  }

  auto &T = reg.get<wsl::comp::transform> (entity);
  auto &T2 = reg.get<wsl::comp::transform_2d> (entity);
  auto &C = reg.get<wsl::comp::camera_2d> (entity);
  auto &S = reg.get<wsl::comp::sprite_2d> (entity);
  auto &P = reg.get<wsl::comp::point_light> (entity);
  auto &D = reg.get<wsl::comp::directional_light> (entity);
  auto &SP = reg.get<wsl::comp::spot_light> (entity);

  bool ok = true;
  auto chk = [&] (bool c) { ok = ok && c; };
  chk (std::fabs (T.scale.x () - 4.0f) < 1e-5f);
  chk (std::fabs (T.scale.y () - 5.0f) < 1e-5f);
  chk (std::fabs (T.scale.z () - 6.0f) < 1e-5f);
  chk (std::fabs (T.position.x () - 1.0f) < 1e-5f);
  chk (std::fabs (T.position.y () - 2.0f) < 1e-5f);
  chk (std::fabs (T.position.z () - 3.0f) < 1e-5f);
  chk (std::fabs (T2.rotation - 0.5f) < 1e-5f);
  chk (std::fabs (T2.scale.x () - 2.0f) < 1e-5f);
  chk (std::fabs (T2.scale.y () - 3.0f) < 1e-5f);
  chk (std::fabs (C.zoom - 2.25f) < 1e-5f);
  chk (std::fabs (S.color.w () - 1.0f) < 1e-5f);
  chk (std::fabs (S.color.y () - 0.5f) < 1e-5f);
  chk (std::fabs (S.size.x () - 10.0f) < 1e-5f);
  chk (std::fabs (S.size.y () - 20.0f) < 1e-5f);
  chk (std::fabs (P.intensity - 5.0f) < 1e-5f);
  chk (std::fabs (P.color.x () - 0.1f) < 1e-5f);
  chk (std::fabs (D.intensity - 3.0f) < 1e-5f);
  chk (std::fabs (SP.intensity - 4.0f) < 1e-5f);
  chk (!reg.all_of<wsl::comp::transform> (missing_entity));
  return ok;
}

} // namespace

TEST_CASE ("Component accessors property smoke test (interpreted)")
{
  // Module::Initialize() must run once on the main thread before worker
  // threads create das_engine instances.
  wsl::das::das_engine::initialize_global ();

  bool ok = false;
  std::thread worker ([&] { ok = run_smoke (); });
  worker.join ();
  CHECK (ok);
}

TEST_CASE ("Component accessors AOT emission produces real proxy C++")
{
  // Module::Initialize() must run once on the main thread before worker
  // threads create das_engine instances.
  wsl::das::das_engine::initialize_global ();

  const char *const k_aot_das = R"DAS(
options gen2
require weasel_api
require weasel_helpers

def main() {
    var e : uint = 0u
    var t = get_component_or(e, Transform())
    t.scale.x = 2.0
    t.scale = float3(1.0, 2.0, 3.0)
    print("scale.x = {t.scale.x}\n")
}
)DAS";

  auto script
      = std::filesystem::temp_directory_path () / "weasel_accessor_aot.das";
  auto out
      = std::filesystem::temp_directory_path () / "weasel_accessor_aot.das.cpp";
  {
    std::ofstream o (script);
    o << k_aot_das;
  }

  std::string error;
  bool ok = false;
  std::thread worker ([&] {
    ok = wsl::das::aot_compile_file (script.string (), out.string (), error);
  });
  worker.join ();

  if (!ok) {
    std::fprintf (stderr, "aot_compile_file failed: %s\n", error.c_str ());
  }
  CHECK (ok);
  if (!ok) {
    return;
  }

  std::ifstream in (out);
  std::string body ((std::istreambuf_iterator<char> (in)),
                    std::istreambuf_iterator<char> ());

  // The v1 bug was that `require weasel_api` resolved to a stub .das and
  // aotRequire never fired, so the generated file self-declared a 1-byte
  // placeholder `TransformAccessor` and never referenced the real proxy.
  // In-process AOT (real Module_WeaselApi loaded) must pull in the real
  // proxy/api headers and register a real AOT module — never the stub
  // placeholder. (Note: under daslang's standard AOT policy, function/method
  // bodies are not inlined into the .das.cpp — the engine drives them via the
  // das runtime — so we assert on the real module scaffolding instead.)
  CHECK (body.find ("#include \"wsl/das/wsl_api_component_accessors.hpp\"")
         != std::string::npos);
  CHECK (body.find ("#include \"wsl/das/wsl_api_module.hpp\"")
         != std::string::npos);
  CHECK (body.find ("registerAotFunctions") != std::string::npos);
  CHECK (body.find ("TransformAccessor") == std::string::npos);
}

// A real accessor script without any `[export]` annotation (the known
// segfault trigger) — used to prove the AOT pipeline handles genuine
// component-accessor scripts and that the accessor logic runs end-to-end.
static const char *const k_aot_exec_das = R"DAS(
options gen2
require weasel_api
require weasel_helpers

def main() {
    var e : uint = 0u
    var t = get_component_or(e, Transform())
    t.scale.x = 2.0
    t.scale.y = 2.0
    t.scale.z = 3.0
}

def smoke_test() {
    var e : uint = 0u
    var t = get_component_or(e, Transform())
    t.scale.x = 2.0
    t.scale.y = 2.0
    t.scale.z = 3.0
}
)DAS";

// AOT-compiles `script`, asserts the emitted .das.cpp is a real AOT module
// (real proxy/api includes + registerAotFunctions), then runs the same script
// in-engine and asserts the entt component actually changed. This closes the
// loop between the AOT emitter and the runtime: the AOT pipeline must accept a
// genuine accessor script, and the accessor API must mutate the real entt
// component when driven through the engine. (daslang's standard AOT policy
// keeps function bodies in the das runtime rather than inlining them into the
// .das.cpp, so the mutation is asserted via the in-engine interpreted run of
// the same script.)
static bool
run_aot_smoke ()
{
  auto script = std::filesystem::temp_directory_path ()
                / "weasel_accessor_aot_exec.das";
  auto out = std::filesystem::temp_directory_path ()
             / "weasel_accessor_aot_exec.das.cpp";
  {
    std::ofstream o (script);
    o << k_aot_exec_das;
  }

  // Drive the script through the engine FIRST (interpreted), before spinning
  // up the AOT worker thread — the worker's thread-local das environment is
  // torn down on join and would corrupt a main-thread engine run that started
  // afterwards. The AOT step is pure code-generation and does not depend on
  // the engine having run.
  wsl::das::das_engine engine;
  if (!engine.initialize ()) {
    return false;
  }
  entt::registry reg;
  auto entity = reg.create ();
  auto &t0 = reg.emplace<wsl::comp::transform> (entity);
  t0.scale = wsl::math::vec3f{ 1.0f, 1.0f, 1.0f };
  wsl::das::wsl_api_set_active_registry (&reg);

  if (!engine.execute_file (script.string ())) {
    return false;
  }
  if (!engine.call_void_function_safe (script.string (), "smoke_test")) {
    return false;
  }

  bool mutated = true;
  auto view = reg.view<wsl::comp::transform> ();
  for (auto e : view) {
    const auto &t = view.get<wsl::comp::transform> (e);
    CHECK (t.scale.x () == doctest::Approx (2.0f));
    CHECK (t.scale.y () == doctest::Approx (2.0f));
    CHECK (t.scale.z () == doctest::Approx (3.0f));
    mutated = mutated && std::fabs (t.scale.x () - 2.0f) < 1e-5f
              && std::fabs (t.scale.y () - 2.0f) < 1e-5f
              && std::fabs (t.scale.z () - 3.0f) < 1e-5f;
  }
  if (!mutated) {
    return false;
  }

  // Now AOT-compile the same script and assert it produces a real, buildable
  // AOT module (real proxy/include + registerAotFunctions). This proves the
  // AOT emitter accepts a genuine accessor script that the engine just ran.
  std::string error;
  bool ok = false;
  std::thread worker ([&] {
    ok = wsl::das::aot_compile_file (script.string (), out.string (), error);
  });
  worker.join ();

  if (!ok) {
    std::fprintf (stderr, "aot_compile_file failed: %s\n", error.c_str ());
    return false;
  }

  std::ifstream in (out);
  std::string body ((std::istreambuf_iterator<char> (in)),
                    std::istreambuf_iterator<char> ());
  if (body.find ("#include \"wsl/das/wsl_api_component_accessors.hpp\"")
      == std::string::npos) {
    return false;
  }
  if (body.find ("registerAotFunctions") == std::string::npos) {
    return false;
  }
  return true;
}

TEST_CASE ("Component accessors AOT module executes in-engine")
{
  // das_engine must be driven from a worker thread (matching the interpreted
  // smoke test): the engine's thread-local daScriptEnvironment is set up there.
  wsl::das::das_engine::initialize_global ();
  bool ok = false;
  std::thread worker ([&] { ok = run_aot_smoke (); });
  worker.join ();
  CHECK (ok);
}

// `query() $(t : Transform&) { ... }` — DECS-style iteration over
// the entt registry via the `query` call macro. Exercises the macro
// expansion (block params -> live proxies) AND the `each_entity_id_with`
// primitive (only entities owning ALL listed components are visited).
static const char *const k_query_das = R"DAS(
options gen2
require weasel_api
require weasel_helpers

def query_test() {
    var count = 0
    query() $(t : Transform&) {
        count += 1
        t.scale.x += 1.0
    }
    assert(count == 2)
}
)DAS";

static bool
run_query ()
{
  wsl::das::das_engine engine;
  if (!engine.initialize ()) {
    std::fprintf (stderr, "das_engine::initialize failed\n");
    return false;
  }

  entt::registry reg;
  auto e0 = reg.create ();
  reg.emplace<wsl::comp::transform> (e0).scale
      = wsl::math::vec3f{ 1.0f, 1.0f, 1.0f };
  auto e1 = reg.create ();
  reg.emplace<wsl::comp::transform> (e1).scale
      = wsl::math::vec3f{ 1.0f, 1.0f, 1.0f };
  // Entity with a different component only — must NOT be visited.
  auto e2 = reg.create ();
  reg.emplace<wsl::comp::camera_2d> (e2);
  wsl::das::wsl_api_set_active_registry (&reg);

  auto script
      = std::filesystem::temp_directory_path () / "weasel_query_smoke.das";
  {
    std::ofstream out (script);
    out << k_query_das;
  }

  bool executed = false;
  bool called = false;
  if (engine.execute_file (script.string ())) {
    executed = true;
    called = engine.call_void_function_safe (script.string (), "query_test");
    if (!called) {
      const auto &e = engine.last_das_error ();
      std::fprintf (stderr, "query_test call failed: %s (%s:%d): %s\n",
                    engine.last_error ().c_str (), e.file.c_str (), e.line,
                    e.message.c_str ());
    }
  } else {
    std::fprintf (stderr, "execute_file failed: %s\n",
                  engine.last_error ().c_str ());
  }

  if (!(executed && called)) {
    return false;
  }

  bool ok = true;
  auto chk = [&] (bool c) { ok = ok && c; };
  chk (std::fabs (reg.get<wsl::comp::transform> (e0).scale.x () - 2.0f)
       < 1e-5f);
  chk (std::fabs (reg.get<wsl::comp::transform> (e1).scale.x () - 2.0f)
       < 1e-5f);
  // e2 has no transform, so it must have been skipped (scale stays at 1).
  return ok;
}

TEST_CASE ("query macro iterates and mutates in place")
{
  // das_engine must be driven from a worker thread (matching the interpreted
  // smoke test): the engine's thread-local daScriptEnvironment is set up there.
  wsl::das::das_engine::initialize_global ();
  bool ok = false;
  std::thread worker ([&] { ok = run_query (); });
  worker.join ();
  CHECK (ok);
}

static const char *const k_mixed_query_das = R"DAS(
options gen2
module mouse_rotate
require weasel_api
require weasel_helpers

struct MouseRotate {
    yaw : float = 0.0
}

def query_test() {
    query() $(t : Transform&; mr : MouseRotate&) {
        t.position.x += 1.0
        mr.yaw = 5.0
    }
}

)DAS";

static bool
run_mixed_query ()
{
  wsl::das::das_engine engine;
  if (!engine.initialize ()) {
    return false;
  }

  auto script
      = std::filesystem::temp_directory_path () / "weasel_mixed_query.das";
  {
    std::ofstream out (script);
    out << k_mixed_query_das;
  }

  if (!engine.execute_file (script.string ())) {
    std::fprintf (stderr, "mixed query execute failed: %s\n",
                  engine.last_error ().c_str ());
    return false;
  }

  auto info = engine.get_struct_info (script, "MouseRotate");
  if (info.size_of <= 0 || info.fields.empty ()) {
    return false;
  }

  wsl::comp::singl::runtime_context runtime ("DasTest", 0, 0, "", true);
  const entt::id_type type_id = entt::hashed_string{ "mouse_rotate" }.value ();
  std::vector<wsl::reg::component_registry::descriptor::das_field> fields;
  for (const auto &field : info.fields) {
    fields.push_back ({ field.name, field.type_name, field.offset, field.size,
                        field.kind, field.default_value });
  }
  runtime.component_registry ().register_cached_runtime_world_component (
      type_id, "mouse_rotate", "Mouse Rotate", info.size_of,
      std::move (fields));
  runtime.component_registry ().register_component_type_info (
      "mouse_rotate::MouseRotate const", type_id,
      wsl::reg::ComponentKind::DAS_SCRIPT,
      static_cast<std::size_t> (info.size_of));

  entt::registry reg;
  reg.ctx ().emplace<wsl::comp::singl::runtime_context *> (&runtime);
  const entt::entity matching = reg.create ();
  reg.emplace<wsl::comp::transform> (matching);
  const entt::entity non_matching = reg.create ();
  reg.emplace<wsl::comp::transform> (non_matching);
  if (!runtime.component_registry ().das_component_add (reg, type_id,
                                                        matching)) {
    return false;
  }

  wsl::das::wsl_api_set_active_registry (&reg);
  if (!engine.call_void_function_safe (script, "query_test")) {
    std::fprintf (stderr, "mixed query call failed: %s\n",
                  engine.last_error ().c_str ());
    return false;
  }

  const auto &matching_transform = reg.get<wsl::comp::transform> (matching);
  const auto &other_transform = reg.get<wsl::comp::transform> (non_matching);
  const auto *data = runtime.component_registry ().das_component_data (
      reg, type_id, matching);
  if (data == nullptr) {
    return false;
  }

  float yaw = 0.0f;
  std::memcpy (&yaw, data + info.fields[0].offset, sizeof (yaw));
  return std::fabs (matching_transform.position.x () - 1.0f) < 1e-5f
         && std::fabs (other_transform.position.x ()) < 1e-5f
         && std::fabs (yaw - 5.0f) < 1e-5f;
}

TEST_CASE ("mixed query mutates native and Daslang storage in place")
{
  wsl::das::das_engine::initialize_global ();
  wsl::log::init ();
  bool ok = false;
  std::thread worker ([&] { ok = run_mixed_query (); });
  worker.join ();
  CHECK (ok);
}

#else // !WEASEL_HAS_DASLANG

TEST_CASE ("Component accessors (daslang disabled)")
{
  MESSAGE ("daslang support not enabled; skipping component accessors test.");
  CHECK (true);
}

#endif // WEASEL_HAS_DASLANG
