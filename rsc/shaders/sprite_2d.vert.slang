struct VSInput {
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct VSOutput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

// Projection matrix passed via push constant (ortho)
[[vk::push_constant]]
cbuffer PushConstants
{
    float4x4 projection;
};

[shader("vertex")]
VSOutput main(VSInput input)
{
    VSOutput o;
    o.pos = mul(projection, float4(input.position, 0.0, 1.0));
    o.uv = input.uv;
    o.color = input.color;
    return o;
}
