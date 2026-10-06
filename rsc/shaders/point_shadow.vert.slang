// point_shadow.vert.slang
// Vertex shader for point-light shadow cubemap rendering.
// Matches your usual vertex input layout.
// Outputs world position so the fragment shader can write radial depth.

struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3;
    uint4  joints  : TEXCOORD4;
    float4 weights : TEXCOORD5;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float3 worldPos : TEXCOORD0;
};

#include "skinning.slang"

cbuffer ShadowMatrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_LightViewProj;
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;

    float4 world = mul(u_Model, float4(skin_position(input.pos, input.joints, input.weights), 1.0));
    o.worldPos = world.xyz;
    o.pos = mul(u_LightViewProj, world);

    return o;
}
