Texture2D u_Texture : register(t0, space0);
SamplerState u_Sampler : register(s0, space0);

struct PSInput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

[shader("fragment")]
float4 main(PSInput input) : SV_Target
{
    float4 tex = u_Texture.Sample(u_Sampler, input.uv);
    return tex * input.color;
}
