// pbr_common.slang
//
// Shared physically-based shading for the engine. Both the standard material
// shader (cube.frag.slang) and the shader-graph generated fragment shaders
// include this module so they can never diverge from the engine's PBR.
//
// This module owns:
//   - the Lighting / IBL / Post / ClusterParams constant buffers (b1..b4),
//   - the IBL / shadow / SSAO textures (t4..t14), the clustered lighting
//     storage buffers (t15, t16) and the shared u_Samplers[15] array,
//   - every PBR helper function,
//   - EvaluateScenePBR(), which performs the full lit + IBL + SSAO + bloom
//     evaluation given already-resolved material parameters.
//
// It deliberately does NOT declare:
//   - the `Material` cbuffer (b0): the shader-graph codegen emits it per-graph
//     (it carries the graph's scalar/vector uniforms, including u_CameraPos in
//     the standard layout), or cube.frag declares it explicitly,
//   - the base-color / metallic-roughness / normal / emissive textures (t0..t3):
//     owned by the including shader,
//   - PSInput / the entry point: each shader declares its own varyings.

#define MAX_POINT_LIGHTS 16
#define MAX_DIR_LIGHTS   4
#define MAX_SPOT_LIGHTS  8

#define MAX_LIGHTS_PER_CLUSTER 100

#define MAX_SHADOWED_DIR_LIGHTS   1
#define MAX_SHADOWED_SPOT_LIGHTS  4
#define MAX_SHADOWED_POINT_LIGHTS 2

static const float PI = 3.14159265359;

struct PointLight
{
    float4 pos_radius;      // xyz=pos (world), w=radius
    float4 color_intensity; // rgb=color, w=intensity
    int4   shadow_info;     // x=shadow index, y=casts_shadows, z/w unused
};

struct DirLight
{
    float4 dir_pad;         // xyz=dir
    float4 color_intensity; // rgb=color, w=intensity
};

struct SpotLight
{
    float4 pos_pad;         // xyz=pos
    float4 dir_pad;         // xyz=dir
    float4 color_intensity; // rgb=color, w=intensity
    float4 angles_pad;      // x=inner_cos, y=outer_cos
};

struct ShadowData
{
    column_major float4x4 light_view_proj;
    float4 params; // x=bias, y=strength, z=map_size, w=enabled
};

struct ShadowedPoint
{
    float4 pos_far; // xyz=light pos, w=far plane
    float4 params;  // x=bias, y=strength, z=enabled, w=unused
};

cbuffer Lighting : register(b1, space3)
{
    int4 u_Counts; // x=num_dir, y=num_spot, z/w unused (point lights come from clusters)

    DirLight   u_DirLights[MAX_DIR_LIGHTS];
    SpotLight  u_SpotLights[MAX_SPOT_LIGHTS];

    float4 u_CameraPos; // xyz
    float4 u_Ambient;   // xyz

    ShadowData    u_DirShadow[MAX_SHADOWED_DIR_LIGHTS];
    ShadowData    u_SpotShadow[MAX_SHADOWED_SPOT_LIGHTS];
    ShadowedPoint u_PointShadow[MAX_SHADOWED_POINT_LIGHTS];
};

cbuffer IBL : register(b2, space3)
{
    float u_IBLIntensity;
    float u_PrefilterMaxMip;
    float2 pad_ibl0;
};

cbuffer Post : register(b3, space3)
{
    float4 u_BloomAndSSAO0; // x=bloomThreshold, y=bloomKnee, z=bloomIntensity, w=ssaoEnabled
    float4 u_SSAOAndPad;    // x=ssaoIntensity, yzw=pad
};

cbuffer ClusterParams : register(b4, space3)
{
    float4x4 u_InverseProjection;
    float2   u_ScreenSize;
    float    u_ZNear;
    float    u_ZFar;
    uint3    u_GridSize;
    uint     u_NumClusters;
    uint3    u_ClusterPadding;
    float4x4 u_View;
    uint     u_NumPointLights;
    uint3    u_LightPadding;
};

struct GpuCluster
{
    float4 minPoint;
    float4 maxPoint;
    uint   count;
    uint   lightIndices[MAX_LIGHTS_PER_CLUSTER];
};

