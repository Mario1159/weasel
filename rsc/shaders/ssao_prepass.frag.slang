// SSAO prepass frag

struct PSInput
{
    float4 pos        : SV_Position;
    float3 viewPos    : TEXCOORD0;
    float3 viewNormal : TEXCOORD1;
};

[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target0
{
    float3 N = normalize(input.viewNormal);
    float linearDepth = -input.viewPos.z; // positive forward depth

    return float4(N, linearDepth);
}
