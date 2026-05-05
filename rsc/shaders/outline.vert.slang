struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD3;
};

struct VSOutput
{
    float4 pos : SV_Position;
};

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
VSOutput main(VSInput input)
{
    VSOutput o;

    float4 world = mul(u_Model, float4(input.pos, 1.0));

    float3x3 Nmat = (float3x3)u_Normal;
    float3 worldNormal = normalize(mul(Nmat, input.normal));

    world.xyz += worldNormal * u_ParamsVS.x;

    o.pos = mul(u_ViewProj, world);
    return o;
}
