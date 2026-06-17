cbuffer BgParams : register(b0, space3)
{
    float2 u_ViewportSize;
    float  u_CheckSizePx;
    float  pad0;
    float4 u_ColorA;
    float4 u_ColorB;
};

struct PSOut {
    float4 color : SV_Target0; // HDR scene
    float4 bloom : SV_Target1; // bloom source
};

[shader("fragment")]
PSOut fsMain(float4 pos : SV_Position)
{
    float2 p = pos.xy / max(u_CheckSizePx, 1.0);
    int cx = (int)floor(p.x);
    int cy = (int)floor(p.y);
    bool odd = ((cx + cy) & 1) != 0;

    float4 c = odd ? u_ColorA : u_ColorB;

    PSOut o;
    o.color = c;
    o.bloom = float4(0, 0, 0, 1); // background shouldn't bloom
    return o;
}
