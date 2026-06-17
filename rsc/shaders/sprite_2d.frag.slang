// SDL GPU SPIR-V resource layout for fragment shaders:
//   space2: sampled textures, storage textures, storage buffers
//   space3: uniform buffers
Texture2D u_Texture : register(t0, space2);
SamplerState u_Sampler : register(s0, space2);

struct PSInput {
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target
{
    float4 tex = u_Texture.Sample(u_Sampler, input.uv);
    return tex * input.color;
}
