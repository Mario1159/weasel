#include "repl_handler.hpp"

#include "comp/area3d.hpp"
#include "comp/camera.hpp"
#include "comp/character_body.hpp"
#include "comp/component_meta.hpp"
#include "comp/directional_light.hpp"
#include "comp/hierarchy.hpp"
#include "comp/model_instance_3d.hpp"
#include "comp/point_light.hpp"
#include "comp/rigid_body.hpp"
#include "comp/singl/physics_manager.hpp"
#include "comp/singl/rendering_manager.hpp"
#include "comp/spot_light.hpp"
#include "comp/transform.hpp"
#include "comp/world_transform.hpp"
#include "math/vector.hpp"
#include "rsc/resource_ids.hpp"
#include "rsc/resource_manager.hpp"
#include "wsl/rsc/project_loader.hpp"
#include "wsl/rsc/project.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/scene_snapshot_serializer.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/comp/singl/editor_context.hpp"
#include "wsl/comp/singl/ui_manager.hpp"
#include "wsl/sys/system.hpp"
#include "wsl/comp/components.hpp"

#include <entt/entt.hpp>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

using namespace entt::literals;

namespace wsl::cli {

// -------- command_executor implementation --------

command_executor::command_executor(wsl::comp::singl::runtime_context& rtc)
    : m_rtc(rtc)
{
}

std::string command_executor::execute(const std::string& line) {
    m_output.str("");
    m_output.clear();
    
    auto tokens = tokenize(line);
    if (tokens.empty()) return "";
    
    const std::string& family = tokens[0];
    if (family == "proj") cmd_proj(tokens);
    else if (family == "scene") cmd_scene(tokens);
    else if (family == "ent") cmd_ent(tokens);
    else if (family == "comp") cmd_comp(tokens);
    else if (family == "sig") cmd_sig(tokens);
    else if (family == "sys") cmd_sys(tokens);
    else if (family == "check") cmd_check(tokens);
    else if (family == "help") cmd_help();
    else if (family == "exit" || family == "quit") m_output << "exit\n";
    else if (family == "cls") m_output << "\033[2J\033[1;1H";
    else m_output << "Unknown command family: " << family << ". Type 'help' for usage.\n";
    
    return m_output.str();
}

std::vector<std::string> command_executor::tokenize(const std::string& line) {
    std::istringstream iss(line);
    return std::vector<std::string>{std::istream_iterator<std::string>{iss},
                                    std::istream_iterator<std::string>{}};
}

wsl::rsc::scene* command_executor::get_active_scene() {
    return m_rtc.scene_manager.get_active();
}

void command_executor::cmd_proj(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        m_output << "Usage: proj <new|load|save|info> [args...]\n";
        return;
    }
    const std::string& action = tokens[1];
    if (action == "new") {
        if (tokens.size() < 4) { m_output << "Usage: proj new <path> <name>\n"; return; }
        wsl::rsc::project proj;
        proj.name = tokens[3];
        proj.root_path = std::filesystem::absolute(tokens[2]).string();
        proj.systems_path = "src/systems";
        proj.components_path = "src/components";
        proj.singletons_path = "src/singletons";
        proj.scenes_path = "rsc/scenes";
        proj.models_path = "rsc/models";
        proj.images_path = "rsc/textures";
        proj.cubemaps_path = "rsc/textures/cubemaps";
        proj.audio_path = "rsc/audio";
        proj.fonts_path = "rsc/fonts";
        proj.ui_layouts_path = "src/ui";
        proj.shaders_path = "rsc/shaders";
        
        wsl::rsc::project_loader loader(&m_rtc);
        if (loader.create(proj)) {
            m_current_project = std::make_shared<wsl::rsc::project>(proj);
            m_output << "Project '" << proj.name << "' created and loaded.\n";
        } else m_output << "Failed to create project.\n";
    } else if (action == "load") {
        if (tokens.size() < 3) { m_output << "Usage: proj load <path>\n"; return; }
        m_current_project = wsl::rsc::project_loader::load(tokens[2]);
        if (m_current_project) {
            m_output << "Project '" << m_current_project->name << "' loaded.\n";
            const std::filesystem::path root(m_current_project->root_path);
            if (std::filesystem::exists(root / m_current_project->systems_path)) {
                m_rtc.runtime_project_module.compile_and_load(*m_current_project);
            }
        } else m_output << "Failed to load project.\n";
    } else if (action == "info") {
        if (!m_current_project) { m_output << "No project loaded.\n"; return; }
        m_output << "Project: " << m_current_project->name << "\nRoot: " << m_current_project->root_path << "\n";
    } else if (action == "save") {
         if (!m_current_project) { m_output << "No project loaded.\n"; return; }
         m_output << "Project saving via REPL is partially implemented (metadata only).\n";
    }
}

