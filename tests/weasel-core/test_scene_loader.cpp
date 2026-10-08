// Part of weasel_core_tests; the doctest main lives in test_event_bus.cpp.
//
// The editor and the resource manager load scenes through `rsc::scene_loader`,
// never through the serializer directly. Scene files are single JSON documents
// that carry the `.wscn` extension (see `rsc::scene_file::extension`), so the
// loader has to pick the JSON reader for them.
//
// Before this was fixed the loader only recognised `.json` / `.scene` /
// `.prefab` as JSON and read everything else -- including every `.wscn` --
// with the msgpack reader. Parsing `{"format_version"...` as msgpack fails
// silently, so the load "succeeded" and produced a scene with no name and no
// entities: a freshly created project opened with no sample cube, no sunlight
// and no default camera.

#include "doctest.h"

#include "wsl/comp/components.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/reg/component_registry.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/scene_loader.hpp"
#include "wsl/rsc/scene_snapshot_serializer.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{

void
register_world_components (wsl::comp::singl::runtime_context &rtc)
{
  using namespace wsl::comp;

  register_component_meta<
      hierarchy, world_transform, transform, model_instance_3d, camera,
      camera_2d, point_light, spot_light, directional_light, rigid_body, area,
      character_body, audio, prefab_instance, sprite_2d, subviewport,
      transform_2d, animator, skeleton_pose> ();

  for_each_type<component_types>::apply ([&rtc]<typename T> () {
    rtc.component_registry ().register_world_component<T> ();
  });
}

/** Builds the trio a brand new project ships with: cube, camera, sunlight. */
void
populate (wsl::rsc::scene &scene)
{
  entt::registry &registry = scene.get_registry ();

  const entt::entity cube = registry.create ();
  scene.set_entity_name (cube, "Sample Cube");
  registry.emplace<wsl::comp::hierarchy> (cube);
  registry.emplace<wsl::comp::transform> (cube, glm::vec3 (0.0F));
  registry.emplace<wsl::comp::world_transform> (cube);
  registry.emplace<wsl::comp::model_instance_3d> (cube);

  const entt::entity camera = registry.create ();
  scene.set_entity_name (camera, "Scene Default Camera");
  registry.emplace<wsl::comp::hierarchy> (camera);
  registry.emplace<wsl::comp::transform> (camera, glm::vec3 (0.0F, 0.0F, 5.0F));
  registry.emplace<wsl::comp::world_transform> (camera);
  registry.emplace<wsl::comp::camera> (camera);

  const entt::entity sun = registry.create ();
  scene.set_entity_name (sun, "Sunlight");
  registry.emplace<wsl::comp::hierarchy> (sun);
  registry.emplace<wsl::comp::transform> (sun, glm::vec3 (0.0F, 5.0F, 0.0F));
  registry.emplace<wsl::comp::world_transform> (sun);
  registry.emplace<wsl::comp::directional_light> (sun);
}

std::string
read_file (const std::string &path)
{
  std::ifstream file (path);
  return std::string ((std::istreambuf_iterator<char> (file)),
                      std::istreambuf_iterator<char> ());
}

} // namespace

TEST_CASE ("scene_loader reads the JSON scene files projects ship as .wscn")
{
  wsl::comp::singl::runtime_context rtc ("SceneLoader", 0, 0, "", true);
  register_world_components (rtc);

  const std::filesystem::path path
      = std::filesystem::temp_directory_path ()
        / "weasel_scene_loader_json_test.wscn";

  {
    wsl::rsc::scene scene (&rtc, nullptr, "Main Scene");
    populate (scene);
    wsl::rsc::io::scene_snapshot_serializer saver (&rtc, scene);
    REQUIRE (saver.save_json (path.string ()));
  }

  // A project-generated scene really is JSON with a .wscn extension.
  CHECK (read_file (path.string ()).starts_with ('{'));

  wsl::rsc::scene_loader const loader{};
  std::shared_ptr<wsl::rsc::scene> const loaded
      = loader (&rtc, nullptr, path.string ());

  REQUIRE (loaded != nullptr);
  CHECK (loaded->get_name () == "Main Scene");

  entt::registry &registry = loaded->get_registry ();
  auto cubes = registry.view<wsl::comp::model_instance_3d> ();
  REQUIRE (std::distance (cubes.begin (), cubes.end ()) == 1);

  auto cameras = registry.view<wsl::comp::camera> ();
  REQUIRE (std::distance (cameras.begin (), cameras.end ()) == 1);

  auto lights = registry.view<wsl::comp::directional_light> ();
  REQUIRE (std::distance (lights.begin (), lights.end ()) == 1);

  auto transforms = registry.view<wsl::comp::transform> ();
  CHECK (std::distance (transforms.begin (), transforms.end ()) == 3);

  CHECK (loaded->get_entity_name (*cubes.begin ()) == "Sample Cube");
  CHECK (loaded->get_entity_name (*lights.begin ()) == "Sunlight");

  std::filesystem::remove (path);
}

TEST_CASE ("scene_loader still reads a msgpack scene saved under .wscn")
{
  // Scenes saved by the editor before the format was unified are msgpack
  // payloads behind the same extension. They must keep loading.
  wsl::comp::singl::runtime_context rtc ("SceneLoader", 0, 0, "", true);
  register_world_components (rtc);

  const std::filesystem::path path
      = std::filesystem::temp_directory_path ()
        / "weasel_scene_loader_binary_test.wscn";

  {
    wsl::rsc::scene scene (&rtc, nullptr, "Legacy Scene");
    populate (scene);
    wsl::rsc::io::scene_snapshot_serializer saver (&rtc, scene);
    REQUIRE (saver.save_binary (path.string ()));
  }

  CHECK_FALSE (read_file (path.string ()).starts_with ('{'));

  wsl::rsc::scene_loader const loader{};
  std::shared_ptr<wsl::rsc::scene> const loaded
      = loader (&rtc, nullptr, path.string ());

  REQUIRE (loaded != nullptr);
  CHECK (loaded->get_name () == "Legacy Scene");
  CHECK (std::distance (loaded->get_registry ()
                            .view<wsl::comp::directional_light> ()
                            .begin (),
                        loaded->get_registry ()
                            .view<wsl::comp::directional_light> ()
                            .end ())
         == 1);

  std::filesystem::remove (path);
}

TEST_CASE ("scene_loader saves .wscn scenes as JSON")
{
  wsl::comp::singl::runtime_context rtc ("SceneLoader", 0, 0, "", true);
  register_world_components (rtc);

  wsl::rsc::scene scene (&rtc, nullptr, "Saved Scene");
  populate (scene);

  const std::filesystem::path path
      = std::filesystem::temp_directory_path ()
        / "weasel_scene_loader_save_test.wscn";

  REQUIRE (wsl::rsc::scene_loader::save (&rtc, scene, path.string (), false));

  const std::string content = read_file (path.string ());
  CHECK (content.starts_with ('{'));

  // And the JSON the editor wrote is what the CLI's `scene load` reads.
  wsl::rsc::scene_loader const loader{};
  std::shared_ptr<wsl::rsc::scene> const loaded
      = loader (&rtc, nullptr, path.string ());
  REQUIRE (loaded != nullptr);
  CHECK (loaded->get_name () == "Saved Scene");

  std::filesystem::remove (path);
}
