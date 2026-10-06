// Part of weasel_core_tests; the doctest main lives in test_event_bus.cpp.
//
// Scene files stream one document per registered world component, back to
// back, with no per-document type tag in the original (v1) format. A loader
// that walks the *current* registry therefore desynchronizes the moment a
// component is added: every document after the new component is consumed as
// the wrong type. Because rfl fills absent fields with defaults, that failure
// is silent -- entities keep only the components that happened to land before
// the insertion point and silently lose the rest.
//
// These tests pin the contract that keeps scene files loadable:
//   * legacy (untagged) files written before a component existed still load
//     every component they contain, and
//   * tagged files carry their own manifest, so they round-trip regardless of
//     which components the current build registers.

#include "doctest.h"

#include "wsl/comp/components.hpp"
#include "wsl/comp/singl/runtime_context.hpp"
#include "wsl/reg/component_registry.hpp"
#include "wsl/reg/singleton_registry.hpp"
#include "wsl/rsc/scene.hpp"
#include "wsl/rsc/scene_snapshot_serializer.hpp"
#include "wsl/serialize/serialize.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <entt/entity/fwd.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{

/** Mirrors the frozen v1 component set the untagged format was written with. */
constexpr std::array<std::string_view, 17> v1_world_components = {
  "hierarchy",
  "world_transform",
  "transform",
  "model_instance_3d",
  "camera",
  "camera_2d",
  "point_light",
  "spot_light",
  "directional_light",
  "rigid_body",
  "area",
  "character_body",
  "audio",
  "prefab_instance",
  "sprite_2d",
  "subviewport",
  "transform_2d",
};

std::string_view
short_type_name (std::string_view type_name)
{
  std::size_t const separator = type_name.rfind ("::");
  std::string_view name = separator == std::string_view::npos
                              ? type_name
                              : type_name.substr (separator + 2);
  while (!name.empty ()
         && (name.back () == ']' || name.back () == ' ' || name.back () == '\n'
             || name.back () == '\r')) {
    name.remove_suffix (1);
  }
  return name;
}

bool
is_v1_component (std::string_view type_name)
{
  std::string_view const name = short_type_name (type_name);
  return std::find (v1_world_components.begin (), v1_world_components.end (),
                    name)
         != v1_world_components.end ();
}

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

/** Fills the scene with data spread across the component block. */
entt::entity
populate (wsl::rsc::scene &scene)
{
  entt::registry &registry = scene.get_registry ();
  entt::entity entity = registry.create ();
  scene.set_entity_name (entity, "Subject");

  registry.emplace<wsl::comp::hierarchy> (entity);
  registry.emplace<wsl::comp::world_transform> (entity);

  wsl::comp::transform transform;
  transform.position = { 1.0F, 2.0F, 3.0F };
  registry.emplace<wsl::comp::transform> (entity, transform);

  wsl::comp::model_instance_3d model_instance;
  model_instance.scene_index = 2;
  registry.emplace<wsl::comp::model_instance_3d> (entity, model_instance);

  wsl::comp::camera camera;
  camera.fov () = 0.5F;
  registry.emplace<wsl::comp::camera> (entity, camera);

  return entity;
}

/**
 * Writes a scene stream that looks like one produced by a build whose
 * registry only contained the v1 component set: the documents that a current
 * build would emit for components outside that set are never written.
 *
 * \p tagged also stamps the component manifest, matching current output.
 */
std::string
build_stream (wsl::comp::singl::runtime_context &rtc, wsl::rsc::scene &scene,
              entt::entity entity, bool tagged)
{
  struct scene_data
  {
    wsl::rsc::io::scene_header header;
    std::vector<entt::entity> entities;
  };

  entt::registry &registry = scene.get_registry ();
  wsl::reg::component_registry &components = rtc.component_registry ();

  scene_data data;
  data.header.scene_name = "Stream Test";
  data.header.entity_names.emplace_back (
      static_cast<uint32_t> (entt::to_integral (entity)), "Subject");
  data.entities.push_back (entity);

  wsl::serialize::json_writer writer;

  const std::vector<const wsl::reg::component_registry::descriptor *> all
      = components.get_world_components (
          wsl::reg::world_component_order::type_id);

  writer.write (data);

  if (tagged) {
    // Mirrors the manifest document the current writer emits.
    struct component_manifest
    {
      uint32_t version = 1;
      std::vector<std::string> component_order;
    };
    component_manifest manifest;
    for (const wsl::reg::component_registry::descriptor *desc : all) {
      if (desc != nullptr && is_v1_component (desc->type_name)) {
        manifest.component_order.push_back (desc->type_name);
      }
    }
    writer.write (manifest);
  }

  for (const wsl::reg::component_registry::descriptor *desc : all) {
    if (desc == nullptr || !is_v1_component (desc->type_name)) {
      continue;
    }
    components.save_world_component_json (writer, registry, desc->type_id);
  }

  components.save_das_components_json (writer, registry);

  for (const wsl::reg::singleton_registry::descriptor *desc :
       rtc.singleton_registry ().get_singleton_components (
           wsl::reg::singleton_component_order::type_id)) {
    if (desc == nullptr || !desc->serialize_with_scene) {
      continue;
    }
    const bool has = desc->contains ? desc->contains (registry) : false;
    if (has) {
      rtc.singleton_registry ().save_singleton_json (writer, registry,
                                                     desc->type_id);
    }
  }

  return writer.json;
}