// IBL + shadow + SSAO textures live in the t4..t14 range so they never collide
// with the material's own t0..t3 (base color / metallic-roughness / normal /
// emissive) or with shader-graph texture-sample nodes (which use t0..t3).
TextureCube u_IrradianceMap        : register(t4,  space2);
TextureCube u_PrefilterMap         : register(t5,  space2);
Texture2D   u_BRDFLUT              : register(t6,  space2);

Texture2D<float>     u_DirShadowMap   : register(t7,  space2);
Texture2D<float>     u_SpotShadowMap0 : register(t8,  space2);
Texture2D<float>     u_SpotShadowMap1 : register(t9,  space2);
Texture2D<float>     u_SpotShadowMap2 : register(t10, space2);
Texture2D<float>     u_SpotShadowMap3 : register(t11, space2);
TextureCube<float>   u_PointShadowMap0 : register(t12, space2);
TextureCube<float>   u_PointShadowMap1 : register(t13, space2);
Texture2D            u_SSAOTex        : register(t14, space2);

// RO point-light buffer: SPIR-V binding 15. The -fvk-t-shift 0 2 flag places
// this at descriptor set 2, binding 15. The same GPU buffer is bound from the
// compute stage at descriptor set 0, binding 15 (read-only storage).
// SDL_GPU's first_slot matches the SPIR-V binding number, so both stages use
// slot 15.
StructuredBuffer<PointLight> u_PointLightBuffer : register(t15, space2);
// Cluster buffer: SPIR-V set 2, binding 16 in the fragment pipeline. Must be
// `StructuredBuffer` (not `RWStructuredBuffer`) so Slang emits the `NonWritable`
// decoration — Vulkan requires fragment-stage storage buffers to be read-only.
// Register `t16` (rather than `u0`) is used because the `-fvk-t-shift` and
// `-fvk-u-shift` flags both map into the same SPIR-V descriptor set, so `u0`
// collides with the `t0` texture and produces two resources at set 2 binding 0.
StructuredBuffer<GpuCluster> u_ClusterBuffer : register(t16, space2);

// Shared sampler array. The shader-graph codegen must NOT redeclare this.
SamplerState u_Samplers[15] : register(s0, space2);

//////////////////////////////////////
// Helpers
//////////////////////////////////////

float SampleShadowMap(
    Texture2D<float> shadowMap,
    SamplerState shadowSampler,
    float4x4 lightViewProj,
    float4 shadowParams,
    float3 worldPos,
    float3 geomN,
    float3 L)
{
    float shadowBias = shadowParams.x;
    float strength   = shadowParams.y;
    float mapSize    = max(shadowParams.z, 1.0);
    float enabled    = shadowParams.w;

    if (enabled < 0.5 || strength <= 0.0)
        return 1.0;

    float3 N = normalize(geomN);
    float3 lightDir = normalize(L);

    float4 lp = mul(lightViewProj, float4(worldPos, 1.0));
    if (lp.w <= 1e-6)
        return 1.0;

    float3 ndc = lp.xyz / lp.w;

    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    float current = ndc.z;

    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    if (current < 0.0 || current > 1.0)
        return 1.0;

    float ndotl = saturate(dot(N, lightDir));
    float bias = max(shadowBias * (1.0 - ndotl), shadowBias * 0.25);

    float2 texel = float2(1.0 / mapSize, 1.0 / mapSize);

    float visibility = 0.0;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 suv = uv + float2(x, y) * texel;
            float depthSample = shadowMap.Sample(shadowSampler, suv);
            visibility += ((current - bias) <= depthSample) ? 1.0 : 0.0;
        }
    }

    visibility /= 9.0;

    return lerp(1.0, visibility, saturate(strength));
}

