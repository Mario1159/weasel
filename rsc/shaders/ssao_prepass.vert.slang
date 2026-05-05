// SSAO Prepass vert

struct VSInput
{
    float3 pos     : POSITION;
    float3 normal  : NORMAL;
    float2 uv      : TEXCOORD0;
    float4 tangent : TEXCOORD1;
};

struct VSOut
{
    float4 pos        : SV_Position;
    float3 viewPos    : TEXCOORD0;
    float3 viewNormal : TEXCOORD1;
};

cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_Model;
    column_major float4x4 u_View;
    column_major float4x4 u_Proj;
    column_major float4x4 u_Normal;
};

[shader("vertex")]
VSOut main(VSInput input)
{
    VSOut o;

    float4 worldPos = mul(u_Model, float4(input.pos, 1.0));
    float4 viewPos4 = mul(u_View, worldPos);
    o.viewPos = viewPos4.xyz;

    float3 worldNormal = normalize(mul((float3x3)u_Normal, input.normal));
    o.viewNormal = normalize(mul((float3x3)u_View, worldNormal));

    o.pos = mul(u_Proj, viewPos4);
    return o;
}
