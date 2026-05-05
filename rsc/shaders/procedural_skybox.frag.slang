cbuffer Params : register(b0, space3)
{
    float3 u_SunDir; // Direction FROM the sun
    float u_Pad;
};

struct PSInput
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;
};

static const float PI = 3.14159265358;

// Units are in megameters.
static const float groundRadiusMM = 6.360;
static const float atmosphereRadiusMM = 6.460;
static const float3 viewPos = float3(0.0, groundRadiusMM + 0.0002, 0.0);

static const float3 rayleighScatteringBase = float3(5.802, 13.558, 33.1);
static const float rayleighAbsorptionBase = 0.0;
static const float mieScatteringBase = 3.996;
static const float mieAbsorptionBase = 4.4;
static const float3 ozoneAbsorptionBase = float3(0.650, 1.881, 0.085);

float getMiePhase(float cosTheta) {
    const float g = 0.8;
    const float scale = 3.0 / (8.0 * PI);
    float num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
    float denom = (2.0 + g * g) * pow(abs(1.0 + g * g - 2.0 * g * cosTheta), 1.5);
    return scale * num / denom;
}

float getRayleighPhase(float cosTheta) {
    const float k = 3.0 / (16.0 * PI);
    return k * (1.0 + cosTheta * cosTheta);
}

void getScatteringValues(float3 pos,
                         out float3 rayleighScattering,
                         out float mieScattering,
                         out float3 extinction) {
    float altitudeKM = (length(pos) - groundRadiusMM) * 1000.0;
    float rayleighDensity = exp(-altitudeKM / 8.0);
    float mieDensity = exp(-altitudeKM / 1.2);

    rayleighScattering = rayleighScatteringBase * rayleighDensity;
    float rayleighAbsorption = rayleighAbsorptionBase * rayleighDensity;

    mieScattering = mieScatteringBase * mieDensity;
    float mieAbsorption = mieAbsorptionBase * mieDensity;

    float3 ozoneAbsorption = ozoneAbsorptionBase * max(0.0, 1.0 - abs(altitudeKM - 25.0) / 15.0);

    extinction = rayleighScattering + rayleighAbsorption + mieScattering + mieAbsorption + ozoneAbsorption;
}

float rayIntersectSphere(float3 ro, float3 rd, float rad) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    if (c > 0.0 && b > 0.0) return -1.0;
    float discr = b * b - c;
    if (discr < 0.0) return -1.0;
    if (discr > b * b) return (-b + sqrt(discr));
    return -b - sqrt(discr);
}

float3 getSunTransmittance(float3 pos, float3 sunDir) {
    if (rayIntersectSphere(pos, sunDir, groundRadiusMM) > 0.0) {
        return float3(0.0, 0.0, 0.0);
    }

    float atmoDist = rayIntersectSphere(pos, sunDir, atmosphereRadiusMM);
    float t = 0.0;
    float3 transmittance = float3(1.0, 1.0, 1.0);
    const float steps = 16.0;

    for (float i = 0.0; i < steps; i += 1.0) {
        float newT = ((i + 0.3) / steps) * atmoDist;
        float dt = newT - t;
        t = newT;

        float3 newPos = pos + t * sunDir;
        float3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);

        transmittance *= exp(-dt * extinction);
    }
    return transmittance;
}

float3 raymarchScattering(float3 pos, float3 rayDir, float3 sunDir, float tMax) {
    float cosTheta = dot(rayDir, sunDir);
    float miePhaseValue = getMiePhase(cosTheta);
    float rayleighPhaseValue = getRayleighPhase(-cosTheta);

    float3 lum = float3(0.0, 0.0, 0.0);
    float3 transmittance = float3(1.0, 1.0, 1.0);
    float t = 0.0;
    const float steps = 16.0;

    for (float i = 0.0; i < steps; i += 1.0) {
        float newT = ((i + 0.3) / steps) * tMax;
        float dt = newT - t;
        t = newT;

        float3 newPos = pos + t * rayDir;
        float3 rayleighScattering, extinction;
        float mieScattering;
        getScatteringValues(newPos, rayleighScattering, mieScattering, extinction);

        float3 sampleTransmittance = exp(-dt * extinction);
        float3 sunTransmittance = getSunTransmittance(newPos, sunDir);

        // Simplified: ignore Multi-Scattering LUT for single-pass cubemap bake
        float3 inScattering = rayleighScattering * (rayleighPhaseValue * sunTransmittance) +
                              mieScattering * (miePhaseValue * sunTransmittance);

        float3 scatteringIntegral = (inScattering - inScattering * sampleTransmittance) / max(0.00001, extinction);
        lum += scatteringIntegral * transmittance;
        transmittance *= sampleTransmittance;
    }
    return lum;
}

float3 jodieReinhardTonemap(float3 c) {
    float l = dot(c, float3(0.2126, 0.7152, 0.0722));
    float3 tc = c / (c + 1.0);
    return lerp(c / (l + 1.0), tc, tc);
}

float3 sunWithBloom(float3 rayDir, float3 sunDir) {
    const float sunSolidAngle = 0.53 * PI / 180.0;
    const float minSunCosTheta = cos(sunSolidAngle);

    float cosTheta = dot(rayDir, sunDir);
    if (cosTheta >= minSunCosTheta) return float3(1.0, 1.0, 1.0);

    float offset = minSunCosTheta - cosTheta;
    float gaussianBloom = exp(-offset * 50000.0) * 0.5;
    float invBloom = 1.0 / (0.02 + offset * 300.0) * 0.01;
    return float3(gaussianBloom + invBloom, gaussianBloom + invBloom, gaussianBloom + invBloom);
}

[shader("fragment")]
float4 main(PSInput input) : SV_Target {
    float3 rayDir = normalize(input.dir);
    float3 sunDir = normalize(-u_SunDir);

    float atmoDist = rayIntersectSphere(viewPos, rayDir, atmosphereRadiusMM);
    float groundDist = rayIntersectSphere(viewPos, rayDir, groundRadiusMM);
    float tMax = (groundDist < 0.0) ? atmoDist : groundDist;

    float3 lum = float3(0,0,0);
    if (atmoDist > 0.0) {
        lum = raymarchScattering(viewPos, rayDir, sunDir, tMax);
    }

    float3 sunLum = sunWithBloom(rayDir, sunDir);
    sunLum = smoothstep(0.002, 1.0, sunLum);
    if (length(sunLum) > 0.0) {
        if (groundDist >= 0.0) {
            sunLum *= 0.0;
        } else {
            sunLum *= getSunTransmittance(viewPos, sunDir);
        }
    }
    lum += sunLum;

    // Tonemapping and gamma as in original requested mainImage
    lum *= 20.0;
    lum = pow(max(0.0, lum), float3(1.3, 1.3, 1.3));
    lum /= (smoothstep(0.0, 0.2, clamp(sunDir.y, 0.0, 1.0)) * 2.0 + 0.15);

    lum = jodieReinhardTonemap(lum);
    lum = pow(max(0.0, lum), float3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    return float4(lum, 1.0);
}