void command_executor::cmd_scene(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        m_output << "Usage: scene <new|load|save|ls|status> [args...]\n";
        return;
    }
    const std::string& action = tokens[1];
    if (action == "new") {
        if (tokens.size() < 3) { m_output << "Usage: scene new <name>\n"; return; }
        auto& scene = m_rtc.scene_manager.create_scene(tokens[2], true);
        m_output << "Scene '" << tokens[2] << "' created and set as active.\n";
    } else if (action == "load") {
        if (tokens.size() < 3) { m_output << "Usage: scene load <path>\n"; return; }
        if (!m_current_project) { m_output << "Error: Load a project first.\n"; return; }
        auto& scene = m_rtc.scene_manager.create_scene(tokens[2], true);
        wsl::rsc::io::scene_snapshot_serializer serializer(&m_rtc, scene);
        if (serializer.load_json(tokens[2])) {
            m_output << "Scene loaded from " << tokens[2] << "\n";
        } else m_output << "Failed to load scene.\n";
    } else if (action == "save") {
        auto* scene = get_active_scene();
        if (!scene) { m_output << "No active scene.\n"; return; }
        std::string path = (tokens.size() > 2) ? tokens[2] : scene->get_name() + ".wscn.json";
        wsl::rsc::io::scene_snapshot_serializer serializer(&m_rtc, *scene);
        if (serializer.save_json(path)) {
            m_output << "Scene saved to " << path << "\n";
        } else m_output << "Failed to save scene.\n";
    } else if (action == "ls") {
        if (!m_current_project) { m_output << "No project loaded.\n"; return; }
        auto assets = wsl::rsc::project_loader::scan_assets(*m_current_project);
        m_output << "Scenes in project:\n";
        for (const auto& s : assets.scenes) m_output << " - " << s << "\n";
    } else if (action == "status") {
        auto* scene = get_active_scene();
        if (!scene) { m_output << "No active scene.\n"; return; }
        m_output << "Active Scene: " << scene->get_name() << "\n";
        m_output << "Entities: " << scene->get_registry().storage<entt::entity>().size() << "\n";
    }
}

void command_executor::cmd_ent(const std::vector<std::string>& tokens) {
    auto* scene = get_active_scene();
    if (!scene) { m_output << "No active scene.\n"; return; }
    
    if (tokens.size() < 2) {
        m_output << "Usage: ent <new|ls|rm|ren|inspect> [args...]\n";
        return;
    }
    
    const std::string& action = tokens[1];
    if (action == "new") {
        auto e = scene->get_registry().create();
        if (tokens.size() > 2) scene->set_entity_name(e, tokens[2]);
        m_output << "Entity " << (uint32_t)e << " created.\n";
    } else if (action == "ls") {
        for (auto [e] : scene->get_registry().storage<entt::entity>().each()) {
            m_output << "ID: " << (uint32_t)e << " | Name: " << scene->get_entity_name(e) << "\n";
        }
    } else if (action == "rm") {
        if (tokens.size() < 3) return;
        entt::entity e = (entt::entity)std::stoul(tokens[2]);
        scene->get_registry().destroy(e);
        m_output << "Entity " << tokens[2] << " destroyed.\n";
    } else if (action == "ren") {
        if (tokens.size() < 4) return;
        entt::entity e = (entt::entity)std::stoul(tokens[2]);
        scene->set_entity_name(e, tokens[3]);
        m_output << "Entity " << tokens[2] << " renamed to '" << tokens[3] << "'.\n";
    } else if (action == "inspect") {
        if (tokens.size() < 3) return;
        entt::entity e = (entt::entity)std::stoul(tokens[2]);
        m_output << "Inspecting Entity " << tokens[2] << ":\n";
        m_output << " Name: " << scene->get_entity_name(e) << "\n";
        m_output << " Components:\n";
        cmd_comp({"comp", "ls", tokens[2]});
    }
}

