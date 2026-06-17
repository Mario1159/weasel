Texture2D u_Input : register(t0, space2);
SamplerState u_Sampler : register(s0, space2);

cbuffer Blur : register(b0, space3)
{
    float2 u_TexelSize; // 1.0 / target_size
    float2 u_Dir;       // (1,0) or (0,1)
};

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

[shader("fragment")]
float4 fsMain(VSOut i) : SV_Target
{
    float2 o = u_TexelSize * u_Dir;

    // weights: 0.227027, 0.316216, 0.070270 (common 5-tap)
    float3 c = u_Input.Sample(u_Sampler, i.uv).rgb * 0.227027;
    c += u_Input.Sample(u_Sampler, i.uv + o * 1.384615).rgb * 0.316216;
    c += u_Input.Sample(u_Sampler, i.uv - o * 1.384615).rgb * 0.316216;
    c += u_Input.Sample(u_Sampler, i.uv + o * 3.230769).rgb * 0.070270;
    c += u_Input.Sample(u_Sampler, i.uv - o * 3.230769).rgb * 0.070270;

    return float4(c, 1.0);
}
