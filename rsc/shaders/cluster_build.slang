// cluster_build.slang
//
// Compute shader that builds the clustered shading AABB grid in view space.
// One thread per cluster. The cluster AABBs are written into
// u_ClusterBuffer and consumed by the fragment shader during forward PBR
// shading (cube.frag.slang).
//
// Algorithm follows `clustered_shading.md` Step 1: tile the screen into a
// grid, convert each tile to view space, then intersect the rays from the
// eye through the tile's corners with the cluster's near/far Z planes.
// Z division is exponential to match the article.
//
// SPIR-V descriptor set layout (required by SDL_GPU compute pipeline
// creation; see SDL_GPU docs):
//   set 0: read-only storage buffers
//   set 1: read-write storage buffers
//   set 2: uniform (c)buffers
// All explicit `[[vk::binding]]` attributes target this layout. The
// matching fragment shader (`cube.frag.slang`) uses the same bindings for
// the shared SSBOs so the same GPU buffer can be bound from both stages.

#define CLUSTER_BUILD_THREADS_X 4
#define CLUSTER_BUILD_THREADS_Y 4
#define CLUSTER_BUILD_THREADS_Z 1

struct Cluster
{
    float4 minPoint;
    float4 maxPoint;
    uint   count;
    uint   lightIndices[100];
};

// cbuffer lives in set 2 (UBO slot for SPIR-V compute).
[[vk::binding (0, 2)]]
cbuffer ClusterParams
{
    float4x4 u_InverseProjection;
    float2   u_ScreenSize;
    float    u_ZNear;
    float    u_ZFar;
    uint3    u_GridSize;
    uint     u_NumClusters;     // total = grid.x * grid.y * grid.z
    uint3    u_ClusterPadding;  // align to 16 bytes
};

// RW SSBO lives in set 1 at binding 0.
[[vk::binding (0, 1)]]
RWStructuredBuffer<Cluster> u_ClusterBuffer;


float3
lineIntersectionWithZPlane (float3 startPoint, float3 endPoint, float zDistance)
{
    float3 direction = endPoint - startPoint;
    float3 normal = float3 (0.0, 0.0, -1.0);
    float t = (zDistance - dot (normal, startPoint)) / dot (normal, direction);
    return startPoint + t * direction;
}

float3
screenToView (float2 screenCoord)
{
    // Normalize to NDC and force the W-divide-friendly depth at the near plane.
    float4 ndc = float4 (screenCoord / u_ScreenSize * 2.0 - 1.0, -1.0, 1.0);
    float4 viewCoord = mul (u_InverseProjection, ndc);
    viewCoord /= viewCoord.w;
    return viewCoord.xyz;
}

[shader ("compute")]
[numthreads (CLUSTER_BUILD_THREADS_X, CLUSTER_BUILD_THREADS_Y, CLUSTER_BUILD_THREADS_Z)]
void
csMain (uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint3 clusterID;
    clusterID.x = dispatchThreadID.x;
    clusterID.y = dispatchThreadID.y;
    clusterID.z = dispatchThreadID.z;

    if (clusterID.x >= u_GridSize.x || clusterID.y >= u_GridSize.y
        || clusterID.z >= u_GridSize.z)
    {
        return;
    }

    uint tileIndex
        = clusterID.x + (clusterID.y * u_GridSize.x)
          + (clusterID.z * u_GridSize.x * u_GridSize.y);

    if (tileIndex >= u_NumClusters) {
        return;
    }

    float2 tileSize = u_ScreenSize / float2 (u_GridSize.xy);

    // Tile in screen space.
    float2 minTile_screenspace = float2 (clusterID.xy) * tileSize;
    float2 maxTile_screenspace = float2 (clusterID.xy + uint2 (1, 1)) * tileSize;

    // Convert tile corners to view space at the near plane.
    float3 minTile = screenToView (minTile_screenspace);
    float3 maxTile = screenToView (maxTile_screenspace);

    float planeNear = u_ZNear
        * pow (u_ZFar / u_ZNear, float (clusterID.z) / float (u_GridSize.z));
    float planeFar = u_ZNear
        * pow (u_ZFar / u_ZNear, float (clusterID.z + 1) / float (u_GridSize.z));

    // Line from the eye (0,0,0 in view space) through each tile corner,
    // intersected with the cluster's near/far Z planes.
    float3 minPointNear = lineIntersectionWithZPlane (
        float3 (0.0, 0.0, 0.0), minTile, planeNear);
    float3 minPointFar = lineIntersectionWithZPlane (
        float3 (0.0, 0.0, 0.0), minTile, planeFar);
    float3 maxPointNear = lineIntersectionWithZPlane (
        float3 (0.0, 0.0, 0.0), maxTile, planeNear);
    float3 maxPointFar = lineIntersectionWithZPlane (
        float3 (0.0, 0.0, 0.0), maxTile, planeFar);

    Cluster c;
    c.minPoint = float4 (min (minPointNear, minPointFar), 0.0);
    c.maxPoint = float4 (max (maxPointNear, maxPointFar), 0.0);
    c.count = 0;

    for (uint i = 0; i < 100; ++i) {
        c.lightIndices[i] = 0;
    }

    u_ClusterBuffer[tileIndex] = c;
}
