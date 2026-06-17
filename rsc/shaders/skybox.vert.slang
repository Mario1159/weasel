cbuffer Matrices : register(b0, space1)
{
    column_major float4x4 u_InvViewProj;
};


struct VSOutput {
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

[shader("vertex")]
VSOutput vsMain(uint id : SV_VertexID)
{
    // Fullscreen triangle in clip space
    float2 pos;
    if (id == 0) pos = float2(-1.0, -1.0);
    else if (id == 1) pos = float2(-1.0,  3.0);
    else pos = float2(3.0, -1.0);

    VSOutput o;
    o.pos = float4(pos, 1.0, 1.0);

    // reconstruct view ray
    // We use the provided u_InvViewProj which should now correctly account
    // for the camera orientation and projection.
    float4 view = mul(u_InvViewProj, float4(pos, 1.0, 1.0));
    o.dir = view.xyz / view.w;

    return o;
}
