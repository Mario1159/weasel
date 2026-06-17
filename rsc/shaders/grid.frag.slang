struct PSInput {
    float4 pos       : SV_Position;
    float3 worldPos  : TEXCOORD0;
};

cbuffer GridData : register(b0, space3)
{
    float3 u_FogCenter;
    float  u_FogRadius;
    float4 u_XAxisColor;
    float4 u_ZAxisColor;
    float4 u_GridColor;
    float4 u_MajorGridColor;
};

struct PSOut {
    float4 color : SV_Target0;
    float4 bloom : SV_Target1;
};

float2 fwidth_approx(float2 v)
{
    return abs(ddx(v)) + abs(ddy(v));
}

float getGrid(float2 worldXZ, float res)
{
    float2 coord = worldXZ * res;
    float2 fw = fwidth_approx(coord);
    float2 grid = abs(frac(coord - 0.5) - 0.5) / max(fw, 0.001);
    float line_val = min(grid.x, grid.y);
    return 1.0 - saturate(line_val);
}

[shader("fragment")]
PSOut fsMain(PSInput input)
{
    // 1. Performance: Early exit for Fog of War
    float d = length(input.worldPos - u_FogCenter);
    float fog = saturate(1.0 - d / u_FogRadius);

    // Very aggressive quartic falloff
    fog = fog * fog * fog * fog;

    if (fog <= 0.005) {
        PSOut o;
        o.color = float4(0, 0, 0, 0);
        o.bloom = float4(0, 0, 0, 1);
        return o;
    }

    float2 uv = input.worldPos.xz;

    // 2. Grid masks
    float thin = getGrid(uv, 1.0);
    float major = getGrid(uv, 0.1);

    // 3. Stable Axes
    float2 fw = fwidth_approx(uv);
    float x_axis = 1.0 - saturate(abs(uv.y) / max(fw.y * 3.0, 0.001));
    float z_axis = 1.0 - saturate(abs(uv.x) / max(fw.x * 3.0, 0.001));

    float4 color = float4(0, 0, 0, 0);
    color = lerp(color, u_GridColor, thin);
    color = lerp(color, u_MajorGridColor, major);
    color = lerp(color, u_XAxisColor, x_axis);
    color = lerp(color, u_ZAxisColor, z_axis);

    color.a *= fog;

    PSOut o;
    o.color = color;
    o.bloom = float4(0, 0, 0, 1);
    return o;
}
