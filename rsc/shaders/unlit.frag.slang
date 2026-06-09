struct PSInput
{
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
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

// Keep the same texture + sampler layout your engine already binds
Texture2D u_BaseColorTex           : register(t0, space2);
Texture2D u_MetallicRoughnessTex   : register(t1, space2);
Texture2D u_NormalTex              : register(t2, space2);
Texture2D u_EmissiveTex            : register(t3, space2);

SamplerState u_Sampler : register(s0, space2);

// Keep Lighting buffer declared so your engine can keep binding it
// (We ignore its contents in unlit)
cbuffer Lighting : register(b1, space3)
{
    int   u_NumPoint;
    int   u_NumDir;
    int   u_NumSpot;
    float pad_l0;

    float4 pad_lights[256]; // dummy padding (doesn't matter, just occupies space)
    float3 u_CameraPos; float pad_l1;
    float3 u_Ambient;   float pad_l2;
};

struct PSOut {
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

[shader("fragment")]
PSOut main(PSInput input)
{
    float4 baseColor = u_BaseColorTex.Sample(u_Sampler, input.uv) * u_BaseColorFactor;
    float3 emissive  = u_EmissiveTex.Sample(u_Sampler, input.uv).rgb * u_EmissiveFactor;

    if (baseColor.a < 0.01)
        discard;

    PSOut o;
    o.color = float4(baseColor.rgb + emissive, baseColor.a);
    o.bloom = float4(emissive, 1.0); // or threshold it if you want
    return o;
}
