// cube.frag.slang
// Standard PBR material shader. Lighting/IBL/shadow/SSAO logic lives in
// pbr_common.slang so it is shared verbatim with the shader-graph generated
// shaders. This file only owns the material-specific inputs (base color /
// metallic-roughness / normal / emissive textures) and feeds them into
// EvaluateScenePBR().
//
// Texture bindings (set 2):
// t0  = base color
// t1  = metallic-roughness
// t2  = normal
// t3  = emissive
// t4..t16 are owned by pbr_common.slang (IBL, shadows, SSAO, cluster buffers).

#include "pbr_common.slang"

struct PSInput
{
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
    float4 tangent   : TEXCOORD3; // xyz=tangent, w=sign
    float3 viewPos   : TEXCOORD4; // view-space position for cluster lookup
};

cbuffer Material : register(b0, space3)
{
    float4 u_BaseColorFactor;
    float  u_MetallicFactor;
    float  u_RoughnessFactor;
    float  u_AlphaCutoff;
    float  u_AlphaMode; // 0=opaque, 1=mask, 2=blend

    float3 u_EmissiveFactor;
    float  u_MipLodBias;
};

Texture2D u_BaseColorTex           : register(t0,  space2);
Texture2D u_MetallicRoughnessTex   : register(t1,  space2);
Texture2D u_NormalTex              : register(t2,  space2);
Texture2D u_EmissiveTex            : register(t3,  space2);

// Normal mapping (references this material's normal texture).
float3 ApplyNormalMap(float3 N, float4 tangent, float2 uv)
{
    float3 T = normalize(tangent.xyz);
    T = normalize(T - N * dot(N, T));

    float3 B = normalize(cross(N, T) * tangent.w);

    float3 nTS = UnpackNormal(u_NormalTex.SampleBias(u_Samplers[2], uv, u_MipLodBias).xyz);

    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(nTS, TBN));
}

[shader("fragment")]
PSOut fsMain(PSInput input)
{
    float3 Ng = normalize(input.normal);
    float3 N  = ApplyNormalMap(Ng, input.tangent, input.uv);
    float3 V  = normalize(u_CameraPos.xyz - input.worldPos);

    float4 baseColor =
        u_BaseColorTex.SampleBias(u_Samplers[0], input.uv, u_MipLodBias) *
        u_BaseColorFactor;

    // alphaMode: 0=opaque, 1=mask, 2=blend
    if (u_AlphaMode > 0.5 && u_AlphaMode < 1.5) // MASK
    {
        if (baseColor.a < u_AlphaCutoff)
            discard;
    }
    else if (u_AlphaMode > 1.5) // BLEND
    {
        if (baseColor.a < 0.001)
            discard;
    }

    float3 albedo = baseColor.rgb;

    float2 mr = u_MetallicRoughnessTex.SampleBias(u_Samplers[1], input.uv, u_MipLodBias).gb;
    float roughness = clamp(mr.x * u_RoughnessFactor, 0.04, 1.0);
    float metallic  = mr.y * u_MetallicFactor;

    float3 emissive =
        u_EmissiveTex.SampleBias(u_Samplers[3], input.uv, u_MipLodBias).rgb *
        u_EmissiveFactor;

    SceneResult r = EvaluateScenePBR(
        albedo, metallic, roughness, emissive,
        N, Ng, V, input.worldPos, input.uv, input.pos, input.viewPos);

    PSOut o;
    o.color = float4(r.color, baseColor.a);
    o.bloom = float4(r.bloom, 1.0);
    return o;
}
