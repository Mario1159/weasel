// skinning.slang — 4-influence linear blend skinning.
//
// Include-only module (never compiled standalone), mirroring pbr_common.slang.
//
// Every vertex shader that draws meshes includes this and calls
// skin_position / skin_normal / skin_tangent. Each is compiled twice: once
// plain, where the macros expand to the identity and the shader pays nothing,
// and once with -DSKINNING, where the palette cbuffer exists and the blend
// actually runs. The renderer binds the skinned pipeline only for draws that
// carry an animation palette, so the plain build must stay free of any palette
// binding.
//
// Joint data comes from the glTF attributes added in the skin-import work
// (TEXCOORD4 = uint4 joint indices, TEXCOORD5 = float4 weights).
//
// Note: Slang flattens cbuffer members into the enclosing scope, so the
// members below are referenced unqualified (u_JointPalette, not
// JointPalette.u_JointPalette); the cbuffer's own name is not a binding.

#ifndef WEASEL_MAX_JOINTS
#define WEASEL_MAX_JOINTS 128
#endif

// Vertex uniform slot the palette binds to. Slot 0 is always the per-draw
// Matrices cbuffer; the outline pass already uses slot 1 for its own params
// and therefore overrides this to 2 before including this file.
#ifndef WEASEL_PALETTE_BINDING
#define WEASEL_PALETTE_BINDING 1
#endif

#ifdef SKINNING

// Slang will not macro-expand a register class inside `register(bN, ...)`,
// so the slot is chosen with a conditional rather than a token paste.
// space1 -> SDL vertex uniform set, matching the -fvk-b-shift 0 1 remap the
// build applies to vertex shaders.
#if WEASEL_PALETTE_BINDING == 1
cbuffer JointPalette : register(b1, space1)
#else
cbuffer JointPalette : register(b2, space1)
#endif
{
    // u_JointCount comes FIRST on purpose.
    //
    // It used to follow the array, which put it at offset 8192 -- the very end
    // of the block. A RenderDoc capture of a live skinned draw showed all 24
    // palette matrices arriving intact while u_JointCount read back as 0 from
    // that exact offset, even though the CPU packed 24 there and uploaded all
    // 8196 bytes in one call (the SDK's push path memcpy's the full length
    // unclamped into a 32 KB pool buffer, so nothing is truncated on write).
    //
    // The scalar is the one value the whole blend gates on: if it reads 0, the
    // `index < u_JointCount` test rejects every influence, skin_position
    // returns 0 for every vertex, and the mesh silently collapses to the origin
    // with no API error. Leading with it keeps it inside the head of the block,
    // ahead of whatever tail the descriptor range ends up covering.
    uint u_JointCount;
    // std140 aligns the array to 16 bytes, so it starts at offset 16.
    column_major float4x4 u_JointPalette[WEASEL_MAX_JOINTS];
};

// Applies the 4-influence blend for one attribute.
//
// Weights are normalized by the importer, so this is a plain unrolled sum.
// The bounds test keeps a stale or short palette from indexing past the
// array and reading garbage; it is uniform across the draw, so it is free.
#define WEASEL_SKIN_ACCUMULATE(acc, vec_expr)                                   \
    [unroll] for (int i = 0; i < 4; ++i)                                      \
    {                                                                          \
        uint index = joints[i];                                                \
        if (index < u_JointCount)                                              \
        {                                                                      \
            acc += (mul(u_JointPalette[index], vec_expr).xyz) * weights[i];     \
        }                                                                      \
    }

float3
skin_position (float3 position, uint4 joints, float4 weights)
{
    float3 skinned = 0.0;
    WEASEL_SKIN_ACCUMULATE (skinned, float4 (position, 1.0))
    return skinned;
}

float3
skin_normal (float3 normal, uint4 joints, float4 weights)
{
    float3 skinned = 0.0;
    // w = 0 so joint translation does not shift the direction.
    WEASEL_SKIN_ACCUMULATE (skinned, float4 (normal, 0.0))
    return skinned;
}

float3
skin_tangent (float3 tangent, uint4 joints, float4 weights)
{
    float3 skinned = 0.0;
    WEASEL_SKIN_ACCUMULATE (skinned, float4 (tangent, 0.0))
    return skinned;
}

#else // !SKINNING

// Identity fallbacks: the plain build has no palette cbuffer at all, so its
// pipeline layout does not need the binding and the vertex cost is zero.
#define skin_position(position, joints, weights) (position)
#define skin_normal(normal, joints, weights) (normal)
#define skin_tangent(tangent, joints, weights) (tangent)

#endif // SKINNING
