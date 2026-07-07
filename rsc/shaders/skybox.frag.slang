TextureCube u_Skybox : register(t0, space2);
SamplerState u_Sampler : register(s0, space2);

struct PSInput {
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

struct PSOut {
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

[shader("fragment")]
PSOut fsMain(PSInput input)
{
    float3 dir = normalize(input.dir);

    float4 c = u_Skybox.Sample(u_Sampler, dir);

    PSOut o;
    o.color = float4(c.rgb, 1.0); // skybox is always opaque
    o.bloom = float4(0,0,0,0);     // keep bloom empty
    return o;
}
