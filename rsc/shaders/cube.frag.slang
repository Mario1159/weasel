// cube.frag.slang
// Supports:
// - Directional lights
// - Point lights
// - Spot lights
// - 1 directional shadow map
// - up to 4 spotlight shadow maps
// - up to 2 point-light shadow cubemaps
// - SSAO (applied to ambient / IBL only)
//
// Texture bindings:
// t0  = base color
// t1  = metallic-roughness
// t2  = normal
// t3  = emissive
// t4  = irradiance cubemap
// t5  = prefilter cubemap
// t6  = BRDF LUT
// t7  = directional shadow map
// t8  = spotlight shadow map 0
// t9  = spotlight shadow map 1
// t10 = spotlight shadow map 2
// t11 = spotlight shadow map 3
// t12 = point shadow map 0
// t13 = point shadow map 1
// t14 = SSAO texture
//
// Samplers must match the same indices.

#define MAX_POINT_LIGHTS 16
#define MAX_DIR_LIGHTS   4
#define MAX_SPOT_LIGHTS  8

#define MAX_SHADOWED_DIR_LIGHTS   1
#define MAX_SHADOWED_SPOT_LIGHTS  4
#define MAX_SHADOWED_POINT_LIGHTS 2

static const float PI = 3.14159265359;

struct PSInput
{
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
    float4 tangent   : TEXCOORD3; // xyz=tangent, w=sign
};

cbuffer Material : register(b0, space3)
{
    float4 u_BaseColorFactor;
    float  u_MetallicFactor;
    float  u_RoughnessFactor;
    float2 pad_m0;

    float3 u_EmissiveFactor;
    float  pad_m1;
};

struct PointLight
{
    float4 pos_radius;      // xyz=pos, w=radius
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
    int4 u_Counts; // x=num_point, y=num_dir, z=num_spot, w=pad

    PointLight u_PointLights[MAX_POINT_LIGHTS];
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

Texture2D u_BaseColorTex           : register(t0,  space2);
Texture2D u_MetallicRoughnessTex   : register(t1,  space2);
Texture2D u_NormalTex              : register(t2,  space2);
Texture2D u_EmissiveTex            : register(t3,  space2);

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

//////////////////////////////////////
// Normal mapping
//////////////////////////////////////

float3 UnpackNormal(float3 n)
{
    return normalize(n * 2.0 - 1.0);
}

float3 ApplyNormalMap(float3 N, float4 tangent, float2 uv)
{
    float3 T = normalize(tangent.xyz);
    T = normalize(T - N * dot(N, T));

    float3 B = normalize(cross(N, T) * tangent.w);

    float3 nTS = UnpackNormal(u_NormalTex.Sample(u_Samplers[2], uv).xyz);

    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(nTS, TBN));
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

struct PSOut
{
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

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
// Main
//////////////////////////////////////

[shader("fragment")]
PSOut main(PSInput input)
{
    int u_NumPoint = u_Counts.x;
    int u_NumDir   = u_Counts.y;
    int u_NumSpot  = u_Counts.z;

    float u_BloomThreshold = u_BloomAndSSAO0.x;
    float u_BloomKnee      = u_BloomAndSSAO0.y;
    float u_BloomIntensity = u_BloomAndSSAO0.z;
    float u_SSAOEnabled    = u_BloomAndSSAO0.w;
    float u_SSAOIntensity  = u_SSAOAndPad.x;

    float3 Ng = normalize(input.normal);
    float3 N  = ApplyNormalMap(Ng, input.tangent, input.uv);
    float3 V  = normalize(u_CameraPos.xyz - input.worldPos);

    float4 baseColor =
        u_BaseColorTex.Sample(u_Samplers[0], input.uv) *
        u_BaseColorFactor;

    float3 albedo = baseColor.rgb;

    float2 mr = u_MetallicRoughnessTex.Sample(u_Samplers[1], input.uv).gb;
    float roughness = clamp(mr.x * u_RoughnessFactor, 0.04, 1.0);
    float metallic  = mr.y * u_MetallicFactor;

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
                input.worldPos,
                Ng,
                L);

            radiance *= s;
        }

        Lo += EvaluateLight(radiance, L, N, V, albedo, metallic, roughness);
    }

    // -------- Point lights --------
    for (int i = 0; i < u_NumPoint; ++i)
    {
        float3 lightPos = u_PointLights[i].pos_radius.xyz;
        float  radius   = u_PointLights[i].pos_radius.w;

        float3 toL = lightPos - input.worldPos;
        float dist = length(toL);
        if (dist > radius)
            continue;

        float3 L = toL / max(dist, 1e-6);
        float atten = saturate(1.0 - dist / radius);

        float3 radiance =
            (u_PointLights[i].color_intensity.rgb *
             u_PointLights[i].color_intensity.w) * atten;

        int shadowIndex = u_PointLights[i].shadow_info.x;
        int hasShadow   = u_PointLights[i].shadow_info.y;

        if (hasShadow != 0 &&
            shadowIndex >= 0 &&
            shadowIndex < MAX_SHADOWED_POINT_LIGHTS)
        {
            float s = 1.0;

            if (shadowIndex == 0)
            {
                s = SamplePointShadow(
                    u_PointShadowMap0,
                    u_Samplers[12],
                    u_PointShadow[0].pos_far.xyz,
                    u_PointShadow[0].pos_far.w,
                    u_PointShadow[0].params,
                    input.worldPos);
            }
            else if (shadowIndex == 1)
            {
                s = SamplePointShadow(
                    u_PointShadowMap1,
                    u_Samplers[13],
                    u_PointShadow[1].pos_far.xyz,
                    u_PointShadow[1].pos_far.w,
                    u_PointShadow[1].params,
                    input.worldPos);
            }

            radiance *= s;
        }

        Lo += EvaluateLight(radiance, L, N, V, albedo, metallic, roughness);
    }

    // -------- Spot lights --------
    for (int i = 0; i < u_NumSpot; ++i)
    {
        float3 lightPos = u_SpotLights[i].pos_pad.xyz;
        float3 lightDir = normalize(u_SpotLights[i].dir_pad.xyz);

        float3 toL = lightPos - input.worldPos;
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
                    input.worldPos,
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
                    input.worldPos,
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
                    input.worldPos,
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
                    input.worldPos,
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
        // Derive the screen-space UV from the current pixel position instead
        // of interpolating it from the triangle vertices. Interpolated clip
        // UVs drift on large triangles, which shows up most obviously on the
        // coarse built-in primitives.
        uint ssaoWidth = 1;
        uint ssaoHeight = 1;
        u_SSAOTex.GetDimensions(ssaoWidth, ssaoHeight);

        float2 ssaoSize = float2(max((float)ssaoWidth, 1.0),
                                 max((float)ssaoHeight, 1.0));
        float2 ssaoUV = input.pos.xy / ssaoSize;

        ao = u_SSAOTex.Sample(u_Samplers[14], ssaoUV).r;
        ao = lerp(1.0, ao, saturate(u_SSAOIntensity));
    }

    ambient *= ao;

    float3 emissive =
        u_EmissiveTex.Sample(u_Samplers[3], input.uv).rgb *
        u_EmissiveFactor;

    float3 scene = ambient + Lo + emissive;
    float3 bloomSrc = SoftThreshold(emissive, u_BloomThreshold, u_BloomKnee);

    PSOut o;
    o.color = float4(scene, baseColor.a);
    o.bloom = float4(bloomSrc, 1.0);
    return o;
}
