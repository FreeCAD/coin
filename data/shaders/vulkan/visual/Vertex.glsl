// data/shaders/vulkan/visual/Vertex.glsl
// Vulkan visual-pass vertex shader for the retained render backend.
//
// Interleaved: 0 vec3 a_position, 1 vec3 a_normal, 2 vec4 a_color, 3 vec2 a_texcoord.
//
// Per-instance model matrix (binding 1, rate INSTANCE): commands sharing
// geometry/material use one instanced vkCmdDraw; non-instanced draws bind a
// one-element instance buffer.  Row-major SbMat rows packed as vec4s and
// assembled as mat4 columns reproduce the old column-major UBO mat4.
//
// Push constants: proj, diffuse color, flags; view/model + per-draw material
// in a std140 UBO (set 1, binding 0); lighting (ambient + lights) in set 0,
// binding 0, once per lighting setup.  Lighting is per-vertex (Gouraud).

#version 450

layout(push_constant) uniform PushConstants {
    vec4  u_color;        // offset 0, 16 bytes
    vec4  u_flags;        // offset 16, 16 bytes
    vec4  u_texParams;    // offset 32, 16 bytes
    vec4  u_texBlend;     // offset 48, 16 bytes
    float u_pointSize;    // offset 64, 16 bytes (pad[3])
    vec4  u_lineParams;   // offset 80, 16 bytes: x = stipple factor,
                          //   y=round points, z=line primitive, w=point primitive
} pc;

// Lighting constant block (written once per lighting setup per frame).
layout(set = 0, binding = 0, std140) uniform LightingBlock {
    vec4  u_ambientLight;         // offset 0
    vec4  u_lightType[8];         // offset 16
    vec4  u_lightColor[8];        // offset 144
    vec4  u_lightDirection[8];    // offset 272
    vec4  u_lightPosition[8];     // offset 400
    vec4  u_lightAttenuation[8];  // offset 528
    vec4  u_lightSpotParams[8];   // offset 656
} lighting;

// Per-draw block (view/model/material), selected by a dynamic offset.
layout(set = 1, binding = 0, std140) uniform DrawBlock {
    mat4  u_view;                 // offset 0
    mat4  u_model;                // offset 64
    vec4  u_emissiveColor;        // offset 128
    vec4  u_materialAmbient;      // offset 144
    vec4  u_materialSpecular;     // offset 160
    vec4  u_materialParams;       // offset 176: x=shininess, y=twoSided,
                                  //            z=lightCount, w=shadingModel
    mat4  u_proj;                 // offset 192: projection (view/model above)
} draw;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_texcoord;
// Per-instance model matrix (binding 1, rate INSTANCE): four row-major SbMat
// vec4 rows assembled as mat4 columns, matching the UBO mat4 transform.
layout(location = 4) in vec4 a_iModelRow0;
layout(location = 5) in vec4 a_iModelRow1;
layout(location = 6) in vec4 a_iModelRow2;
layout(location = 7) in vec4 a_iModelRow3;

// Lighting is evaluated per fragment (see Fragment.glsl): Gouraud bands on
// coarse CAD tessellation look faceted; interpolated eye pos/normal do not.
layout(location = 0) out vec4 v_color;
layout(location = 1) out vec3 v_eyePos;
layout(location = 2) out vec3 v_eyeNormal;
layout(location = 3) out vec2 v_texcoord;

void main()
{
    mat4 u_iModel = mat4(a_iModelRow0, a_iModelRow1,
                         a_iModelRow2, a_iModelRow3);
    vec4 worldPos = u_iModel * vec4(a_position, 1.0);
    vec4 eyePos = draw.u_view * worldPos;
    mat3 normalMatrix = transpose(inverse(mat3(draw.u_view * u_iModel)));
    vec3 eyeNormal = normalMatrix * a_normal;

    vec4 clip = draw.u_proj * eyePos;
    // Coin/OpenGL bottom-left origin -> Vulkan top-left; flip Y to match.
    clip.y = -clip.y;
    // Coin projections are OpenGL-style (NDC depth [-1,1]); Vulkan clips [0,1].
    // Remap z but leave w so it survives the divide: z' = 0.5*(z + w).
    clip.z = 0.5 * clip.z + 0.5 * clip.w;
    // Vulkan has no implicit point size: push the retained
    // SoDrawStyle/SoPointSizeElement value (points and FC_VULKAN_POINTS only).
    gl_PointSize = pc.u_pointSize;
    gl_Position = clip;

    v_color = pc.u_flags.x > 0.5 ? a_color : vec4(pc.u_color.rgb, 1.0);
    v_eyePos = eyePos.xyz;
    v_eyeNormal = eyeNormal;
    v_texcoord = a_texcoord;
}
