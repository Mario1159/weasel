struct VSOutput {
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
};

cbuffer SceneData : register(b0, space1)
{
    column_major float4x4 u_ViewProj;
    float3 u_FogCenter;
    float  u_FogRadius;
};

[shader("vertex")]
VSOutput vsMain(uint v_id : SV_VertexID)
{
    // Generate a quad centered at the Fog of War center
    float2 positions[6] = {
        float2(-1.0, -1.0), float2( 1.0, -1.0), float2(-1.0,  1.0),
        float2(-1.0,  1.0), float2( 1.0, -1.0), float2( 1.0,  1.0)
    };

    VSOutput output;

    // Scale the plane to the visibility radius and center on the gaze point
    float2 p = positions[v_id] * u_FogRadius;
    float3 worldPos = float3(p.x + u_FogCenter.x, 0.0, p.y + u_FogCenter.z);

    output.worldPos = worldPos;
    output.pos = mul(u_ViewProj, float4(worldPos, 1.0));

    return output;
}
