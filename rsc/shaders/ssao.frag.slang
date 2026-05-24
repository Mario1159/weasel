// SSAO frag

static const int KERNEL_SIZE = 64;

struct FSIn
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

cbuffer SSAOParams : register(b0, space3)
{
    column_major float4x4 u_Proj;
    column_major float4x4 u_InvProj;
    float4 u_ScreenNoiseScale;          // xy used
    float4 u_RadiusBiasPowerIntensity;  // x=radius y=bias z=power w=intensity
};

cbuffer SSAOKernel : register(b1, space3)
{
    float4 u_Samples[KERNEL_SIZE];
};

Texture2D u_NormalDepthTex : register(t0, space2);
Texture2D u_NoiseTex       : register(t1, space2);
SamplerState u_Samplers[2] : register(s0, space2);

float3 ReconstructViewPos(float2 uv, float linearDepth)
{
    float2 ndc = uv * 2.0 - 1.0;

    float4 farPos = mul(u_InvProj, float4(ndc, 1.0, 1.0));
    farPos /= max(farPos.w, 1e-6);

    float3 ray = farPos.xyz;
    return ray * (linearDepth / max(-ray.z, 1e-6));
}

[shader("fragment")]
float main(FSIn input) : SV_Target0
{
    float4 nd = u_NormalDepthTex.Sample(u_Samplers[0], input.uv);
    float3 N = normalize(nd.xyz);
    float depth = nd.w;

    if (depth <= 1e-6)
        return 1.0;

    float3 origin = ReconstructViewPos(input.uv, depth);

    float2 noiseUV = input.uv * u_ScreenNoiseScale.xy;
    float3 randVec = normalize(u_NoiseTex.Sample(u_Samplers[1], noiseUV).xyz);

    float3 T = normalize(randVec - N * dot(randVec, N));
    float3 B = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    float radius = u_RadiusBiasPowerIntensity.x;
    float bias   = u_RadiusBiasPowerIntensity.y;
    float power  = u_RadiusBiasPowerIntensity.z;

    float occ = 0.0;

    [unroll]
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        float3 sampleVS = origin + mul(u_Samples[i].xyz, TBN) * radius;

        float4 sampleClip = mul(u_Proj, float4(sampleVS, 1.0));
        sampleClip.xyz /= max(sampleClip.w, 1e-6);

        float2 sampleUV = sampleClip.xy * 0.5 + 0.5;

        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
            sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float4 sampleND = u_NormalDepthTex.Sample(u_Samplers[0], sampleUV);
        float sampleDepth = sampleND.w;
        float3 samplePos = ReconstructViewPos(sampleUV, sampleDepth);

        float rangeWeight = smoothstep(0.0, 1.0, radius / (abs(origin.z - samplePos.z) + 1e-4));
        occ += ((samplePos.z >= sampleVS.z + bias) ? 1.0 : 0.0) * rangeWeight;
    }

    occ = 1.0 - (occ / KERNEL_SIZE);
    occ = pow(saturate(occ), power);

    return occ;
}
