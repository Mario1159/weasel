// ibl_irradiance.frag.slang
// Diffuse irradiance convolution of an environment cubemap.
// Render target: cubemap (e.g., 32x32 RGBA16F or RGBA8 if you stay LDR)
// For each face, render fullscreen triangle with a cbuffer specifying face index.

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
    int   u_FaceIndex;   // 0..5 : +X -X +Y -Y +Z -Z
    float u_EnvIntensity;
    float2 pad0;
};

// Convert (uv, face) to a direction vector.
// uv in [0..1]. We map to [-1..1] and use standard cubemap face transforms.
float3 FaceUVToDir(int face, float2 uv)
{
    float2 a = uv * 2.0 - 1.0;

    // Note: choose a consistent convention with your skybox.
    // This mapping matches common DX cubemap face conventions.
    if (face == 0) return normalize(float3( 1.0,  -a.y, -a.x)); // +X
    if (face == 1) return normalize(float3(-1.0,  -a.y,  a.x)); // -X
    if (face == 2) return normalize(float3( a.x,  1.0,   a.y)); // +Y
    if (face == 3) return normalize(float3( a.x, -1.0,  -a.y)); // -Y
    if (face == 4) return normalize(float3( a.x,  -a.y,  1.0)); // +Z
    return             normalize(float3(-a.x,  -a.y, -1.0));     // -Z
}

// Cosine-weighted hemisphere sampling around N.
// We do a simple numerical integration over phi/theta (good enough for low-res irradiance maps).
[shader("fragment")]
float4 fsMain(PSInput input) : SV_Target
{
    float2 uv = input.uv;
    // If your fullscreen triangle doesn't provide uv, you must derive it; same note as BRDF.

    float3 N = FaceUVToDir(u_FaceIndex, uv);

    // Build tangent frame
    float3 up = (abs(N.y) < 0.999) ? float3(0.0, 1.0, 0.0) : float3(0.0, 0.0, 1.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float3 irradiance = float3(0,0,0);

    // Integration quality: these step counts are a good balance for 32x32 irradiance maps.
    const int PHI_SAMPLES = 32;
    const int THETA_SAMPLES = 16;

    for (int t = 0; t < THETA_SAMPLES; ++t)
    {
        // theta in [0..pi/2]
        float theta = (float(t) + 0.5) / float(THETA_SAMPLES) * (0.5 * PI);
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (int p = 0; p < PHI_SAMPLES; ++p)
        {
            float phi = (float(p) + 0.5) / float(PHI_SAMPLES) * (2.0 * PI);

            float3 hemiDirLocal = float3(cos(phi) * sinTheta,
                                         sin(phi) * sinTheta,
                                         cosTheta);

            // Transform to world around N
            float3 L = normalize(T * hemiDirLocal.x + B * hemiDirLocal.y + N * hemiDirLocal.z);

            // cosine-weighted integral
            float NdotL = max(dot(N, L), 0.0);

            float3 sample = u_Environment.SampleLevel(u_Sampler, float3(L.x, -L.y, L.z), 0).rgb;
            irradiance += sample * NdotL;
        }
    }

    // Scale by integral constants
    irradiance *= (PI / float(PHI_SAMPLES * THETA_SAMPLES));
    irradiance *= u_EnvIntensity;

    return float4(irradiance, 1.0);
}