void command_executor::cmd_comp(const std::vector<std::string>& tokens) {
    auto* scene = get_active_scene();
    if (!scene) return;
    
    if (tokens.size() < 3) {
        m_output << "Usage: comp <ls|add|rm|set> <ent_id> [args...]\n";
        return;
    }
    
    entt::entity e = (entt::entity)std::stoul(tokens[2]);
    const std::string& action = tokens[1];
    
    if (action == "ls") {
        for (auto [id, storage] : scene->get_registry().storage()) {
            if (storage.contains(e)) {
                auto type = entt::resolve(id);
                if (type) {
                    m_output << " - " << id << " (Hash)\n";
                } else {
                    m_output << " - Unknown Component (Hash: " << id << ")\n";
                }
            }
        }
    } else if (action == "add") {
        if (tokens.size() < 4) return;
        auto type = entt::resolve(entt::hashed_string(tokens[3].c_str()));
        if (type) {
            (void)type.construct(entt::forward_as_any(scene->get_registry()), entt::forward_as_any(e));
            m_output << "Added " << tokens[3] << " to " << tokens[2] << "\n";
        } else m_output << "Unknown component type: " << tokens[3] << "\n";
    }
}

void command_executor::cmd_sig(const std::vector<std::string>& tokens) {
    m_output << "Signal management not yet implemented in REPL.\n";
}

void command_executor::cmd_sys(const std::vector<std::string>& tokens) {
    auto* scene = get_active_scene();
    if (!scene) return;
    if (tokens.size() < 2) return;
    
    if (tokens[1] == "ls") {
        m_output << "Active Systems:\n";
    }
}

void command_executor::cmd_check(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) return;
    m_output << "Validation placeholder for " << tokens[1] << "\n";
}

void command_executor::cmd_help() {
    m_output << "Available commands:\n"
              << " proj <new|load|info>      - Project management\n"
              << " scene <new|load|save|ls>  - Scene management\n"
              << " ent <new|ls|rm|inspect>   - Entity management\n"
              << " comp <ls|add|rm|set>      - Component management\n"
              << " sig conn ...              - Signal management\n"
              << " sys ls ...                - System management\n"
              << " cls                       - Clear screen\n"
              << " help                      - Show this help\n"
              << " exit                      - Exit REPL\n";
}

// -------- repl_handler implementation --------

repl_handler::repl_handler(const std::string& engine_res_path, bool attach)
    : m_engine_res_path(engine_res_path)
    , m_attach(attach)
{
}