float SamplePointShadow(
    TextureCube<float> shadowMap,
    SamplerState shadowSampler,
    float3 lightPos,
    float farPlane,
    float4 params,
    float3 worldPos)
{
    float bias     = params.x;
    float strength = params.y;
    float enabled  = params.z;

    if (enabled < 0.5 || strength <= 0.0)
        return 1.0;

    float3 fragToLight = worldPos - lightPos;
    float currentDist = length(fragToLight);

    if (currentDist <= 1e-6)
        return 1.0;

    float3 dir = fragToLight / currentDist;

    float3 up = (abs(dir.y) < 0.99) ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 right = normalize(cross(up, dir));
    float3 forward = normalize(cross(dir, right));

    float diskRadius = 0.02 + 0.03 * saturate(currentDist / farPlane);

    float visibility = 0.0;

    static const float2 poissonDisk[20] =
    {
        float2( 0.000,  0.000),
        float2( 0.538,  0.185),
        float2(-0.538,  0.185),
        float2( 0.538, -0.185),
        float2(-0.538, -0.185),

        float2( 0.185,  0.538),
        float2(-0.185,  0.538),
        float2( 0.185, -0.538),
        float2(-0.185, -0.538),

        float2( 0.707,  0.000),
        float2(-0.707,  0.000),
        float2( 0.000,  0.707),
        float2( 0.000, -0.707),

        float2( 0.353,  0.353),
        float2(-0.353,  0.353),
        float2( 0.353, -0.353),
        float2(-0.353, -0.353),

        float2( 0.900,  0.250),
        float2(-0.900,  0.250),
        float2( 0.000, -0.900)
    };

    [unroll]
    for (int i = 0; i < 20; ++i)
    {
        float2 o = poissonDisk[i] * diskRadius;
        float3 sampleDir = normalize(dir + right * o.x + forward * o.y);

        float stored = shadowMap.Sample(shadowSampler, sampleDir);
        float closestDist = stored * farPlane;

        visibility += ((currentDist - bias) <= closestDist) ? 1.0 : 0.0;
    }

    visibility /= 20.0;

    return lerp(1.0, visibility, saturate(strength));
}

float3 UnpackNormal(float3 n)
{
    return normalize(n * 2.0 - 1.0);
}