std::filesystem::path
write_temp (const std::string &contents)
{
  std::filesystem::path const path
      = std::filesystem::temp_directory_path ()
        / "weasel_scene_component_stream_test.wscn.json";
  std::ofstream file (path);
  file << contents;
  file.close ();
  return path;
}

/** Asserts the entity came back with every component the stream described. */
void
check_loaded (wsl::rsc::scene &scene, entt::entity entity)
{
  entt::registry &registry = scene.get_registry ();

  REQUIRE (registry.valid (entity));
  CHECK (scene.get_entity_name (entity) == "Subject");
  CHECK (registry.all_of<wsl::comp::transform> (entity));
  CHECK (registry.all_of<wsl::comp::model_instance_3d> (entity));
  CHECK (registry.all_of<wsl::comp::camera> (entity));
  CHECK (registry.all_of<wsl::comp::hierarchy> (entity));

  const wsl::comp::transform &transform
      = registry.get<wsl::comp::transform> (entity);
  CHECK (transform.position.x () == doctest::Approx (1.0F));
  CHECK (transform.position.z () == doctest::Approx (3.0F));

  // Values that sit in different slots of the component block prove the
  // documents were consumed by the right component types.
  CHECK (registry.get<wsl::comp::model_instance_3d> (entity).scene_index == 2);
  CHECK (registry.get<wsl::comp::camera> (entity).fov ()
         == doctest::Approx (0.5F));
}

} // namespace

TEST_CASE ("scene headers stay readable when fields are added later")
{
  // Regression: rfl treats plain struct fields as *required*, so adding a
  // field to scene_header made every previously saved scene fail to parse,
  // silently producing scenes with no entities at all. The component
  // manifest is therefore a separate document, and the header contract is
  // frozen. This is the exact header shape written by builds from before the
  // manifest existed.
  std::string const legacy_header
      = R"({"header":{"scene_name":"Main Scene","is_prefab":false,"systems":[],"entity_names":[[0,"Cube"]],"connections":[],"autoload":[],"camera":0},"entities":[{"id":0}]})";

  struct scene_data
  {
    wsl::rsc::io::scene_header header;
    std::vector<entt::entity> entities;
  };

  scene_data data;
  std::string error;
  REQUIRE (wsl::serialize::json_read (legacy_header, data, &error));
  CHECK (error.empty ());
  CHECK (data.header.scene_name == "Main Scene");
  REQUIRE (data.header.entity_names.size () == 1);
  CHECK (data.header.entity_names[0].second == "Cube");
  REQUIRE (data.entities.size () == 1);
}

TEST_CASE (
    "scene load tolerates components registered after the file was saved")
{
  wsl::comp::singl::runtime_context rtc ("SceneStream", 0, 0, "", true);
  register_world_components (rtc);

  wsl::rsc::scene scene (&rtc, nullptr, "Stream Test");
  entt::entity const entity = populate (scene);

  // A file whose component block predates `animator` (untagged, no manifest).
  std::string const legacy = build_stream (rtc, scene, entity, false);
  REQUIRE_FALSE (legacy.empty ());

  std::filesystem::path const path = write_temp (legacy);
  wsl::rsc::io::scene_snapshot_serializer loader (&rtc, scene);
  REQUIRE (loader.load_json (path.string ()));
  std::filesystem::remove (path);

  check_loaded (scene, entity);

  // The component that the file never described must not have been created,
  // and must not have consumed a document either.
  CHECK (scene.get_registry ().all_of<wsl::comp::animator> (entity) == false);
}

TEST_CASE ("tagged scene streams round-trip without shifting documents")
{
  wsl::comp::singl::runtime_context rtc ("SceneStream", 0, 0, "", true);
  register_world_components (rtc);

  wsl::rsc::scene scene (&rtc, nullptr, "Stream Test");
  entt::entity const entity = populate (scene);

  // A manifest-tagged file that also predates `animator`: the manifest tells
  // the loader exactly which documents to expect.
  std::string const tagged = build_stream (rtc, scene, entity, true);
  REQUIRE_FALSE (tagged.empty ());

  std::filesystem::path const path = write_temp (tagged);
  wsl::rsc::io::scene_snapshot_serializer loader (&rtc, scene);
  REQUIRE (loader.load_json (path.string ()));
  std::filesystem::remove (path);

  check_loaded (scene, entity);
  CHECK (scene.get_registry ().all_of<wsl::comp::animator> (entity) == false);
}

TEST_CASE ("saved scenes record a manifest and reload every component")
{
  wsl::comp::singl::runtime_context rtc ("SceneStream", 0, 0, "", true);
  register_world_components (rtc);

  wsl::rsc::scene scene (&rtc, nullptr, "Stream Test");
  entt::entity const entity = populate (scene);
  scene.get_registry ().emplace<wsl::comp::animator> (entity);

  std::filesystem::path const path
      = std::filesystem::temp_directory_path ()
        / "weasel_scene_manifest_roundtrip.wscn.json";

  {
    wsl::rsc::io::scene_snapshot_serializer saver (&rtc, scene);
    REQUIRE (saver.save_json (path.string ()));
  }

  wsl::rsc::io::scene_snapshot_serializer loader (&rtc, scene);
  REQUIRE (loader.load_json (path.string ()));
  std::filesystem::remove (path);

  check_loaded (scene, entity);
  CHECK (scene.get_registry ().all_of<wsl::comp::animator> (entity));
}
