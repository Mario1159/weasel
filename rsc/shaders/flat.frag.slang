struct PSOut
{
    float4 color : SV_Target0; // HDR scene color
    float4 bloom : SV_Target1; // bloom source
};

[shader("fragment")]
PSOut main(float4 color : COLOR0)
{
    PSOut o;
    o.color = color;
    o.bloom = float4(0.0, 0.0, 0.0, 1.0); // debug should not bloom
    return o;
}
