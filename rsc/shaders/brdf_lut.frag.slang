// brdf_lut.frag.slang
// Output: RG = (A,B) split-sum BRDF LUT (LearnOpenGL convention)
// Use RG16F or RGBA16F. Must be LINEAR (no sRGB).
// Sampler for later sampling should be CLAMP, LINEAR.

static const float PI = 3.14159265359;
static const uint  SAMPLE_COUNT = 32;

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555u) << 1)  | ((bits & 0xAAAAAAAAu) >> 1);
    bits = ((bits & 0x33333333u) << 2)  | ((bits & 0xCCCCCCCCu) >> 2);
    bits = ((bits & 0x0F0F0F0Fu) << 4)  | ((bits & 0xF0F0F0F0u) >> 4);
    bits = ((bits & 0x00FF00FFu) << 8)  | ((bits & 0xFF00FF00u) >> 8);
    return float(bits) * 2.3283064365386963e-10; // / 2^32
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
    float3 T  = normalize(cross(up, N));
    float3 B  = cross(N, T);

    return normalize(T * H.x + B * H.y + N * H.z);
}

// IMPORTANT: IBL variant (matches Godot's intent).
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    // Godot style: a = roughness, k = a^2 / 2
    float a = roughness;
    float k = (a * a) * 0.5;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
    float ggxV = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggxL = GeometrySchlickGGX_IBL(NdotL, roughness);
    return ggxV * ggxL;
}

// Returns (A,B) so that: specIBL = prefiltered * (F0 * A + B)
float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V = float3(sqrt(max(1.0 - NdotV * NdotV, 0.0)), 0.0, NdotV);
    float3 N = float3(0.0, 0.0, 1.0);

    float A = 0.0;
    float B = 0.0;

    // avoid singularities
    roughness = max(roughness, 1e-4);
    NdotV     = max(NdotV,     1e-4);

    [loop]
    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, roughness, N);
        float3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));

        if (NdotL > 0.0)
        {
            float G = GeometrySmith_IBL(NdotV, NdotL, roughness);

            // same structure as both your code and Godot's:
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-5);

            // Fresnel term for split-sum
            float Fc = pow(1.0 - VdotH, 5.0);

            // LearnOpenGL split-sum convention:
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A *= (1.0 / float(SAMPLE_COUNT));
    B *= (1.0 / float(SAMPLE_COUNT));
    return float2(A, B);
}

[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target
{
    float2 uv = saturate(input.uv);

    uv.y = 1.0 - uv.y;

    float NdotV     = saturate(uv.x);
    float roughness = saturate(uv.y);

    float2 brdf = IntegrateBRDF(NdotV, roughness);

    // Store in RG; BA unused.
    return float4(brdf, 0.0, 1.0);
}
