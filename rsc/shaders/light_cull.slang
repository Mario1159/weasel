// light_cull.slang
//
// Compute shader that assigns lights to clusters via sphere-vs-AABB
// intersection. One thread per cluster. Lights visible to a cluster are
// appended to the cluster's local light list up to MAX_LIGHTS_PER_CLUSTER.
//
// SPIR-V descriptor set layout (required by SDL_GPU compute pipeline
// creation; see SDL_GPU docs):
//   set 0: read-only storage buffers
//   set 1: read-write storage buffers
//   set 2: uniform (c)buffers
// The view matrix is applied to the light's world-space position here so
// the AABB test can be done entirely in view space.

#define MAX_LIGHTS_PER_CLUSTER 100
#define LIGHT_CULL_THREADS_X 64

struct GpuPointLight
{
    float4 pos_radius;      // xyz = world pos, w = radius
    float4 color_intensity; // rgb = color, w = intensity
    int4   shadow_info;     // x = shadow index, y = cast_shadows
};

struct Cluster
{
    float4 minPoint;
    float4 maxPoint;
    uint   count;
    uint   lightIndices[MAX_LIGHTS_PER_CLUSTER];
};

// RO light buffer: set 0, binding 0. SDL_GPU's compute pipeline allocates
// exactly `num_readonly_storage_buffers` consecutive binding slots
// starting at 0 in set 0; the SPIR-V binding number must be 0 here.
[[vk::binding (0, 0)]]
StructuredBuffer<GpuPointLight>  u_LightBuffer;

// RW cluster buffer: set 1, binding 0.
[[vk::binding (0, 1)]]
RWStructuredBuffer<Cluster>      u_ClusterBuffer;

// cbuffer lives in set 2 at binding 0.
[[vk::binding (0, 2)]]
cbuffer ClusterParams
{
    float4x4 u_InverseProjection;
    float2   u_ScreenSize;
    float    u_ZNear;
    float    u_ZFar;
    uint3    u_GridSize;
    uint     u_NumClusters;
    uint3    u_ClusterPadding;
    float4x4 u_View;
    uint     u_NumPointLights;
    uint3    u_LightPadding;
};

bool
sphereAABBIntersection (float3 center, float radius, float3 aabbMin, float3 aabbMax)
{
    float3 closestPoint = clamp (center, aabbMin, aabbMax);
    float3 diff = closestPoint - center;
    float distanceSquared = dot (diff, diff);
    return distanceSquared <= radius * radius;
}

[shader ("compute")]
[numthreads (LIGHT_CULL_THREADS_X, 1, 1)]
void
csMain (uint3 groupID : SV_GroupID, uint3 threadIDInGroup : SV_GroupThreadID)
{
    uint index = groupID.x * LIGHT_CULL_THREADS_X + threadIDInGroup.x;
    if (index >= u_NumClusters) {
        return;
    }

    Cluster cluster = u_ClusterBuffer[index];
    cluster.count = 0;

    // AABB is already in view space.
    float3 aabbMin = cluster.minPoint.xyz;
    float3 aabbMax = cluster.maxPoint.xyz;

    for (uint i = 0; i < u_NumPointLights; ++i) {
        GpuPointLight light = u_LightBuffer[i];

        // Transform world-space light position into view space.
        float4 viewPosH = mul (u_View, float4 (light.pos_radius.xyz, 1.0));
        float3 viewPos = viewPosH.xyz;

        if (sphereAABBIntersection (viewPos, light.pos_radius.w, aabbMin, aabbMax)) {
            if (cluster.count < MAX_LIGHTS_PER_CLUSTER) {
                cluster.lightIndices[cluster.count] = i;
                cluster.count++;
            }
        }
    }

    u_ClusterBuffer[index] = cluster;
}
