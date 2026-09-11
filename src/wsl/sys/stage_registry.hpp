#pragma once

#ifndef IN_MODULE_INTERFACE
#include <algorithm>
#endif
#ifndef IN_MODULE_INTERFACE
#include <cstddef>
#endif
#ifndef IN_MODULE_INTERFACE
#include <string>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

namespace wsl::sys
{

/**
 * Ordered registry of execution **stages**.
 *
 * Stages are an ordered list of named buckets. A stage's order is its *position
 * in this vector* (not an author-supplied integer). Systems reference stages by
 * name. The scheduler uses stage order to resolve write-after-write (WAW)
 * conflicts: between two writers of the same component in different stages, the
 * earlier stage runs first and the later stage *wins* the component value.
 *
 * Stages are soft by default — systems across stages may overlap at runtime;
 * only WAW pairs are serialized by stage order. `stage_fences_enabled` promotes
 * every stage boundary to a hard fence.
 */
class stage_registry
{
public:
  static constexpr std::size_t k_invalid = static_cast<std::size_t> (-1);

  /** Default stage assigned to systems that declare none. */
  static constexpr const char *default_stage = "logic";

  stage_registry () { register_default_stages (); }

  /** Position of `name` in the ordered list, or `k_invalid` if unknown. */
  std::size_t
  position_of (const std::string &name) const
  {
    for (std::size_t i = 0; i < m_stages.size (); ++i) {
      if (m_stages[i] == name) {
        return i;
      }
    }
    return k_invalid;
  }

  bool
  contains (const std::string &name) const
  {
    return position_of (name) != k_invalid;
  }

  /** Append a stage at the end (no-op if already present). */
  void
  register_stage (const std::string &name)
  {
    if (!contains (name)) {
      m_stages.push_back (name);
    }
  }

  /** Insert `name` immediately before `before_name` (append if not found). */
  void
  insert_stage (const std::string &name, const std::string &before_name)
  {
    if (contains (name)) {
      return;
    }
    auto it = std::find (m_stages.begin (), m_stages.end (), before_name);
    if (it == m_stages.end ()) {
      m_stages.push_back (name);
    } else {
      m_stages.insert (it, name);
    }
  }

  /** Insert `name` at `position` (clamped to the end). */
  void
  register_stage_at (const std::string &name, std::size_t position)
  {
    if (contains (name)) {
      return;
    }
    if (position > m_stages.size ()) {
      position = m_stages.size ();
    }
    m_stages.insert (m_stages.begin () + static_cast<std::ptrdiff_t> (position),
                     name);
  }

  const std::vector<std::string> &
  stages () const
  {
    return m_stages;
  }

private:
  void
  register_default_stages ()
  {
    static const char *const defaults[]
        = { "input",   "pre_physics",  "transform",
            "physics", "post_physics", "animation",
            "logic",   "camera",       "render_build" };
    for (const char *d : defaults) {
      m_stages.push_back (d);
    }
  }

  std::vector<std::string> m_stages;
};

} // namespace wsl::sys
