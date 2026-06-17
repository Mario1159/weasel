struct VSInput {
    float2 position : POSITION;
    float4 color    : COLOR0;
    float2 uv       : TEXCOORD0;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float4 color : COLOR0;
    float2 uv    : TEXCOORD0;
};

[shader("vertex")]
VSOutput vsMain(VSInput input)
{
    VSOutput o;

    // RmlUi already provides clip-space coords
    o.pos = float4(input.position.xy, 0.0, 1.0);

    // Color is already normalized floats from UBYTE4_NORM
    o.color = input.color;
    o.uv = input.uv;

    return o;
}
