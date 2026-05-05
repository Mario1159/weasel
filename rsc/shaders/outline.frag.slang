struct PSInput
{
    float4 pos : SV_Position;
};

cbuffer OutlineParamsPS : register(b0, space3)
{
    float4 u_OutlineColor;
    float4 u_Params;
};

struct PSOutput
{
    float4 color0 : SV_Target0;
    float4 color1 : SV_Target1;
};

[shader("fragment")]
PSOutput main(PSInput input)
{
    PSOutput o;
    o.color0 = u_OutlineColor;
    o.color1 = float4(0.0, 0.0, 0.0, 1.0);
    return o;
}
