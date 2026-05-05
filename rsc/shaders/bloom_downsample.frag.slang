Texture2D u_BloomSrc : register(t0, space2);
SamplerState u_Sampler : register(s0, space2);

cbuffer Downsample : register(b0, space3)
{
    float2 u_SrcTexelSize; // 1.0 / src_size
    float2 pad0;
};

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 main(VSOut i) : SV_Target
{
    float2 t = u_SrcTexelSize;

    float3 c =
        u_BloomSrc.Sample(u_Sampler, i.uv + float2(-t.x, -t.y)).rgb +
        u_BloomSrc.Sample(u_Sampler, i.uv + float2(+t.x, -t.y)).rgb +
        u_BloomSrc.Sample(u_Sampler, i.uv + float2(-t.x, +t.y)).rgb +
        u_BloomSrc.Sample(u_Sampler, i.uv + float2(+t.x, +t.y)).rgb;

    c *= 0.25;
    return float4(c, 1.0);
}
