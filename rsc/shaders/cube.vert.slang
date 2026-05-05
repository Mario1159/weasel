// cube.vert.slang

cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_ViewProj;
    column_major float4x4 u_NormalMatrix; // padded & safe
};

struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3; // xyz=tangent, w=sign
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 tangent  : TEXCOORD3; // PS reads TEXCOORD3
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput o;

    float4 world = mul(u_Model, float4(input.pos, 1.0));
    float4 clip  = mul(u_ViewProj, world);

    o.pos = clip;
    o.worldPos = world.xyz;

    float3x3 Nmat = (float3x3)u_NormalMatrix;
    o.normal = mul(Nmat, input.normal);

    // transform tangent the same way as normal (keep handedness sign in w)
    float3 T = mul(Nmat, input.tangent.xyz);
    o.tangent = float4(T, input.tangent.w);

    o.uv = input.uv;

    return o;
}
