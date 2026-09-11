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
#include <unordered_map>
#endif
#ifndef IN_MODULE_INTERFACE
#include <unordered_set>
#endif
#ifndef IN_MODULE_INTERFACE
#include <vector>
#endif

#ifndef IN_MODULE_INTERFACE
#include <entt/entt.hpp>
#endif

#include "reg/system_factory_registry.hpp"
#include "sys/stage_registry.hpp"
#include "sys/system.hpp"

namespace wsl::sys
{

/** Tunable behavior of the dependency graph / scheduler. */
struct graph_config
{
  bool parallel_systems_enabled = true;
  bool parallel_render_build_enabled = true;
  bool stage_fences_enabled = false;
  bool strict_system_ordering = false;
  bool warn_undeclared_cross_tier = true;
  bool deterministic_system_order = false;
};

/**
 * Builds execution levels for a pass from:
 *  - declared system dependencies (type-id and string, resolved to instances),
 *  - component read/write access captured via `register_iteration_rw`
 *    (CADS), and
 *  - stage order (used to resolve write-after-write conflicts).
 *
 * The result is a list of levels; every system in level N must finish before
 * level N+1 begins. The caller fences between levels.
 */
class system_dependency_graph
{
public:
  struct node_info
  {
    ecs_system *sys;
    std::vector<entt::id_type> reads;
    std::vector<entt::id_type> writes;
    std::size_t stage_pos;
    bool failed;
    bool unknown_access; // no captured component access at all
  };

  static std::vector<std::vector<ecs_system *>>
  build_levels (const std::vector<ecs_system *> &systems,
                const stage_registry &stages,
                const reg::system_factory_registry &factory,
                const graph_config &cfg)
  {
    const std::size_t n = systems.size ();
    if (n == 0) {
      return {};
    }

    // Per-system resolved data.
    std::vector<node_info> inf (n);
    std::unordered_map<entt::id_type, std::vector<std::size_t>> by_type;

    for (std::size_t i = 0; i < n; ++i) {
      ecs_system *s = systems[i];
      inf[i].sys = s;
      s->collect_component_access (inf[i].reads, inf[i].writes);
      inf[i].stage_pos = stages.position_of (s->get_stage ());
      if (inf[i].stage_pos == stage_registry::k_invalid) {
        wsl::log::sys ()->warn (
            "system '{}' references unknown stage '{}'; defaulting to '{}'",
            s->get_name (), s->get_stage (), stage_registry::default_stage);
        inf[i].stage_pos = stages.position_of (stage_registry::default_stage);
      }
      inf[i].failed = s->has_failed ();
      inf[i].unknown_access = inf[i].reads.empty () && inf[i].writes.empty ();
      by_type[s->get_type_id ()].push_back (i);
    }

    // Predecessor sets (predecessors of i must finish before i).
    std::vector<std::vector<std::size_t>> preds (n);

    auto add_edge = [&] (std::size_t from, std::size_t to) {
      if (from == to) {
        return;
      }
      preds[to].push_back (from);
    };

    // Declared relationship set (either direction). Declared dependencies
    // dominate inferred component edges, so a system that both declares a
    // dependency AND shares a component with another never produces a
    // contradictory inferred edge (which would create a cycle).
    std::unordered_set<uint64_t> declared_rel;
    auto rel_key = [&] (std::size_t a, std::size_t b) { return a * n + b; };
    auto add_declared_edge = [&] (std::size_t from, std::size_t to) {
      if (from == to) {
        return;
      }
      add_edge (from, to);
      declared_rel.insert (rel_key (from, to));
      declared_rel.insert (rel_key (to, from));
    };

    // 1) Declared dependencies.
    std::vector<bool> skip (n, false);
    for (std::size_t i = 0; i < n; ++i) {
      ecs_system *s = systems[i];
      // type-id based
      for (const auto &dep :
           factory.get_system_dependencies (s->get_type_id ())) {
        for (std::size_t j : by_type[dep.type_id]) {
          add_declared_edge (j, i);
        }
      }
      // string based (with instance-name fallback)
      for (const std::string &name : s->get_dependencies ()) {
        std::vector<std::size_t> targets
            = resolve_string (name, systems, factory);
        for (std::size_t j : targets) {
          add_declared_edge (j, i);
        }
      }
    }

    // 2) Component read/write edges (CADS) + WAW stage resolution.
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) {
        if (i == j) {
          continue;
        }
        const node_info &A = inf[i];
        const node_info &B = inf[j];
        // Declared dependency already orders this pair; do not add a
        // contradictory inferred edge.
        if (declared_rel.count (rel_key (i, j))) {
          continue;
        }
        if (A.writes.empty () || (B.reads.empty () && B.writes.empty ())) {
          // WAW only needs writers; skip if B has no captured access.
        }
        // A writes C, B reads C  -> A before B. Break mutual read/write
        // 2-cycles deterministically (lower type_id owns the edge).
        if (overlap (A.writes, B.reads)) {
          bool mutual = overlap (B.writes, A.reads);
          if (mutual && A.sys->get_type_id () > B.sys->get_type_id ()) {
            continue;
          }
          add_edge (i, j);
        }
        // A writes C, B writes C (WAW) -> order by stage, then type_id
        if (overlap (A.writes, B.writes)) {
          resolve_waw (i, j, inf, stages, cfg, add_edge);
        }
      }
    }

