// Material texture sampling helpers.

vec4 coin_material_textured_color(sampler2D textureSampler, vec2 texcoord,
                                  vec4 visualColor, float materialAlpha,
                                  float textureAlphaIncludesOpacity,
                                  float textureHasAlpha,
                                  int textureModel, vec4 blendColor)
{
  vec4 texel = texture(textureSampler, texcoord);
  float primaryAlpha = visualColor.a *
    (textureAlphaIncludesOpacity > 0.5 ? 1.0 : materialAlpha);
  float textureAlpha = textureHasAlpha > 0.5 ? texel.a : 1.0;
  vec3 rgb = visualColor.rgb;
  float alpha = primaryAlpha;

  switch (textureModel) {
  case 1: // DECAL
    rgb = mix(visualColor.rgb, texel.rgb, textureAlpha);
    break;
  case 2: // BLEND
    rgb = mix(visualColor.rgb, blendColor.rgb, texel.rgb);
    alpha = primaryAlpha * textureAlpha;
    break;
  case 3: // REPLACE
    rgb = texel.rgb;
    alpha = textureHasAlpha > 0.5 ? texel.a : primaryAlpha;
    break;
  case 0: // MODULATE
  default:
    rgb = visualColor.rgb * texel.rgb;
    alpha = primaryAlpha * textureAlpha;
    break;
  }
  return vec4(rgb, alpha);
}
