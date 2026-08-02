#include "das_system_adapter.hpp"
#include "das_engine.hpp"
#include "wsl_api_module.hpp"
#include "../log/log.hpp"

#include "daScript/daScript.h"

namespace wsl::das
{

namespace
{

template <typename Fn>
bool
safe_invoke (Context *ctx, const char *method_name,
             const std::string &script_path, Fn &&fn)
{
  if (!ctx) {
    return false;
  }

  // Save context state (same as evalWithCatch)
  auto aa = ctx->abiArg;
  auto acm = ctx->abiCMRES;
  auto atba = ctx->abiThisBlockArg;
  char *EP, *SP;
  ctx->stack.watermark (EP, SP);
  auto *saved_throwBuf = ctx->throwBuf;

  // Install SIGSEGV handler and set throwBuf to our jmp_buf
  das_signal::install ();
  jmp_buf sj;
  das_signal::tls_jmp = &sj;
  ctx->throwBuf = &sj;

  bool caught = false;
  if (setjmp (sj) == 0) {
    fn ();
  } else {
    caught = true;
  }

  // Restore context state
  ctx->throwBuf = saved_throwBuf;
  das_signal::tls_jmp = nullptr;
  das_signal::restore ();

  if (caught) {
    const char *ex = ctx->getException ();
    if (ex) {
      std::string file;
      int line = ctx->exceptionAt.line;
      if (ctx->exceptionAt.fileInfo) {
        file = ctx->exceptionAt.fileInfo->name;
      }
      wsl::log::cmake ()->error (
          "das_system_adapter::{} failed in '{}': {} (at {}:{})", method_name,
          script_path, ex, file, line);
      ctx->clearException ();
    } else {
      wsl::log::cmake ()->error (
          "das_system_adapter::{} SEGFAULT in '{}' — possible null "
          "dereference or invalid pointer in daslang script",
          method_name, script_path);
    }
    // Restore context state after longjmp
    ctx->abiArg = aa;
    ctx->abiCMRES = acm;
    ctx->abiThisBlockArg = atba;
    ctx->stack.pop (EP, SP);
    return false;
  }

  // Check for daScript panics
  const char *ex = ctx->getException ();
  if (ex) {
    std::string file;
    int line = ctx->exceptionAt.line;
    if (ctx->exceptionAt.fileInfo) {
      file = ctx->exceptionAt.fileInfo->name;
    }
    wsl::log::cmake ()->error (
        "das_system_adapter::{} failed in '{}': {} (at {}:{})", method_name,
        script_path, ex, file, line);
    ctx->clearException ();
    return false;
  }
  return true;
}

} // anonymous namespace

das_system_adapter::das_system_adapter (const std::string &name,
                                        const std::string &script_path,
                                        das_engine &engine,
                                        entt::id_type type_id, void *class_ptr,
                                        const StructInfo *class_info,
                                        Context *ctx)
    : sys::ecs_system (name), EcsSystemAdapter (class_info),
      m_script_path (script_path), m_engine (engine), m_type_id (type_id),
      m_class_ptr (class_ptr), m_ctx (ctx)
{
  set_editor_active (false);
}

entt::id_type
das_system_adapter::get_type_id () const
{
  return m_type_id;
}

const char *
das_system_adapter::get_type_name () const
{
  return "das_system";
}

void
das_system_adapter::on_init (entt::registry &registry)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  m_has_failed = false;
  wsl_api_set_active_registry (&registry);
  auto fn = get_on_init (m_class_ptr);
  if (fn) {
    if (!safe_invoke (m_ctx, "on_init", m_script_path,
                      [&] () { invoke_on_init (m_ctx, fn, m_class_ptr); })) {
      m_has_failed = true;
      wsl::log::sys ()->warn ("System '{}' marked as failed, will be skipped",
                              get_name ());
    }
  }
}

void
das_system_adapter::on_update (entt::registry &registry, double dt)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  if (m_has_failed) {
    return;
  }
  wsl_api_set_active_registry (&registry);
  auto fn = get_on_update (m_class_ptr);
  if (fn) {
    if (!safe_invoke (m_ctx, "on_update", m_script_path, [&] () {
          invoke_on_update (m_ctx, fn, m_class_ptr, static_cast<float> (dt));
        })) {
      m_has_failed = true;
      wsl::log::sys ()->warn ("System '{}' marked as failed, will be skipped",
                              get_name ());
    }
  }
}

void
das_system_adapter::on_inactive (entt::registry &registry)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  wsl_api_set_active_registry (&registry);
  if (auto fn = get_on_inactive (m_class_ptr)) {
    if (!safe_invoke (m_ctx, "on_inactive", m_script_path, [&] () {
          invoke_on_inactive (m_ctx, fn, m_class_ptr);
        })) {
      m_has_failed = true;
      wsl::log::sys ()->warn ("System '{}' marked as failed, will be skipped",
                              get_name ());
    }
  }
}

void
das_system_adapter::on_event (registry_handle reg, const engine_event &ev)
{
  if (!m_class_ptr || !m_ctx) {
    return;
  }
  if (m_has_failed) {
    return;
  }
  wsl_api_set_active_registry (reg.get ());
  wsl_api_set_current_event (&ev);
  if (auto fn = get_on_event (m_class_ptr)) {
    if (!safe_invoke (m_ctx, "on_event", m_script_path,
                      [&] () { invoke_on_event (m_ctx, fn, m_class_ptr); })) {
      m_has_failed = true;
      wsl::log::sys ()->warn ("System '{}' marked as failed, will be skipped",
                              get_name ());
    }
  }
  wsl_api_set_current_event (nullptr);
}

} // namespace wsl::das
