// point_shadow.frag.slang
// Fragment shader for point-light shadow cubemap rendering.
// Writes normalized radial distance into depth.
//
// The sampling side should reconstruct:
//   closestDist = sampledDepth * farPlane

struct PSInput
{
    float4 pos      : SV_Position;
    float3 worldPos : TEXCOORD0;
};

cbuffer PointShadowParams : register(b0, space3)
{
    float4 u_LightPosFar; // xyz = light position, w = far plane
};

[shader("fragment")]
float main(PSInput input) : SV_Depth
{
    float3 lightPos = u_LightPosFar.xyz;
    float  farPlane = max(u_LightPosFar.w, 0.0001);

    float dist = length(input.worldPos - lightPos);
    return saturate(dist / farPlane);
}
