// data/shaders/vulkan/visual/BackgroundFragment.glsl
// Background gradient: linear top->bottom between topColor/bottomColor in screen
// space (gl_FragCoord.y == 0 is top in Vulkan).  Push constants, no descriptor sets.

#version 450

layout(push_constant) uniform BackgroundPush {
    vec4 topColor;       // offset 0, 16 bytes
    vec4 bottomColor;    // offset 16, 16 bytes
    vec4 viewport;       // offset 32, 16 bytes: x=width, y=height,
                         //                    z=originX, w=originY
} bg;

layout(location = 0) out vec4 fragColor;

void main()
{
    float h = max(bg.viewport.y, 1.0);
    // gl_FragCoord is absolute: subtract the viewport origin so a sub-region
    // viewport interpolates over its own extent, not the whole framebuffer.
    float y = gl_FragCoord.y - bg.viewport.w;
    float t = clamp(y / h, 0.0, 1.0);
    fragColor = mix(bg.topColor, bg.bottomColor, t);
}
