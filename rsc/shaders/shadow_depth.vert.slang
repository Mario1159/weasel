cbuffer ShadowMatrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_LightViewProj;
};

struct VSInput {
    float3 pos : POSITION;
};

struct VSOutput {
    float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput main(VSInput v)
{
    VSOutput o;
    float4 world = mul(u_Model, float4(v.pos, 1.0));
    o.pos = mul(u_LightViewProj, world);
    return o;
}
