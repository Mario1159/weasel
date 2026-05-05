// Fragment Slot 0
Texture2D equirect_map : register(t0, space2);
SamplerState equirect_sampler : register(s0, space2);

struct VSOutput {
    float4 position : SV_Position;
    float3 world_pos : TEXCOORD0;
};

// Constant for 1/PI and 1/(2*PI)
static const float PI = 3.14159265359;
static const float INV_PI = 0.31830988618;
static const float INV_2PI = 0.15915494309;

float2 sample_equirect(float3 v) {
    // atan2 returns [-PI, PI]
    // asin returns [-PI/2, PI/2]
    // Standard equirectangular mapping.
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));

    // Scale to [-0.5, 0.5]
    uv *= float2(INV_2PI, INV_PI);

    // Shift to [0, 1]
    uv += 0.5;

    // Flip Y: asin(1) is top (v.y=1), we want uv.y=0 if top-left origin
    // Current: asin(1)*INV_PI + 0.5 = 0.5 + 0.5 = 1.0 (bottom)
    // New: 1.0 - (0.5 + 0.5) = 0.0 (top)
    uv.y = 1.0 - uv.y;

    return uv;
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target {
    float3 dir = normalize(input.world_pos);
    float2 uv = sample_equirect(dir);
    return equirect_map.Sample(equirect_sampler, uv);
}
