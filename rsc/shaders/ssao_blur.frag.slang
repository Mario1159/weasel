// SSAO blur frag

struct FSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

cbuffer BlurParams : register(b0, space3)
{
    float4 u_TexelSize; // xy used
};

Texture2D u_AOTex : register(t0, space2);
SamplerState u_Samplers[1] : register(s0, space2);

[shader("fragment")]
float fsMain(FSIn input) : SV_Target0
{
    float sum = 0.0;
    float wsum = 0.0;

    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            float2 uv = input.uv + float2(x, y) * u_TexelSize.xy;
            float w = 1.0;
            sum += u_AOTex.Sample(u_Samplers[0], uv).r * w;
            wsum += w;
        }
    }

    return sum / max(wsum, 1e-5);
}
