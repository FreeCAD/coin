// data/shaders/vulkan/wide-line/WideLineInstancedVertex.glsl
// Vulkan wide-line vertex shader (GPU instanced) for the retained backend.
//
// Replaces CPU quad expansion: one instance per segment, six vertices per
// instance.  Binding 0 carries the object-space endpoints/colors (rate
// INSTANCE); the model matrix is in binding 1 as in the visual pipeline.
// Expansion mirrors SoVulkanRenderBackendWideLine.cpp: transform endpoints by
// u_proj*u_view*model; apply Coin Y-flip and OpenGL->Vulkan depth remap;
// near-clip the hidden endpoint onto z = 0; offset corners in NDC by line
// width, scaled by clip w for perspective.
//
// Push-constant layout matches the visual pass; only u_lineGeom is read here.

#version 450

layout(push_constant) uniform PushConstants {
    vec4  u_color;        // offset 0, 16 bytes
    vec4  u_flags;        // offset 16, 16 bytes
    vec4  u_texParams;    // offset 32, 16 bytes
    vec4  u_texBlend;     // offset 48, 16 bytes
    float u_pointSize;    // offset 64, 16 bytes (pad[3])
    vec4  u_lineParams;   // offset 80, 16 bytes
    vec4  u_lineGeom;     // offset 96, 16 bytes: x = line width (device px),
                          // y=viewport width, z=viewport height, w=device pixel ratio
} pc;

// Per-draw block (set 1, binding 0); projection at offset 192 keeps push constants <=128B.
layout(set = 1, binding = 0, std140) uniform DrawBlock {
    mat4  u_view;                 // offset 0
    mat4  u_model;                // offset 64
    vec4  u_emissiveColor;        // offset 128
    vec4  u_materialAmbient;      // offset 144
    vec4  u_materialSpecular;     // offset 160
    vec4  u_materialParams;       // offset 176
    mat4  u_proj;                 // offset 192
} draw;

// Instance-rate endpoints/colors (binding 0).  One instance is one segment, so
// binding 1's per-instance model must NOT be read here (the index advances into
// the next command's ring slot); all segments share the DrawBlock model.
layout(location = 0) in vec4 a_p0;
layout(location = 1) in vec4 a_p1;
layout(location = 2) in vec4 a_c0;
layout(location = 3) in vec4 a_c1;

layout(location = 0) out vec4 v_color;
// Interface compatibility with WideLineFragment.glsl (always reads loc 1);
// non-stippled only, but write a meaningful distance so the varying is defined.
layout(location = 1) out float v_lineDistance;

// Triangle order as CPU producer: [0]=p0+off, [1]=p0-off, [2]=p1+off, [3]=p1-off.
const int kTriOrder[6] = int[](0, 1, 2, 2, 1, 3);

void main()
{
    const float kNearEps = 1.0e-5;
    v_lineDistance = 0.0;

    mat4 model = draw.u_model;
    mat4 mvp = draw.u_proj * draw.u_view * model;

    vec4 c0 = mvp * vec4(a_p0.xyz, 1.0);
    vec4 c1 = mvp * vec4(a_p1.xyz, 1.0);
    // Coin/OpenGL bottom-left origin -> Vulkan top-left.
    c0.y = -c0.y;
    c1.y = -c1.y;
    // OpenGL [-1,1] depth -> Vulkan [0,1] (keep w for the perspective divide).
    c0.z = 0.5 * c0.z + 0.5 * c0.w;
    c1.z = 0.5 * c1.z + 0.5 * c1.w;

    // Near-clip as producer: hidden endpoint interpolated onto z = 0 with t = z0/(z0-z1).
    bool visible0 = (c0.w > kNearEps) && (c0.z >= 0.0);
    bool visible1 = (c1.w > kNearEps) && (c1.z >= 0.0);
    if (!visible0 && !visible1) {
        // Fully clipped: collapse to a degenerate primitive.
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        v_color = vec4(0.0);
        return;
    }
    float tA = 0.0;
    float tB = 1.0;
    if (!(visible0 && visible1)) {
        float denom = c0.z - c1.z;
        float tclip = (denom != 0.0) ? c0.z / denom : 0.0;
        tA = visible0 ? 0.0 : tclip;
        tB = visible1 ? 1.0 : tclip;
    }
    vec4 cA = mix(c0, c1, tA);
    vec4 cB = mix(c0, c1, tB);
    if (cA.w <= kNearEps || cB.w <= kNearEps) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        v_color = vec4(0.0);
        return;
    }

    vec2 ndc0 = cA.xy / cA.w;
    vec2 ndc1 = cB.xy / cB.w;
    vec2 d = ndc1 - ndc0;
    float len = length(d);
    if (len < 1.0e-8) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
        v_color = vec4(0.0);
        return;
    }
    vec2 dir = d / len;
    // Half-width offset in NDC, one axis per viewport dimension (as produced).
    float offx = -dir.y * pc.u_lineGeom.x / max(pc.u_lineGeom.y, 1.0);
    float offy =  dir.x * pc.u_lineGeom.x / max(pc.u_lineGeom.z, 1.0);

    int corner = kTriOrder[gl_VertexIndex];
    bool endpoint1 = corner >= 2;
    float sign = ((corner & 1) == 0) ? 1.0 : -1.0;
    // Per-segment screen distance (px): 0 at p0 corners, length at p1 corners.
    v_lineDistance = endpoint1
        ? length(d * vec2(max(pc.u_lineGeom.y, 1.0),
                          max(pc.u_lineGeom.z, 1.0)) * 0.5)
        : 0.0;
    vec4 base = endpoint1 ? cB : cA;
    float w = base.w;
    // Scale offset by w so the perspective divide yields constant screen width.
    vec4 pos = base;
    pos.x += sign * offx * w;
    pos.y += sign * offy * w;
    gl_Position = pos;

    vec4 colA = mix(a_c0, a_c1, tA);
    vec4 colB = mix(a_c0, a_c1, tB);
    vec4 col = endpoint1 ? colB : colA;
    v_color = pc.u_flags.x > 0.5 ? col : vec4(pc.u_color.rgb, 1.0);
}
