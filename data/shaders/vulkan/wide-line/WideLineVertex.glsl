// data/shaders/vulkan/wide-line/WideLineVertex.glsl
// Vulkan wide-line vertex shader for the retained render backend.
//
// CPU expands segments into quads (no geometry shader): a_position is the
// clip-space corner (xyz plus original clip w), Y-flip and depth remap already
// applied; a_lineDistance carries polyline distance for screen-space stipple.
// Push-constant layout matches the visual pass; only the fields below are read.

#version 450

layout(push_constant) uniform PushConstants {
    vec4  u_color;        // offset 0, 16 bytes
    vec4  u_flags;        // offset 16, 16 bytes
    vec4  u_texParams;    // offset 32, 16 bytes
    vec4  u_texBlend;     // offset 48, 16 bytes
    float u_pointSize;    // offset 64, 16 bytes (pad[3])
    vec4  u_lineParams;   // offset 80, 16 bytes: x = stipple factor (px/bit),
                          // y = 16-bit stipple pattern
} pc;

layout(location = 0) in vec4 a_position;
layout(location = 2) in vec4 a_color;
layout(location = 4) in float a_lineDistance;

layout(location = 0) out vec4 v_color;
layout(location = 1) out float v_lineDistance;

void main()
{
    gl_Position = a_position;
    v_color = pc.u_flags.x > 0.5 ? a_color : vec4(pc.u_color.rgb, 1.0);
    v_lineDistance = a_lineDistance;
}
