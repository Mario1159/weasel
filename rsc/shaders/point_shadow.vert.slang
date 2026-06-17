// point_shadow.vert.slang
// Vertex shader for point-light shadow cubemap rendering.
// Matches your usual vertex input layout.
// Outputs world position so the fragment shader can write radial depth.

struct VSInput
{
    float3 pos     : POSITION;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float3 worldPos : TEXCOORD0;
};

cbuffer ShadowMatrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_LightViewProj;
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;

    float4 world = mul(u_Model, float4(input.pos, 1.0));
    o.worldPos = world.xyz;
    o.pos = mul(u_LightViewProj, world);

    return o;
}
