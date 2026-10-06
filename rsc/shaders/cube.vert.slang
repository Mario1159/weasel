// cube.vert.slang

#include "skinning.slang"

cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_ViewProj;
    column_major float4x4 u_NormalMatrix; // padded & safe
    column_major float4x4 u_View;        // world -> view
};

struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3; // xyz=tangent, w=sign
    uint4  joints  : TEXCOORD4;
    float4 weights : TEXCOORD5;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3; // PS reads TEXCOORD3
    float3 viewPos  : TEXCOORD4; // used by cluster lookup
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;

    // Skinning happens in model space, before the world transform, because
    // the palette is model-space (model matrix * inverse-bind).
    float3 skinnedPos = skin_position(input.pos, input.joints, input.weights);

    float4 world = mul(u_Model, float4(skinnedPos, 1.0));
    float4 clip  = mul(u_ViewProj, world);
    float4 view  = mul(u_View, world);

    o.pos = clip;
    o.worldPos = world.xyz;
    o.viewPos  = view.xyz;

    float3x3 Nmat = (float3x3)u_NormalMatrix;
    o.normal = mul(Nmat, skin_normal(input.normal, input.joints, input.weights));

    // transform tangent the same way as normal (keep handedness sign in w)
    float3 T = mul(Nmat, skin_tangent(input.tangent.xyz, input.joints, input.weights));
    o.tangent = float4(T, input.tangent.w);

    o.uv = input.uv;

    return o;
}
