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
    float4 pos : SV_Position;
};

// The outline pass binds its own params cbuffer at b1, so the skinning
// palette goes one slot higher.
#define WEASEL_PALETTE_BINDING 2
#include "skinning.slang"

cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_ViewProj;
    column_major float4x4 u_Normal;
};

cbuffer OutlineParamsVS : register(b1, space1)
{
    float4 u_OutlineColorVS;
    float4 u_ParamsVS; // x = width
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;

    float4 world = mul(u_Model, float4(skin_position(input.pos, input.joints, input.weights), 1.0));

    float3x3 Nmat = (float3x3)u_Normal;
    float3 worldNormal = normalize(mul(Nmat, skin_normal(input.normal, input.joints, input.weights)));

    world.xyz += worldNormal * u_ParamsVS.x;

    o.pos = mul(u_ViewProj, world);
    return o;
}
