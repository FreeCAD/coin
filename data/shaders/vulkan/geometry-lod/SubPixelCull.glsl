// data/shaders/vulkan/geometry-lod/SubPixelCull.glsl
// GPU sub-pixel primitive culling (geometry LOD) for the raster Vulkan backend.
//
// One invocation per triangle: read its object-space positions, transform by
// the visual shader's model*view*projection, project to device pixels, and drop
// it below a screen-area threshold.  Survivors are compacted into a dense index
// buffer and drawn with vkCmdDrawIndexedIndirect (culled costs no shading/raster).
//
// Runs OUTSIDE the render pass (Vulkan forbids compute inside one): recorded
// before vkCmdBeginRenderPass, ordered against the draws by a memory barrier.
//
// Vertex stride 32 B (8 floats: pos 0..2, normal 3..5, color 6, texcoord 7, see
// VULKAN_VERTEX_STRIDE).  u_offsets.x/y are float/uint bases into the (possibly
// shared) buffers (descriptors bind whole buffers).  indirect.indexCount is the
// atomic append cursor and the field vkCmdDrawIndexedIndirect reads; the CPU
// zeroes only that 4-byte field per frame with vkCmdFillBuffer.

#version 450

layout(local_size_x = 64) in;

// Distinct from the raster PushConstants so offline reflection (spirv_layout_check.py) is unambiguous.
layout(push_constant) uniform SubPixelCullPush {
    mat4 u_mvp;        // offset 0:  combined model*view*projection (row-major
                       //            SbMat packed as mat4 columns)
    vec4 u_params;     // offset 64: x = viewport width  (px),
                       //            y=viewport height, z=min area (px^2*2), w=triangle count
    vec4 u_offsets;    // offset 80: x = vertex base (floats),
                       //            y=index base (uints), z=1 indexed / 0 non-indexed
} pc;

layout(set = 0, binding = 0) readonly buffer VertexData {
    float data[];
} verts;

layout(set = 0, binding = 1) readonly buffer IndexData {
    uint data[];
} idx;

layout(set = 0, binding = 2) writeonly buffer OutIndexData {
    uint data[];
} outIdx;

// Indirect command: indexCount is the append cursor; other fields are set once.
layout(set = 0, binding = 3) buffer IndirectData {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;
} indirect;

const uint VERTEX_FLOATS = 8u;

vec3 loadPosition(const uint vertexIndex)
{
    const uint base = uint(pc.u_offsets.x) + vertexIndex * VERTEX_FLOATS;
    return vec3(verts.data[base], verts.data[base + 1u], verts.data[base + 2u]);
}

void main()
{
    const uint prim = gl_GlobalInvocationID.x;
    if (prim >= uint(pc.u_params.w)) return;

    // Indexed: read the index buffer.  Non-indexed list: vertices 3i, 3i+1,
    // 3i+2.  Survivors are always written as indices.
    const bool indexed = pc.u_offsets.z > 0.5;
    uint i0;
    uint i1;
    uint i2;
    if (indexed) {
        const uint ibase = uint(pc.u_offsets.y) + prim * 3u;
        i0 = idx.data[ibase];
        i1 = idx.data[ibase + 1u];
        i2 = idx.data[ibase + 2u];
    }
    else {
        i0 = prim * 3u;
        i1 = i0 + 1u;
        i2 = i0 + 2u;
    }

    const vec4 c0 = pc.u_mvp * vec4(loadPosition(i0), 1.0);
    const vec4 c1 = pc.u_mvp * vec4(loadPosition(i1), 1.0);
    const vec4 c2 = pc.u_mvp * vec4(loadPosition(i2), 1.0);

    // Keep triangles touching/crossing the near plane (w <= eps): the
    // rasterizer clips them; culling an undefined projection would pop.
    const float kNearEps = 1.0e-6;
    bool keep = false;
    if (c0.w <= kNearEps || c1.w <= kNearEps || c2.w <= kNearEps) {
        keep = true;
    }
    else {
        // NDC -> device pixels; the 0.5 (origin cancels) makes the cross
        // product twice the triangle area in px^2.
        const vec2 vp = pc.u_params.xy * 0.5;
        const vec2 p0 = (c0.xy / c0.w) * vp;
        const vec2 p1 = (c1.xy / c1.w) * vp;
        const vec2 p2 = (c2.xy / c2.w) * vp;
        const float cross = abs((p1.x - p0.x) * (p2.y - p0.y) -
                                (p2.x - p0.x) * (p1.y - p0.y));
        keep = cross >= pc.u_params.z;
    }
    if (!keep) return;

    const uint slot = atomicAdd(indirect.indexCount, 3u);
    outIdx.data[slot] = i0;
    outIdx.data[slot + 1u] = i1;
    outIdx.data[slot + 2u] = i2;
}