//////////////////////////////////////
// PBR helper functions
//////////////////////////////////////

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float ggx1 = GeometrySchlickGGX(max(dot(N, V), 0.0), roughness);
    float ggx2 = GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 F90 = float3(1.0, 1.0, 1.0);
    return F0 + (max(F90 - roughness.xxx, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

//////////////////////////////////////
// Light evaluator
//////////////////////////////////////

float3 EvaluateLight(
    float3 radiance,
    float3 L,
    float3 N,
    float3 V,
    float3 albedo,
    float metallic,
    float roughness)
{
    float3 H = normalize(V + L);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denom = max(4.0 *
        max(dot(N, V), 0.0) *
        max(dot(N, L), 0.0), 0.001);

    float3 specular = numerator / denom;

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

//////////////////////////////////////
// IBL
//////////////////////////////////////

float3 EvaluateIBL(
    float3 N,
    float3 V,
    float3 albedo,
    float metallic,
    float roughness,
    float iblIntensity)
{
    float NdotV = max(dot(N, V), 0.0);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F  = FresnelSchlickRoughness(NdotV, F0, roughness);

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - metallic);

    float3 irradiance = u_IrradianceMap.Sample(u_Samplers[4], N).rgb;
    float3 diffuse = irradiance * albedo;

    float3 R = reflect(-V, N);

    float mip = roughness * u_PrefilterMaxMip;
    float3 prefiltered = u_PrefilterMap.SampleLevel(u_Samplers[5], R, mip).rgb;

    float2 brdf = u_BRDFLUT.Sample(u_Samplers[6], float2(NdotV, roughness)).rg;
    float3 specular = prefiltered * (F0 * brdf.x + brdf.y);

    return (kD * diffuse + specular) * iblIntensity;
}

//////////////////////////////////////
// Bloom
//////////////////////////////////////

float Luminance(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 SoftThreshold(float3 c, float threshold, float knee)
{
    float l = Luminance(c);
    float soft = saturate((l - threshold + knee) / max(2.0 * knee, 1e-5));
    soft = soft * soft;
    float contrib = max(l - threshold, 0.0) + soft * knee;
    return c * (contrib / max(l, 1e-5));
}

//////////////////////////////////////
// Full scene evaluation
//////////////////////////////////////

struct SceneResult
{
    float3 color;
    float3 bloom;
};

struct PSOut
{
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

// Computes the fully lit result (ambient + IBL + direct lights + emissive,
// SSAO-modulated) for the given resolved material parameters. `screenPos` is
// the fragment's SV_Position (used to derive SSAO UVs); `viewPos` is the
// view-space position (used by the clustered point-light lookup).
SceneResult EvaluateScenePBR(
    float3 albedo, float metallic, float roughness, float3 emissive,
    float3 N, float3 Ng, float3 V, float3 worldPos,
    float2 uv, float4 screenPos, float3 viewPos)
{
    int u_NumDir  = u_Counts.x;
    int u_NumSpot = u_Counts.y;

    float u_BloomThreshold = u_BloomAndSSAO0.x;
    float u_BloomKnee      = u_BloomAndSSAO0.y;
    float u_BloomIntensity = u_BloomAndSSAO0.z;
    float u_SSAOEnabled    = u_BloomAndSSAO0.w;
    float u_SSAOIntensity  = u_SSAOAndPad.x;

    float3 Lo = 0.0;

    // -------- Directional lights --------
    for (int i = 0; i < u_NumDir; ++i)
    {
        float3 L = normalize(-u_DirLights[i].dir_pad.xyz);
        float3 radiance =
            u_DirLights[i].color_intensity.rgb *
            u_DirLights[i].color_intensity.w;

        if (i < MAX_SHADOWED_DIR_LIGHTS && u_DirShadow[i].params.w > 0.5)
        {
            float s = SampleShadowMap(
                u_DirShadowMap,
                u_Samplers[7],
                u_DirShadow[i].light_view_proj,
                u_DirShadow[i].params,
                worldPos,
                Ng,
                L);

            radiance *= s;
        }

        Lo += EvaluateLight(radiance, L, N, V, albedo, metallic, roughness);
    }

    // -------- Point lights (clustered) --------
    {
        float  viewZ       = abs(viewPos.z);
        float  zTileF      = (log(max(viewZ, 1e-4) / max(u_ZNear, 1e-4))
                              * (float)u_GridSize.z)
                             / log(max(u_ZFar / max(u_ZNear, 1e-4), 1.0001));
        uint   zTile       = (uint)clamp((int)zTileF, 0, (int)u_GridSize.z - 1);

        uint2  tileSize    = uint2(max((uint)(u_ScreenSize.x / (float)u_GridSize.x), 1u),
                                   max((uint)(u_ScreenSize.y / (float)u_GridSize.y), 1u));
        uint2  tileXY      = uint2(min((uint)(screenPos.x) / tileSize.x, u_GridSize.x - 1u),
                                   min((uint)(screenPos.y) / tileSize.y, u_GridSize.y - 1u));

        uint   tileIndex   = tileXY.x + (tileXY.y * u_GridSize.x)
                             + (zTile * u_GridSize.x * u_GridSize.y);

        if (tileIndex < u_NumClusters) {
            GpuCluster cluster = u_ClusterBuffer[tileIndex];
            uint lightCount = min(cluster.count, (uint)MAX_LIGHTS_PER_CLUSTER);

            [loop]
            for (uint i = 0; i < lightCount; ++i) {
                uint lightIndex = cluster.lightIndices[i];
                if (lightIndex >= (uint)u_NumPointLights) continue;

                PointLight light = u_PointLightBuffer[lightIndex];

                float3 lightPos = light.pos_radius.xyz;
                float  radius   = light.pos_radius.w;

                float3 toL  = lightPos - worldPos;
                float  dist = length(toL);
                if (dist > radius) continue;

                float3 L = toL / max(dist, 1e-6);
                float atten = saturate(1.0 - dist / radius);

                float3 radiance
                    = (light.color_intensity.rgb * light.color_intensity.w) * atten;

                int shadowIndex = light.shadow_info.x;
                int hasShadow   = light.shadow_info.y;

                if (hasShadow != 0 && shadowIndex >= 0
                    && shadowIndex < MAX_SHADOWED_POINT_LIGHTS) {
                    float s = 1.0;

                    if (shadowIndex == 0) {
                        s = SamplePointShadow(
                            u_PointShadowMap0,
                            u_Samplers[12],
                            u_PointShadow[0].pos_far.xyz,
                            u_PointShadow[0].pos_far.w,
                            u_PointShadow[0].params,
                            worldPos);
                    } else if (shadowIndex == 1) {
                        s = SamplePointShadow(
                            u_PointShadowMap1,
                            u_Samplers[13],
                            u_PointShadow[1].pos_far.xyz,
                            u_PointShadow[1].pos_far.w,
                            u_PointShadow[1].params,
                            worldPos);
                    }

                    radiance *= s;
                }

                Lo += EvaluateLight(radiance, L, N, V, albedo, metallic, roughness);
            }
        }
    }

    // -------- Spot lights --------
    for (int i = 0; i < u_NumSpot; ++i)
    {
        float3 lightPos = u_SpotLights[i].pos_pad.xyz;
        float3 lightDir = normalize(u_SpotLights[i].dir_pad.xyz);

        float3 toL = lightPos - worldPos;
        float dist = length(toL);
        float3 L = toL / max(dist, 1e-6);

        float theta = dot(L, normalize(-lightDir));

        float inner_cos = u_SpotLights[i].angles_pad.x;
        float outer_cos = u_SpotLights[i].angles_pad.y;

        float eps  = max(inner_cos - outer_cos, 0.0001);
        float cone = saturate((theta - outer_cos) / eps);

        float3 radiance =
            (u_SpotLights[i].color_intensity.rgb *
             u_SpotLights[i].color_intensity.w) * cone;

        if (i < MAX_SHADOWED_SPOT_LIGHTS && u_SpotShadow[i].params.w > 0.5)
        {
            float s = 1.0;

            if (i == 0)
            {
                s = SampleShadowMap(
                    u_SpotShadowMap0,
                    u_Samplers[8],
                    u_SpotShadow[0].light_view_proj,
                    u_SpotShadow[0].params,
                    worldPos,
                    Ng,
                    L);
            }
            else if (i == 1)
            {
                s = SampleShadowMap(
                    u_SpotShadowMap1,
                    u_Samplers[9],
                    u_SpotShadow[1].light_view_proj,
                    u_SpotShadow[1].params,
                    worldPos,
                    Ng,
                    L);
            }
            else if (i == 2)
            {
                s = SampleShadowMap(
                    u_SpotShadowMap2,
                    u_Samplers[10],
                    u_SpotShadow[2].light_view_proj,
                    u_SpotShadow[2].params,
                    worldPos,
                    Ng,
                    L);
            }
            else if (i == 3)
            {
                s = SampleShadowMap(
                    u_SpotShadowMap3,
                    u_Samplers[11],
                    u_SpotShadow[3].light_view_proj,
                    u_SpotShadow[3].params,
                    worldPos,
                    Ng,
                    L);
            }

            radiance *= s;
        }

        Lo += EvaluateLight(radiance, L, N, V, albedo, metallic, roughness);
    }

    float3 ambient = u_Ambient.xyz * albedo;
    if (u_IBLIntensity > 0.0)
    {
        ambient = EvaluateIBL(N, V, albedo, metallic, roughness, u_IBLIntensity);
    }

    // SSAO affects ambient/IBL only, not direct lighting or emissive.
    float ao = 1.0;
    if (u_SSAOEnabled > 0.5)
    {
        uint ssaoWidth = 1;
        uint ssaoHeight = 1;
        u_SSAOTex.GetDimensions(ssaoWidth, ssaoHeight);

        float2 ssaoSize = float2(max((float)ssaoWidth, 1.0),
                                 max((float)ssaoHeight, 1.0));
        float2 ssaoUV = screenPos.xy / ssaoSize;

        ao = u_SSAOTex.Sample(u_Samplers[14], ssaoUV).r;
        ao = lerp(1.0, ao, saturate(u_SSAOIntensity));
    }

    ambient *= ao;

    float3 scene = ambient + Lo + emissive;
    float3 bloomSrc = SoftThreshold(emissive, u_BloomThreshold, u_BloomKnee);

    SceneResult r;
    r.color = scene;
    r.bloom = bloomSrc;
    return r;
}
