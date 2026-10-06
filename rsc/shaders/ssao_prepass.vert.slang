// SSAO Prepass vert

struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3;
    uint4  joints  : TEXCOORD4;
    float4 weights : TEXCOORD5;
};

struct VSOut
{
    float4 pos        : SV_Position;
    float3 viewPos    : TEXCOORD0;
    float3 viewNormal : TEXCOORD1;
};

#include "skinning.slang"

cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_View;
    column_major float4x4 u_Proj;
    column_major float4x4 u_Normal;
};

[shader("vertex")]
VSOut vsMain(VSInput input)
{
    VSOut o;

    float4 worldPos = mul(u_Model, float4(skin_position(input.pos, input.joints, input.weights), 1.0));
    float4 viewPos4 = mul(u_View, worldPos);
    o.viewPos = viewPos4.xyz;

    float3 worldNormal = normalize(mul((float3x3)u_Normal, skin_normal(input.normal, input.joints, input.weights)));
    o.viewNormal = normalize(mul((float3x3)u_View, worldNormal));

    o.pos = mul(u_Proj, viewPos4);
    return o;
}
