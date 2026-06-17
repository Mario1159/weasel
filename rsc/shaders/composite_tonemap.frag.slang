// composite_tonemap.frag.slang
// SPIR-V / SDL_gpu3 layout:
//   Fragment sampled textures/samplers: space2  (set 2)
//   Fragment uniform buffers:           space3  (set 3)

Texture2D sceneTex : register(t0, space2);
SamplerState sceneSamp : register(s0, space2);

Texture2D bloomTex : register(t1, space2);
SamplerState bloomSamp : register(s1, space2);

cbuffer Params : register(b0, space3)
{
    float exposure;
    float bloomIntensity;
    float2 _pad;
};

struct PSIn {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 tonemap_reinhard(float3 x) { return x / (1.0 + x); }

[shader("fragment")]
float4 fsMain(PSIn i) : SV_Target
{
    float2 uvScene = float2(i.uv.x, 1.0 - i.uv.y); // <-- flip only scene
    float2 uvBloom = i.uv;                         // <-- keep bloom as-is

    float3 scene = sceneTex.Sample(sceneSamp, uvScene).rgb;
    float3 bloom = bloomTex.Sample(bloomSamp, uvBloom).rgb;

    float3 hdr = (scene + bloom * bloomIntensity) * exposure;
    float3 ldr = tonemap_reinhard(hdr);
    ldr = pow(ldr, 1.0 / 2.2);

    return float4(ldr, 1.0);
}
