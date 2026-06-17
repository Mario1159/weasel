// ibl_prefilter.frag.slang
// Specular prefilter of environment cubemap for GGX at given roughness.
// Render target: cubemap with mip chain.
// For each (face, mip), render fullscreen triangle and set roughness accordingly.

static const float PI = 3.14159265359;

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

TextureCube u_Environment : register(t0, space2);
SamplerState u_Sampler    : register(s0, space2);

cbuffer Params : register(b0, space3)
{
    int   u_FaceIndex;     // 0..5
    float u_Roughness;     // 0..1 (set from mip)
    float u_EnvIntensity;
    float pad0;
};

// Same face mapping as irradiance shader.
float3 FaceUVToDir(int face, float2 uv)
{
    float2 a = uv * 2.0 - 1.0;

    if (face == 0) return normalize(float3( 1.0,  -a.y, -a.x)); // +X
    if (face == 1) return normalize(float3(-1.0,  -a.y,  a.x)); // -X
    if (face == 2) return normalize(float3( a.x,  1.0,   a.y)); // +Y
    if (face == 3) return normalize(float3( a.x, -1.0,  -a.y)); // -Y
    if (face == 4) return normalize(float3( a.x,  -a.y,  1.0)); // +Z
    return             normalize(float3(-a.x,  -a.y, -1.0));     // -Z
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1)  | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2)  | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4)  | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8)  | ((bits & 0xFF00FF00u) >> 8);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    float3 up = (abs(N.z) < 0.999) ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float3 sampleVec = T * H.x + B * H.y + N * H.z;
    return normalize(sampleVec);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 1e-6);
}

[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target
{
    float2 uv = input.uv;

    float3 R = FaceUVToDir(u_FaceIndex, uv);
    float3 N = R; // for prefiltering, N = R
    float3 V = R;

    float roughness = saturate(u_Roughness);

    const uint SAMPLE_COUNT = 32;

    float3 prefilteredColor = float3(0,0,0);
    float totalWeight = 0.0;

    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, roughness, N);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // --- Importance sampling PDF for GGX ---
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);

            float D = DistributionGGX(N, H, roughness);
            float pdf = max((D * NdotH / max(4.0 * HdotV, 1e-6)), 1e-6);

            // --- Solid angle of a texel and of this sample ---
            // You can optionally compute mip based on pdf + env resolution.
            // BUT since you are rendering into explicit mip levels already,
            // sampling mip=0 is acceptable and common for prefilter passes,
            // because the integration itself produces the blur.
            float3 sample = u_Environment.SampleLevel(u_Sampler, float3(L.x, -L.y, L.z), 0).rgb;

            prefilteredColor += sample * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 1e-6);
    prefilteredColor *= u_EnvIntensity;

    return float4(prefilteredColor, 1.0);
}
