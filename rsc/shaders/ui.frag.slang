struct PSInput {
    float4 pos   : SV_Position;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target
{
    return input.color;
}
