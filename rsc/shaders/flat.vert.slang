cbuffer MVP : register(b0, space1)
{
    float4x4 u_VP;
};

struct VSInput {
    float3 pos   : POSITION;
    float4 color : COLOR0;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;
    o.pos = mul(u_VP, float4(input.pos, 1.0));
    o.color = input.color;
    return o;
}