void repl_handler::run(std::optional<std::string> initial_project) {
    spdlog::info("repl_handler::run() called, m_attach={}", m_attach);
    // If attaching, connect to editor server
    if (m_attach) {
        if (!initial_project) {
            std::cerr << "Error: --attach requires --project to be specified.\n";
            return;
        }
        // Extract project root directory (not the wslpro.json file)
        std::filesystem::path proj_path(*initial_project);
        spdlog::info("repl_handler: initial project path: {}", proj_path.string());
        if (proj_path.filename().string() == "wslpro.json") {
            proj_path = proj_path.parent_path();
            spdlog::info("repl_handler: detected wslpro.json, using parent: {}", proj_path.string());
        }
        std::string abs_project_path = std::filesystem::weakly_canonical(proj_path).string();
        spdlog::info("repl_handler: attaching to project at: {}", abs_project_path);
        if (!m_editor_client.connect(abs_project_path)) {
            std::cerr << "Error: Failed to connect to editor server for project: " << *initial_project << "\n";
            std::cerr << "Make sure the editor is running with the same project loaded.\n";
            return;
        }
        spdlog::info("Connected to editor server for project: {}", *initial_project);
    } else {
        // Initialize local runtime context
        m_rtc = std::make_unique<wsl::comp::singl::runtime_context>("Weasel REPL", 0, 0, m_engine_res_path, true);
        m_rtc->set_editor_ctx(nullptr);
        
        // Register components/systems
        wsl::comp::singl::runtime_context::register_meta();
        wsl::comp::singl::editor_context::register_meta();
        wsl::comp::singl::ui_manager::register_meta();
        
        wsl::comp::register_component_meta<
            wsl::comp::hierarchy, wsl::comp::world_transform, wsl::comp::transform, wsl::math::vec3f, wsl::math::quatf,
            wsl::rsc::model_id, wsl::comp::model_instance_3d, wsl::comp::camera, wsl::comp::point_light,
            wsl::comp::spot_light, wsl::comp::directional_light,
            wsl::comp::rigid_body, wsl::comp::area, wsl::comp::character_body,
            wsl::rsc::scene_manager, wsl::rsc::resource_manager_view>();
        
        wsl::comp::for_each_type<wsl::comp::component_types>::apply ([this]<typename T> () {
            m_rtc->component_registry.register_world_component<T> ();
        });
        
        m_rtc->singleton_registry
            .register_bound_singleton_component<wsl::comp::singl::runtime_context> (
                { "Runtime Context", true });
        m_rtc->singleton_registry
            .register_bound_singleton_component<wsl::comp::singl::editor_context> (
                { "Editor Context", true });
        m_rtc->singleton_registry.register_bound_singleton_component<wsl::rsc::scene_manager> (
            { "Scene Manager", true });
        m_rtc->singleton_registry
            .register_bound_singleton_component<wsl::rsc::resource_manager_view> (
                { "Resource Manager", true });
        m_rtc->singleton_registry
            .register_bound_singleton_component<wsl::comp::singl::ui_manager> (
                { "UI Manager", true, false, true });
        m_rtc->singleton_registry
            .register_singleton_component<wsl::comp::singl::rendering_manager> (
                { "Rendering Manager", true });
        m_rtc->singleton_registry
            .register_singleton_component<wsl::comp::singl::physics_manager> (
                { "Physics Manager", true });
        
        if (initial_project) {
            // Load project locally
            m_local_executor = std::make_unique<command_executor>(*m_rtc);
            m_local_executor->execute("proj load " + *initial_project);
        }
    }
    
    std::string line;
    std::cout << "Weasel Engine REPL\nType 'help' for commands.\n";
    while (m_running && std::cout << "wsl> " && std::getline(std::cin, line)) {
        if (line.empty()) continue;
        execute_command(line);
    }
}

void repl_handler::execute_command(const std::string& line) {
    // If attached to editor server, forward command remotely
    if (m_attach && m_editor_client.is_connected()) {
        auto response = m_editor_client.execute_command(line);
        if (response) {
            std::cout << *response << "\n";
            // Check if exit was requested
            if (response->find("exit") != std::string::npos) {
                m_running = false;
            }
        } else {
            std::cout << "Error: Lost connection to editor server.\n";
            m_running = false;
        }
        return;
    }
    
    // Otherwise, execute locally
    if (m_local_executor) {
        std::string output = m_local_executor->execute(line);
        std::cout << output;
        
        if (output.find("exit") != std::string::npos) {
            m_running = false;
        }
    }
}

} // namespace wsl::cli
