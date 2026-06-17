struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[shader("vertex")]
VSOut vsMain(uint id : SV_VertexID)
{
    float2 p = (id == 0) ? float2(-1, -1) :
               (id == 1) ? float2(-1,  3) :
                           float2( 3, -1);

    VSOut o;
    o.pos = float4(p, 0, 1);

    // Map clip-space [-1..1] to UV [0..1]
    // Note: p can go beyond 1 for fullscreen tri, so clamp in PS if needed.
    o.uv = p * 0.5f + 0.5f;
    return o;
}
