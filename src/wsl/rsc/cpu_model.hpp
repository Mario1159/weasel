#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>


namespace wsl
{

namespace rsc
{

namespace raw
{

/** Represents a UV texture coordinate transformation. */
struct uv_xform
{
  /** Offset of the UV coordinates. */
  glm::vec2 offset = { 0, 0 };
  /** Scale of the UV coordinates. */
  glm::vec2 scale = { 1, 1 };
  /** Rotation of the UV coordinates in radians. */
  float rotation = 0.0F;
  /** Whether the transformation is valid. */
  bool valid = false;
};

/** Represents a texture stored in system memory. */
struct cpu_texture
{
  /** Width of the texture in pixels. */
  int width = 0;
  /** Height of the texture in pixels. */
  int height = 0;
  /** Raw pixel data in RGBA8 format. */
  std::vector<uint8_t> pixels; // RGBA8
};

/** Represents a texture slot, combining a texture with an optional transform. */
struct cpu_tex_slot
{
  /** Shared pointer to the CPU texture. */
  std::shared_ptr<cpu_texture> tex;
  /** Per-slot UV transformation. */
  uv_xform uv; // per-slot transform
};

/** Represents a material stored in system memory. */
struct cpu_material
{
  /** Base color (RGBA) of the material. */
  glm::vec4 base_color_factor = glm::vec4 (1.0F);
  /** Metallic factor of the material. */
  float metallic_factor = 0.0F;
  /** Roughness factor of the material. */
  float roughness_factor = 1.0F;

  /** Emissive color (RGB) of the material. */
  glm::vec3 emissive_factor = glm::vec3 (0.0F);
  /** Strength of the emissive color. */
  float emissive_strength = 1.0F;

  /** Base color texture slot. */
  cpu_tex_slot base_color;
  /** Metallic-roughness texture slot. */
  cpu_tex_slot metallic_roughness;
  /** Normal map texture slot. */
  cpu_tex_slot normal;
  /** Emissive map texture slot. */
  cpu_tex_slot emissive;

  /** Whether the material is double-sided. */
  bool double_sided = false;
};

/** Represents a single vertex in a CPU mesh. */
struct cpu_vertex
{
  /** Position of the vertex. */
  glm::vec3 pos;
  /** Normal vector of the vertex. */
  glm::vec3 normal;
  /** UV coordinates of the vertex. */
  glm::vec2 uv;
  /** Tangent vector and sign. */
  glm::vec4 tangent; // xyz = tangent, w = sign
};

/** Represents a geometric primitive within a mesh. */
struct cpu_primitive
{
  /** List of vertices. */
  std::vector<cpu_vertex> vertices;
  /** List of indices for indexed rendering. */
  std::vector<uint32_t> indices;
  /** Material applied to this primitive. */
  cpu_material material;
};

/** Represents a mesh composed of multiple primitives. */
struct cpu_mesh
{
  /** List of primitives that make up this mesh. */
  std::vector<cpu_primitive> primitives;
};

/** Represents a node in a scene hierarchy. */
struct cpu_node
{
  /** Local transformation matrix. */
  glm::mat4 local_transform;
  /** Indices of meshes (LODs) attached to this node. */
  std::vector<int> mesh_lods;
  /** Child nodes in the hierarchy. */
  std::vector<cpu_node> children;
};

/** Represents a scene containing a hierarchy of nodes. */
struct cpu_scene
{
  /** Root nodes of the scene hierarchy. */
  std::vector<cpu_node> roots;
};

/** Represents a complete 3D model with meshes and scenes. */
struct cpu_model
{
  /** List of meshes in the model. */
  std::vector<cpu_mesh> meshes;
  /** List of scenes in the model. */
  std::vector<cpu_scene> scenes;

  /** Groups of meshes for Level of Detail (LOD) management. */
  std::vector<std::vector<int>> lod_groups;

  /** Index of the default scene. */
  int default_scene = -1;
};

} // namespace raw

} // namespace rsc

} // namespace wsl