    // 3) Failed cascade: drop systems whose dependency failed.
    for (std::size_t i = 0; i < n; ++i) {
      // any predecessor failed?
      bool dep_failed = false;
      for (std::size_t p : preds[i]) {
        if (inf[p].failed) {
          dep_failed = true;
        }
      }
      if (inf[i].failed || dep_failed) {
        skip[i] = true;
      }
    }

    // Topological level assignment (longest path) via Kahn.
    std::vector<std::size_t> level (n, 0);
    std::vector<std::size_t> indeg (n, 0);
    for (std::size_t i = 0; i < n; ++i) {
      indeg[i] = preds[i].size ();
    }
    std::vector<std::size_t> ready;
    for (std::size_t i = 0; i < n; ++i) {
      if (indeg[i] == 0 && !skip[i]) {
        ready.push_back (i);
      }
    }
    std::vector<std::size_t> order;
    while (!ready.empty ()) {
      std::size_t i = ready.back ();
      ready.pop_back ();
      order.push_back (i);
      for (std::size_t j = 0; j < n; ++j) {
        if (skip[j]) {
          continue;
        }
        auto it = std::find (preds[j].begin (), preds[j].end (), i);
        if (it != preds[j].end ()) {
          level[j] = std::max (level[j], level[i] + 1);
          if (--indeg[j] == 0) {
            ready.push_back (j);
          }
        }
      }
    }
    // Any node not in `order` is part of a cycle.
    if (order.size () + count (skip.begin (), skip.end (), true) < n) {
      wsl::log::sys ()->error (
          "system_dependency_graph: cycle detected; serializing remaining "
          "systems in a fallback level");
      for (std::size_t i = 0; i < n; ++i) {
        if (skip[i]
            || std::find (order.begin (), order.end (), i) != order.end ()) {
          continue;
        }
        level[i] = *std::max_element (level.begin (), level.end ()) + 1;
        order.push_back (i);
      }
    }

    // 4) Unknown-access systems: serialize them (own levels, after everyone).
    std::size_t known_max = 0;
    for (std::size_t i = 0; i < n; ++i) {
      if (!inf[i].unknown_access && !skip[i]) {
        known_max = std::max (known_max, level[i]);
      }
    }
    // Order unknowns by type_id for determinism.
    std::vector<std::size_t> unknowns;
    for (std::size_t i = 0; i < n; ++i) {
      if (inf[i].unknown_access && !skip[i]) {
        unknowns.push_back (i);
      }
    }
    std::sort (unknowns.begin (), unknowns.end (),
               [&] (std::size_t a, std::size_t b) {
                 return systems[a]->get_type_id () < systems[b]->get_type_id ();
               });
    for (std::size_t k = 0; k < unknowns.size (); ++k) {
      level[unknowns[k]] = known_max + 1 + k;
    }

