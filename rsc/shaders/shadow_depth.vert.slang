#include "skinning.slang"

cbuffer ShadowMatrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_LightViewProj;
};

struct VSInput {
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3;
    uint4  joints  : TEXCOORD4;
    float4 weights : TEXCOORD5;
};

struct VSOutput {
    float4 pos : SV_Position;
};

[shader("vertex")]
VSOutput vsMain(VSInput v)
{
    VSOutput o;
    float4 world = mul(u_Model, float4(skin_position(v.pos, v.joints, v.weights), 1.0));
    o.pos = mul(u_LightViewProj, world);
    return o;
}