    // 5) Stage fences (optional): promote each stage boundary to a hard fence.
    if (cfg.stage_fences_enabled) {
      for (std::size_t i = 0; i < n; ++i) {
        if (skip[i]) {
          continue;
        }
        std::size_t base = inf[i].stage_pos;
        // raise to at least its stage position; keep intra-stage component
        // ordering by taking max with existing level relative offset.
        level[i] = std::max (level[i], base);
      }
      // ensure within-stage component edges still raise levels
      for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j : preds[i]) {
          if (inf[i].stage_pos == inf[j].stage_pos && !skip[i] && !skip[j]) {
            level[i] = std::max (level[i], level[j] + 1);
          }
        }
      }
    }

    const std::size_t max_level
        = *std::max_element (level.begin (), level.end ()) + 1;
    std::vector<std::vector<ecs_system *>> levels (max_level);
    for (std::size_t i = 0; i < n; ++i) {
      if (skip[i]) {
        continue;
      }
      levels[level[i]].push_back (systems[i]);
    }
    return levels;
  }

private:
  static bool
  overlap (const std::vector<entt::id_type> &a,
           const std::vector<entt::id_type> &b)
  {
    for (entt::id_type x : a) {
      if (std::find (b.begin (), b.end (), x) != b.end ()) {
        return true;
      }
    }
    return false;
  }

  // Resolve a declared dependency name to concrete instance indices.
  static std::vector<std::size_t>
  resolve_string (const std::string &name,
                  const std::vector<ecs_system *> &systems,
                  const reg::system_factory_registry &factory)
  {
    std::vector<std::size_t> out;
    // 1) display name via factory
    if (const auto *desc = factory.find_system (name)) {
      const entt::id_type tid = desc->type_id;
      for (std::size_t i = 0; i < systems.size (); ++i) {
        if (systems[i]->get_type_id () == tid) {
          out.push_back (i);
        }
      }
    }
    // 2) fallback: match any instance's get_name()
    if (out.empty ()) {
      for (std::size_t i = 0; i < systems.size (); ++i) {
        if (systems[i]->get_name () == name) {
          out.push_back (i);
        }
      }
    }
    return out;
  }

  // WAW: order writers by stage; same stage -> type_id tie-break + warning.
  static void
  resolve_waw (std::size_t i, std::size_t j, const std::vector<node_info> &inf,
               const stage_registry &stages, const graph_config &cfg,
               const std::function<void (std::size_t, std::size_t)> &add_edge)
  {
    const std::size_t si = inf[i].stage_pos;
    const std::size_t sj = inf[j].stage_pos;
    if (si < sj) {
      add_edge (i, j); // i earlier -> i before j (j wins)
    } else if (sj < si) {
      add_edge (j, i);
    } else {
      // same stage: stable order by type_id, warn (or error if strict)
      const char *kind = cfg.strict_system_ordering ? "error" : "warn";
      if (cfg.strict_system_ordering) {
        wsl::log::sys ()->error (
            "WAW on same stage between '{}' and '{}' — declare an explicit "
            "order or assign different stages",
            inf[i].sys->get_name (), inf[j].sys->get_name ());
      } else {
        wsl::log::sys ()->warn (
            "WAW between '{}' and '{}' on same stage '{}' — using type_id "
            "order (last writer wins), set strict_system_ordering for error",
            inf[i].sys->get_name (), inf[j].sys->get_name (),
            stages.stages ().empty () ? "" : stages.stages ()[si]);
      }
      (void)kind;
      if (inf[i].sys->get_type_id () < inf[j].sys->get_type_id ()) {
        add_edge (i, j);
      } else {
        add_edge (j, i);
      }
    }
  }
};

} // namespace wsl::sys
